#include "ebooster/DnsRelayController.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace ebooster {

namespace {

bool RunSystemNetsh(const std::wstring& args) {
    wchar_t sysDir[MAX_PATH];
    if (GetSystemDirectoryW(sysDir, MAX_PATH) == 0) {
        return false;
    }
    std::wstring netshPath = std::wstring(sysDir) + L"\\netsh.exe";
    std::wstring fullCmd = L"\"" + netshPath + L"\" " + args;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdBuf(fullCmd.begin(), fullCmd.end());
    cmdBuf.push_back(L'\0');

    BOOL ok = CreateProcessW(netshPath.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) return false;

    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, 3000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    return exitCode == 0;
}

} // namespace

const std::vector<std::string>& DnsRelayController::GetDefaultGameSuffixes() {
    static const std::vector<std::string> s_suffixes = {
        // Valve / Counter-Strike 2 / Dota 2 / Steam SDR
        "valve.net",
        "valvesoftware.com",
        "counter-strike.net",
        "steamserver.net",
        "steampowered.com",
        "steamcommunity.com",

        // Riot Games / Valorant / League of Legends
        "pvp.net",
        "riotgames.com",
        "leagueoflegends.com",
        "riotcdn.net",

        // Electronic Arts / Apex Legends
        "ea.com",
        "respawn.com",
        "origin.com",
        "pulse.ea.com",

        // Epic Games / Fortnite / Rocket League
        "epicgames.com",
        "ol.epicgames.com",
        "ps.epicgames.com",

        // Activision / Blizzard / Call of Duty / Warzone / Overwatch
        "activision.com",
        "blizzard.com",
        "battle.net",
        "callofduty.com",
        "demonware.net",

        // Ubisoft Connect / Rainbow Six
        "ubisoft.com",
        "ubi.com"
    };
    return s_suffixes;
}

DnsRelayController::DnsRelayController(std::string vpsDnsIp,
                                       uint16_t vpsDnsPort,
                                       std::string upstreamIspDns,
                                       uint16_t listenPort)
    : m_vpsDnsIp(std::move(vpsDnsIp))
    , m_vpsDnsPort(vpsDnsPort)
    , m_upstreamIspDns(std::move(upstreamIspDns))
    , m_listenPort(listenPort)
    , m_gameSuffixes(GetDefaultGameSuffixes()) {
}

DnsRelayController::~DnsRelayController() {
    Stop();
}

bool DnsRelayController::Start() {
    if (m_isRunning.exchange(true)) {
        return true;
    }

    // Initialize Winsock UDP socket on 127.0.0.1:listenPort
    m_listenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_listenSocket == INVALID_SOCKET) {
        LogFmt(LogLevel::Error, "Smart DNS Relay: Failed to create UDP socket (Win32 Error: {})", WSAGetLastError());
        m_isRunning = false;
        return false;
    }

    BOOL reuse = TRUE;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(m_listenPort);
    StringToInAddr("127.0.0.1", bindAddr.sin_addr);

    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
        LogFmt(LogLevel::Error, "Smart DNS Relay: Failed to bind to 127.0.0.1:{} (Win32 Error: {}). Ensure port 53 is free.",
               m_listenPort, WSAGetLastError());
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        m_isRunning = false;
        return false;
    }

    // Configure the primary network adapter to point to 127.0.0.1
    ConfigureAdapterDns(true);
    FlushDnsCache();

    s_pInstance = this;

    LogFmt(LogLevel::Success, "Smart DNS Relay started on 127.0.0.1:{} (Game GeoDNS -> {}:{}, Regular Web -> {}:53)",
           m_listenPort, m_vpsDnsIp, m_vpsDnsPort, m_upstreamIspDns);

    m_workerThread = std::jthread([this](std::stop_token st) {
        ServerLoop(st);
    });

    return true;
}

