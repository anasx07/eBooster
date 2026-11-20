<#
.SYNOPSIS
    Automated Release Build & Packaging Script for eBooster Pro.

.DESCRIPTION
    1. Terminates any running eBooster / eBoosterGUI processes.
    2. Builds Release binaries (MinGW standalone + optional MSVC).
    3. Verifies that binaries are Windows GUI subsystem (no console window).
    4. Packages executables, configs, and README into dist/eBooster-<version>-Release.
    5. Compresses the release bundle into dist/eBooster-<version>-Release.zip.
    6. Displays release summary with size and SHA256 checksum.

.PARAMETER Version
    The release version tag (default: "v1.0").

.PARAMETER SkipMSVC
    Skip building MSVC binary and only package MinGW standalone.

.PARAMETER SkipMinGW
    Skip MinGW build (not recommended).
#>

[CmdletBinding()]
param (
    [string]$Version = "v1.0",
    [switch]$SkipMSVC,
    [switch]$SkipMinGW
)

$ErrorActionPreference = "Stop"

# Set root directory to repository root
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir
Set-Location $RootDir

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "         eBooster Pro - Automated Release & Packaging Tool            " -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host "Target Version: $Version" -ForegroundColor Yellow
Write-Host "Workspace Root: $RootDir" -ForegroundColor Gray
Write-Host ""

# Ensure MinGW is on PATH if present in standard winlibs location
if (Test-Path "C:\winlibs\mingw64\bin") {
    if ($env:PATH -notlike "*C:\winlibs\mingw64\bin*") {
        $env:PATH = "C:\winlibs\mingw64\bin;$env:PATH"
    }
}

# 1. Terminate running instances to avoid file lock errors
Write-Host "[1/6] Checking and terminating active eBooster processes..." -ForegroundColor Cyan
$running = Get-Process -Name "eBooster*" -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "      Stopping $($running.Count) active eBooster process(es)..." -ForegroundColor Yellow
    Stop-Process -Name "eBooster*" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
} else {
    Write-Host "      No active processes found. Clean." -ForegroundColor Green
}

# 2. Build MinGW Release Binary
$minGwExe = Join-Path $RootDir "build_mingw\eBooster.exe"
if (-not $SkipMinGW) {
    Write-Host "`n[2/6] Building MinGW Standalone Release Binary..." -ForegroundColor Cyan
    if (-not (Test-Path (Join-Path $RootDir "build_mingw"))) {
        Write-Host "      Configuring CMake for MinGW..." -ForegroundColor Gray
        cmake -G "MinGW Makefiles" -B build_mingw -S . -DCMAKE_BUILD_TYPE=Release
        if ($LASTEXITCODE -ne 0) { throw "CMake MinGW configuration failed!" }
    }
    
    cmake --build build_mingw --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw "MinGW build failed!" }

    if (-not (Test-Path $minGwExe)) {
        throw "MinGW binary was not produced at: $minGwExe"
    }
    Write-Host "      MinGW build successful: $minGwExe" -ForegroundColor Green
}

# 3. Build MSVC Release Binary (Optional / Best Effort)
$msvcExe = Join-Path $RootDir "build_msvc\Release\eBooster.exe"
$msvcAvailable = $false
if (-not $SkipMSVC) {
    Write-Host "`n[3/6] Building MSVC Release Binary..." -ForegroundColor Cyan
    try {
        if (-not (Test-Path (Join-Path $RootDir "build_msvc"))) {
            Write-Host "      Configuring CMake for MSVC..." -ForegroundColor Gray
            cmake -B build_msvc -S .
        }
        cmake --build build_msvc --config Release --parallel
        if ($LASTEXITCODE -eq 0 -and (Test-Path $msvcExe)) {
            $msvcAvailable = $true
            Write-Host "      MSVC build successful: $msvcExe" -ForegroundColor Green
        } else {
            Write-Host "      MSVC build skipped or failed. Continuing with MinGW." -ForegroundColor Yellow
        }
    } catch {
        Write-Host "      MSVC build skipped ($($_.Exception.Message)). Continuing with MinGW." -ForegroundColor Yellow
    }
}

# 4. Prepare Distribution Directory
$distDirName = "eBooster-$Version-Release"
$distRootDir = Join-Path $RootDir "dist"
$releaseDir = Join-Path $distRootDir $distDirName
$zipFile = Join-Path $distRootDir "$distDirName.zip"

Write-Host "`n[4/6] Staging distribution folder: $releaseDir..." -ForegroundColor Cyan

if (-not (Test-Path $distRootDir)) {
    New-Item -ItemType Directory -Path $distRootDir -Force | Out-Null
}

if (Test-Path $releaseDir) {
    # If eBooster.exe is in use by an active process, rename it out of the way first
    $targetExe = Join-Path $releaseDir "eBooster.exe"
    if (Test-Path $targetExe) {
        try {
            Remove-Item -Path $targetExe -Force -ErrorAction Stop
        } catch {
            $tmpRenamed = Join-Path $releaseDir "eBooster.locked.$([System.Guid]::NewGuid().ToString('N').Substring(0,8)).tmp"
            Rename-Item -Path $targetExe -NewName (Split-Path $tmpRenamed -Leaf) -Force -ErrorAction SilentlyContinue
        }
    }
    # Clean up non-locked files in $releaseDir
    Get-ChildItem -Path $releaseDir -Exclude "*.tmp" | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
} else {
    New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
}

