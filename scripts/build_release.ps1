<#
.SYNOPSIS
    Automated Release Build Script for eBooster Pro.

.DESCRIPTION
    1. Terminates any running eBooster processes to release file locks.
    2. Builds Release binary (Visual Studio MSVC standalone /MT).
    3. Stages single standalone eBooster.exe into dist/eBooster.exe.
    4. Computes file size and SHA-256 hash.

.PARAMETER Version
    The release version tag (default: "v1.0").

.PARAMETER SkipMSVC
    Skip MSVC build.

.PARAMETER SkipMinGW
    Skip MinGW build (default: MSVC only).
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
Write-Host "         eBooster Pro - Standalone Executable Build Tool              " -ForegroundColor Green
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
Write-Host "[1/4] Checking and terminating active eBooster processes..." -ForegroundColor Cyan
$running = Get-Process -Name "eBooster*" -ErrorAction SilentlyContinue
if ($running) {
    Write-Host "      Stopping $($running.Count) active eBooster process(es)..." -ForegroundColor Yellow
    Stop-Process -Name "eBooster*" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
} else {
    Write-Host "      No active processes found. Clean." -ForegroundColor Green
}

# 2. Build MinGW Release Binary (if requested)
$minGwExe = Join-Path $RootDir "build_mingw\eBooster.exe"
if (-not $SkipMinGW -and -not (Test-Path (Join-Path $RootDir "build_msvc"))) {
    Write-Host "`n[2/4] Building MinGW Standalone Release Binary..." -ForegroundColor Cyan
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

# 3. Build Visual Studio (MSVC) Release Binary
$msvcExe = Join-Path $RootDir "build_msvc\Release\eBooster.exe"
$msvcAvailable = $false
if (-not $SkipMSVC) {
    Write-Host "`n[3/4] Building MSVC Standalone Release Binary (/MT Static Runtime)..." -ForegroundColor Cyan
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
            if ($SkipMinGW) { throw "MSVC build failed (exit code $LASTEXITCODE)!" }
            Write-Host "      MSVC build skipped or failed. Continuing with MinGW." -ForegroundColor Yellow
        }
    } catch {
        if ($SkipMinGW) { throw $_ }
        Write-Host "      MSVC build skipped ($($_.Exception.Message)). Continuing with MinGW." -ForegroundColor Yellow
    }
}

# 4. Prepare Distribution Directory & Single Executable Output
$distRootDir = Join-Path $RootDir "dist"
$targetExe = Join-Path $distRootDir "eBooster.exe"

Write-Host "`n[4/4] Staging standalone executable: $targetExe..." -ForegroundColor Cyan

if (-not (Test-Path $distRootDir)) {
    New-Item -ItemType Directory -Path $distRootDir -Force | Out-Null
}

# If eBooster.exe is in use by an active process, rename it out of the way first
if (Test-Path $targetExe) {
    try {
        Remove-Item -Path $targetExe -Force -ErrorAction Stop
    } catch {
        $tmpRenamed = Join-Path $distRootDir "eBooster.locked.$([System.Guid]::NewGuid().ToString('N').Substring(0,8)).tmp"
        Rename-Item -Path $targetExe -NewName (Split-Path $tmpRenamed -Leaf) -Force -ErrorAction SilentlyContinue
    }
}

# Copy Visual Studio (MSVC) binary as primary eBooster.exe
if ($msvcAvailable -and (Test-Path $msvcExe)) {
    Copy-Item -Path $msvcExe -Destination $targetExe -Force
    Write-Host "      Copied Visual Studio (MSVC) binary -> $targetExe" -ForegroundColor Green
} elseif (Test-Path $minGwExe) {
    Copy-Item -Path $minGwExe -Destination $targetExe -Force
    Write-Host "      Copied MinGW fallback binary -> $targetExe" -ForegroundColor Green
} else {
    throw "No compiled binary found to package!"
}

# Verification and Summary
$exeItem = Get-Item $targetExe
$sizeMB = [math]::Round($exeItem.Length / 1MB, 2)
$hash = (Get-FileHash -Path $targetExe -Algorithm SHA256).Hash

Write-Host "`n======================================================================" -ForegroundColor Green
Write-Host "                     BUILD SUCCESSFUL (STANDALONE)                    " -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Green
Write-Host "Version       : $Version" -ForegroundColor Yellow
Write-Host "Binary Output : $targetExe" -ForegroundColor Cyan
Write-Host "File Size     : $sizeMB MB ($($exeItem.Length) bytes)" -ForegroundColor White
Write-Host "SHA256 Hash   : $hash" -ForegroundColor Gray
Write-Host "======================================================================`n" -ForegroundColor Green
