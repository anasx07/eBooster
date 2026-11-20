#include "ebooster/InGameOverlay.hpp"
#include <format>
#include <cmath>

namespace ebooster {

InGameOverlay::InGameOverlay() = default;

InGameOverlay::~InGameOverlay() {
    Shutdown();
}

bool InGameOverlay::Initialize(HINSTANCE hInstance, HWND hParent) {
    m_hInstance = hInstance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpszClassName = L"eBoosterInGameOverlayClass";

    RegisterClassExW(&wc);

    DWORD dwExStyle = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE;
    if (m_clickThrough) {
        dwExStyle |= WS_EX_TRANSPARENT;
    }
    DWORD dwStyle = WS_POPUP;

    int w = static_cast<int>(m_baseWidth * m_dpiScale);
    int h = static_cast<int>(m_baseHeight * m_dpiScale);

    m_hWnd = CreateWindowExW(
        dwExStyle,
        wc.lpszClassName,
        L"eBooster In-Game OSD",
        dwStyle,
        0, 0, w, h,
        hParent,
        nullptr,
        m_hInstance,
        this
    );

    if (!m_hWnd) {
        return false;
    }

    // Set transparency color key RGB(1, 1, 1) and 95% opacity (242/255)
    SetLayeredWindowAttributes(m_hWnd, RGB(1, 1, 1), 242, LWA_COLORKEY | LWA_ALPHA);

    RecreateFonts();
    Reposition();

    return true;
}

void InGameOverlay::Shutdown() {
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }

    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }

    if (m_hBoldFont) {
        DeleteObject(m_hBoldFont);
        m_hBoldFont = nullptr;
    }

    if (m_hInstance) {
        UnregisterClassW(L"eBoosterInGameOverlayClass", m_hInstance);
    }
}

void InGameOverlay::Show() {
    m_isVisible.store(true);
    if (m_hWnd) {
        Reposition();
        ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
        UpdateWindow(m_hWnd);
        Redraw();
    }
}

void InGameOverlay::Hide() {
    m_isVisible.store(false);
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_HIDE);
    }
}

void InGameOverlay::Toggle() {
    if (m_isVisible.load()) {
        Hide();
    } else {
        Show();
    }
}

void InGameOverlay::SetPosition(OverlayPosition pos) {
    m_position = pos;
    if (m_hWnd) {
        Reposition();
    }
}

void InGameOverlay::SetClickThrough(bool clickThrough) {
    m_clickThrough = clickThrough;
    if (m_hWnd) {
        LONG_PTR exStyle = GetWindowLongPtr(m_hWnd, GWL_EXSTYLE);
        if (clickThrough) {
            exStyle |= WS_EX_TRANSPARENT;
        } else {
            exStyle &= ~WS_EX_TRANSPARENT;
        }
        SetWindowLongPtr(m_hWnd, GWL_EXSTYLE, exStyle);
        SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

void InGameOverlay::SetDpiScale(float scale) {
    if (std::abs(m_dpiScale - scale) > 0.01f) {
        m_dpiScale = scale;
        RecreateFonts();
        if (m_hWnd) {
            Reposition();
            Redraw();
        }
    }
}

void InGameOverlay::UpdateTelemetry(const OverlayData& data) {
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        m_data = data;
    }
    if (m_isVisible.load() && m_hWnd) {
        Redraw();
    }
}

void InGameOverlay::Redraw() {
    if (m_hWnd) {
        InvalidateRect(m_hWnd, nullptr, FALSE);
    }
}

void InGameOverlay::RecreateFonts() {
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
    if (m_hBoldFont) {
        DeleteObject(m_hBoldFont);
        m_hBoldFont = nullptr;
    }

    int fontSize = static_cast<int>(std::round(13.0f * m_dpiScale));
    if (fontSize < 11) fontSize = 11;

    m_hFont = CreateFontW(
        -fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI"
    );

    m_hBoldFont = CreateFontW(
        -fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI"
    );
}

void InGameOverlay::Reposition() {
    if (!m_hWnd) return;

    int w = static_cast<int>(m_baseWidth * m_dpiScale);
    int h = static_cast<int>(m_baseHeight * m_dpiScale);
    int margin = static_cast<int>(18 * m_dpiScale);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int x = margin;
    int y = margin;

    switch (m_position) {
        case OverlayPosition::TopRight:
            x = screenW - w - margin;
            y = margin;
            break;
        case OverlayPosition::TopLeft:
            x = margin;
            y = margin;
            break;
        case OverlayPosition::TopCenter:
            x = (screenW - w) / 2;
            y = margin;
            break;
        case OverlayPosition::BottomRight:
            x = screenW - w - margin;
            y = screenH - h - margin;
            break;
    }

    SetWindowPos(
        m_hWnd,
        HWND_TOPMOST,
        x, y, w, h,
        SWP_NOACTIVATE | (m_isVisible.load() ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)
    );
}

LRESULT CALLBACK InGameOverlay::WndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    InGameOverlay* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto* pCreate = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = reinterpret_cast<InGameOverlay*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<InGameOverlay*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->WndProc(uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT InGameOverlay::WndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hWnd, &ps);
            Paint(hdc);
            EndPaint(m_hWnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1; // Handled in double buffer

        case WM_NCHITTEST: {
            if (!m_clickThrough) {
                // If click-through is disabled, allow dragging anywhere on the pill
                return HTCAPTION;
            }
            return HTTRANSPARENT;
        }
        case WM_DESTROY:
            m_hWnd = nullptr;
            return 0;
    }
    return DefWindowProcW(m_hWnd, uMsg, wParam, lParam);
}

