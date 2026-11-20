#include "ebooster/RouteController.hpp"
#include "ebooster/DnsRelayController.hpp"
#include "ebooster/NetworkOptimizer.hpp"

namespace ebooster {

RouteController::RouteController(std::string gatewayIp, std::string clientInternalIp, std::string vpsPublicEndpoint)
    : m_gatewayIp(std::move(gatewayIp))
    , m_clientIp(std::move(clientInternalIp))
    , m_vpsPublicEndpoint(std::move(vpsPublicEndpoint))
    , m_leaseThreadRunning(true) {
    m_leaseThread = std::jthread([this](std::stop_token st) {
        while (!st.stop_requested() && m_leaseThreadRunning) {
            std::unique_lock<std::mutex> cvLock(m_leaseMutex);
            m_leaseCv.wait_for(cvLock, std::chrono::seconds(60), [&]() {
                return !m_leaseThreadRunning || st.stop_requested();
            });
            if (!m_leaseThreadRunning || st.stop_requested()) break;
            cvLock.unlock();

            RefreshRouteLeases();
        }
    });
}

RouteController::~RouteController() {
    {
        std::lock_guard<std::mutex> lock(m_leaseMutex);
        m_leaseThreadRunning = false;
    }
    m_leaseCv.notify_all();
    if (m_leaseThread.joinable()) {
        m_leaseThread.request_stop();
        m_leaseThread.join();
    }

    RemoveAllRoutes();
    UnregisterGlobalRollbackHandler();
}

bool RouteController::InitializeAdapter(bool silent) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    DWORD ifIndex = 0;
    IPAddr destIp{};
    inet_pton(AF_INET, "8.8.8.8", &destIp);
    if (GetBestInterface(destIp, &ifIndex) == NO_ERROR) {
        m_wgIfIndex = ifIndex;
        m_adapterInitialized = true;
        if (!silent) {
            LogFmt(LogLevel::Info, "Active physical network interface bound (IfIndex: {})", m_wgIfIndex);
        }
        return true;
    }

    m_adapterInitialized = false;
    return false;
}

size_t RouteController::SweepOrphanRoutes() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    size_t sweptCount = 0;

    PMIB_IPFORWARD_TABLE2 pTable = nullptr;
    DWORD ret = GetIpForwardTable2(AF_INET, &pTable);
    if (ret != NO_ERROR || !pTable) {
        LogFmt(LogLevel::Warn, "Startup Sanitation: Unable to query routing table (Win32 Error: {})", ret);
        return 0;
    }

    for (ULONG i = 0; i < pTable->NumEntries; ++i) {
        const auto& row = pTable->Table[i];
        if (row.DestinationPrefix.Prefix.si_family != AF_INET ||
            row.NextHop.si_family != AF_INET) {
            continue;
        }

        std::string nextHop = InAddrToString(row.NextHop.Ipv4.sin_addr);
        if (nextHop == m_gatewayIp) {
            std::string destIp = InAddrToString(row.DestinationPrefix.Prefix.Ipv4.sin_addr);
            uint8_t prefixLen = row.DestinationPrefix.PrefixLength;

            // Never remove default route 0.0.0.0/0 if somehow pointed here, only specific host/CIDR routes
            if (prefixLen == 0) continue;

            MIB_IPFORWARD_ROW2 delRow = row;
            DWORD delRet = DeleteIpForwardEntry2(&delRow);
            if (delRet == NO_ERROR || delRet == ERROR_NOT_FOUND) {
                ++sweptCount;
                LogFmt(LogLevel::Route, "Startup Sanitation: Swept orphan route {}/{} -> {}",
                       destIp, prefixLen, nextHop);
            }
        }
    }

    FreeMibTable(pTable);

    // Also sweep any orphan Kill Switch blackhole routes (pointing to 127.0.0.1 on loopback)
    PMIB_IPFORWARD_TABLE2 pTableLoop = nullptr;
    if (GetIpForwardTable2(AF_INET, &pTableLoop) == NO_ERROR && pTableLoop) {
        NET_IFINDEX loopbackIdx = GetLoopbackIfIndex();
        for (ULONG i = 0; i < pTableLoop->NumEntries; ++i) {
            const auto& row = pTableLoop->Table[i];
            if (row.InterfaceIndex == loopbackIdx &&
                row.DestinationPrefix.Prefix.si_family == AF_INET &&
                row.NextHop.si_family == AF_INET) {
                std::string nextHop = InAddrToString(row.NextHop.Ipv4.sin_addr);
                if (nextHop == "127.0.0.1") {
                    std::string destIp = InAddrToString(row.DestinationPrefix.Prefix.Ipv4.sin_addr);
                    if (!destIp.starts_with("127.")) {
                        MIB_IPFORWARD_ROW2 delRow = row;
                        if (DeleteIpForwardEntry2(&delRow) == NO_ERROR) {
                            ++sweptCount;
                            LogFmt(LogLevel::Route, "Startup Sanitation: Swept orphan Kill Switch blackhole {}/{} -> 127.0.0.1",
                                   destIp, row.DestinationPrefix.PrefixLength);
                        }
                    }
                }
            }
        }
        FreeMibTable(pTableLoop);
    }

    // Also sweep any orphan IPv6 null routes on loopback (::/1 and 8000::/1) from earlier sessions
    PMIB_IPFORWARD_TABLE2 pTable6 = nullptr;
    if (GetIpForwardTable2(AF_INET6, &pTable6) == NO_ERROR && pTable6) {
        NET_IFINDEX loopbackIdx = GetLoopbackIfIndex();
        for (ULONG i = 0; i < pTable6->NumEntries; ++i) {
            const auto& row = pTable6->Table[i];
            if (row.DestinationPrefix.Prefix.si_family == AF_INET6 &&
                row.InterfaceIndex == loopbackIdx &&
                row.DestinationPrefix.PrefixLength == 1) {
                MIB_IPFORWARD_ROW2 delRow = row;
                if (DeleteIpForwardEntry2(&delRow) == NO_ERROR) {
                    ++sweptCount;
                    Log(LogLevel::Route, "Startup Sanitation: Swept orphan IPv6 null route.");
                }
            }
        }
        FreeMibTable(pTable6);
    }

    if (sweptCount > 0) {
        LogFmt(LogLevel::Success, "Startup Sanitation: Cleaned {} stale/orphan route(s) from prior session.", sweptCount);
    } else {
        Log(LogLevel::Info, "Startup Sanitation: Routing table clean. Zero orphan routes detected.");
    }

    return sweptCount;
}

