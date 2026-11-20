#include "ebooster/NetworkOptimizer.hpp"
#include "ebooster/Common.hpp"
#include "ebooster/SteamDetector.hpp"
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <format>
#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <atomic>
#include <thread>
#include <tlhelp32.h>

namespace ebooster {

static const wchar_t* SYSTEM_PROFILE_KEY   = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile";
static const wchar_t* GAMES_TASK_KEY       = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games";
static const wchar_t* TCPIP_INTERFACES_KEY = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces";
static const wchar_t* DWM_KEY              = L"SOFTWARE\\Microsoft\\Windows\\Dwm";

namespace {

bool RunSilentNetsh(const std::wstring& args) {
    wchar_t sysDir[MAX_PATH];
    if (GetSystemDirectoryW(sysDir, MAX_PATH) == 0) {
        return false;
    }
    std::wstring netshExe = std::wstring(sysDir) + L"\\netsh.exe";
    std::wstring fullCmd = L"\"" + netshExe + L"\" advfirewall firewall " + args;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    
    std::vector<wchar_t> cmdBuf(fullCmd.begin(), fullCmd.end());
    cmdBuf.push_back(L'\0');

    if (!CreateProcessW(netshExe.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }

    WaitForSingleObject(pi.hProcess, 4000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (exitCode == 0);
}

} // anonymous namespace

bool NetworkOptimizer::CheckMpoDisabled() {
    HKEY hDwm = nullptr;
    bool disabled = false;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, DWM_KEY, 0, KEY_READ, &hDwm) == ERROR_SUCCESS) {
        DWORD val = 0;
        DWORD size = sizeof(DWORD);
        if (RegQueryValueExW(hDwm, L"OverlayTestMode", nullptr, nullptr, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS) {
            disabled = (val == 5);
        }
        RegCloseKey(hDwm);
    }
    return disabled;
}

bool NetworkOptimizer::SetMpoDisabled(bool disable, std::string& outDetails) {
    HKEY hDwm = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, DWM_KEY, 0, KEY_READ | KEY_WRITE, &hDwm) == ERROR_SUCCESS) {
        if (disable) {
            DWORD val = 5;
            RegSetValueExW(hDwm, L"OverlayTestMode", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(DWORD));
            outDetails = "DWM Multiplane Overlay (MPO) DISABLED (OverlayTestMode=5). Frame-pacing stutter eliminated.";
        } else {
            RegDeleteValueW(hDwm, L"OverlayTestMode");
            outDetails = "DWM Multiplane Overlay (MPO) restored to Windows default.";
        }
        RegCloseKey(hDwm);
        Log(LogLevel::Info, outDetails);
        return true;
    }
    return false;
}

bool NetworkOptimizer::CheckEFootballAntiLagShield() {
    return RunSilentNetsh(L"show rule name=\"eBooster_eFootball_AntiLag\"");
}

