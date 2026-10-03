#include "src/ui/floating_clock.h"
#include "src/ui/d2d_renderer.h"
#include "src/core/timer_engine.h"
#include "src/db/repository.h"
#include "src/utils/updater.h"
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
    ID_MENU_CHECK_UPDATE,
    ID_MENU_EXIT
};
} // namespace

FloatingClock& FloatingClock::Instance() {
    static FloatingClock instance;
    return instance;
}

FloatingClock::FloatingClock() = default;

FloatingClock::~FloatingClock() {
    if (m_hWnd) {
        KillTimer(m_hWnd, ID_TOPMOST_TIMER);
    }
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

    if (m_alwaysOnTop) {
        SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    SetTimer(m_hWnd, ID_TOPMOST_TIMER, 250, nullptr);

    RecreateBitmapAndTarget(m_width, m_height);

    Render();
    return true;
}

void FloatingClock::EnsureTopmost() {
    if (m_alwaysOnTop && m_hWnd && IsWindowVisible(m_hWnd)) {
        HWND hPrev = GetWindow(m_hWnd, GW_HWNDPREV);
        if (hPrev != nullptr) {
            SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
}

void FloatingClock::RecreateBitmapAndTarget(int width, int height) {
    bool sizeChanged = (m_width != width || m_height != height);
    m_width = width;
    m_height = height;

    if (m_dcRenderTarget && !sizeChanged) {
        if (m_textFormatTime) {
            m_textFormatTime->Release();
            m_textFormatTime = nullptr;
        }
        m_textFormatTime = D2DRenderer::Instance().CreateTextFormat(
            L"Segoe UI", static_cast<float>(m_fontSize), DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER
        );
        return;
    }

    if (m_textFormatTime) { m_textFormatTime->Release(); m_textFormatTime = nullptr; }
    if (m_textFormatStatus) { m_textFormatStatus->Release(); m_textFormatStatus = nullptr; }
    if (m_brushBg) { m_brushBg->Release(); m_brushBg = nullptr; }
    if (m_brushBorder) { m_brushBorder->Release(); m_brushBorder = nullptr; }
    if (m_brushText) { m_brushText->Release(); m_brushText = nullptr; }
    if (m_brushAccent) { m_brushAccent->Release(); m_brushAccent = nullptr; }
    if (m_dcRenderTarget) { m_dcRenderTarget->Release(); m_dcRenderTarget = nullptr; }

    if (m_hBitmap) { DeleteObject(m_hBitmap); m_hBitmap = nullptr; }
    if (m_memDC) { DeleteDC(m_memDC); m_memDC = nullptr; }

    HDC screenDC = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_width;
    bmi.bmiHeader.biHeight = -m_height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hBitmap = CreateDIBSection(m_memDC, &bmi, DIB_RGB_COLORS, &m_bitmapBits, nullptr, 0);
    SelectObject(m_memDC, m_hBitmap);

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

            // 极简白底、精细浅灰轮廓、高对比度曜石黑文字、极光蓝强调色
            m_dcRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.99f, 0.99f, 1.0f, 0.90f), &m_brushBg);
            m_dcRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.80f, 0.83f, 0.88f, 0.85f), &m_brushBorder);
            m_dcRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.06f, 0.09f, 0.16f, 1.0f), &m_brushText);
            m_dcRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.14f, 0.39f, 0.92f, 1.0f), &m_brushAccent);

            m_textFormatTime = D2DRenderer::Instance().CreateTextFormat(
                L"Segoe UI", static_cast<float>(m_fontSize), DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER
            );
            float statusPt = m_fontSize >= 26 ? 11.5f : 10.5f;
            m_textFormatStatus = D2DRenderer::Instance().CreateTextFormat(
                L"Microsoft YaHei UI", statusPt, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER
            );
        }
    }

    if (m_hWnd) {
        SetWindowPos(
            m_hWnd,
            m_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
            0, 0, m_width, m_height,
            SWP_NOMOVE | SWP_NOACTIVATE
        );
    }
}

void FloatingClock::Show() {
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_SHOWNOACTIVATE);
        EnsureTopmost();
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
        SetWindowPos(
            m_hWnd,
            m_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
            m_posX, m_posY, 0, 0,
            SWP_NOSIZE | SWP_NOACTIVATE
        );
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

