#pragma once

#include "ebooster/Common.hpp"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace ebooster {

struct ProcessInfo {
    DWORD pid{0};
    std::wstring exeName;
    DWORD parentPid{0};
};

// Preset mapping for game launchers to their child game binaries
struct LauncherPreset {
    std::wstring launcherExe;               // e.g. L"steam.exe"
    std::wstring friendlyName;              // e.g. L"Steam"
    std::vector<std::wstring> childGameExes; // Known child game binaries
    std::vector<std::wstring> ignoredExes;   // Non-game utility binaries (CEF renderers, overlays, crashpad)
};

using ProcessStartCallback = std::function<void(DWORD pid, const std::wstring& exeName)>;
using ProcessStopCallback  = std::function<void(DWORD pid, const std::wstring& exeName)>;

class ProcessMonitor {
public:
    explicit ProcessMonitor(std::wstring targetExeName, uint32_t pollIntervalMs = 500);
    ~ProcessMonitor();

    // Non-copyable, non-movable
    ProcessMonitor(const ProcessMonitor&) = delete;
    ProcessMonitor& operator=(const ProcessMonitor&) = delete;

    void SetCallbacks(ProcessStartCallback onStart, ProcessStopCallback onStop);
    void SetChildTargets(std::vector<std::wstring> childExes);
    void SetLauncherTargets(std::vector<std::wstring> launcherExes);

    bool Start();
    void Stop();

    bool IsRunning() const noexcept { return m_isRunning.load(); }
    bool HasActiveGameProcess() const;
    std::vector<DWORD> GetActivePids() const;
    std::unordered_map<DWORD, std::wstring> GetActiveProcesses() const;

    // Presets & auto-detection helpers
    static const std::vector<LauncherPreset>& GetDefaultLauncherPresets();
    static const LauncherPreset* FindPresetForLauncher(const std::wstring& exeName);
    static const LauncherPreset* FindPresetForGame(const std::wstring& exeName);
    static bool IsKnownLauncher(const std::wstring& exeName);

private:
    void WorkerLoop(std::stop_token stopToken);
    std::unordered_map<DWORD, std::wstring> EnumerateTargetProcesses();

    std::wstring m_targetExeName;
    std::vector<std::wstring> m_childExeNames;
    std::vector<std::wstring> m_launcherExeNames;
    bool m_trackChildProcesses{true};
    uint32_t m_pollIntervalMs;

    ProcessStartCallback m_onStart;
    ProcessStopCallback  m_onStop;

    std::atomic<bool> m_isRunning{false};
    std::jthread m_workerThread;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::unordered_map<DWORD, std::wstring> m_activeProcesses;
};

} // namespace ebooster