bool RouteController::AddHostRoute(const std::string& targetIp, uint32_t metric) {
    if (!m_adapterInitialized) {
        if (!InitializeAdapter()) {
            return false;
        }
    }

    IN_ADDR targetAddr{};
    if (!StringToInAddr(targetIp, targetAddr)) {
        LogFmt(LogLevel::Error, "Invalid IPv4 address format: {}", targetIp);
        return false;
    }

    // Safety Loop Guard: Never inject routes to private subnets, loopbacks, or tunnel endpoints
    if (IsPrivateOrLoopbackIp(targetIp)) {
        return false;
    }

    if (targetIp == m_gatewayIp || targetIp == m_clientIp ||
        (!m_vpsPublicEndpoint.empty() && targetIp == m_vpsPublicEndpoint)) {
        LogFmt(LogLevel::Warn, "[Routing Loop Guard] Refusing to inject route for Gateway/Client endpoint {}", targetIp);
        return false;
    }

    IN_ADDR gatewayAddr{};
    if (!StringToInAddr(m_gatewayIp, gatewayAddr)) {
        LogFmt(LogLevel::Error, "Invalid Gateway IPv4 address: {}", m_gatewayIp);
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_killSwitchEngaged) {
        if (m_killSwitchBlackholeRoutes.contains(targetIp)) return true;

        NET_IFINDEX loopbackIdx = GetLoopbackIfIndex();
        NET_LUID loopbackLuid{};
        ConvertInterfaceIndexToLuid(loopbackIdx, &loopbackLuid);

        MIB_IPFORWARD_ROW2 bhRow;
        InitializeIpForwardEntry(&bhRow);
        bhRow.InterfaceIndex = loopbackIdx;
        bhRow.InterfaceLuid  = loopbackLuid;
        bhRow.DestinationPrefix.Prefix.si_family = AF_INET;
        bhRow.DestinationPrefix.Prefix.Ipv4.sin_addr = targetAddr;
        bhRow.DestinationPrefix.PrefixLength = 32;
        bhRow.NextHop.si_family = AF_INET;
        StringToInAddr("127.0.0.1", bhRow.NextHop.Ipv4.sin_addr);
        bhRow.Metric = metric;
        bhRow.Protocol = static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT);
        bhRow.ValidLifetime = ROUTE_LEASE_SECONDS;
        bhRow.PreferredLifetime = ROUTE_LEASE_SECONDS;

        DWORD ret = CreateIpForwardEntry2(&bhRow);
        if (ret == NO_ERROR || ret == ERROR_OBJECT_ALREADY_EXISTS) {
            m_killSwitchBlackholeRoutes[targetIp] = bhRow;
            MIB_IPFORWARD_ROW2 normRow = bhRow;
            normRow.InterfaceIndex = m_wgIfIndex;
            normRow.InterfaceLuid  = m_wgIfLuid;
            normRow.NextHop.Ipv4.sin_addr = gatewayAddr;
            m_savedRoutesBeforeKillSwitch[targetIp] = normRow;
            LogFmt(LogLevel::Route, "[KILL SWITCH] Blackholed new host: {}/32 -> 127.0.0.1", targetIp);
            return true;
        }
        return false;
    }

    if (m_injectedRoutes.contains(targetIp)) {
        // Route already active
        return true;
    }

    MIB_IPFORWARD_ROW2 row;
    InitializeIpForwardEntry(&row);

    row.InterfaceIndex = m_wgIfIndex;
    row.InterfaceLuid  = m_wgIfLuid;

    row.DestinationPrefix.Prefix.si_family = AF_INET;
    row.DestinationPrefix.Prefix.Ipv4.sin_addr = targetAddr;
    row.DestinationPrefix.PrefixLength = 32; // /32 host route for single server

    row.NextHop.si_family = AF_INET;
    row.NextHop.Ipv4.sin_addr = gatewayAddr;

    row.Metric = metric;
    row.Protocol = static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT);
    row.ValidLifetime = ROUTE_LEASE_SECONDS;
    row.PreferredLifetime = ROUTE_LEASE_SECONDS;

    DWORD ret = CreateIpForwardEntry2(&row);
    if (ret == NO_ERROR || ret == ERROR_OBJECT_ALREADY_EXISTS) {
        m_injectedRoutes[targetIp] = row;
        LogFmt(LogLevel::Route, "Injected host route: {}/32 -> {} via WireGuard IfIndex {} (Metric: {})",
               targetIp, m_gatewayIp, m_wgIfIndex, metric);
        return true;
    }

    LogFmt(LogLevel::Error, "Failed to inject route for {} (Win32 Error: {})", targetIp, ret);
    if (ret == ERROR_ACCESS_DENIED) {
        LogFmt(LogLevel::Error, "Access denied: eBooster requires Administrator privileges to modify routing tables.");
    }
    return false;
}

