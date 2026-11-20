#pragma once

#include "ebooster/Common.hpp"
#include "ebooster/BoosterEngine.hpp"
#include "ebooster/ProfileManager.hpp"
#include "ebooster/SteamDetector.hpp"
#include "ebooster/TextureLoader.hpp"
#include "ebooster/NetworkOptimizer.hpp"
#include "ebooster/UserPreferences.hpp"
#include "ebooster/MatchSniffer.hpp"
#include "ebooster/InGameOverlay.hpp"
#include "ebooster/UpdateManager.hpp"

#include <d3d11.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <memory>
#include <cmath>
#include <mutex>
#include <shared_mutex>

#define WM_TRAYICON (WM_APP + 1)

namespace ebooster {

enum class GuiPage {
    Dashboard,
    Routes,
    Logs,
    Settings
};

class GuiApp {
public:
    // Modal state for Active Windows Picker (OBS Style)
    struct ActiveWindowItem {
        HWND hWnd{nullptr};
        DWORD pid{0};
        std::string exeName;
        std::string windowTitle;
    };

    GuiApp();
    ~GuiApp();

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    void Run();
    void Cleanup();
    void SetCmdLineMinimized(bool min) noexcept { m_cmdLineMinimized = min; }

private:
    // Win32 & DirectX 11 setup
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();

    // UI Styling & Layout
    void SetupImGuiStyle();
    void RenderMainInterface();

    // Pages (all static, no draggable windows)
    void RenderTopBar();
    void RenderSidebar();
    void RenderPageDashboard();
    void RenderPageRoutes();
    void RenderPageLogs();
    void RenderPageSettings();
    void RenderAddGameModal();
    void RenderWindowPickerModal();
    void RenderUpdateModal();
    void RefreshActiveWindowsList();

    // Engine & Profile & Server control
    void StartEngine();
    void StopEngine();
    void UpdatePingHistory();
    void ApplySelectedProfile(int idx);
    void DeleteProfile(int idx);
    void SaveProfilesToDisk();

    // Windows Gaming Latency & TCP/UDP Optimizer
    void RefreshOptimizerStatus();
    void ApplyNetworkOptimizer();
    void RevertNetworkOptimizer();

    // Global In-Game Hotkey (Ctrl + Shift + B)
    void RegisterBoostHotkey();
    void UnregisterBoostHotkey();

    // Mini In-Game Overlay (OSD) Hotkey (Ctrl + Shift + O)
    void RegisterOverlayHotkey();
    void UnregisterOverlayHotkey();
    void ToggleInGameOverlay();
    void UpdateOverlayTelemetry();

    // Match Session Report
    void CopyMatchReportToClipboard();

    // Steam Auto-Detection & Game Artwork
    void ScanSteamGames(bool showNotification = false);
    void UpdateActiveGameArtwork();

    // System Tray Management (Shell_NotifyIcon)
    void InitTrayIcon();
    void RemoveTrayIcon();
    void UpdateTrayIcon();
    void MinimizeToTray();
    void RestoreFromTray();
    void ShowTrayContextMenu();
    bool IsStartWithWindowsEnabled() const;
    bool SetStartWithWindows(bool enable);

    // Zero-Click Auto-Boost & Notifications
    void CheckAndProcessAutoBoost();
    void ShowToastNotification(const std::wstring& title, const std::wstring& message, DWORD infoFlags = NIIF_INFO);

    // Game Match Session Stats HUD
    void RenderMatchSessionHud(float availWidth, float cardHeight = 0.0f);

    // High-DPI Scaling Engine
    float m_dpiScale{1.0f};
    float S(float px) const noexcept { return std::round(px * m_dpiScale); }
    float GetDpiScale() const noexcept { return m_dpiScale; }
    void UpdateDpiScale();

    // State
    HWND m_hWnd{nullptr};
    ID3D11Device* m_pd3dDevice{nullptr};
    ID3D11DeviceContext* m_pd3dDeviceContext{nullptr};
    IDXGISwapChain* m_pSwapChain{nullptr};
    ID3D11RenderTargetView* m_mainRenderTargetView{nullptr};
    bool m_swapChainOccluded{false};

    GuiPage m_currentPage{GuiPage::Dashboard};
    bool m_isBoosting{false};
    bool m_isAdmin{false};

    EngineConfig m_config;
    std::unique_ptr<BoosterEngine> m_engine;

    std::vector<GameProfile> m_profiles;
    int m_selectedProfileIdx{0};
    char m_customExeBuf[128] = "cs2.exe";
    char m_manualIpBuf[64] = "";

    // Modal state for "+ Add Custom Game"
    bool m_showAddGameModal{false};
    char m_modalNameBuf[128] = "";
    char m_modalExeBuf[128] = "";
    char m_modalChildBuf[256] = "";
    char m_modalLauncherBuf[128] = "";
    char m_modalPortsBuf[64] = "27015";
    char m_modalCidrsBuf[256] = "";
    char m_modalSteamIdBuf[32] = "";
    std::string m_modalErrorMsg;

    // Modal state for Active Windows Picker (OBS Style)
    bool m_showWindowPickerModal{false};
    int m_windowPickerTabMode{0}; // 0 = Specific Windows/Tabs, 1 = Entire Applications
    char m_windowPickerFilter[128] = "";
    std::vector<ActiveWindowItem> m_activeWindows;

