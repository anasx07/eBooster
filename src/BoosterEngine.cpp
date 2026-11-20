#include "ebooster/BoosterEngine.hpp"
#include "ebooster/NetworkOptimizer.hpp"

namespace ebooster {

BoosterEngine::BoosterEngine(EngineConfig config)
    : m_config(std::move(config)) {
    m_processMonitor     = std::make_unique<ProcessMonitor>(m_config.targetExe, m_config.processPollMs);
    m_connectionSniffer  = std::make_unique<ConnectionSniffer>(m_config.snifferPollMs);
    m_etwSniffer         = std::make_unique<EtwSniffer>();
    m_routeController    = std::make_unique<RouteController>();
    m_dnsRelay           = std::make_unique<DnsRelayController>("", static_cast<uint16_t>(53), m_config.upstreamIspDns);
    m_matchTracker       = std::make_unique<MatchSessionTracker>();
}

BoosterEngine::~BoosterEngine() {
    Stop();
}

bool BoosterEngine::Initialize() {
    Log(LogLevel::Info, "==========================================================");
    Log(LogLevel::Info, "       eBooster - Low Latency Game Traffic Relay          ");
    Log(LogLevel::Info, "==========================================================");

    if (!IsProcessElevated()) {
        Log(LogLevel::Error, "CRITICAL: eBooster is NOT running with Administrator privileges!");
        Log(LogLevel::Warn,  "Windows IP Helper API (CreateIpForwardEntry2) will fail without elevation.");
        Log(LogLevel::Warn,  "Please re-launch this terminal or application as Administrator.");
        return false;
    }

    // Register Ctrl+C / Close signal handler to guarantee route rollback
    RouteController::RegisterGlobalRollbackHandler(m_routeController.get());

    // Connect callbacks
    m_processMonitor->SetCallbacks(
        [this](DWORD pid, const std::wstring& exe) { OnGameStarted(pid, exe); },
        [this](DWORD pid, const std::wstring& exe) { OnGameStopped(pid, exe); }
    );
    m_processMonitor->SetChildTargets(m_config.childExes);
    m_processMonitor->SetLauncherTargets(m_config.launcherExes);

    m_connectionSniffer->SetCallback(
        [this](const GameEndpoint& ep) { OnEndpointDiscovered(ep); }
    );

    m_etwSniffer->SetCallback(
        [this](const std::string& ip, uint16_t port, DWORD pid) { OnUdpEndpointDiscovered(ip, port, pid); }
    );
    m_etwSniffer->SetPacketCallback(
        [this](const std::string& ip, uint16_t port, DWORD /*pid*/) {
            if (m_matchTracker) {
                m_matchTracker->RegisterPacket(ip, port, true);
            }
        }
    );

    Log(LogLevel::Info, "eBooster running in Native Network Optimizer & Server Lock Mode (Zero VPN / Anti-Cheat Safe).");

    // Startup Sanitation: Sweep any stale orphan routes, DNS bindings & shields from prior crashes
    m_routeController->SweepOrphanRoutes();
    DnsRelayController::SanitizeDnsOnStartup();
    NetworkOptimizer::SetEFootballAntiLagShield(false);

    return true;
}

bool BoosterEngine::Start() {
    if (m_isRunning.exchange(true)) {
        return false;
    }


    // Engage IPv6 Leak Shield if configured (blackholes dual-stack leaks to force IPv4 relay)
    if (m_config.blockIpv6Leaks && m_routeController) {
        m_routeController->EnableIpv6LeakProtection();
    }

    if (m_config.enableSmartDns && m_dnsRelay) {
        m_dnsRelay->Start();
    }

    // Start Windows IP Helper route & interface change monitor (NotifyRouteChange2)
    StartNetworkChangeMonitor();

    // Launch watchdog thread for Auto-Reconnect Resilience & Gaming Kill Switch
    m_watchdogThread = std::jthread([this](std::stop_token st) {
        uint32_t consecutiveFailures = 0;
        while (!st.stop_requested()) {
            {
                std::unique_lock<std::mutex> lock(m_resilienceMutex);
                m_resilienceCv.wait_for(lock, std::chrono::milliseconds(1500), [&]() {
                    return st.stop_requested() || m_networkChangeDetected.load(std::memory_order_relaxed);
                });
            }
            if (st.stop_requested()) break;

            if (!m_isRunning.load()) {
                continue;
            }

            bool networkChanged = m_networkChangeDetected.exchange(false);
            std::string benchTarget = m_config.ispBenchmarkIp.empty() ? "1.1.1.1" : m_config.ispBenchmarkIp;
            auto rtt = ebooster::PingHost(benchTarget, 800);

            if (networkChanged || !rtt.has_value()) {
                if (networkChanged) {
                    Log(LogLevel::Warn, "[Network Watchdog] OS Network Interface/Route Change detected!");
                }
                if (!rtt.has_value()) {
                    ++consecutiveFailures;
                } else {
                    consecutiveFailures = 0;
                }

                if (networkChanged || consecutiveFailures >= 2) {
                    m_isReconnecting.store(true);
                    bool physicalUp = false;
                    for (int retry = 0; retry < 6 && !st.stop_requested(); ++retry) {
                        auto probeRtt = ebooster::PingHost(benchTarget, 600);
                        if (probeRtt.has_value()) {
                            physicalUp = true;
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }

                    if (physicalUp) {
                        consecutiveFailures = 0;
                        m_isReconnecting.store(false);
                        Log(LogLevel::Success, "[Network Watchdog] Physical network link ACTIVE & STABLE.");
                        if (m_resilienceCallback) {
                            m_resilienceCallback(true);
                        }
                    }
                }
            } else {
                if (m_isReconnecting.load()) {
                    m_isReconnecting.store(false);
                }
                consecutiveFailures = 0;
            }

            // Periodic live match sniffer telemetry update
            if (m_matchTracker && m_routeController) {
                auto stats = m_routeController->GetAdapterTrafficStats();
                if (stats.has_value()) {
                    static uint64_t s_prevPkts = 0;
                    uint64_t curPkts = stats->totalPackets;
                    uint32_t deltaPkts = (curPkts >= s_prevPkts) ? static_cast<uint32_t>(curPkts - s_prevPkts) : 0;
                    s_prevPkts = curPkts;
                    m_matchTracker->UpdateFallbackPacketRate(deltaPkts);
                }

                auto match = m_matchTracker->GetActiveMatchInfo();
                if (!match.serverIp.empty()) {
                    auto matchRtt = ebooster::PingHost(match.serverIp, 300);
                    if (matchRtt.has_value()) {
                        m_matchTracker->UpdatePing(static_cast<float>(*matchRtt));
                    }
                }
            }
        }
    });

    LogFmt(LogLevel::Info, "Starting Process Monitor for target: '{}'...", ToNarrowString(m_config.targetExe));
    m_processMonitor->Start();

    // Check if game is already running at startup
    auto existingPids = m_processMonitor->GetActivePids();
    if (!existingPids.empty()) {
        LogFmt(LogLevel::Success, "Target game is ALREADY running at launch (PID: {}). Initiating sniffing...",
               existingPids.front());
        

        if (m_matchTracker) {
            m_matchTracker->SetInitialProfileHint(ToNarrowString(m_config.targetExe), "34.175.112.88");
        }

        m_connectionSniffer->StartSniffing(existingPids);
        m_etwSniffer->Start(existingPids);
    }

    return true;
}

void BoosterEngine::Stop([[maybe_unused]] bool unused) {
    if (!m_isRunning.exchange(false)) {
        return;
    }

    Log(LogLevel::Info, "Shutting down eBooster engine...");
    StopNetworkChangeMonitor();
    m_resilienceCv.notify_all();

    try {
        if (m_watchdogThread.joinable()) {
            m_watchdogThread.request_stop();
            m_watchdogThread.join();
        }
    } catch (...) {}

    try {
        if (m_dnsRelay) {
            m_dnsRelay->Stop();
        }
    } catch (...) {}

    try {
        if (m_processMonitor) {
            m_processMonitor->Stop();
        }
    } catch (...) {}

    try {
        if (m_connectionSniffer) {
            m_connectionSniffer->StopSniffing();
            m_connectionSniffer->ResetDiscoveredEndpoints();
        }
    } catch (...) {}

    try {
        if (m_etwSniffer) {
            m_etwSniffer->Stop();
            m_etwSniffer->ResetDiscoveredEndpoints();
        }
    } catch (...) {}

    try {
        if (m_routeController) {
            m_routeController->DisableKillSwitch();
            m_routeController->DisableIpv6LeakProtection();
            m_routeController->RemoveAllRoutes();
        }
    } catch (...) {}


    // Sweep any game-specific anti-lag firewall rules on engine stop
    try {
        std::wstring lowerTarget = m_config.targetExe;
        std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), ::towlower);
        if (lowerTarget.find(L"efootball") != std::wstring::npos) {
            NetworkOptimizer::SetEFootballAntiLagShield(false);
        }
    } catch (...) {}

