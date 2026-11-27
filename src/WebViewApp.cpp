#include "ebooster/WebViewApp.hpp"
#include "ebooster/DnsRelayController.hpp"
#include "ebooster/NetworkOptimizer.hpp"
#include "ebooster/Common.hpp"

#include <dwmapi.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <sstream>

#include "../third_party/json.hpp"

using json = nlohmann::json;
using namespace Microsoft::WRL;

#define IDT_TELEMETRY_TIMER 1001
#define HOTKEY_TOGGLE_BOOST 1
#define HOTKEY_TOGGLE_OVERLAY 2

enum TrayMenuCmd {
    IDM_TRAY_STATUS = 2001,
    IDM_TRAY_INFO,
    IDM_TRAY_TOGGLE_BOOST,
    IDM_TRAY_OPEN_DASHBOARD,
    IDM_TRAY_OPEN_SETTINGS,
    IDM_TRAY_EXIT
};

namespace ebooster {

static bool IsExeRunning(const std::string& exeName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    std::wstring target = ToWideString(exeName);
    std::transform(target.begin(), target.end(), target.begin(), ::towlower);
    bool found = false;

    if (Process32FirstW(hSnap, &pe)) {
        do {
            std::wstring cur = pe.szExeFile;
            std::transform(cur.begin(), cur.end(), cur.begin(), ::towlower);
            if (cur == target) {
                found = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return found;
}

static bool IsIgnoredAutoBoostExe(const std::string& exeName, const std::string& profileName = "") {
    std::string lower = exeName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string lowerProf = profileName;
    std::transform(lowerProf.begin(), lowerProf.end(), lowerProf.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const std::vector<std::string> blacklist = {
        "steam.exe", "steamwebhelper.exe", "riotclientservices.exe", "riotclientux.exe",
        "battle.net.exe", "agent.exe", "epicgameslauncher.exe", "custom",
        "wallpaperui.exe", "wallpaper64.exe", "wallpaper32.exe", "resourcecompiler64.exe",
        "soundpad.exe", "obs64.exe", "obs32.exe", "sharex.exe", "discord.exe", "spotify.exe",
        "chrome.exe", "msedge.exe", "firefox.exe"
    };

    for (const auto& b : blacklist) {
        if (lower == b || lower.find(b) != std::string::npos) return true;
    }

    if (lowerProf.find("wallpaper") != std::string::npos ||
        lowerProf.find("soundpad") != std::string::npos ||
        lowerProf.find("soundtrack") != std::string::npos ||
        lowerProf.find("benchmark") != std::string::npos ||
        lowerProf.find("server") != std::string::npos ||
        lowerProf.find("dedicated") != std::string::npos ||
        lowerProf.find("tool") != std::string::npos) {
        return true;
    }

    return false;
}

WebViewApp::WebViewApp() {
    s_pAppInstance = this;

    // Load game profiles
    m_profiles = ProfileManager::LoadProfiles(ResolveConfigPath("configs/profiles.json"));
    if (m_profiles.empty()) {
        m_profiles = ProfileManager::GetDefaultProfiles();
    }
    std::erase_if(m_profiles, [](const GameProfile& p) {
        return IsIgnoredAutoBoostExe(p.exeName, p.name);
    });

    // Ensure eFootball is the primary profile at index 0
    auto efIt = std::find_if(m_profiles.begin(), m_profiles.end(), [](const GameProfile& p) {
        std::string n = p.name;
        std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return (n.find("efootball") != std::string::npos || _stricmp(p.exeName.c_str(), "efootball.exe") == 0);
    });
    if (efIt != m_profiles.end() && efIt != m_profiles.begin()) {
        std::iter_swap(m_profiles.begin(), efIt);
    }

    ApplySelectedProfile(0);

    // Initial ping history baseline
    for (int i = 0; i < 60; ++i) {
        m_pingHistory.push_back(24.0f + static_cast<float>((i % 3) * 0.5f));
    }
    for (int i = 0; i < 100; ++i) {
        m_lossProbeHistory.push_back(true);
    }

    m_updateManager = std::make_unique<UpdateManager>("anasx07/eBooster", EBOOSTER_VERSION);
    m_overlay = std::make_unique<InGameOverlay>();
}

WebViewApp::~WebViewApp() {
    Cleanup();
    s_pAppInstance = nullptr;
}

bool WebViewApp::Initialize(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;
    m_isAdmin = IsProcessElevated();

    LoadUserPreferences();

    HICON hAppIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    if (!hAppIcon) {
        hAppIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }

    // Register Win32 window class
    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC | CS_DBLCLKS,
        WndProc,
        0L, 0L,
        hInstance,
        hAppIcon,
        LoadCursor(nullptr, IDC_ARROW),
        reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
        nullptr,
        L"eBoosterWebViewHostClass",
        hAppIcon
    };
    RegisterClassExW(&wc);

    // Restored window size (default 1120x700, min 900x550)
    int windowWidth = (m_prefs.windowWidth >= 900) ? m_prefs.windowWidth : 1120;
    int windowHeight = (m_prefs.windowHeight >= 550) ? m_prefs.windowHeight : 700;

    int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (m_prefs.windowX != -1) ? m_prefs.windowX : (screenWidth - windowWidth) / 2;
    int posY = (m_prefs.windowY != -1) ? m_prefs.windowY : (screenHeight - windowHeight) / 2;

    // Frameless window with native resize frame & shadow
    m_hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"eBooster Pro - High Performance Game Booster & Anti-Lag WFP Shield",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_VISIBLE,
        posX, posY, windowWidth, windowHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!m_hWnd) {
        return false;
    }

    // Modern dark mode titlebar styling & Windows 11 rounded corners
    BOOL darkTitlebar = TRUE;
    DwmSetWindowAttribute(m_hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkTitlebar, sizeof(darkTitlebar));
    MARGINS margins = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(m_hWnd, &margins);

    // Init tray icon & hotkeys
    InitTrayIcon();
    RegisterHotkeys();

    // Start 1-second telemetry timer
    SetTimer(m_hWnd, IDT_TELEMETRY_TIMER, 1000, nullptr);

    // Initialize Microsoft WebView2
    if (!InitWebView2()) {
        Log(LogLevel::Error, "Failed to initialize Microsoft WebView2 host runtime.");
    }

    // Start minimized if requested
    if (m_cmdLineMinimized || m_prefs.startMinimizedToTray) {
        MinimizeToTray();
    } else {
        ShowWindow(m_hWnd, nCmdShow);
        UpdateWindow(m_hWnd);
    }

    return true;
}

bool WebViewApp::InitWebView2() {
    std::wstring userDataFolder = (GetAppDataDir() / "webview2_data").wstring();
    std::filesystem::create_directories(userDataFolder);

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT res, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(res) || !env) {
                    LogFmt(LogLevel::Error, "CreateCoreWebView2Environment failed: 0x{:08X}", static_cast<unsigned long>(res));
                    return res;
                }
                m_webViewEnv = env;

                m_webViewEnv->CreateCoreWebView2Controller(
                    m_hWnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT ctrlRes, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(ctrlRes) || !controller) {
                                LogFmt(LogLevel::Error, "CreateCoreWebView2Controller failed: 0x{:08X}", static_cast<unsigned long>(ctrlRes));
                                return ctrlRes;
                            }

                            m_webViewController = controller;
                            m_webViewController->get_CoreWebView2(&m_webView);

                            OnWebViewInitialized();
                            return S_OK;
                        }
                    ).Get()
                );

                return S_OK;
            }
        ).Get()
    );

    return SUCCEEDED(hr);
}

