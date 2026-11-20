#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace ebooster {

struct GameProfile {
    std::string name;
    std::string exeName;
    std::vector<std::string> childExes;
    std::vector<std::string> launcherExes;
    std::string defaultPortHint;
    std::vector<std::string> clusterCidrs;
    bool isCustom{false};
    uint32_t steamAppId{0};
    std::string bannerPath{};
    std::string logoPath{};
};

class ProfileManager {
public:
    // Returns default hardcoded profile list
    static std::vector<GameProfile> GetDefaultProfiles();

    // Loads profiles from configs/profiles.json. If file missing, creates it with defaults.
    static std::vector<GameProfile> LoadProfiles(const std::filesystem::path& configPath = "configs/profiles.json");

    // Saves profile list to JSON file with clean indentation
    static bool SaveProfiles(const std::filesystem::path& configPath, const std::vector<GameProfile>& profiles);
};

} // namespace ebooster