std::wstring NetworkOptimizer::FindEFootballPath() {
    // 1. Check if running process exists
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                std::wstring name = pe.szExeFile;
                std::transform(name.begin(), name.end(), name.begin(), ::towlower);
                if (name == L"efootball.exe" || name == L"efootballonline.exe") {
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                    if (hProc) {
                        wchar_t fullPath[MAX_PATH] = {0};
                        DWORD sz = MAX_PATH;
                        if (QueryFullProcessImageNameW(hProc, 0, fullPath, &sz)) {
                            CloseHandle(hProc);
                            CloseHandle(hSnap);
                            return std::wstring(fullPath);
                        }
                        CloseHandle(hProc);
                    }
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    // 2. Check well-known default installation paths
    const std::vector<std::wstring> known = {
        L"D:\\SteamLibrary\\steamapps\\common\\eFootball\\eFootball\\Binaries\\Win64\\eFootball.exe",
        L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\eFootball\\eFootball\\Binaries\\Win64\\eFootball.exe",
        L"C:\\SteamLibrary\\steamapps\\common\\eFootball\\eFootball\\Binaries\\Win64\\eFootball.exe",
        L"E:\\SteamLibrary\\steamapps\\common\\eFootball\\eFootball\\Binaries\\Win64\\eFootball.exe"
    };
    for (const auto& p : known) {
        if (std::filesystem::exists(p)) {
            return p;
        }
    }

    // 3. Scan Steam libraries
    auto steamPath = SteamDetector::GetSteamInstallPath();
    if (!steamPath.empty()) {
        auto libraries = SteamDetector::GetLibraryFolders(steamPath);
        for (const auto& lib : libraries) {
            auto candidate = lib / "steamapps" / "common" / "eFootball" / "eFootball" / "Binaries" / "Win64" / "eFootball.exe";
            if (std::filesystem::exists(candidate)) {
                return candidate.wstring();
            }
        }
    }
    return L"";
}

bool NetworkOptimizer::SetEFootballAntiLagShield(bool enable, const std::wstring& customExePath, std::string* outDetails) {
    // Delete any prior rule first to avoid duplicate rule stacking
    RunSilentNetsh(L"delete rule name=\"eBooster_eFootball_AntiLag\"");

    if (!enable) {
        if (outDetails) *outDetails = "eFootball Anti-TCP Lag Shield DEACTIVATED. Outbound firewall rules swept.";
        Log(LogLevel::Info, "eFootball Anti-TCP Lag Shield DEACTIVATED.");
        return true;
    }

    std::wstring targetPath = customExePath.empty() ? FindEFootballPath() : customExePath;
    std::wstring addCmd;
    if (!targetPath.empty()) {
        addCmd = std::format(L"add rule name=\"eBooster_eFootball_AntiLag\" dir=out action=block program=\"{}\" protocol=TCP remoteport=30000-35000,5736 enable=yes", targetPath);
    } else {
        addCmd = L"add rule name=\"eBooster_eFootball_AntiLag\" dir=out action=block program=\"eFootball.exe\" protocol=TCP remoteport=30000-35000,5736 enable=yes";
    }

    bool ok = RunSilentNetsh(addCmd);
    if (ok) {
        std::string msg = std::format("⚡ eFootball Anti-TCP Lag Shield ACTIVE: Outbound TCP 30000-35000 & 5736 blocked (Target: {}). Low-latency UDP enforced.",
            targetPath.empty() ? "eFootball.exe" : ToNarrowString(targetPath));
        if (outDetails) *outDetails = msg;
        Log(LogLevel::Success, msg);
        return true;
    } else {
        std::string msg = "Failed to add eFootball Anti-Lag Shield firewall rule (Check Administrator rights).";
        if (outDetails) *outDetails = msg;
        Log(LogLevel::Error, msg);
        return false;
    }
}

OptimizerStatus NetworkOptimizer::CheckStatus() {
    OptimizerStatus status{};

    // 1. Check Multimedia SystemProfile
    HKEY hProfile = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, SYSTEM_PROFILE_KEY, 0, KEY_READ, &hProfile) == ERROR_SUCCESS) {
        DWORD throttling = 0;
        DWORD size = sizeof(DWORD);
        if (RegQueryValueExW(hProfile, L"NetworkThrottlingIndex", nullptr, nullptr, reinterpret_cast<LPBYTE>(&throttling), &size) == ERROR_SUCCESS) {
            status.throttlingDisabled = (throttling == 0xFFFFFFFF);
        }

        DWORD responsiveness = 20;
        size = sizeof(DWORD);
        if (RegQueryValueExW(hProfile, L"SystemResponsiveness", nullptr, nullptr, reinterpret_cast<LPBYTE>(&responsiveness), &size) == ERROR_SUCCESS) {
            status.responsivenessOptimized = (responsiveness == 0);
        }

        RegCloseKey(hProfile);
    }

    // 2. Check Tasks\Games
    HKEY hGames = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, GAMES_TASK_KEY, 0, KEY_READ, &hGames) == ERROR_SUCCESS) {
        DWORD priority = 0;
        DWORD size = sizeof(DWORD);
        if (RegQueryValueExW(hGames, L"Priority", nullptr, nullptr, reinterpret_cast<LPBYTE>(&priority), &size) == ERROR_SUCCESS) {
            status.gamingPriorityElevated = (priority == 6);
        }
        RegCloseKey(hGames);
    }

    // 3. Check Network Interfaces for Nagle's algorithm (TcpAckFrequency & TCPNoDelay)
    HKEY hInterfaces = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, TCPIP_INTERFACES_KEY, 0, KEY_READ, &hInterfaces) == ERROR_SUCCESS) {
        DWORD subkeyCount = 0;
        if (RegQueryInfoKeyW(hInterfaces, nullptr, nullptr, nullptr, &subkeyCount, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            int matching = 0;
            wchar_t subkeyName[256];

            for (DWORD i = 0; i < subkeyCount; ++i) {
                DWORD nameLen = 256;
                if (RegEnumKeyExW(hInterfaces, i, subkeyName, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    HKEY hAdapter = nullptr;
                    if (RegOpenKeyExW(hInterfaces, subkeyName, 0, KEY_READ, &hAdapter) == ERROR_SUCCESS) {
                        DWORD ackFreq = 0;
                        DWORD noDelay = 0;
                        DWORD sz = sizeof(DWORD);
                        bool hasAck = (RegQueryValueExW(hAdapter, L"TcpAckFrequency", nullptr, nullptr, reinterpret_cast<LPBYTE>(&ackFreq), &sz) == ERROR_SUCCESS && ackFreq == 1);
                        sz = sizeof(DWORD);
                        bool hasNoDelay = (RegQueryValueExW(hAdapter, L"TCPNoDelay", nullptr, nullptr, reinterpret_cast<LPBYTE>(&noDelay), &sz) == ERROR_SUCCESS && noDelay == 1);

                        if (hasAck && hasNoDelay) {
                            matching++;
                        }
                        RegCloseKey(hAdapter);
                    }
                }
            }
            status.optimizedAdaptersCount = matching;
            status.nagleDisabled = (matching > 0);
        }
        RegCloseKey(hInterfaces);
    }

    // 4. Check MPO and eFootball Shield & Geofilter
    status.mpoDisabled = CheckMpoDisabled();
    status.efootballAntiLagShield = CheckEFootballAntiLagShield();
    status.efootballGeofilterActive = CheckEFootballGeofilterActive();
    status.activeKonamiRegion = status.efootballGeofilterActive ? GetCurrentEFootballGeofilter() : KonamiRegion::Auto_All;

    status.allOptimized = status.throttlingDisabled &&
                          status.responsivenessOptimized &&
                          status.gamingPriorityElevated &&
                          status.nagleDisabled &&
                          status.mpoDisabled;

    return status;
}

bool NetworkOptimizer::ApplyGamingTweaks(std::string& outDetails) {
    int changesMade = 0;

    // 1. Configure SystemProfile
    HKEY hProfile = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, SYSTEM_PROFILE_KEY, 0, KEY_WRITE, &hProfile) == ERROR_SUCCESS) {
        DWORD valThrottling = 0xFFFFFFFF;
        DWORD valResp = 0;
        RegSetValueExW(hProfile, L"NetworkThrottlingIndex", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&valThrottling), sizeof(DWORD));
        RegSetValueExW(hProfile, L"SystemResponsiveness", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&valResp), sizeof(DWORD));
        RegCloseKey(hProfile);
        changesMade++;
    }

    // 2. Configure Tasks\Games
    HKEY hGames = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, GAMES_TASK_KEY, 0, KEY_WRITE, &hGames) == ERROR_SUCCESS) {
        DWORD gpuPriority = 8;
        DWORD priority = 6;
        const wchar_t* schedCat = L"High";
        const wchar_t* sfio = L"High";

        RegSetValueExW(hGames, L"GPU Priority", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&gpuPriority), sizeof(DWORD));
        RegSetValueExW(hGames, L"Priority", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&priority), sizeof(DWORD));
        RegSetValueExW(hGames, L"Scheduling Category", 0, REG_SZ, reinterpret_cast<const BYTE*>(schedCat), static_cast<DWORD>((wcslen(schedCat) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hGames, L"SFIO Priority", 0, REG_SZ, reinterpret_cast<const BYTE*>(sfio), static_cast<DWORD>((wcslen(sfio) + 1) * sizeof(wchar_t)));
        RegCloseKey(hGames);
        changesMade++;
    }

    // 3. Disable Nagle's Algorithm on all active IP interfaces
    int adaptersConfigured = 0;
    HKEY hInterfaces = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, TCPIP_INTERFACES_KEY, 0, KEY_READ, &hInterfaces) == ERROR_SUCCESS) {
        DWORD subkeyCount = 0;
        if (RegQueryInfoKeyW(hInterfaces, nullptr, nullptr, nullptr, &subkeyCount, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            wchar_t subkeyName[256];

            for (DWORD i = 0; i < subkeyCount; ++i) {
                DWORD nameLen = 256;
                if (RegEnumKeyExW(hInterfaces, i, subkeyName, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    HKEY hAdapter = nullptr;
                    if (RegOpenKeyExW(hInterfaces, subkeyName, 0, KEY_READ | KEY_WRITE, &hAdapter) == ERROR_SUCCESS) {
                        DWORD valOne = 1;
                        RegSetValueExW(hAdapter, L"TcpAckFrequency", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&valOne), sizeof(DWORD));
                        RegSetValueExW(hAdapter, L"TCPNoDelay", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&valOne), sizeof(DWORD));
                        adaptersConfigured++;
                        RegCloseKey(hAdapter);
                    }
                }
            }
        }
        RegCloseKey(hInterfaces);
    }

    // 4. Disable MPO to eliminate micro-stutter
    std::string mpoDetails;
    SetMpoDisabled(true, mpoDetails);

    outDetails = std::format("Applied gaming tweaks: NetworkThrottlingIndex=0xFFFFFFFF, SystemResponsiveness=0, GamePriority=6, Nagle disabled on {} network adapter(s), MPO disabled.", adaptersConfigured);
    Log(LogLevel::Success, outDetails);
    return (changesMade > 0 || adaptersConfigured > 0);
}

