cmake_minimum_required(VERSION 3.25)
project(eBooster VERSION 1.0.1 LANGUAGES CXX)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
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
    src/TextureLoader.cpp
    src/BoosterEngine.cpp
    src/NetworkOptimizer.cpp
    src/UserPreferences.cpp
    src/MatchSniffer.cpp
    src/InGameOverlay.cpp
    src/UpdateManager.cpp
)

# -------------------------------------------------------------
# Modern Desktop GUI Target (eBooster - DirectX 11 + Dear ImGui)
# -------------------------------------------------------------
set(IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp
)

add_executable(eBooster WIN32
    ${CORE_SOURCES}
    ${RESOURCE_SOURCES}
    ${IMGUI_SOURCES}
    src/GuiApp.cpp
    src/gui_main.cpp
)

target_link_libraries(eBooster PRIVATE
    ws2_32
    iphlpapi
    advapi32
    tdh
    dnsapi
    d3d11
    d3dcompiler
    dxgi
    dwmapi
    windowscodecs
    urlmon
    ole32
    shlwapi
    crypt32
    winhttp
)

# -------------------------------------------------------------
# Post-Build: Auto-sync configs/ directory to output binaries
# -------------------------------------------------------------
add_custom_command(TARGET eBooster POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/configs"
        "$<TARGET_FILE_DIR:eBooster>/configs"
    COMMENT "Auto-syncing configs/ to eBooster output directory"
)
