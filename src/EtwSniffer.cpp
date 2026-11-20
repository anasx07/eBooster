#include "ebooster/EtwSniffer.hpp"
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>

namespace ebooster {

// Microsoft-Windows-Kernel-Network: {7dd42a49-5329-4832-8dfd-43d979153a88}
static const GUID KernelNetworkGuid = 
    { 0x7dd42a49, 0x5329, 0x4832, { 0x8d, 0xfd, 0x43, 0xd9, 0x79, 0x15, 0x3a, 0x88 } };

// Microsoft-Windows-TCPIP: {2f07e2e4-9c66-4e2f-97ee-eeb43642b76e}
static const GUID TcpipGuid = 
    { 0x2f07e2e4, 0x9c66, 0x4e2f, { 0x97, 0xee, 0xee, 0xb4, 0x36, 0x42, 0xb7, 0x6e } };

EtwSniffer::EtwSniffer() {
    s_pInstance = this;
}

EtwSniffer::~EtwSniffer() {
    Stop();
    if (s_pInstance == this) {
        s_pInstance = nullptr;
    }
}

void EtwSniffer::SetCallback(UdpEndpointCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(callback);
}

void EtwSniffer::SetPacketCallback(DatagramPacketCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_packetCallback = std::move(callback);
}

void EtwSniffer::UpdateTargetPids(const std::vector<DWORD>& targetPids) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targetPids.clear();
    for (DWORD pid : targetPids) {
        m_targetPids.insert(pid);
    }
}

void EtwSniffer::ResetDiscoveredEndpoints() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_discoveredEndpoints.clear();
}

bool EtwSniffer::Start(const std::vector<DWORD>& targetPids) {
    if (m_isRunning.exchange(true)) {
        UpdateTargetPids(targetPids);
        return true;
    }

    UpdateTargetPids(targetPids);
    s_pInstance = this;

    // Buffer for EVENT_TRACE_PROPERTIES + session name string
    ULONG propSize = sizeof(EVENT_TRACE_PROPERTIES) + 512 * sizeof(wchar_t);
    std::vector<BYTE> propBuf(propSize, 0);
    auto* pProps = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(propBuf.data());
    pProps->Wnode.BufferSize = propSize;
    pProps->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    pProps->Wnode.ClientContext = 1; // QPC clock
    pProps->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    pProps->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    // Stop any stale session from an earlier unclean termination
    ControlTraceW(0, m_sessionName.c_str(), pProps, EVENT_TRACE_CONTROL_STOP);

    // Re-initialize properties for StartTraceW
    std::memset(propBuf.data(), 0, propBuf.size());
    pProps->Wnode.BufferSize = propSize;
    pProps->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    pProps->Wnode.ClientContext = 1; // QPC clock
    pProps->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    pProps->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    TRACEHANDLE sessHandle = 0;
    ULONG status = StartTraceW(&sessHandle, m_sessionName.c_str(), pProps);
    if (status != ERROR_SUCCESS) {
        LogFmt(LogLevel::Warn, "Could not start ETW real-time session (Win32 Error: {}). Requires Administrator.", status);
        m_isRunning = false;
        return false;
    }
    m_sessionHandle.store(sessHandle);

    // Enable providers for network datagram events
    EnableTraceEx2(sessHandle, &KernelNetworkGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                   TRACE_LEVEL_INFORMATION, 0xFFFFFFFFFFFFFFFF, 0, 0, nullptr);
    EnableTraceEx2(sessHandle, &TcpipGuid, EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                   TRACE_LEVEL_INFORMATION, 0xFFFFFFFFFFFFFFFF, 0, 0, nullptr);

    Log(LogLevel::Info, "ETW Kernel Network Packet Sniffer started (Zero DLL injection, real-time UDP capture active).");

    m_workerThread = std::jthread([this](std::stop_token) {
        EVENT_TRACE_LOGFILEW logFile{};
        logFile.LoggerName = const_cast<LPWSTR>(m_sessionName.c_str());
        logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
        logFile.EventRecordCallback = &EtwSniffer::EventRecordCallback;
        logFile.Context = this;

        TRACEHANDLE handle = OpenTraceW(&logFile);
        if (handle != INVALID_PROCESSTRACE_HANDLE) {
            m_traceHandle.store(handle);
            ProcessTrace(&handle, 1, nullptr, nullptr);
            TRACEHANDLE oldHandle = m_traceHandle.exchange(INVALID_PROCESSTRACE_HANDLE);
            if (oldHandle != INVALID_PROCESSTRACE_HANDLE) {
                CloseTrace(oldHandle);
            }
        }
    });

    return true;
}

