#include "ebooster/ProcessMonitor.hpp"
#include <algorithm>
#include <unordered_set>

namespace ebooster {

namespace {

bool MatchesList(const std::wstring& exe, const std::vector<std::wstring>& list) {
    for (const auto& item : list) {
        if (_wcsicmp(exe.c_str(), item.c_str()) == 0) {
            return true;
        }
    }
    return false;
}

bool IsIgnoredUtility(const std::wstring& exe, const LauncherPreset* preset) {
    if (preset && MatchesList(exe, preset->ignoredExes)) {
        return true;
    }

    // Generic Windows and helper utilities that should never be tracked as games
    static const std::vector<std::wstring> genericIgnored = {
        L"conhost.exe", L"cmd.exe", L"powershell.exe", L"explorer.exe",
        L"rundll32.exe", L"werfault.exe", L"wermgr.exe", L"CrashReportClient.exe",
        L"vcredist_x64.exe", L"DirectX_Setup.exe", L"dxwebsetup.exe",
        L"steamwebhelper.exe", L"gameoverlayui.exe", L"steamservice.exe",
        L"RiotClientUx.exe", L"RiotClientUxRender.exe", L"RiotClientCrashHandler.exe",
        L"EpicWebHelper.exe", L"UnrealCEFSubProcess.exe", L"Agent.exe"
    };

    return MatchesList(exe, genericIgnored);
}

} // anonymous namespace

ProcessMonitor::ProcessMonitor(std::wstring targetExeName, uint32_t pollIntervalMs)
    : m_targetExeName(std::move(targetExeName))
    , m_pollIntervalMs(pollIntervalMs) {
}

ProcessMonitor::~ProcessMonitor() {
    Stop();
}

void ProcessMonitor::SetCallbacks(ProcessStartCallback onStart, ProcessStopCallback onStop) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onStart = std::move(onStart);
    m_onStop  = std::move(onStop);
}

void ProcessMonitor::SetChildTargets(std::vector<std::wstring> childExes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_childExeNames = std::move(childExes);
}

void ProcessMonitor::SetLauncherTargets(std::vector<std::wstring> launcherExes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_launcherExeNames = std::move(launcherExes);
}

std::unordered_map<DWORD, std::wstring> ProcessMonitor::GetActiveProcesses() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeProcesses;
}

