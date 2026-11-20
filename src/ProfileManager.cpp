#include "ebooster/ProfileManager.hpp"
#include "ebooster/Common.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace ebooster {

namespace {

size_t SkipWs(const std::string& str, size_t pos) {
    while (pos < str.size() && (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\r' || str[pos] == '\n')) {
        ++pos;
    }
    return pos;
}

bool ParseString(const std::string& str, size_t& pos, std::string& val) {
    pos = SkipWs(str, pos);
    if (pos >= str.size() || str[pos] != '"') return false;
    ++pos; // skip opening quote

    val.clear();
    while (pos < str.size()) {
        char c = str[pos++];
        if (c == '"') return true;
        if (c == '\\' && pos < str.size()) {
            char esc = str[pos++];
            switch (esc) {
                case '"':  val += '"'; break;
                case '\\': val += '\\'; break;
                case '/':  val += '/'; break;
                case 'b':  val += '\b'; break;
                case 'f':  val += '\f'; break;
                case 'n':  val += '\n'; break;
                case 'r':  val += '\r'; break;
                case 't':  val += '\t'; break;
                default:   val += esc; break;
            }
        } else {
            val += c;
        }
    }
    return false;
}

bool ParseStringArray(const std::string& str, size_t& pos, std::vector<std::string>& arr) {
    pos = SkipWs(str, pos);
    if (pos >= str.size() || str[pos] != '[') return false;
    ++pos; // skip '['

    arr.clear();
    while (pos < str.size()) {
        pos = SkipWs(str, pos);
        if (pos < str.size() && str[pos] == ']') {
            ++pos;
            return true;
        }

        std::string elem;
        if (!ParseString(str, pos, elem)) break;
        arr.push_back(std::move(elem));

        pos = SkipWs(str, pos);
        if (pos < str.size() && str[pos] == ',') {
            ++pos;
        }
    }
    return false;
}

bool ParseProfileObject(const std::string& str, size_t& pos, GameProfile& prof) {
    pos = SkipWs(str, pos);
    if (pos >= str.size() || str[pos] != '{') return false;
    ++pos; // skip '{'

    while (pos < str.size()) {
        pos = SkipWs(str, pos);
        if (pos < str.size() && str[pos] == '}') {
            ++pos;
            return true;
        }

        std::string key;
        if (!ParseString(str, pos, key)) break;

        pos = SkipWs(str, pos);
        if (pos >= str.size() || str[pos] != ':') break;
        ++pos; // skip ':'

        pos = SkipWs(str, pos);
        if (key == "name") {
            ParseString(str, pos, prof.name);
        } else if (key == "exeName") {
            ParseString(str, pos, prof.exeName);
        } else if (key == "childExes") {
            ParseStringArray(str, pos, prof.childExes);
        } else if (key == "launcherExes") {
            ParseStringArray(str, pos, prof.launcherExes);
        } else if (key == "defaultPortHint") {
            ParseString(str, pos, prof.defaultPortHint);
        } else if (key == "clusterCidrs") {
            ParseStringArray(str, pos, prof.clusterCidrs);
        } else if (key == "isCustom") {
            std::string boolStr;
            while (pos < str.size() && std::isalpha(static_cast<unsigned char>(str[pos]))) {
                boolStr += str[pos++];
            }
            prof.isCustom = (boolStr == "true" || boolStr == "1");
        } else if (key == "steamAppId") {
            pos = SkipWs(str, pos);
            uint32_t val = 0;
            while (pos < str.size() && std::isdigit(static_cast<unsigned char>(str[pos]))) {
                val = val * 10 + (str[pos++] - '0');
            }
            prof.steamAppId = val;
        } else if (key == "bannerPath") {
            ParseString(str, pos, prof.bannerPath);
        } else if (key == "logoPath") {
            ParseString(str, pos, prof.logoPath);
        } else {
            // Skip unrecognized value
            int depth = 0;
            bool inStr = false;
            while (pos < str.size()) {
                char c = str[pos];
                if (c == '"' && (pos == 0 || str[pos - 1] != '\\')) {
                    inStr = !inStr;
                } else if (!inStr) {
                    if (c == '{' || c == '[') ++depth;
                    else if (c == '}' || c == ']') {
                        if (depth == 0) break;
                        --depth;
                    } else if (c == ',' && depth == 0) {
                        break;
                    }
                }
                ++pos;
            }
        }

        pos = SkipWs(str, pos);
        if (pos < str.size() && str[pos] == ',') {
            ++pos;
        }
    }
    return false;
}

std::vector<GameProfile> ParseProfilesJson(const std::string& json) {
    std::vector<GameProfile> profiles;
    size_t pos = SkipWs(json, 0);
    if (pos >= json.size() || json[pos] != '[') return profiles;
    ++pos; // skip '['

    while (pos < json.size()) {
        pos = SkipWs(json, pos);
        if (pos < json.size() && json[pos] == ']') {
            break;
        }

        GameProfile p;
        if (ParseProfileObject(json, pos, p)) {
            if (!p.name.empty() && !p.exeName.empty()) {
                profiles.push_back(std::move(p));
            }
        } else {
            break;
        }

        pos = SkipWs(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
        }
    }
    return profiles;
}

} // anonymous namespace

std::vector<GameProfile> ProfileManager::GetDefaultProfiles() {
    return {
        {
            .name = "eFootball [Steam]",
            .exeName = "eFootball.exe",
            .childExes = {"Settings.exe", "efootballonline.exe"},
            .launcherExes = {"steam.exe", "steamwebhelper.exe"},
            .defaultPortHint = "5735, 5521, 50000",
            .clusterCidrs = {
                "34.159.0.0/16", "35.246.0.0/16", "35.242.0.0/16", "35.198.0.0/16",
                "34.175.0.0/16", "34.155.0.0/16", "34.154.0.0/16", "34.65.0.0/16",
                "34.89.0.0/16",  "34.141.0.0/16", "34.13.0.0/16",  "34.32.0.0/16",
                "34.38.0.0/16",  "34.39.0.0/16",  "34.118.0.0/16", "166.117.0.0/16",
                "18.102.0.0/16", "15.161.0.0/16", "16.22.0.0/16"
            },
            .isCustom = false,
            .steamAppId = 1665460
        },
        {
            .name = "Counter-Strike 2",
            .exeName = "cs2.exe",
            .childExes = {},
            .launcherExes = {"steam.exe"},
            .defaultPortHint = "27015",
            .clusterCidrs = {"155.133.224.0/19", "162.254.192.0/19", "146.66.152.0/21"},
            .isCustom = false,
            .steamAppId = 730
        },
        {
            .name = "Valorant",
            .exeName = "VALORANT-Win64-Shipping.exe",
            .childExes = {"VALORANT.exe"},
            .launcherExes = {"RiotClientServices.exe", "RiotClientUx.exe"},
            .defaultPortHint = "7000-8000",
            .clusterCidrs = {"162.249.72.0/21", "104.160.128.0/19", "192.64.168.0/21"},
            .isCustom = false,
            .steamAppId = 0
        },
        {
            .name = "Apex Legends",
            .exeName = "r5apex.exe",
            .childExes = {"r5apex_dx12.exe"},
            .launcherExes = {"EADesktop.exe", "EALauncher.exe", "steam.exe"},
            .defaultPortHint = "27000-27050",
            .clusterCidrs = {"159.153.0.0/16", "52.28.0.0/14"},
            .isCustom = false,
            .steamAppId = 1172470
        },
        {
            .name = "Fortnite",
            .exeName = "FortniteClient-Win64-Shipping.exe",
            .childExes = {"FortniteLauncher.exe"},
            .launcherExes = {"EpicGamesLauncher.exe"},
            .defaultPortHint = "9000-9100",
            .clusterCidrs = {"3.120.0.0/14", "18.194.0.0/15"},
            .isCustom = false,
            .steamAppId = 0
        },
        {
            .name = "Call of Duty",
            .exeName = "cod.exe",
            .childExes = {"bootstrapper.exe"},
            .launcherExes = {"Battle.net.exe"},
            .defaultPortHint = "3074",
            .clusterCidrs = {"185.34.104.0/22", "137.221.64.0/19"},
            .isCustom = false,
            .steamAppId = 0
        },
        {
            .name = "Dota 2",
            .exeName = "dota2.exe",
            .childExes = {},
            .launcherExes = {"steam.exe"},
            .defaultPortHint = "27015",
            .clusterCidrs = {"155.133.224.0/19", "162.254.192.0/19"},
            .isCustom = false,
            .steamAppId = 570
        },
        {
            .name = "War Thunder",
            .exeName = "aces.exe",
            .childExes = {"aces-min-cpu.exe", "BEService_x64.exe"},
            .launcherExes = {"steam.exe", "launcher.exe"},
            .defaultPortHint = "Dynamic",
            .clusterCidrs = {},
            .isCustom = false,
            .steamAppId = 236390
        },
        {
            .name = "Steam Launcher (Auto-Detect)",
            .exeName = "steam.exe",
            .childExes = {"cs2.exe", "dota2.exe", "deadlock.exe", "tf_win64.exe", "rustclient.exe", "r5apex.exe"},
            .launcherExes = {"steam.exe"},
            .defaultPortHint = "Dynamic",
            .clusterCidrs = {"155.133.224.0/19", "162.254.192.0/19"},
            .isCustom = false
        },
        {
            .name = "Riot Client (Auto-Detect)",
            .exeName = "RiotClientServices.exe",
            .childExes = {"VALORANT-Win64-Shipping.exe", "VALORANT.exe", "League of Legends.exe"},
            .launcherExes = {"RiotClientServices.exe"},
            .defaultPortHint = "Dynamic",
            .clusterCidrs = {"162.249.72.0/21", "104.160.128.0/19"},
            .isCustom = false
        },
        {
            .name = "Battle.net (Auto-Detect)",
            .exeName = "Battle.net.exe",
            .childExes = {"cod.exe", "bootstrapper.exe", "Overwatch.exe"},
            .launcherExes = {"Battle.net.exe"},
            .defaultPortHint = "Dynamic",
            .clusterCidrs = {"185.34.104.0/22", "137.221.64.0/19"},
            .isCustom = false
        },
        {
            .name = "Epic Games (Auto-Detect)",
            .exeName = "EpicGamesLauncher.exe",
            .childExes = {"FortniteClient-Win64-Shipping.exe", "RocketLeague.exe"},
            .launcherExes = {"EpicGamesLauncher.exe"},
            .defaultPortHint = "Dynamic",
            .clusterCidrs = {"3.120.0.0/14"},
            .isCustom = false
        },
        {
            .name = "Custom Executable",
            .exeName = "custom",
            .childExes = {},
            .launcherExes = {},
            .defaultPortHint = "Any",
            .clusterCidrs = {},
            .isCustom = false
        }
    };
}

std::vector<GameProfile> ProfileManager::LoadProfiles(const std::filesystem::path& configPath) {
    if (std::filesystem::exists(configPath)) {
        try {
            std::ifstream file(configPath, std::ios::in | std::ios::binary);
            if (file.is_open()) {
                std::stringstream ss;
                ss << file.rdbuf();
                std::string content = ss.str();
                auto parsed = ParseProfilesJson(content);
                if (!parsed.empty()) {
                    LogFmt(LogLevel::Info, "Loaded {} game profile(s) from '{}'", parsed.size(), configPath.string());
                    return parsed;
                }

                // If file had content but parsing failed: backup corrupted file and do NOT overwrite!
                if (!content.empty() && content.find_first_not_of(" \t\r\n") != std::string::npos) {
                    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    std::filesystem::path bakPath = configPath;
                    bakPath += std::format(".corrupt.{}.bak", now);
                    std::error_code ec;
                    std::filesystem::copy_file(configPath, bakPath, std::filesystem::copy_options::overwrite_existing, ec);
                    LogFmt(LogLevel::Error, "Syntax error in profiles '{}'. Backed up to '{}'. Preserving user file.",
                           configPath.string(), bakPath.string());
                    return GetDefaultProfiles();
                }
            }
        } catch (const std::exception& e) {
            LogFmt(LogLevel::Warn, "Error reading profiles from '{}': {}. Falling back to default profiles.",
                   configPath.string(), e.what());
        }
    }

    // Only if file does NOT exist: generate defaults and save
    auto defaults = GetDefaultProfiles();
    SaveProfiles(configPath, defaults);
    return defaults;
}

bool ProfileManager::SaveProfiles(const std::filesystem::path& configPath, const std::vector<GameProfile>& profiles) {
    try {
        if (configPath.has_parent_path()) {
            std::filesystem::create_directories(configPath.parent_path());
        }

        std::string out = "[\n";
        for (size_t i = 0; i < profiles.size(); ++i) {
            const auto& p = profiles[i];
            out += "  {\n";
            out += "    \"name\": " + EscapeJsonString(p.name) + ",\n";
            out += "    \"exeName\": " + EscapeJsonString(p.exeName) + ",\n";

            out += "    \"childExes\": [";
            for (size_t c = 0; c < p.childExes.size(); ++c) {
                if (c > 0) out += ", ";
                out += EscapeJsonString(p.childExes[c]);
            }
            out += "],\n";

            out += "    \"launcherExes\": [";
            for (size_t l = 0; l < p.launcherExes.size(); ++l) {
                if (l > 0) out += ", ";
                out += EscapeJsonString(p.launcherExes[l]);
            }
            out += "],\n";

            out += "    \"defaultPortHint\": " + EscapeJsonString(p.defaultPortHint) + ",\n";

            out += "    \"clusterCidrs\": [";
            for (size_t r = 0; r < p.clusterCidrs.size(); ++r) {
                if (r > 0) out += ", ";
                out += EscapeJsonString(p.clusterCidrs[r]);
            }
            out += "],\n";

            out += "    \"isCustom\": " + std::string(p.isCustom ? "true" : "false") + ",\n";
            out += "    \"steamAppId\": " + std::to_string(p.steamAppId) + ",\n";
            out += "    \"bannerPath\": " + EscapeJsonString(p.bannerPath) + ",\n";
            out += "    \"logoPath\": " + EscapeJsonString(p.logoPath) + "\n";
            out += "  }" + std::string(i + 1 < profiles.size() ? "," : "") + "\n";
        }
        out += "]\n";

        // Atomic file write: write to .tmp then rename
        std::filesystem::path tmpPath = configPath;
        tmpPath += ".tmp";

        std::ofstream file(tmpPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (file.is_open()) {
            file.write(out.data(), out.size());
            file.close();

            std::error_code ec;
            std::filesystem::rename(tmpPath, configPath, ec);
            if (ec) {
                // If rename fails (e.g. Windows file lock), fallback to copy + remove
                std::filesystem::copy_file(tmpPath, configPath, std::filesystem::copy_options::overwrite_existing, ec);
                std::filesystem::remove(tmpPath, ec);
            }

            LogFmt(LogLevel::Info, "Saved {} game profile(s) to '{}'", profiles.size(), configPath.string());
            return true;
        }
    } catch (const std::exception& e) {
        LogFmt(LogLevel::Error, "Failed to save game profiles to '{}': {}", configPath.string(), e.what());
    }

    return false;
}

} // namespace ebooster
