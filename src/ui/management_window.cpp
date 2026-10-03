#include "src/ui/management_window.h"
#include "src/ui/floating_clock.h"
#include "src/db/repository.h"
#include "src/utils/updater.h"
#include "src/app/resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

namespace yanlv {

namespace {
const wchar_t* const MANAGEMENT_WINDOW_CLASS = L"YanlvManagementWindowClass";
const wchar_t* const TAB_PANEL_CLASS = L"YanlvTabPanelClass";
const wchar_t* const CLOCK_PREVIEW_CLASS = L"YanlvClockPreviewClass";
const wchar_t* const SEGMENTED_CONTROL_CLASS = L"YanlvSegmentedControlClass";
const wchar_t* const SLIDER_CONTROL_CLASS = L"YanlvSliderClass";
const wchar_t* const TOGGLE_SWITCH_CLASS = L"YanlvToggleSwitchClass";

enum ControlId {
    ID_TAB_STATS = 3001,
    ID_TAB_RECORDS,
    ID_TAB_SETTINGS,
    ID_COMBO_FILTER,
    ID_LIST_RECORDS,
    ID_BTN_CHANGE_CAT,
    ID_BTN_DELETE_RECORD,
    ID_LIST_CATEGORIES,
    ID_EDIT_NEW_CAT,
    ID_BTN_ADD_CAT,
    ID_BTN_RENAME_CAT,
    ID_BTN_DELETE_CAT,
    ID_SEG_IDLE_MODE,
    ID_SLIDER_OPACITY,
    ID_COMBO_FONT_SIZE,
    ID_COMBO_TEXT_COLOR,
    ID_SEG_BREAK_MODE,
    ID_EDIT_MEDIA_PATH,
    ID_BTN_BROWSE_MEDIA,
    ID_TOGGLE_VIDEO_MUTED,
    ID_BTN_SAVE_SETTINGS,
    ID_BTN_CHECK_UPDATE,
    ID_BTN_OPEN_DATA_DIR,
    ID_CLOCK_PREVIEW,
    ID_BTN_WIN_MIN,
    ID_BTN_WIN_CLOSE
};

LRESULT CALLBACK SegmentedControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        // 卡片纯白底底衬
        HBRUSH hCardWhite = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        FillRect(memDC, &rc, hCardWhite);

        // 分段控件浅灰胶囊背景 #F1F5F9
        HBRUSH hBg = CreateSolidBrush(RGB(241, 245, 249));
        HPEN hPenBg = CreatePen(PS_SOLID, 1, RGB(241, 245, 249));
        HGDIOBJ oldBr = SelectObject(memDC, hBg);
        HGDIOBJ oldPen = SelectObject(memDC, hPenBg);
        RoundRect(memDC, rc.left, rc.top, rc.right, rc.bottom, 8, 8);

        LONG_PTR sel = GetWindowLongPtrW(hWnd, GWLP_USERDATA); // 0 或 1
        int w = rc.right - rc.left;
        int itemW = (w - 6) / 2;

        RECT rcSel;
        if (sel == 0) {
            rcSel = { 3, 3, 3 + itemW, rc.bottom - 3 };
        } else {
            rcSel = { 3 + itemW, 3, rc.right - 3, rc.bottom - 3 };
        }

        // 选中项纯白药丸与细边框
        HBRUSH hWhite = CreateSolidBrush(RGB(255, 255, 255));
        HPEN hBorder = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        SelectObject(memDC, hWhite);
        SelectObject(memDC, hBorder);
        RoundRect(memDC, rcSel.left, rcSel.top, rcSel.right, rcSel.bottom, 6, 6);
        DeleteObject(hWhite);
        DeleteObject(hBorder);

        SelectObject(memDC, oldBr);
        SelectObject(memDC, oldPen);
        DeleteObject(hBg);
        DeleteObject(hPenBg);

        // 提取两项文本（格式："选项1|选项2"）
        wchar_t buf[128] = { 0 };
        GetWindowTextW(hWnd, buf, 128);
        std::wstring text(buf);
        std::wstring text1 = text, text2 = text;
        size_t pipePos = text.find(L'|');
        if (pipePos != std::wstring::npos) {
            text1 = text.substr(0, pipePos);
            text2 = text.substr(pipePos + 1);
        }

        auto& mw = ManagementWindow::Instance();
        SetBkMode(memDC, TRANSPARENT);

        // 绘制第一项
        RECT rc1 = { 3, 3, 3 + itemW, rc.bottom - 3 };
        SelectObject(memDC, (sel == 0) ? mw.m_hFontBold : mw.m_hFont);
        SetTextColor(memDC, (sel == 0) ? RGB(65, 100, 222) : RGB(100, 116, 139));
        DrawTextW(memDC, text1.c_str(), -1, &rc1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 绘制第二项
        RECT rc2 = { 3 + itemW, 3, rc.right - 3, rc.bottom - 3 };
        SelectObject(memDC, (sel == 1) ? mw.m_hFontBold : mw.m_hFont);
        SetTextColor(memDC, (sel == 1) ? RGB(65, 100, 222) : RGB(100, 116, 139));
        DrawTextW(memDC, text2.c_str(), -1, &rc2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        RECT rc;
        GetClientRect(hWnd, &rc);
        int x = LOWORD(lParam);
        int mid = (rc.right - rc.left) / 2;
        LONG_PTR oldSel = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        LONG_PTR newSel = (x < mid) ? 0 : 1;
        if (newSel != oldSel) {
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, newSel);
            InvalidateRect(hWnd, nullptr, FALSE);
            SendMessageW(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED), reinterpret_cast<LPARAM>(hWnd));
        }
        return 0;
    }
    case WM_SETCURSOR: {
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    }
    case BM_GETCHECK: {
        return GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    }
    case BM_SETCHECK: {
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, wParam);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK SliderProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        // 卡片纯白底底衬
        HBRUSH hWhite = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        FillRect(memDC, &rc, hWhite);

        int pos = static_cast<int>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (pos < 0) pos = 0;
        if (pos > 100) pos = 100;

        int cy = (rc.bottom - rc.top) / 2;
        int trackStartX = 8;
        int trackEndX = rc.right - 8;
        int trackW = trackEndX - trackStartX;
        int knobX = trackStartX + (trackW * pos) / 100;

        // 灰色轨道背景 #E2E8F0
        HBRUSH hGray = CreateSolidBrush(RGB(226, 232, 240));
        HPEN hPenGray = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HGDIOBJ oldBr = SelectObject(memDC, hGray);
        HGDIOBJ oldPen = SelectObject(memDC, hPenGray);
        RoundRect(memDC, trackStartX, cy - 3, trackEndX, cy + 3, 4, 4);

        // 蓝色活动轨道 #4164DE
        if (knobX > trackStartX) {
            HBRUSH hBlue = CreateSolidBrush(RGB(65, 100, 222));
            HPEN hPenBlue = CreatePen(PS_SOLID, 1, RGB(65, 100, 222));
            SelectObject(memDC, hBlue);
            SelectObject(memDC, hPenBlue);
            RoundRect(memDC, trackStartX, cy - 3, knobX, cy + 3, 4, 4);
            DeleteObject(hBlue);
            DeleteObject(hPenBlue);
        }

        // 滑块手柄按钮：纯正皇家蓝实心圆点 #4164DE
        HBRUSH hThumbBr = CreateSolidBrush(RGB(65, 100, 222));
        HPEN hThumbPen = CreatePen(PS_SOLID, 1, RGB(65, 100, 222));
        HGDIOBJ oldTBr = SelectObject(memDC, hThumbBr);
        HGDIOBJ oldTPen = SelectObject(memDC, hThumbPen);
        Ellipse(memDC, knobX - 7, cy - 7, knobX + 7, cy + 7);
        SelectObject(memDC, oldTBr);
        SelectObject(memDC, oldTPen);
        DeleteObject(hThumbBr);
        DeleteObject(hThumbPen);

