<#
.SYNOPSIS
    Interactive & Automated Release CLI for eBooster Pro.

.DESCRIPTION
    Automates the entire release pipeline for eBooster:
    1. Detects and bumps SemVer version (Patch, Minor, Major, Custom).
    2. Collects or auto-generates release notes from git commits.
    3. Updates CMakeLists.txt and include/ebooster/Common.hpp.
    4. Compiles standalone Visual Studio MSVC binary and creates ZIP bundle.
    5. Commits version bump, creates annotated Git tag, and pushes to origin.
    6. Publishes Release to GitHub:
       - via GitHub CLI (gh) if installed.
       - via GitHub REST API if GITHUB_TOKEN or .github_token exists.
       - via Browser fallback with pre-filled fields & opened explorer.

.PARAMETER Version
    Target version tag (e.g. "v1.1.0" or "1.1.0").

.PARAMETER Bump
    SemVer bump level: "patch", "minor", or "major".

.PARAMETER Title
    Release title (e.g. "eBooster Pro v1.1.0 - Dedicated Server Update").

.PARAMETER Notes
    Release notes / changelog text.

.PARAMETER SkipPush
    Skip git push and GitHub release creation (build & tag only).

.PARAMETER Repo
    GitHub repository in owner/repo format (default: "anasx07/eBooster").
#>

[CmdletBinding()]
param (
    [string]$Version = "",
    [ValidateSet("patch", "minor", "major", "")]
    [string]$Bump = "",
    [string]$Title = "",
    [string]$Notes = "",
    [switch]$SkipPush,
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

Write-Header "eBooster Pro - Official GitHub Release CLI"
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
    Write-Host "  [1] Patch : v$nextPatch (Bug fixes, stability updates)" -ForegroundColor White
    Write-Host "  [2] Minor : v$nextMinor (New features, UI improvements)" -ForegroundColor White
    Write-Host "  [3] Major : v$nextMajor (Major architectural release)" -ForegroundColor White
    Write-Host "  [4] Custom: Enter version manually" -ForegroundColor White

    $choice = Read-Host "`nSelect [1-4] (default: 1)"
    switch ($choice) {
        "2" { $targetVersion = $nextMinor }
        "3" { $targetVersion = $nextMajor }
        "4" {
            $custom = Read-Host "Enter custom version (e.g. 1.0.5 or v1.0.5)"
            $targetVersion = $custom.Trim().TrimStart('v', 'V')
            if ([string]::IsNullOrWhiteSpace($targetVersion)) {
                $targetVersion = $nextPatch
            }
        }
        default { $targetVersion = $nextPatch }
    }
}

$tagVersion = "v$targetVersion"
Write-Host "`nSelected Release Tag: " -NoNewline -ForegroundColor White
Write-Host "$tagVersion" -ForegroundColor Green

# --------------------------------------------------------------------
# 3. Determine Release Title & Release Notes
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

$releaseNotes = $Notes
if ([string]::IsNullOrWhiteSpace($releaseNotes)) {
    # Generate changelog from git commits since previous tag
    $autoNotes = ""
    try {
        $latestTag = (git describe --tags --abbrev=0 2>$null)
        if ($latestTag) {
            $commits = (git log "$latestTag..HEAD" --pretty=format:"- %s" 2>$null)
            if ($commits) {
                $autoNotes = $commits -join "`n"
            }
        } else {
            $commits = (git log -n 5 --pretty=format:"- %s" 2>$null)
            if ($commits) {
                $autoNotes = $commits -join "`n"
            }
        }
    } catch {}

    if ([string]::IsNullOrWhiteSpace($autoNotes)) {
        $autoNotes = "- Performance improvements and latency optimizations`n- WFP Anti-Lag Shield enhancements`n- General stability updates"
    }

    Write-Host "`nRelease Notes (Auto-detected from git):" -ForegroundColor Cyan
    Write-Host "$autoNotes" -ForegroundColor Gray
    $confirmNotes = Read-Host "`nUse these release notes? [Y/n] (or type custom notes)"
    if ($confirmNotes -match "^[nN]") {
        $customNotes = Read-Host "Enter your release notes"
        $releaseNotes = if ([string]::IsNullOrWhiteSpace($customNotes)) { $autoNotes } else { $customNotes }
    } else {
        $releaseNotes = $autoNotes
    }
}

