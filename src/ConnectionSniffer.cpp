#include "ebooster/ConnectionSniffer.hpp"

namespace ebooster {

ConnectionSniffer::ConnectionSniffer(uint32_t pollIntervalMs)
    : m_pollIntervalMs(pollIntervalMs) {
}

ConnectionSniffer::~ConnectionSniffer() {
    StopSniffing();
}

void ConnectionSniffer::SetCallback(EndpointDiscoveredCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onDiscovered = std::move(callback);
}

std::string ConnectionSniffer::TcpStateToString(DWORD state) {
    switch (state) {
        case MIB_TCP_STATE_CLOSED:     return "CLOSED";
        case MIB_TCP_STATE_LISTEN:     return "LISTEN";
        case MIB_TCP_STATE_SYN_SENT:   return "SYN_SENT";
        case MIB_TCP_STATE_SYN_RCVD:   return "SYN_RCVD";
        case MIB_TCP_STATE_ESTAB:      return "ESTABLISHED";
        case MIB_TCP_STATE_FIN_WAIT1:  return "FIN_WAIT1";
        case MIB_TCP_STATE_FIN_WAIT2:  return "FIN_WAIT2";
        case MIB_TCP_STATE_CLOSE_WAIT: return "CLOSE_WAIT";
        case MIB_TCP_STATE_CLOSING:    return "CLOSING";
        case MIB_TCP_STATE_LAST_ACK:   return "LAST_ACK";
        case MIB_TCP_STATE_TIME_WAIT:  return "TIME_WAIT";
        case MIB_TCP_STATE_DELETE_TCB: return "DELETE_TCB";
        default:                       return "UNKNOWN";
    }
}

std::vector<GameEndpoint> ConnectionSniffer::ScanTcpEndpoints(DWORD targetPid) {
    std::vector<GameEndpoint> endpoints;

    DWORD dwSize = 0;
    DWORD ret = GetExtendedTcpTable(nullptr, &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    std::vector<uint8_t> buffer;

    while (ret == ERROR_INSUFFICIENT_BUFFER) {
        buffer.resize(dwSize);
        ret = GetExtendedTcpTable(buffer.data(), &dwSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    }

    if (ret != NO_ERROR || buffer.empty()) {
        return endpoints;
    }

    auto* pTable = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
    for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
        const auto& row = pTable->table[i];

        if (targetPid != 0 && row.dwOwningPid != targetPid) {
            continue;
        }

        std::string remoteIp = Ipv4ToString(row.dwRemoteAddr);
        // Exclude 0.0.0.0, 127.0.0.1, private LAN subnets, and WireGuard subnet
        if (IsPrivateOrLoopbackIp(remoteIp)) {
            continue;
        }

        GameEndpoint ep;
        ep.remoteIp = std::move(remoteIp);
        ep.remotePort = ntohs(static_cast<u_short>(row.dwRemotePort));
        ep.localPort  = ntohs(static_cast<u_short>(row.dwLocalPort));
        ep.protocol   = TransportProtocol::TCP;
        ep.pid        = row.dwOwningPid;
        ep.stateDescription = TcpStateToString(row.dwState);

        endpoints.push_back(std::move(ep));
    }

    return endpoints;
}

std::vector<GameEndpoint> ConnectionSniffer::ScanUdpSockets(DWORD targetPid) {
    std::vector<GameEndpoint> endpoints;

    DWORD dwSize = 0;
    DWORD ret = GetExtendedUdpTable(nullptr, &dwSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    std::vector<uint8_t> buffer;

    while (ret == ERROR_INSUFFICIENT_BUFFER) {
        buffer.resize(dwSize);
        ret = GetExtendedUdpTable(buffer.data(), &dwSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    }

    if (ret != NO_ERROR || buffer.empty()) {
        return endpoints;
    }

    auto* pTable = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buffer.data());
    for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
        const auto& row = pTable->table[i];

        if (targetPid != 0 && row.dwOwningPid != targetPid) {
            continue;
        }

        GameEndpoint ep;
        ep.remoteIp = ""; // UDP row in standard MIB table is locally bound socket
        ep.remotePort = 0;
        ep.localPort  = ntohs(static_cast<u_short>(row.dwLocalPort));
        ep.protocol   = TransportProtocol::UDP;
        ep.pid        = row.dwOwningPid;
        ep.stateDescription = "BOUND";

        endpoints.push_back(std::move(ep));
    }

    return endpoints;
}

bool ConnectionSniffer::StartSniffing(const std::vector<DWORD>& targetPids) {
    if (m_isSniffing.exchange(true)) {
        UpdateTargetPids(targetPids);
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_targetPids = targetPids;
    }

    m_workerThread = std::jthread([this](std::stop_token st) {
        WorkerLoop(st);
    });

    return true;
}

void ConnectionSniffer::UpdateTargetPids(const std::vector<DWORD>& targetPids) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_targetPids = targetPids;
}

void ConnectionSniffer::StopSniffing() {
    if (!m_isSniffing.exchange(false)) {
        return;
    }

    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_cv.notify_all();
        m_workerThread.join();
    }
}

void ConnectionSniffer::ResetDiscoveredEndpoints() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_discoveredRemoteIps.clear();
}

void ConnectionSniffer::WorkerLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        std::vector<DWORD> pids;
        EndpointDiscoveredCallback callback;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            pids = m_targetPids;
            callback = m_onDiscovered;
        }

        for (DWORD pid : pids) {
            auto endpoints = ScanTcpEndpoints(pid);

            for (const auto& ep : endpoints) {
                bool isNew = false;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (!m_discoveredRemoteIps.contains(ep.remoteIp)) {
                        m_discoveredRemoteIps.insert(ep.remoteIp);
                        isNew = true;
                    }
                }

                if (isNew) {
                    LogFmt(LogLevel::Net, "Game server connection detected: {}:{} ({}) [PID: {}]",
                           ep.remoteIp, ep.remotePort, ep.stateDescription, ep.pid);
                    if (callback) {
                        callback(ep);
                    }
                }
            }
        }

        std::unique_lock<std::mutex> cvLock(m_mutex);
        m_cv.wait_for(cvLock, std::chrono::milliseconds(m_pollIntervalMs), [&stopToken] {
            return stopToken.stop_requested();
        });
    }
}

} // namespace ebooster
