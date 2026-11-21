# GEMINI.md - eBooster Pro Engineering & Architecture Guide

Welcome to the **eBooster Pro** codebase reference for Gemini and AI agents. This document provides a complete, authoritative overview of the application architecture, coding standards, DevOps workflows, and design invariants.

---

## 1. Project Overview & Identity

* **Name**: eBooster Pro
* **Tagline**: High-Performance Windows Game Booster, Anti-Lag WFP Shield & Dedicated Server Lock
* **Repository**: [https://github.com/anasx07/eBooster](https://github.com/anasx07/eBooster)
* **Author / Maintainer**: `anasx07`
* **Language & Standard**: Modern **C++23** (`/std:c++latest` on MSVC, `-std=c++23` on GCC)
* **Target OS**: Windows 10 & Windows 11 (64-bit only, `x64compatible`)
* **Graphics & UI Framework**: DirectX 11 (`d3d11`, `dxgi`, `d3dcompiler`) + [Dear ImGui](https://github.com/ocornut/imgui) (Docking branch) + [FontAwesome 6](https://fontawesome.com/)
* **Packaging**:
  - **Installer**: `eBooster-Setup.exe` (Inno Setup 6, LZMA2/Ultra compression, Start Menu, Desktop shortcut, clean Windows Uninstaller).
  - **Portable**: `eBooster-Portable.exe` (Single self-contained standalone binary with `/MT` static runtime, zero DLL dependencies).

### Visual & Brand Identity
* **Theme**: **eFootball Stadium (Palette 2)**
  - Background: Deep Midnight Navy (`#080B14`)
  - Surface Cards: Stadium Dark Blue (`#0D121F`)
  - Accent / Primary: Electric Stadium Blue (`#00A3FF`)
  - Highlights / Active State: Vivid eFootball Neon Magenta (`#FF007F`)
* **Icon / Logo**: **Concept C (Radar Lock)**
  - Multi-resolution Windows icon (`assets/app.ico`: 256x256 down to 16x16 with 32-bit ARGB PNG frames).
  - Embedded as Win32 Resource ID `1` in `assets/app.rc` for Explorer, Taskbar, Alt+Tab, and System Tray.

---

## 2. Core Architecture & Safety Invariants

### 🔒 100% Anti-Cheat Compliance Guarantee
* **Zero DLL Injection**: eBooster NEVER injects DLLs (`LoadLibrary`, `CreateRemoteThread`, `SetWindowsHookEx`) into any game process.
* **Zero Memory Modification**: eBooster NEVER opens game handles with `PROCESS_VM_WRITE` or `PROCESS_VM_READ`.
* **Pure Network & Kernel Level**:
  - All packet routing and port filtering are executed purely through the **Windows Filtering Platform (WFP)** API (`fwpuclnt.lib`) and standard Windows Network Socket APIs (`iphlpapi.lib`, `ws2_32.lib`).
  - Safe for all anti-cheat systems: Easy Anti-Cheat (EAC), BattlEye, Vanguard, Ricochet, Valve Anti-Cheat (VAC).

### 📁 Clean Storage Architecture (%LOCALAPPDATA%\eBooster)
To prevent cluttering the user's Desktop or Downloads folder:
* **Storage Location**: `C:\Users\<User>\AppData\Local\eBooster\`
  - `configs/profiles.json`: Game profiles and cluster definitions.
  - `configs/user_preferences.json`: Persisted user settings, window geometry, and hotkeys.
  - `configs/dns_backup.json`: Adapter DNS backup for clean restoration on shutdown.
  - `cache/steam/<appId>/header.jpg`: Downloaded Steam banners and media.
  - `cache/imgui.ini`: Window state and positioning.
  - `updates/eBooster_update.exe`: Downloaded update binaries for background self-update.
* **Path Helper**: ALWAYS use `ResolveConfigPath("configs/...")`, `GetAppDataDir()`, or `GetAppCacheDir()` from `ebooster/Common.hpp`. Never construct raw relative paths in writing code.
* **Portable Mode Fallback**: If `portable.txt` exists alongside `eBooster.exe`, `IsPortableMode()` returns `true`, and all storage remains local to the executable directory (ideal for USB drives).

---

## 3. Directory Layout

```
eBooster/
├── .github/
│   └── workflows/
│       ├── ci.yml                 # PR & Push CI compilation verification (MSVC Release)
│       └── release.yml            # Automated Cloud Release: Inno Setup, Portable, Checksums, Winget
├── assets/
│   ├── app.ico                    # Multi-resolution Windows application icon (16px to 256px)
│   ├── app.manifest               # Embedded Win32 execution manifest (requireAdministrator, High-DPI)
│   ├── app.rc                     # Windows resource script (Icon ID 1 + Manifest ID 1 24)
│   └── logo.png                   # High-resolution raster logo
├── configs/
│   ├── default_servers.json       # Shipped master server nodes list
│   ├── profiles.json              # Default pre-seeded game profiles (eFootball, CS2, etc.)
│   └── user_preferences.json      # Default user settings template
├── docs/
│   └── devops_guide.md            # Authenticode code signing & Winget publishing guide
├── include/
│   └── ebooster/
│       ├── BoosterEngine.hpp      # Engine coordinator (polling, state management)
│       ├── Common.hpp             # Shared utilities, Win32 RAII handles, path resolution, logging
│       ├── ConnectionSniffer.hpp  # Extended TCP/UDP table polling
│       ├── DnsRelayController.hpp # Smart DNS routing and DHCP restore logic
│       ├── EtwSniffer.hpp         # Kernel ETW socket event tracing
│       ├── GuiApp.hpp             # Win32 / DirectX 11 / ImGui desktop application
│       ├── InGameOverlay.hpp      # Floating DirectX 11 mini HUD overlay
│       ├── MatchSniffer.hpp       # Live match detection & opponent IP geolocation
│       ├── NetworkOptimizer.hpp   # Windows gaming TCP/UDP registry & network latency tweaks
│       ├── ProcessMonitor.hpp     # Fast game executable process detection
│       ├── ProfileManager.hpp     # JSON profiles parser, serializer, and auto-backup
│       ├── RouteController.hpp    # WFP kernel filter controller & dedicated server region lock
│       ├── SteamDetector.hpp      # Steam library scanner & Akamai CDN banner fetcher
│       ├── TextureLoader.hpp      # WIC texture loader (DirectX 11 SRVs)
│       ├── UpdateManager.hpp      # In-App GitHub Releases Auto-Update engine
│       └── UserPreferences.hpp    # User settings JSON serialization & geometry management
├── installer/
│   └── eBooster.iss               # Inno Setup 6 compiler script
├── scripts/
│   ├── build_release.ps1          # Standalone release build, Inno Setup packaging, checksums
│   ├── generate_icon.ps1          # Multi-resolution .ico generator from source image
│   └── release.ps1                # Interactive & automated Release CLI (SemVer bump, git tag & push)
├── src/
│   ├── BoosterEngine.cpp
│   ├── Common.cpp
│   ├── ConnectionSniffer.cpp
│   ├── DnsRelayController.cpp
│   ├── EtwSniffer.cpp
│   ├── GuiApp.cpp                 # Complete UI rendering, tabs, modals, style setup
│   ├── gui_main.cpp               # WinMain entry point, single-instance mutex, DPI awareness
│   ├── InGameOverlay.cpp
│   ├── MatchSniffer.cpp
│   ├── NetworkOptimizer.cpp
│   ├── ProcessMonitor.cpp
│   ├── ProfileManager.cpp
│   ├── RouteController.cpp
│   ├── SteamDetector.cpp
│   ├── TextureLoader.cpp
│   ├── UpdateManager.cpp
│   └── UserPreferences.cpp
├── third_party/
│   ├── fontawesome/               # FontAwesome 6 Solid Icons header & glyph ranges
│   └── imgui/                     # Dear ImGui library (Docking branch) + DX11 / Win32 backends
├── CMakeLists.txt                 # Master CMake build configuration (MSVC / MinGW, static /MT)
├── LICENSE                        # MIT License
├── README.md                      # Public project documentation & badges
├── release.bat                    # One-click Windows release launcher
└── GEMINI.md                      # This developer & AI agent guide
```

---

## 4. Key Subsystems & Implementation Details

### 1. In-App GitHub Releases Auto-Updater (`UpdateManager`)
* Communicates directly with the GitHub API (`https://api.github.com/repos/anasx07/eBooster/releases/latest`) using native `WinHttp`.
* Follows AWS S3 redirects automatically (`WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS`).
* Downloads the update binary in the background in 64 KB chunks to `%LOCALAPPDATA%\eBooster\updates\eBooster_update.exe`.
* **Atomic Win32 Replacement**: Windows NT prohibits overwriting running executables, but permits renaming them. `ApplyUpdateAndRestart()`:
  1. Renames `curPath` -> `curPath + ".old.exe"`.
  2. Copies `updates\eBooster_update.exe` -> `curPath` (works across drives).
  3. Launches the new executable via `ShellExecuteW`.
  4. Calls `ExitProcess(0)`.
  5. On next boot, `CleanupOldExecutable()` safely deletes `.old.exe`.

### 2. Network Latency & Kernel Optimizer (`NetworkOptimizer`)
* **Nagle's Algorithm**: Sets `TcpAckFrequency=1` and `TCPNoDelay=1` across active network interfaces in `HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Interfaces`.
* **Network Throttling**: Sets `NetworkThrottlingIndex=0xFFFFFFFF` in `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile`.
* **System Responsiveness**: Sets `SystemResponsiveness=0` (allocates 100% CPU priority to games).
* **Game Scheduling Priority**: Elevates multimedia scheduler priority to `Priority=6`, `Scheduling Category=High`.
* **DWM Multiplane Overlay (MPO) Fix**: Disables `OverlayTestMode=5` in `HKLM\SOFTWARE\Microsoft\Windows\Dwm` to eliminate GPU micro-stuttering.
* **eFootball Anti-TCP Lag Shield**: Intercepts and blocks outbound TCP connections on ports `30000-35000` and `5736` using WFP, forcing the game into pure UDP dedicated server transport.

### 3. WFP Region Lock (`RouteController`)
* Uses Windows Filtering Platform session handles (`FwpmEngineOpen0`).
* Enforces destination server IP filters using standard CIDR blocks.
* Drops unwanted remote game servers, ensuring matchmaking locks onto optimal low-latency clusters.

---

## 5. Build, Run & Verification Commands

### Prerequisites
* Windows 10/11 x64
* CMake 3.25+
* Visual Studio 2022 (MSVC with "Desktop development with C++", Windows SDK 10.0.26100.0+)
* Optional: Inno Setup 6 (for building installers locally)

### 1. Compile Release Standalone Executable (MSVC)
```powershell
# Configure CMake
cmake -B build_msvc -S .

# Compile Release binary (/MT static runtime)
cmake --build build_msvc --config Release --parallel
```
* Binary Output: `build_msvc\Release\eBooster.exe` (Standalone, zero DLL dependencies).

### 2. Local Packaging & Checksums Script
```powershell
# Builds MSVC standalone binary, stages eBooster-Portable.exe, compiles Inno Setup (if present), and generates dist\checksums.txt
powershell -ExecutionPolicy Bypass -File scripts\build_release.ps1 -Version "v1.0.4" -SkipMinGW
```

### 3. Automated Cloud Release CLI
```powershell
# Interactive mode (prompts for SemVer bump: Patch, Minor, Major):
.\release.bat

# Or direct non-interactive command:
powershell -ExecutionPolicy Bypass -File scripts\release.ps1 -Version "1.0.5" -Title "eBooster Pro v1.0.5" -NoPrompt
```

---

## 6. CI/CD & DevOps Architecture

### Continuous Integration (`.github/workflows/ci.yml`)
* **Trigger**: Every `push` to `main` and every `pull_request` targeting `main`.
* **Runner**: `windows-latest`
* **Task**: Compiles MSVC Release build in parallel and validates output binary integrity.

### Release Build & Publish (`.github/workflows/release.yml`)
* **Trigger**: Pushing an annotated Git tag starting with `v*` (e.g. `v1.0.5`).
* **Runner**: `windows-latest`
* **Pipeline Steps**:
  1. Installs Inno Setup via Chocolatey (`choco install innosetup --no-progress -y`).
  2. Builds `/MT` static standalone binary with Visual Studio MSVC 2022.
  3. Compiles Inno Setup installer (`dist\eBooster-Setup.exe`).
  4. Stages portable executable (`dist\eBooster-Portable.exe`).
  5. Computes SHA-256 integrity hashes into `dist\checksums.txt`.
  6. Authenticode Code Signing (`signtool.exe` with RFC 3161 timestamping) if `CERTIFICATE_BASE64` secret exists.
  7. Publishes GitHub Release with structured markdown release notes.
  8. Submits package update to Windows Package Manager (`winget`) via `vedantmgoyal2009/winget-releaser@v2` if `WINGET_TOKEN` secret exists.

---

## 7. Development Guidelines & Rules for AI Agents

1. **Anti-Cheat Integrity**: Never suggest, write, or accept code that performs DLL injection, thread hijacking, process memory reading/writing, or inline hooking on games. Keep all interactions strictly at the WFP network and Win32 socket layers.
2. **Desktop Purity**: Never save files in current working directory or executable directory unless `IsPortableMode()` is explicitly active. All persistent configs and cache must go through `ResolveConfigPath()`, `GetAppDataDir()`, or `GetAppCacheDir()`.
3. **Static Runtime (`/MT`)**: Always maintain `/MT` static runtime linking in `CMakeLists.txt`. Do not introduce dynamic runtime DLL dependencies (`MSVCP140.dll`, `VCRUNTIME140.dll`).
4. **Win32 Handles**: Always wrap Win32 handles (`HANDLE`, `SC_HANDLE`, `HKEY`) in RAII wrappers (`MakeUniqueHandle`, `UniqueHandle`) to prevent resource and socket leaks.
5. **Atomic File I/O**: When saving configuration files (`ProfileManager::SaveProfiles`, `PreferencesManager::Save`), always write to a temporary `.tmp` file first and perform an atomic `std::filesystem::rename` to prevent JSON corruption during abrupt crashes or power loss.
6. **Thread Safety**: Network polling, ETW sniffing, and GitHub API update checks run on background worker threads. Protect shared state with `std::mutex` and use atomic flags (`std::atomic<bool>`) for cancellation tokens.
7. **DPI & Scaling**: GUI rendering uses DPI scaling (`m_dpiScale`). Scale all font sizes, padding, frame spacing, and child window geometries by `m_dpiScale` to ensure crisp rendering on 1080p, 1440p, and 4K high-DPI displays.