bool RouteController::AddCidrRoute(const std::string& cidr, uint32_t metric) {
    if (!m_adapterInitialized) {
        if (!InitializeAdapter()) {
            return false;
        }
    }

    std::string ipPart = cidr;
    uint8_t prefixLen = 32;

    auto slashPos = cidr.find('/');
    if (slashPos != std::string::npos) {
        ipPart = cidr.substr(0, slashPos);
        try {
            int p = std::stoi(cidr.substr(slashPos + 1));
            if (p < 1 || p > 32) {
                LogFmt(LogLevel::Error, "CIDR prefix out of bounds [1, 32]: {}", p);
                return false;
            }
            prefixLen = static_cast<uint8_t>(p);
        } catch (...) {
            LogFmt(LogLevel::Error, "Malformed CIDR prefix: {}", cidr);
            return false;
        }
    }

    IN_ADDR targetAddr{};
    if (!StringToInAddr(ipPart, targetAddr)) {
        LogFmt(LogLevel::Error, "Invalid CIDR IP format: {}", ipPart);
        return false;
    }

    // Mask out host bits to guarantee canonical network prefix
    uint32_t hostMask = (prefixLen == 32) ? 0xFFFFFFFFU : htonl(~((1ULL << (32 - prefixLen)) - 1));
    targetAddr.s_addr &= hostMask;

    std::string canonicalIp = InAddrToString(targetAddr);
    if (canonicalIp == m_gatewayIp || canonicalIp == m_clientIp ||
        (!m_vpsPublicEndpoint.empty() && canonicalIp == m_vpsPublicEndpoint)) {
        LogFmt(LogLevel::Warn, "[Routing Loop Guard] Refusing to inject CIDR {} for Gateway/Client endpoint", cidr);
        return false;
    }

    IN_ADDR gatewayAddr{};
    if (!StringToInAddr(m_gatewayIp, gatewayAddr)) {
        LogFmt(LogLevel::Error, "Invalid Gateway IPv4 address: {}", m_gatewayIp);
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_killSwitchEngaged) {
        if (m_killSwitchBlackholeRoutes.contains(cidr)) return true;

        NET_IFINDEX loopbackIdx = GetLoopbackIfIndex();
        NET_LUID loopbackLuid{};
        ConvertInterfaceIndexToLuid(loopbackIdx, &loopbackLuid);

        MIB_IPFORWARD_ROW2 bhRow;
        InitializeIpForwardEntry(&bhRow);
        bhRow.InterfaceIndex = loopbackIdx;
        bhRow.InterfaceLuid  = loopbackLuid;
        bhRow.DestinationPrefix.Prefix.si_family = AF_INET;
        bhRow.DestinationPrefix.Prefix.Ipv4.sin_addr = targetAddr;
        bhRow.DestinationPrefix.PrefixLength = prefixLen;
        bhRow.NextHop.si_family = AF_INET;
        StringToInAddr("127.0.0.1", bhRow.NextHop.Ipv4.sin_addr);
        bhRow.Metric = metric;
        bhRow.Protocol = static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT);
        bhRow.ValidLifetime = ROUTE_LEASE_SECONDS;
        bhRow.PreferredLifetime = ROUTE_LEASE_SECONDS;

        DWORD ret = CreateIpForwardEntry2(&bhRow);
        if (ret == NO_ERROR || ret == ERROR_OBJECT_ALREADY_EXISTS) {
            m_killSwitchBlackholeRoutes[cidr] = bhRow;
            MIB_IPFORWARD_ROW2 normRow = bhRow;
            normRow.InterfaceIndex = m_wgIfIndex;
            normRow.InterfaceLuid  = m_wgIfLuid;
            normRow.NextHop.Ipv4.sin_addr = gatewayAddr;
            m_savedRoutesBeforeKillSwitch[cidr] = normRow;
            LogFmt(LogLevel::Route, "[KILL SWITCH] Blackholed new subnet: {} -> 127.0.0.1", cidr);
            return true;
        }
        return false;
    }

    if (m_injectedRoutes.contains(cidr)) {
        return true;
    }

    MIB_IPFORWARD_ROW2 row;
    InitializeIpForwardEntry(&row);

    row.InterfaceIndex = m_wgIfIndex;
    row.InterfaceLuid  = m_wgIfLuid;

    row.DestinationPrefix.Prefix.si_family = AF_INET;
    row.DestinationPrefix.Prefix.Ipv4.sin_addr = targetAddr;
    row.DestinationPrefix.PrefixLength = prefixLen;

    row.NextHop.si_family = AF_INET;
    row.NextHop.Ipv4.sin_addr = gatewayAddr;

    row.Metric = metric;
    row.Protocol = static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT);
    row.ValidLifetime = ROUTE_LEASE_SECONDS;
    row.PreferredLifetime = ROUTE_LEASE_SECONDS;

    DWORD ret = CreateIpForwardEntry2(&row);
    if (ret == NO_ERROR || ret == ERROR_OBJECT_ALREADY_EXISTS) {
        m_injectedRoutes[cidr] = row;
        LogFmt(LogLevel::Route, "Injected cluster CIDR route: {} -> {} via WireGuard (Metric: {})",
               cidr, m_gatewayIp, metric);
        return true;
    }

    LogFmt(LogLevel::Error, "Failed to inject CIDR route for {} (Win32 Error: {})", cidr, ret);
    return false;
}

