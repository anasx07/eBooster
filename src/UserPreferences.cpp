#include "ebooster/UserPreferences.hpp"
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

bool ExtractStringValue(const std::string& json, const std::string& key, std::string& outVal) {
    std::string needle = "\"" + key + "\"";
    size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) return false;

    size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) return false;

    size_t startQuote = json.find('"', colonPos + 1);
    if (startQuote == std::string::npos) return false;

    size_t endQuote = json.find('"', startQuote + 1);
    if (endQuote == std::string::npos) return false;

    outVal = json.substr(startQuote + 1, endQuote - startQuote - 1);
    return true;
}

bool ExtractBoolValue(const std::string& json, const std::string& key, bool& outVal) {
    std::string needle = "\"" + key + "\"";
    size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) return false;

    size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) return false;

    size_t valPos = SkipWs(json, colonPos + 1);
    if (valPos >= json.size()) return false;

    if (json.compare(valPos, 4, "true") == 0) {
        outVal = true;
        return true;
    }
    if (json.compare(valPos, 5, "false") == 0) {
        outVal = false;
        return true;
    }
    return false;
}

bool ExtractIntValue(const std::string& json, const std::string& key, int& outVal) {
    std::string needle = "\"" + key + "\"";
    size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos) return false;

    size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos) return false;

    size_t valPos = SkipWs(json, colonPos + 1);
    if (valPos >= json.size()) return false;

    try {
        size_t endPos = valPos;
        while (endPos < json.size() && (std::isdigit(static_cast<unsigned char>(json[endPos])) || json[endPos] == '-')) {
            ++endPos;
        }
        if (endPos > valPos) {
            outVal = std::stoi(json.substr(valPos, endPos - valPos));
            return true;
        }
    } catch (...) {}
    return false;
}

bool ExtractUintValue(const std::string& json, const std::string& key, uint32_t& outVal) {
    int v = 0;
    if (ExtractIntValue(json, key, v) && v >= 0) {
        outVal = static_cast<uint32_t>(v);
        return true;
    }
    return false;
}

} // anonymous namespace

