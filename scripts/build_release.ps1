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

# 4. Prepare Distribution Directory, Standalone, Portable & Installer
$distRootDir = Join-Path $RootDir "dist"
$targetExe = Join-Path $distRootDir "eBooster.exe"
$targetPortable = Join-Path $distRootDir "eBooster-Portable.exe"
$targetSetup = Join-Path $distRootDir "eBooster-Setup.exe"

Write-Host "`n[4/4] Staging binaries and packaging distribution in: $distRootDir..." -ForegroundColor Cyan

if (-not (Test-Path $distRootDir)) {
    New-Item -ItemType Directory -Path $distRootDir -Force | Out-Null
}

# If files are locked, handle gracefully
foreach ($f in @($targetExe, $targetPortable)) {
    if (Test-Path $f) {
        try {
            Remove-Item -Path $f -Force -ErrorAction Stop
        } catch {
            $tmpRenamed = Join-Path $distRootDir "$((Get-Item $f).BaseName).locked.$([System.Guid]::NewGuid().ToString('N').Substring(0,8)).tmp"
            Rename-Item -Path $f -NewName (Split-Path $tmpRenamed -Leaf) -Force -ErrorAction SilentlyContinue
        }
    }
}

# Copy Visual Studio (MSVC) binary as primary eBooster.exe and eBooster-Portable.exe
if ($msvcAvailable -and (Test-Path $msvcExe)) {
    Copy-Item -Path $msvcExe -Destination $targetExe -Force
    Copy-Item -Path $msvcExe -Destination $targetPortable -Force
    Write-Host "      Staged Base Binary     -> $targetExe" -ForegroundColor Green
    Write-Host "      Staged Portable Binary -> $targetPortable" -ForegroundColor Green
} elseif (Test-Path $minGwExe) {
    Copy-Item -Path $minGwExe -Destination $targetExe -Force
    Copy-Item -Path $minGwExe -Destination $targetPortable -Force
    Write-Host "      Staged Base Binary (MinGW)     -> $targetExe" -ForegroundColor Green
    Write-Host "      Staged Portable Binary (MinGW) -> $targetPortable" -ForegroundColor Green
} else {
    throw "No compiled binary found to package!"
}

# Copy configs and ui/dist to dist directory for portable standalone distribution
if (Test-Path (Join-Path $RootDir "ui\dist")) {
    Copy-Item -Path (Join-Path $RootDir "ui\dist") -Destination (Join-Path $distRootDir "ui\dist") -Recurse -Force
    Write-Host "      Staged React UI dist   -> $(Join-Path $distRootDir 'ui\dist')" -ForegroundColor Green
}
if (Test-Path (Join-Path $RootDir "configs")) {
    Copy-Item -Path (Join-Path $RootDir "configs") -Destination (Join-Path $distRootDir "configs") -Recurse -Force
    Write-Host "      Staged Configs         -> $(Join-Path $distRootDir 'configs')" -ForegroundColor Green
}

# Clean version for Inno Setup define (e.g. "1.0.3" from "v1.0.3")
$cleanVer = $Version.TrimStart('v', 'V')
if ([string]::IsNullOrWhiteSpace($cleanVer)) { $cleanVer = "1.0.0" }

# 5. Look for Inno Setup Compiler (ISCC) and compile Installer
$isccPaths = @(
    "ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "C:\ProgramData\chocolatey\bin\iscc.exe"
)
$isccExe = $null
foreach ($candidate in $isccPaths) {
    if (Get-Command $candidate -ErrorAction SilentlyContinue) {
        $isccExe = (Get-Command $candidate).Source
        break
    } elseif (Test-Path $candidate) {
        $isccExe = $candidate
        break
    }
}

$issScript = Join-Path $RootDir "installer\eBooster.iss"
if ($isccExe -and (Test-Path $issScript)) {
    Write-Host "`n      Found Inno Setup Compiler: $isccExe" -ForegroundColor Cyan
    Write-Host "      Compiling eBooster-Setup.exe (v$cleanVer)..." -ForegroundColor Cyan
    & $isccExe "/DMyAppVersion=$cleanVer" $issScript | Out-Null
    if ($LASTEXITCODE -eq 0 -and (Test-Path $targetSetup)) {
        Write-Host "      Built Installer        -> $targetSetup" -ForegroundColor Green
    } else {
        Write-Host "      Inno Setup build failed (exit code: $LASTEXITCODE). Continuing." -ForegroundColor Yellow
    }
} else {
    Write-Host "      Inno Setup not found locally. Installer will be built automatically on GitHub Actions CI/CD." -ForegroundColor Yellow
}

# 6. Generate SHA-256 Checksums (checksums.txt)
$checksumFile = Join-Path $distRootDir "checksums.txt"
$checksumLines = @()
foreach ($distFile in @($targetSetup, $targetPortable)) {
    if (Test-Path $distFile) {
        $fHash = (Get-FileHash -Path $distFile -Algorithm SHA256).Hash.ToLower()
        $fName = Split-Path $distFile -Leaf
        $checksumLines += "$fHash  $fName"
    }
}
if ($checksumLines.Count -gt 0) {
    $checksumContent = ($checksumLines -join "`r`n") + "`r`n"
    Set-Content -Path $checksumFile -Value $checksumContent -NoNewline -Encoding ASCII
    Write-Host "      Generated Checksums    -> $checksumFile" -ForegroundColor Green
}

# Verification and Summary
$exeItem = Get-Item $targetExe
$sizeMB = [math]::Round($exeItem.Length / 1MB, 2)
$hash = (Get-FileHash -Path $targetExe -Algorithm SHA256).Hash

Write-Host "`n======================================================================" -ForegroundColor Green
Write-Host "                BUILD & PACKAGING SUCCESSFUL                          " -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Green
Write-Host "Target Version : $Version" -ForegroundColor Yellow
Write-Host "Base Binary    : $targetExe ($sizeMB MB)" -ForegroundColor White
Write-Host "Portable Binary: $targetPortable ($sizeMB MB)" -ForegroundColor Cyan
if (Test-Path $targetSetup) {
    $setupItem = Get-Item $targetSetup
    $setupMB = [math]::Round($setupItem.Length / 1MB, 2)
    Write-Host "Setup Package  : $targetSetup ($setupMB MB)" -ForegroundColor Magenta
}
if (Test-Path $checksumFile) {
    Write-Host "Checksums File : $checksumFile" -ForegroundColor Gray
}
Write-Host "SHA256 (Base)  : $hash" -ForegroundColor Gray
Write-Host "======================================================================`n" -ForegroundColor Green
