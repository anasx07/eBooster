#pragma once

#include <string>
#include <filesystem>
#include <cstdint>

namespace ebooster {

struct UserPreferences {
    // Window geometry
    int windowX{-1};
    int windowY{-1};
    int windowWidth{1100};
    int windowHeight{680};
    bool windowMaximized{false};

    // Profiles & Nodes
    std::string selectedProfileName{"eFootball (Steam)"};
    std::string selectedServerId{"fr-mrs-live"};

    // Automation & Controls
    bool autoBoostOnLaunch{false};
    bool autoStopOnExit{true};
    bool globalHotkeyEnabled{true};
    bool closeToTray{true};
    bool minimizeToTray{true};
    bool startWithWindows{false};
    bool startMinimizedToTray{false};

    // Tunneling & Routing
    int tunnelMode{0}; // 0 = SplitTunnelGameOnly, 1 = FullSystemVpn
    bool enableKillSwitch{false};
    bool blockIpv6Leaks{true};
    bool enableSmartDns{true};
    uint32_t processPollMs{500};
    uint32_t snifferPollMs{1000};
    std::string ispBenchmarkIp{"185.60.112.157"};

    // Konami eFootball Features
    int selectedKonamiRegion{0}; // KonamiRegion enum value
    bool efootballGeofilterActive{false};
    bool efootballAutoLock{true}; // Auto-lock selected server on startup
    bool efootballAntiLagShield{false};

    // In-Game Overlay (OSD)
    bool overlayEnabled{false};
    bool overlayClickThrough{true};
    int overlayPosition{0}; // 0 = TopRight, 1 = TopLeft, 2 = TopCenter, 3 = BottomRight

    // GitHub Software Updates
    bool autoCheckUpdates{true};
    std::string githubRepo{"anasx07/eBooster"};
};

class PreferencesManager {
public:
    static UserPreferences Load(const std::filesystem::path& path = {});
    static bool Save(const std::filesystem::path& path, const UserPreferences& prefs);
};

} // namespace ebooster