    Log(LogLevel::Success, "eBooster engine stopped cleanly. All host routes rolled back.");
}

void BoosterEngine::OnGameStarted(DWORD pid, const std::wstring& exeName) {
    LogFmt(LogLevel::Process, "Target game process active: {} [PID: {}]", ToNarrowString(exeName), pid);
    
    // Elevate game process priority for zero input jitter & optimal frame pacing
    HANDLE hProc = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (hProc) {
        SetPriorityClass(hProc, HIGH_PRIORITY_CLASS);
        CloseHandle(hProc);
    }

    // Check if target is eFootball and engage Anti-TCP Lag Shield
    std::wstring lowerExe = exeName;
    std::transform(lowerExe.begin(), lowerExe.end(), lowerExe.begin(), ::towlower);
    if (lowerExe.find(L"efootball") != std::wstring::npos) {
        wchar_t fullPath[MAX_PATH] = {0};
        DWORD sz = MAX_PATH;
        HANDLE hQuery = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hQuery) {
            if (QueryFullProcessImageNameW(hQuery, 0, fullPath, &sz)) {
                NetworkOptimizer::SetEFootballAntiLagShield(true, fullPath);
            }
            CloseHandle(hQuery);
        } else {
            NetworkOptimizer::SetEFootballAntiLagShield(true);
        }
    }


    // Update target PIDs and start background socket & ETW packet sniffing
    auto activePids = m_processMonitor->GetActivePids();
    if (activePids.empty()) {
        activePids.push_back(pid);
    }

    if (m_matchTracker) {
        std::string narrowExe = ToNarrowString(exeName);
        std::transform(narrowExe.begin(), narrowExe.end(), narrowExe.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        m_matchTracker->SetInitialProfileHint(narrowExe, "34.175.112.88");
    }

    m_connectionSniffer->StartSniffing(activePids);
    m_etwSniffer->Start(activePids);
}