    // Steam & Game Artwork Textures
    std::unique_ptr<TextureLoader> m_textureLoader;
    ID3D11ShaderResourceView* m_currentBannerSrv{nullptr};
    ID3D11ShaderResourceView* m_currentLogoSrv{nullptr};
    int m_bannerWidth{0};
    int m_bannerHeight{0};
    int m_logoWidth{0};
    int m_logoHeight{0};
    bool m_isScanningSteam{false};

    // Inline port editing state
    char m_editPortBuf[64] = "";

    // Latency comparison & metrics (eBooster Relay vs Local Physical ISP)
    std::deque<float> m_pingHistory;
    std::deque<float> m_ispPingHistory;
    float m_currentPing{28.0f};
    float m_currentIspPing{62.0f};
    float m_pingReduction{34.0f};
    std::chrono::steady_clock::time_point m_lastPingCheck;
    std::atomic<float> m_asyncPing{28.0f};
    std::atomic<float> m_asyncIspPing{62.0f};
    std::jthread m_pingWorkerThread;
    char m_ispBenchmarkBuf[64] = "185.60.112.157";

    // Real-Time Telemetry: Jitter (Standard Deviation σ) & Dynamic Packet Loss
    std::deque<bool> m_lossProbeHistory;
    std::mutex m_telemetryMutex;
    std::atomic<float> m_currentPacketLoss{0.0f};
    std::atomic<float> m_currentJitter{0.4f};
    uint64_t m_sessionBaseDiscards{0};
    float CalculateCurrentJitter() const;
    void RecordProbeResult(bool success);

    // Game Match Session Stats HUD
    bool m_sessionActive{false};
    std::chrono::steady_clock::time_point m_sessionStartTime{};
    std::chrono::seconds m_lastSessionDuration{0};
    uint64_t m_sessionPackets{0};
    uint64_t m_sessionBytes{0};
    uint64_t m_sessionBasePackets{0};
    uint64_t m_sessionBaseBytes{0};
    uint64_t m_lastSessionPackets{0};
    uint64_t m_lastSessionBytes{0};
    float m_sessionMaxSpikeSaved{0.0f};
    float m_lastSessionMaxSpikeSaved{0.0f};
    float m_sessionPeakIspPing{0.0f};
    float m_sessionMinBoosterPing{999.0f};

    // Live Opponent & Server Match Sniffer Telemetry
    ActiveMatchInfo m_currentMatchInfo{};
    ActiveMatchInfo m_lastMatchInfo{};

    // Live real-time log stream for in-app console
    std::mutex m_logMutex;
    std::deque<LogMessage> m_uiLogBuffer;
    uint64_t m_logSinkId{0};
    char m_logSearchFilter[128]{""};
    int m_logLevelFilter{0}; // 0 = ALL, 1 = INFO, 2 = OK/SUCCESS, 3 = NET, 4 = WARN, 5 = FAIL/ERROR
    bool m_logAutoScroll{true};
    std::string m_logsCopiedFeedback;
    std::chrono::steady_clock::time_point m_logsCopiedTime{};

    // System Tray state & configuration
    NOTIFYICONDATAW m_nid{};
    bool m_trayIconCreated{false};
    bool m_minimizeToTray{true};
    bool m_closeToTray{true};
    bool m_startWithWindows{false};
    bool m_startMinimizedToTray{false};
    bool m_cmdLineMinimized{false};
    bool m_hasShownTrayNotification{false};
    bool m_forceQuit{false};



    // Zero-Click Auto-Boost on Game Launch
    bool m_autoBoostOnLaunch{false};
    bool m_autoStopOnExit{true};
    bool m_autoBoostStarted{false};
    std::string m_autoBoostActiveExe;
    std::string m_autoStoppedGameName;
    std::atomic<bool> m_requestAutoStart{false};
    std::atomic<bool> m_requestAutoStop{false};
    std::atomic<int> m_autoTargetProfileIdx{-1};
    std::jthread m_autoBoostThread;
    std::mutex m_profileMutex;

    // Active Routes filtering
    char m_routeSearchFilter[64] = "";

    // Windows Gaming Optimizer state
    OptimizerStatus m_optimizerStatus{};
    std::string m_optimizerMsg;

    // Konami Dedicated Server Geofilter state
    KonamiRegion m_selectedKonamiRegion{KonamiRegion::Auto_All};
    bool m_efootballAutoLock{true};
    std::string m_konamiGeofilterMsg;
    std::atomic<bool> m_isBenchmarkingKonami{false};
    KonamiRegion m_bestKonamiRegion{KonamiRegion::Auto_All};
    float m_bestKonamiPing{-1.0f};
    std::string m_bestKonamiSummary;
    void BenchmarkKonamiServers();

    // Global Hotkey (Ctrl + Shift + B)
    bool m_globalHotkeyEnabled{true};
    static constexpr int HOTKEY_ID_BOOST = 1001;

    // Mini Floating In-Game Overlay (OSD)
    std::unique_ptr<InGameOverlay> m_overlay;
    bool m_overlayEnabled{false};
    bool m_overlayClickThrough{true};
    int m_overlayPosition{0};
    static constexpr int HOTKEY_ID_OVERLAY = 1002;

    // Software Updates via GitHub Releases
    std::unique_ptr<UpdateManager> m_updateManager;
    char m_githubRepoBuf[128]{"anas-r/eBooster"};
    bool m_showUpdateModal{false};
    bool m_hasCheckedUpdateOnLaunch{false};

    // User Preferences Persistence
    UserPreferences m_prefs{};
    bool m_isInitialized{false};
    void LoadUserPreferences();
    void SaveUserPreferences();

    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static inline GuiApp* s_pAppInstance{nullptr};
};

} // namespace ebooster
