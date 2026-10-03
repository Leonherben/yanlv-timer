#include "src/ui/break_fullscreen.h"
#include "src/ui/d2d_renderer.h"
#include "src/ui/floating_clock.h"
#include "src/core/timer_engine.h"
#include <windowsx.h>
#include <algorithm>
#include <cwchar>

namespace yanlv {

namespace {
const wchar_t* const BREAK_WINDOW_CLASS = L"YanlvBreakFullscreenClass";
constexpr UINT WM_GRAPHNOTIFY = WM_APP + 201;
constexpr UINT IDC_OVERLAY_SKIP = 6001;
constexpr UINT IDC_OVERLAY_TIME = 6002;

bool IsVideoFile(const std::wstring& path) {
    if (path.empty()) return false;
    size_t dotPos = path.find_last_of(L'.');
    if (dotPos == std::wstring::npos) return false;
    std::wstring ext = path.substr(dotPos);
    for (auto& c : ext) {
        c = static_cast<wchar_t>(::towlower(c));
    }
    return (ext == L".mp4" || ext == L".wmv" || ext == L".avi" ||
            ext == L".mkv" || ext == L".mov" || ext == L".m4v" ||
            ext == L".webm" || ext == L".flv" || ext == L".mpg" || ext == L".mpeg");
}
} // namespace

BreakFullscreen& BreakFullscreen::Instance() {
    static BreakFullscreen instance;
    return instance;
}

BreakFullscreen::BreakFullscreen() = default;

BreakFullscreen::~BreakFullscreen() {
    ReleaseMediaResources();
    if (m_hOverlayFont) {
        DeleteObject(m_hOverlayFont);
        m_hOverlayFont = nullptr;
    }
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

bool BreakFullscreen::ShowBreak(const std::wstring& mediaPath, bool videoMuted) {
    if (!m_hWnd) {
        if (!CreateFullscreenWindow()) {
            return false;
        }
    }

    // 更新屏幕尺寸与位置
    HMONITOR hMon = MonitorFromWindow(FloatingClock::Instance().GetHwnd(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoW(hMon, &mi);

    m_screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    m_screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;

    SetWindowPos(
        m_hWnd, HWND_TOPMOST,
        mi.rcMonitor.left, mi.rcMonitor.top,
        m_screenWidth, m_screenHeight,
        SWP_SHOWWINDOW
    );

    // 计算底部按钮位置 (图片模式下使用)
    m_skipBtnRect = D2D1::RectF(
        (m_screenWidth - 110.0f) / 2.0f,
        static_cast<float>(m_screenHeight) - 64.0f,
        (m_screenWidth + 110.0f) / 2.0f,
        static_cast<float>(m_screenHeight) - 28.0f
    );

    // 释放旧资源
    ReleaseMediaResources();

    // 如果指定了媒体路径且是视频文件，尝试使用 DirectShow 播放视频
    if (!mediaPath.empty() && IsVideoFile(mediaPath)) {
        if (PlayVideoFile(mediaPath, videoMuted)) {
            SetForegroundWindow(m_hWnd);
            SetFocus(m_hWnd);
            return true;
        }
    }

    // 视频未播放或非视频，使用 Direct2D 静态图片渲染模式
    if (D2DRenderer::Instance().Initialize()) {
        D2D1_SIZE_U size = D2D1::SizeU(m_screenWidth, m_screenHeight);
        D2DRenderer::Instance().GetD2DFactory()->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hWnd, size),
            &m_renderTarget
        );

        if (m_renderTarget) {
            m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), &m_brushBlack);
            m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.08f, 0.08f, 0.12f, 0.65f), &m_brushPill);
            m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.95f, 0.95f, 0.98f, 1.0f), &m_brushText);
            m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f), &m_brushSkipBtn);

            m_textFormatTime = D2DRenderer::Instance().CreateTextFormat(
                L"Consolas", 16.0f, DWRITE_FONT_WEIGHT_BOLD, DWRITE_TEXT_ALIGNMENT_CENTER
            );
            m_textFormatBtn = D2DRenderer::Instance().CreateTextFormat(
                L"Microsoft YaHei", 12.0f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_TEXT_ALIGNMENT_CENTER
            );
        }
    }

    // 尝试加载静态图片
    if (!mediaPath.empty() && !IsVideoFile(mediaPath)) {
        LoadImageFile(mediaPath);
    }
    if (!m_loadedBitmap) {
        // 尝试加载默认内置图片 assets/default_relax.jpg
        LoadImageFile(L"assets/default_relax.jpg");
    }

    SetForegroundWindow(m_hWnd);
    SetFocus(m_hWnd);
    Render();
    return true;
}

