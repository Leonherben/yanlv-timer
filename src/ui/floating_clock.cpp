#include "src/ui/floating_clock.h"
#include "src/ui/d2d_renderer.h"
#include "src/core/timer_engine.h"
#include "src/db/repository.h"
#include <windowsx.h>
#include <cwchar>
#include <algorithm>

namespace yanlv {

namespace {
const wchar_t* const FLOATING_WINDOW_CLASS = L"YanlvFloatingClockClass";

enum MenuCommand {
    ID_MENU_START = 1001,
    ID_MENU_PAUSE_RESUME,
    ID_MENU_ABORT,
    ID_MENU_HIDE,
    ID_MENU_MANAGEMENT,
    ID_MENU_ALWAYS_TOP,
    ID_MENU_EXIT
};
} // namespace

FloatingClock& FloatingClock::Instance() {
    static FloatingClock instance;
    return instance;
}

FloatingClock::FloatingClock() = default;

FloatingClock::~FloatingClock() {
    if (m_dcRenderTarget) {
        m_dcRenderTarget->Release();
        m_dcRenderTarget = nullptr;
    }
    if (m_brushBg) m_brushBg->Release();
    if (m_brushBorder) m_brushBorder->Release();
    if (m_brushText) m_brushText->Release();
    if (m_brushAccent) m_brushAccent->Release();
    if (m_textFormatTime) m_textFormatTime->Release();
    if (m_textFormatStatus) m_textFormatStatus->Release();

    if (m_hBitmap) DeleteObject(m_hBitmap);
    if (m_memDC) DeleteDC(m_memDC);

    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

bool FloatingClock::Create() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = FLOATING_WINDOW_CLASS;

    RegisterClassExW(&wc);

    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
    if (m_alwaysOnTop) {
        exStyle |= WS_EX_TOPMOST;
    }

    m_hWnd = CreateWindowExW(
        exStyle,
        FLOATING_WINDOW_CLASS,
        L"言律时钟 - 悬浮",
        WS_POPUP,
        m_posX, m_posY, m_width, m_height,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) return false;

    // 创建内存 DC 与 DIBSection
    HDC screenDC = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_width;
    bmi.bmiHeader.biHeight = -m_height; // 自上而下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(m_memDC, &bmi, DIB_RGB_COLORS, &m_bitmapBits, nullptr, 0);
    SelectObject(m_memDC, m_hBitmap);

    // 创建 Direct2D DC Render Target
    if (D2DRenderer::Instance().Initialize()) {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0,
            D2D1_RENDER_TARGET_USAGE_NONE,
            D2D1_FEATURE_LEVEL_DEFAULT
        );

        D2DRenderer::Instance().GetD2DFactory()->CreateDCRenderTarget(&props, &m_dcRenderTarget);

        if (m_dcRenderTarget) {
            RECT rc{0, 0, m_width, m_height};
            m_dcRenderTarget->BindDC(m_memDC, &rc);

            m_dcRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.09f, 0.12f, 0.85f), &m_brushBg);
            m_dcRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.18f), &m_brushBorder);
            m_dcRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.96f, 0.96f, 0.98f, 1.0f), &m_brushText);
            m_dcRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.30f, 0.82f, 0.88f, 1.0f), &m_brushAccent);

            m_textFormatTime = D2DRenderer::Instance().CreateTextFormat(
                L"Consolas", 22.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER
            );
            m_textFormatStatus = D2DRenderer::Instance().CreateTextFormat(
                L"Microsoft YaHei", 10.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER
            );
        }
    }

    Render();
    return true;
}

void FloatingClock::Show() {
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
        Render();
    }
}

void FloatingClock::Hide() {
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_HIDE);
    }
}

void FloatingClock::ToggleVisibility() {
    if (IsVisible()) {
        Hide();
    } else {
        Show();
    }
}

bool FloatingClock::IsVisible() const {
    return m_hWnd && IsWindowVisible(m_hWnd);
}

