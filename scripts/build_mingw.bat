@echo off
setlocal
cd /d "%~dp0\.."

set "PATH=C:\winlibs\mingw64\bin;%PATH%"

echo [eBooster] Configuring with MinGW GCC (C++23)...
cmake -G "MinGW Makefiles" -B build_mingw -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] MinGW CMake configuration failed.
    exit /b %ERRORLEVEL%
)

echo [eBooster] Compiling Release build...
cmake --build build_mingw --parallel
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed.
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Build completed: build_mingw\eBooster.exe
echo Run as Administrator:
echo   build_mingw\eBooster.exe
echo.
pause
