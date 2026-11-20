#pragma once

#include "ebooster/Common.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>
#include <optional>
#include <functional>

namespace ebooster {

struct ActiveMatchInfo {
    bool isMatchActive{false};
    std::string serverIp;
    uint16_t serverPort{0};
    std::string clusterName;      // e.g. "Madrid Dedicated Cluster"
    std::string locationName;     // e.g. "Madrid, Spain"
    std::string countryCode;      // e.g. "ES"
    std::string countryFlag;      // e.g. "🇪🇸"

    // Matchmaking / Opponent Details
    std::string opponentIp;       // e.g. "34.175.112.88" or peer IP
    std::string opponentLocation; // e.g. "Spain (Madrid)"
    std::string opponentFlag;     // e.g. "🇪🇸"
    std::string matchType;        // "Direct Dedicated Match" or "Direct P2P Match"

    // Dynamic Match Telemetry
    float livePingMs{0.0f};       // Measured or estimated live latency
    uint32_t packetRate{0};       // pkts/sec (e.g. 128 pkts/sec)
    std::string tickrateTier;     // "Competitive High-Tick" (128) or "Standard Match Tickrate" (64/60)

    std::chrono::steady_clock::time_point lastPacketTime;
};

class MatchClassifier {
public:
    struct ResolvedEndpointInfo {
        std::string clusterName;
        std::string locationName;
        std::string countryCode;
        std::string countryFlag;
        bool isDedicatedServer{true};
        std::string matchType;
    };

    // Fast offline IP subnet classifier (0ms latency, zero external API queries)
    static ResolvedEndpointInfo ClassifyIp(const std::string& ip, uint16_t port = 0);
};

class MatchSessionTracker {
public:
    MatchSessionTracker();
    ~MatchSessionTracker();

    // Reset session state
    void Reset();

    // Register an in-game packet from ETW or ConnectionSniffer
    void RegisterPacket(const std::string& remoteIp, uint16_t remotePort, bool isUdp = true);

    // Update fallback packet rate (e.g. from adapter delta when ETW is absent)
    void UpdateFallbackPacketRate(uint32_t pktsPerSec);

    // Update live latency to active match server
    void UpdatePing(float pingMs);

    // Set initial fallback match profile (e.g. eFootball -> Madrid Dedicated Cluster)
    void SetInitialProfileHint(const std::string& gameName, const std::string& defaultServerIp = "");

    // Query active match state for HUD rendering
    ActiveMatchInfo GetActiveMatchInfo() const;

private:
    mutable std::mutex m_mutex;
    mutable ActiveMatchInfo m_matchInfo;

    std::atomic<uint64_t> m_packetsInWindow{0};
    std::atomic<uint32_t> m_computedPacketRate{0};
    std::chrono::steady_clock::time_point m_lastRateCalcTime;
    std::chrono::steady_clock::time_point m_lastPacketTime;
};

} // namespace ebooster