void DnsRelayController::Stop() {
    if (!m_isRunning.exchange(false)) {
        return;
    }

    // Close listening socket to unblock any pending recvfrom
    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }

    // Restore original network adapter DNS configuration
    ConfigureAdapterDns(false);
    FlushDnsCache();

    if (s_pInstance == this) {
        s_pInstance = nullptr;
    }

    Log(LogLevel::Info, "Smart DNS Relay stopped cleanly. Restored original Windows DNS configuration.");
}

DnsStats DnsRelayController::GetStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void DnsRelayController::FlushDnsCache() {
    HMODULE hDnsApi = LoadLibraryW(L"dnsapi.dll");
    if (hDnsApi) {
        using DnsFlushProc = BOOL(WINAPI*)();
        auto pfnFlush = reinterpret_cast<DnsFlushProc>(reinterpret_cast<void*>(GetProcAddress(hDnsApi, "DnsFlushResolverCache")));
        if (pfnFlush) {
            pfnFlush();
        }
        FreeLibrary(hDnsApi);
    }
}

std::string DnsRelayController::ExtractDomainName(const uint8_t* buf, int len, int offset) const {
    std::string domain;
    int i = offset;
    while (i < len) {
        uint8_t labelLen = buf[i++];
        if (labelLen == 0) break;
        if ((labelLen & 0xC0) == 0xC0) {
            // Pointer compression in question section
            break;
        }
        if (i + labelLen > len) break;
        if (!domain.empty()) domain += '.';
        domain.append(reinterpret_cast<const char*>(&buf[i]), labelLen);
        i += labelLen;
    }
    return domain;
}

bool DnsRelayController::IsGameDomain(const std::string& domain) const {
    if (domain.empty()) return false;

    std::string lowerDomain = domain;
    std::transform(lowerDomain.begin(), lowerDomain.end(), lowerDomain.begin(),
                   [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& suffix : m_gameSuffixes) {
        std::string lowerSuffix = suffix;
        std::transform(lowerSuffix.begin(), lowerSuffix.end(), lowerSuffix.begin(),
                       [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

        if (lowerDomain == lowerSuffix) {
            return true;
        }
        if (lowerDomain.size() > lowerSuffix.size() &&
            lowerDomain.ends_with("." + lowerSuffix)) {
            return true;
        }
    }
    return false;
}

int DnsRelayController::ForwardQuery(const uint8_t* queryBuf, int queryLen,
                                     const std::string& upstreamIp, uint16_t upstreamPort,
                                     uint8_t* respBuf, int respBufCap, int timeoutMs) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return -1;

    DWORD tv = static_cast<DWORD>(timeoutMs);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

    sockaddr_in destAddr{};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(upstreamPort);
    StringToInAddr(upstreamIp, destAddr.sin_addr);

    int sent = sendto(s, reinterpret_cast<const char*>(queryBuf), queryLen, 0,
                      reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));
    if (sent <= 0) {
        closesocket(s);
        return -1;
    }

    sockaddr_in fromAddr{};
    int fromLen = sizeof(fromAddr);
    int received = recvfrom(s, reinterpret_cast<char*>(respBuf), respBufCap, 0,
                            reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

    closesocket(s);
    return received;
}

void DnsRelayController::HandleQuery(SOCKET s, const uint8_t* queryBuf, int queryLen, const sockaddr_in& clientAddr) {
    if (queryLen < 12) return;

    std::string domain = ExtractDomainName(queryBuf, queryLen, 12);
    bool isGame = IsGameDomain(domain);

    std::string vpsIp;
    uint16_t vpsPort;
    std::string ispIp;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stats.totalQueries++;
        vpsIp = m_vpsDnsIp;
        vpsPort = m_vpsDnsPort;
        ispIp = m_upstreamIspDns;
    }

    std::vector<uint8_t> respBuf(4096);
    int respLen = -1;

    if (isGame) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.gameQueriesVps++;
        }
        LogFmt(LogLevel::Net, "Smart DNS [GeoDNS]: '{}' -> Forwarded to VPS Relay ({}:{})", domain, vpsIp, vpsPort);

        // Forward to VPS DNS
        respLen = ForwardQuery(queryBuf, queryLen, vpsIp, vpsPort, respBuf.data(), static_cast<int>(respBuf.size()), 1200);

        // Fallback to local ISP if VPS times out or port 53 is closed on relay
        if (respLen <= 0) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stats.fallbackQueries++;
            }
            LogFmt(LogLevel::Warn, "Smart DNS: VPS timeout for '{}', falling back to local ISP ({}:53)", domain, ispIp);
            respLen = ForwardQuery(queryBuf, queryLen, ispIp, 53, respBuf.data(), static_cast<int>(respBuf.size()), 2000);
        }
    } else {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.regularQueriesIsp++;
        }
        // Regular browsing traffic resolves via local ISP
        respLen = ForwardQuery(queryBuf, queryLen, ispIp, 53, respBuf.data(), static_cast<int>(respBuf.size()), 2000);
    }

    if (respLen > 0) {
        sendto(s, reinterpret_cast<const char*>(respBuf.data()), respLen, 0,
               reinterpret_cast<const sockaddr*>(&clientAddr), sizeof(clientAddr));
    }
}

