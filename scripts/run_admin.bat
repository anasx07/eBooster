@echo off
setlocal
cd /d "%~dp0\.."

:: Check if already running as Administrator
net session >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    goto :RunExe
)

echo [eBooster] Administrator privileges required for routing manipulation.
echo Requesting UAC elevation...
powershell -Command "Start-Process cmd.exe -ArgumentList '/k \"\"%~f0\"\"' -Verb RunAs"
exit /b

:RunExe
set "EXE_PATH="
if exist "build_mingw\eBooster.exe" (
    set "EXE_PATH=build_mingw\eBooster.exe"
) else if exist "build_msvc\Release\eBooster.exe" (
    set "EXE_PATH=build_msvc\Release\eBooster.exe"
) else if exist "dist\eBooster-v1.0-Release\eBooster.exe" (
    set "EXE_PATH=dist\eBooster-v1.0-Release\eBooster.exe"
)

if "%EXE_PATH%"=="" (
    echo [ERROR] No compiled eBooster.exe found! Please build the project first.
    pause
    exit /b 1
)

echo Starting %EXE_PATH% as Administrator...
start "" "%EXE_PATH%"
exit /b
