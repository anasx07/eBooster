@echo off
setlocal
cd /d "%~dp0\.."
title eBooster Pro - Release Builder
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_release.ps1" %*
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build release failed with exit code %ERRORLEVEL%.
    pause
    exit /b %ERRORLEVEL%
)
pause