        SelectObject(memDC, oldBr);
        SelectObject(memDC, oldPen);
        DeleteObject(hGray);
        DeleteObject(hPenGray);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        SetCapture(hWnd);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int x = LOWORD(lParam);
        int trackStartX = 8;
        int trackEndX = rc.right - 8;
        int trackW = trackEndX - trackStartX;
        int newPos = (x - trackStartX) * 100 / (trackW > 0 ? trackW : 1);
        if (newPos < 0) newPos = 0;
        if (newPos > 100) newPos = 100;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, newPos);
        InvalidateRect(hWnd, nullptr, FALSE);
        SendMessageW(GetParent(hWnd), WM_HSCROLL, MAKEWPARAM(newPos, 0), reinterpret_cast<LPARAM>(hWnd));
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (GetCapture() == hWnd) {
            RECT rc;
            GetClientRect(hWnd, &rc);
            int x = static_cast<short>(LOWORD(lParam));
            int trackStartX = 8;
            int trackEndX = rc.right - 8;
            int trackW = trackEndX - trackStartX;
            int newPos = (x - trackStartX) * 100 / (trackW > 0 ? trackW : 1);
            if (newPos < 0) newPos = 0;
            if (newPos > 100) newPos = 100;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, newPos);
            InvalidateRect(hWnd, nullptr, FALSE);
            SendMessageW(GetParent(hWnd), WM_HSCROLL, MAKEWPARAM(newPos, 0), reinterpret_cast<LPARAM>(hWnd));
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (GetCapture() == hWnd) {
            ReleaseCapture();
        }
        return 0;
    }
    case WM_SETCURSOR: {
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    }
    case TBM_GETPOS: {
        return GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    }
    case TBM_SETPOS: {
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, lParam);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ToggleSwitchProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        // 卡片纯白底底衬
        HBRUSH hWhite = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        FillRect(memDC, &rc, hWhite);

        LONG_PTR chk = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        COLORREF pillCol = chk ? RGB(65, 100, 222) : RGB(203, 213, 225); // 蓝 #4164DE 或 灰 #CBD5E1

        HBRUSH hPillBr = CreateSolidBrush(pillCol);
        HPEN hPillPen = CreatePen(PS_SOLID, 1, pillCol);
        HGDIOBJ oldBr = SelectObject(memDC, hPillBr);
        HGDIOBJ oldPen = SelectObject(memDC, hPillPen);
        RoundRect(memDC, 0, 0, rc.right, rc.bottom, rc.bottom, rc.bottom);

        // 滑块小白球
        int knobSize = rc.bottom - 4;
        int knobX = chk ? (rc.right - 2 - knobSize) : 2;
        HBRUSH hKnobBr = CreateSolidBrush(RGB(255, 255, 255));
        HPEN hKnobPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
        SelectObject(memDC, hKnobBr);
        SelectObject(memDC, hKnobPen);
        Ellipse(memDC, knobX, 2, knobX + knobSize, 2 + knobSize);

        SelectObject(memDC, oldBr);
        SelectObject(memDC, oldPen);
        DeleteObject(hPillBr);
        DeleteObject(hPillPen);
        DeleteObject(hKnobBr);
        DeleteObject(hKnobPen);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        LONG_PTR chk = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, !chk);
        InvalidateRect(hWnd, nullptr, FALSE);
        SendMessageW(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), BN_CLICKED), reinterpret_cast<LPARAM>(hWnd));
        return 0;
    }
    case WM_SETCURSOR: {
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    }
    case BM_GETCHECK: {
        return GetWindowLongPtrW(hWnd, GWLP_USERDATA) ? BST_CHECKED : BST_UNCHECKED;
    }
    case BM_SETCHECK: {
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (wParam == BST_CHECKED || wParam == 1) ? 1 : 0);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