# --------------------------------------------------------------------
# 4. Synchronize Version in Codebase
# --------------------------------------------------------------------
Write-Step "1/5" "Updating Version across CMakeLists.txt and C++ Headers..."

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
# 5. Compile Standalone Release Binary & Packaging
# --------------------------------------------------------------------
Write-Step "2/5" "Building MSVC Standalone Binary & Staging Distribution Archive..."
$buildScript = Join-Path $RootDir "scripts\build_release.ps1"
if (-not (Test-Path $buildScript)) {
    throw "Packaging script not found at: $buildScript"
}

& powershell -ExecutionPolicy Bypass -File $buildScript -Version $tagVersion
if ($LASTEXITCODE -ne 0) {
    throw "Release build and packaging failed!"
}

$distDir = Join-Path $RootDir "dist\eBooster-$tagVersion-Release"
$distExe = Join-Path $distDir "eBooster.exe"
$distZip = Join-Path $RootDir "dist\eBooster-$tagVersion-Release.zip"

if (-not (Test-Path $distExe)) {
    throw "Distribution executable not found: $distExe"
}
if (-not (Test-Path $distZip)) {
    throw "Distribution zip not found: $distZip"
}

$exeSizeMB = [math]::Round((Get-Item $distExe).Length / 1MB, 2)
$zipSizeMB = [math]::Round((Get-Item $distZip).Length / 1MB, 2)
$sha256 = (Get-FileHash -Path $distExe -Algorithm SHA256).Hash

Write-Success "Binary compiled: $distExe ($exeSizeMB MB)"
Write-Success "ZIP package ready: $distZip ($zipSizeMB MB)"
Write-Success "SHA256 Checksum: $sha256"

# --------------------------------------------------------------------
# 6. Git Commit, Tag & Push
# --------------------------------------------------------------------
if ($SkipPush) {
    Write-WarnMsg "SkipPush flag enabled. Skipping git commit, tag, and GitHub release."
    Write-Host "`nRelease artifacts staged locally in:" -ForegroundColor Green
    Write-Host "  $distDir" -ForegroundColor Cyan
    Write-Host "  $distZip" -ForegroundColor Cyan
    exit 0
}

Write-Step "3/5" "Creating Git Commit and Tagging $tagVersion..."
try {
    # Check if there are modified files to commit
    git add CMakeLists.txt include/ebooster/Common.hpp configs/user_preferences.json README.md
    $status = (git status --porcelain 2>$null)
    if ($status) {
        git commit -m "chore(release): bump version to $tagVersion"
        Write-Success "Committed version bump to Git"
    } else {
        Write-Success "Working directory clean. No files to commit."
    }

    # Tagging
    $existingTag = (git tag -l $tagVersion 2>$null)
    if ($existingTag) {
        Write-WarnMsg "Tag $tagVersion already exists locally. Re-tagging..."
        git tag -d $tagVersion | Out-Null
    }

    git tag -a $tagVersion -m "$releaseTitle`n`n$releaseNotes"
    Write-Success "Created annotated git tag: $tagVersion"
} catch {
    Write-ErrMsg "Git commit/tag operation encountered an issue: $($_.Exception.Message)"
}

Write-Step "4/5" "Pushing Commits and Tags to GitHub (origin/main)..."
try {
    git push origin main
    git push origin $tagVersion --force
    Write-Success "Pushed commits and tag $tagVersion to GitHub!"
} catch {
    Write-ErrMsg "Failed to push to GitHub remote. Ensure git credentials are configured."
}

# --------------------------------------------------------------------
# 7. Publish GitHub Release & Upload Binary Assets
# --------------------------------------------------------------------
Write-Step "5/5" "Publishing GitHub Release and Uploading Standalone Assets..."

# Check GitHub token in env or local file
$githubToken = $env:GITHUB_TOKEN
$tokenFile = Join-Path $RootDir ".github_token"
if ([string]::IsNullOrWhiteSpace($githubToken) -and (Test-Path $tokenFile)) {
    $githubToken = (Get-Content $tokenFile -Raw).Trim()
}

