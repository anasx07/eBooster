#include "ebooster/WebViewApp.hpp"
#include "ebooster/RouteController.hpp"
#include "ebooster/DnsRelayController.hpp"
#include "ebooster/NetworkOptimizer.hpp"
#include <windows.h>
#include <shellapi.h>
#include <string_view>

static LONG WINAPI EmergencyCrashFilter(EXCEPTION_POINTERS* pEx) {
    (void)pEx;
    ebooster::DnsRelayController::EmergencyCleanup();
    ebooster::NetworkOptimizer::SetEFootballAntiLagShield(false);
    ebooster::NetworkOptimizer::SetEFootballGeofilter(ebooster::KonamiRegion::Auto_All);
    return EXCEPTION_CONTINUE_SEARCH;
}

static int RunGuiApplication(bool startMinimized) {
    // Enable Per-Monitor V2 High-DPI Awareness
    typedef BOOL (WINAPI* PFN_SetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
    auto pfnSetProcessDpiAwarenessContext = reinterpret_cast<PFN_SetProcessDpiAwarenessContext>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext")));
    if (pfnSetProcessDpiAwarenessContext) {
        pfnSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    // Register top-level crash filter to clean up state even on unexpected crash
    SetUnhandledExceptionFilter(EmergencyCrashFilter);

    // Enforce Single-Instance execution to prevent competing WFP engines
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\eBooster_Pro_SingleInstance_Mutex");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"eBoosterWebViewHostClass", nullptr);
        if (hExisting) {
            ShowWindow(hExisting, SW_SHOW);
            ShowWindow(hExisting, SW_RESTORE);
            SetForegroundWindow(hExisting);
        }
        CloseHandle(hMutex);
        return 0;
    }

    // Initialize COM for WebView2
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    ebooster::WebViewApp app;
    app.SetCmdLineMinimized(startMinimized);

    int showCmd = startMinimized ? SW_HIDE : SW_SHOWDEFAULT;
    if (!app.Initialize(hInstance, showCmd)) {
        CoUninitialize();
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    app.Run();
    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);
    return 0;
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nShowCmd*/) {
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool startMinimized = false;
    if (argvW) {
        for (int i = 1; i < argc; ++i) {
            std::wstring_view arg = argvW[i];
            if (arg == L"--minimized" || arg == L"--tray" || arg == L"-tray" || arg == L"/minimized") {
                startMinimized = true;
                break;
            }
        }
        LocalFree(argvW);
    }
    return RunGuiApplication(startMinimized);
}
#endif

int main(int argc, char* argv[]) {
    bool startMinimized = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--minimized" || arg == "--tray" || arg == "-tray" || arg == "/minimized") {
            startMinimized = true;
            break;
        }
    }
    return RunGuiApplication(startMinimized);
}
