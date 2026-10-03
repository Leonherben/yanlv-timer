#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <cstdint>
#include <dshow.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>

namespace yanlv {

class BreakFullscreen {
public:
    static BreakFullscreen& Instance();

    bool ShowBreak(const std::wstring& mediaPath = L"", bool videoMuted = true);
    void CloseBreak();
    bool IsActive() const { return m_hWnd != nullptr && IsWindowVisible(m_hWnd); }

    void UpdateCountdown(int64_t remainingSeconds);

private:
    BreakFullscreen();
    ~BreakFullscreen();
    BreakFullscreen(const BreakFullscreen&) = delete;
    BreakFullscreen& operator=(const BreakFullscreen&) = delete;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    bool CreateFullscreenWindow();
    void Render();
    void ReleaseMediaResources();
    bool LoadImageFile(const std::wstring& filePath);
    bool PlayVideoFile(const std::wstring& filePath, bool isMuted);
    void SetupVideoOverlay();

    HWND m_hWnd = nullptr;
    int m_screenWidth = 1920;
    int m_screenHeight = 1080;

    // Direct2D 渲染资源 (静态图片背景时使用)
    ID2D1HwndRenderTarget* m_renderTarget = nullptr;
    ID2D1Bitmap* m_loadedBitmap = nullptr;
    ID2D1SolidColorBrush* m_brushBlack = nullptr;
    ID2D1SolidColorBrush* m_brushPill = nullptr;
    ID2D1SolidColorBrush* m_brushText = nullptr;
    ID2D1SolidColorBrush* m_brushSkipBtn = nullptr;
    IDWriteTextFormat* m_textFormatTime = nullptr;
    IDWriteTextFormat* m_textFormatBtn = nullptr;

    std::wstring m_countdownText = L"05:00";
    bool m_isHoveringSkip = false;
    D2D1_RECT_F m_skipBtnRect{};

    // DirectShow 视频播放
    IGraphBuilder* m_graphBuilder = nullptr;
    IMediaControl* m_mediaControl = nullptr;
    IVideoWindow* m_videoWindow = nullptr;
    IBasicAudio* m_basicAudio = nullptr;
    IMediaSeeking* m_mediaSeeking = nullptr;
    IMediaEventEx* m_mediaEvent = nullptr;
    bool m_isVideoPlaying = false;

    // 视频全屏悬浮控制覆盖层
    HWND m_hBtnSkipOverlay = nullptr;
    HWND m_hStaticTimeOverlay = nullptr;
    HFONT m_hOverlayFont = nullptr;
};

} // namespace yanlv
