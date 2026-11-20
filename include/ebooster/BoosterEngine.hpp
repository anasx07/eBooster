#pragma once

#include "ebooster/Common.hpp"
#include "ebooster/ProcessMonitor.hpp"
#include "ebooster/ConnectionSniffer.hpp"
#include "ebooster/EtwSniffer.hpp"
#include "ebooster/RouteController.hpp"
#include "ebooster/DnsRelayController.hpp"
#include "ebooster/MatchSniffer.hpp"

#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace ebooster {

struct EngineConfig {
    std::wstring targetExe = L"eFootball.exe";
    std::vector<std::wstring> childExes;
    std::vector<std::wstring> launcherExes;
    uint32_t processPollMs = 500;
    uint32_t snifferPollMs = 1000;
    std::vector<std::string> clusterCidrs;
    bool enableSmartDns{true};
    std::string upstreamIspDns = "1.1.1.1";
    std::string ispBenchmarkIp = "185.60.112.157";
    bool blockIpv6Leaks{true};
    bool enableKillSwitch{false};
};

class BoosterEngine {
public:
    explicit BoosterEngine(EngineConfig config);
    ~BoosterEngine();

    // Initializes controllers and checks elevation
    bool Initialize();

    // Starts process monitoring and reactive network optimization
    bool Start();

    // Stops engine and cleans up all state safely
    void Stop(bool unused = false);

    // Manual route management
    bool AddManualRoute(const std::string& serverIp);
    bool RemoveManualRoute(const std::string& serverIp);

    // Queries
    bool IsRunning() const noexcept { return m_isRunning.load(); }
    const EngineConfig& GetConfig() const noexcept { return m_config; }
    RouteController& GetRouteController() { return *m_routeController; }
    const RouteController& GetRouteController() const { return *m_routeController; }
    DnsRelayController& GetDnsRelay() { return *m_dnsRelay; }
    const DnsRelayController& GetDnsRelay() const { return *m_dnsRelay; }

    bool IsKillSwitchEngaged() const;
    void SetKillSwitchEnabled(bool enable);

    // Live Opponent & Server Match Sniffer
    ActiveMatchInfo GetActiveMatchInfo() const;

    // Auto-Reconnect Resilience & Link State Watchdog
    bool IsReconnecting() const noexcept { return m_isReconnecting.load(); }
    void TriggerResilienceReconnection();
    void SetResilienceCallback(std::function<void(bool restored)> cb) { m_resilienceCallback = std::move(cb); }

private:
    void OnGameStarted(DWORD pid, const std::wstring& exeName);
    void OnGameStopped(DWORD pid, const std::wstring& exeName);
    void OnEndpointDiscovered(const GameEndpoint& endpoint);
    void OnUdpEndpointDiscovered(const std::string& remoteIp, uint16_t remotePort, DWORD pid);

    void StartNetworkChangeMonitor();
    void StopNetworkChangeMonitor();
    static VOID NETIOAPI_API_ RouteChangeCallback(PVOID callerContext, PMIB_IPFORWARD_ROW2 row, MIB_NOTIFICATION_TYPE notificationType);
    static VOID NETIOAPI_API_ InterfaceChangeCallback(PVOID callerContext, PMIB_IPINTERFACE_ROW row, MIB_NOTIFICATION_TYPE notificationType);

    HANDLE m_hRouteNotify{nullptr};
    HANDLE m_hIfNotify{nullptr};
    std::atomic<bool> m_networkChangeDetected{false};
    std::atomic<bool> m_isReconnecting{false};
    std::mutex m_resilienceMutex;
    std::condition_variable m_resilienceCv;
    std::function<void(bool restored)> m_resilienceCallback;

    EngineConfig m_config;
    std::atomic<bool> m_isRunning{false};

    std::unique_ptr<ProcessMonitor>      m_processMonitor;
    std::unique_ptr<ConnectionSniffer>   m_connectionSniffer;
    std::unique_ptr<EtwSniffer>          m_etwSniffer;
    std::unique_ptr<RouteController>     m_routeController;
    std::unique_ptr<DnsRelayController>  m_dnsRelay;
    std::unique_ptr<MatchSessionTracker> m_matchTracker;

    // m_watchdogThread declared last so it is joined and cancelled BEFORE any controllers are destroyed
    std::jthread m_watchdogThread;
};

} // namespace ebooster
