#pragma once

#include "ebooster/Common.hpp"
#include "ebooster/BoosterEngine.hpp"
#include "ebooster/ProfileManager.hpp"
#include "ebooster/SteamDetector.hpp"
#include "ebooster/NetworkOptimizer.hpp"
#include "ebooster/UserPreferences.hpp"
#include "ebooster/MatchSniffer.hpp"
#include "ebooster/InGameOverlay.hpp"
#include "ebooster/UpdateManager.hpp"

#include <windows.h>
#include <shellapi.h>
#include <wrl.h>
#include <WebView2.h>

#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>

#define WM_TRAYICON (WM_APP + 1)
#define WM_APP_STATE_CHANGED (WM_APP + 2)

namespace ebooster {

class WebViewApp {
public:
    WebViewApp();
    ~WebViewApp();

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    void Run();
    void Cleanup();
    void SetCmdLineMinimized(bool min) noexcept { m_cmdLineMinimized = min; }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // Core WebView2 setup
    bool InitWebView2();
    void OnWebViewInitialized();
    void HandleWebMessage(const std::wstring& messageJson);
    void SendJsonToWeb(const std::string& jsonString);

    // State Serialization
    std::string BuildStateJson() const;
    std::string BuildTelemetryJson();

    // Engine & Routing
    void StartEngine();
    void StopEngine();
    void ApplySelectedProfile(int idx);
    void DeleteProfile(int idx);
    void SaveProfilesToDisk();
    void CheckAndProcessAutoBoost();

    // Background Ping & Telemetry Probe
    void StartPingWorker();
    void StopPingWorker();
    void PingWorkerLoop(std::stop_token st);

    // Network Optimizer
    void ApplyNetworkOptimizer();
    void RevertNetworkOptimizer();

    // Startup & System Tray
    void InitTrayIcon();
    void RemoveTrayIcon();
    void UpdateTrayIcon();
    void MinimizeToTray();
    void RestoreFromTray();
    void ShowTrayContextMenu();
    bool IsStartWithWindowsEnabled() const;
    bool SetStartWithWindows(bool enable);

    // Hotkeys & Overlay
    void RegisterHotkeys();
    void UnregisterHotkeys();
    void ToggleInGameOverlay();
    void UpdateOverlayTelemetry();

    // User Preferences & Window Geometry
    void LoadUserPreferences();
    void SaveUserPreferences();
    void CaptureWindowGeometry();

    // Windows state
    HINSTANCE m_hInstance{nullptr};
    HWND m_hWnd{nullptr};
    NOTIFYICONDATAW m_nid{};
    bool m_trayIconActive{false};
    bool m_cmdLineMinimized{false};
    bool m_isAdmin{false};

    // Engine & Profiles
    bool m_isBoosting{false};
    std::unique_ptr<BoosterEngine> m_engine;
    EngineConfig m_config;
    std::vector<GameProfile> m_profiles;
    int m_selectedProfileIdx{0};
    UserPreferences m_prefs;

    // Telemetry & Metrics
    std::atomic<float> m_currentPing{24.0f};
    std::atomic<float> m_baselineIspPing{48.0f};
    std::atomic<float> m_currentPacketLoss{0.0f};
    std::atomic<float> m_currentJitter{1.2f};
    mutable std::mutex m_telemetryMutex;
    std::deque<float> m_pingHistory;
    std::deque<bool> m_lossProbeHistory;
    std::jthread m_pingThread;
    std::atomic<bool> m_isBenchmarking{false};

    // Update status for UI
    mutable std::mutex m_updateMutex;
    bool m_updateChecking{false};
    bool m_updateAvailable{false};
    std::string m_updateLatestVersion;
    std::string m_updateStatusMsg;

    // Subsystems
    std::unique_ptr<InGameOverlay> m_overlay;
    std::unique_ptr<UpdateManager> m_updateManager;

    // Microsoft WebView2 COM interfaces
    Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_webViewEnv;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_webViewController;
    Microsoft::WRL::ComPtr<ICoreWebView2> m_webView;
    bool m_webViewReady{false};

    static inline WebViewApp* s_pAppInstance{nullptr};
};

} // namespace ebooster