bool RouteController::HealAndReapplyRoutes() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Refresh adapter in case Wintun interface bounced
    InitializeAdapter(true);

    if (m_wgIfIndex == 0) {
        Log(LogLevel::Warn, "[Resilience] WireGuard interface index not resolved yet; route healing deferred.");
        return false;
    }

    IN_ADDR gwAddr{};
    StringToInAddr(m_gatewayIp, gwAddr);
    size_t healedCount = 0;

    if (m_killSwitchEngaged) {
        for (auto& [dest, savedRow] : m_savedRoutesBeforeKillSwitch) {
            savedRow.InterfaceIndex = m_wgIfIndex;
            savedRow.InterfaceLuid  = m_wgIfLuid;
            savedRow.NextHop.Ipv4.sin_addr = gwAddr;
        }

        for (auto& [dest, bhRow] : m_killSwitchBlackholeRoutes) {
            bhRow.ValidLifetime = ROUTE_LEASE_SECONDS;
            bhRow.PreferredLifetime = ROUTE_LEASE_SECONDS;
            DWORD ret = SetIpForwardEntry2(&bhRow);
            if (ret != NO_ERROR && ret != ERROR_OBJECT_ALREADY_EXISTS) {
                CreateIpForwardEntry2(&bhRow);
            }
        }
        Log(LogLevel::Info, "[Resilience] Maintained kill-switch blackhole isolation during link recovery.");
        return true;
    }

    for (auto& [dest, row] : m_injectedRoutes) {
        row.InterfaceIndex = m_wgIfIndex;
        row.InterfaceLuid  = m_wgIfLuid;
        row.NextHop.si_family = AF_INET;
        row.NextHop.Ipv4.sin_addr = gwAddr;
        row.Metric = 1;
        row.Protocol = static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT);
        row.ValidLifetime = ROUTE_LEASE_SECONDS;
        row.PreferredLifetime = ROUTE_LEASE_SECONDS;

        DWORD ret = SetIpForwardEntry2(&row);
        if (ret != NO_ERROR && ret != ERROR_OBJECT_ALREADY_EXISTS) {
            ret = CreateIpForwardEntry2(&row);
        }

        if (ret == NO_ERROR || ret == ERROR_OBJECT_ALREADY_EXISTS) {
            ++healedCount;
        } else {
            LogFmt(LogLevel::Warn, "[Resilience] Failed to re-apply route for {} (Win32: {})", dest, ret);
        }
    }

    if (m_ipv6LeakProtectionActive) {
        if (m_ipv6Route1.has_value()) {
            MIB_IPFORWARD_ROW2 row = *m_ipv6Route1;
            DWORD ret = SetIpForwardEntry2(&row);
            if (ret != NO_ERROR && ret != ERROR_OBJECT_ALREADY_EXISTS) {
                CreateIpForwardEntry2(&row);
            }
        }
        if (m_ipv6Route2.has_value()) {
            MIB_IPFORWARD_ROW2 row = *m_ipv6Route2;
            DWORD ret = SetIpForwardEntry2(&row);
            if (ret != NO_ERROR && ret != ERROR_OBJECT_ALREADY_EXISTS) {
                CreateIpForwardEntry2(&row);
            }
        }
    }

    LogFmt(LogLevel::Success, "[Resilience] Healed and re-synchronized {} active route(s) onto WireGuard interface {}.",
           healedCount, m_wgIfIndex);
    return true;
}