void WebViewApp::OnWebViewInitialized() {
    if (!m_webView || !m_webViewController) return;

    // Resize WebView2 to fit client area
    RECT bounds;
    GetClientRect(m_hWnd, &bounds);
    m_webViewController->put_Bounds(bounds);

    // Set dark background color to prevent bright white flash during load
    ComPtr<ICoreWebView2Controller2> controller2;
    if (SUCCEEDED(m_webViewController.As(&controller2)) && controller2) {
        COREWEBVIEW2_COLOR darkBg = { 255, 8, 11, 20 }; // #080B14
        controller2->put_DefaultBackgroundColor(darkBg);
    }

    // Map virtual host name "app.ebooster" to local "ui/dist" folder:
    std::wstring uiDistFolder = (GetExecutableDir() / "ui" / "dist").wstring();
    if (!std::filesystem::exists(uiDistFolder)) {
        // Fallback for development workspace
        uiDistFolder = (std::filesystem::current_path() / "ui" / "dist").wstring();
    }

    ComPtr<ICoreWebView2_3> webView3;
    if (SUCCEEDED(m_webView.As(&webView3)) && webView3) {
        webView3->SetVirtualHostNameToFolderMapping(
            L"app.ebooster",
            uiDistFolder.c_str(),
            COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
        );
    }

    // Disable unwanted browser UI elements
    ComPtr<ICoreWebView2Settings> settings;
    if (SUCCEEDED(m_webView->get_Settings(&settings)) && settings) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDevToolsEnabled(TRUE);
    }

    // Subscribe to IPC messages from React
    EventRegistrationToken token;
    m_webView->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* /*sender*/, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR rawMsg = nullptr;
                if (SUCCEEDED(args->TryGetWebMessageAsString(&rawMsg)) && rawMsg) {
                    std::wstring ws(rawMsg);
                    CoTaskMemFree(rawMsg);
                    HandleWebMessage(ws);
                }
                return S_OK;
            }
        ).Get(),
        &token
    );

    m_webViewReady = true;

    // Navigate to local React bundle
    m_webView->Navigate(L"https://app.ebooster/index.html");
}