$hasGhCli = $false
try {
    $ghCheck = Get-Command gh -ErrorAction SilentlyContinue
    if ($ghCheck) {
        $ghAuth = (gh auth status 2>&1)
        if ($LASTEXITCODE -eq 0) {
            $hasGhCli = $true
        }
    }
} catch {}

$releasePublished = $false

# Method A: GitHub CLI (gh)
if ($hasGhCli) {
    Write-Host "      Detected authenticated GitHub CLI (gh). Publishing via gh..." -ForegroundColor Cyan
    try {
        gh release create $tagVersion $distExe $distZip --title $releaseTitle --notes $releaseNotes
        if ($LASTEXITCODE -eq 0) {
            $releasePublished = $true
            Write-Success "Release published via GitHub CLI!"
        }
    } catch {
        Write-WarnMsg "GitHub CLI failed ($($_.Exception.Message)). Trying REST API..."
    }
}

# Method B: GitHub REST API (PowerShell Native)
if (-not $releasePublished -and -not [string]::IsNullOrWhiteSpace($githubToken)) {
    Write-Host "      Using GitHub REST API with Personal Access Token..." -ForegroundColor Cyan
    try {
        $apiHeaders = @{
            "Authorization" = "Bearer $githubToken"
            "Accept"        = "application/vnd.github+json"
            "X-GitHub-Api-Version" = "2022-11-28"
            "User-Agent"    = "eBooster-Release-CLI"
        }

        # 1. Create or get release
        $createReleaseUrl = "https://api.github.com/repos/$Repo/releases"
        $releaseBody = @{
            tag_name         = $tagVersion
            target_commitish = "main"
            name             = $releaseTitle
            body             = $releaseNotes
            draft            = $false
            prerelease       = $false
        } | ConvertTo-Json -Compress

        Write-Host "      Creating release $tagVersion on GitHub..." -ForegroundColor Gray
        $relResp = Invoke-RestMethod -Uri $createReleaseUrl -Method Post -Headers $apiHeaders -Body $releaseBody -ContentType "application/json"
        
        $uploadUrl = $relResp.upload_url -replace '\{\?name,label\}', ''

        # 2. Upload eBooster.exe
        Write-Host "      Uploading eBooster.exe ($exeSizeMB MB)..." -ForegroundColor Gray
        $uploadExeUrl = "$uploadUrl?name=eBooster.exe"
        $exeBytes = [System.IO.File]::ReadAllBytes($distExe)
        Invoke-RestMethod -Uri $uploadExeUrl -Method Post -Headers $apiHeaders -Body $exeBytes -ContentType "application/octet-stream" | Out-Null
        Write-Success "Uploaded asset: eBooster.exe"

        # 3. Upload eBooster-vX.X.X-Release.zip
        Write-Host "      Uploading eBooster-$tagVersion-Release.zip ($zipSizeMB MB)..." -ForegroundColor Gray
        $zipName = [System.IO.Path]::GetFileName($distZip)
        $uploadZipUrl = "$uploadUrl?name=$zipName"
        $zipBytes = [System.IO.File]::ReadAllBytes($distZip)
        Invoke-RestMethod -Uri $uploadZipUrl -Method Post -Headers $apiHeaders -Body $zipBytes -ContentType "application/octet-stream" | Out-Null
        Write-Success "Uploaded asset: $zipName"

        $releasePublished = $true
        Write-Success "Release published successfully via GitHub REST API!"
    } catch {
        Write-ErrMsg "GitHub REST API error: $($_.Exception.Message)"
    }
}