NET_IFINDEX RouteController::GetLoopbackIfIndex() const {
    ULONG flags = GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG outBufLen = 16384;
    std::vector<uint8_t> buffer(outBufLen);
    auto* pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    ULONG dwRet = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &outBufLen);
    if (dwRet == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        dwRet = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, pAddresses, &outBufLen);
    }

    if (dwRet == NO_ERROR) {
        for (auto* pCurr = pAddresses; pCurr != nullptr; pCurr = pCurr->Next) {
            if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
                return pCurr->IfIndex;
            }
        }
    }
    return 1; // Standard Windows loopback interface index
}

bool RouteController::EnableIpv6LeakProtection() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_ipv6LeakProtectionActive) return true;

    NET_IFINDEX loopbackIdx = GetLoopbackIfIndex();
    NET_LUID loopbackLuid{};
    ConvertInterfaceIndexToLuid(loopbackIdx, &loopbackLuid);

    Log(LogLevel::Info, "Engaging IPv6 Leak Shield (Blackholing ::/1 and 8000::/1)...");

    // Route 1: ::/1
    MIB_IPFORWARD_ROW2 row1;
    InitializeIpForwardEntry(&row1);
    row1.InterfaceIndex = loopbackIdx;
    row1.InterfaceLuid = loopbackLuid;
    row1.DestinationPrefix.Prefix.si_family = AF_INET6;
    row1.DestinationPrefix.PrefixLength = 1;
    row1.NextHop.si_family = AF_INET6;
    row1.Metric = 1;
    row1.Protocol = static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT);
    row1.ValidLifetime = ROUTE_LEASE_SECONDS;
    row1.PreferredLifetime = ROUTE_LEASE_SECONDS;

    // Route 2: 8000::/1
    MIB_IPFORWARD_ROW2 row2 = row1;
    row2.DestinationPrefix.Prefix.Ipv6.sin6_addr.s6_addr[0] = 0x80;

    DWORD ret1 = CreateIpForwardEntry2(&row1);
    DWORD ret2 = CreateIpForwardEntry2(&row2);

    if (ret1 == NO_ERROR || ret1 == ERROR_OBJECT_ALREADY_EXISTS) {
        m_ipv6Route1 = row1;
    }
    if (ret2 == NO_ERROR || ret2 == ERROR_OBJECT_ALREADY_EXISTS) {
        m_ipv6Route2 = row2;
    }

    if ((ret1 == NO_ERROR || ret1 == ERROR_OBJECT_ALREADY_EXISTS) &&
        (ret2 == NO_ERROR || ret2 == ERROR_OBJECT_ALREADY_EXISTS)) {
        m_ipv6LeakProtectionActive = true;
        Log(LogLevel::Success, "IPv6 Leak Shield ACTIVE: Outbound IPv6 blackholed to prevent dual-stack ISP bypass.");
        return true;
    }

    LogFmt(LogLevel::Warn, "IPv6 Leak Shield: Failed to inject null routes via IP Helper (Win32: {}, {})", ret1, ret2);
    return false;
}

