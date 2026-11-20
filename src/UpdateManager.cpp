#include "ebooster/UpdateManager.hpp"
#include "ebooster/Common.hpp"

#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#if defined(_MSC_VER)
#pragma comment(lib, "winhttp.lib")
#endif

namespace ebooster {

namespace {

struct WinHttpHandleDeleter {
    void operator()(HINTERNET h) const noexcept {
        if (h) {
            WinHttpCloseHandle(h);
        }
    }
};

using ScopedHINTERNET = std::unique_ptr<void, WinHttpHandleDeleter>;

bool ExtractJsonString(const std::string& json, const std::string& key, std::string& outVal, size_t startSearch = 0) {
    std::string needle = "\"" + key + "\"";
    size_t keyPos = json.find(needle, startSearch);
    if (keyPos == std::string::npos) return false;

    size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) return false;

    size_t startQuote = json.find('"', colonPos + 1);
    if (startQuote == std::string::npos) return false;

    // Handle escaped quotes inside string
    size_t endQuote = startQuote + 1;
    while (endQuote < json.size()) {
        if (json[endQuote] == '"' && json[endQuote - 1] != '\\') {
            break;
        }
        ++endQuote;
    }
    if (endQuote >= json.size()) return false;

    std::string raw = json.substr(startQuote + 1, endQuote - startQuote - 1);
    
    // Unescape basic json characters: \n, \r, \t, \"
    std::string unescaped;
    unescaped.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            if (next == 'n') { unescaped += '\n'; ++i; }
            else if (next == 'r') { unescaped += '\r'; ++i; }
            else if (next == 't') { unescaped += '\t'; ++i; }
            else if (next == '"') { unescaped += '"'; ++i; }
            else if (next == '\\') { unescaped += '\\'; ++i; }
            else { unescaped += next; ++i; }
        } else {
            unescaped += raw[i];
        }
    }

    outVal = std::move(unescaped);
    return true;
}

} // namespace

UpdateManager::UpdateManager(std::string githubRepo, std::string currentVersion)
    : m_githubRepo(std::move(githubRepo))
    , m_currentVersion(std::move(currentVersion)) {
    m_updateInfo.currentVersion = m_currentVersion;
}

UpdateManager::~UpdateManager() {
    if (m_workerThread.joinable()) {
        m_workerThread.request_stop();
        m_workerThread.join();
    }
}

void UpdateManager::SetRepository(const std::string& repo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_githubRepo = repo;
}

const std::string& UpdateManager::GetRepository() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_githubRepo;
}

const std::string& UpdateManager::GetCurrentVersion() const {
    return m_currentVersion;
}

UpdateInfo UpdateManager::GetUpdateInfo() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_updateInfo;
}

std::string UpdateManager::GetStatusMessage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_statusMessage;
}

void UpdateManager::ResetState() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = UpdateState::Idle;
    m_statusMessage.clear();
    m_downloadProgress = 0.0f;
    m_downloadedBytes = 0;
    m_totalBytes = 0;
}

bool UpdateManager::IsNewerVersion(const std::string& currentVer, const std::string& remoteVer) {
    auto parseVersion = [](std::string_view s) -> std::vector<int> {
        std::vector<int> parts;
        if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
            s.remove_prefix(1);
        }
        int cur = 0;
        bool hasNum = false;
        for (char c : s) {
            if (c >= '0' && c <= '9') {
                cur = cur * 10 + (c - '0');
                hasNum = true;
            } else if (c == '.' || c == '-' || c == '_') {
                if (hasNum) {
                    parts.push_back(cur);
                    cur = 0;
                    hasNum = false;
                }
            }
        }
        if (hasNum) {
            parts.push_back(cur);
        }
        while (parts.size() < 3) {
            parts.push_back(0);
        }
        return parts;
    };

    auto curParts = parseVersion(currentVer);
    auto remParts = parseVersion(remoteVer);

    for (size_t i = 0; i < std::min(curParts.size(), remParts.size()); ++i) {
        if (remParts[i] > curParts[i]) return true;
        if (remParts[i] < curParts[i]) return false;
    }
    return remParts.size() > curParts.size();
}