const std::vector<LauncherPreset>& ProcessMonitor::GetDefaultLauncherPresets() {
    static const std::vector<LauncherPreset> s_presets = {
        {
            .launcherExe = L"steam.exe",
            .friendlyName = L"Steam",
            .childGameExes = {
                L"cs2.exe", L"dota2.exe", L"deadlock.exe", L"tf_win64.exe", L"tf.exe",
                L"rustclient.exe", L"pubg.exe", L"TslGame.exe", L"Destiny2.exe",
                L"r5apex.exe", L"r5apex_dx12.exe", L"RainbowSix.exe", L"RainbowSix_Vulkan.exe",
                L"RocketLeague.exe", L"DayZ_x64.exe", L"Helldivers2.exe"
            },
            .ignoredExes = {
                L"steamwebhelper.exe", L"gameoverlayui.exe", L"steamservice.exe",
                L"steamerrorreporter.exe", L"gldriverquery.exe", L"gldriverquery64.exe",
                L"vulkaninfo.exe", L"vulkaninfo64.exe", L"secure_desktop_wrapper.exe"
            }
        },
        {
            .launcherExe = L"RiotClientServices.exe",
            .friendlyName = L"Riot Games Launcher",
            .childGameExes = {
                L"VALORANT-Win64-Shipping.exe", L"VALORANT.exe",
                L"League of Legends.exe", L"LeagueClient.exe"
            },
            .ignoredExes = {
                L"RiotClientUx.exe", L"RiotClientUxRender.exe", L"RiotClientCrashHandler.exe",
                L"vgtray.exe", L"vgc.exe"
            }
        },
        {
            .launcherExe = L"RiotClientUx.exe",
            .friendlyName = L"Riot Client UX",
            .childGameExes = {
                L"VALORANT-Win64-Shipping.exe", L"VALORANT.exe",
                L"League of Legends.exe", L"LeagueClient.exe"
            },
            .ignoredExes = {
                L"RiotClientUxRender.exe", L"RiotClientCrashHandler.exe"
            }
        },
        {
            .launcherExe = L"Battle.net.exe",
            .friendlyName = L"Battle.net",
            .childGameExes = {
                L"cod.exe", L"bootstrapper.exe", L"Overwatch.exe", L"Overwatch2.exe",
                L"BlackOpsColdWar.exe", L"ModernWarfare.exe", L"Diablo IV.exe"
            },
            .ignoredExes = {
                L"Agent.exe", L"Battle.net Helper.exe", L"BlizzardError.exe"
            }
        },
        {
            .launcherExe = L"EpicGamesLauncher.exe",
            .friendlyName = L"Epic Games Launcher",
            .childGameExes = {
                L"FortniteClient-Win64-Shipping.exe", L"FortniteLauncher.exe",
                L"RocketLeague.exe", L"FallGuys_client_game.exe", L"GenshinImpact.exe"
            },
            .ignoredExes = {
                L"EpicWebHelper.exe", L"UnrealCEFSubProcess.exe", L"CrashReportClient.exe"
            }
        },
        {
            .launcherExe = L"EADesktop.exe",
            .friendlyName = L"EA app",
            .childGameExes = {
                L"r5apex.exe", L"r5apex_dx12.exe", L"BF2042.exe", L"FC24.exe",
                L"FC25.exe", L"FIFA23.exe"
            },
            .ignoredExes = {
                L"EABackgroundService.exe", L"EALauncher.exe", L"QtWebEngineProcess.exe",
                L"Link2EA.exe", L"EAConnect_microsoft.exe"
            }
        },
        {
            .launcherExe = L"Origin.exe",
            .friendlyName = L"Origin",
            .childGameExes = {
                L"r5apex.exe", L"r5apex_dx12.exe", L"BF2042.exe"
            },
            .ignoredExes = {
                L"OriginWebHelperService.exe", L"OriginCrashReporter.exe"
            }
        },
        {
            .launcherExe = L"upc.exe",
            .friendlyName = L"Ubisoft Connect",
            .childGameExes = {
                L"RainbowSix.exe", L"RainbowSix_Vulkan.exe", L"TheDivision2.exe",
                L"GhostReconBreakpoint.exe"
            },
            .ignoredExes = {
                L"UbisoftConnect.exe", L"UbisoftGameLauncher.exe", L"UbisoftGameLauncher64.exe",
                L"UplayCrashReporter.exe"
            }
        }
    };
    return s_presets;
}

const LauncherPreset* ProcessMonitor::FindPresetForLauncher(const std::wstring& exeName) {
    if (exeName.empty()) return nullptr;
    const auto& presets = GetDefaultLauncherPresets();
    for (const auto& p : presets) {
        if (_wcsicmp(p.launcherExe.c_str(), exeName.c_str()) == 0) {
            return &p;
        }
    }
    return nullptr;
}

const LauncherPreset* ProcessMonitor::FindPresetForGame(const std::wstring& exeName) {
    if (exeName.empty()) return nullptr;
    const auto& presets = GetDefaultLauncherPresets();
    for (const auto& p : presets) {
        for (const auto& child : p.childGameExes) {
            if (_wcsicmp(child.c_str(), exeName.c_str()) == 0) {
                return &p;
            }
        }
    }
    return nullptr;
}

bool ProcessMonitor::IsKnownLauncher(const std::wstring& exeName) {
    return FindPresetForLauncher(exeName) != nullptr;
}

bool ProcessMonitor::Start() {
    if (m_isRunning.exchange(true)) {
        return false; // Already running
    }

    LogFmt(LogLevel::Process, "Process Monitor started. Target: '{}' (Poll: {}ms, Child Tracking: {})",
           ToNarrowString(m_targetExeName), m_pollIntervalMs, m_trackChildProcesses ? "ENABLED" : "DISABLED");

    if (IsKnownLauncher(m_targetExeName)) {
        LogFmt(LogLevel::Info, "Target '{}' recognized as Game Launcher. Auto-tracking child game instances via Windows process hierarchy.",
               ToNarrowString(m_targetExeName));
    }

    m_workerThread = std::jthread([this](std::stop_token st) {
        WorkerLoop(st);
    });

    return true;
}