bool RouteController::DisableIpv6LeakProtectionUnlocked() {
    if (!m_ipv6LeakProtectionActive) return true;

    if (m_ipv6Route1.has_value()) {
        MIB_IPFORWARD_ROW2 row = *m_ipv6Route1;
        DeleteIpForwardEntry2(&row);
        m_ipv6Route1.reset();
    }
    if (m_ipv6Route2.has_value()) {
        MIB_IPFORWARD_ROW2 row = *m_ipv6Route2;
        DeleteIpForwardEntry2(&row);
        m_ipv6Route2.reset();
    }

    m_ipv6LeakProtectionActive = false;
    Log(LogLevel::Info, "IPv6 Leak Shield DISENGAGED: Normal IPv6 routing restored.");
    return true;
}

bool RouteController::DisableIpv6LeakProtection() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return DisableIpv6LeakProtectionUnlocked();
}

bool RouteController::EnableKillSwitch() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_killSwitchEngaged) return true;

    NET_IFINDEX loopbackIdx = GetLoopbackIfIndex();
    NET_LUID loopbackLuid{};
    ConvertInterfaceIndexToLuid(loopbackIdx, &loopbackLuid);

    Log(LogLevel::Warn, "[KILL SWITCH] Engaging fail-safe blackhole across all game and VPN routes...");

    // Save existing routes
    m_savedRoutesBeforeKillSwitch = m_injectedRoutes;

    // Delete existing routes pointing to interface
    for (auto it = m_injectedRoutes.begin(); it != m_injectedRoutes.end(); ) {
        DeleteIpForwardEntry2(&it->second);
        it = m_injectedRoutes.erase(it);
    }

    // Now inject blackhole routes to Loopback for all previously active targets
    for (const auto& [dest, oldRow] : m_savedRoutesBeforeKillSwitch) {
        MIB_IPFORWARD_ROW2 bhRow;
        InitializeIpForwardEntry(&bhRow);
        bhRow.InterfaceIndex = loopbackIdx;
        bhRow.InterfaceLuid  = loopbackLuid;
        bhRow.DestinationPrefix = oldRow.DestinationPrefix;
        bhRow.NextHop.si_family = AF_INET;
        StringToInAddr("127.0.0.1", bhRow.NextHop.Ipv4.sin_addr);
        bhRow.Metric = 1;
        bhRow.Protocol = static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT);
        bhRow.ValidLifetime = ROUTE_LEASE_SECONDS;
        bhRow.PreferredLifetime = ROUTE_LEASE_SECONDS;

        DWORD ret = CreateIpForwardEntry2(&bhRow);
        if (ret == NO_ERROR || ret == ERROR_OBJECT_ALREADY_EXISTS) {
            m_killSwitchBlackholeRoutes[dest] = bhRow;
        } else {
            LogFmt(LogLevel::Warn, "[KILL SWITCH] Could not inject loopback blackhole for {} (Win32: {})", dest, ret);
        }
    }

    m_killSwitchEngaged = true;
    LogFmt(LogLevel::Error, "[KILL SWITCH ACTIVE] Blackholed {} route(s) to Loopback. Outbound game traffic isolated from ISP fallback.",
           m_killSwitchBlackholeRoutes.size());
    return true;
}