void UpdateManager::CheckForUpdatesAsync(std::function<void(const UpdateInfo&)> onComplete) {
    if (m_state == UpdateState::Checking || m_state == UpdateState::Downloading) {
        return;
    }

    m_state = UpdateState::Checking;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusMessage = "Connecting to GitHub Releases API...";
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_workerThread = std::jthread([this, onComplete = std::move(onComplete)](std::stop_token st) {
        std::string repo;
        std::string currentVer;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            repo = m_githubRepo;
            currentVer = m_currentVersion;
        }

        UpdateInfo info;
        info.currentVersion = currentVer;

        // Initialize WinHTTP
        ScopedHINTERNET hSession(WinHttpOpen(
            L"eBooster-Updater/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        ));

        if (!hSession) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Failed to initialize Windows HTTP client.";
            if (onComplete) onComplete(info);
            return;
        }

        ScopedHINTERNET hConnect(WinHttpConnect(
            hSession.get(),
            L"api.github.com",
            INTERNET_DEFAULT_HTTPS_PORT,
            0
        ));

        if (!hConnect) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Could not reach api.github.com.";
            if (onComplete) onComplete(info);
            return;
        }

        std::wstring apiPath = L"/repos/" + ToWideString(repo) + L"/releases/latest";
        ScopedHINTERNET hRequest(WinHttpOpenRequest(
            hConnect.get(),
            L"GET",
            apiPath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        ));

        if (!hRequest) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Failed to create HTTP request.";
            if (onComplete) onComplete(info);
            return;
        }

        // Set GitHub API User-Agent and Accept headers
        LPCWSTR headers = L"User-Agent: eBooster-App\r\nAccept: application/vnd.github.v3+json\r\n";
        WinHttpAddRequestHeaders(hRequest.get(), headers, static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD);

        BOOL sendOk = WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!sendOk || !WinHttpReceiveResponse(hRequest.get(), nullptr)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Failed to receive response from GitHub.";
            if (onComplete) onComplete(info);
            return;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (statusCode == 404) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::UpToDate;
            m_statusMessage = "No published releases found on this repository.";
            if (onComplete) onComplete(info);
            return;
        }

        if (statusCode != 200) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = std::format("GitHub API returned error (HTTP {}).", statusCode);
            if (onComplete) onComplete(info);
            return;
        }

        // Read entire JSON response
        std::string json;
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest.get(), &bytesAvailable) && bytesAvailable > 0) {
            if (st.stop_requested()) return;
            std::vector<char> buffer(bytesAvailable + 1);
            DWORD bytesRead = 0;
            if (WinHttpReadData(hRequest.get(), buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
                json.append(buffer.data(), bytesRead);
            }
        }

        // Parse release fields
        std::string tag, title, body, releaseHtmlUrl;
        ExtractJsonString(json, "tag_name", tag);
        ExtractJsonString(json, "name", title);
        ExtractJsonString(json, "body", body);
        ExtractJsonString(json, "html_url", releaseHtmlUrl);

        info.latestVersion = tag;
        info.releaseTitle  = title.empty() ? tag : title;
        info.releaseNotes  = body;
        info.releaseUrl    = releaseHtmlUrl;

        // Find binary asset download URL in assets array
        // Prefer "eBooster.exe", then any ".exe", then ".zip"
        size_t assetsPos = json.find("\"assets\"");
        if (assetsPos != std::string::npos) {
            size_t searchPos = assetsPos;
            std::string candidateUrl;
            std::string candidateName;

            while (true) {
                std::string assetUrl, assetName;
                size_t nextUrl = json.find("\"browser_download_url\"", searchPos);
                if (nextUrl == std::string::npos) break;

                ExtractJsonString(json, "browser_download_url", assetUrl, nextUrl);
                ExtractJsonString(json, "name", assetName, std::max(assetsPos, nextUrl > 300 ? nextUrl - 300 : assetsPos));

                std::string lowerName = assetName;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });

                // For in-app binary replacement, never download setup/installer packages
                if (lowerName.find("setup") != std::string::npos || lowerName.find("installer") != std::string::npos) {
                    searchPos = nextUrl + 22;
                    continue;
                }

                if (lowerName == "ebooster.exe" || lowerName == "ebooster-portable.exe") {
                    info.downloadUrl = assetUrl;
                    info.assetName = assetName;
                    break;
                } else if (lowerName.ends_with(".exe") && candidateUrl.empty()) {
                    candidateUrl = assetUrl;
                    candidateName = assetName;
                }

                searchPos = nextUrl + 22;
            }

            if (info.downloadUrl.empty() && !candidateUrl.empty()) {
                info.downloadUrl = candidateUrl;
                info.assetName = candidateName;
            }
        }

        // If no direct asset found, fallback to the release page URL
        if (info.downloadUrl.empty()) {
            info.downloadUrl = releaseHtmlUrl;
        }

        bool newer = IsNewerVersion(currentVer, info.latestVersion);
        info.available = newer;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_updateInfo = info;
            if (newer) {
                m_state = UpdateState::UpdateAvailable;
                m_statusMessage = std::format("New update available: {}!", info.latestVersion);
                LogFmt(LogLevel::Success, "⚡ [UPDATE] New version available on GitHub: {} (Current: {})",
                       info.latestVersion, currentVer);
            } else {
                m_state = UpdateState::UpToDate;
                m_statusMessage = std::format("You are running the latest version ({}).", currentVer);
                LogFmt(LogLevel::Info, "⚡ [UPDATE] eBooster is up-to-date ({})", currentVer);
            }
        }

        if (onComplete) {
            onComplete(info);
        }
    });
}

