#pragma once

#include "ebooster/ProfileManager.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace ebooster {

struct DetectedSteamGame {
    uint32_t appId{0};
    std::string name;
    std::string installDir;
    std::string primaryExe;
    std::vector<std::string> childExes;
    std::string launcherExe{"steam.exe"};
    std::string localBannerPath;
    std::string localLogoPath;
};

class SteamDetector {
public:
    // Finds the main Steam installation path from Windows Registry (HKCU\Software\Valve\Steam)
    static std::filesystem::path GetSteamInstallPath();

    // Locates all Steam library folders from libraryfolders.vdf (C:\, D:\SteamLibrary, etc.)
    static std::vector<std::filesystem::path> GetLibraryFolders(const std::filesystem::path& steamPath);

    // Scans all libraries and discovers all installed games
    static std::vector<DetectedSteamGame> ScanInstalledGames();

    // Finds best local banner and logo in Steam's appcache/librarycache or downloads from Steam CDN
    static void ResolveGameArt(DetectedSteamGame& game, const std::filesystem::path& steamPath);

    // Downloads official Steam CDN banner if missing locally
    static std::string DownloadCdnBanner(uint32_t appId);

    // Converts detected Steam games into GameProfile objects
    static std::vector<GameProfile> ConvertToProfiles(const std::vector<DetectedSteamGame>& games);
};

} // namespace ebooster