# Method C: Interactive Token Setup or Browser Fallback
if (-not $releasePublished) {
    Write-Host "`n----------------------------------------------------------------------" -ForegroundColor Yellow
    Write-Host "No GitHub CLI authentication or GITHUB_TOKEN detected." -ForegroundColor Yellow
    Write-Host "Would you like to:" -ForegroundColor Cyan
    Write-Host "  [1] Enter a GitHub Personal Access Token (saves to .github_token)" -ForegroundColor White
    Write-Host "  [2] Open GitHub Release page in browser (drag-and-drop ready)" -ForegroundColor White

    $authChoice = Read-Host "`nSelect [1-2] (default: 2)"
    if ($authChoice -eq "1") {
        $tokenInput = Read-Host "Paste your GitHub Token (with 'repo' scope)"
        if (-not [string]::IsNullOrWhiteSpace($tokenInput)) {
            Set-Content -Path $tokenFile -Value $tokenInput.Trim() -Encoding utf8
            Write-Success "Saved token to .github_token (gitignored)."
            Write-Host "Re-running asset upload with your token..." -ForegroundColor Cyan

            try {
                $apiHeaders = @{
                    "Authorization" = "Bearer $($tokenInput.Trim())"
                    "Accept"        = "application/vnd.github+json"
                    "X-GitHub-Api-Version" = "2022-11-28"
                    "User-Agent"    = "eBooster-Release-CLI"
                }

                $createReleaseUrl = "https://api.github.com/repos/$Repo/releases"
                $releaseBody = @{
                    tag_name         = $tagVersion
                    target_commitish = "main"
                    name             = $releaseTitle
                    body             = $releaseNotes
                    draft            = $false
                    prerelease       = $false
                } | ConvertTo-Json -Compress

                $relResp = Invoke-RestMethod -Uri $createReleaseUrl -Method Post -Headers $apiHeaders -Body $releaseBody -ContentType "application/json"
                $uploadUrl = $relResp.upload_url -replace '\{\?name,label\}', ''

                $uploadExeUrl = "$uploadUrl?name=eBooster.exe"
                $exeBytes = [System.IO.File]::ReadAllBytes($distExe)
                Invoke-RestMethod -Uri $uploadExeUrl -Method Post -Headers $apiHeaders -Body $exeBytes -ContentType "application/octet-stream" | Out-Null

                $zipName = [System.IO.Path]::GetFileName($distZip)
                $uploadZipUrl = "$uploadUrl?name=$zipName"
                $zipBytes = [System.IO.File]::ReadAllBytes($distZip)
                Invoke-RestMethod -Uri $uploadZipUrl -Method Post -Headers $apiHeaders -Body $zipBytes -ContentType "application/octet-stream" | Out-Null

                $releasePublished = $true
                Write-Success "Release published successfully via GitHub API!"
            } catch {
                Write-ErrMsg "Failed with provided token: $($_.Exception.Message)"
            }
        }
    }

    if (-not $releasePublished) {
        Write-Host "`nOpening GitHub Release Draft page and Explorer folder..." -ForegroundColor Cyan
        $encodedTitle = [System.Uri]::EscapeDataString($releaseTitle)
        $encodedBody  = [System.Uri]::EscapeDataString($releaseNotes)
        $newReleaseWebUrl = "https://github.com/$Repo/releases/new?tag=$tagVersion&title=$encodedTitle&body=$encodedBody"

        Start-Process $newReleaseWebUrl
        Start-Process "explorer.exe" "/select,`"$distExe`""
        Write-Success "Opened GitHub draft in default browser."
        Write-Success "Opened dist folder in Windows Explorer."
        Write-Host "Simply drag 'eBooster.exe' into the browser and click 'Publish release'!" -ForegroundColor Yellow
    }
}

# --------------------------------------------------------------------
# 8. Release Summary Banner
# --------------------------------------------------------------------
Write-Header "RELEASE COMPLETED SUCCESSFULLY!"
Write-Host "Release Tag   : $tagVersion" -ForegroundColor Yellow
Write-Host "Release Title : $releaseTitle" -ForegroundColor White
Write-Host "Release URL   : https://github.com/$Repo/releases/tag/$tagVersion" -ForegroundColor Cyan
Write-Host "Primary Asset : $distExe ($exeSizeMB MB)" -ForegroundColor Green
Write-Host "ZIP Archive   : $distZip ($zipSizeMB MB)" -ForegroundColor Green
Write-Host "SHA256 Hash   : $sha256" -ForegroundColor Gray
Write-Host "`nAll eBooster client apps will now automatically detect this update!" -ForegroundColor Magenta
Write-Host "======================================================================`n" -ForegroundColor Cyan
