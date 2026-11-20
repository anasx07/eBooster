@echo off
setlocal
cd /d "%~dp0\.."

echo [eBooster] Configuring with Visual Studio 2026 / MSVC (x64)...
cmake -G "Visual Studio 18 2026" -A x64 -B build_msvc -S .
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo [eBooster] Compiling Release build...
cmake --build build_msvc --config Release --parallel
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed.
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Build completed: build_msvc\Release\eBooster.exe
echo Run as Administrator:
echo   build_msvc\Release\eBooster.exe
echo.
pause
