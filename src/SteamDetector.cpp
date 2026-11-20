#include "ebooster/SteamDetector.hpp"
#include "ebooster/Common.hpp"

#include <windows.h>
#include <urlmon.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace ebooster {

namespace {

[[maybe_unused]] std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n\"");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n\"");
    return str.substr(first, (last - first + 1));
}

bool IsIgnoredExe(const std::string& filename) {
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    const std::vector<std::string> blacklist = {
        "unins", "crash", "report", "redist", "setup", "install",
        "update", "downloader", "launcher", "helper", "diagnostics",
        "benchmark", "tool", "config", "patch", "cefprocess",
        "wallpaper", "soundpad", "obs", "sharex", "discord", "spotify",
        "resourcecompiler", "screensaver"
    };

    for (const auto& term : blacklist) {
        if (lower.find(term) != std::string::npos) return true;
    }
    return false;
}

} // anonymous namespace

std::filesystem::path SteamDetector::GetSteamInstallPath() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t pathBuf[MAX_PATH];
        DWORD bufSize = sizeof(pathBuf);
        DWORD type = REG_SZ;
        if (RegQueryValueExW(hKey, L"SteamPath", nullptr, &type, reinterpret_cast<LPBYTE>(pathBuf), &bufSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::filesystem::path(pathBuf);
        }
        RegCloseKey(hKey);
    }
    return "";
}

std::vector<std::filesystem::path> SteamDetector::GetLibraryFolders(const std::filesystem::path& steamPath) {
    std::vector<std::filesystem::path> libraries;
    if (steamPath.empty() || !std::filesystem::exists(steamPath)) {
        return libraries;
    }

    libraries.push_back(steamPath);

    std::filesystem::path vdfPath = steamPath / "steamapps" / "libraryfolders.vdf";
    if (!std::filesystem::exists(vdfPath)) {
        return libraries;
    }

    std::ifstream file(vdfPath);
    if (!file.is_open()) return libraries;

    std::string line;
    while (std::getline(file, line)) {
        size_t pathKey = line.find("\"path\"");
        if (pathKey != std::string::npos) {
            size_t start = line.find('"', pathKey + 6);
            if (start != std::string::npos) {
                size_t end = line.find('"', start + 1);
                if (end != std::string::npos) {
                    std::string pStr = line.substr(start + 1, end - start - 1);
                    // Replace double backslashes
                    std::string cleanPath;
                    for (size_t i = 0; i < pStr.size(); ++i) {
                        if (pStr[i] == '\\' && i + 1 < pStr.size() && pStr[i + 1] == '\\') {
                            continue;
                        }
                        cleanPath += pStr[i];
                    }
                    std::filesystem::path libPath(cleanPath);
                    if (std::filesystem::exists(libPath)) {
                        bool found = false;
                        for (const auto& existing : libraries) {
                            if (std::filesystem::equivalent(existing, libPath)) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            libraries.push_back(libPath);
                        }
                    }
                }
            }
        }
    }
    return libraries;
}

