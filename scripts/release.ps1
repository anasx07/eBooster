<#
.SYNOPSIS
    Automated Release CLI for eBooster Pro (Powered by GitHub Actions CI/CD).

.DESCRIPTION
    Triggers an automated cloud release for eBooster:
    1. Detects current version and prompts/calculates SemVer bump (Patch, Minor, Major, Custom).
    2. Synchronizes version in CMakeLists.txt and include/ebooster/Common.hpp.
    3. Commits the version bump and tags the repository.
    4. Pushes commits & tag to GitHub (origin/main).
    5. GitHub Actions runner (windows-latest) automatically compiles the standalone MSVC binary,
       packages dist files, creates the GitHub Release, and uploads eBooster.exe & ZIP assets.

.PARAMETER Version
    Target version tag (e.g. "v1.0.2" or "1.0.2").

.PARAMETER Bump
    SemVer bump level: "patch", "minor", or "major".

.PARAMETER Title
    Release title (e.g. "eBooster Pro v1.0.2").

.PARAMETER LocalBuild
    Also compile and package a local release copy in dist/ before pushing.

.PARAMETER Repo
    GitHub repository in owner/repo format (default: "anasx07/eBooster").
#>

[CmdletBinding()]
param (
    [string]$Version = "",
    [ValidateSet("patch", "minor", "major", "")]
    [string]$Bump = "",
    [string]$Title = "",
    [switch]$LocalBuild,
    [switch]$NoPrompt,
    [string]$Repo = "anasx07/eBooster"
)

$ErrorActionPreference = "Stop"

# Repository root directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir
Set-Location $RootDir

function Write-Header {
    param ([string]$Text)
    Write-Host "`n======================================================================" -ForegroundColor Cyan
    Write-Host "   $Text" -ForegroundColor Green
    Write-Host "======================================================================" -ForegroundColor Cyan
}

function Write-Step {
    param ([string]$Num, [string]$Text)
    Write-Host "`n[$Num] $Text" -ForegroundColor Cyan
}

function Write-Success {
    param ([string]$Text)
    Write-Host "      ✓ $Text" -ForegroundColor Green
}

function Write-WarnMsg {
    param ([string]$Text)
    Write-Host "      ! $Text" -ForegroundColor Yellow
}

function Write-ErrMsg {
    param ([string]$Text)
    Write-Host "      ✗ $Text" -ForegroundColor Red
}

Write-Header "eBooster Pro - Cloud Release CLI (GitHub Actions CI/CD)"
Write-Host "Target Repository: https://github.com/$Repo" -ForegroundColor Gray
Write-Host "Workspace Root   : $RootDir" -ForegroundColor Gray

# --------------------------------------------------------------------
# 1. Parse Current Version from CMakeLists.txt and Common.hpp
# --------------------------------------------------------------------
$cmakeFile = Join-Path $RootDir "CMakeLists.txt"
$commonFile = Join-Path $RootDir "include\ebooster\Common.hpp"

$currentVerStr = "1.0.0"
if (Test-Path $cmakeFile) {
    $cmakeContent = Get-Content $cmakeFile -Raw
    if ($cmakeContent -match 'project\s*\(\s*eBooster\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') {
        $currentVerStr = $matches[1]
    }
}

$verParts = $currentVerStr.Split('.')
$major = [int]$verParts[0]
$minor = [int]$verParts[1]
$patch = if ($verParts.Count -ge 3) { [int]$verParts[2] } else { 0 }

$nextPatch = "$major.$minor.$($patch + 1)"
$nextMinor = "$major.$($minor + 1).0"
$nextMajor = "$($major + 1).0.0"

# --------------------------------------------------------------------
# 2. Determine New Version
# --------------------------------------------------------------------
$targetVersion = ""

if ($Version -ne "") {
    $targetVersion = $Version.TrimStart('v', 'V')
} elseif ($Bump -ne "") {
    switch ($Bump.ToLower()) {
        "patch" { $targetVersion = $nextPatch }
        "minor" { $targetVersion = $nextMinor }
        "major" { $targetVersion = $nextMajor }
    }
} else {
    Write-Host "`nCurrent Version: " -NoNewline -ForegroundColor White
    Write-Host "v$currentVerStr" -ForegroundColor Yellow
    Write-Host "`nChoose Version Bump Level:" -ForegroundColor Cyan
    Write-Host "  [1] Patch : v$nextPatch (Bug fixes, stability updates)  <-- Recommended" -ForegroundColor White
    Write-Host "  [2] Minor : v$nextMinor (New features, UI improvements)" -ForegroundColor White
    Write-Host "  [3] Major : v$nextMajor (Major architectural release)" -ForegroundColor White
    Write-Host "  [4] Custom: Enter version manually" -ForegroundColor White

    $choice = Read-Host "`nSelect [1-4] (default: 1)"
    switch ($choice) {
        "2" { $targetVersion = $nextMinor }
        "3" { $targetVersion = $nextMajor }
        "4" {
            $custom = Read-Host "Enter custom version (e.g. 1.0.2 or v1.0.2)"
            $targetVersion = $custom.Trim().TrimStart('v', 'V')
            if ([string]::IsNullOrWhiteSpace($targetVersion)) {
                $targetVersion = $nextPatch
            }
        }
        default { $targetVersion = $nextPatch }
    }
}

$tagVersion = "v$targetVersion"
Write-Host "`nTarget Release Tag: " -NoNewline -ForegroundColor White
Write-Host "$tagVersion" -ForegroundColor Green