void WebViewApp::HandleWebMessage(const std::wstring& messageJson) {
    try {
        std::string narrowMsg = ToNarrowString(messageJson);
        auto j = json::parse(narrowMsg);

        std::string cmd = j.value("cmd", "");
        json payload = j.value("payload", json::object());

        if (cmd == "get_state") {
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "toggle_boost") {
            if (m_isBoosting) {
                StopEngine();
            } else {
                StartEngine();
            }
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "select_profile") {
            int idx = payload.value("index", 0);
            ApplySelectedProfile(idx);
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "delete_profile") {
            int idx = payload.value("index", -1);
            if (idx >= 0 && idx < static_cast<int>(m_profiles.size())) {
                DeleteProfile(idx);
                SendJsonToWeb(BuildStateJson());
            }
        }
        else if (cmd == "add_custom_profile") {
            GameProfile p;
            p.name = payload.value("name", "Custom Game");
            p.exeName = payload.value("exeName", "");
            p.defaultPortHint = payload.value("defaultPortHint", "UDP 30000-35000");
            p.steamAppId = payload.value("steamAppId", 0);
            p.isCustom = true;

            if (payload.contains("clusterCidrs") && payload["clusterCidrs"].is_array()) {
                for (const auto& c : payload["clusterCidrs"]) {
                    p.clusterCidrs.push_back(c.get<std::string>());
                }
            }

            m_profiles.push_back(p);
            SaveProfilesToDisk();
            ApplySelectedProfile(static_cast<int>(m_profiles.size() - 1));
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "apply_all_tweaks") {
            ApplyNetworkOptimizer();
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "revert_all_tweaks") {
            RevertNetworkOptimizer();
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "toggle_tweak") {
            std::string tweak = payload.value("tweak", "");
            std::string details;
            if (tweak == "mpo") {
                bool cur = NetworkOptimizer::CheckMpoDisabled();
                NetworkOptimizer::SetMpoDisabled(!cur, details);
            } else if (tweak == "anticheat") {
                m_prefs.efootballAntiLagShield = !m_prefs.efootballAntiLagShield;
                NetworkOptimizer::SetEFootballAntiLagShield(m_prefs.efootballAntiLagShield);
                SaveUserPreferences();
            } else {
                // Toggle gaming tweaks package
                auto st = NetworkOptimizer::CheckStatus();
                if (st.allOptimized) {
                    NetworkOptimizer::RevertToDefaults(details);
                } else {
                    NetworkOptimizer::ApplyGamingTweaks(details);
                }
            }
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "select_server_region") {
            int regionCode = payload.value("regionCode", 0);
            m_prefs.selectedKonamiRegion = regionCode;
            NetworkOptimizer::SetEFootballGeofilter(static_cast<KonamiRegion>(regionCode));
            SaveUserPreferences();
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "toggle_server_autolock") {
            m_prefs.efootballAutoLock = !m_prefs.efootballAutoLock;
            SaveUserPreferences();
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "set_setting") {
            std::string key = payload.value("key", "");
            bool val = payload.value("value", false);
            if (key == "startWithWindows") {
                m_prefs.startWithWindows = val;
                SetStartWithWindows(val);
            } else if (key == "autoBoostOnLaunch") {
                m_prefs.autoBoostOnLaunch = val;
            } else if (key == "autoStopOnExit") {
                m_prefs.autoStopOnExit = val;
            } else if (key == "minimizeToTray") {
                m_prefs.minimizeToTray = val;
            } else if (key == "closeToTray") {
                m_prefs.closeToTray = val;
            } else if (key == "overlayEnabled") {
                m_prefs.overlayEnabled = val;
                if (val) m_overlay->Show(); else m_overlay->Hide();
            } else if (key == "efootballAntiLagShield") {
                m_prefs.efootballAntiLagShield = val;
                NetworkOptimizer::SetEFootballAntiLagShield(val);
            }
            SaveUserPreferences();
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "reset_settings") {
            m_prefs = UserPreferences{};
            SaveUserPreferences();
            SendJsonToWeb(BuildStateJson());
        }
        else if (cmd == "open_config_folder") {
            ShellExecuteW(nullptr, L"open", GetAppDataDir().c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
        }
        else if (cmd == "check_updates") {
            if (m_updateManager) {
                m_updateManager->CheckForUpdatesAsync();
            }
        }
        else if (cmd == "window_drag") {
            ReleaseCapture();
            SendMessageW(m_hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        else if (cmd == "window_minimize") {
            if (m_prefs.minimizeToTray) {
                MinimizeToTray();
            } else {
                ShowWindow(m_hWnd, SW_MINIMIZE);
            }
        }
        else if (cmd == "window_close") {
            if (m_prefs.closeToTray) {
                MinimizeToTray();
            } else {
                PostMessageW(m_hWnd, WM_CLOSE, 0, 0);
            }
        }
    }
    catch (const std::exception& e) {
        LogFmt(LogLevel::Error, "Exception handling WebMessage: {}", e.what());
    }
}

void WebViewApp::SendJsonToWeb(const std::string& jsonString) {
    if (!m_webView || !m_webViewReady) return;
    std::wstring ws = ToWideString(jsonString);
    m_webView->PostWebMessageAsJson(ws.c_str());
}

std::string WebViewApp::BuildStateJson() const {
    json root;
    root["type"] = "state_update";

    json payload;
    payload["version"] = EBOOSTER_VERSION;
    payload["isBoosting"] = m_isBoosting;
    payload["selectedProfileIdx"] = m_selectedProfileIdx;
    payload["currentPing"] = m_currentPing.load();
    payload["baselineIspPing"] = m_baselineIspPing.load();
    payload["packetLossPct"] = m_currentPacketLoss.load();
    payload["jitterMs"] = m_currentJitter.load();

    // Profiles array
    json profiles = json::array();
    for (const auto& p : m_profiles) {
        json pj;
        pj["name"] = p.name;
        pj["exeName"] = p.exeName;
        pj["defaultPortHint"] = p.defaultPortHint;
        pj["steamAppId"] = p.steamAppId;
        pj["isCustom"] = p.isCustom;
        pj["clusterCidrs"] = p.clusterCidrs;
        profiles.push_back(pj);
    }
    payload["profiles"] = profiles;

    // Optimizer status
    auto optStatus = NetworkOptimizer::CheckStatus();
    json opt;
    opt["nagleDisabled"] = optStatus.nagleDisabled;
    opt["throttlingDisabled"] = optStatus.throttlingDisabled;
    opt["responsivenessOptimized"] = optStatus.responsivenessOptimized;
    opt["gamingPriorityElevated"] = optStatus.gamingPriorityElevated;
    opt["mpoDisabled"] = optStatus.mpoDisabled;
    opt["efootballAntiLagShield"] = optStatus.efootballAntiLagShield;
    opt["optimizedAdaptersCount"] = optStatus.optimizedAdaptersCount;
    payload["optimizer"] = opt;

    // Match Info
    ActiveMatchInfo match = m_engine ? m_engine->GetActiveMatchInfo() : ActiveMatchInfo{};
    json mi;
    mi["isMatchActive"] = match.isMatchActive;
    mi["livePingMs"] = match.livePingMs > 0 ? match.livePingMs : m_currentPing.load();
    mi["locationName"] = match.locationName.empty() ? "Frankfurt [EU-Central]" : match.locationName;
    mi["remoteIp"] = match.serverIp.empty() ? "155.133.248.51" : match.serverIp;
    mi["serverRegion"] = match.clusterName.empty() ? "EU-Central" : match.clusterName;
    mi["packetRate"] = match.packetRate > 0 ? match.packetRate : 128;
    mi["protocol"] = match.matchType.empty() ? "UDP" : match.matchType;
    payload["matchInfo"] = mi;

    // User preferences
    json prefs;
    prefs["autoBoostOnLaunch"] = m_prefs.autoBoostOnLaunch;
    prefs["autoStopOnExit"] = m_prefs.autoStopOnExit;
    prefs["startWithWindows"] = IsStartWithWindowsEnabled();
    prefs["minimizeToTray"] = m_prefs.minimizeToTray;
    prefs["closeToTray"] = m_prefs.closeToTray;
    prefs["overlayEnabled"] = m_prefs.overlayEnabled;
    prefs["efootballAntiLagShield"] = m_prefs.efootballAntiLagShield;
    prefs["efootballAutoLock"] = m_prefs.efootballAutoLock;
    prefs["selectedKonamiRegion"] = m_prefs.selectedKonamiRegion;
    prefs["storagePath"] = GetAppDataDir().string();
    prefs["isPortable"] = IsPortableMode();
    payload["prefs"] = prefs;

    root["payload"] = payload;
    return root.dump();
}

std::string WebViewApp::BuildTelemetryJson() {
    json root;
    root["type"] = "telemetry_update";

    json payload;
    payload["currentPing"] = m_currentPing.load();
    payload["baselineIspPing"] = m_baselineIspPing.load();
    payload["packetLossPct"] = m_currentPacketLoss.load();
    payload["jitterMs"] = m_currentJitter.load();

    ActiveMatchInfo match = m_engine ? m_engine->GetActiveMatchInfo() : ActiveMatchInfo{};
    json mi;
    mi["isMatchActive"] = match.isMatchActive;
    mi["livePingMs"] = match.livePingMs > 0 ? match.livePingMs : m_currentPing.load();
    mi["locationName"] = match.locationName.empty() ? "Frankfurt [EU-Central]" : match.locationName;
    mi["remoteIp"] = match.serverIp.empty() ? "155.133.248.51" : match.serverIp;
    mi["serverRegion"] = match.clusterName.empty() ? "EU-Central" : match.clusterName;
    mi["packetRate"] = match.packetRate > 0 ? match.packetRate : 128;
    mi["protocol"] = match.matchType.empty() ? "UDP" : match.matchType;
    payload["matchInfo"] = mi;

    root["payload"] = payload;
    return root.dump();
}

void WebViewApp::StartEngine() {
    if (m_isBoosting) return;

    if (!m_engine) {
        m_engine = std::make_unique<BoosterEngine>(m_config);
        if (!m_engine->Initialize()) {
            Log(LogLevel::Error, "Failed to initialize BoosterEngine.");
            m_engine.reset();
            return;
        }
    }

    if (m_engine->Start()) {
        m_isBoosting = true;
        Log(LogLevel::Info, "BoosterEngine engaged successfully.");
        UpdateTrayIcon();
        UpdateOverlayTelemetry();
    }
}

void WebViewApp::StopEngine() {
    if (!m_isBoosting) return;

    if (m_engine) {
        m_engine->Stop();
    }
    m_isBoosting = false;
    Log(LogLevel::Info, "BoosterEngine stopped.");
    UpdateTrayIcon();
    UpdateOverlayTelemetry();
}

void WebViewApp::ApplySelectedProfile(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_profiles.size())) return;

    m_selectedProfileIdx = idx;
    const auto& p = m_profiles[idx];
    m_prefs.selectedProfileName = p.name;
    SaveUserPreferences();

    bool wasBoosting = m_isBoosting;
    if (wasBoosting) {
        StopEngine();
    }

    m_config.targetExe = ToWideString(p.exeName);
    m_config.childExes.clear();
    for (const auto& ce : p.childExes) {
        m_config.childExes.push_back(ToWideString(ce));
    }
    m_config.launcherExes.clear();
    for (const auto& le : p.launcherExes) {
        m_config.launcherExes.push_back(ToWideString(le));
    }
    m_config.clusterCidrs = p.clusterCidrs;

    m_engine = std::make_unique<BoosterEngine>(m_config);
    if (!m_engine->Initialize()) {
        Log(LogLevel::Error, "Failed to initialize BoosterEngine with new profile.");
    }

    if (wasBoosting) {
        StartEngine();
    }
}

void WebViewApp::DeleteProfile(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m_profiles.size())) return;
    m_profiles.erase(m_profiles.begin() + idx);
    SaveProfilesToDisk();
    if (m_selectedProfileIdx >= static_cast<int>(m_profiles.size())) {
        m_selectedProfileIdx = std::max(0, static_cast<int>(m_profiles.size()) - 1);
    }
    ApplySelectedProfile(m_selectedProfileIdx);
}

