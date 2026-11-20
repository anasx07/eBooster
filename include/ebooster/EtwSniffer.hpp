#pragma once

#include "ebooster/Common.hpp"
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_set>
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>

namespace ebooster {

using UdpEndpointCallback = std::function<void(const std::string& remoteIp, uint16_t remotePort, DWORD pid)>;
using DatagramPacketCallback = std::function<void(const std::string& remoteIp, uint16_t remotePort, DWORD pid)>;

class EtwSniffer {
public:
    EtwSniffer();
    ~EtwSniffer();

    EtwSniffer(const EtwSniffer&) = delete;
    EtwSniffer& operator=(const EtwSniffer&) = delete;

    void SetCallback(UdpEndpointCallback callback);
    void SetPacketCallback(DatagramPacketCallback callback);
    bool Start(const std::vector<DWORD>& targetPids);
    void UpdateTargetPids(const std::vector<DWORD>& targetPids);
    void Stop();
    void ResetDiscoveredEndpoints();

    bool IsRunning() const noexcept { return m_isRunning.load(); }

private:
    static void WINAPI EventRecordCallback(PEVENT_RECORD pEvent);
    void HandleEvent(PEVENT_RECORD pEvent);

    std::wstring m_sessionName{L"eBoosterUdpTrace"};
    std::atomic<TRACEHANDLE> m_sessionHandle{0};
    std::atomic<TRACEHANDLE> m_traceHandle{INVALID_PROCESSTRACE_HANDLE};

    std::atomic<bool> m_isRunning{false};
    std::jthread m_workerThread;

    mutable std::mutex m_mutex;
    std::unordered_set<DWORD> m_targetPids;
    UdpEndpointCallback m_callback;
    DatagramPacketCallback m_packetCallback;
    std::unordered_set<std::string> m_discoveredEndpoints;

    static inline EtwSniffer* s_pInstance{nullptr};
};

} // namespace ebooster