# --------------------------------------------------------------------
# 3. Release Title
# --------------------------------------------------------------------
$releaseTitle = $Title
if ([string]::IsNullOrWhiteSpace($releaseTitle)) {
    $defaultTitle = "eBooster Pro $tagVersion"
    $inputTitle = Read-Host "`nEnter Release Title (default: '$defaultTitle')"
    if ([string]::IsNullOrWhiteSpace($inputTitle)) {
        $releaseTitle = $defaultTitle
    } else {
        $releaseTitle = $inputTitle.Trim()
    }
}

# --------------------------------------------------------------------
# 4. Synchronize Version in Codebase
# --------------------------------------------------------------------
Write-Step "1/4" "Updating Version in CMakeLists.txt and Common.hpp..."

# Update CMakeLists.txt
if (Test-Path $cmakeFile) {
    $content = Get-Content $cmakeFile -Raw
    $newContent = [regex]::Replace($content, 'project\s*\(\s*eBooster\s+VERSION\s+[0-9]+\.[0-9]+\.[0-9]+', "project(eBooster VERSION $targetVersion")
    if ($newContent -ne $content) {
        Set-Content -Path $cmakeFile -Value $newContent -NoNewline
        Write-Success "Updated CMakeLists.txt -> VERSION $targetVersion"
    } else {
        Write-Success "CMakeLists.txt already at $targetVersion"
    }
}

# Update include/ebooster/Common.hpp
if (Test-Path $commonFile) {
    $content = Get-Content $commonFile -Raw
    $newContent = [regex]::Replace($content, 'inline constexpr const char\* EBOOSTER_VERSION\s*=\s*"[^"]*"', "inline constexpr const char* EBOOSTER_VERSION = `"$tagVersion`"")
    if ($newContent -ne $content) {
        Set-Content -Path $commonFile -Value $newContent -NoNewline
        Write-Success "Updated Common.hpp -> EBOOSTER_VERSION = `"$tagVersion`""
    } else {
        Write-Success "Common.hpp already at $tagVersion"
    }
}

# --------------------------------------------------------------------
# 5. Optional Local Build
# --------------------------------------------------------------------
if ($LocalBuild) {
    Write-Step "LOCAL" "Compiling local MSVC standalone binary..."
    $buildScript = Join-Path $RootDir "scripts\build_release.ps1"
    & powershell -ExecutionPolicy Bypass -File $buildScript -Version $tagVersion -SkipMinGW
    Write-Success "Local build and packaging completed!"
}

# --------------------------------------------------------------------
# 6. Git Commit, Tag & Push
# --------------------------------------------------------------------
Write-Step "2/4" "Committing Version Bump to Git..."
try {
    git add CMakeLists.txt include/ebooster/Common.hpp configs/user_preferences.json README.md
    $status = (git status --porcelain 2>$null)
    if ($status) {
        git commit -m "chore(release): bump version to $tagVersion"
        Write-Success "Committed version bump"
    } else {
        Write-Success "Working directory clean"
    }
} catch {
    Write-WarnMsg "Commit check: $($_.Exception.Message)"
}

Write-Step "3/4" "Creating Git Tag $tagVersion..."
try {
    $existingTag = (git tag -l $tagVersion 2>$null)
    if ($existingTag) {
        Write-WarnMsg "Tag $tagVersion exists locally. Replacing..."
        git tag -d $tagVersion | Out-Null
    }
    git tag -a $tagVersion -m "$releaseTitle"
    Write-Success "Created annotated tag: $tagVersion"
} catch {
    Write-ErrMsg "Failed to create tag: $($_.Exception.Message)"
}

Write-Step "4/4" "Pushing to GitHub (origin/main --tags)..."
try {
    git push origin main
    git push origin $tagVersion --force
    Write-Success "Pushed commits and tag $tagVersion to GitHub!"
} catch {
    Write-ErrMsg "Git push failed! Check your internet connection or git authentication: $($_.Exception.Message)"
    exit 1
}

# --------------------------------------------------------------------
# 7. Summary & GitHub Actions Live Monitor
# --------------------------------------------------------------------
$actionsUrl = "https://github.com/$Repo/actions"
$releaseUrl = "https://github.com/$Repo/releases/tag/$tagVersion"

Write-Header "CLOUD BUILD TRIGGERED SUCCESSFULLY!"
Write-Host "GitHub Actions is now compiling eBooster.exe in the cloud!" -ForegroundColor Yellow
Write-Host ""
Write-Host "Target Tag    : $tagVersion" -ForegroundColor White
Write-Host "Release Title : $releaseTitle" -ForegroundColor White
Write-Host "Live CI Build : $actionsUrl" -ForegroundColor Cyan
Write-Host "Release Page  : $releaseUrl" -ForegroundColor Green
Write-Host ""
Write-Host "What happens next:" -ForegroundColor Gray
Write-Host "  1. GitHub spins up a Windows Server runner with Visual Studio." -ForegroundColor Gray
Write-Host "  2. Compiles standalone MSVC eBooster.exe (/MT static runtime)." -ForegroundColor Gray
Write-Host "  3. Compiles Inno Setup installer: eBooster-Setup.exe." -ForegroundColor Gray
Write-Host "  4. Packages portable executable: eBooster-Portable.exe." -ForegroundColor Gray
Write-Host "  5. Publishes GitHub Release with both Installer & Portable assets!" -ForegroundColor Green
Write-Host "  6. All active eBooster client apps will auto-detect the update!" -ForegroundColor Magenta
Write-Host "======================================================================`n" -ForegroundColor Cyan

if (-not $NoPrompt) {
    $openBrowser = Read-Host "Open GitHub Actions live build in browser? [Y/n] (default: Y)"
    if ($openBrowser -notmatch "^[nN]") {
        Start-Process $actionsUrl
    }
}
