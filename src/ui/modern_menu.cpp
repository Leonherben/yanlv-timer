#include "src/ui/modern_menu.h"
#include <windowsx.h>
#include <dwmapi.h>

namespace yanlv {

namespace {

const wchar_t* const MODERN_MENU_CLASS = L"YanlvModernMenuWindowClass";

struct MenuWindowContext {
    std::vector<ModernMenuItem> items;
    std::vector<RECT> itemRects;
    int hoverIndex = -1;
    int selectedId = 0;
    bool isClosed = false;
    HFONT hFont = nullptr;
    HFONT hFontBold = nullptr;
};

LRESULT CALLBACK ModernMenuProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto ctx = reinterpret_cast<MenuWindowContext*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (uMsg) {
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

        // 1. 纯白背景与细边框 #E2E8F0
        HBRUSH hWhite = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        FillRect(memDC, &rc, hWhite);

        HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HGDIOBJ oldPen = SelectObject(memDC, hBorderPen);
        HGDIOBJ oldBr = SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, 0, 0, w, h, 14, 14);
        SelectObject(memDC, oldPen);
        SelectObject(memDC, oldBr);
        DeleteObject(hBorderPen);

        SetBkMode(memDC, TRANSPARENT);

        // 2. 绘制各项
        if (ctx) {
            for (size_t i = 0; i < ctx->items.size(); ++i) {
                const auto& item = ctx->items[i];
                const auto& itemRc = ctx->itemRects[i];

                if (item.isSeparator) {
                    HPEN hSepPen = CreatePen(PS_SOLID, 1, RGB(241, 245, 249));
                    HGDIOBJ oP = SelectObject(memDC, hSepPen);
                    int midY = (itemRc.top + itemRc.bottom) / 2;
                    MoveToEx(memDC, itemRc.left + 8, midY, nullptr);
                    LineTo(memDC, itemRc.right - 8, midY);
                    SelectObject(memDC, oP);
                    DeleteObject(hSepPen);
                    continue;
                }

                bool isHover = (ctx->hoverIndex == static_cast<int>(i)) && !item.isDisabled;

                if (isHover) {
                    COLORREF bgCol = item.isDanger ? RGB(254, 242, 242) : RGB(238, 242, 255); // #FEF2F2 或 #EEF2FF
                    COLORREF borderCol = item.isDanger ? RGB(254, 202, 202) : RGB(199, 210, 254);
                    HBRUSH hHoverBr = CreateSolidBrush(bgCol);
                    HPEN hHoverPen = CreatePen(PS_SOLID, 1, borderCol);
                    HGDIOBJ oB = SelectObject(memDC, hHoverBr);
                    HGDIOBJ oP = SelectObject(memDC, hHoverPen);
                    RoundRect(memDC, itemRc.left + 2, itemRc.top, itemRc.right - 2, itemRc.bottom, 8, 8);
                    SelectObject(memDC, oB);
                    SelectObject(memDC, oP);
                    DeleteObject(hHoverBr);
                    DeleteObject(hHoverPen);
                }

                // 勾选状态指示
                if (item.isChecked) {
                    RECT rcCheck = { itemRc.left + 8, itemRc.top, itemRc.left + 26, itemRc.bottom };
                    SelectObject(memDC, ctx->hFontBold);
                    SetTextColor(memDC, isHover ? RGB(65, 100, 222) : RGB(65, 100, 222));
                    DrawTextW(memDC, L"✓", -1, &rcCheck, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }

                // 文字
                RECT rcText = { itemRc.left + 30, itemRc.top, itemRc.right - 12, itemRc.bottom };
                COLORREF textCol;
                if (item.isDisabled) {
                    textCol = RGB(148, 163, 184); // #94A3B8
                } else if (item.isDanger) {
                    textCol = isHover ? RGB(220, 38, 38) : RGB(239, 68, 68); // #DC2626 / #EF4444
                } else {
                    textCol = isHover ? RGB(65, 100, 222) : RGB(30, 41, 59); // 悬停皇家蓝 / 常态深灰
                }

                SelectObject(memDC, (item.isChecked || isHover) ? ctx->hFontBold : ctx->hFont);
                SetTextColor(memDC, textCol);
                DrawTextW(memDC, item.text.c_str(), -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
        }

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_KILLFOCUS:
    case WM_CANCELMODE:
        if (ctx) ctx->isClosed = true;
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

} // namespace

int ModernMenu::Show(HWND hParent, int screenX, int screenY, const std::vector<ModernMenuItem>& items) {
    if (items.empty()) return 0;

    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    static bool s_registered = false;
    if (!s_registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ModernMenuProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = MODERN_MENU_CLASS;
        RegisterClassExW(&wc);
        s_registered = true;
    }

    MenuWindowContext ctx;
    ctx.items = items;

    ctx.hFont = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );
    ctx.hFontBold = CreateFontW(
        -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    // 测量与布局计算
    int menuWidth = 208;
    int curY = 8;
    ctx.itemRects.resize(items.size());

    for (size_t i = 0; i < items.size(); ++i) {
        int itemH = items[i].isSeparator ? 8 : 34;
        ctx.itemRects[i] = { 8, curY, menuWidth - 8, curY + itemH };
        curY += itemH;
    }
    int menuHeight = curY + 8;

    // 防止超出工作区屏幕边界
    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    int posX = screenX;
    int posY = screenY;
    if (posX + menuWidth > rcWork.right) posX = rcWork.right - menuWidth - 4;
    if (posY + menuHeight > rcWork.bottom) posY = posY - menuHeight;
    if (posX < rcWork.left) posX = rcWork.left + 4;
    if (posY < rcWork.top) posY = rcWork.top + 4;

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        MODERN_MENU_CLASS,
        L"",
        WS_POPUP,
        posX, posY, menuWidth, menuHeight,
        hParent, nullptr, hInstance, nullptr
    );

    if (!hWnd) {
        if (ctx.hFont) DeleteObject(ctx.hFont);
        if (ctx.hFontBold) DeleteObject(ctx.hFontBold);
        return 0;
    }

    SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ctx));