bool NetworkOptimizer::RevertToDefaults(std::string& outDetails) {
    // 1. Revert SystemProfile
    HKEY hProfile = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, SYSTEM_PROFILE_KEY, 0, KEY_WRITE, &hProfile) == ERROR_SUCCESS) {
        DWORD valThrottling = 10;
        DWORD valResp = 20;
        RegSetValueExW(hProfile, L"NetworkThrottlingIndex", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&valThrottling), sizeof(DWORD));
        RegSetValueExW(hProfile, L"SystemResponsiveness", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&valResp), sizeof(DWORD));
        RegCloseKey(hProfile);
    }

    // 2. Revert Tasks\Games
    HKEY hGames = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, GAMES_TASK_KEY, 0, KEY_WRITE, &hGames) == ERROR_SUCCESS) {
        DWORD gpuPriority = 8;
        DWORD priority = 2;
        const wchar_t* schedCat = L"Medium";
        const wchar_t* sfio = L"Normal";

        RegSetValueExW(hGames, L"GPU Priority", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&gpuPriority), sizeof(DWORD));
        RegSetValueExW(hGames, L"Priority", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&priority), sizeof(DWORD));
        RegSetValueExW(hGames, L"Scheduling Category", 0, REG_SZ, reinterpret_cast<const BYTE*>(schedCat), static_cast<DWORD>((wcslen(schedCat) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hGames, L"SFIO Priority", 0, REG_SZ, reinterpret_cast<const BYTE*>(sfio), static_cast<DWORD>((wcslen(sfio) + 1) * sizeof(wchar_t)));
        RegCloseKey(hGames);
    }

    // 3. Remove custom Nagle keys from interfaces
    int adaptersReverted = 0;
    HKEY hInterfaces = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, TCPIP_INTERFACES_KEY, 0, KEY_READ, &hInterfaces) == ERROR_SUCCESS) {
        DWORD subkeyCount = 0;
        if (RegQueryInfoKeyW(hInterfaces, nullptr, nullptr, nullptr, &subkeyCount, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            wchar_t subkeyName[256];

            for (DWORD i = 0; i < subkeyCount; ++i) {
                DWORD nameLen = 256;
                if (RegEnumKeyExW(hInterfaces, i, subkeyName, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
                    HKEY hAdapter = nullptr;
                    if (RegOpenKeyExW(hInterfaces, subkeyName, 0, KEY_WRITE, &hAdapter) == ERROR_SUCCESS) {
                        RegDeleteValueW(hAdapter, L"TcpAckFrequency");
                        RegDeleteValueW(hAdapter, L"TCPNoDelay");
                        adaptersReverted++;
                        RegCloseKey(hAdapter);
                    }
                }
            }
        }
        RegCloseKey(hInterfaces);
    }

    // 4. Restore MPO
    std::string mpoDetails;
    SetMpoDisabled(false, mpoDetails);

    outDetails = std::format("Reverted to Windows defaults: NetworkThrottlingIndex=10, SystemResponsiveness=20, GamePriority=2, Nagle restored on {} adapter(s), MPO enabled.", adaptersReverted);
    Log(LogLevel::Info, outDetails);
    return true;
}

// =============================================================================
// Konami eFootball Dedicated Server Geofilter (Region Lock)
// =============================================================================

namespace {

static std::atomic<KonamiRegion> s_currentGeofilter{KonamiRegion::Auto_All};

static const std::vector<std::string> s_allKonamiGameServers = {
    // Spain (Madrid)
    "34.175.112.88", "51.95.38.54", "51.48.223.143", "15.216.87.136",
    // France (Paris)
    "34.155.199.109", "15.236.165.57", "15.188.178.40", "15.188.70.90",
    // Netherlands (Amsterdam)
    "34.13.163.26", "34.38.154.179", "34.147.16.82", "35.204.72.11",
    // Germany (Frankfurt, Berlin, Hamburg)
    "34.159.137.177", "34.32.98.33", "15.220.152.64",
    // United Kingdom (London)
    "34.39.17.131", "35.242.180.12",
    // Italy (Milan, Torino)
    "34.154.179.38", "34.17.93.174", "18.102.9.53", "15.161.73.105", "16.22.9.71",
    // Switzerland (Zurich)
    "34.65.93.216", "51.34.16.129", "51.96.162.11", "51.96.111.15",
    // Poland (Warsaw)
    "34.118.91.25", "34.116.180.11", "15.220.65.50",
    // Finland (Helsinki)
    "35.228.63.230", "34.88.120.45", "16.16.92.10",
    // Greece (Athens)
    "75.45.132.226", "75.45.132.164", "75.45.131.224", "75.45.132.53", "75.45.132.133",
    // Turkey (Istanbul)
    "83.119.132.139", "83.119.135.24", "83.119.132.191", "83.119.132.5", "83.119.135.234",
    // UAE & Gulf (Dubai)
    "51.112.179.174", "34.18.108.143", "13.152.0.18",
    // Saudi Arabia (Riyadh)
    "34.166.38.226", "15.206.180.90", "51.107.12.44",
    // Qatar (Doha)
    "34.158.40.11", "51.112.243.44",
    // Nigeria (Lagos)
    "96.0.47.74", "96.0.47.5", "96.0.47.198", "96.0.47.108", "96.0.41.221", "96.0.44.155", "96.0.46.52", "96.0.47.71",
    // South Africa (Johannesburg)
    "34.35.84.61", "13.244.80.12",
    // Brazil (São Paulo)
    "34.95.169.15", "35.198.22.80", "15.228.40.11", "96.0.16.253", "96.0.16.157", "96.0.16.27",
    // Chile (Santiago)
    "34.176.17.72", "96.0.16.188", "96.0.16.199", "96.0.16.109", "96.0.20.177", "96.0.22.98",
    // US East (Virginia / NY)
    "34.85.172.113", "35.203.65.250", "34.138.103.216", "34.86.110.15",
    // US Central (Iowa / Chicago)
    "34.162.74.80", "35.224.16.17", "34.174.25.145", "34.66.190.22",
    // US West (Oregon / California)
    "34.82.40.13", "34.106.69.65", "34.125.24.248", "34.102.12.90",
    // Japan (Tokyo)
    "35.221.126.142", "34.97.138.165", "13.112.44.80",
    // South Korea (Seoul)
    "34.64.235.198", "15.164.70.12",
    // Singapore
    "34.128.68.219", "35.247.162.43", "34.87.112.90",
    // Hong Kong
    "34.92.192.14", "35.229.165.185", "56.68.61.220", "56.68.118.84",
    // India (Mumbai)
    "34.93.13.191", "34.131.13.17", "96.0.64.14", "96.0.65.148",
    // Australia (Sydney)
    "35.244.67.26", "34.129.43.95"
};

} // anonymous namespace

static std::vector<KonamiRegionInfo>& InitKonamiRegionsData() {
    static std::vector<KonamiRegionInfo> s_regions = {
        { KonamiRegion::Auto_All, "Auto / All Servers", "Global", "[Auto]", "Default Konami matchmaking. Pings all global clusters without restriction.", {}, "", -1.0f },
        { KonamiRegion::Madrid_ES, "Madrid", "Spain", "[ES]", "Konami Madrid Cluster (4 servers). Lowest physical latency for Morocco (25-45ms).",
            {"34.175.112.88", "51.95.38.54", "51.48.223.143", "15.216.87.136"}, "34.175.112.88", -1.0f },
        { KonamiRegion::Paris_FR, "Paris", "France", "[FR]", "Konami Paris Cluster (4 servers). High European player volume & fast tier-1 routing.",
            {"34.155.199.109", "15.236.165.57", "15.188.178.40", "15.188.70.90"}, "34.155.199.109", -1.0f },
        { KonamiRegion::Amsterdam_NL, "Amsterdam", "Netherlands", "[NL]", "Konami Amsterdam Cluster (4 servers). Low jitter AMS-IX exchange routing.",
            {"34.13.163.26", "34.38.154.179", "34.147.16.82", "35.204.72.11"}, "34.13.163.26", -1.0f },
        { KonamiRegion::Frankfurt_DE, "Frankfurt", "Germany", "[DE]", "Konami Central Europe Cluster (3 servers: Frankfurt, Berlin, Hamburg).",
            {"34.159.137.177", "34.32.98.33", "15.220.152.64"}, "34.159.137.177", -1.0f },
        { KonamiRegion::London_GB, "London", "United Kingdom", "[GB]", "Konami London Cluster (LINX Internet Exchange).",
            {"34.39.17.131", "35.242.180.12"}, "34.39.17.131", -1.0f },
        { KonamiRegion::Milan_IT, "Milan / Italy", "Italy", "[IT]", "Konami Italy Cluster (5 servers: Milan, Torino). Direct Mediterranean transit.",
            {"34.154.179.38", "34.17.93.174", "18.102.9.53", "15.161.73.105", "16.22.9.71"}, "34.154.179.38", -1.0f },
        { KonamiRegion::Zurich_CH, "Zurich", "Switzerland", "[CH]", "Konami Swiss Cluster (4 servers). Central European alpine transit.",
            {"34.65.93.216", "51.34.16.129", "51.96.162.11", "51.96.111.15"}, "34.65.93.216", -1.0f },
        { KonamiRegion::Warsaw_PL, "Warsaw", "Poland", "[PL]", "Konami Warsaw Cluster (3 servers). Eastern European matchmaking hub.",
            {"34.118.91.25", "34.116.180.11", "15.220.65.50"}, "34.118.91.25", -1.0f },
        { KonamiRegion::Helsinki_FI, "Helsinki", "Finland", "[FI]", "Konami Nordic Cluster (3 servers). Northern Europe & Baltic routing.",
            {"35.228.63.230", "34.88.120.45", "16.16.92.10"}, "35.228.63.230", -1.0f },
        { KonamiRegion::Athens_GR, "Athens", "Greece", "[GR]", "Konami Athens Cluster (5 servers). East Mediterranean routing.",
            {"75.45.132.226", "75.45.132.164", "75.45.131.224", "75.45.132.53", "75.45.132.133"}, "75.45.132.226", -1.0f },
        { KonamiRegion::Istanbul_TR, "Istanbul", "Turkey", "[TR]", "Konami Istanbul Cluster (5 servers). Optimal MENA matchmaking pool & smooth hitreg.",
            {"83.119.132.139", "83.119.135.24", "83.119.132.191", "83.119.132.5", "83.119.135.234"}, "83.119.132.139", -1.0f },
        { KonamiRegion::Dubai_AE, "Dubai / Gulf", "UAE & Gulf", "[AE]", "Konami Gulf Cluster (3 servers: Dubai, Dammam, Muscat).",
            {"34.18.108.143", "51.112.179.174", "13.152.0.18"}, "34.18.108.143", -1.0f },
        { KonamiRegion::Riyadh_SA, "Riyadh", "Saudi Arabia", "[SA]", "Konami Saudi Arabia Cluster (3 servers: Riyadh, Jeddah).",
            {"34.166.38.226", "15.206.180.90", "51.107.12.44"}, "34.166.38.226", -1.0f },
        { KonamiRegion::Doha_QA, "Doha", "Qatar", "[QA]", "Konami Qatar Cluster (2 servers). Gulf state dedicated matchmaker.",
            {"34.158.40.11", "51.112.243.44"}, "34.158.40.11", -1.0f },
        { KonamiRegion::Lagos_NG, "Lagos", "Nigeria", "[NG]", "Konami Lagos Cluster (8 servers). Dedicated African matchmaking hub.",
            {"96.0.47.74", "96.0.47.5", "96.0.47.198", "96.0.47.108", "96.0.41.221", "96.0.44.155", "96.0.46.52", "96.0.47.71"}, "96.0.47.74", -1.0f },
        { KonamiRegion::Johannesburg_ZA, "Johannesburg", "South Africa", "[ZA]", "Konami Southern Africa Cluster (2 servers).",
            {"34.35.84.61", "13.244.80.12"}, "34.35.84.61", -1.0f },
        { KonamiRegion::SaoPaulo_BR, "São Paulo", "Brazil", "[BR]", "Konami Brazil Cluster (6 servers). South America main server.",
            {"34.95.169.15", "35.198.22.80", "15.228.40.11", "96.0.16.253", "96.0.16.157", "96.0.16.27"}, "34.95.169.15", -1.0f },
        { KonamiRegion::Santiago_CL, "Santiago", "Chile", "[CL]", "Konami Chile / Pacific South America Cluster (6 servers).",
            {"34.176.17.72", "96.0.16.188", "96.0.16.199", "96.0.16.109", "96.0.20.177", "96.0.22.98"}, "34.176.17.72", -1.0f },
        { KonamiRegion::USEast_US, "US East (Virginia / NY)", "United States", "[US]", "Konami North America East Cluster (4 servers).",
            {"34.85.172.113", "35.203.65.250", "34.138.103.216", "34.86.110.15"}, "34.85.172.113", -1.0f },
        { KonamiRegion::USCentral_US, "US Central (Iowa / Chicago)", "United States", "[US]", "Konami North America Central Cluster (4 servers).",
            {"34.162.74.80", "35.224.16.17", "34.174.25.145", "34.66.190.22"}, "34.162.74.80", -1.0f },
        { KonamiRegion::USWest_US, "US West (Oregon / California)", "United States", "[US]", "Konami North America West Cluster (4 servers).",
            {"34.82.40.13", "34.106.69.65", "34.125.24.248", "34.102.12.90"}, "34.82.40.13", -1.0f },
        { KonamiRegion::Tokyo_JP, "Tokyo", "Japan", "[JP]", "Konami Japan Headquarters Cluster (3 servers). Ultra-low jitter domestic network.",
            {"35.221.126.142", "34.97.138.165", "13.112.44.80"}, "35.221.126.142", -1.0f },
        { KonamiRegion::Seoul_KR, "Seoul", "South Korea", "[KR]", "Konami South Korea Cluster (2 servers). Fast fiber Asian hub.",
            {"34.64.235.198", "15.164.70.12"}, "34.64.235.198", -1.0f },
        { KonamiRegion::Singapore_SG, "Singapore", "Singapore", "[SG]", "Konami Southeast Asia Cluster (3 servers). Multi-region Asian exchange.",
            {"34.128.68.219", "35.247.162.43", "34.87.112.90"}, "34.128.68.219", -1.0f },
        { KonamiRegion::HongKong_HK, "Hong Kong", "Hong Kong", "[HK]", "Konami East Asia Cluster (4 servers).",
            {"34.92.192.14", "35.229.165.185", "56.68.61.220", "56.68.118.84"}, "34.92.192.14", -1.0f },
        { KonamiRegion::Mumbai_IN, "Mumbai", "India", "[IN]", "Konami South Asia Cluster (4 servers).",
            {"34.93.13.191", "34.131.13.17", "96.0.64.14", "96.0.65.148"}, "34.93.13.191", -1.0f },
        { KonamiRegion::Sydney_AU, "Sydney", "Australia", "[AU]", "Konami Oceania Cluster (2 servers: Sydney, Melbourne).",
            {"35.244.67.26", "34.129.43.95"}, "35.244.67.26", -1.0f },
        { KonamiRegion::WestEurope_Preset, "Western Europe Hub", "EU Multi-Hub", "[EU]", "Allows Madrid, Paris, Amsterdam, Frankfurt, London, Milan & Zurich (blocks rest).",
            {
                "34.175.112.88", "51.95.38.54", "51.48.223.143", "15.216.87.136",
                "34.155.199.109", "15.236.165.57", "15.188.178.40", "15.188.70.90",
                "34.13.163.26", "34.38.154.179", "34.147.16.82", "35.204.72.11",
                "34.159.137.177", "34.32.98.33", "15.220.152.64",
                "34.39.17.131", "35.242.180.12",
                "34.154.179.38", "34.17.93.174", "18.102.9.53", "15.161.73.105", "16.22.9.71",
                "34.65.93.216", "51.34.16.129", "51.96.162.11", "51.96.111.15"
            }, "34.175.112.88", -1.0f
        }
    };
    return s_regions;
}

const std::vector<KonamiRegionInfo>& NetworkOptimizer::GetKonamiRegions() {
    return InitKonamiRegionsData();
}

std::vector<KonamiRegionInfo>& NetworkOptimizer::GetKonamiRegionsMutable() {
    return InitKonamiRegionsData();
}

void NetworkOptimizer::BenchmarkAllKonamiRegions(std::atomic<bool>& isRunningFlag, std::function<void(KonamiRegion best, float ping)> onFinish) {
    if (isRunningFlag.exchange(true)) {
        return;
    }

    std::thread([&isRunningFlag, onFinish = std::move(onFinish)]() {
        Log(LogLevel::Net, "Launching parallel multi-threaded ICMP latency benchmark across all Konami regions...");
        auto& regions = GetKonamiRegionsMutable();
        std::vector<std::thread> workers;

        for (auto& reg : regions) {
            if (reg.region == KonamiRegion::Auto_All || reg.probeIp.empty()) continue;

            workers.emplace_back([&reg]() {
                HANDLE hIcmp = IcmpCreateFile();
                if (hIcmp == INVALID_HANDLE_VALUE) return;

                IN_ADDR inAddr{};
                if (inet_pton(AF_INET, reg.probeIp.c_str(), &inAddr) != 1) {
                    IcmpCloseHandle(hIcmp);
                    return;
                }

                char sendData[] = "eBoosterProbe";
                DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
                std::vector<uint8_t> replyBuffer(replySize);

                std::vector<uint32_t> samples;
                for (int s = 0; s < 3; ++s) {
                    DWORD replies = IcmpSendEcho(hIcmp, inAddr.s_addr, sendData, static_cast<WORD>(sizeof(sendData)),
                                                 nullptr, replyBuffer.data(), replySize, 500);
                    if (replies > 0) {
                        auto* pEcho = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuffer.data());
                        if (pEcho->Status == IP_SUCCESS) {
                            samples.push_back(pEcho->RoundTripTime);
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(25));
                }
                IcmpCloseHandle(hIcmp);

                if (!samples.empty()) {
                    std::sort(samples.begin(), samples.end());
                    reg.livePing = static_cast<float>(samples[samples.size() / 2]);
                } else {
                    reg.livePing = 999.0f;
                }
            });
        }

        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }

        // Set West Europe preset to lowest among European clusters
        float westMin = 999.0f;
        for (const auto& r : regions) {
            if ((r.region == KonamiRegion::Madrid_ES || r.region == KonamiRegion::Paris_FR ||
                 r.region == KonamiRegion::Frankfurt_DE || r.region == KonamiRegion::Milan_IT ||
                 r.region == KonamiRegion::London_GB) && r.livePing > 0.0f) {
                if (r.livePing < westMin) westMin = r.livePing;
            }
        }
        for (auto& r : regions) {
            if (r.region == KonamiRegion::WestEurope_Preset) {
                r.livePing = (westMin < 900.0f) ? westMin : -1.0f;
            }
        }

        // Find Best Specific Region
        KonamiRegion best = KonamiRegion::Auto_All;
        float bestLatency = 999.0f;
        for (const auto& r : regions) {
            if (r.region != KonamiRegion::Auto_All && r.region != KonamiRegion::WestEurope_Preset) {
                if (r.livePing > 0.0f && r.livePing < bestLatency) {
                    bestLatency = r.livePing;
                    best = r.region;
                }
            }
        }

        LogFmt(LogLevel::Success, "Konami Region Benchmark Complete: Best server is identified with {:.1f}ms latency.",
               bestLatency);

        isRunningFlag.store(false);

        if (onFinish) {
            onFinish(best, bestLatency);
        }
    }).detach();
}

