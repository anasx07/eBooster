#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <windows.h>

namespace ebooster {

enum class KonamiRegion {
    Auto_All = 0,
    Madrid_ES,
    Paris_FR,
    Amsterdam_NL,
    Frankfurt_DE,
    London_GB,
    Milan_IT,
    Zurich_CH,
    Warsaw_PL,
    Helsinki_FI,
    Athens_GR,
    Istanbul_TR,
    Dubai_AE,
    Riyadh_SA,
    Doha_QA,
    Lagos_NG,
    Johannesburg_ZA,
    SaoPaulo_BR,
    Santiago_CL,
    USEast_US,
    USCentral_US,
    USWest_US,
    Tokyo_JP,
    Seoul_KR,
    Singapore_SG,
    HongKong_HK,
    Mumbai_IN,
    Sydney_AU,
    WestEurope_Preset
};

struct KonamiRegionInfo {
    KonamiRegion region{KonamiRegion::Auto_All};
    std::string name;
    std::string country;
    std::string flag;
    std::string desc;
    std::vector<std::string> ips;
    std::string probeIp;
    float livePing{-1.0f};
};

struct OptimizerStatus {
    bool nagleDisabled{false};
    bool throttlingDisabled{false};
    bool responsivenessOptimized{false};
    bool gamingPriorityElevated{false};
    bool mpoDisabled{false};
    bool efootballAntiLagShield{false};
    bool efootballGeofilterActive{false};
    KonamiRegion activeKonamiRegion{KonamiRegion::Auto_All};
    bool allOptimized{false};
    int optimizedAdaptersCount{0};
};

class NetworkOptimizer {
public:
    // Queries current Windows registry gaming settings & shield state
    static OptimizerStatus CheckStatus();

    // Applies zero-lag gaming tweaks (disables Nagle, removes throttling, sets 100% CPU to games, disables MPO)
    static bool ApplyGamingTweaks(std::string& outDetails);

    // Reverts all settings back to default Windows stock configuration
    static bool RevertToDefaults(std::string& outDetails);

    // DWM Multiplane Overlay (MPO) micro-stutter fix
    static bool CheckMpoDisabled();
    static bool SetMpoDisabled(bool disable, std::string& outDetails);

    // eFootball Anti-TCP Lag Shield (Blocks TCP 30000-35000 & 5736 to prevent muddy gameplay)
    static bool CheckEFootballAntiLagShield();
    static bool SetEFootballAntiLagShield(bool enable, const std::wstring& customExePath = L"", std::string* outDetails = nullptr);
    static std::wstring FindEFootballPath();

    // eFootball Dedicated Server Geofilter / Region Lock
    // Selectively blackholes UDP speedtests to non-selected Konami server clusters,
    // forcing 100% of matches to land on the chosen dedicated server (e.g. Istanbul or Lagos).
    static const std::vector<KonamiRegionInfo>& GetKonamiRegions();
    static std::vector<KonamiRegionInfo>& GetKonamiRegionsMutable();
    static bool CheckEFootballGeofilterActive();
    static KonamiRegion GetCurrentEFootballGeofilter();
    static bool SetEFootballGeofilter(KonamiRegion region, const std::wstring& customExePath = L"", std::string* outDetails = nullptr);

    // Multi-threaded parallel latency benchmark across all Konami regions
    static void BenchmarkAllKonamiRegions(std::atomic<bool>& isRunningFlag, std::function<void(KonamiRegion best, float ping)> onFinish = nullptr);
};

} // namespace ebooster