static WNDPROC s_oldEditProc = nullptr;
static LRESULT CALLBACK CueEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        LRESULT res = CallWindowProcW(s_oldEditProc, hWnd, uMsg, wParam, lParam);
        int len = GetWindowTextLengthW(hWnd);
        if (len == 0 && GetFocus() != hWnd) {
            HDC hdc = GetDC(hWnd);
            RECT rc;
            GetClientRect(hWnd, &rc);
            rc.left += 8;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(148, 163, 184)); // #94A3B8
            auto& mw = ManagementWindow::Instance();
            SelectObject(hdc, mw.m_hFont);
            DrawTextW(hdc, L"输入新类别名称", -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ReleaseDC(hWnd, hdc);
        }
        return res;
    } else if (uMsg == WM_SETFOCUS || uMsg == WM_KILLFOCUS) {
        InvalidateRect(hWnd, nullptr, TRUE);
    }
    return CallWindowProcW(s_oldEditProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK TabPanelProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        LONG_PTR panelTag = GetWindowLongPtrW(hWnd, GWLP_USERDATA);

        if (panelTag == 3) { // 设置面板
            // 浅灰背景 #F5F6F8
            HBRUSH hBg = CreateSolidBrush(RGB(245, 246, 248));
            FillRect(memDC, &rc, hBg);
            DeleteObject(hBg);

            auto& mw = ManagementWindow::Instance();

            // 1. 页面标题（直接绘制，保证纯净无灰边）
            SetBkMode(memDC, TRANSPARENT);
            SelectObject(memDC, mw.m_hFontTitle);
            SetTextColor(memDC, RGB(15, 23, 42)); // #0F172A
            RECT rcTitle = { 24, 14, 400, 42 };
            DrawTextW(memDC, L"时钟与偏好设置", -1, &rcTitle, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // 右上角脏状态提示
            SelectObject(memDC, mw.m_hFontBold);
            if (mw.m_isDirty) {
                SetTextColor(memDC, RGB(217, 119, 6)); // #D97706
                RECT rcDirty = { rc.right - 200, 14, rc.right - 24, 36 };
                DrawTextW(memDC, L"● 有未保存的修改", -1, &rcDirty, DT_RIGHT | DT_TOP | DT_SINGLELINE);
            } else {
                SetTextColor(memDC, RGB(16, 185, 129)); // #10B981
                RECT rcClean = { rc.right - 200, 14, rc.right - 24, 36 };
                DrawTextW(memDC, L"✓ 设置已保存", -1, &rcClean, DT_RIGHT | DT_TOP | DT_SINGLELINE);
            }

            // 绘制 4 张纯白卡片：
            HBRUSH hWhite = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240)); // #E2E8F0
            HGDIOBJ oldBr = SelectObject(memDC, hWhite);
            HGDIOBJ oldPen = SelectObject(memDC, hPen);

            // 卡片 1：悬浮时钟 (x=24, y=56, w=450, h=194)
            RoundRect(memDC, 24, 56, 474, 250, 14, 14);

            // 卡片 2：外观预览 (x=488, y=56, w=336, h=194)
            RoundRect(memDC, 488, 56, rc.right - 24, 250, 14, 14);

            // 卡片 3：休息设置 (x=24, y=262, w=800, h=160)
            RoundRect(memDC, 24, 262, rc.right - 24, 422, 14, 14);

            // 卡片 4：专注类别 (x=24, y=434, w=800, h=164)
            RoundRect(memDC, 24, 434, rc.right - 24, 598, 14, 14);

            SelectObject(memDC, oldBr);
            SelectObject(memDC, oldPen);
            DeleteObject(hPen);

            // 卡片标题与图标（直接绘制，绝无灰块）
            SelectObject(memDC, mw.m_hFontSection);
            SetTextColor(memDC, RGB(15, 23, 42));

            // 卡片 1 标题：悬浮时钟
            RECT rcH1 = { 42, 70, 200, 92 };
            DrawTextW(memDC, L"⏱ 悬浮时钟", -1, &rcH1, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // 卡片 2 标题：外观预览
            SelectObject(memDC, mw.m_hFont);
            SetTextColor(memDC, RGB(100, 116, 139));
            RECT rcH2 = { 506, 70, 650, 92 };
            DrawTextW(memDC, L"外观预览", -1, &rcH2, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // 卡片 3 标题：休息设置
            SelectObject(memDC, mw.m_hFontSection);
            SetTextColor(memDC, RGB(15, 23, 42));
            RECT rcH3 = { 42, 276, 200, 298 };
            DrawTextW(memDC, L"☕ 休息设置", -1, &rcH3, DT_LEFT | DT_TOP | DT_SINGLELINE);

            SelectObject(memDC, mw.m_hFontSub);
            SetTextColor(memDC, RGB(100, 116, 139));
            RECT rcH3Tag = { rc.right - 100, 278, rc.right - 42, 298 };
            DrawTextW(memDC, L"5 分钟", -1, &rcH3Tag, DT_RIGHT | DT_TOP | DT_SINGLELINE);

            // 休息背景媒体外框 (纯浅色胶囊 #F8FAFC, 细边框 #E2E8F0)
            HBRUSH hMediaBg = CreateSolidBrush(RGB(248, 250, 252));
            HPEN hMediaPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
            HGDIOBJ oldMediaBr = SelectObject(memDC, hMediaBg);
            HGDIOBJ oldMediaPen = SelectObject(memDC, hMediaPen);
            RoundRect(memDC, 118, 343, 686, 377, 6, 6);
            SelectObject(memDC, oldMediaBr);
            SelectObject(memDC, oldMediaPen);
            DeleteObject(hMediaBg);
            DeleteObject(hMediaPen);

            // 卡片 4 标题：专注类别
            SelectObject(memDC, mw.m_hFontSection);
            SetTextColor(memDC, RGB(15, 23, 42));
            RECT rcH4 = { 42, 448, 200, 470 };
            DrawTextW(memDC, L"🏷 专注类别", -1, &rcH4, DT_LEFT | DT_TOP | DT_SINGLELINE);

            // ListBox 类别列表外框 (细边框 #E2E8F0, 6px 圆角)
            HPEN hListPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
            HGDIOBJ oldLPen = SelectObject(memDC, hListPen);
            HGDIOBJ oldLBr = SelectObject(memDC, GetStockObject(NULL_BRUSH));
            RoundRect(memDC, 41, 481, 373, 585, 8, 8);
            SelectObject(memDC, oldLPen);
            SelectObject(memDC, oldLBr);
            DeleteObject(hListPen);

        } else {
            // 纯白整版面板
            HBRUSH hWhite = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
            FillRect(memDC, &rc, hWhite);

            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
            HGDIOBJ oldPen = SelectObject(memDC, hPen);
            HGDIOBJ oldBr = SelectObject(memDC, GetStockObject(NULL_BRUSH));
            RoundRect(memDC, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
            SelectObject(memDC, oldPen);
            SelectObject(memDC, oldBr);
            DeleteObject(hPen);
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(30, 41, 59));
        return reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));
    }
    case WM_CTLCOLOREDIT: {
        HWND hCtl = reinterpret_cast<HWND>(lParam);
        HDC hdc = reinterpret_cast<HDC>(wParam);
        if (GetDlgCtrlID(hCtl) == ID_EDIT_MEDIA_PATH) {
            SetTextColor(hdc, RGB(51, 65, 85));
            SetBkColor(hdc, RGB(248, 250, 252));
            static HBRUSH s_hBrMedia = CreateSolidBrush(RGB(248, 250, 252));
            return reinterpret_cast<INT_PTR>(s_hBrMedia);
        }
        SetTextColor(hdc, RGB(15, 23, 42));
        SetBkColor(hdc, RGB(255, 255, 255));
        return reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));
    }
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, RGB(15, 23, 42));
        SetBkColor(hdc, RGB(255, 255, 255));
        return reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));
    }
    case WM_MEASUREITEM:
    case WM_HSCROLL:
    case WM_COMMAND:
    case WM_NOTIFY:
    case WM_DRAWITEM:
        return SendMessageW(GetParent(hWnd), uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

std::wstring FormatDuration(int64_t seconds) {
    int64_t hrs = seconds / 3600;
    int64_t mins = (seconds % 3600) / 60;
    int64_t secs = seconds % 60;

    std::wstringstream ss;
    if (hrs > 0) {
        ss << hrs << L"小时 " << mins << L"分 " << secs << L"秒";
    } else if (mins > 0) {
        ss << mins << L"分 " << secs << L"秒";
    } else {
        ss << secs << L"秒";
    }
    return ss.str();
}

std::wstring FormatTimestamp(int64_t timestamp) {
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm tm{};
    localtime_s(&tm, &t);

    std::wstringstream ss;
    ss << std::put_time(&tm, L"%Y-%m-%d %H:%M");
    return ss.str();
}

COLORREF ParseHexColor(const std::string& hex) {
    if (hex.size() >= 7 && hex[0] == '#') {
        unsigned int r = 15, g = 23, b = 42;
        if (sscanf_s(hex.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            return RGB(r, g, b);
        }
    }
    return RGB(15, 23, 42);
}

} // namespace

ManagementWindow& ManagementWindow::Instance() {
    static ManagementWindow instance;
    return instance;
}

ManagementWindow::ManagementWindow() = default;

ManagementWindow::~ManagementWindow() {
    if (m_hFont) DeleteObject(m_hFont);
    if (m_hFontBold) DeleteObject(m_hFontBold);
    if (m_hFontSub) DeleteObject(m_hFontSub);
    if (m_hFontSection) DeleteObject(m_hFontSection);
    if (m_hFontTitle) DeleteObject(m_hFontTitle);

    if (m_hBrushBg) DeleteObject(m_hBrushBg);
    if (m_hBrushCard) DeleteObject(m_hBrushCard);
    if (m_hBrushInput) DeleteObject(m_hBrushInput);
    if (m_hBrushAccent) DeleteObject(m_hBrushAccent);
    if (m_hBrushBorder) DeleteObject(m_hBrushBorder);
    if (m_hBrushCardSub) DeleteObject(m_hBrushCardSub);
    if (m_hPenBorder) DeleteObject(m_hPenBorder);

    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

bool ManagementWindow::Create() {
    if (m_hWnd) return true;

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icex);

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // 浅灰背景、纯白卡片、皇家蓝强调色体系
    m_hBrushBg = CreateSolidBrush(RGB(245, 246, 248));       // #F5F6F8 现代浅灰底
    m_hBrushCard = CreateSolidBrush(RGB(255, 255, 255));     // #FFFFFF 纯白卡片底
    m_hBrushInput = CreateSolidBrush(RGB(255, 255, 255));    // #FFFFFF 白色输入底
    m_hBrushAccent = CreateSolidBrush(RGB(65, 100, 222));    // #4164DE 皇家蓝强调色
    m_hBrushBorder = CreateSolidBrush(RGB(226, 232, 240));   // #E2E8F0 Slate-200 边框
    m_hBrushCardSub = CreateSolidBrush(RGB(241, 245, 249));  // #F1F5F9 辅助浅灰底
    m_hPenBorder = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));

    // 注册主窗口类
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_hBrushBg;
    wc.lpszClassName = MANAGEMENT_WINDOW_CLASS;
    RegisterClassExW(&wc);

    // 注册面板类
    WNDCLASSEXW wcPanel{};
    wcPanel.cbSize = sizeof(WNDCLASSEXW);
    wcPanel.style = CS_HREDRAW | CS_VREDRAW;
    wcPanel.lpfnWndProc = TabPanelProc;
    wcPanel.hInstance = hInstance;
    wcPanel.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcPanel.hbrBackground = m_hBrushBg;
    wcPanel.lpszClassName = TAB_PANEL_CLASS;
    RegisterClassExW(&wcPanel);

    // 注册时钟实时预览控件类
    WNDCLASSEXW wcPreview{};
    wcPreview.cbSize = sizeof(WNDCLASSEXW);
    wcPreview.style = CS_HREDRAW | CS_VREDRAW;
    wcPreview.lpfnWndProc = ClockPreviewProc;
    wcPreview.hInstance = hInstance;
    wcPreview.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcPreview.hbrBackground = nullptr;
    wcPreview.lpszClassName = CLOCK_PREVIEW_CLASS;
    RegisterClassExW(&wcPreview);

    // 注册分段切换控件类
    WNDCLASSEXW wcSeg{};
    wcSeg.cbSize = sizeof(WNDCLASSEXW);
    wcSeg.style = CS_HREDRAW | CS_VREDRAW;
    wcSeg.lpfnWndProc = SegmentedControlProc;
    wcSeg.hInstance = hInstance;
    wcSeg.hCursor = LoadCursor(nullptr, IDC_HAND);
    wcSeg.hbrBackground = nullptr;
    wcSeg.lpszClassName = SEGMENTED_CONTROL_CLASS;
    RegisterClassExW(&wcSeg);

    // 注册平滑滑块类
    WNDCLASSEXW wcSlider{};
    wcSlider.cbSize = sizeof(WNDCLASSEXW);
    wcSlider.style = CS_HREDRAW | CS_VREDRAW;
    wcSlider.lpfnWndProc = SliderProc;
    wcSlider.hInstance = hInstance;
    wcSlider.hCursor = LoadCursor(nullptr, IDC_HAND);
    wcSlider.hbrBackground = nullptr;
    wcSlider.lpszClassName = SLIDER_CONTROL_CLASS;
    RegisterClassExW(&wcSlider);

    // 注册现代切换开关类
    WNDCLASSEXW wcToggle{};
    wcToggle.cbSize = sizeof(WNDCLASSEXW);
    wcToggle.style = CS_HREDRAW | CS_VREDRAW;
    wcToggle.lpfnWndProc = ToggleSwitchProc;
    wcToggle.hInstance = hInstance;
    wcToggle.hCursor = LoadCursor(nullptr, IDC_HAND);
    wcToggle.hbrBackground = nullptr;
    wcToggle.lpszClassName = TOGGLE_SWITCH_CLASS;
    RegisterClassExW(&wcToggle);

    // 严格遵循字体层次：正文 14px、粗体 14px、辅助说明 12px、分组标题 15px、页面主指标 20px
    m_hFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    m_hFontBold = CreateFontW(
        -14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    m_hFontSub = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    m_hFontSection = CreateFontW(
        -15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    m_hFontTitle = CreateFontW(
        -20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    int w = 864;
    int h = 728;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    m_hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        MANAGEMENT_WINDOW_CLASS,
        L"言律时钟 - 控制中心",
        WS_POPUP | WS_CLIPCHILDREN | WS_MINIMIZEBOX,
        x, y, w, h,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) return false;

    HRGN hRgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, 16, 16);
    SetWindowRgn(m_hWnd, hRgn, TRUE);

    CreateModernTabs(m_hWnd);
    CreateStatsPage(m_hWnd);
    CreateRecordsPage(m_hWnd);
    CreateSettingsPage(m_hWnd);

    // 底部全局操作栏
    m_hBtnCheckUpdate = CreateWindowW(
        L"BUTTON", L"检查新版本",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        24, 672, 106, 36,
        m_hWnd, reinterpret_cast<HMENU>(ID_BTN_CHECK_UPDATE), hInstance, nullptr
    );

    m_hBtnOpenDataDir = CreateWindowW(
        L"BUTTON", L"打开数据目录",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        138, 672, 116, 36,
        m_hWnd, reinterpret_cast<HMENU>(ID_BTN_OPEN_DATA_DIR), hInstance, nullptr
    );

    m_hBtnSaveSettings = CreateWindowW(
        L"BUTTON", L"保存并应用",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        704, 670, 136, 40,
        m_hWnd, reinterpret_cast<HMENU>(ID_BTN_SAVE_SETTINGS), hInstance, nullptr
    );

    SwitchTab(0);
    return true;
}