bool NetworkOptimizer::CheckEFootballGeofilterActive() {
    bool exists = RunSilentNetsh(L"show rule name=\"eBooster_eFootball_Geofilter\"");
    if (!exists) {
        s_currentGeofilter.store(KonamiRegion::Auto_All);
    }
    return exists;
}

KonamiRegion NetworkOptimizer::GetCurrentEFootballGeofilter() {
    return s_currentGeofilter.load();
}

bool NetworkOptimizer::SetEFootballGeofilter(KonamiRegion region, const std::wstring& customExePath, std::string* outDetails) {
    // 1. Always purge previous geofilter rule to ensure clean state
    RunSilentNetsh(L"delete rule name=\"eBooster_eFootball_Geofilter\"");

    // 2. If Auto_All, restore unconstrained matchmaking
    if (region == KonamiRegion::Auto_All) {
        s_currentGeofilter.store(KonamiRegion::Auto_All);
        std::string msg = "Konami Dedicated Server Geofilter DEACTIVATED: All global server clusters unlocked (Auto Matchmaking).";
        if (outDetails) *outDetails = msg;
        Log(LogLevel::Info, msg);
        return true;
    }

    // 3. Find target region details
    const auto& regions = GetKonamiRegions();
    auto it = std::find_if(regions.begin(), regions.end(), [region](const KonamiRegionInfo& r) {
        return r.region == region;
    });

    if (it == regions.end() || it->ips.empty()) {
        std::string msg = "Selected Konami region has no registered server IPs.";
        if (outDetails) *outDetails = msg;
        Log(LogLevel::Error, msg);
        return false;
    }

    // 4. Build whitelist and determine blacklisted IPs
    std::unordered_set<std::string> allowedSet(it->ips.begin(), it->ips.end());
    std::vector<std::string> blockedIps;
    blockedIps.reserve(s_allKonamiGameServers.size());

    for (const auto& ip : s_allKonamiGameServers) {
        if (!allowedSet.contains(ip)) {
            blockedIps.push_back(ip);
        }
    }

    if (blockedIps.empty()) {
        std::string msg = "No servers to blackhole.";
        if (outDetails) *outDetails = msg;
        return false;
    }

    // 5. Build comma-separated IP string
    std::string blockedIpsStr;
    for (size_t i = 0; i < blockedIps.size(); ++i) {
        if (i > 0) blockedIpsStr += ",";
        blockedIpsStr += blockedIps[i];
    }

    // 6. Target executable path (Process-isolated if known)
    std::wstring targetPath = customExePath.empty() ? FindEFootballPath() : customExePath;
    std::wstring addCmd;
    if (!targetPath.empty()) {
        addCmd = std::format(
            L"add rule name=\"eBooster_eFootball_Geofilter\" dir=out action=block program=\"{}\" protocol=UDP remoteport=5521,50000,10000,30000,5735 remoteip=\"{}\" enable=yes",
            targetPath, ToWideString(blockedIpsStr));
    } else {
        addCmd = std::format(
            L"add rule name=\"eBooster_eFootball_Geofilter\" dir=out action=block protocol=UDP remoteport=5521,50000,10000,30000,5735 remoteip=\"{}\" enable=yes",
            ToWideString(blockedIpsStr));
    }

    // 7. Execute silent netsh
    bool ok = RunSilentNetsh(addCmd);
    if (ok) {
        s_currentGeofilter.store(region);
        std::string msg = std::format(
            "[LOCK] Konami Dedicated Server LOCKED to {} ({}): {} remote server IPs blackholed. eFootball matchmaker forced to {} server.",
            it->name, it->country, blockedIps.size(), it->name);
        if (outDetails) *outDetails = msg;
        Log(LogLevel::Success, msg);
        return true;
    } else {
        std::string msg = "Failed to add Konami Geofilter firewall rule (Verify Administrator elevation).";
        if (outDetails) *outDetails = msg;
        Log(LogLevel::Error, msg);
        return false;
    }
}

} // namespace ebooster