void FloatingClock::SetPosition(int x, int y) {
    m_posX = x;
    m_posY = y;
    if (m_hWnd) {
        SetWindowPos(m_hWnd, nullptr, m_posX, m_posY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        Render();
    }
}

void FloatingClock::GetPosition(int& x, int& y) const {
    x = m_posX;
    y = m_posY;
}

void FloatingClock::SetAlwaysOnTop(bool onTop) {
    m_alwaysOnTop = onTop;
    if (m_hWnd) {
        SetWindowPos(
            m_hWnd,
            onTop ? HWND_TOPMOST : HWND_NOTOPMOST,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
        );
    }
}

void FloatingClock::UpdateDisplay(int64_t remainingSeconds, TimerState state) {
    m_currentSeconds = remainingSeconds;
    m_currentState = state;

    int64_t hrs = remainingSeconds / 3600;
    int64_t mins = (remainingSeconds % 3600) / 60;
    int64_t secs = remainingSeconds % 60;

    wchar_t buf[32];
    if (hrs > 0) {
        swprintf_s(buf, L"%02lld:%02lld:%02lld", hrs, mins, secs);
    } else {
        swprintf_s(buf, L"%02lld:%02lld", mins, secs);
    }
    m_timeString = buf;

    switch (state) {
    case TimerState::Idle:
        m_statusString = L"待开始";
        if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.30f, 0.82f, 0.88f, 1.0f)); // 青蓝
        break;
    case TimerState::Studying:
        m_statusString = L"专注中";
        if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.26f, 0.65f, 0.96f, 1.0f)); // 宁静蓝
        break;
    case TimerState::Paused:
        m_statusString = L"已暂停";
        if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(1.0f, 0.65f, 0.15f, 1.0f)); // 暖琥珀
        break;
    case TimerState::BreakPending:
        m_statusString = L"待休息";
        if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.40f, 0.80f, 0.40f, 1.0f)); // 柔绿
        break;
    case TimerState::Breaking:
        m_statusString = L"休息中";
        if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.35f, 0.85f, 0.45f, 1.0f)); // 翡翠绿
        break;
    }

    Render();
}

void FloatingClock::Render() {
    if (!m_hWnd || !m_dcRenderTarget) return;

    RECT rc{0, 0, m_width, m_height};
    m_dcRenderTarget->BindDC(m_memDC, &rc);

    m_dcRenderTarget->BeginDraw();
    m_dcRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 绘制圆角胶囊背景
    D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(
        D2D1::RectF(2.0f, 2.0f, static_cast<float>(m_width) - 2.0f, static_cast<float>(m_height) - 2.0f),
        16.0f, 16.0f
    );
    m_dcRenderTarget->FillRoundedRectangle(rrect, m_brushBg);
    m_dcRenderTarget->DrawRoundedRectangle(rrect, m_brushBorder, 1.2f);

    // 绘制左侧状态指示点
    float dotX = 18.0f;
    float dotY = static_cast<float>(m_height) / 2.0f;
    m_dcRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), 4.5f, 4.5f), m_brushAccent);

    // 绘制状态文字 (点下方微标或旁边)
    D2D1_RECT_F statusRect = D2D1::RectF(26.0f, 6.0f, 62.0f, static_cast<float>(m_height) - 6.0f);
    m_dcRenderTarget->DrawText(
        m_statusString.c_str(),
        static_cast<UINT32>(m_statusString.size()),
        m_textFormatStatus,
        statusRect,
        m_brushAccent
    );

    // 绘制时间文本 (居右侧主体区域)
    D2D1_RECT_F timeRect = D2D1::RectF(58.0f, 2.0f, static_cast<float>(m_width) - 8.0f, static_cast<float>(m_height) - 2.0f);
    m_dcRenderTarget->DrawText(
        m_timeString.c_str(),
        static_cast<UINT32>(m_timeString.size()),
        m_textFormatTime,
        timeRect,
        m_brushText
    );

    m_dcRenderTarget->EndDraw();

    // 更新分层窗口
    POINT ptSrc{0, 0};
    SIZE sz{m_width, m_height};
    POINT ptDst{m_posX, m_posY};
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(m_hWnd, nullptr, &ptDst, &sz, m_memDC, &ptSrc, 0, &blend, ULW_ALPHA);
}