void ManagementWindow::CreateModernTabs(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // 顶部三大 Tab 导航
    m_hBtnTabStats = CreateWindowW(
        L"BUTTON", L"学习看板",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        180, 6, 96, 44,
        hWnd, reinterpret_cast<HMENU>(ID_TAB_STATS), hInstance, nullptr
    );

    m_hBtnTabRecords = CreateWindowW(
        L"BUTTON", L"专注记录",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        282, 6, 96, 44,
        hWnd, reinterpret_cast<HMENU>(ID_TAB_RECORDS), hInstance, nullptr
    );

    m_hBtnTabSettings = CreateWindowW(
        L"BUTTON", L"设置",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        384, 6, 80, 44,
        hWnd, reinterpret_cast<HMENU>(ID_TAB_SETTINGS), hInstance, nullptr
    );

    // 现代无边框窗口右上角控制按钮
    m_hBtnWinMin = CreateWindowW(
        L"BUTTON", L"—",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        864 - 82, 10, 32, 32,
        hWnd, reinterpret_cast<HMENU>(ID_BTN_WIN_MIN), hInstance, nullptr
    );

    m_hBtnWinClose = CreateWindowW(
        L"BUTTON", L"✕",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        864 - 44, 10, 32, 32,
        hWnd, reinterpret_cast<HMENU>(ID_BTN_WIN_CLOSE), hInstance, nullptr
    );
}

void ManagementWindow::CreateStatsPage(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    m_hPanelStats = CreateWindowExW(
        0, TAB_PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 52, 848, 606,
        hWnd, nullptr, hInstance, nullptr
    );
    SetWindowLongPtrW(m_hPanelStats, GWLP_USERDATA, 1);

    // 今日学习
    HWND hGrpToday = CreateWindowW(L"STATIC", L"今日专注", WS_CHILD | WS_VISIBLE, 42, 28, 350, 22, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(hGrpToday, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSection), TRUE);

    m_hStaticToday = CreateWindowW(L"STATIC", L"今日学习：0分 0秒 (0次)", WS_CHILD | WS_VISIBLE, 42, 56, 350, 32, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(m_hStaticToday, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontTitle), TRUE);

    // 累计学习
    HWND hGrpTotal = CreateWindowW(L"STATIC", L"累计专注", WS_CHILD | WS_VISIBLE, 420, 28, 350, 22, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(hGrpTotal, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSection), TRUE);

    m_hStaticTotal = CreateWindowW(L"STATIC", L"累计总计：0分 0秒 (0次)", WS_CHILD | WS_VISIBLE, 420, 56, 350, 32, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(m_hStaticTotal, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontTitle), TRUE);

    // 类别分布列表
    HWND hGrpCat = CreateWindowW(L"STATIC", L"类别专注分布明细", WS_CHILD | WS_VISIBLE, 42, 106, 350, 22, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(hGrpCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSection), TRUE);

    m_hListCatStats = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        42, 136, 764, 435,
        m_hPanelStats, nullptr, hInstance, nullptr
    );
    SendMessage(m_hListCatStats, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    ListView_SetExtendedListViewStyle(m_hListCatStats, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = const_cast<LPWSTR>(L"专注分类");
    col.cx = 260;
    col.iSubItem = 0;
    ListView_InsertColumn(m_hListCatStats, 0, &col);

    col.pszText = const_cast<LPWSTR>(L"专注时长");
    col.cx = 260;
    col.iSubItem = 1;
    ListView_InsertColumn(m_hListCatStats, 1, &col);

    col.pszText = const_cast<LPWSTR>(L"专注完成次数");
    col.cx = 220;
    col.iSubItem = 2;
    ListView_InsertColumn(m_hListCatStats, 2, &col);
}

void ManagementWindow::CreateRecordsPage(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    m_hPanelRecords = CreateWindowExW(
        0, TAB_PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 52, 848, 606,
        hWnd, nullptr, hInstance, nullptr
    );
    SetWindowLongPtrW(m_hPanelRecords, GWLP_USERDATA, 2);

    HWND hLblFilter = CreateWindowW(L"STATIC", L"筛选类别：", WS_CHILD | WS_VISIBLE, 42, 24, 75, 20, m_hPanelRecords, nullptr, hInstance, nullptr);
    SendMessage(hLblFilter, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hComboFilter = CreateWindowW(
        L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        122, 20, 200, 240,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_COMBO_FILTER), hInstance, nullptr
    );
    SendMessage(m_hComboFilter, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hBtnChangeCat = CreateWindowW(
        L"BUTTON", L"修改记录分类",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        520, 18, 136, 32,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_BTN_CHANGE_CAT), hInstance, nullptr
    );

    m_hBtnDeleteRecord = CreateWindowW(
        L"BUTTON", L"删除选定记录",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        670, 18, 136, 32,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_BTN_DELETE_RECORD), hInstance, nullptr
    );

    m_hListRecords = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        42, 64, 764, 505,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_LIST_RECORDS), hInstance, nullptr
    );
    SendMessage(m_hListRecords, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    ListView_SetExtendedListViewStyle(m_hListRecords, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.pszText = const_cast<LPWSTR>(L"完成时间");
    col.cx = 240;
    col.iSubItem = 0;
    ListView_InsertColumn(m_hListRecords, 0, &col);

    col.pszText = const_cast<LPWSTR>(L"专注分类");
    col.cx = 240;
    col.iSubItem = 1;
    ListView_InsertColumn(m_hListRecords, 1, &col);

    col.pszText = const_cast<LPWSTR>(L"专注时长");
    col.cx = 260;
    col.iSubItem = 2;
    ListView_InsertColumn(m_hListRecords, 2, &col);
}

void ManagementWindow::CreateSettingsPage(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    m_hPanelSettings = CreateWindowExW(
        0, TAB_PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 52, 848, 606,
        hWnd, nullptr, hInstance, nullptr
    );
    SetWindowLongPtrW(m_hPanelSettings, GWLP_USERDATA, 3); // 3 标识设置面板

    // ==========================================
    // 卡片 1：悬浮时钟 (x=24, y=56, w=450, h=194)
    // ==========================================
    // 待机显示
    HWND hLblIdle = CreateWindowW(L"STATIC", L"待机显示", WS_CHILD | WS_VISIBLE, 42, 108, 80, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblIdle, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hSegIdleMode = CreateWindowW(
        SEGMENTED_CONTROL_CLASS, L"当前时间|计划倒计时",
        WS_CHILD | WS_VISIBLE,
        240, 102, 214, 32,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_SEG_IDLE_MODE), hInstance, nullptr
    );

    // 背景不透明度
    HWND hLblOpac = CreateWindowW(L"STATIC", L"背景不透明度", WS_CHILD | WS_VISIBLE, 42, 148, 100, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblOpac, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hSliderOpacity = CreateWindowW(
        SLIDER_CONTROL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE,
        180, 146, 210, 24,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_SLIDER_OPACITY), hInstance, nullptr
    );

    m_hStaticOpacityVal = CreateWindowW(L"STATIC", L"85%", WS_CHILD | WS_VISIBLE | SS_RIGHT, 400, 148, 54, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(m_hStaticOpacityVal, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontBold), TRUE);

    // 时间大小与文字颜色
    HWND hLblFont = CreateWindowW(L"STATIC", L"时间大小", WS_CHILD | WS_VISIBLE, 42, 184, 80, 18, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblFont, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSub), TRUE);

    m_hComboFontSize = CreateWindowW(
        L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        42, 204, 190, 180,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_COMBO_FONT_SIZE), hInstance, nullptr
    );
    SendMessage(m_hComboFontSize, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    SendMessageW(m_hComboFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"微型 · 18 pt"));
    SendMessageW(m_hComboFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"标准 · 22 pt"));
    SendMessageW(m_hComboFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"醒目 · 26 pt"));
    SendMessageW(m_hComboFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"特大 · 32 pt"));

    HWND hLblCol = CreateWindowW(L"STATIC", L"文字颜色", WS_CHILD | WS_VISIBLE, 254, 184, 80, 18, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblCol, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSub), TRUE);

    m_hComboTextColor = CreateWindowW(
        L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        254, 204, 200, 180,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_COMBO_TEXT_COLOR), hInstance, nullptr
    );
    SendMessage(m_hComboTextColor, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"曜石黑"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"晨曦蓝"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"初音绿"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"活力橙"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"优雅紫"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"极净白"));

    // ==========================================
    // 卡片 2：外观预览 (x=488, y=56, w=336, h=194)
    // ==========================================
    m_hClockPreviewWnd = CreateWindowExW(
        0, CLOCK_PREVIEW_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE,
        506, 96, 300, 124,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_CLOCK_PREVIEW), hInstance, nullptr
    );

    // ==========================================
    // 卡片 3：休息设置 (x=24, y=262, w=800, h=160)
    // ==========================================
    HWND hLblFinish = CreateWindowW(L"STATIC", L"专注结束后", WS_CHILD | WS_VISIBLE, 42, 312, 100, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblFinish, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hSegBreakMode = CreateWindowW(
        SEGMENTED_CONTROL_CLASS, L"自动全屏休息|先提醒我",
        WS_CHILD | WS_VISIBLE,
        590, 306, 214, 32,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_SEG_BREAK_MODE), hInstance, nullptr
    );

    HWND hLblMedia = CreateWindowW(L"STATIC", L"休息背景", WS_CHILD | WS_VISIBLE, 42, 350, 80, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblMedia, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hEditMediaPath = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
        126, 348, 552, 24,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_EDIT_MEDIA_PATH), hInstance, nullptr
    );
    SendMessage(m_hEditMediaPath, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hBtnBrowseMedia = CreateWindowW(
        L"BUTTON", L"更换文件",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        696, 343, 108, 34,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_BROWSE_MEDIA), hInstance, nullptr
    );

    HWND hLblMuted = CreateWindowW(L"STATIC", L"静音播放", WS_CHILD | WS_VISIBLE, 42, 392, 80, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblMuted, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hToggleVideoMuted = CreateWindowW(
        TOGGLE_SWITCH_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE,
        760, 390, 44, 24,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_TOGGLE_VIDEO_MUTED), hInstance, nullptr
    );

    // ==========================================
    // 卡片 4：专注类别 (x=24, y=434, w=800, h=164)
    // ==========================================
    m_hListCategories = CreateWindowExW(
        0, L"LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | WS_VSCROLL,
        42, 482, 330, 102,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_LIST_CATEGORIES), hInstance, nullptr
    );
    SendMessage(m_hListCategories, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hEditNewCat = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        388, 482, 256, 32,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_EDIT_NEW_CAT), hInstance, nullptr
    );
    SendMessage(m_hEditNewCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    SendMessageW(m_hEditNewCat, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"输入新类别名称"));
    s_oldEditProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_hEditNewCat, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CueEditProc)));

    m_hBtnAddCat = CreateWindowW(
        L"BUTTON", L"添加类别",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        654, 482, 150, 32,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_ADD_CAT), hInstance, nullptr
    );

    m_hBtnRenameCat = CreateWindowW(
        L"BUTTON", L"重命名",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        388, 524, 124, 30,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_RENAME_CAT), hInstance, nullptr
    );

    m_hBtnDeleteCat = CreateWindowW(
        L"BUTTON", L"删除类别",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        520, 524, 124, 30,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_DELETE_CAT), hInstance, nullptr
    );
}

