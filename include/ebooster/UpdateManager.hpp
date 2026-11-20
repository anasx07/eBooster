#pragma once

#include "ebooster/Common.hpp"

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <filesystem>

namespace ebooster {

struct UpdateInfo {
    bool available{false};
    std::string currentVersion{EBOOSTER_VERSION};
    std::string latestVersion;
    std::string releaseTitle;
    std::string releaseNotes;
    std::string releaseUrl;
    std::string downloadUrl;
    uint64_t assetSize{0};
    std::string assetName;
};

enum class UpdateState {
    Idle,
    Checking,
    UpToDate,
    UpdateAvailable,
    Downloading,
    Downloaded,
    Failed
};

class UpdateManager {
public:
    explicit UpdateManager(std::string githubRepo = "anasx07/eBooster", std::string currentVersion = EBOOSTER_VERSION);
    ~UpdateManager();

    // Configuration
    void SetRepository(const std::string& repo);
    const std::string& GetRepository() const;
    const std::string& GetCurrentVersion() const;

    // Status queries
    UpdateState GetState() const { return m_state.load(); }
    UpdateInfo GetUpdateInfo() const;
    std::string GetStatusMessage() const;
    float GetDownloadProgress() const { return m_downloadProgress.load(); }
    uint64_t GetDownloadedBytes() const { return m_downloadedBytes.load(); }
    uint64_t GetTotalBytes() const { return m_totalBytes.load(); }

    // Actions
    void CheckForUpdatesAsync(std::function<void(const UpdateInfo&)> onComplete = nullptr);
    void DownloadUpdateAsync(std::function<void(bool success)> onComplete = nullptr);
    bool ApplyUpdateAndRestart();
    void ResetState();

    // Semver comparison helper
    static bool IsNewerVersion(const std::string& currentVer, const std::string& remoteVer);

    // Startup cleanup of previous update backup (.old.exe)
    static void CleanupOldExecutable();

private:
    std::string m_githubRepo;
    std::string m_currentVersion;

    std::atomic<UpdateState> m_state{UpdateState::Idle};
    UpdateInfo m_updateInfo;
    std::string m_statusMessage;
    std::filesystem::path m_downloadedFilePath;

    std::atomic<float> m_downloadProgress{0.0f};
    std::atomic<uint64_t> m_downloadedBytes{0};
    std::atomic<uint64_t> m_totalBytes{0};

    std::jthread m_workerThread;
    mutable std::mutex m_mutex;
};

} // namespace ebooster