void UpdateManager::DownloadUpdateAsync(std::function<void(bool success)> onComplete) {
    if (m_state == UpdateState::Downloading) {
        return;
    }

    std::string downloadUrl;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        downloadUrl = m_updateInfo.downloadUrl;
    }

    if (downloadUrl.empty() || downloadUrl.find("http") != 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = UpdateState::Failed;
        m_statusMessage = "Invalid update download URL.";
        if (onComplete) onComplete(false);
        return;
    }

    m_state = UpdateState::Downloading;
    m_downloadProgress = 0.0f;
    m_downloadedBytes = 0;
    m_totalBytes = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_statusMessage = "Starting download from GitHub...";
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_workerThread = std::jthread([this, downloadUrl, onComplete = std::move(onComplete)](std::stop_token st) {
        std::wstring wideUrl = ToWideString(downloadUrl);

        // Crack URL into components
        URL_COMPONENTSW urlComp{};
        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.dwHostNameLength  = static_cast<DWORD>(-1);
        urlComp.dwUrlPathLength   = static_cast<DWORD>(-1);
        urlComp.dwExtraInfoLength = static_cast<DWORD>(-1);

        if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &urlComp)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Could not parse download URL.";
            if (onComplete) onComplete(false);
            return;
        }

        std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
        std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);

        ScopedHINTERNET hSession(WinHttpOpen(
            L"eBooster-Updater/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        ));
        if (!hSession) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Failed to open WinHTTP session.";
            if (onComplete) onComplete(false);
            return;
        }

        ScopedHINTERNET hConnect(WinHttpConnect(
            hSession.get(),
            hostName.c_str(),
            urlComp.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
            0
        ));
        if (!hConnect) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Failed to connect to asset download server.";
            if (onComplete) onComplete(false);
            return;
        }

        DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        ScopedHINTERNET hRequest(WinHttpOpenRequest(
            hConnect.get(),
            L"GET",
            urlPath.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags
        ));
        if (!hRequest) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Failed to create download request.";
            if (onComplete) onComplete(false);
            return;
        }

        // Enable automatic redirect following (essential for GitHub releases -> AWS S3 assets)
        DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hRequest.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

        LPCWSTR headers = L"User-Agent: eBooster-App\r\n";
        WinHttpAddRequestHeaders(hRequest.get(), headers, static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD);

        if (!WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(hRequest.get(), nullptr)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Failed to receive download response.";
            if (onComplete) onComplete(false);
            return;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (statusCode != 200) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = std::format("Download failed (HTTP {}).", statusCode);
            if (onComplete) onComplete(false);
            return;
        }

        // Query content length
        DWORD contentLength = 0;
        DWORD clSize = sizeof(contentLength);
        if (WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &clSize, WINHTTP_NO_HEADER_INDEX)) {
            m_totalBytes.store(contentLength);
        }

        auto updateDir = GetAppDataDir() / "updates";
        std::error_code ecDir;
        std::filesystem::create_directories(updateDir, ecDir);
        std::filesystem::path targetFile = updateDir / "eBooster_update.exe";
        std::ofstream outFile(targetFile, std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Cannot write update file to disk (Permission denied).";
            if (onComplete) onComplete(false);
            return;
        }

        constexpr DWORD CHUNK_SIZE = 64 * 1024; // 64 KB chunks
        std::vector<char> chunk(CHUNK_SIZE);
        uint64_t totalRead = 0;

        while (true) {
            if (st.stop_requested()) {
                outFile.close();
                std::error_code ec;
                std::filesystem::remove(targetFile, ec);
                return;
            }

            DWORD bytesAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest.get(), &bytesAvailable) || bytesAvailable == 0) {
                break;
            }

            DWORD toRead = std::min<DWORD>(bytesAvailable, CHUNK_SIZE);
            DWORD bytesRead = 0;
            if (WinHttpReadData(hRequest.get(), chunk.data(), toRead, &bytesRead) && bytesRead > 0) {
                outFile.write(chunk.data(), bytesRead);
                totalRead += bytesRead;
                m_downloadedBytes.store(totalRead);

                if (contentLength > 0) {
                    m_downloadProgress.store(static_cast<float>(totalRead) / static_cast<float>(contentLength));
                }
            } else {
                break;
            }
        }

        outFile.close();

        if (totalRead == 0) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state = UpdateState::Failed;
            m_statusMessage = "Zero bytes downloaded from server.";
            if (onComplete) onComplete(false);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_downloadedFilePath = targetFile;
            m_state = UpdateState::Downloaded;
            m_statusMessage = std::format("Download complete! ({:.2f} MB)", static_cast<double>(totalRead) / (1024.0 * 1024.0));
            LogFmt(LogLevel::Success, "⚡ [UPDATE] Downloaded update binary: {} ({:.2f} MB)",
                   targetFile.string(), static_cast<double>(totalRead) / (1024.0 * 1024.0));
        }

        if (onComplete) {
            onComplete(true);
        }
    });
}