bool BreakFullscreen::PlayVideoFile(const std::wstring& filePath, bool isMuted) {
    HRESULT hr = CoCreateInstance(
        CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
        IID_IGraphBuilder, reinterpret_cast<void**>(&m_graphBuilder)
    );
    if (FAILED(hr) || !m_graphBuilder) {
        return false;
    }

    hr = m_graphBuilder->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&m_mediaControl));
    if (FAILED(hr)) {
        ReleaseMediaResources();
        return false;
    }

    m_graphBuilder->QueryInterface(IID_IVideoWindow, reinterpret_cast<void**>(&m_videoWindow));
    m_graphBuilder->QueryInterface(IID_IBasicAudio, reinterpret_cast<void**>(&m_basicAudio));
    m_graphBuilder->QueryInterface(IID_IMediaSeeking, reinterpret_cast<void**>(&m_mediaSeeking));
    m_graphBuilder->QueryInterface(IID_IMediaEventEx, reinterpret_cast<void**>(&m_mediaEvent));

    // 使用 DirectShow 自动构建解码滤镜链
    hr = m_graphBuilder->RenderFile(filePath.c_str(), nullptr);
    if (FAILED(hr)) {
        ReleaseMediaResources();
        return false;
    }

    // 配置视频显示窗口
    if (m_videoWindow) {
        m_videoWindow->put_Owner(reinterpret_cast<OAHWND>(m_hWnd));
        m_videoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS);

        // 计算等比自适应居中尺寸
        long vidWidth = 0, vidHeight = 0;
        IBasicVideo* pBasicVideo = nullptr;
        if (SUCCEEDED(m_graphBuilder->QueryInterface(IID_IBasicVideo, reinterpret_cast<void**>(&pBasicVideo)))) {
            pBasicVideo->GetVideoSize(&vidWidth, &vidHeight);
            pBasicVideo->Release();
        }

        long vx = 0, vy = 0, vw = m_screenWidth, vh = m_screenHeight;
        if (vidWidth > 0 && vidHeight > 0) {
            double aspect = static_cast<double>(vidWidth) / static_cast<double>(vidHeight);
            double screenAspect = static_cast<double>(m_screenWidth) / static_cast<double>(m_screenHeight);
            if (screenAspect > aspect) {
                vh = m_screenHeight;
                vw = static_cast<long>(vh * aspect);
                vx = (m_screenWidth - vw) / 2;
                vy = 0;
            } else {
                vw = m_screenWidth;
                vh = static_cast<long>(vw / aspect);
                vx = 0;
                vy = (m_screenHeight - vh) / 2;
            }
        }
        m_videoWindow->SetWindowPosition(vx, vy, vw, vh);

        // 转发按键与鼠标消息至宿主窗口
        m_videoWindow->put_MessageDrain(reinterpret_cast<OAHWND>(m_hWnd));
        m_videoWindow->put_Visible(OATRUE);
    }

    // 设置声音：0 为原声音量，-10000 为完全静音
    if (m_basicAudio) {
        m_basicAudio->put_Volume(isMuted ? -10000 : 0);
    }

    // 注册循环播放事件
    if (m_mediaEvent) {
        m_mediaEvent->SetNotifyWindow(reinterpret_cast<OAHWND>(m_hWnd), WM_GRAPHNOTIFY, 0);
    }

    hr = m_mediaControl->Run();
    if (FAILED(hr)) {
        ReleaseMediaResources();
        return false;
    }

    m_isVideoPlaying = true;
    SetupVideoOverlay();
    return true;
}