namespace {
D2D1_COLOR_F HexToD2DColor(const std::string& hex) {
    if (hex.size() >= 7 && hex[0] == '#') {
        unsigned int r = 15, g = 23, b = 42;
        if (sscanf_s(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            return D2D1::ColorF(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
        }
    }
    return D2D1::ColorF(0.06f, 0.09f, 0.16f, 1.0f); // 极简曜石黑 #0F172A
}
} // namespace

void FloatingClock::UpdateDisplay(int64_t remainingSeconds, TimerState state) {
    m_currentSeconds = remainingSeconds;
    m_currentState = state;

    if (state == TimerState::Idle && m_showRealTimeWhenIdle) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        tm localTm{};
        localtime_s(&localTm, &now);
        wchar_t buf[32];
        swprintf_s(buf, L"%02d:%02d:%02d", localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
        m_timeString = buf;
        m_statusString = L"CLOCK";
        if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.14f, 0.39f, 0.92f, 1.0f)); // #2563EB
    } else {
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
            m_statusString = L"IDLE";
            if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.23f, 0.51f, 0.96f, 1.0f)); // #3B82F6
            break;
        case TimerState::Studying:
            m_statusString = L"FOCUS";
            if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.14f, 0.39f, 0.92f, 1.0f)); // #2563EB
            break;
        case TimerState::Paused:
            m_statusString = L"PAUSE";
            if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.92f, 0.55f, 0.10f, 1.0f)); // #EA580C
            break;
        case TimerState::BreakPending:
            m_statusString = L"BREAK";
            if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.224f, 0.773f, 0.733f, 1.0f)); // #39C5BB
            break;
        case TimerState::Breaking:
            m_statusString = L"REST";
            if (m_brushAccent) m_brushAccent->SetColor(D2D1::ColorF(0.224f, 0.773f, 0.733f, 1.0f)); // #39C5BB
            break;
        }
    }

    EnsureTopmost();
    Render();
}

void FloatingClock::Render() {
    if (!m_hWnd || !m_dcRenderTarget) return;

    RECT rc{0, 0, m_width, m_height};
    m_dcRenderTarget->BindDC(m_memDC, &rc);

    m_dcRenderTarget->BeginDraw();
    m_dcRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    // 绘制圆角胶囊背景 (极简现代白色质感胶囊)
    float cornerR = static_cast<float>(m_height) / 2.0f - 2.0f;
    D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(
        D2D1::RectF(2.0f, 2.0f, static_cast<float>(m_width) - 2.0f, static_cast<float>(m_height) - 2.0f),
        cornerR, cornerR
    );

    float bgAlpha = static_cast<float>(m_opacityPercent) / 100.0f;
    if (m_opacityPercent > 0) {
        // 纯净白底磨砂胶囊
        m_brushBg->SetColor(D2D1::ColorF(0.99f, 0.99f, 1.0f, bgAlpha));
        // 精细边缘 Slate 轮廓
        m_brushBorder->SetColor(D2D1::ColorF(0.80f, 0.83f, 0.88f, (std::min)(0.88f, bgAlpha * 0.85f)));
        m_dcRenderTarget->FillRoundedRectangle(rrect, m_brushBg);
        m_dcRenderTarget->DrawRoundedRectangle(rrect, m_brushBorder, 1.2f);
    }

    // 绘制左侧状态指示点 (圆润呼吸指示点)
    float dotX = 16.0f;
    float dotY = static_cast<float>(m_height) / 2.0f;
    m_dcRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(dotX, dotY), 4.2f, 4.2f), m_brushAccent);

    // 绘制状态文字 (点右侧微标，英文大写)
    float statusW = 46.0f;
    D2D1_RECT_F statusRect = D2D1::RectF(dotX + 6.0f, 4.0f, dotX + 6.0f + statusW, static_cast<float>(m_height) - 4.0f);
    m_dcRenderTarget->DrawText(
        m_statusString.c_str(),
        static_cast<UINT32>(m_statusString.size()),
        m_textFormatStatus,
        statusRect,
        m_brushAccent
    );

    // 绘制精致浅色竖向分隔线
    float sepX = dotX + 6.0f + statusW + 6.0f;
    ID2D1SolidColorBrush* dividerBrush = nullptr;
    m_dcRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.88f, 0.92f, (std::min)(0.80f, bgAlpha * 0.80f)), &dividerBrush);
    if (dividerBrush) {
        m_dcRenderTarget->DrawLine(
            D2D1::Point2F(sepX, 15.0f),
            D2D1::Point2F(sepX, static_cast<float>(m_height) - 15.0f),
            dividerBrush,
            1.0f
        );
        dividerBrush->Release();
    }

    // 绘制时间文本 (居右侧高对比度数字)
    if (m_brushText) {
        m_brushText->SetColor(HexToD2DColor(m_textColorHex));
    }
    D2D1_RECT_F timeRect = D2D1::RectF(sepX + 4.0f, 2.0f, static_cast<float>(m_width) - 8.0f, static_cast<float>(m_height) - 2.0f);
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

