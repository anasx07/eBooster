#include "ebooster/Common.hpp"
#include <sstream>
#include <iomanip>
#include <array>
#include <mutex>

#include <atomic>

namespace ebooster {

struct LogSinkEntry {
    uint64_t id;
    LogSink sink;
};

static std::mutex g_logMutex;
static std::vector<LogSinkEntry> g_logSinks;
static std::atomic<uint64_t> g_nextSinkId{1};

uint64_t RegisterLogSink(LogSink sink) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    uint64_t id = g_nextSinkId.fetch_add(1, std::memory_order_relaxed);
    g_logSinks.push_back({id, std::move(sink)});
    return id;
}

void UnregisterLogSink(uint64_t id) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::erase_if(g_logSinks, [id](const LogSinkEntry& entry) {
        return entry.id == id;
    });
}

void Log(LogLevel level, std::string_view msg) {
    // Get current local time
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::tm tm_buf{};
    localtime_s(&tm_buf, &time_t_now);

    std::string timeStr = std::format("{:02d}:{:02d}:{:02d}", tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);

    const char* prefix = "[INFO]";
    const char* colorCode = "\033[0m"; // Default

    switch (level) {
        case LogLevel::Info:
            prefix = "[INFO]";
            colorCode = "\033[36m"; // Cyan
            break;
        case LogLevel::Success:
            prefix = "[OK]  ";
            colorCode = "\033[32m"; // Green
            break;
        case LogLevel::Process:
            prefix = "[PROC]";
            colorCode = "\033[35m"; // Magenta
            break;
        case LogLevel::Net:
            prefix = "[NET] ";
            colorCode = "\033[34m"; // Blue
            break;
        case LogLevel::Route:
            prefix = "[ROUTE]";
            colorCode = "\033[33m"; // Yellow
            break;
        case LogLevel::Warn:
            prefix = "[WARN]";
            colorCode = "\033[93m"; // Bright Yellow
            break;
        case LogLevel::Error:
            prefix = "[FAIL]";
            colorCode = "\033[91m"; // Bright Red
            break;
    }

    std::cout << std::format("[{:02d}:{:02d}:{:02d}.{:03d}] {}{}\033[0m {}\n",
        tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, static_cast<int>(ms.count()),
        colorCode, prefix, msg);

    // Snapshot sinks under lock to avoid holding mutex during callbacks (deadlock-free)
    std::vector<LogSink> sinksSnapshot;
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        sinksSnapshot.reserve(g_logSinks.size());
        for (const auto& entry : g_logSinks) {
            sinksSnapshot.push_back(entry.sink);
        }
    }

    // Broadcast to UI log sinks OUTSIDE lock
    LogMessage entry{level, std::move(timeStr), prefix, std::string(msg)};
    for (const auto& sink : sinksSnapshot) {
        try {
            sink(entry);
        } catch (...) {}
    }
}

std::string Ipv4ToString(DWORD ipNetworkOrder) {
    IN_ADDR inAddr;
    inAddr.s_addr = ipNetworkOrder;
    return InAddrToString(inAddr);
}

std::string InAddrToString(const IN_ADDR& addr) {
    char buffer[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, const_cast<IN_ADDR*>(&addr), buffer, sizeof(buffer)) != nullptr) {
        return std::string(buffer);
    }
    return "0.0.0.0";
}

bool StringToInAddr(const std::string& ipStr, IN_ADDR& outAddr) {
    return inet_pton(AF_INET, ipStr.c_str(), &outAddr) == 1;
}

bool IsPrivateOrLoopbackIp(const std::string& ipStr) {
    IN_ADDR inAddr{};
    if (!StringToInAddr(ipStr, inAddr)) {
        return true; // Invalid format treated as unroutable
    }

    uint32_t hostIp = ntohl(inAddr.s_addr);
    uint8_t b1 = static_cast<uint8_t>((hostIp >> 24) & 0xFF);
    uint8_t b2 = static_cast<uint8_t>((hostIp >> 16) & 0xFF);

    // 0.0.0.0/8 (Broadcast/Current network)
    if (b1 == 0) return true;

    // 127.0.0.0/8 (Loopback)
    if (b1 == 127) return true;

    // 10.0.0.0/8 (Private class A - includes 10.66.66.0/24 WireGuard internal net)
    if (b1 == 10) return true;

    // 172.16.0.0/12 (Private class B: 172.16.0.0 - 172.31.255.255)
    if (b1 == 172 && (b2 >= 16 && b2 <= 31)) return true;

    // 192.168.0.0/16 (Private class C)
    if (b1 == 192 && b2 == 168) return true;

    // 169.254.0.0/16 (Link Local / APIPA)
    if (b1 == 169 && b2 == 254) return true;

    // 224.0.0.0 to 239.255.255.255 (Multicast)
    if (b1 >= 224 && b1 <= 239) return true;

    // 255.255.255.255 (Limited Broadcast)
    if (hostIp == 0xFFFFFFFF) return true;

    return false;
}