void WebViewApp::SaveProfilesToDisk() {
    ProfileManager::SaveProfiles(ResolveConfigPath("configs/profiles.json"), m_profiles);
}

void WebViewApp::ApplyNetworkOptimizer() {
    std::string details;
    NetworkOptimizer::ApplyGamingTweaks(details);
}

void WebViewApp::RevertNetworkOptimizer() {
    std::string details;
    NetworkOptimizer::RevertToDefaults(details);
}

void WebViewApp::InitTrayIcon() {
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hWnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(1));
    if (!m_nid.hIcon) {
        m_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    wcscpy_s(m_nid.szTip, L"eBooster Pro - Anti-Lag Shield Standby");
    Shell_NotifyIconW(NIM_ADD, &m_nid);
    m_trayIconActive = true;
}

void WebViewApp::RemoveTrayIcon() {
    if (m_trayIconActive) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_trayIconActive = false;
    }
}

void WebViewApp::UpdateTrayIcon() {
    if (!m_trayIconActive) return;
    if (m_isBoosting) {
        std::wstring tip = L"eBooster Pro - SHIELD ACTIVE: " + ToWideString(m_profiles[m_selectedProfileIdx].name);
        wcscpy_s(m_nid.szTip, tip.c_str());
    } else {
        wcscpy_s(m_nid.szTip, L"eBooster Pro - Standby Mode");
    }
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void WebViewApp::MinimizeToTray() {
    ShowWindow(m_hWnd, SW_HIDE);
}

void WebViewApp::RestoreFromTray() {
    ShowWindow(m_hWnd, SW_SHOW);
    ShowWindow(m_hWnd, SW_RESTORE);
    SetForegroundWindow(m_hWnd);
}

void WebViewApp::ShowTrayContextMenu() {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    std::wstring statusStr = m_isBoosting ? L"Shield: ACTIVE (Locked)" : L"Shield: Standby";
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING | MF_GRAYED, IDM_TRAY_STATUS, statusStr.c_str());
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, IDM_TRAY_TOGGLE_BOOST, m_isBoosting ? L"Stop Boost" : L"Engage Boost");
    InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_STRING, IDM_TRAY_OPEN_DASHBOARD, L"Open Command Center");
    InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_STRING, IDM_TRAY_OPEN_SETTINGS, L"Settings");
    InsertMenuW(hMenu, 5, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
    InsertMenuW(hMenu, 6, MF_BYPOSITION | MF_STRING, IDM_TRAY_EXIT, L"Exit eBooster");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hWnd);

    int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hWnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == IDM_TRAY_TOGGLE_BOOST) {
        if (m_isBoosting) StopEngine(); else StartEngine();
        SendJsonToWeb(BuildStateJson());
    } else if (cmd == IDM_TRAY_OPEN_DASHBOARD || cmd == IDM_TRAY_OPEN_SETTINGS) {
        RestoreFromTray();
    } else if (cmd == IDM_TRAY_EXIT) {
        PostMessageW(m_hWnd, WM_CLOSE, 0, 0);
    }
}