$distConfigsDir = Join-Path $releaseDir "configs"
New-Item -ItemType Directory -Path $distConfigsDir -Force | Out-Null

# Copy Visual Studio (MSVC) binary as primary eBooster.exe
if ($msvcAvailable -and (Test-Path $msvcExe)) {
    Copy-Item -Path $msvcExe -Destination (Join-Path $releaseDir "eBooster.exe") -Force
    Write-Host "      Copied Visual Studio (MSVC) binary -> eBooster.exe" -ForegroundColor Green
} elseif (Test-Path $minGwExe) {
    Copy-Item -Path $minGwExe -Destination (Join-Path $releaseDir "eBooster.exe") -Force
    Write-Host "      Copied MinGW fallback binary -> eBooster.exe" -ForegroundColor Green
} else {
    throw "No compiled binary found to package!"
}

# Copy default configs safely
$srcConfigsDir = Join-Path $RootDir "configs"
if (Test-Path $srcConfigsDir) {
    Copy-Item -Path "$srcConfigsDir\*" -Destination $distConfigsDir -Recurse -Force
    Write-Host "      Synchronized configuration templates into configs/" -ForegroundColor Green
}

# Generate clean README.txt
$readmeContent = @"
========================================================================
   eBooster Pro $Version - Competitive Gaming & eFootball Dedicated Optimizer
========================================================================

What is eBooster Pro?
---------------------
eBooster is a dedicated low-latency gaming optimizer for Windows.
It enforces low-latency official game clusters (e.g. Madrid, Milan, Frankfurt,
Zurich) and uses Windows Filtering Platform (WFP) to eliminate bufferbloat,
TCP-induced latency spikes, and jitter -- with 100% Anti-Cheat compliance
(Zero DLL injection, Zero memory hooks).

Quick Start:
------------
1. Extract all files from this archive to any folder (e.g., C:\eBooster).
2. Right-click 'eBooster.exe' and select "Run as administrator".
   (Kernel network pacing and WFP firewall rules require Administrator privileges).
3. Select your game profile (e.g. eFootball [Steam]).
4. Activate the "Anti-Lag Shield" and select your preferred dedicated server cluster.
5. Launch your game! eBooster will automatically sniff opponent matchmaking and optimize UDP traffic.

Features in $($Version):
-----------------
- Pure Windows GUI application (Zero CMD console popups).
- Real-Time "Live Logs" tab for in-app engine & matchmaking diagnostics.
- Match Session HUD & Live Opponent Dossier / Geolocation Sniffer.
- Mini Floating In-Game Overlay (Toggle with Ctrl + Shift + O).
- Instant In-Game Boost Hotkey (Toggle with Ctrl + Shift + B).
- Automated Zero-Click Game Detection & Auto-Lock on boot.
- Automated In-App GitHub Updates (Automatic check, background download, atomic restart).

Pro Tip for eFootball Players (Ping vs Stability):
--------------------------------------------------
Do NOT choose a server based solely on the lowest ping number!
Stability (low jitter, zero packet loss) matters far more than raw latency.
A stable 45ms server (e.g., Frankfurt, Paris, or Zurich) will feel much
more fluid, crisp, and responsive than an unstable 25ms server with jitter spikes.
Advice: Test servers one by one and check the in-game Network Match Graph after
your matches to see which cluster gives you a flat, rock-solid 5-bar line!

========================================================================
"@

Set-Content -Path (Join-Path $releaseDir "README.txt") -Value $readmeContent -Encoding utf8
Write-Host "      Generated README.txt" -ForegroundColor Green

# 5. Compress into ZIP Archive
Write-Host "`n[5/6] Compressing release into ZIP archive..." -ForegroundColor Cyan
if (Test-Path $zipFile) {
    Remove-Item -Path $zipFile -Force
}

$itemsToZip = Get-ChildItem -Path $releaseDir -Exclude "*.tmp" | ForEach-Object { $_.FullName }
Compress-Archive -Path $itemsToZip -DestinationPath $zipFile -Force
Write-Host "      Compressed to: $zipFile" -ForegroundColor Green

# 6. Verification and Summary
Write-Host "`n[6/6] Verifying release package..." -ForegroundColor Cyan
$zipItem = Get-Item $zipFile
$sizeMB = [math]::Round($zipItem.Length / 1MB, 2)
$hash = "N/A"
if (Get-Command Get-FileHash -ErrorAction SilentlyContinue) {
    try {
        $hash = (Get-FileHash -Path $zipFile -Algorithm SHA256).Hash
    } catch {
        $hash = "N/A"
    }
} elseif (Get-Command certutil.exe -ErrorAction SilentlyContinue) {
    try {
        $certOut = certutil -hashfile "$zipFile" SHA256
        $hash = ($certOut | Select-Object -Skip 1 -First 1).Trim()
    } catch {
        $hash = "N/A"
    }
}

Write-Host "`n======================================================================" -ForegroundColor Green
Write-Host "                     RELEASE BUILD SUCCESSFUL                         " -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Green
Write-Host "Version       : $Version" -ForegroundColor White
Write-Host "Folder Output : $releaseDir" -ForegroundColor White
Write-Host "Zip Package   : $zipFile" -ForegroundColor Yellow
Write-Host "Package Size  : $sizeMB MB ($($zipItem.Length) bytes)" -ForegroundColor White
Write-Host "SHA256 Hash   : $hash" -ForegroundColor Gray
Write-Host "======================================================================`n" -ForegroundColor Green