void ProcessMonitor::Stop() {
    if (!m_isRunning.exchange(false)) {
        return;
    }

    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_cv.notify_all();
        m_workerThread.join();
    }

    // Clear remaining active processes on shutdown without triggering cascaded callbacks
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeProcesses.clear();
    }

    LogFmt(LogLevel::Process, "Process Monitor stopped.");
}

bool ProcessMonitor::HasActiveGameProcess() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_activeProcesses.empty();
}

std::vector<DWORD> ProcessMonitor::GetActivePids() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DWORD> pids;
    pids.reserve(m_activeProcesses.size());
    for (const auto& [pid, _] : m_activeProcesses) {
        pids.push_back(pid);
    }
    return pids;
}

std::unordered_map<DWORD, std::wstring> ProcessMonitor::EnumerateTargetProcesses() {
    std::unordered_map<DWORD, std::wstring> activeGames;
    std::wstring target;
    std::vector<std::wstring> childTargets;
    std::vector<std::wstring> launcherTargets;
    bool trackChildren = true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        target = m_targetExeName;
        childTargets = m_childExeNames;
        launcherTargets = m_launcherExeNames;
        trackChildren = m_trackChildProcesses;
    }

    if (target.empty() && childTargets.empty()) {
        return activeGames;
    }

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) {
        LogFmt(LogLevel::Error, "CreateToolhelp32Snapshot failed with error code {}", GetLastError());
        return activeGames;
    }
    UniqueHandle snapGuard = MakeUniqueHandle(hSnap);

    struct ProcRecord {
        DWORD pid;
        DWORD parentPid;
        std::wstring exeName;
    };

    std::vector<ProcRecord> procs;
    std::unordered_map<DWORD, size_t> pidToIndex;
    std::unordered_map<DWORD, std::vector<DWORD>> parentToChildren;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapGuard.get(), &pe)) {
        do {
            size_t idx = procs.size();
            pidToIndex[pe.th32ProcessID] = idx;
            parentToChildren[pe.th32ParentProcessID].push_back(pe.th32ProcessID);
            procs.push_back(ProcRecord{
                .pid = pe.th32ProcessID,
                .parentPid = pe.th32ParentProcessID,
                .exeName = pe.szExeFile
            });
        } while (Process32NextW(snapGuard.get(), &pe));
    }

    // Determine if target itself is a known launcher
    const LauncherPreset* targetPreset = FindPresetForLauncher(target);
    bool targetIsLauncher = (targetPreset != nullptr) || IsKnownLauncher(target);

    // If target is a game, look up its launcher preset (e.g. CS2 -> Steam, Valorant -> Riot Client)
    const LauncherPreset* gamePreset = nullptr;
    if (!targetIsLauncher) {
        gamePreset = FindPresetForGame(target);
    }

    // 1. Direct Target Matches (only if target is NOT a launcher)
    if (!targetIsLauncher) {
        for (const auto& p : procs) {
            if (_wcsicmp(p.exeName.c_str(), target.c_str()) == 0) {
                activeGames[p.pid] = p.exeName;
            }
            if (MatchesList(p.exeName, childTargets)) {
                activeGames[p.pid] = p.exeName;
            }
        }
    }

    // 2. Child Process Hierarchy Tracking via th32ParentProcessID
    if (trackChildren) {
        // Collect candidate parent PIDs
        std::vector<std::pair<DWORD, const LauncherPreset*>> parentLaunchers;

        for (const auto& p : procs) {
            // Check if process matches target (when target is a launcher)
            if (targetIsLauncher && _wcsicmp(p.exeName.c_str(), target.c_str()) == 0) {
                parentLaunchers.emplace_back(p.pid, targetPreset);
            }
            // Check if process matches configured launcher targets
            if (MatchesList(p.exeName, launcherTargets)) {
                parentLaunchers.emplace_back(p.pid, FindPresetForLauncher(p.exeName));
            }
            // Check if process matches preset launcher for this game
            if (gamePreset && _wcsicmp(p.exeName.c_str(), gamePreset->launcherExe.c_str()) == 0) {
                parentLaunchers.emplace_back(p.pid, gamePreset);
            }
            // If direct target is running, also track its spawned child processes (e.g. bootstrap -> shipping binary)
            if (!targetIsLauncher && _wcsicmp(p.exeName.c_str(), target.c_str()) == 0) {
                parentLaunchers.emplace_back(p.pid, nullptr);
            }
        }

        // BFS traversal down the process tree for each candidate parent
        for (const auto& [parentPid, preset] : parentLaunchers) {
            std::vector<DWORD> queue;
            if (auto it = parentToChildren.find(parentPid); it != parentToChildren.end()) {
                queue.insert(queue.end(), it->second.begin(), it->second.end());
            }

            std::unordered_set<DWORD> visited;
            while (!queue.empty()) {
                DWORD childPid = queue.back();
                queue.pop_back();

                if (!visited.insert(childPid).second) continue;

                auto it = pidToIndex.find(childPid);
                if (it == pidToIndex.end()) continue;
                const auto& childProc = procs[it->second];

                // Skip ignored utility binaries
                if (IsIgnoredUtility(childProc.exeName, preset)) {
                    continue;
                }

                bool isMatch = false;

                // Match against primary target
                if (!targetIsLauncher && _wcsicmp(childProc.exeName.c_str(), target.c_str()) == 0) {
                    isMatch = true;
                }
                // Match against explicit child targets
                else if (MatchesList(childProc.exeName, childTargets)) {
                    isMatch = true;
                }
                // Match against preset child games
                else if (preset && MatchesList(childProc.exeName, preset->childGameExes)) {
                    isMatch = true;
                }
                // If user selected a launcher directly without explicit child list, match non-utility executables
                else if (targetIsLauncher && !preset && !IsIgnoredUtility(childProc.exeName, nullptr)) {
                    isMatch = true;
                }

                if (isMatch) {
                    activeGames[childProc.pid] = childProc.exeName;
                }

                // Continue traversing grandchildren (e.g. Launcher -> Bootstrap -> Game Binary)
                if (auto childIt = parentToChildren.find(childPid); childIt != parentToChildren.end()) {
                    for (DWORD grandChild : childIt->second) {
                        queue.push_back(grandChild);
                    }
                }
            }
        }
    }

    return activeGames;
}