void DnsRelayController::ServerLoop(std::stop_token stopToken) {
    std::vector<uint8_t> buf(4096);

    while (!stopToken.stop_requested() && m_isRunning) {
        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(m_listenSocket, &readFds);

        timeval tv{0, 250000}; // 250ms timeout for checking stopToken
        int sel = select(0, &readFds, nullptr, nullptr, &tv);
        if (sel <= 0) {
            continue;
        }

        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        int recvd = recvfrom(m_listenSocket, reinterpret_cast<char*>(buf.data()),
                             static_cast<int>(buf.size()), 0,
                             reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);

        if (recvd > 0) {
            HandleQuery(m_listenSocket, buf.data(), recvd, clientAddr);
        }
    }
}

bool DnsRelayController::ConfigureAdapterDns(bool enable) {
    // 1. Locate primary active physical internet adapter
    DWORD bestIfIndex = 0;
    IN_ADDR testTarget{};
    StringToInAddr("8.8.8.8", testTarget);
    if (GetBestInterface(testTarget.s_addr, &bestIfIndex) != NO_ERROR) {
        Log(LogLevel::Warn, "Smart DNS: Could not determine primary network interface for DNS binding.");
        return false;
    }

    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_MULTICAST;
    ULONG outBufLen = 16384;
    std::vector<uint8_t> buffer(outBufLen);
    auto* pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    if (GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    }

    std::wstring adapterName;
    bool wasDhcp = true;
    std::vector<std::string> prevServers;

    for (auto* pCurr = pAddresses; pCurr != nullptr; pCurr = pCurr->Next) {
        if (pCurr->IfIndex == bestIfIndex) {
            adapterName = pCurr->FriendlyName ? pCurr->FriendlyName : L"";
            wasDhcp = (pCurr->Dhcpv4Server.lpSockaddr != nullptr);

            for (auto* pDns = pCurr->FirstDnsServerAddress; pDns != nullptr; pDns = pDns->Next) {
                if (pDns->Address.lpSockaddr->sa_family == AF_INET) {
                    auto* sa_in = reinterpret_cast<sockaddr_in*>(pDns->Address.lpSockaddr);
                    std::string ip = InAddrToString(sa_in->sin_addr);
                    if (ip != "127.0.0.1" && ip != "0.0.0.0") {
                        prevServers.push_back(ip);
                    }
                }
            }
            break;
        }
    }

    if (adapterName.empty()) {
        Log(LogLevel::Warn, "Smart DNS: Could not match adapter friendly name for primary interface.");
        return false;
    }

    if (enable) {
        m_adapterName = adapterName;
        m_wasDhcp = wasDhcp;
        m_originalDnsServers = prevServers;

        // Configure adapter DNS to 127.0.0.1
        std::wstring args = L"interface ipv4 set dnsservers name=\"" + m_adapterName + L"\" static 127.0.0.1 primary validate=no";
        bool ok = RunSystemNetsh(args);

        // Also add fallback secondary DNS if available
        std::wstring fallback = !m_originalDnsServers.empty() ? ToWideString(m_originalDnsServers[0]) : L"1.1.1.1";
        std::wstring args2 = L"interface ipv4 add dnsservers name=\"" + m_adapterName + L"\" " + fallback + L" index=2 validate=no";
        RunSystemNetsh(args2);

        m_adapterConfigured = ok;
        if (m_adapterConfigured) {
            SaveDnsBackup(m_adapterName, m_wasDhcp, m_originalDnsServers);
        }
        return m_adapterConfigured;
    } else {
        if (!m_adapterConfigured) return true;

        if (m_wasDhcp || m_originalDnsServers.empty()) {
            std::wstring args = L"interface ipv4 set dnsservers name=\"" + m_adapterName + L"\" source=dhcp";
            RunSystemNetsh(args);
        } else {
            std::wstring args = L"interface ipv4 set dnsservers name=\"" + m_adapterName + L"\" static " +
                               ToWideString(m_originalDnsServers[0]) + L" primary validate=no";
            RunSystemNetsh(args);

            for (size_t i = 1; i < m_originalDnsServers.size(); ++i) {
                std::wstring argsAdd = L"interface ipv4 add dnsservers name=\"" + m_adapterName + L"\" " +
                                      ToWideString(m_originalDnsServers[i]) + L" index=" + std::to_wstring(i + 1) + L" validate=no";
                RunSystemNetsh(argsAdd);
            }
        }

        ClearDnsBackup();
        m_adapterConfigured = false;
        return true;
    }
}