void FloatingClock::ApplyConfig(const AppConfig& config) {
    m_opacityPercent = config.clockOpacityPercent;
    m_fontSize = config.clockFontSize;
    m_textColorHex = config.clockTextColor;
    if (m_textColorHex.empty()) {
        m_textColorHex = "#0F172A"; // 极简白主题下默认高对比曜石黑
    }
    m_showRealTimeWhenIdle = config.showRealTimeWhenIdle;
    SetAlwaysOnTop(config.alwaysOnTop);

    int targetW = 196;
    int targetH = 54;
    if (m_fontSize == 18) { targetW = 176; targetH = 48; }
    else if (m_fontSize == 22) { targetW = 196; targetH = 54; }
    else if (m_fontSize == 26) { targetW = 224; targetH = 60; }
    else if (m_fontSize == 32) { targetW = 258; targetH = 68; }

    RecreateBitmapAndTarget(targetW, targetH);
    UpdateDisplay(m_currentSeconds, m_currentState);
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
    AppendMenuW(hMenu, MF_STRING, ID_MENU_CHECK_UPDATE, L"检查更新...");
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
    case ID_MENU_CHECK_UPDATE:
        Updater::CheckForUpdatesAsync(m_hWnd, false);
        break;
    case ID_MENU_ALWAYS_TOP:
        SetAlwaysOnTop(!m_alwaysOnTop);
        Repository::Instance().SaveClockPosition(m_posX, m_posY, m_alwaysOnTop);
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
    case WM_TIMER: {
        if (wParam == ID_TOPMOST_TIMER) {
            EnsureTopmost();
        }
        return 0;
    }
    case WM_WINDOWPOSCHANGING: {
        if (m_alwaysOnTop) {
            auto wp = reinterpret_cast<WINDOWPOS*>(lParam);
            wp->hwndInsertAfter = HWND_TOPMOST;
            wp->flags &= ~SWP_NOZORDER;
        }
        break;
    }
    case WM_ACTIVATE: {
        EnsureTopmost();
        break;
    }
    case WM_MOUSEACTIVATE: {
        return MA_NOACTIVATE;
    }
    case WM_LBUTTONDOWN: {
        GetCursorPos(&m_dragStartCursor);
        m_dragStartWindowPos.x = m_posX;
        m_dragStartWindowPos.y = m_posY;
        m_isDragging = false;
        SetCapture(hWnd);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (GetCapture() == hWnd) {
            POINT curPt;
            GetCursorPos(&curPt);
            int dx = curPt.x - m_dragStartCursor.x;
            int dy = curPt.y - m_dragStartCursor.y;
            if (!m_isDragging && (abs(dx) > 3 || abs(dy) > 3)) {
                m_isDragging = true;
            }
            if (m_isDragging) {
                m_posX = m_dragStartWindowPos.x + dx;
                m_posY = m_dragStartWindowPos.y + dy;
                // 允许自由拖出界、跨屏、贴合边角 (越界自由摆放)
                SetPosition(m_posX, m_posY);
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (GetCapture() == hWnd) {
            ReleaseCapture();
            if (m_isDragging) {
                m_isDragging = false;
                Repository::Instance().SaveClockPosition(m_posX, m_posY, m_alwaysOnTop);
            } else {
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