void InGameOverlay::Paint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    OverlayData data;
    {
        std::lock_guard<std::mutex> lock(m_dataMutex);
        data = m_data;
    }

    // 1. Create Double Buffering Memory DC
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    auto* oldBmp = reinterpret_cast<HBITMAP>(SelectObject(memDC, memBmp));

    // 2. Fill background with transparency color key RGB(1, 1, 1)
    COLORREF colorKey = RGB(1, 1, 1);
    HBRUSH hKeyBrush = CreateSolidBrush(colorKey);
    FillRect(memDC, &rc, hKeyBrush);
    DeleteObject(hKeyBrush);

    // 3. Draw Cyberpunk Dark Frosted Glass Capsule
    int radius = static_cast<int>(14 * m_dpiScale);
    HRGN hRgn = CreateRoundRectRgn(0, 0, width, height, radius, radius);

    // Dark slate body RGB(11, 14, 20)
    HBRUSH hBgBrush = CreateSolidBrush(RGB(11, 14, 20));
    FillRgn(memDC, hRgn, hBgBrush);
    DeleteObject(hBgBrush);

    // Neon glowing border outline
    COLORREF borderColor = data.isBoosting ? RGB(0, 220, 255) : RGB(70, 80, 95);
    HBRUSH hBorderBrush = CreateSolidBrush(borderColor);
    FrameRgn(memDC, hRgn, hBorderBrush, 1, 1);
    DeleteObject(hBorderBrush);
    DeleteObject(hRgn);

    // 4. Glowing Status Indicator Dot (Left)
    int dotX = static_cast<int>(13 * m_dpiScale);
    int dotY = height / 2;
    int dotR = static_cast<int>(3.5f * m_dpiScale);
    COLORREF dotColor = data.isBoosting ? RGB(0, 245, 140) : RGB(100, 110, 125);
    HBRUSH hDotBrush = CreateSolidBrush(dotColor);
    SelectObject(memDC, GetStockObject(NULL_PEN));
    auto* oldBrush = reinterpret_cast<HBRUSH>(SelectObject(memDC, hDotBrush));
    Ellipse(memDC, dotX - dotR, dotY - dotR, dotX + dotR, dotY + dotR);
    SelectObject(memDC, oldBrush);
    DeleteObject(hDotBrush);

    // 5. Draw Formatted Telemetry Elements
    SetBkMode(memDC, TRANSPARENT);
    auto* oldFont = reinterpret_cast<HFONT>(SelectObject(memDC, m_hFont));

    int curX = dotX + dotR + static_cast<int>(9 * m_dpiScale);
    int textY = (height - static_cast<int>(16 * m_dpiScale)) / 2;

    auto DrawSegment = [&](const std::wstring& text, COLORREF color, bool bold = false) {
        if (bold && m_hBoldFont) {
            SelectObject(memDC, m_hBoldFont);
        } else if (m_hFont) {
            SelectObject(memDC, m_hFont);
        }
        SetTextColor(memDC, color);
        TextOutW(memDC, curX, textY, text.c_str(), static_cast<int>(text.length()));
        SIZE sz{};
        GetTextExtentPoint32W(memDC, text.c_str(), static_cast<int>(text.length()), &sz);
        curX += sz.cx;
    };

    COLORREF colMuted = RGB(95, 105, 125);
    COLORREF colCyan  = RGB(0, 225, 255);
    COLORREF colMint  = RGB(0, 245, 140);
    COLORREF colAmber = RGB(245, 185, 60);
    COLORREF colRed   = RGB(255, 75, 75);

    // Ping value
    COLORREF pingColor = (data.pingMs < 55.0f) ? colMint : ((data.pingMs < 85.0f) ? colAmber : colRed);
    std::wstring pingText = std::format(L"⚡ {:.0f}ms", data.pingMs);
    DrawSegment(pingText, pingColor, true);

    // Location / Cluster
    if (!data.serverLocation.empty()) {
        std::wstring locText = L" (" + ToWideString(data.serverLocation) + L")";
        DrawSegment(locText, colAmber, false);
    }

    // Separator
    DrawSegment(L"  ·  ", colMuted, false);

    // Packet Loss
    COLORREF lossColor = (data.packetLossPct == 0.0f) ? colMint : colRed;
    std::wstring lossText = (data.packetLossPct == 0.0f) ? L"0% Loss" : std::format(L"{:.1f}% Loss", data.packetLossPct);
    DrawSegment(lossText, lossColor, true);

    // Separator
    DrawSegment(L"  ·  ", colMuted, false);

    // Protocol
    DrawSegment(L"Pure UDP", colCyan, true);

    // Tickrate (if space permits)
    if (data.packetRate > 0 && width > static_cast<int>(320 * m_dpiScale)) {
        DrawSegment(L"  ·  ", colMuted, false);
        std::wstring rateText = std::format(L"{} pkts/s", data.packetRate);
        DrawSegment(rateText, colMint, false);
    }

    // 6. Blit from memory DC to window DC
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    // Cleanup GDI objects
    SelectObject(memDC, oldFont);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

} // namespace ebooster