bool RouteController::DisableKillSwitch() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_killSwitchEngaged) return true;

    Log(LogLevel::Info, "[KILL SWITCH] Disengaging fail-safe blackhole. Restoring routes...");

    // Delete blackhole routes from Loopback
    for (auto& [dest, bhRow] : m_killSwitchBlackholeRoutes) {
        DeleteIpForwardEntry2(&bhRow);
    }
    m_killSwitchBlackholeRoutes.clear();

    // Restore saved routes
    size_t restoredCount = 0;
    for (auto& [dest, oldRow] : m_savedRoutesBeforeKillSwitch) {
        oldRow.InterfaceIndex = m_wgIfIndex;
        oldRow.InterfaceLuid  = m_wgIfLuid;
        oldRow.NextHop.si_family = AF_INET;
        StringToInAddr(m_gatewayIp, oldRow.NextHop.Ipv4.sin_addr);
        oldRow.ValidLifetime = ROUTE_LEASE_SECONDS;
        oldRow.PreferredLifetime = ROUTE_LEASE_SECONDS;

        DWORD ret = CreateIpForwardEntry2(&oldRow);
        if (ret == NO_ERROR || ret == ERROR_OBJECT_ALREADY_EXISTS) {
            m_injectedRoutes[dest] = oldRow;
            ++restoredCount;
        } else {
            LogFmt(LogLevel::Warn, "[KILL SWITCH] Failed to restore route for {} -> {} (Win32: {})",
                   dest, m_gatewayIp, ret);
        }
    }
    m_savedRoutesBeforeKillSwitch.clear();
    m_killSwitchEngaged = false;

    LogFmt(LogLevel::Success, "[KILL SWITCH RESTORED] Normal routing active. Successfully restored {} route(s) to relay.", restoredCount);
    return true;
}

void RouteController::RefreshRouteLeases() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_injectedRoutes.empty()) {
        for (auto& [ip, row] : m_injectedRoutes) {
            row.ValidLifetime = ROUTE_LEASE_SECONDS;
            row.PreferredLifetime = ROUTE_LEASE_SECONDS;
            DWORD ret = SetIpForwardEntry2(&row);
            if (ret != NO_ERROR && ret != ERROR_OBJECT_ALREADY_EXISTS) {
                CreateIpForwardEntry2(&row);
            }
        }
    }

    if (m_killSwitchEngaged) {
        for (auto& [ip, row] : m_killSwitchBlackholeRoutes) {
            row.ValidLifetime = ROUTE_LEASE_SECONDS;
            row.PreferredLifetime = ROUTE_LEASE_SECONDS;
            SetIpForwardEntry2(&row);
        }
    }

    if (m_ipv6LeakProtectionActive) {
        if (m_ipv6Route1.has_value()) {
            MIB_IPFORWARD_ROW2 row = *m_ipv6Route1;
            row.ValidLifetime = ROUTE_LEASE_SECONDS;
            row.PreferredLifetime = ROUTE_LEASE_SECONDS;
            SetIpForwardEntry2(&row);
        }
        if (m_ipv6Route2.has_value()) {
            MIB_IPFORWARD_ROW2 row = *m_ipv6Route2;
            row.ValidLifetime = ROUTE_LEASE_SECONDS;
            row.PreferredLifetime = ROUTE_LEASE_SECONDS;
            SetIpForwardEntry2(&row);
        }
    }
}

bool RouteController::RemoveRoute(const std::string& targetIp) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_killSwitchEngaged) {
        m_savedRoutesBeforeKillSwitch.erase(targetIp);
        auto itBh = m_killSwitchBlackholeRoutes.find(targetIp);
        if (itBh != m_killSwitchBlackholeRoutes.end()) {
            DeleteIpForwardEntry2(&itBh->second);
            m_killSwitchBlackholeRoutes.erase(itBh);
            LogFmt(LogLevel::Route, "[KILL SWITCH] Removed blackholed route: {}/32", targetIp);
            return true;
        }
        return false;
    }

    auto it = m_injectedRoutes.find(targetIp);
    if (it == m_injectedRoutes.end()) {
        return false;
    }

    DWORD ret = DeleteIpForwardEntry2(&it->second);
    if (ret == NO_ERROR || ret == ERROR_NOT_FOUND) {
        LogFmt(LogLevel::Route, "Removed host route: {}/32", targetIp);
        m_injectedRoutes.erase(it);
        return true;
    }

    LogFmt(LogLevel::Error, "Failed to remove route for {} (Win32 Error: {})", targetIp, ret);
    return false;
}

