# 🛠️ eBooster Pro - DevOps & Distribution Guide

This guide explains how to enable **Authenticode Code Signing** (Windows SmartScreen bypass) and **Automated Winget Publishing** (`winget install anasx07.eBooster`).

---

## 1. 🔏 Code Signing (Windows Authenticode & SmartScreen)

When users download `.exe` files from the internet, Windows Defender SmartScreen shows an *"Unknown Publisher"* warning unless the executable is digitally signed with an Authenticode certificate.

The release workflow `.github/workflows/release.yml` is **pre-wired and ready** for automatic code signing.

### How to Activate Automatic Code Signing:
1. Obtain an Authenticode code signing certificate (`.pfx` file) from a trusted CA (e.g., Certum, Sectigo, DigiCert, or SignPath Foundation).
2. Convert your `.pfx` file to a Base64 string in PowerShell:
   ```powershell
   [Convert]::ToBase64String([System.IO.File]::ReadAllBytes("path\to\your_certificate.pfx")) | Set-Clipboard
   ```
3. Go to your GitHub Repository:
   - **Settings** → **Secrets and variables** → **Actions** → Click **New repository secret**.
4. Add the following two secrets:
   - **Secret 1**:
     - Name: `CERTIFICATE_BASE64`
     - Value: *(Paste the Base64 clipboard content)*
   - **Secret 2**:
     - Name: `CERTIFICATE_PASSWORD`
     - Value: *(Your certificate's export password)*

> **Result**: On the next release, GitHub Actions will automatically sign both `eBooster-Setup.exe` and `eBooster-Portable.exe` using `signtool.exe` with dual SHA-256 and DigiCert RFC 3161 timestamps! If these secrets are not set, signing is safely skipped.

---

## 2. 🌐 Windows Package Manager (`winget install anasx07.eBooster`)

You can allow millions of Windows 10 and 11 users to install and update eBooster directly via the command line:
```powershell
winget install anasx07.eBooster
winget upgrade anasx07.eBooster
```

The release workflow uses `vedantmgoyal2009/winget-releaser@v2` to automatically submit manifests to Microsoft's official `microsoft/winget-pkgs` repository on every release.

### How to Activate Winget Automation:
1. Create a GitHub Personal Access Token (PAT):
   - Navigate to: **GitHub Settings** → **Developer settings** → **Personal access tokens** → **Tokens (classic)** (or visit: `https://github.com/settings/tokens/new`).
   - Note / Name: `eBooster Winget Releaser`
   - Expiration: Choose desired duration (e.g. 1 year or No expiration).
   - Select scope: **`public_repo`** (required to fork `microsoft/winget-pkgs` and submit PRs).
   - Click **Generate token** and copy the token string (`ghp_...`).
2. Add the token to Repository Secrets:
   - Go to your repository: **Settings** → **Secrets and variables** → **Actions** → **New repository secret**.
   - Name: **`WINGET_TOKEN`**
   - Value: *(Paste your `ghp_...` token)*

> **Result**: On every future release, GitHub Actions will automatically calculate the installer's SHA-256, format the Microsoft Winget manifest YAML, and submit a Pull Request to `microsoft/winget-pkgs` on your behalf!
