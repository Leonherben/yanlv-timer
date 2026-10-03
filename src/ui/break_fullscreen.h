#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <cstdint>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfmediaengine.h>
#include <string>

namespace yanlv {

class BreakFullscreen {
public:
    static BreakFullscreen& Instance();

    bool ShowBreak(const std::wstring& mediaPath = L"");
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
    bool PlayVideoFile(const std::wstring& filePath);

    HWND m_hWnd = nullptr;
    int m_screenWidth = 1920;
    int m_screenHeight = 1080;

    // Direct2D 渲染资源 (仅在全屏期间持有，退出立即释放)
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

    // Media Foundation
    IMFMediaEngine* m_mediaEngine = nullptr;
    bool m_isVideo = false;
};

} // namespace yanlv