bool UpdateManager::ApplyUpdateAndRestart() {
    wchar_t currentExe[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, currentExe, MAX_PATH);

    std::filesystem::path curPath = currentExe;
    std::filesystem::path updatePath = GetAppDataDir() / "updates" / "eBooster_update.exe";
    std::filesystem::path oldPath = curPath.parent_path() / (curPath.stem().string() + ".old.exe");

    if (!std::filesystem::exists(updatePath)) {
        Log(LogLevel::Error, "Cannot apply update: eBooster_update.exe does not exist in updates cache.");
        return false;
    }

    // Windows NT allows renaming currently running executables
    std::error_code ec;
    if (std::filesystem::exists(oldPath)) {
        std::filesystem::remove(oldPath, ec);
    }

    if (!MoveFileW(curPath.c_str(), oldPath.c_str())) {
        LogFmt(LogLevel::Error, "MoveFileW to old backup failed (Win32 Error: {})", GetLastError());
        return false;
    }

    // Copy new binary into curPath (works across drives/volumes)
    if (!CopyFileW(updatePath.c_str(), curPath.c_str(), FALSE)) {
        LogFmt(LogLevel::Error, "CopyFileW new binary failed (Win32 Error: {}). Rolling back...", GetLastError());
        MoveFileW(oldPath.c_str(), curPath.c_str());
        return false;
    }

    // Clean up cached update binary
    std::filesystem::remove(updatePath, ec);

    Log(LogLevel::Success, "⚡ Update applied cleanly! Spawning new eBooster instance and restarting...");

    // Spawn new version
    HINSTANCE hInst = ShellExecuteW(nullptr, L"open", curPath.c_str(), nullptr, nullptr, SW_SHOW);
    if (reinterpret_cast<INT_PTR>(hInst) <= 32) {
        LogFmt(LogLevel::Error, "Failed to launch updated binary (ShellExecute error: {})", reinterpret_cast<INT_PTR>(hInst));
        return false;
    }

    // Terminate current process cleanly
    ExitProcess(0);
    return true;
}

void UpdateManager::CleanupOldExecutable() {
    wchar_t currentExe[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, currentExe, MAX_PATH);
    std::filesystem::path curPath = currentExe;
    std::filesystem::path oldFile = curPath.parent_path() / (curPath.stem().string() + ".old.exe");
    std::error_code ec;
    if (std::filesystem::exists(oldFile, ec)) {
        std::filesystem::remove(oldFile, ec);
    }
}

} // namespace ebooster
