#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <netioapi.h>
#include <tlhelp32.h>

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <format>
#include <iostream>
#include <chrono>
#include <functional>
#include <cstdint>
#include <filesystem>

namespace ebooster {

// RAII Deleter for Win32 Kernel Handles (Process, Thread, Event, Mutex, File)
struct WinHandleDeleter {
    void operator()(HANDLE h) const noexcept {
        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            ::CloseHandle(h);
        }
    }
};

using UniqueHandle = std::unique_ptr<void, WinHandleDeleter>;

inline UniqueHandle MakeUniqueHandle(HANDLE h) noexcept {
    return UniqueHandle(h);
}

// Service Control Manager & Service Handles
struct ScHandleDeleter {
    void operator()(SC_HANDLE h) const noexcept {
        if (h != nullptr) {
            ::CloseServiceHandle(h);
        }
    }
};
using UniqueScHandle = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, ScHandleDeleter>;

inline UniqueScHandle MakeUniqueScHandle(SC_HANDLE h) noexcept {
    return UniqueScHandle(h);
}

// ICMP Echo File Handles
struct IcmpHandleDeleter {
    void operator()(HANDLE h) const noexcept {
        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            ::IcmpCloseHandle(h);
        }
    }
};
using UniqueIcmpHandle = std::unique_ptr<void, IcmpHandleDeleter>;

inline UniqueIcmpHandle MakeUniqueIcmpHandle(HANDLE h) noexcept {
    return UniqueIcmpHandle(h);
}

// Winsock UDP/TCP Sockets
class UniqueSocket {
public:
    UniqueSocket() noexcept : m_sock(INVALID_SOCKET) {}
    explicit UniqueSocket(SOCKET s) noexcept : m_sock(s) {}
    ~UniqueSocket() noexcept { reset(); }

    UniqueSocket(const UniqueSocket&) = delete;
    UniqueSocket& operator=(const UniqueSocket&) = delete;

    UniqueSocket(UniqueSocket&& other) noexcept : m_sock(other.release()) {}
    UniqueSocket& operator=(UniqueSocket&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] SOCKET get() const noexcept { return m_sock; }
    [[nodiscard]] bool isValid() const noexcept { return m_sock != INVALID_SOCKET; }
    explicit operator bool() const noexcept { return isValid(); }

    SOCKET release() noexcept {
        SOCKET s = m_sock;
        m_sock = INVALID_SOCKET;
        return s;
    }

    void reset(SOCKET s = INVALID_SOCKET) noexcept {
        if (m_sock != INVALID_SOCKET) {
            ::closesocket(m_sock);
        }
        m_sock = s;
    }

private:
    SOCKET m_sock{INVALID_SOCKET};
};

// Registry Keys
struct RegKeyDeleter {
    void operator()(HKEY k) const noexcept {
        if (k != nullptr) {
            ::RegCloseKey(k);
        }
    }
};
using UniqueHKey = std::unique_ptr<std::remove_pointer_t<HKEY>, RegKeyDeleter>;

inline UniqueHKey MakeUniqueHKey(HKEY k) noexcept {
    return UniqueHKey(k);
}

// Log levels
enum class LogLevel {
    Info,
    Success,
    Process,
    Net,
    Route,
    Warn,
    Error
};

struct LogMessage {
    LogLevel level;
    std::string timestamp;
    std::string prefix;
    std::string message;
};

using LogSink = std::function<void(const LogMessage&)>;
uint64_t RegisterLogSink(LogSink sink);
void UnregisterLogSink(uint64_t id);
void Log(LogLevel level, std::string_view msg);

template <typename... Args>
inline void LogFmt(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    try {
        std::string message = std::format(fmt, std::forward<Args>(args)...);
        Log(level, message);
    } catch (const std::exception& e) {
        Log(LogLevel::Error, std::format("Logging format failure: {}", e.what()));
    }
}

// IP conversion & validation utilities
std::string Ipv4ToString(DWORD ipNetworkOrder);
std::string InAddrToString(const IN_ADDR& addr);
bool StringToInAddr(const std::string& ipStr, IN_ADDR& outAddr);
bool IsPrivateOrLoopbackIp(const std::string& ipStr);
bool IsProcessElevated();

// String helpers
std::wstring ToWideString(std::string_view str);
std::string ToNarrowString(std::wstring_view wstr);
std::string EscapeJsonString(std::string_view str);

// Network latency utilities
std::optional<uint32_t> PingHost(const std::string& ipAddress, uint32_t timeoutMs = 800);

// Path resolution helpers
std::filesystem::path GetExecutableDir();
std::filesystem::path ResolveConfigPath(const std::filesystem::path& relPath);

} // namespace ebooster