void DnsRelayController::SaveDnsBackup(const std::wstring& adapterName, bool wasDhcp, const std::vector<std::string>& servers) {
    try {
        auto path = ResolveConfigPath("configs/dns_backup.json");
        std::ofstream file(path);
        if (!file.is_open()) return;

        file << "{\n";
        file << "  \"adapterName\": \"" << ToNarrowString(adapterName) << "\",\n";
        file << "  \"wasDhcp\": " << (wasDhcp ? "true" : "false") << ",\n";
        file << "  \"servers\": [";
        for (size_t i = 0; i < servers.size(); ++i) {
            file << "\"" << servers[i] << "\"";
            if (i + 1 < servers.size()) file << ", ";
        }
        file << "]\n";
        file << "}\n";
    } catch (...) {}
}

void DnsRelayController::ClearDnsBackup() {
    try {
        auto path = ResolveConfigPath("configs/dns_backup.json");
        std::error_code ec;
        std::filesystem::remove(path, ec);
    } catch (...) {}
}

bool DnsRelayController::RestoreDnsFromBackupFile() {
    auto path = ResolveConfigPath("configs/dns_backup.json");
    if (!std::filesystem::exists(path)) return false;

    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    std::wstring adapterName;
    bool wasDhcp = true;
    std::vector<std::string> servers;

    while (std::getline(file, line)) {
        if (line.find("\"adapterName\":") != std::string::npos) {
            auto firstQuote = line.find('"', line.find(':'));
            if (firstQuote != std::string::npos) {
                auto secondQuote = line.find('"', firstQuote + 1);
                if (secondQuote != std::string::npos) {
                    adapterName = ToWideString(line.substr(firstQuote + 1, secondQuote - firstQuote - 1));
                }
            }
        } else if (line.find("\"wasDhcp\":") != std::string::npos) {
            wasDhcp = (line.find("true") != std::string::npos);
        } else if (line.find("\"servers\":") != std::string::npos) {
            size_t q1 = line.find('"');
            while (q1 != std::string::npos) {
                size_t q2 = line.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                std::string token = line.substr(q1 + 1, q2 - q1 - 1);
                if (token != "servers" && token.find('.') != std::string::npos) {
                    servers.push_back(token);
                }
                q1 = line.find('"', q2 + 1);
            }
        }
    }
    file.close();

    if (!adapterName.empty()) {
        if (wasDhcp || servers.empty()) {
            std::wstring args = L"interface ipv4 set dnsservers name=\"" + adapterName + L"\" source=dhcp";
            RunSystemNetsh(args);
        } else {
            std::wstring args = L"interface ipv4 set dnsservers name=\"" + adapterName + L"\" static " +
                               ToWideString(servers[0]) + L" primary validate=no";
            RunSystemNetsh(args);
            for (size_t i = 1; i < servers.size(); ++i) {
                std::wstring argsAdd = L"interface ipv4 add dnsservers name=\"" + adapterName + L"\" " +
                                      ToWideString(servers[i]) + L" index=" + std::to_wstring(i + 1) + L" validate=no";
                RunSystemNetsh(argsAdd);
            }
        }
        LogFmt(LogLevel::Success, "DNS Sanitation: Restored adapter '{}' DNS from crash backup (Source: {}).",
               ToNarrowString(adapterName), wasDhcp ? "DHCP" : "Static");
    }

    ClearDnsBackup();
    FlushDnsCache();
    return true;
}