UserPreferences PreferencesManager::Load(const std::filesystem::path& path) {
    std::filesystem::path actualPath = path.empty() ? ResolveConfigPath("configs/user_preferences.json") : path;
    UserPreferences prefs{};
    if (!std::filesystem::exists(actualPath)) {
        return prefs;
    }

    try {
        std::ifstream file(actualPath);
        if (!file.is_open()) return prefs;

        std::string json((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        file.close();

        ExtractIntValue(json, "windowX", prefs.windowX);
        ExtractIntValue(json, "windowY", prefs.windowY);
        ExtractIntValue(json, "windowWidth", prefs.windowWidth);
        ExtractIntValue(json, "windowHeight", prefs.windowHeight);
        ExtractBoolValue(json, "windowMaximized", prefs.windowMaximized);

        ExtractStringValue(json, "selectedProfileName", prefs.selectedProfileName);
        ExtractStringValue(json, "selectedServerId", prefs.selectedServerId);

        ExtractBoolValue(json, "autoBoostOnLaunch", prefs.autoBoostOnLaunch);
        ExtractBoolValue(json, "autoStopOnExit", prefs.autoStopOnExit);
        ExtractBoolValue(json, "globalHotkeyEnabled", prefs.globalHotkeyEnabled);
        ExtractBoolValue(json, "closeToTray", prefs.closeToTray);
        ExtractBoolValue(json, "minimizeToTray", prefs.minimizeToTray);
        ExtractBoolValue(json, "startWithWindows", prefs.startWithWindows);
        ExtractBoolValue(json, "startMinimizedToTray", prefs.startMinimizedToTray);

        ExtractIntValue(json, "tunnelMode", prefs.tunnelMode);
        ExtractBoolValue(json, "enableKillSwitch", prefs.enableKillSwitch);
        ExtractBoolValue(json, "blockIpv6Leaks", prefs.blockIpv6Leaks);
        ExtractBoolValue(json, "enableSmartDns", prefs.enableSmartDns);
        ExtractUintValue(json, "processPollMs", prefs.processPollMs);
        ExtractUintValue(json, "snifferPollMs", prefs.snifferPollMs);
        ExtractStringValue(json, "ispBenchmarkIp", prefs.ispBenchmarkIp);

        ExtractIntValue(json, "selectedKonamiRegion", prefs.selectedKonamiRegion);
        ExtractBoolValue(json, "efootballGeofilterActive", prefs.efootballGeofilterActive);
        ExtractBoolValue(json, "efootballAutoLock", prefs.efootballAutoLock);
        ExtractBoolValue(json, "efootballAntiLagShield", prefs.efootballAntiLagShield);

        ExtractBoolValue(json, "overlayEnabled", prefs.overlayEnabled);
        ExtractBoolValue(json, "overlayClickThrough", prefs.overlayClickThrough);
        ExtractIntValue(json, "overlayPosition", prefs.overlayPosition);

        ExtractBoolValue(json, "autoCheckUpdates", prefs.autoCheckUpdates);
        ExtractStringValue(json, "githubRepo", prefs.githubRepo);

    } catch (...) {}

    return prefs;
}

bool PreferencesManager::Save(const std::filesystem::path& path, const UserPreferences& prefs) {
    std::filesystem::path actualPath = path.empty() ? ResolveConfigPath("configs/user_preferences.json") : path;
    try {
        if (actualPath.has_parent_path()) {
            std::filesystem::create_directories(actualPath.parent_path());
        }

        std::filesystem::path tmpPath = actualPath;
        tmpPath += ".tmp";

        std::ofstream file(tmpPath, std::ios::trunc | std::ios::binary);
        if (!file.is_open()) return false;

        file << "{\n";
        file << "  \"windowX\": " << prefs.windowX << ",\n";
        file << "  \"windowY\": " << prefs.windowY << ",\n";
        file << "  \"windowWidth\": " << prefs.windowWidth << ",\n";
        file << "  \"windowHeight\": " << prefs.windowHeight << ",\n";
        file << "  \"windowMaximized\": " << (prefs.windowMaximized ? "true" : "false") << ",\n";
        file << "  \"selectedProfileName\": " << EscapeJsonString(prefs.selectedProfileName) << ",\n";
        file << "  \"selectedServerId\": " << EscapeJsonString(prefs.selectedServerId) << ",\n";
        file << "  \"autoBoostOnLaunch\": " << (prefs.autoBoostOnLaunch ? "true" : "false") << ",\n";
        file << "  \"autoStopOnExit\": " << (prefs.autoStopOnExit ? "true" : "false") << ",\n";
        file << "  \"globalHotkeyEnabled\": " << (prefs.globalHotkeyEnabled ? "true" : "false") << ",\n";
        file << "  \"closeToTray\": " << (prefs.closeToTray ? "true" : "false") << ",\n";
        file << "  \"minimizeToTray\": " << (prefs.minimizeToTray ? "true" : "false") << ",\n";
        file << "  \"startWithWindows\": " << (prefs.startWithWindows ? "true" : "false") << ",\n";
        file << "  \"startMinimizedToTray\": " << (prefs.startMinimizedToTray ? "true" : "false") << ",\n";
        file << "  \"tunnelMode\": " << prefs.tunnelMode << ",\n";
        file << "  \"enableKillSwitch\": " << (prefs.enableKillSwitch ? "true" : "false") << ",\n";
        file << "  \"blockIpv6Leaks\": " << (prefs.blockIpv6Leaks ? "true" : "false") << ",\n";
        file << "  \"enableSmartDns\": " << (prefs.enableSmartDns ? "true" : "false") << ",\n";
        file << "  \"processPollMs\": " << prefs.processPollMs << ",\n";
        file << "  \"snifferPollMs\": " << prefs.snifferPollMs << ",\n";
        file << "  \"ispBenchmarkIp\": " << EscapeJsonString(prefs.ispBenchmarkIp) << ",\n";
        file << "  \"selectedKonamiRegion\": " << prefs.selectedKonamiRegion << ",\n";
        file << "  \"efootballGeofilterActive\": " << (prefs.efootballGeofilterActive ? "true" : "false") << ",\n";
        file << "  \"efootballAutoLock\": " << (prefs.efootballAutoLock ? "true" : "false") << ",\n";
        file << "  \"efootballAntiLagShield\": " << (prefs.efootballAntiLagShield ? "true" : "false") << ",\n";
        file << "  \"overlayEnabled\": " << (prefs.overlayEnabled ? "true" : "false") << ",\n";
        file << "  \"overlayClickThrough\": " << (prefs.overlayClickThrough ? "true" : "false") << ",\n";
        file << "  \"overlayPosition\": " << prefs.overlayPosition << ",\n";
        file << "  \"autoCheckUpdates\": " << (prefs.autoCheckUpdates ? "true" : "false") << ",\n";
        file << "  \"githubRepo\": " << EscapeJsonString(prefs.githubRepo) << "\n";
        file << "}\n";
        file.close();

        std::error_code ec;
        std::filesystem::rename(tmpPath, actualPath, ec);
        if (ec) {
            std::filesystem::copy_file(tmpPath, actualPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmpPath, ec);
        }

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace ebooster