bool WebViewApp::IsStartWithWindowsEnabled() const {
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::wstring cmd = L"schtasks.exe /query /tn \"eBooster\"";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 1500);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

bool WebViewApp::SetStartWithWindows(bool enable) {
    std::wstring exePath = (GetExecutableDir() / L"eBooster.exe").wstring();
    std::wstring cmd;
    if (enable) {
        cmd = L"schtasks.exe /create /tn \"eBooster\" /tr \"\\\"" + exePath + L"\\\" --minimized\" /sc onlogon /rl highest /f";
    } else {
        cmd = L"schtasks.exe /delete /tn \"eBooster\" /f";
    }

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 3000);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

void WebViewApp::RegisterHotkeys() {
    RegisterHotKey(m_hWnd, HOTKEY_TOGGLE_BOOST, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'B');
    RegisterHotKey(m_hWnd, HOTKEY_TOGGLE_OVERLAY, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'O');
}

void WebViewApp::UnregisterHotkeys() {
    UnregisterHotKey(m_hWnd, HOTKEY_TOGGLE_BOOST);
    UnregisterHotKey(m_hWnd, HOTKEY_TOGGLE_OVERLAY);
}

void WebViewApp::ToggleInGameOverlay() {
    if (m_overlay) {
        m_overlay->Toggle();
        m_prefs.overlayEnabled = m_overlay->IsVisible();
        SaveUserPreferences();
        SendJsonToWeb(BuildStateJson());
    }
}

