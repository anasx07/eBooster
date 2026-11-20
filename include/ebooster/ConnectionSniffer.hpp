#pragma once

#include "ebooster/Common.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace ebooster {

enum class TransportProtocol {
    TCP,
    UDP
};

struct GameEndpoint {
    std::string remoteIp;
    uint16_t remotePort{0};
    uint16_t localPort{0};
    TransportProtocol protocol{TransportProtocol::TCP};
    DWORD pid{0};
    std::string stateDescription;

    bool operator==(const GameEndpoint& other) const {
        return remoteIp == other.remoteIp &&
               remotePort == other.remotePort &&
               protocol == other.protocol &&
               pid == other.pid;
    }
};

using EndpointDiscoveredCallback = std::function<void(const GameEndpoint& endpoint)>;

class ConnectionSniffer {
public:
    explicit ConnectionSniffer(uint32_t pollIntervalMs = 1000);
    ~ConnectionSniffer();

    // Non-copyable, non-movable
    ConnectionSniffer(const ConnectionSniffer&) = delete;
    ConnectionSniffer& operator=(const ConnectionSniffer&) = delete;

    void SetCallback(EndpointDiscoveredCallback callback);

    // Synchronous scans for a given PID
    std::vector<GameEndpoint> ScanTcpEndpoints(DWORD targetPid);
    std::vector<GameEndpoint> ScanUdpSockets(DWORD targetPid);

    // Continuous asynchronous background sniffing for target PIDs
    bool StartSniffing(const std::vector<DWORD>& targetPids);
    void UpdateTargetPids(const std::vector<DWORD>& targetPids);
    void StopSniffing();
    void ResetDiscoveredEndpoints();

private:
    void WorkerLoop(std::stop_token stopToken);
    static std::string TcpStateToString(DWORD state);

    uint32_t m_pollIntervalMs;
    EndpointDiscoveredCallback m_onDiscovered;

    std::atomic<bool> m_isSniffing{false};
    std::jthread m_workerThread;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<DWORD> m_targetPids;
    std::unordered_set<std::string> m_discoveredRemoteIps;
};

} // namespace ebooster