size_t DnsRelayController::SanitizeDnsOnStartup() {
    size_t repairedCount = 0;

    // 1. Check if backup file exists from a crashed/killed session
    if (RestoreDnsFromBackupFile()) {
        ++repairedCount;
    }

    // 2. Scan all physical network adapters for any orphaned 127.0.0.1 DNS bindings
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_MULTICAST;
    ULONG outBufLen = 16384;
    std::vector<uint8_t> buffer(outBufLen);
    auto* pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    if (GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(outBufLen);
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &outBufLen);
    }

    for (auto* pCurr = pAddresses; pCurr != nullptr; pCurr = pCurr->Next) {
        if (pCurr->IfType == IF_TYPE_SOFTWARE_LOOPBACK || pCurr->OperStatus != IfOperStatusUp) {
            continue;
        }

        std::wstring adapterName = pCurr->FriendlyName ? pCurr->FriendlyName : L"";
        if (adapterName.empty() ||
            adapterName.find(L"WireGuard") != std::wstring::npos ||
            adapterName.find(L"ebooster") != std::wstring::npos) {
            continue;
        }

        for (auto* pDns = pCurr->FirstDnsServerAddress; pDns != nullptr; pDns = pDns->Next) {
            if (pDns->Address.lpSockaddr->sa_family == AF_INET) {
                auto* sa_in = reinterpret_cast<sockaddr_in*>(pDns->Address.lpSockaddr);
                std::string ip = InAddrToString(sa_in->sin_addr);
                if (ip == "127.0.0.1") {
                    std::wstring args = L"interface ipv4 set dnsservers name=\"" + adapterName + L"\" source=dhcp";
                    RunSystemNetsh(args);
                    LogFmt(LogLevel::Warn, "DNS Sanitation: Detected orphaned 127.0.0.1 on adapter '{}'. Resetting to DHCP.",
                           ToNarrowString(adapterName));
                    ++repairedCount;
                    break;
                }
            }
        }
    }

    if (repairedCount > 0) {
        FlushDnsCache();
        LogFmt(LogLevel::Success, "DNS Sanitation: Cleared {} orphaned 127.0.0.1 adapter(s). Normal internet restored.", repairedCount);
    } else {
        Log(LogLevel::Info, "DNS Sanitation: Windows DNS clean. Zero orphan 127.0.0.1 adapters detected.");
    }

    return repairedCount;
}

void DnsRelayController::EmergencyCleanup() {
    if (s_pInstance != nullptr) {
        s_pInstance->Stop();
    } else {
        RestoreDnsFromBackupFile();
    }
}

} // namespace ebooster