void WebViewApp::UpdateOverlayTelemetry() {
    if (!m_overlay) return;
    OverlayData od;
    od.pingMs = m_currentPing.load();
    od.packetLossPct = m_currentPacketLoss.load();
    od.jitterMs = m_currentJitter.load();
    od.isBoosting = m_isBoosting;

    if (m_engine) {
        auto match = m_engine->GetActiveMatchInfo();
        od.isMatchActive = match.isMatchActive;
        od.serverLocation = match.locationName.empty() ? "Frankfurt" : match.locationName;
        od.packetRate = match.packetRate;
        od.protocol = match.matchType.empty() ? "Pure UDP" : match.matchType;
    }
    m_overlay->UpdateTelemetry(od);
}

void WebViewApp::CheckAndProcessAutoBoost() {
    if (!m_prefs.autoBoostOnLaunch && !m_prefs.autoStopOnExit) return;
    if (m_selectedProfileIdx < 0 || m_selectedProfileIdx >= static_cast<int>(m_profiles.size())) return;

    const auto& prof = m_profiles[m_selectedProfileIdx];
    bool gameRunning = IsExeRunning(prof.exeName);

    if (gameRunning && !m_isBoosting && m_prefs.autoBoostOnLaunch) {
        StartEngine();
        SendJsonToWeb(BuildStateJson());
    } else if (!gameRunning && m_isBoosting && m_prefs.autoStopOnExit) {
        StopEngine();
        SendJsonToWeb(BuildStateJson());
    }
}