    // 设置精致圆角区域
    HRGN hRgn = CreateRoundRectRgn(0, 0, menuWidth + 1, menuHeight + 1, 14, 14);
    SetWindowRgn(hWnd, hRgn, TRUE);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);
    SetForegroundWindow(hWnd);
    SetCapture(hWnd);

    // 模态交互消息循环
    MSG msg;
    while (!ctx.isClosed && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                ctx.isClosed = true;
                break;
            } else if (msg.wParam == VK_DOWN) {
                int next = ctx.hoverIndex + 1;
                while (next < static_cast<int>(ctx.items.size()) && (ctx.items[next].isSeparator || ctx.items[next].isDisabled)) {
                    next++;
                }
                if (next < static_cast<int>(ctx.items.size())) {
                    ctx.hoverIndex = next;
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                continue;
            } else if (msg.wParam == VK_UP) {
                int prev = ctx.hoverIndex - 1;
                while (prev >= 0 && (ctx.items[prev].isSeparator || ctx.items[prev].isDisabled)) {
                    prev--;
                }
                if (prev >= 0) {
                    ctx.hoverIndex = prev;
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                continue;
            } else if (msg.wParam == VK_RETURN) {
                if (ctx.hoverIndex >= 0 && ctx.hoverIndex < static_cast<int>(ctx.items.size())) {
                    if (!ctx.items[ctx.hoverIndex].isSeparator && !ctx.items[ctx.hoverIndex].isDisabled) {
                        ctx.selectedId = ctx.items[ctx.hoverIndex].id;
                        ctx.isClosed = true;
                        break;
                    }
                }
                continue;
            }
        } else if (msg.message == WM_MOUSEMOVE) {
            POINT ptScreen = msg.pt;
            POINT ptClient = ptScreen;
            ScreenToClient(hWnd, &ptClient);

            int newHover = -1;
            for (size_t i = 0; i < ctx.items.size(); ++i) {
                if (!ctx.items[i].isSeparator && !ctx.items[i].isDisabled) {
                    if (PtInRect(&ctx.itemRects[i], ptClient)) {
                        newHover = static_cast<int>(i);
                        break;
                    }
                }
            }
            if (newHover != ctx.hoverIndex) {
                ctx.hoverIndex = newHover;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        } else if (msg.message == WM_LBUTTONUP) {
            POINT ptScreen = msg.pt;
            POINT ptClient = ptScreen;
            ScreenToClient(hWnd, &ptClient);

            for (size_t i = 0; i < ctx.items.size(); ++i) {
                if (!ctx.items[i].isSeparator && !ctx.items[i].isDisabled) {
                    if (PtInRect(&ctx.itemRects[i], ptClient)) {
                        ctx.selectedId = ctx.items[i].id;
                        break;
                    }
                }
            }
            ctx.isClosed = true;
            break;
        } else if (msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN || 
                   msg.message == WM_NCLBUTTONDOWN || msg.message == WM_NCRBUTTONDOWN) {
            POINT ptScreen = msg.pt;
            RECT rcWnd;
            GetWindowRect(hWnd, &rcWnd);
            if (!PtInRect(&rcWnd, ptScreen)) {
                // 点击在菜单外，关闭
                ctx.isClosed = true;
                break;
            }
        } else if (msg.message == WM_CANCELMODE || msg.message == WM_KILLFOCUS) {
            ctx.isClosed = true;
            break;
        } else if (msg.message == WM_CAPTURECHANGED) {
            if (reinterpret_cast<HWND>(msg.lParam) != hWnd) {
                ctx.isClosed = true;
                break;
            }
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ReleaseCapture();
    DestroyWindow(hWnd);

    if (ctx.hFont) DeleteObject(ctx.hFont);
    if (ctx.hFontBold) DeleteObject(ctx.hFontBold);

    return ctx.selectedId;
}

} // namespace yanlv