bool IsProcessElevated() {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return false;
    }
    UniqueHandle tokenHandle = MakeUniqueHandle(hToken);

    TOKEN_ELEVATION elevation{};
    DWORD dwSize = sizeof(TOKEN_ELEVATION);
    if (GetTokenInformation(tokenHandle.get(), TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
        return elevation.TokenIsElevated != 0;
    }
    return false;
}

std::wstring ToWideString(std::string_view str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded);
    return result;
}

std::string ToNarrowString(std::wstring_view wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    std::string result(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), sizeNeeded, nullptr, nullptr);
    return result;
}

std::string EscapeJsonString(std::string_view str) {
    std::string out = "\"";
    for (char c : str) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    out += "\"";
    return out;
}

std::filesystem::path GetExecutableDir() {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len > 0) {
        return std::filesystem::path(buffer).parent_path();
    }
    return std::filesystem::current_path();
}

bool IsPortableMode() {
    static bool checked = false;
    static bool portable = false;
    if (!checked) {
        auto exeDir = GetExecutableDir();
        // Activate portable mode only if explicit "portable.txt" marker is placed alongside exe
        if (std::filesystem::exists(exeDir / "portable.txt")) {
            portable = true;
        }
        checked = true;
    }
    return portable;
}

std::filesystem::path GetAppDataDir() {
    if (IsPortableMode()) {
        return GetExecutableDir();
    }

    static std::filesystem::path s_appDataDir;
    if (!s_appDataDir.empty()) {
        return s_appDataDir;
    }

    PWSTR path = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        s_appDataDir = std::filesystem::path(path) / "eBooster";
        CoTaskMemFree(path);
    } else {
        wchar_t envBuf[MAX_PATH];
        DWORD envLen = GetEnvironmentVariableW(L"LOCALAPPDATA", envBuf, MAX_PATH);
        if (envLen > 0 && envLen < MAX_PATH) {
            s_appDataDir = std::filesystem::path(envBuf) / "eBooster";
        } else {
            s_appDataDir = GetExecutableDir();
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(s_appDataDir, ec);
    return s_appDataDir;
}

std::filesystem::path GetAppCacheDir() {
    auto cacheDir = GetAppDataDir() / "cache";
    std::error_code ec;
    std::filesystem::create_directories(cacheDir, ec);
    return cacheDir;
}

std::filesystem::path ResolveConfigPath(const std::filesystem::path& relPath) {
    if (IsPortableMode()) {
        auto p = GetExecutableDir() / relPath;
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        return p;
    }

    auto appData = GetAppDataDir();
    auto dest = appData / relPath;

    // If configuration doesn't exist in AppData yet, check for shipped seed defaults
    if (!std::filesystem::exists(dest)) {
        std::error_code ec;
        auto seed = GetExecutableDir() / relPath;
        if (std::filesystem::exists(seed)) {
            std::filesystem::create_directories(dest.parent_path(), ec);
            std::filesystem::copy_file(seed, dest, std::filesystem::copy_options::overwrite_existing, ec);
        } else {
            auto devSeed = GetExecutableDir().parent_path().parent_path() / relPath;
            if (std::filesystem::exists(devSeed)) {
                std::filesystem::create_directories(dest.parent_path(), ec);
                std::filesystem::copy_file(devSeed, dest, std::filesystem::copy_options::overwrite_existing, ec);
            }
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(dest.parent_path(), ec);
    return dest;
}

bool OpenFolderInExplorer(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::filesystem::create_directories(path, ec);
    }
    HINSTANCE hInst = ShellExecuteW(nullptr, L"explore", path.wstring().c_str(), nullptr, nullptr, SW_SHOW);
    if (reinterpret_cast<INT_PTR>(hInst) <= 32) {
        hInst = ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOW);
    }
    return reinterpret_cast<INT_PTR>(hInst) > 32;
}

std::optional<uint32_t> PingHost(const std::string& ipAddress, uint32_t timeoutMs) {
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) return std::nullopt;

    IN_ADDR inAddr{};
    if (inet_pton(AF_INET, ipAddress.c_str(), &inAddr) != 1) {
        IcmpCloseHandle(hIcmp);
        return std::nullopt;
    }

    char sendData[32] = "eBooster Low-Latency Probe";
    const DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
    std::vector<uint8_t> replyBuffer(replySize, 0);

    DWORD replies = IcmpSendEcho(hIcmp, inAddr.s_addr, sendData, static_cast<WORD>(sizeof(sendData)),
                                 nullptr, replyBuffer.data(), replySize, timeoutMs);
    IcmpCloseHandle(hIcmp);

    if (replies > 0) {
        auto* pEcho = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuffer.data());
        if (pEcho->Status == IP_SUCCESS) {
            return pEcho->RoundTripTime;
        }
    }
    return std::nullopt;
}

} // namespace ebooster