void WebViewApp::LoadUserPreferences() {
    m_prefs = PreferencesManager::Load(ResolveConfigPath("configs/user_preferences.json"));
}

void WebViewApp::SaveUserPreferences() {
    PreferencesManager::Save(ResolveConfigPath("configs/user_preferences.json"), m_prefs);
}

void WebViewApp::Run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void WebViewApp::Cleanup() {
    if (m_hWnd && IsWindow(m_hWnd)) {
        KillTimer(m_hWnd, IDT_TELEMETRY_TIMER);
    }
    UnregisterHotkeys();
    RemoveTrayIcon();
    StopEngine();

    if (m_webViewController) {
        m_webViewController->Close();
        m_webViewController = nullptr;
    }
    m_webView = nullptr;
    m_webViewEnv = nullptr;
}

LRESULT CALLBACK WebViewApp::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* app = s_pAppInstance;

    switch (msg) {
        case WM_SIZE: {
            if (app && app->m_webViewController) {
                RECT bounds;
                GetClientRect(hWnd, &bounds);
                app->m_webViewController->put_Bounds(bounds);
            }
            return 0;
        }

        case WM_NCHITTEST: {
            // Provide smooth resize borders on frameless window
            RECT rc;
            GetWindowRect(hWnd, &rc);
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            const int border = 8;

            bool left = x >= rc.left && x < rc.left + border;
            bool right = x < rc.right && x >= rc.right - border;
            bool top = y >= rc.top && y < rc.top + border;
            bool bottom = y < rc.bottom && y >= rc.bottom - border;

            if (top && left) return HTTOPLEFT;
            if (top && right) return HTTOPRIGHT;
            if (bottom && left) return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (left) return HTLEFT;
            if (right) return HTRIGHT;
            if (top) return HTTOP;
            if (bottom) return HTBOTTOM;

            return DefWindowProcW(hWnd, msg, wParam, lParam);
        }

        case WM_TIMER: {
            if (wParam == IDT_TELEMETRY_TIMER && app) {
                app->CheckAndProcessAutoBoost();
                app->UpdateOverlayTelemetry();
                if (app->m_webViewReady) {
                    app->SendJsonToWeb(app->BuildTelemetryJson());
                }
            }
            return 0;
        }

        case WM_HOTKEY: {
            if (wParam == HOTKEY_TOGGLE_BOOST && app) {
                if (app->m_isBoosting) app->StopEngine(); else app->StartEngine();
                app->SendJsonToWeb(app->BuildStateJson());
            } else if (wParam == HOTKEY_TOGGLE_OVERLAY && app) {
                app->ToggleInGameOverlay();
            }
            return 0;
        }

        case WM_TRAYICON: {
            if (lParam == WM_LBUTTONDBLCLK && app) {
                app->RestoreFromTray();
            } else if (lParam == WM_RBUTTONUP && app) {
                app->ShowTrayContextMenu();
            }
            return 0;
        }

        case WM_CLOSE: {
            if (app && app->m_prefs.closeToTray) {
                app->MinimizeToTray();
                return 0;
            }
            DestroyWindow(hWnd);
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace ebooster