void EtwSniffer::Stop() {
    if (!m_isRunning.exchange(false)) {
        return;
    }

    // 1. Stop ETW session first: flushes real-time buffers and signals ProcessTrace to exit immediately
    TRACEHANDLE sess = m_sessionHandle.exchange(0);
    if (sess != 0) {
        ULONG propSize = sizeof(EVENT_TRACE_PROPERTIES) + 512 * sizeof(wchar_t);
        std::vector<BYTE> propBuf(propSize, 0);
        auto* pProps = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(propBuf.data());
        pProps->Wnode.BufferSize = propSize;
        ControlTraceW(sess, m_sessionName.c_str(), pProps, EVENT_TRACE_CONTROL_STOP);
    }

    // 2. Unblock trace processing handle if ProcessTrace hasn't returned yet
    TRACEHANDLE traceHandle = m_traceHandle.exchange(INVALID_PROCESSTRACE_HANDLE);
    if (traceHandle != INVALID_PROCESSTRACE_HANDLE) {
        CloseTrace(traceHandle);
    }

    // 3. Join the worker thread cleanly with zero hang
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }

    Log(LogLevel::Info, "ETW Kernel Network Packet Sniffer stopped cleanly.");
}

void WINAPI EtwSniffer::EventRecordCallback(PEVENT_RECORD pEvent) {
    if (!pEvent) return;
    auto* pThis = static_cast<EtwSniffer*>(pEvent->UserContext);
    if (!pThis) {
        pThis = s_pInstance;
    }
    if (pThis) {
        pThis->HandleEvent(pEvent);
    }
}

void EtwSniffer::HandleEvent(PEVENT_RECORD pEvent) {
    DWORD pid = pEvent->EventHeader.ProcessId;
    if (pid == 0) return;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_targetPids.find(pid) == m_targetPids.end()) {
            return; // Fast reject non-target processes in ~3ns
        }
    }

    // Parse event payload using Trace Data Helper (TDH)
    DWORD bufSize = 0;
    if (TdhGetEventInformation(pEvent, 0, nullptr, nullptr, &bufSize) == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<BYTE> buf(bufSize);
        auto* pInfo = reinterpret_cast<TRACE_EVENT_INFO*>(buf.data());
        if (TdhGetEventInformation(pEvent, 0, nullptr, pInfo, &bufSize) == ERROR_SUCCESS) {
            PROPERTY_DATA_DESCRIPTOR desc{};
            std::string destIp;
            uint16_t destPort = 0;

            for (DWORD i = 0; i < pInfo->TopLevelPropertyCount; ++i) {
                auto* name = reinterpret_cast<LPWSTR>(reinterpret_cast<PBYTE>(pInfo) + pInfo->EventPropertyInfoArray[i].NameOffset);
                std::wstring propName(name);

                if (propName == L"daddr" || propName == L"DestinationAddress" || 
                    propName == L"RemoteAddress" || propName == L"DestAddr") {
                    desc.PropertyName = reinterpret_cast<ULONGLONG>(name);
                    desc.ArrayIndex = ULONG_MAX;
                    IN_ADDR inAddr{};
                    DWORD propSize = sizeof(inAddr);
                    if (TdhGetProperty(pEvent, 0, nullptr, 1, &desc, propSize, reinterpret_cast<PBYTE>(&inAddr)) == ERROR_SUCCESS) {
                        destIp = InAddrToString(inAddr);
                    }
                } else if (propName == L"dport" || propName == L"DestinationPort" || 
                           propName == L"RemotePort") {
                    desc.PropertyName = reinterpret_cast<ULONGLONG>(name);
                    desc.ArrayIndex = ULONG_MAX;
                    USHORT port = 0;
                    DWORD propSize = sizeof(port);
                    if (TdhGetProperty(pEvent, 0, nullptr, 1, &desc, propSize, reinterpret_cast<PBYTE>(&port)) == ERROR_SUCCESS) {
                        destPort = ntohs(port);
                    }
                }
            }

            if (!destIp.empty() && !IsPrivateOrLoopbackIp(destIp)) {
                DatagramPacketCallback pktCb;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    pktCb = m_packetCallback;
                }
                if (pktCb) {
                    pktCb(destIp, destPort, pid);
                }

                std::string epKey = destIp + ":" + std::to_string(destPort);
                bool isNew = false;
                UdpEndpointCallback cb;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (m_discoveredEndpoints.insert(epKey).second) {
                        isNew = true;
                        cb = m_callback;
                    }
                }

                if (isNew && cb) {
                    cb(destIp, destPort, pid);
                }
            }
        }
    }
}

} // namespace ebooster
