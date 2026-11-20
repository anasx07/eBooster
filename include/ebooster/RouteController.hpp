#pragma once

#include "ebooster/Common.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <thread>
#include <condition_variable>

namespace ebooster {

struct RouteInfo {
    std::string destinationIp;
    uint8_t prefixLength{32};
    std::string gatewayIp;
    NET_IFINDEX ifIndex{0};
    uint32_t metric{1};
};

struct AdapterTrafficStats {
    uint64_t inPackets{0};
    uint64_t outPackets{0};
    uint64_t totalPackets{0};
    uint64_t inBytes{0};
    uint64_t outBytes{0};
    uint64_t totalBytes{0};
    uint64_t inDiscards{0};
    uint64_t outDiscards{0};
    uint64_t inErrors{0};
    uint64_t outErrors{0};
    uint64_t totalDiscards{0};
};

class RouteController {
public:
    explicit RouteController(std::string gatewayIp = "10.66.66.1",
                             std::string clientInternalIp = "10.66.66.2",
                             std::string vpsPublicEndpoint = "");
    ~RouteController();

    // Non-copyable, non-movable
    RouteController(const RouteController&) = delete;
    RouteController& operator=(const RouteController&) = delete;

    // Discovers the WireGuard network adapter by client internal IP or adapter description
    bool InitializeAdapter(bool silent = false);

    // Startup Sanitation: Sweeps any stale orphan routes pointing to gateway leftover from earlier crashes
    size_t SweepOrphanRoutes();

    // Injects a /32 host route to target IP via 10.66.66.1 with finite lease
    bool AddHostRoute(const std::string& targetIp, uint32_t metric = 1);

    // Injects a CIDR subnet route (e.g. "155.133.224.0/19") via 10.66.66.1 with finite lease
    bool AddCidrRoute(const std::string& cidr, uint32_t metric = 1);

    // Periodic lease renewal
    void RefreshRouteLeases();

    // IPv6 Leak Shield (blocks dual-stack IPv6 traffic bypasses during active boost)
    bool EnableIpv6LeakProtection();
    bool DisableIpv6LeakProtection();
    bool IsIpv6LeakProtectionActive() const noexcept { return m_ipv6LeakProtectionActive; }
    NET_IFINDEX GetLoopbackIfIndex() const;

    // Gaming Kill Switch (Fail-Safe Disconnect)
    bool EnableKillSwitch();
    bool DisableKillSwitch();
    bool IsKillSwitchEngaged() const noexcept { return m_killSwitchEngaged; }

    // Auto-Reconnect Resilience & Self-Healing Routes
    bool HealAndReapplyRoutes();

    // Rollback / Removal mechanisms
    bool RemoveRoute(const std::string& targetIp);
    size_t RemoveAllRoutes();
    size_t RemoveGameRoutes() { return RemoveAllRoutes(); }

    // Status queries
    std::vector<RouteInfo> GetActiveRoutes() const;
    size_t GetActiveRouteCount() const;

    NET_IFINDEX GetWireGuardIfIndex() const noexcept { return m_wgIfIndex; }
    std::optional<AdapterTrafficStats> GetAdapterTrafficStats() const;

    // Installs global Ctrl+C / Console close hook to ensure routes are NEVER orphaned
    static void RegisterGlobalRollbackHandler(RouteController* controllerInstance);
    static void UnregisterGlobalRollbackHandler();

private:
    static constexpr ULONG ROUTE_LEASE_SECONDS = 600; // 10 minutes finite kernel lease

    std::string m_gatewayIp;
    std::string m_clientIp;
    std::string m_vpsPublicEndpoint;

    NET_IFINDEX m_wgIfIndex{0};
    NET_LUID    m_wgIfLuid{};
    std::wstring m_adapterFriendlyName;
    bool m_adapterInitialized{false};

    bool m_ipv6LeakProtectionActive{false};
    std::optional<MIB_IPFORWARD_ROW2> m_ipv6Route1;
    std::optional<MIB_IPFORWARD_ROW2> m_ipv6Route2;

    bool m_killSwitchEngaged{false};
    std::unordered_map<std::string, MIB_IPFORWARD_ROW2> m_killSwitchBlackholeRoutes;
    std::unordered_map<std::string, MIB_IPFORWARD_ROW2> m_savedRoutesBeforeKillSwitch;

    mutable std::recursive_mutex m_mutex;
    // Map of target IP -> MIB_IPFORWARD_ROW2
    std::unordered_map<std::string, MIB_IPFORWARD_ROW2> m_injectedRoutes;

    std::mutex m_leaseMutex;
    std::condition_variable m_leaseCv;
    bool m_leaseThreadRunning{false};
    std::jthread m_leaseThread;

    bool DisableIpv6LeakProtectionUnlocked();

    static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);
    static inline RouteController* s_pInstance{nullptr};
};

} // namespace ebooster
