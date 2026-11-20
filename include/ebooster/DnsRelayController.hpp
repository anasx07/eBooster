#pragma once

#include "ebooster/Common.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

namespace ebooster {

struct DnsStats {
    uint64_t totalQueries{0};
    uint64_t gameQueriesVps{0};
    uint64_t regularQueriesIsp{0};
    uint64_t fallbackQueries{0};
};

class DnsRelayController {
public:
    explicit DnsRelayController(std::string vpsDnsIp = "10.66.66.1",
                                uint16_t vpsDnsPort = 53,
                                std::string upstreamIspDns = "1.1.1.1",
                                uint16_t listenPort = 53);
    ~DnsRelayController();

    // Non-copyable, non-movable
    DnsRelayController(const DnsRelayController&) = delete;
    DnsRelayController& operator=(const DnsRelayController&) = delete;

    bool Start();
    void Stop();

    bool IsRunning() const noexcept { return m_isRunning.load(); }
    DnsStats GetStats() const;

    static void FlushDnsCache();
    static const std::vector<std::string>& GetDefaultGameSuffixes();

    // Startup Sanitation & Crash Recovery
    static size_t SanitizeDnsOnStartup();
    static bool RestoreDnsFromBackupFile();
    static void SaveDnsBackup(const std::wstring& adapterName, bool wasDhcp, const std::vector<std::string>& servers);
    static void ClearDnsBackup();
    static void EmergencyCleanup();

private:
    static inline DnsRelayController* s_pInstance{nullptr};
    void ServerLoop(std::stop_token stopToken);
    void HandleQuery(SOCKET s, const uint8_t* queryBuf, int queryLen, const sockaddr_in& clientAddr);
    int ForwardQuery(const uint8_t* queryBuf, int queryLen,
                     const std::string& upstreamIp, uint16_t upstreamPort,
                     uint8_t* respBuf, int respBufCap, int timeoutMs);

    bool ConfigureAdapterDns(bool enable);
    bool IsGameDomain(const std::string& domain) const;
    std::string ExtractDomainName(const uint8_t* buf, int len, int offset = 12) const;

    std::string m_vpsDnsIp;
    uint16_t m_vpsDnsPort;
    std::string m_upstreamIspDns;
    uint16_t m_listenPort;

    std::vector<std::string> m_gameSuffixes;
    std::atomic<bool> m_isRunning{false};
    std::jthread m_workerThread;
    SOCKET m_listenSocket{INVALID_SOCKET};

    mutable std::mutex m_mutex;
    DnsStats m_stats;

    // Original adapter configuration backup for clean rollback
    std::wstring m_adapterName;
    bool m_wasDhcp{true};
    std::vector<std::string> m_originalDnsServers;
    bool m_adapterConfigured{false};
};

} // namespace ebooster
