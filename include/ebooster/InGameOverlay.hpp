#pragma once

#include "ebooster/Common.hpp"
#include <string>
#include <atomic>
#include <mutex>
#include <windows.h>

namespace ebooster {

enum class OverlayPosition {
    TopRight = 0,
    TopLeft = 1,
    TopCenter = 2,
    BottomRight = 3
};

struct OverlayData {
    float pingMs{35.0f};
    std::string serverLocation{"Madrid"};
    float packetLossPct{0.0f};
    float jitterMs{0.4f};
    uint32_t packetRate{128};
    std::string protocol{"Pure UDP"};
    bool isBoosting{true};
    bool isMatchActive{true};
};

class InGameOverlay {
public:
    InGameOverlay();
    ~InGameOverlay();

    InGameOverlay(const InGameOverlay&) = delete;
    InGameOverlay& operator=(const InGameOverlay&) = delete;

    bool Initialize(HINSTANCE hInstance, HWND hParent = nullptr);
    void Shutdown();

    void Show();
    void Hide();
    void Toggle();
    bool IsVisible() const noexcept { return m_isVisible.load(); }

    void SetPosition(OverlayPosition pos);
    OverlayPosition GetPosition() const noexcept { return m_position; }

    void SetClickThrough(bool clickThrough);
    bool IsClickThrough() const noexcept { return m_clickThrough; }

    void SetDpiScale(float scale);
    float GetDpiScale() const noexcept { return m_dpiScale; }

    void UpdateTelemetry(const OverlayData& data);
    void Redraw();

    HWND GetHwnd() const noexcept { return m_hWnd; }

private:
    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void Paint(HDC hdc);
    void Reposition();
    void RecreateFonts();

    HINSTANCE m_hInstance{nullptr};
    HWND m_hWnd{nullptr};
    std::atomic<bool> m_isVisible{false};
    bool m_clickThrough{true};
    OverlayPosition m_position{OverlayPosition::TopRight};
    float m_dpiScale{1.0f};

    mutable std::mutex m_dataMutex;
    OverlayData m_data;

    int m_baseWidth{350};
    int m_baseHeight{36};

    HFONT m_hFont{nullptr};
    HFONT m_hBoldFont{nullptr};
};

} // namespace ebooster