void ProcessMonitor::WorkerLoop(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        auto currentProcesses = EnumerateTargetProcesses();

        std::vector<std::pair<DWORD, std::wstring>> newlyStarted;
        std::vector<std::pair<DWORD, std::wstring>> newlyStopped;
        ProcessStartCallback onStart;
        ProcessStopCallback  onStop;
        std::wstring currentTarget;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            onStart = m_onStart;
            onStop  = m_onStop;
            currentTarget = m_targetExeName;

            // Detect new starts
            for (const auto& [pid, exe] : currentProcesses) {
                if (!m_activeProcesses.contains(pid)) {
                    newlyStarted.emplace_back(pid, exe);
                }
            }

            // Detect terminations
            for (const auto& [pid, exe] : m_activeProcesses) {
                if (!currentProcesses.contains(pid)) {
                    newlyStopped.emplace_back(pid, exe);
                }
            }

            // Update state
            m_activeProcesses = currentProcesses;
        }

        // Fire callbacks outside the lock to prevent deadlocks
        for (const auto& [pid, exe] : newlyStarted) {
            if (_wcsicmp(exe.c_str(), currentTarget.c_str()) != 0) {
                LogFmt(LogLevel::Success, "Child game process detected via hierarchy: {} (PID: {}) [Target: {}]",
                       ToNarrowString(exe), pid, ToNarrowString(currentTarget));
            } else {
                LogFmt(LogLevel::Success, "Game process detected: {} (PID: {})", ToNarrowString(exe), pid);
            }

            if (onStart) {
                onStart(pid, exe);
            }
        }

        for (const auto& [pid, exe] : newlyStopped) {
            LogFmt(LogLevel::Warn, "Game process terminated: {} (PID: {})", ToNarrowString(exe), pid);
            if (onStop) {
                onStop(pid, exe);
            }
        }

        // Low-latency, zero CPU busy-waiting sleep with cooperative cancellation
        std::unique_lock<std::mutex> cvLock(m_mutex);
        m_cv.wait_for(cvLock, std::chrono::milliseconds(m_pollIntervalMs), [&stopToken] {
            return stopToken.stop_requested();
        });
    }
}

} // namespace ebooster