void ManagementWindow::SwitchTab(int tabIndex) {
    m_currentTabIndex = tabIndex;
    ShowWindow(m_hPanelStats, tabIndex == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hPanelRecords, tabIndex == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hPanelSettings, tabIndex == 2 ? SW_SHOW : SW_HIDE);

    // 重新绘制顶层 Tab 按钮状态
    InvalidateRect(m_hWnd, nullptr, TRUE);
    if (m_hBtnTabStats) InvalidateRect(m_hBtnTabStats, nullptr, TRUE);
    if (m_hBtnTabRecords) InvalidateRect(m_hBtnTabRecords, nullptr, TRUE);
    if (m_hBtnTabSettings) InvalidateRect(m_hBtnTabSettings, nullptr, TRUE);

    if (tabIndex == 0) RefreshStats();
    else if (tabIndex == 1) RefreshRecords();
    else if (tabIndex == 2) RefreshSettings();
}

void ManagementWindow::Show() {
    if (!m_hWnd) Create();
    ShowWindow(m_hWnd, SW_SHOW);
    SetForegroundWindow(m_hWnd);
    RefreshAll();
}

void ManagementWindow::Hide() {
    if (m_hWnd) ShowWindow(m_hWnd, SW_HIDE);
}

bool ManagementWindow::IsVisible() const {
    return m_hWnd && IsWindowVisible(m_hWnd);
}

void ManagementWindow::RefreshAll() {
    RefreshStats();
    RefreshRecords();
    RefreshSettings();
}

void ManagementWindow::RefreshStats() {
    StudyStatistics stats = Repository::Instance().GetOverallStatistics();
    auto catStats = Repository::Instance().GetCategoryStatistics();

    // 更新今日和累计
    std::wstringstream ssToday;
    ssToday << L"今日专注：" << FormatDuration(stats.todayDurationSeconds) << L" (" << stats.todayCount << L"次)";
    SetWindowTextW(m_hStaticToday, ssToday.str().c_str());

    std::wstringstream ssTotal;
    ssTotal << L"累计总计：" << FormatDuration(stats.totalDurationSeconds) << L" (" << stats.totalCount << L"次)";
    SetWindowTextW(m_hStaticTotal, ssTotal.str().c_str());

    // 刷新类别明细列表
    ListView_DeleteAllItems(m_hListCatStats);
    int idx = 0;
    for (const auto& cs : catStats) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = idx;
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(cs.categoryName.c_str());
        ListView_InsertItem(m_hListCatStats, &item);

        std::wstring durStr = FormatDuration(cs.totalDurationSeconds);
        ListView_SetItemText(m_hListCatStats, idx, 1, const_cast<LPWSTR>(durStr.c_str()));

        std::wstring countStr = std::to_wstring(cs.totalCount) + L" 次";
        ListView_SetItemText(m_hListCatStats, idx, 2, const_cast<LPWSTR>(countStr.c_str()));
        idx++;
    }
}

void ManagementWindow::RefreshRecords() {
    m_cachedCategories = Repository::Instance().GetAllCategories();
    m_cachedRecords = Repository::Instance().GetRecords();

    // 刷新筛选下拉框
    SendMessage(m_hComboFilter, CB_RESETCONTENT, 0, 0);
    SendMessageW(m_hComboFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"全部类别"));

    for (const auto& cat : m_cachedCategories) {
        SendMessageW(m_hComboFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(cat.name.c_str()));
    }
    SendMessage(m_hComboFilter, CB_SETCURSEL, 0, 0);

    // 刷新记录列表
    ListView_DeleteAllItems(m_hListRecords);
    int idx = 0;
    for (const auto& rec : m_cachedRecords) {
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = idx;
        item.iSubItem = 0;
        std::wstring timeStr = FormatTimestamp(rec.endTime);
        item.pszText = const_cast<LPWSTR>(timeStr.c_str());
        item.lParam = static_cast<LPARAM>(rec.id);
        ListView_InsertItem(m_hListRecords, &item);

        ListView_SetItemText(m_hListRecords, idx, 1, const_cast<LPWSTR>(rec.categoryName.c_str()));

        std::wstring durStr = FormatDuration(rec.actualDuration);
        ListView_SetItemText(m_hListRecords, idx, 2, const_cast<LPWSTR>(durStr.c_str()));
        idx++;
    }
}

void ManagementWindow::RefreshSettings() {
    AppConfig config;
    Repository::Instance().LoadConfig(config);

    // 1. 待机模式
    m_previewShowRealTime = config.showRealTimeWhenIdle;
    SendMessage(m_hSegIdleMode, BM_SETCHECK, config.showRealTimeWhenIdle ? 0 : 1, 0);

    // 2. 背景不透明度
    m_previewOpacity = config.clockOpacityPercent;
    SendMessageW(m_hSliderOpacity, TBM_SETPOS, TRUE, m_previewOpacity);
    SetWindowTextW(m_hStaticOpacityVal, (std::to_wstring(m_previewOpacity) + L"%").c_str());

    // 3. 字号
    m_previewFontSize = config.clockFontSize;
    int fontSel = 1; // 默认 22pt
    if (config.clockFontSize <= 18) fontSel = 0;
    else if (config.clockFontSize <= 22) fontSel = 1;
    else if (config.clockFontSize <= 26) fontSel = 2;
    else fontSel = 3;
    SendMessage(m_hComboFontSize, CB_SETCURSEL, fontSel, 0);

    // 4. 文字颜色
    m_previewTextColorHex = config.clockTextColor;
    int colorSel = 0;
    if (config.clockTextColor == "#2563EB") colorSel = 1;
    else if (config.clockTextColor == "#39C5BB") colorSel = 2;
    else if (config.clockTextColor == "#EA580C") colorSel = 3;
    else if (config.clockTextColor == "#7C3AED") colorSel = 4;
    else if (config.clockTextColor == "#FFFFFF") colorSel = 5;
    SendMessage(m_hComboTextColor, CB_SETCURSEL, colorSel, 0);

    // 5. 休息模式
    SendMessage(m_hSegBreakMode, BM_SETCHECK, (config.breakMode == BreakMode::Auto) ? 0 : 1, 0);

    // 6. 媒体文件路径
    m_fullMediaPath = config.customMediaPath;
    if (!m_fullMediaPath.empty()) {
        const wchar_t* fileName = PathFindFileNameW(m_fullMediaPath.c_str());
        SetWindowTextW(m_hEditMediaPath, (std::wstring(L"🎞  ") + fileName).c_str());
    } else {
        SetWindowTextW(m_hEditMediaPath, L"🎞  默认内置休息壁纸 (default_relax.jpg)");
    }
    UpdateMediaTooltip();

    // 7. 静音播放开关
    SendMessage(m_hToggleVideoMuted, BM_SETCHECK, config.videoMuted ? BST_CHECKED : BST_UNCHECKED, 0);

    // 8. 类别列表刷新
    m_cachedCategories = Repository::Instance().GetAllCategories();
    SendMessage(m_hListCategories, LB_RESETCONTENT, 0, 0);
    for (const auto& cat : m_cachedCategories) {
        SendMessageW(m_hListCategories, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(cat.name.c_str()));
    }
    if (!m_cachedCategories.empty()) {
        SendMessage(m_hListCategories, LB_SETCURSEL, 0, 0);
        SetWindowTextW(m_hEditNewCat, L"");
        // 默认第一项为“未分类”(id=1)，直接禁用重命名与删除按钮
        EnableWindow(m_hBtnRenameCat, FALSE);
        EnableWindow(m_hBtnDeleteCat, FALSE);
    }

    SetDirty(false);
    UpdatePreview();
}

