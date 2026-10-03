#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include "src/core/timer_types.h"
#include <string>
#include <functional>

namespace yanlv {

class FloatingClock {
public:
    static FloatingClock& Instance();

    bool Create();
    void Show();
    void Hide();
    void ToggleVisibility();
    bool IsVisible() const;

    void SetPosition(int x, int y);
    void GetPosition(int& x, int& y) const;
    void SetAlwaysOnTop(bool onTop);
    bool IsAlwaysOnTop() const { return m_alwaysOnTop; }

    void UpdateDisplay(int64_t remainingSeconds, TimerState state);
    void ApplyConfig(const AppConfig& config);

    HWND GetHwnd() const { return m_hWnd; }

    void SetOnQuickStartRequested(std::function<void()> cb) { m_onQuickStart = cb; }
    void SetOnOpenManagementRequested(std::function<void()> cb) { m_onOpenManagement = cb; }

private:
    FloatingClock();
    ~FloatingClock();
    FloatingClock(const FloatingClock&) = delete;
    FloatingClock& operator=(const FloatingClock&) = delete;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static constexpr UINT_PTR ID_TOPMOST_TIMER = 9001;

    void Render();
    void RecreateBitmapAndTarget(int width, int height);
    void ShowContextMenu(int screenX, int screenY);
    void EnsureTopmost();

    HWND m_hWnd = nullptr;
    int m_width = 188;
    int m_height = 54;
    int m_posX = 120;
    int m_posY = 120;
    bool m_alwaysOnTop = true;

    // 个性化配置状态
    int m_opacityPercent = 85;
    int m_fontSize = 22;
    std::string m_textColorHex = "#0F172A";
    bool m_showRealTimeWhenIdle = true;

    // 绘制资源
    HDC m_memDC = nullptr;
    HBITMAP m_hBitmap = nullptr;
    void* m_bitmapBits = nullptr;
    ID2D1DCRenderTarget* m_dcRenderTarget = nullptr;
    IDWriteTextFormat* m_textFormatTime = nullptr;
    IDWriteTextFormat* m_textFormatStatus = nullptr;

    ID2D1SolidColorBrush* m_brushBg = nullptr;
    ID2D1SolidColorBrush* m_brushBorder = nullptr;
    ID2D1SolidColorBrush* m_brushText = nullptr;
    ID2D1SolidColorBrush* m_brushAccent = nullptr;

    // 当前显示状态
    int64_t m_currentSeconds = 25 * 60;
    TimerState m_currentState = TimerState::Idle;
    std::wstring m_timeString = L"25:00";
    std::wstring m_statusString = L"待开始";

    // 鼠标点击判断
    POINT m_mouseDownPos{0, 0};
    bool m_isDragging = false;

    std::function<void()> m_onQuickStart;
    std::function<void()> m_onOpenManagement;
};

} // namespace yanlv