void BoosterEngine::OnGameStopped(DWORD pid, const std::wstring& exeName) {
    LogFmt(LogLevel::Process, "Target game process exited: {} [PID: {}]", ToNarrowString(exeName), pid);

    auto remainingPids = m_processMonitor->GetActivePids();
    if (remainingPids.empty()) {
        m_connectionSniffer->StopSniffing();
        m_connectionSniffer->ResetDiscoveredEndpoints();
        m_etwSniffer->Stop();
        m_etwSniffer->ResetDiscoveredEndpoints();

        if (m_matchTracker) {
            m_matchTracker->Reset();
        }

        if (m_routeController) {
            size_t count = m_routeController->RemoveAllRoutes();
            if (count > 0) {
                LogFmt(LogLevel::Success, "Cleaned up {} routes. Regular traffic continues unaffected.", count);
            }
        }
    } else {
        m_connectionSniffer->UpdateTargetPids(remainingPids);
        m_etwSniffer->UpdateTargetPids(remainingPids);
    }
}

void BoosterEngine::OnEndpointDiscovered(const GameEndpoint& endpoint) {
    if (endpoint.remoteIp.empty()) {
        return;
    }

    if (m_matchTracker) {
        m_matchTracker->RegisterPacket(endpoint.remoteIp, endpoint.remotePort, endpoint.protocol == TransportProtocol::UDP);
    }
}