void BreakFullscreen::SetupVideoOverlay() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    if (!m_hOverlayFont) {
        m_hOverlayFont = CreateFontW(
            -16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas"
        );
    }

    // 右上角倒计时药丸
    int pillW = 100;
    int pillH = 34;
    int pillX = m_screenWidth - pillW - 28;
    int pillY = 24;

    if (!m_hStaticTimeOverlay) {
        m_hStaticTimeOverlay = CreateWindowExW(
            WS_EX_TOPMOST, L"STATIC", m_countdownText.c_str(),
            WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE | WS_CLIPSIBLINGS,
            pillX, pillY, pillW, pillH,
            m_hWnd, reinterpret_cast<HMENU>(IDC_OVERLAY_TIME), hInstance, nullptr
        );
        if (m_hOverlayFont) {
            SendMessage(m_hStaticTimeOverlay, WM_SETFONT, reinterpret_cast<WPARAM>(m_hOverlayFont), TRUE);
        }
    } else {
        SetWindowPos(m_hStaticTimeOverlay, HWND_TOP, pillX, pillY, pillW, pillH, SWP_SHOWWINDOW);
        SetWindowTextW(m_hStaticTimeOverlay, m_countdownText.c_str());
    }

    // 底部结束休息按钮
    int btnW = 120;
    int btnH = 38;
    int btnX = (m_screenWidth - btnW) / 2;
    int btnY = m_screenHeight - 64;

    if (!m_hBtnSkipOverlay) {
        m_hBtnSkipOverlay = CreateWindowExW(
            WS_EX_TOPMOST, L"BUTTON", L"结束休息",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_CLIPSIBLINGS,
            btnX, btnY, btnW, btnH,
            m_hWnd, reinterpret_cast<HMENU>(IDC_OVERLAY_SKIP), hInstance, nullptr
        );
        HFONT hBtnFont = CreateFontW(
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei"
        );
        if (hBtnFont) {
            SendMessage(m_hBtnSkipOverlay, WM_SETFONT, reinterpret_cast<WPARAM>(hBtnFont), TRUE);
        }
    } else {
        SetWindowPos(m_hBtnSkipOverlay, HWND_TOP, btnX, btnY, btnW, btnH, SWP_SHOWWINDOW);
    }

    // 保证悬浮控件置于视频窗口上方
    SetWindowPos(m_hStaticTimeOverlay, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(m_hBtnSkipOverlay, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

bool BreakFullscreen::LoadImageFile(const std::wstring& filePath) {
    IWICImagingFactory* wic = D2DRenderer::Instance().GetWICFactory();
    if (!wic || !m_renderTarget) return false;

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = wic->CreateDecoderFromFilename(
        filePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder
    );
    if (FAILED(hr)) return false;

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        decoder->Release();
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    hr = wic->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(
            frame, GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut
        );
        if (SUCCEEDED(hr)) {
            hr = m_renderTarget->CreateBitmapFromWicBitmap(converter, nullptr, &m_loadedBitmap);
        }
        converter->Release();
    }

    frame->Release();
    decoder->Release();
    return SUCCEEDED(hr) && m_loadedBitmap != nullptr;
}

void BreakFullscreen::ReleaseMediaResources() {
    if (m_mediaEvent) {
        m_mediaEvent->SetNotifyWindow(reinterpret_cast<OAHWND>(nullptr), 0, 0);
        m_mediaEvent->Release();
        m_mediaEvent = nullptr;
    }
    if (m_mediaControl) {
        m_mediaControl->Stop();
        m_mediaControl->Release();
        m_mediaControl = nullptr;
    }
    if (m_videoWindow) {
        m_videoWindow->put_Visible(OAFALSE);
        m_videoWindow->put_Owner(reinterpret_cast<OAHWND>(nullptr));
        m_videoWindow->put_MessageDrain(reinterpret_cast<OAHWND>(nullptr));
        m_videoWindow->Release();
        m_videoWindow = nullptr;
    }
    if (m_basicAudio) {
        m_basicAudio->Release();
        m_basicAudio = nullptr;
    }
    if (m_mediaSeeking) {
        m_mediaSeeking->Release();
        m_mediaSeeking = nullptr;
    }
    if (m_graphBuilder) {
        m_graphBuilder->Release();
        m_graphBuilder = nullptr;
    }
    m_isVideoPlaying = false;

    if (m_hBtnSkipOverlay) {
        DestroyWindow(m_hBtnSkipOverlay);
        m_hBtnSkipOverlay = nullptr;
    }
    if (m_hStaticTimeOverlay) {
        DestroyWindow(m_hStaticTimeOverlay);
        m_hStaticTimeOverlay = nullptr;
    }

    if (m_loadedBitmap) {
        m_loadedBitmap->Release();
        m_loadedBitmap = nullptr;
    }
    if (m_brushBlack) { m_brushBlack->Release(); m_brushBlack = nullptr; }
    if (m_brushPill) { m_brushPill->Release(); m_brushPill = nullptr; }
    if (m_brushText) { m_brushText->Release(); m_brushText = nullptr; }
    if (m_brushSkipBtn) { m_brushSkipBtn->Release(); m_brushSkipBtn = nullptr; }
    if (m_textFormatTime) { m_textFormatTime->Release(); m_textFormatTime = nullptr; }
    if (m_textFormatBtn) { m_textFormatBtn->Release(); m_textFormatBtn = nullptr; }
    if (m_renderTarget) {
        m_renderTarget->Release();
        m_renderTarget = nullptr;
    }
}

void BreakFullscreen::CloseBreak() {
    if (m_hWnd && IsWindowVisible(m_hWnd)) {
        ShowWindow(m_hWnd, SW_HIDE);
    }
    ReleaseMediaResources();
}

void BreakFullscreen::UpdateCountdown(int64_t remainingSeconds) {
    int64_t mins = remainingSeconds / 60;
    int64_t secs = remainingSeconds % 60;
    wchar_t buf[16];
    swprintf_s(buf, L"%02lld:%02lld", mins, secs);
    m_countdownText = buf;

    if (m_isVideoPlaying && m_hStaticTimeOverlay) {
        SetWindowTextW(m_hStaticTimeOverlay, m_countdownText.c_str());
    } else if (IsActive()) {
        Render();
    }
}

void BreakFullscreen::Render() {
    if (!m_renderTarget || !IsActive() || m_isVideoPlaying) return;

    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(D2D1::ColorF(0.04f, 0.04f, 0.06f, 1.0f));

    // 绘制媒体（保持宽高比居中 Letterbox）
    if (m_loadedBitmap) {
        D2D1_SIZE_F bmpSize = m_loadedBitmap->GetSize();
        float scale = (std::min)(
            static_cast<float>(m_screenWidth) / bmpSize.width,
            static_cast<float>(m_screenHeight) / bmpSize.height
        );
        float dw = bmpSize.width * scale;
        float dh = bmpSize.height * scale;
        float dx = (static_cast<float>(m_screenWidth) - dw) / 2.0f;
        float dy = (static_cast<float>(m_screenHeight) - dh) / 2.0f;

        m_renderTarget->DrawBitmap(m_loadedBitmap, D2D1::RectF(dx, dy, dx + dw, dy + dh));
    } else {
        // 无外置媒体时的默认宁静星空背景
        D2D1_RECT_F fullRect = D2D1::RectF(0, 0, static_cast<float>(m_screenWidth), static_cast<float>(m_screenHeight));
        m_brushPill->SetColor(D2D1::ColorF(0.06f, 0.08f, 0.12f, 1.0f));
        m_renderTarget->FillRectangle(fullRect, m_brushPill);

        // 柔和光晕
        ID2D1SolidColorBrush* glowBrush = nullptr;
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.42f, 0.55f, 0.15f), &glowBrush);
        if (glowBrush) {
            m_renderTarget->FillEllipse(
                D2D1::Ellipse(D2D1::Point2F(m_screenWidth * 0.5f, m_screenHeight * 0.45f), 280.0f, 280.0f),
                glowBrush
            );
            glowBrush->Release();
        }
    }

    // 绘制右上角微型倒计时胶囊 (05:00)
    float pillW = 96.0f;
    float pillH = 34.0f;
    float pillX = static_cast<float>(m_screenWidth) - pillW - 28.0f;
    float pillY = 24.0f;
    D2D1_ROUNDED_RECT pillRRect = D2D1::RoundedRect(
        D2D1::RectF(pillX, pillY, pillX + pillW, pillY + pillH), 17.0f, 17.0f
    );
    m_brushPill->SetColor(D2D1::ColorF(0.06f, 0.08f, 0.12f, 0.75f));
    m_renderTarget->FillRoundedRectangle(pillRRect, m_brushPill);

    m_renderTarget->DrawText(
        m_countdownText.c_str(),
        static_cast<UINT32>(m_countdownText.size()),
        m_textFormatTime,
        D2D1::RectF(pillX, pillY + 1.0f, pillX + pillW, pillY + pillH),
        m_brushText
    );

    // 绘制底部“结束休息”按钮
    D2D1_ROUNDED_RECT skipRRect = D2D1::RoundedRect(m_skipBtnRect, 14.0f, 14.0f);
    m_brushSkipBtn->SetColor(
        m_isHoveringSkip ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.28f) : D2D1::ColorF(0.08f, 0.08f, 0.12f, 0.55f)
    );
    m_renderTarget->FillRoundedRectangle(skipRRect, m_brushSkipBtn);

    std::wstring skipText = L"结束休息";
    m_renderTarget->DrawText(
        skipText.c_str(),
        static_cast<UINT32>(skipText.size()),
        m_textFormatBtn,
        m_skipBtnRect,
        m_brushText
    );

    m_renderTarget->EndDraw();
}