void FloatingClock::ShowContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    TimerState st = TimerEngine::Instance().GetState();

    if (st == TimerState::Idle) {
        AppendMenuW(hMenu, MF_STRING, ID_MENU_START, L"开始学习 (Enter)");
    } else if (st == TimerState::Studying) {
        AppendMenuW(hMenu, MF_STRING, ID_MENU_PAUSE_RESUME, L"暂停学习 (Space)");
        AppendMenuW(hMenu, MF_STRING, ID_MENU_ABORT, L"提前结束本轮");
    } else if (st == TimerState::Paused) {
        AppendMenuW(hMenu, MF_STRING, ID_MENU_PAUSE_RESUME, L"继续学习 (Space)");
        AppendMenuW(hMenu, MF_STRING, ID_MENU_ABORT, L"提前结束本轮");
    } else if (st == TimerState::Breaking) {
        AppendMenuW(hMenu, MF_STRING, ID_MENU_ABORT, L"结束本次休息");
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_MANAGEMENT, L"控制中心 (统计与记录)...");
    AppendMenuW(hMenu, MF_STRING | (m_alwaysOnTop ? MF_CHECKED : MF_UNCHECKED), ID_MENU_ALWAYS_TOP, L"窗口置顶");
    AppendMenuW(hMenu, MF_STRING, ID_MENU_HIDE, L"隐藏时钟 (可在托盘唤醒)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_MENU_EXIT, L"退出言律时钟");

    SetForegroundWindow(m_hWnd);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screenX, screenY, 0, m_hWnd, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
    case ID_MENU_START:
        if (m_onQuickStart) m_onQuickStart();
        break;
    case ID_MENU_PAUSE_RESUME:
        if (st == TimerState::Studying) TimerEngine::Instance().Pause();
        else if (st == TimerState::Paused) TimerEngine::Instance().Resume();
        break;
    case ID_MENU_ABORT:
        if (st == TimerState::Breaking) {
            TimerEngine::Instance().SkipBreak();
        } else {
            TimerEngine::Instance().Abort();
        }
        break;
    case ID_MENU_MANAGEMENT:
        if (m_onOpenManagement) m_onOpenManagement();
        break;
    case ID_MENU_ALWAYS_TOP:
        SetAlwaysOnTop(!m_alwaysOnTop);
        break;
    case ID_MENU_HIDE:
        Hide();
        break;
    case ID_MENU_EXIT:
        PostQuitMessage(0);
        break;
    }
}

LRESULT CALLBACK FloatingClock::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    FloatingClock* self = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<FloatingClock*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<FloatingClock*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT FloatingClock::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_LBUTTONDOWN: {
        m_mouseDownPos.x = GET_X_LPARAM(lParam);
        m_mouseDownPos.y = GET_Y_LPARAM(lParam);
        m_isDragging = false;
        SetCapture(hWnd);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (GetCapture() == hWnd) {
            int curX = GET_X_LPARAM(lParam);
            int curY = GET_Y_LPARAM(lParam);
            int dx = curX - m_mouseDownPos.x;
            int dy = curY - m_mouseDownPos.y;
            if (abs(dx) > 3 || abs(dy) > 3) {
                m_isDragging = true;
                m_posX += dx;
                m_posY += dy;
                SetPosition(m_posX, m_posY);
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (GetCapture() == hWnd) {
            ReleaseCapture();
            if (!m_isDragging) {
                // 单击响应逻辑
                TimerState st = TimerEngine::Instance().GetState();
                if (st == TimerState::Idle) {
                    if (m_onQuickStart) m_onQuickStart();
                } else if (st == TimerState::Studying) {
                    TimerEngine::Instance().Pause();
                } else if (st == TimerState::Paused) {
                    TimerEngine::Instance().Resume();
                }
            }
        }
        return 0;
    }
    case WM_RBUTTONUP: {
        POINT pt;
        GetCursorPos(&pt);
        ShowContextMenu(pt.x, pt.y);
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

} // namespace yanlv