void BoosterEngine::OnUdpEndpointDiscovered(const std::string& remoteIp, uint16_t remotePort, DWORD pid) {
    if (remoteIp.empty()) {
        return;
    }

    if (m_matchTracker) {
        m_matchTracker->RegisterPacket(remoteIp, remotePort, true);
    }

    LogFmt(LogLevel::Net, "[ETW] Outbound UDP Datagram detected from game PID {}: {}:{}",
           pid, remoteIp, remotePort);
}

ActiveMatchInfo BoosterEngine::GetActiveMatchInfo() const {
    if (!m_matchTracker) {
        return ActiveMatchInfo{};
    }
    return m_matchTracker->GetActiveMatchInfo();
}

bool BoosterEngine::AddManualRoute(const std::string& serverIp) {
    return m_routeController->AddHostRoute(serverIp, 1);
}

bool BoosterEngine::RemoveManualRoute(const std::string& serverIp) {
    return m_routeController->RemoveRoute(serverIp);
}

bool BoosterEngine::IsKillSwitchEngaged() const {
    return m_routeController && m_routeController->IsKillSwitchEngaged();
}

void BoosterEngine::SetKillSwitchEnabled(bool enable) {
    m_config.enableKillSwitch = enable;
    if (!enable && m_routeController && m_routeController->IsKillSwitchEngaged()) {
        m_routeController->DisableKillSwitch();
    }
}

VOID NETIOAPI_API_ BoosterEngine::RouteChangeCallback(PVOID callerContext, PMIB_IPFORWARD_ROW2 row, MIB_NOTIFICATION_TYPE /*notificationType*/) {
    if (!callerContext) return;
    auto* self = static_cast<BoosterEngine*>(callerContext);

    // Filter out lease renewal notifications made by eBooster itself
    if (row && row->Protocol == MIB_IPPROTO_NETMGMT) {
        return;
    }

    self->m_networkChangeDetected.store(true, std::memory_order_release);
    self->m_resilienceCv.notify_one();
}

VOID NETIOAPI_API_ BoosterEngine::InterfaceChangeCallback(PVOID callerContext, PMIB_IPINTERFACE_ROW /*row*/, MIB_NOTIFICATION_TYPE /*notificationType*/) {
    if (!callerContext) return;
    auto* self = static_cast<BoosterEngine*>(callerContext);

    self->m_networkChangeDetected.store(true, std::memory_order_release);
    self->m_resilienceCv.notify_one();
}

void BoosterEngine::StartNetworkChangeMonitor() {
    StopNetworkChangeMonitor();

    DWORD status = NotifyRouteChange2(AF_INET, (PIPFORWARD_CHANGE_CALLBACK)RouteChangeCallback, this, FALSE, &m_hRouteNotify);
    if (status != NO_ERROR) {
        LogFmt(LogLevel::Warn, "[Resilience] NotifyRouteChange2 registration failed (Win32: {})", status);
        m_hRouteNotify = nullptr;
    }

    status = NotifyIpInterfaceChange(AF_INET, (PIPINTERFACE_CHANGE_CALLBACK)InterfaceChangeCallback, this, FALSE, &m_hIfNotify);
    if (status != NO_ERROR) {
        LogFmt(LogLevel::Warn, "[Resilience] NotifyIpInterfaceChange registration failed (Win32: {})", status);
        m_hIfNotify = nullptr;
    }

    if (m_hRouteNotify || m_hIfNotify) {
        Log(LogLevel::Info, "[Resilience] OS Network Change & Route Watchdog active (Instant Auto-Reconnect).");
    }
}

void BoosterEngine::StopNetworkChangeMonitor() {
    if (m_hRouteNotify) {
        CancelMibChangeNotify2(m_hRouteNotify);
        m_hRouteNotify = nullptr;
    }
    if (m_hIfNotify) {
        CancelMibChangeNotify2(m_hIfNotify);
        m_hIfNotify = nullptr;
    }
}

void BoosterEngine::TriggerResilienceReconnection() {
    Log(LogLevel::Info, "[Resilience] Manual Network Re-Sync & Route Healing triggered.");
    m_networkChangeDetected.store(true, std::memory_order_release);
    m_resilienceCv.notify_one();
}

} // namespace ebooster