void ManagementWindow::UpdateMediaTooltip() {
    if (!m_hTooltipMedia && m_hPanelSettings) {
        HINSTANCE hInstance = GetModuleHandle(nullptr);
        m_hTooltipMedia = CreateWindowExW(
            WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
            WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            m_hPanelSettings, nullptr, hInstance, nullptr
        );
    }
    if (m_hTooltipMedia && m_hEditMediaPath) {
        TOOLINFOW ti{};
        ti.cbSize = sizeof(TOOLINFOW);
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = m_hPanelSettings;
        ti.uId = reinterpret_cast<UINT_PTR>(m_hEditMediaPath);
        std::wstring tip = m_fullMediaPath.empty() ? L"当前使用系统默认内置壁纸" : m_fullMediaPath;
        ti.lpszText = const_cast<LPWSTR>(tip.c_str());
        SendMessageW(m_hTooltipMedia, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    }
}

void ManagementWindow::UpdatePreview() {
    if (m_hClockPreviewWnd) {
        InvalidateRect(m_hClockPreviewWnd, nullptr, TRUE);
    }
}

void ManagementWindow::SetDirty(bool dirty) {
    m_isDirty = dirty;
    if (m_hPanelSettings) {
        InvalidateRect(m_hPanelSettings, nullptr, FALSE);
    }
}

void ManagementWindow::OnChangeRecordCategory() {
    int sel = ListView_GetNextItem(m_hListRecords, -1, LVNI_SELECTED);
    if (sel < 0) {
        MessageBoxW(m_hWnd, L"请先在列表中选中一条专注记录！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = sel;
    ListView_GetItem(m_hListRecords, &item);
    int64_t recordId = static_cast<int64_t>(item.lParam);

    if (m_cachedCategories.empty()) return;
    int nextCatIdx = 0;
    for (size_t i = 0; i < m_cachedCategories.size(); ++i) {
        if (m_cachedCategories[i].name == m_cachedRecords[sel].categoryName) {
            nextCatIdx = static_cast<int>((i + 1) % m_cachedCategories.size());
            break;
        }
    }

    const auto& targetCat = m_cachedCategories[nextCatIdx];
    if (Repository::Instance().UpdateRecordCategory(recordId, targetCat.id)) {
        RefreshRecords();
        RefreshStats();
    }
}

void ManagementWindow::OnDeleteRecord() {
    int sel = ListView_GetNextItem(m_hListRecords, -1, LVNI_SELECTED);
    if (sel < 0) {
        MessageBoxW(m_hWnd, L"请先在列表中选中一条要删除的记录！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = sel;
    ListView_GetItem(m_hListRecords, &item);
    int64_t recordId = static_cast<int64_t>(item.lParam);

    int ret = MessageBoxW(m_hWnd, L"确定要删除这条专注记录吗？此操作无法撤销。", L"确认删除", MB_YESNO | MB_ICONQUESTION);
    if (ret == IDYES) {
        if (Repository::Instance().DeleteRecord(recordId)) {
            RefreshRecords();
            RefreshStats();
        }
    }
}

void ManagementWindow::OnAddCategory() {
    wchar_t buf[64];
    GetWindowTextW(m_hEditNewCat, buf, 64);
    std::wstring name(buf);
    if (name.empty()) {
        MessageBoxW(m_hWnd, L"请输入类别名称！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    if (Repository::Instance().AddCategory(name) > 0) {
        SetWindowTextW(m_hEditNewCat, L"");
        RefreshSettings();
        RefreshStats();
        RefreshRecords();
    } else {
        MessageBoxW(m_hWnd, L"添加失败，类别名称已存在！", L"错误", MB_OK | MB_ICONERROR);
    }
}

void ManagementWindow::OnRenameCategory() {
    int sel = static_cast<int>(SendMessage(m_hListCategories, LB_GETCURSEL, 0, 0));
    if (sel < 0 || sel >= static_cast<int>(m_cachedCategories.size())) {
        MessageBoxW(m_hWnd, L"请先在列表中选中一个类别！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    wchar_t buf[64];
    GetWindowTextW(m_hEditNewCat, buf, 64);
    std::wstring newName(buf);
    if (newName.empty()) {
        MessageBoxW(m_hWnd, L"请在输入框中填入新的类别名称！", L"提示", MB_OK | MB_ICONWARNING);
        return;
    }

    if (Repository::Instance().UpdateCategoryName(m_cachedCategories[sel].id, newName)) {
        SetWindowTextW(m_hEditNewCat, L"");
        RefreshSettings();
        RefreshStats();
        RefreshRecords();
    } else {
        MessageBoxW(m_hWnd, L"重命名失败，名称可能重复！", L"错误", MB_OK | MB_ICONERROR);
    }
}

void ManagementWindow::OnDeleteCategory() {
    int sel = static_cast<int>(SendMessage(m_hListCategories, LB_GETCURSEL, 0, 0));
    if (sel < 0 || sel >= static_cast<int>(m_cachedCategories.size())) {
        return;
    }

    const auto& cat = m_cachedCategories[sel];
    if (cat.id == 1) {
        return; // 默认分类不可删除
    }

    std::wstring prompt = L"确定要删除类别【" + cat.name + L"】吗？\n\n数据安全保证：\n该类别下的所有历史专注记录将完整保留并自动转入“未分类”，总专注时长与统计看板不受任何影响。";
    int ret = MessageBoxW(m_hWnd, prompt.c_str(), L"确认删除类别", MB_YESNO | MB_ICONQUESTION);
    if (ret == IDYES) {
        if (Repository::Instance().DeleteCategory(cat.id)) {
            SetWindowTextW(m_hEditNewCat, L"");
            RefreshSettings();
            RefreshStats();
            RefreshRecords();
        } else {
            MessageBoxW(m_hWnd, L"删除类别失败！", L"错误", MB_OK | MB_ICONERROR);
        }
    }
}

void ManagementWindow::OnBrowseMedia() {
    wchar_t fileName[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"媒体与视频文件\0*.mp4;*.wmv;*.avi;*.mkv;*.mov;*.m4v;*.jpg;*.jpeg;*.png;*.bmp;*.webp\0所有文件\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        m_fullMediaPath = fileName;
        SetWindowTextW(m_hEditMediaPath, (std::wstring(L"🎞  ") + PathFindFileNameW(fileName)).c_str());
        UpdateMediaTooltip();
        SetDirty(true);
    }
}

void ManagementWindow::OnSaveSettings() {
    AppConfig config;
    Repository::Instance().LoadConfig(config);

    // 1. 待机显示模式
    config.showRealTimeWhenIdle = (SendMessage(m_hSegIdleMode, BM_GETCHECK, 0, 0) == 0);

    // 2. 透明度
    config.clockOpacityPercent = m_previewOpacity;

    // 3. 字体大小
    int fontIdx = static_cast<int>(SendMessage(m_hComboFontSize, CB_GETCURSEL, 0, 0));
    const int fontSizes[] = { 18, 22, 26, 32 };
    if (fontIdx >= 0 && fontIdx < 4) {
        config.clockFontSize = fontSizes[fontIdx];
    }

    // 4. 文字颜色
    int colorIdx = static_cast<int>(SendMessage(m_hComboTextColor, CB_GETCURSEL, 0, 0));
    const char* const colors[] = { "#0F172A", "#2563EB", "#39C5BB", "#EA580C", "#7C3AED", "#FFFFFF" };
    if (colorIdx >= 0 && colorIdx < 6) {
        config.clockTextColor = colors[colorIdx];
    }

    // 5. 始终置顶（默认始终保持置顶）
    config.alwaysOnTop = true;

    // 6. 休息设置
    if (SendMessage(m_hSegBreakMode, BM_GETCHECK, 0, 0) == 0) {
        config.breakMode = BreakMode::Auto;
    } else {
        config.breakMode = BreakMode::Remind;
    }

    config.customMediaPath = m_fullMediaPath;
    config.videoMuted = (SendMessage(m_hToggleVideoMuted, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // 保持当前时钟实时坐标
    int curX, curY;
    FloatingClock::Instance().GetPosition(curX, curY);
    config.clockPosX = curX;
    config.clockPosY = curY;

    // 持久化保存
    Repository::Instance().SaveConfig(config);

    // 立即生效到桌面悬浮时钟
    FloatingClock::Instance().ApplyConfig(config);

    SetDirty(false);
}

LRESULT CALLBACK ManagementWindow::ClockPreviewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        // 0. 先填充纯白底色，消除圆角外部黑边
        HBRUSH hWhiteBg = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        FillRect(memDC, &rc, hWhiteBg);

        // 1. 模拟桌面浅灰蓝圆角画布 #E8EEF5
        HBRUSH hCanvasBr = CreateSolidBrush(RGB(232, 238, 245));
        HPEN hCanvasPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HGDIOBJ oldBr = SelectObject(memDC, hCanvasBr);
        HGDIOBJ oldPen = SelectObject(memDC, hCanvasPen);
        RoundRect(memDC, 0, 0, w, h, 14, 14);
        SelectObject(memDC, oldBr);
        SelectObject(memDC, oldPen);
        DeleteObject(hCanvasBr);
        DeleteObject(hCanvasPen);

        // 2. 居中绘制桌面悬浮时钟胶囊小部件
        auto& mw = ManagementWindow::Instance();
        int capW = 150;
        int capH = 52;
        int capX = (w - capW) / 2;
        int capY = (h - capH) / 2;

        float alpha = mw.m_previewOpacity / 100.0f;
        if (alpha > 0.0f) {
            HDC blendDC = CreateCompatibleDC(memDC);
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = capW;
            bmi.bmiHeader.biHeight = capH;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            void* pBits = nullptr;
            HBITMAP hBlendBmp = CreateDIBSection(blendDC, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
            HGDIOBJ oldBlendBmp = SelectObject(blendDC, hBlendBmp);

            HBRUSH hWhiteBr = CreateSolidBrush(RGB(255, 255, 255));
            RECT blendRc{ 0, 0, capW, capH };
            FillRect(blendDC, &blendRc, hWhiteBr);
            DeleteObject(hWhiteBr);

            BLENDFUNCTION bf{};
            bf.BlendOp = AC_SRC_OVER;
            bf.SourceConstantAlpha = static_cast<BYTE>(alpha * 255.0f);

            HRGN hRgn = CreateRoundRectRgn(capX, capY, capX + capW, capY + capH, 16, 16);
            SelectClipRgn(memDC, hRgn);
            GdiAlphaBlend(memDC, capX, capY, capW, capH, blendDC, 0, 0, capW, capH, bf);
            SelectClipRgn(memDC, nullptr);
            DeleteObject(hRgn);

            SelectObject(blendDC, oldBlendBmp);
            DeleteObject(hBlendBmp);
            DeleteDC(blendDC);

            HPEN hCapPen = CreatePen(PS_SOLID, 1, RGB(203, 213, 225));
            HGDIOBJ oldCapPen = SelectObject(memDC, hCapPen);
            HGDIOBJ oldCapBr = SelectObject(memDC, GetStockObject(NULL_BRUSH));
            RoundRect(memDC, capX, capY, capX + capW, capY + capH, 16, 16);
            SelectObject(memDC, oldCapPen);
            SelectObject(memDC, oldCapBr);
            DeleteObject(hCapPen);
        }

        // 3. 绘制居中时间数字
        HFONT hFontTime = CreateFontW(
            -(mw.m_previewFontSize + 2), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
        );
        HGDIOBJ oldTimeFont = SelectObject(memDC, hFontTime);
        COLORREF timeCol = ParseHexColor(mw.m_previewTextColorHex);
        SetTextColor(memDC, timeCol);
        SetBkMode(memDC, TRANSPARENT);

        std::wstring timeString;
        if (mw.m_previewShowRealTime) {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            tm localTm{};
            localtime_s(&localTm, &now);
            wchar_t buf[32];
            swprintf_s(buf, L"%02d:%02d", localTm.tm_hour, localTm.tm_min);
            timeString = buf;
        } else {
            timeString = L"25:00";
        }

        RECT timeRc{ capX, capY, capX + capW, capY + capH };
        DrawTextW(memDC, timeString.c_str(), -1, &timeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(memDC, oldTimeFont);
        DeleteObject(hFontTime);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ManagementWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ManagementWindow* self = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<ManagementWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<ManagementWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT ManagementWindow::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN: {
        POINT pt = { static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)) };
        if (pt.y < 52) {
            HWND hChild = ChildWindowFromPoint(hWnd, pt);
            if (hChild == hWnd || hChild == nullptr) {
                ReleaseCapture();
                SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                return 0;
            }
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, w, h);
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        // 1. 顶部白色导航栏背景 (0..52)
        RECT rcNav = { 0, 0, w, 52 };
        FillRect(memDC, &rcNav, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));

        // 2. 顶部左侧品牌文字 "⏱ 言律时钟" + "控制中心" 标签
        SetBkMode(memDC, TRANSPARENT);
        SelectObject(memDC, m_hFontBold);
        SetTextColor(memDC, RGB(15, 23, 42)); // Slate-900
        RECT rcBrand = { 20, 14, 110, 38 };
        DrawTextW(memDC, L"⏱ 言律时钟", -1, &rcBrand, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT rcTag = { 112, 16, 172, 36 };
        HBRUSH hTagBr = CreateSolidBrush(RGB(241, 245, 249)); // #F1F5F9
        HPEN hTagPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HGDIOBJ oB = SelectObject(memDC, hTagBr);
        HGDIOBJ oP = SelectObject(memDC, hTagPen);
        RoundRect(memDC, rcTag.left, rcTag.top, rcTag.right, rcTag.bottom, 6, 6);
        SelectObject(memDC, oB);
        SelectObject(memDC, oP);
        DeleteObject(hTagBr);
        DeleteObject(hTagPen);

        SelectObject(memDC, m_hFontSub);
        SetTextColor(memDC, RGB(100, 116, 139)); // Slate-500
        DrawTextW(memDC, L"控制中心", -1, &rcTag, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 3. 导航栏底部分割线 (y=51)
        HPEN hPenLine = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HGDIOBJ oldP = SelectObject(memDC, hPenLine);
        MoveToEx(memDC, 0, 51, nullptr);
        LineTo(memDC, w, 51);
        SelectObject(memDC, oldP);
        DeleteObject(hPenLine);

        // 4. 下半部分浅灰背景 (52..bottom)
        RECT rcBody = { 0, 52, w, h };
        HBRUSH hBodyBr = CreateSolidBrush(RGB(245, 246, 248));
        FillRect(memDC, &rcBody, hBodyBr);
        DeleteObject(hBodyBr);

        // 5. 整个无边框窗口外层 1px 圆角细边框
        HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HGDIOBJ oBorderP = SelectObject(memDC, hBorderPen);
        HGDIOBJ oBorderB = SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, 0, 0, w, h, 16, 16);
        SelectObject(memDC, oBorderP);
        SelectObject(memDC, oBorderB);
        DeleteObject(hBorderPen);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_MEASUREITEM: {
        auto mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (mis && mis->CtlID == ID_LIST_CATEGORIES) {
            mis->itemHeight = 32;
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLORDLG: {
        return reinterpret_cast<INT_PTR>(m_hBrushBg);
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(30, 41, 59)); // Slate-800
        return reinterpret_cast<INT_PTR>(m_hBrushCard);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, RGB(15, 23, 42)); // Slate-900
        SetBkColor(hdc, RGB(255, 255, 255));
        return reinterpret_cast<INT_PTR>(m_hBrushInput);
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(30, 41, 59));
        return reinterpret_cast<INT_PTR>(m_hBrushCard);
    }
    case WM_HSCROLL: {
        // 透明度滑块变动
        if (reinterpret_cast<HWND>(lParam) == m_hSliderOpacity) {
            int pos = static_cast<int>(SendMessageW(m_hSliderOpacity, TBM_GETPOS, 0, 0));
            m_previewOpacity = pos;
            SetWindowTextW(m_hStaticOpacityVal, (std::to_wstring(pos) + L"%").c_str());
            SetDirty(true);
            UpdatePreview();
            return 0;
        }
        break;
    }
    case WM_DRAWITEM: {
        auto dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!dis) break;
        int id = static_cast<int>(dis->CtlID);
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;

        if (id == ID_BTN_WIN_MIN) {
            bool isDown = (dis->itemState & ODS_SELECTED) != 0;
            if (isDown) {
                HBRUSH hBr = CreateSolidBrush(RGB(241, 245, 249));
                FillRect(hdc, &rc, hBr);
                DeleteObject(hBr);
            }
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(100, 116, 139));
            SelectObject(hdc, m_hFont);
            DrawTextW(hdc, L"—", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_BTN_WIN_CLOSE) {
            bool isDown = (dis->itemState & ODS_SELECTED) != 0;
            if (isDown) {
                HBRUSH hBr = CreateSolidBrush(RGB(239, 68, 68));
                FillRect(hdc, &rc, hBr);
                DeleteObject(hBr);
            }
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isDown ? RGB(255, 255, 255) : RGB(100, 116, 139));
            SelectObject(hdc, m_hFont);
            DrawTextW(hdc, L"✕", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_TAB_STATS || id == ID_TAB_RECORDS || id == ID_TAB_SETTINGS) {
            bool isActive = (id == ID_TAB_STATS && m_currentTabIndex == 0) ||
                            (id == ID_TAB_RECORDS && m_currentTabIndex == 1) ||
                            (id == ID_TAB_SETTINGS && m_currentTabIndex == 2);

            HBRUSH hBr = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
            FillRect(hdc, &rc, hBr);

            if (isActive) {
                // 活动 Tab 底部 #4164DE 皇家蓝指示条
                RECT rcIndicator = { rc.left + 16, rc.bottom - 3, rc.right - 16, rc.bottom - 1 };
                HBRUSH hBrInd = CreateSolidBrush(RGB(65, 100, 222));
                FillRect(hdc, &rcIndicator, hBrInd);
                DeleteObject(hBrInd);
            }

            const wchar_t* title = L"";
            if (id == ID_TAB_STATS) title = L"学习看板";
            else if (id == ID_TAB_RECORDS) title = L"专注记录";
            else if (id == ID_TAB_SETTINGS) title = L"设置";

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isActive ? RGB(65, 100, 222) : RGB(100, 116, 139));
            SelectObject(hdc, isActive ? m_hFontBold : m_hFont);
            DrawTextW(hdc, title, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_LIST_CATEGORIES) {
            bool isSel = (dis->itemState & ODS_SELECTED) != 0;
            COLORREF bgCol = isSel ? RGB(238, 242, 255) : RGB(255, 255, 255);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            FillRect(hdc, &rc, hBr);
            DeleteObject(hBr);

            if (isSel) {
                HPEN hSelPen = CreatePen(PS_SOLID, 1, RGB(199, 210, 254)); // #C7D2FE
                HGDIOBJ oldP = SelectObject(hdc, hSelPen);
                HGDIOBJ oldB = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                RoundRect(hdc, rc.left + 2, rc.top + 2, rc.right - 2, rc.bottom - 2, 6, 6);
                SelectObject(hdc, oldP);
                SelectObject(hdc, oldB);
                DeleteObject(hSelPen);
            }

            wchar_t itemText[64] = { 0 };
            SendMessageW(dis->hwndItem, LB_GETTEXT, dis->itemID, reinterpret_cast<LPARAM>(itemText));

            SetBkMode(hdc, TRANSPARENT);
            if (dis->itemID == 0) { // 未分类
                SelectObject(hdc, m_hFontBold);
                SetTextColor(hdc, isSel ? RGB(65, 100, 222) : RGB(15, 23, 42));
                RECT rcName = { rc.left + 12, rc.top, rc.left + 72, rc.bottom };
                DrawTextW(hdc, itemText, -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, m_hFontSub);
                SetTextColor(hdc, RGB(100, 116, 139));
                RECT rcDef = { rc.left + 76, rc.top, rc.left + 150, rc.bottom };
                DrawTextW(hdc, L"默认类别", -1, &rcDef, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                SetTextColor(hdc, RGB(148, 163, 184));
                RECT rcLock = { rc.right - 80, rc.top, rc.right - 12, rc.bottom };
                DrawTextW(hdc, L"不可删除", -1, &rcLock, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            } else {
                SelectObject(hdc, m_hFont);
                SetTextColor(hdc, isSel ? RGB(65, 100, 222) : RGB(30, 41, 59));
                RECT rcName = { rc.left + 12, rc.top, rc.right - 12, rc.bottom };
                DrawTextW(hdc, itemText, -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            return TRUE;
        } else if (id == ID_BTN_SAVE_SETTINGS) {
            // 唯一高强调保存按钮：#4164DE 皇家蓝，纯白文字，8px 圆角
            bool isDown = (dis->itemState & ODS_SELECTED) != 0;
            COLORREF bgCol = isDown ? RGB(42, 69, 168) : RGB(65, 100, 222);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            HPEN hPen = CreatePen(PS_SOLID, 1, bgCol);
            HGDIOBJ oldBr = SelectObject(hdc, hBr);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(hBr);
            DeleteObject(hPen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, m_hFontBold);
            DrawTextW(hdc, L"保存并应用", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_BTN_CHANGE_CAT || id == ID_BTN_ADD_CAT || id == ID_BTN_RENAME_CAT || 
                   id == ID_BTN_BROWSE_MEDIA || id == ID_BTN_CHECK_UPDATE || id == ID_BTN_OPEN_DATA_DIR) {
            bool isDown = (dis->itemState & ODS_SELECTED) != 0;
            COLORREF bgCol = isDown ? RGB(241, 245, 249) : RGB(255, 255, 255);
            COLORREF borderCol = isDown ? RGB(148, 163, 184) : RGB(226, 232, 240);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
            HGDIOBJ oldBr = SelectObject(hdc, hBr);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(hBr);
            DeleteObject(hPen);

            wchar_t btnText[64] = {0};
            GetWindowTextW(dis->hwndItem, btnText, 64);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(51, 65, 85));
            SelectObject(hdc, m_hFont);
            DrawTextW(hdc, btnText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_BTN_DELETE_RECORD || id == ID_BTN_DELETE_CAT) {
            bool isDisabled = (dis->itemState & ODS_DISABLED) != 0;
            bool isDown = (dis->itemState & ODS_SELECTED) != 0;

            COLORREF bgCol = isDisabled ? RGB(248, 250, 252) : (isDown ? RGB(254, 242, 242) : RGB(255, 255, 255));
            COLORREF borderCol = isDisabled ? RGB(226, 232, 240) : (isDown ? RGB(248, 113, 113) : RGB(254, 202, 202));
            COLORREF textCol = isDisabled ? RGB(148, 163, 184) : RGB(220, 38, 38);

            HBRUSH hBr = CreateSolidBrush(bgCol);
            HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
            HGDIOBJ oldBr = SelectObject(hdc, hBr);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(hBr);
            DeleteObject(hPen);

            wchar_t btnText[64] = {0};
            GetWindowTextW(dis->hwndItem, btnText, 64);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, textCol);
            SelectObject(hdc, m_hFont);
            DrawTextW(hdc, btnText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        WORD code = HIWORD(wParam);

        if (id == ID_BTN_WIN_MIN) {
            ShowWindow(m_hWnd, SW_MINIMIZE);
            return 0;
        } else if (id == ID_BTN_WIN_CLOSE) {
            Hide();
            return 0;
        } else if (id == ID_TAB_STATS) {
            SwitchTab(0);
            return 0;
        } else if (id == ID_TAB_RECORDS) {
            SwitchTab(1);
            return 0;
        } else if (id == ID_TAB_SETTINGS) {
            SwitchTab(2);
            return 0;
        } else if (id == ID_COMBO_FILTER && code == CBN_SELCHANGE) {
            RefreshRecords();
            return 0;
        } else if (id == ID_SEG_IDLE_MODE) {
            m_previewShowRealTime = (SendMessage(m_hSegIdleMode, BM_GETCHECK, 0, 0) == 0);
            SetDirty(true);
            UpdatePreview();
            return 0;
        } else if (id == ID_SEG_BREAK_MODE || id == ID_TOGGLE_VIDEO_MUTED) {
            SetDirty(true);
            return 0;
        } else if (id == ID_COMBO_FONT_SIZE && code == CBN_SELCHANGE) {
            int sel = static_cast<int>(SendMessage(m_hComboFontSize, CB_GETCURSEL, 0, 0));
            const int sizes[] = { 18, 22, 26, 32 };
            if (sel >= 0 && sel < 4) m_previewFontSize = sizes[sel];
            SetDirty(true);
            UpdatePreview();
            return 0;
        } else if (id == ID_COMBO_TEXT_COLOR && code == CBN_SELCHANGE) {
            int sel = static_cast<int>(SendMessage(m_hComboTextColor, CB_GETCURSEL, 0, 0));
            const char* const colors[] = { "#0F172A", "#2563EB", "#39C5BB", "#EA580C", "#7C3AED", "#FFFFFF" };
            if (sel >= 0 && sel < 6) m_previewTextColorHex = colors[sel];
            SetDirty(true);
            UpdatePreview();
            return 0;
        } else if (id == ID_LIST_CATEGORIES && code == LBN_SELCHANGE) {
            int sel = static_cast<int>(SendMessage(m_hListCategories, LB_GETCURSEL, 0, 0));
            if (sel >= 0 && sel < static_cast<int>(m_cachedCategories.size())) {
                const auto& cat = m_cachedCategories[sel];
                if (cat.id == 1) {
                    // 默认“未分类”：不可重命名、不可删除
                    SetWindowTextW(m_hEditNewCat, L"");
                    EnableWindow(m_hBtnRenameCat, FALSE);
                    EnableWindow(m_hBtnDeleteCat, FALSE);
                } else {
                    SetWindowTextW(m_hEditNewCat, cat.name.c_str());
                    EnableWindow(m_hBtnRenameCat, TRUE);
                    EnableWindow(m_hBtnDeleteCat, TRUE);
                }
            }
            return 0;
        } else if (id == ID_BTN_CHANGE_CAT) {
            OnChangeRecordCategory();
            return 0;
        } else if (id == ID_BTN_DELETE_RECORD) {
            OnDeleteRecord();
            return 0;
        } else if (id == ID_BTN_ADD_CAT) {
            OnAddCategory();
            return 0;
        } else if (id == ID_BTN_RENAME_CAT) {
            OnRenameCategory();
            return 0;
        } else if (id == ID_BTN_DELETE_CAT) {
            OnDeleteCategory();
            return 0;
        } else if (id == ID_BTN_BROWSE_MEDIA) {
            OnBrowseMedia();
            return 0;
        } else if (id == ID_BTN_SAVE_SETTINGS) {
            OnSaveSettings();
            return 0;
        } else if (id == ID_BTN_CHECK_UPDATE) {
            Updater::CheckForUpdatesAsync(m_hWnd, false);
            return 0;
        } else if (id == ID_BTN_OPEN_DATA_DIR) {
            Updater::OpenDataDirectory(m_hWnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE: {
        Hide();
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

} // namespace yanlv
