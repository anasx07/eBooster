cmake_minimum_required(VERSION 3.25)
project(eBooster VERSION 1.0.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Unicode Win32 APIs and lean Windows headers
add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)

set(RESOURCE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /permissive- /utf-8 /EHsc)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

# Header includes
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/include
)

# Common engine sources
set(CORE_SOURCES
    src/Common.cpp
    src/ProcessMonitor.cpp
    src/ConnectionSniffer.cpp
    src/EtwSniffer.cpp
    src/RouteController.cpp
    src/DnsRelayController.cpp
    src/ProfileManager.cpp
    src/SteamDetector.cpp
    src/BoosterEngine.cpp
    src/NetworkOptimizer.cpp
    src/UserPreferences.cpp
    src/MatchSniffer.cpp
    src/InGameOverlay.cpp
    src/UpdateManager.cpp
)

# -------------------------------------------------------------
# Modern Desktop GUI Target (eBooster - Native C++23 + WebView2)
# -------------------------------------------------------------
add_executable(eBooster WIN32
    ${CORE_SOURCES}
    ${RESOURCE_SOURCES}
    src/WebViewApp.cpp
    src/gui_main.cpp
)

target_link_libraries(eBooster PRIVATE
    ws2_32
    iphlpapi
    advapi32
    tdh
    dnsapi
    dwmapi
    windowscodecs
    ole32
    oleaut32
    shlwapi
    crypt32
    winhttp
    shell32
    version
    urlmon
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2/x64/WebView2LoaderStatic.lib
)

# -------------------------------------------------------------
# Post-Build: Auto-sync configs/ and ui/dist/ to output binaries
# -------------------------------------------------------------
add_custom_command(TARGET eBooster POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/configs"
        "$<TARGET_FILE_DIR:eBooster>/configs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/dist"
        "$<TARGET_FILE_DIR:eBooster>/ui/dist"
    COMMENT "Auto-syncing configs/ and ui/dist/ to eBooster output directory"
)