bool BreakFullscreen::CreateFullscreenWindow() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = BREAK_WINDOW_CLASS;

    RegisterClassExW(&wc);

    m_hWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        BREAK_WINDOW_CLASS,
        L"言律时钟 - 5分钟全屏休息",
        WS_POPUP,
        0, 0, 1920, 1080,
        nullptr, nullptr, hInstance, this
    );

    return m_hWnd != nullptr;
}

LRESULT CALLBACK BreakFullscreen::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    BreakFullscreen* self = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<BreakFullscreen*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<BreakFullscreen*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT BreakFullscreen::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id == IDC_OVERLAY_SKIP) {
            TimerEngine::Instance().SkipBreak();
            CloseBreak();
            FloatingClock::Instance().Show();
            return 0;
        }
        break;
    }
    case WM_GRAPHNOTIFY: {
        if (m_mediaEvent) {
            long evCode;
            LONG_PTR p1, p2;
            while (SUCCEEDED(m_mediaEvent->GetEvent(&evCode, &p1, &p2, 0))) {
                m_mediaEvent->FreeEventParams(evCode, p1, p2);
                if (evCode == EC_COMPLETE) {
                    if (m_mediaSeeking) {
                        LONGLONG startPos = 0;
                        m_mediaSeeking->SetPositions(
                            &startPos, AM_SEEKING_AbsolutePositioning,
                            nullptr, AM_SEEKING_NoPositioning
                        );
                    }
                }
            }
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HWND hCtrl = reinterpret_cast<HWND>(lParam);
        if (hCtrl == m_hStaticTimeOverlay) {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetTextColor(hdc, RGB(245, 245, 250));
            SetBkColor(hdc, RGB(20, 24, 32));
            static HBRUSH hOverlayBg = CreateSolidBrush(RGB(20, 24, 32));
            return reinterpret_cast<INT_PTR>(hOverlayBg);
        }
        break;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            // Esc 退出全屏，继续后台休息并在悬浮时钟显示
            CloseBreak();
            FloatingClock::Instance().Show();
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        if (m_isVideoPlaying) return 0;

        float mx = static_cast<float>(GET_X_LPARAM(lParam));
        float my = static_cast<float>(GET_Y_LPARAM(lParam));

        bool hover = (mx >= m_skipBtnRect.left && mx <= m_skipBtnRect.right &&
                      my >= m_skipBtnRect.top && my <= m_skipBtnRect.bottom);
        if (hover != m_isHoveringSkip) {
            m_isHoveringSkip = hover;
            Render();
        }

        // 跟踪鼠标离开窗口
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (m_isHoveringSkip) {
            m_isHoveringSkip = false;
            Render();
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (m_isVideoPlaying) return 0;

        float mx = static_cast<float>(GET_X_LPARAM(lParam));
        float my = static_cast<float>(GET_Y_LPARAM(lParam));

        if (mx >= m_skipBtnRect.left && mx <= m_skipBtnRect.right &&
            my >= m_skipBtnRect.top && my <= m_skipBtnRect.bottom) {
            // 点击结束休息
            TimerEngine::Instance().SkipBreak();
            CloseBreak();
            FloatingClock::Instance().Show();
            return 0;
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        if (!m_isVideoPlaying) {
            Render();
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

} // namespace yanlv