size_t RouteController::RemoveAllRoutes() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    size_t count = 0;

    if (m_killSwitchEngaged) {
        for (auto& [dest, bhRow] : m_killSwitchBlackholeRoutes) {
            DeleteIpForwardEntry2(&bhRow);
            ++count;
        }
        m_killSwitchBlackholeRoutes.clear();
        m_savedRoutesBeforeKillSwitch.clear();
        m_killSwitchEngaged = false;
        Log(LogLevel::Info, "[KILL SWITCH] Flushed all blackhole routes.");
    }

    std::vector<std::string> toRemove;
    toRemove.reserve(m_injectedRoutes.size());

    for (auto& [ip, row] : m_injectedRoutes) {
        DWORD ret = DeleteIpForwardEntry2(&row);
        if (ret == NO_ERROR || ret == ERROR_NOT_FOUND) {
            ++count;
        } else {
            std::string displayPrefix = (ip.find('/') != std::string::npos) ? ip : (ip + "/32");
            LogFmt(LogLevel::Warn, "Could not delete route {} (Win32 Error: {})", displayPrefix, ret);
        }
        toRemove.push_back(ip);
    }

    for (const auto& ip : toRemove) {
        m_injectedRoutes.erase(ip);
    }

    m_injectedRoutes.clear();
    DisableIpv6LeakProtectionUnlocked();

    if (count > 0) {
        LogFmt(LogLevel::Route, "Cleaned up {} active host route(s).", count);
    }

    return count;
}

std::vector<RouteInfo> RouteController::GetActiveRoutes() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<RouteInfo> routes;

    if (m_killSwitchEngaged) {
        routes.reserve(m_killSwitchBlackholeRoutes.size());
        for (const auto& [ip, row] : m_killSwitchBlackholeRoutes) {
            RouteInfo info;
            info.destinationIp = ip;
            info.prefixLength  = row.DestinationPrefix.PrefixLength;
            info.gatewayIp     = "127.0.0.1";
            info.ifIndex       = row.InterfaceIndex;
            info.metric        = row.Metric;
            routes.push_back(std::move(info));
        }
        return routes;
    }

    routes.reserve(m_injectedRoutes.size());
    for (const auto& [ip, row] : m_injectedRoutes) {
        RouteInfo info;
        info.destinationIp = ip;
        info.prefixLength  = row.DestinationPrefix.PrefixLength;
        info.gatewayIp     = InAddrToString(row.NextHop.Ipv4.sin_addr);
        info.ifIndex       = row.InterfaceIndex;
        info.metric        = row.Metric;
        routes.push_back(std::move(info));
    }

    return routes;
}

size_t RouteController::GetActiveRouteCount() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_killSwitchEngaged ? m_killSwitchBlackholeRoutes.size() : m_injectedRoutes.size();
}

std::optional<AdapterTrafficStats> RouteController::GetAdapterTrafficStats() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_adapterInitialized || m_wgIfIndex == 0) {
        return std::nullopt;
    }

    MIB_IF_ROW2 ifRow{};
    ifRow.InterfaceIndex = m_wgIfIndex;
    if (GetIfEntry2(&ifRow) == NO_ERROR) {
        AdapterTrafficStats stats;
        stats.inPackets = ifRow.InUcastPkts + ifRow.InNUcastPkts;
        stats.outPackets = ifRow.OutUcastPkts + ifRow.OutNUcastPkts;
        stats.totalPackets = stats.inPackets + stats.outPackets;
        stats.inBytes = ifRow.InOctets;
        stats.outBytes = ifRow.OutOctets;
        stats.totalBytes = stats.inBytes + stats.outBytes;
        stats.inDiscards = ifRow.InDiscards;
        stats.outDiscards = ifRow.OutDiscards;
        stats.inErrors = ifRow.InErrors;
        stats.outErrors = ifRow.OutErrors;
        stats.totalDiscards = stats.inDiscards + stats.outDiscards + stats.inErrors + stats.outErrors;
        return stats;
    }
    return std::nullopt;
}

void RouteController::RegisterGlobalRollbackHandler(RouteController* controllerInstance) {
    s_pInstance = controllerInstance;
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
}

void RouteController::UnregisterGlobalRollbackHandler() {
    if (s_pInstance != nullptr) {
        SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);
        s_pInstance = nullptr;
    }
}

BOOL WINAPI RouteController::ConsoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            if (s_pInstance != nullptr) {
                Log(LogLevel::Warn, "Console shutdown event intercepted! Safely rolling back all injected routes...");
                s_pInstance->DisableKillSwitch();
                s_pInstance->DisableIpv6LeakProtection();
                s_pInstance->RemoveAllRoutes();
            }
            DnsRelayController::EmergencyCleanup();
            NetworkOptimizer::SetEFootballAntiLagShield(false);
            NetworkOptimizer::SetEFootballGeofilter(KonamiRegion::Auto_All);
            return TRUE;
        default:
            return FALSE;
    }
}

} // namespace ebooster