std::vector<DetectedSteamGame> SteamDetector::ScanInstalledGames() {
    std::vector<DetectedSteamGame> games;
    auto steamPath = GetSteamInstallPath();
    if (steamPath.empty()) {
        Log(LogLevel::Warn, "Steam installation path not found in Windows Registry.");
        return games;
    }

    auto libraries = GetLibraryFolders(steamPath);
    LogFmt(LogLevel::Info, "Discovered {} Steam library folder(s) to scan.", libraries.size());

    for (const auto& lib : libraries) {
        std::filesystem::path steamapps = lib / "steamapps";
        if (!std::filesystem::exists(steamapps)) continue;

        try {
            for (const auto& entry : std::filesystem::directory_iterator(steamapps)) {
                if (!entry.is_regular_file()) continue;
                std::string filename = entry.path().filename().string();
                if (!filename.starts_with("appmanifest_") || !filename.ends_with(".acf")) continue;

                // Parse AppID from filename
                std::string idStr = filename.substr(12, filename.size() - 16);
                uint32_t appId = 0;
                try {
                    appId = static_cast<uint32_t>(std::stoul(idStr));
                } catch (...) {
                    continue;
                }

                // Filter out non-game software, tools, live wallpapers, utilities, and runtimes
                if (appId == 431960 || // Wallpaper Engine
                    appId == 388080 || // Soundpad
                    appId == 1494460 || // OBS Studio
                    appId == 282440 || // ShareX
                    appId == 431730 || // Aseprite
                    appId == 365670 || // Blender
                    appId == 250820 || // SteamVR
                    appId == 228980 || // Common Redistributables
                    appId == 896660 || // Proton
                    appId == 221410 || // Rocksmith Audio Tool
                    appId == 1096900   // RPG Maker MZ
                ) continue;

                std::ifstream acfFile(entry.path());
                if (!acfFile.is_open()) continue;

                std::string line;
                std::string name;
                std::string installDir;

                while (std::getline(acfFile, line)) {
                    if (line.find("\"name\"") != std::string::npos) {
                        size_t first = line.find('"', line.find("\"name\"") + 6);
                        if (first != std::string::npos) {
                            size_t second = line.find('"', first + 1);
                            if (second != std::string::npos) {
                                name = line.substr(first + 1, second - first - 1);
                            }
                        }
                    } else if (line.find("\"installdir\"") != std::string::npos) {
                        size_t first = line.find('"', line.find("\"installdir\"") + 12);
                        if (first != std::string::npos) {
                            size_t second = line.find('"', first + 1);
                            if (second != std::string::npos) {
                                installDir = line.substr(first + 1, second - first - 1);
                            }
                        }
                    }
                }

                if (name.empty() || installDir.empty()) continue;

                // Strip trademark (™ \xe2\x84\xa2) and registered (® \xc2\xae) symbols so fonts render cleanly
                std::string sanitizedName;
                for (size_t i = 0; i < name.size(); ++i) {
                    if (static_cast<unsigned char>(name[i]) == 0xe2 && i + 2 < name.size() &&
                        static_cast<unsigned char>(name[i+1]) == 0x84 && static_cast<unsigned char>(name[i+2]) == 0xa2) {
                        i += 2;
                        continue;
                    }
                    if (static_cast<unsigned char>(name[i]) == 0xc2 && i + 1 < name.size() &&
                        static_cast<unsigned char>(name[i+1]) == 0xae) {
                        i += 1;
                        continue;
                    }
                    sanitizedName.push_back(name[i]);
                }
                name = sanitizedName;

                std::string lowerName = name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::string lowerDir = installDir;
                std::transform(lowerDir.begin(), lowerDir.end(), lowerDir.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                if (lowerName.find("wallpaper") != std::string::npos ||
                    lowerDir.find("wallpaper") != std::string::npos ||
                    lowerName.find("soundpad") != std::string::npos ||
                    lowerName.find("obs studio") != std::string::npos ||
                    lowerName.find("benchmark") != std::string::npos ||
                    lowerName.find("dedicated server") != std::string::npos ||
                    lowerName.find("soundtrack") != std::string::npos ||
                    lowerName.find("redistributable") != std::string::npos) {
                    continue;
                }

                std::filesystem::path gameRoot = steamapps / "common" / installDir;
                if (!std::filesystem::exists(gameRoot)) continue;

                DetectedSteamGame game;
                game.appId = appId;
                game.name = name;
                game.installDir = installDir;
                game.launcherExe = "steam.exe";

                // Scan game root directory for executables
                std::vector<std::pair<std::filesystem::path, uintmax_t>> foundExes;
                try {
                    for (const auto& item : std::filesystem::recursive_directory_iterator(
                             gameRoot, std::filesystem::directory_options::skip_permission_denied)) {
                        if (item.is_regular_file() && item.path().extension() == ".exe") {
                            std::string exeName = item.path().filename().string();
                            if (!IsIgnoredExe(exeName)) {
                                foundExes.emplace_back(item.path(), item.file_size());
                            }
                        }
                    }
                } catch (...) {}

                if (foundExes.empty()) continue;

                // Sort by file size descending (largest .exe is almost always the main game binary)
                std::sort(foundExes.begin(), foundExes.end(), [](const auto& a, const auto& b) {
                    return a.second > b.second;
                });

                game.primaryExe = foundExes[0].first.filename().string();
                for (size_t e = 1; e < foundExes.size() && e < 5; ++e) {
                    std::string childName = foundExes[e].first.filename().string();
                    if (childName != game.primaryExe) {
                        game.childExes.push_back(childName);
                    }
                }

                // Resolve game artwork (Local appcache or CDN)
                ResolveGameArt(game, steamPath);

                games.push_back(std::move(game));
            }
        } catch (...) {}
    }

    LogFmt(LogLevel::Success, "Steam Game Detection complete: found {} installed game(s).", games.size());
    return games;
}

void SteamDetector::ResolveGameArt(DetectedSteamGame& game, const std::filesystem::path& steamPath) {
    if (steamPath.empty()) return;

    // Check Steam's local appcache/librarycache/<appId>
    std::filesystem::path appcache = steamPath / "appcache" / "librarycache" / std::to_string(game.appId);
    if (std::filesystem::exists(appcache)) {
        try {
            // Check for logo.png
            std::filesystem::path logoCandidate = appcache / "logo.png";
            if (std::filesystem::exists(logoCandidate)) {
                game.localLogoPath = logoCandidate.string();
            }

            // Check for library_header.jpg, library_capsule.jpg, library_hero.jpg
            for (const auto& item : std::filesystem::recursive_directory_iterator(appcache)) {
                if (item.is_regular_file()) {
                    std::string fname = item.path().filename().string();
                    if (fname == "library_header.jpg" || fname == "header.jpg") {
                        game.localBannerPath = item.path().string();
                        break;
                    }
                    if (fname == "library_hero.jpg" && game.localBannerPath.empty()) {
                        game.localBannerPath = item.path().string();
                    }
                    if (fname == "logo.png" && game.localLogoPath.empty()) {
                        game.localLogoPath = item.path().string();
                    }
                }
            }
        } catch (...) {}
    }

    // If local banner not found, download official CDN banner into local cache
    if (game.localBannerPath.empty() && game.appId > 0) {
        game.localBannerPath = DownloadCdnBanner(game.appId);
    }
}

std::string SteamDetector::DownloadCdnBanner(uint32_t appId) {
    if (appId == 0) return "";

    std::filesystem::path cacheDir = GetAppCacheDir() / "steam" / std::to_string(appId);
    try {
        std::filesystem::create_directories(cacheDir);
    } catch (...) {
        return "";
    }

    std::filesystem::path outBanner = cacheDir / "header.jpg";
    if (std::filesystem::exists(outBanner) && std::filesystem::file_size(outBanner) > 1000) {
        return outBanner.string();
    }

    std::string url = "https://cdn.akamai.steamstatic.com/steam/apps/" + std::to_string(appId) + "/header.jpg";
    std::wstring wUrl(url.begin(), url.end());
    std::wstring wOut = outBanner.wstring();

    HRESULT hr = URLDownloadToFileW(nullptr, wUrl.c_str(), wOut.c_str(), 0, nullptr);
    if (SUCCEEDED(hr) && std::filesystem::exists(outBanner) && std::filesystem::file_size(outBanner) > 1000) {
        return outBanner.string();
    }

    return "";
}

std::vector<GameProfile> SteamDetector::ConvertToProfiles(const std::vector<DetectedSteamGame>& games) {
    std::vector<GameProfile> profiles;
    profiles.reserve(games.size());

    for (const auto& g : games) {
        GameProfile p;
        p.name = g.name + " [Steam]";
        p.exeName = g.primaryExe;
        p.childExes = g.childExes;
        p.launcherExes = { g.launcherExe, "steamwebhelper.exe" };
        p.defaultPortHint = "Dynamic";
        p.isCustom = false;
        p.steamAppId = g.appId;
        p.bannerPath = g.localBannerPath;
        p.logoPath = g.localLogoPath;
        profiles.push_back(std::move(p));
    }

    return profiles;
}

} // namespace ebooster
