#include "src/ui/quick_start_popup.h"
#include "src/core/timer_engine.h"
#include "src/db/repository.h"
#include "src/ui/floating_clock.h"
#include <commctrl.h>
#include <string>

namespace yanlv {

namespace {
const wchar_t* const POPUP_WINDOW_CLASS = L"YanlvQuickStartPopupClass";
const wchar_t* const INPUT_BOX_CLASS = L"YanlvQuickInputBoxClass";

enum ControlId {
    ID_COMBO_CATEGORY = 2001,
    ID_EDIT_DURATION,
    ID_BTN_25,
    ID_BTN_45,
    ID_BTN_60,
    ID_BTN_START,
    ID_BTN_ADD_CATEGORY,
    ID_BTN_CLOSE
};

struct InputBoxData {
    const wchar_t* title = nullptr;
    const wchar_t* prompt = nullptr;
    std::wstring result;
    bool confirmed = false;
    HFONT hFont = nullptr;
    HFONT hFontBold = nullptr;
};

LRESULT CALLBACK InputBoxProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto data = reinterpret_cast<InputBoxData*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    switch (uMsg) {
    case WM_CREATE: {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        data = reinterpret_cast<InputBoxData*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));

        data->hFont = CreateFontW(
            -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
        );
        data->hFontBold = CreateFontW(
            -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
        );

        HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 20, 56, 276, 26, hWnd, reinterpret_cast<HMENU>(101), cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 136, 96, 76, 30, hWnd, reinterpret_cast<HMENU>(IDOK), cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 220, 96, 76, 30, hWnd, reinterpret_cast<HMENU>(IDCANCEL), cs->hInstance, nullptr);

        EnumChildWindows(hWnd, [](HWND hChild, LPARAM lp) -> BOOL {
            SendMessage(hChild, WM_SETFONT, lp, TRUE);
            return TRUE;
        }, reinterpret_cast<LPARAM>(data->hFont));

        SetFocus(hEdit);
        return 0;
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

        HBRUSH hWhite = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        FillRect(memDC, &rc, hWhite);

        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HGDIOBJ oldPen = SelectObject(memDC, hPen);
        HGDIOBJ oldBr = SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, 0, 0, w, h, 14, 14);
        SelectObject(memDC, oldPen);
        SelectObject(memDC, oldBr);
        DeleteObject(hPen);

        if (data) {
            SetBkMode(memDC, TRANSPARENT);
            SelectObject(memDC, data->hFontBold);
            SetTextColor(memDC, RGB(15, 23, 42));
            RECT rcTitle = { 20, 14, w - 20, 34 };
            DrawTextW(memDC, data->title ? data->title : L"输入", -1, &rcTitle, DT_LEFT | DT_TOP | DT_SINGLELINE);

            SelectObject(memDC, data->hFont);
            SetTextColor(memDC, RGB(100, 116, 139));
            RECT rcPrompt = { 20, 36, w - 20, 54 };
            DrawTextW(memDC, data->prompt ? data->prompt : L"", -1, &rcPrompt, DT_LEFT | DT_TOP | DT_SINGLELINE);
        }

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int y = static_cast<short>(HIWORD(lParam));
        if (y < 48) {
            ReleaseCapture();
            SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
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
        bool isDown = (dis->itemState & ODS_SELECTED) != 0;

        if (id == IDOK) {
            COLORREF bgCol = isDown ? RGB(42, 69, 168) : RGB(65, 100, 222);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            HPEN hPen = CreatePen(PS_SOLID, 1, bgCol);
            HGDIOBJ oB = SelectObject(hdc, hBr);
            HGDIOBJ oP = SelectObject(hdc, hPen);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, oB);
            SelectObject(hdc, oP);
            DeleteObject(hBr);
            DeleteObject(hPen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, data && data->hFontBold ? data->hFontBold : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            DrawTextW(hdc, L"确定", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == IDCANCEL) {
            COLORREF bgCol = isDown ? RGB(226, 232, 240) : RGB(241, 245, 249);
            COLORREF borderCol = RGB(226, 232, 240);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
            HGDIOBJ oB = SelectObject(hdc, hBr);
            HGDIOBJ oP = SelectObject(hdc, hPen);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, oB);
            SelectObject(hdc, oP);
            DeleteObject(hBr);
            DeleteObject(hPen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(71, 85, 105));
            SelectObject(hdc, data && data->hFont ? data->hFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
            DrawTextW(hdc, L"取消", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, RGB(15, 23, 42));
        SetBkColor(hdc, RGB(255, 255, 255));
        return reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDOK) {
            HWND hEdit = GetDlgItem(hWnd, 101);
            wchar_t buf[128] = {0};
            GetWindowTextW(hEdit, buf, 128);
            if (data) {
                data->result = buf;
                data->confirmed = true;
            }
            DestroyWindow(hWnd);
            return 0;
        } else if (id == IDCANCEL) {
            DestroyWindow(hWnd);
            return 0;
        }
        break;
    }
    case WM_DESTROY: {
        if (data) {
            if (data->hFont) { DeleteObject(data->hFont); data->hFont = nullptr; }
            if (data->hFontBold) { DeleteObject(data->hFontBold); data->hFontBold = nullptr; }
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

bool ShowQuickInputBox(HWND hParent, const wchar_t* title, const wchar_t* prompt, std::wstring& outText) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = InputBoxProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        wc.lpszClassName = INPUT_BOX_CLASS;
        RegisterClassExW(&wc);
        registered = true;
    }

    InputBoxData data;
    data.title = title;
    data.prompt = prompt;

    RECT rcParent{0, 0, 0, 0};
    if (hParent) GetWindowRect(hParent, &rcParent);
    int w = 316, h = 144;
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        INPUT_BOX_CLASS, title,
        WS_POPUP | WS_VISIBLE,
        x, y, w, h,
        hParent, nullptr, hInstance, &data
    );

    if (!hDlg) return false;

    HRGN hRgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, 14, 14);
    SetWindowRgn(hDlg, hRgn, TRUE);

    EnableWindow(hParent, FALSE);
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_RETURN) {
                SendMessageW(hDlg, WM_COMMAND, IDOK, 0);
                continue;
            } else if (msg.wParam == VK_ESCAPE) {
                SendMessageW(hDlg, WM_COMMAND, IDCANCEL, 0);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (data.confirmed) {
        size_t first = data.result.find_first_not_of(L" \t\r\n");
        if (first == std::wstring::npos) return false;
        size_t last = data.result.find_last_not_of(L" \t\r\n");
        outText = data.result.substr(first, (last - first + 1));
        return !outText.empty();
    }
    return false;
}

} // namespace

QuickStartPopup& QuickStartPopup::Instance() {
    static QuickStartPopup instance;
    return instance;
}

QuickStartPopup::QuickStartPopup() = default;

QuickStartPopup::~QuickStartPopup() {
    if (m_hFont) DeleteObject(m_hFont);
    if (m_hFontBold) DeleteObject(m_hFontBold);
    if (m_hBrushBg) DeleteObject(m_hBrushBg);
    if (m_hBrushBorder) DeleteObject(m_hBrushBorder);
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

bool QuickStartPopup::Create() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    m_hBrushBg = CreateSolidBrush(RGB(255, 255, 255));
    m_hBrushBorder = CreateSolidBrush(RGB(226, 232, 240));

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_hBrushBg;
    wc.lpszClassName = POPUP_WINDOW_CLASS;

    RegisterClassExW(&wc);

    m_hFont = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    m_hFontBold = CreateFontW(
        -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    int w = 284;
    int h = 216;

    m_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        POPUP_WINDOW_CLASS,
        L"开启专注",
        WS_POPUP | WS_CLIPCHILDREN,
        0, 0, w, h,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) return false;

    HRGN hRgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, 14, 14);
    SetWindowRgn(m_hWnd, hRgn, TRUE);

    // 关闭小按钮
    m_hBtnClose = CreateWindowW(
        L"BUTTON", L"✕",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        250, 8, 24, 24, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_CLOSE), hInstance, nullptr
    );

    // 类别标签、下拉框与新建按钮
    CreateWindowW(L"STATIC", L"专注类别", WS_CHILD | WS_VISIBLE, 16, 48, 64, 20, m_hWnd, nullptr, hInstance, nullptr);
    m_hComboCategory = CreateWindowW(
        L"COMBOBOX", L"", 
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 
        84, 46, 154, 200, m_hWnd, reinterpret_cast<HMENU>(ID_COMBO_CATEGORY), hInstance, nullptr
    );
    m_hBtnAddCategory = CreateWindowW(
        L"BUTTON", L"+",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP,
        244, 45, 24, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_ADD_CATEGORY), hInstance, nullptr
    );

    // 时长标签与快捷按钮
    CreateWindowW(L"STATIC", L"预设时长", WS_CHILD | WS_VISIBLE, 16, 84, 64, 20, m_hWnd, nullptr, hInstance, nullptr);
    m_hBtn25 = CreateWindowW(L"BUTTON", L"25m", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 84, 82, 54, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_25), hInstance, nullptr);
    m_hBtn45 = CreateWindowW(L"BUTTON", L"45m", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 144, 82, 54, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_45), hInstance, nullptr);
    m_hBtn60 = CreateWindowW(L"BUTTON", L"60m", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 204, 82, 64, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_60), hInstance, nullptr);

    // 自定义分钟输入
    CreateWindowW(L"STATIC", L"自定义(分)", WS_CHILD | WS_VISIBLE, 16, 120, 68, 20, m_hWnd, nullptr, hInstance, nullptr);
    m_hEditDuration = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"25", 
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER | WS_TABSTOP, 
        88, 118, 56, 24, m_hWnd, reinterpret_cast<HMENU>(ID_EDIT_DURATION), hInstance, nullptr
    );

    // 开始按钮 (现代极简主按钮，皇家蓝高对比)
    m_hBtnStart = CreateWindowW(
        L"BUTTON", L"开始专注", 
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | WS_TABSTOP, 
        16, 158, 252, 38, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_START), hInstance, nullptr
    );

    // 绑定字体
    EnumChildWindows(m_hWnd, [](HWND hChild, LPARAM lParam) -> BOOL {
        HFONT hFont = reinterpret_cast<HFONT>(lParam);
        SendMessage(hChild, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(m_hFont));

    return true;
}

void QuickStartPopup::PopulateCategories() {
    SendMessage(m_hComboCategory, CB_RESETCONTENT, 0, 0);
    m_categories = Repository::Instance().GetAllCategories();

    AppConfig config;
    Repository::Instance().LoadConfig(config);

    int selectIndex = 0;
    for (size_t i = 0; i < m_categories.size(); ++i) {
        SendMessageW(m_hComboCategory, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(m_categories[i].name.c_str()));
        if (m_categories[i].id == config.lastCategoryId) {
            selectIndex = static_cast<int>(i);
        }
    }
    // 添加最后一项 "+ 新建类别..."
    SendMessageW(m_hComboCategory, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"+ 新建类别..."));

    SendMessage(m_hComboCategory, CB_SETCURSEL, selectIndex, 0);

    // 设置上次时长
    int lastMinutes = static_cast<int>(config.lastDurationSeconds / 60);
    if (lastMinutes <= 0) lastMinutes = 25;
    m_selectedPreset = (lastMinutes == 25 || lastMinutes == 45 || lastMinutes == 60) ? lastMinutes : -1;
    SetWindowTextW(m_hEditDuration, std::to_wstring(lastMinutes).c_str());
}

void QuickStartPopup::ShowNear(int anchorX, int anchorY) {
    if (!m_hWnd) Create();

    PopulateCategories();

    int w = 284;
    int h = 216;
    int x = anchorX;
    int y = anchorY + 60; // 悬浮窗下方

    // 防止超出屏幕边界
    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    if (x + w > rcWork.right) x = rcWork.right - w - 10;
    if (y + h > rcWork.bottom) y = anchorY - h - 10; // 向上弹出
    if (x < rcWork.left) x = rcWork.left + 10;
    if (y < rcWork.top) y = rcWork.top + 10;

    HRGN hRgn = CreateRoundRectRgn(0, 0, w + 1, h + 1, 14, 14);
    SetWindowRgn(m_hWnd, hRgn, TRUE);

    SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW);
    SetForegroundWindow(m_hWnd);
    SetFocus(m_hBtnStart);
}

void QuickStartPopup::Hide() {
    if (m_hWnd) {
        ShowWindow(m_hWnd, SW_HIDE);
    }
}

bool QuickStartPopup::IsVisible() const {
    return m_hWnd && IsWindowVisible(m_hWnd);
}

void QuickStartPopup::OnAddCategoryClicked() {
    std::wstring newName;
    if (ShowQuickInputBox(m_hWnd, L"新建学习类别", L"请输入新的学习类别名称：", newName)) {
        int64_t newId = Repository::Instance().AddCategory(newName);
        if (newId > 0) {
            AppConfig config;
            Repository::Instance().LoadConfig(config);
            config.lastCategoryId = newId;
            Repository::Instance().SaveConfig(config);

            PopulateCategories();
        } else {
            MessageBoxW(m_hWnd, L"添加失败，可能已存在同名类别！", L"提示", MB_OK | MB_ICONWARNING);
            PopulateCategories();
        }
    } else {
        PopulateCategories();
    }
}

void QuickStartPopup::OnStartClicked() {
    int curSel = static_cast<int>(SendMessage(m_hComboCategory, CB_GETCURSEL, 0, 0));
    int64_t catId = 1;
    if (curSel >= 0 && curSel < static_cast<int>(m_categories.size())) {
        catId = m_categories[curSel].id;
    }

    wchar_t buf[32];
    GetWindowTextW(m_hEditDuration, buf, 32);
    int minutes = 25;
    try {
        minutes = std::stoi(buf);
    } catch (...) {
        minutes = 25;
    }

    if (minutes <= 0) minutes = 1;
    if (minutes > 360) minutes = 360; // 最长 6 小时

    int64_t durationSeconds = static_cast<int64_t>(minutes) * 60;

    // 保存最后选择
    AppConfig config;
    Repository::Instance().LoadConfig(config);
    config.lastCategoryId = catId;
    config.lastDurationSeconds = durationSeconds;
    Repository::Instance().SaveConfig(config);

    Hide();

    // 启动学习
    TimerEngine::Instance().StartStudy(catId, durationSeconds);
}

LRESULT CALLBACK QuickStartPopup::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    QuickStartPopup* self = nullptr;
    if (uMsg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<QuickStartPopup*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<QuickStartPopup*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT QuickStartPopup::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
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

        // 1. 纯白底衬
        HBRUSH hWhite = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        FillRect(memDC, &rc, hWhite);

        // 2. 细微边框 #E2E8F0
        HPEN hBorderPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HGDIOBJ oldPen = SelectObject(memDC, hBorderPen);
        HGDIOBJ oldBr = SelectObject(memDC, GetStockObject(NULL_BRUSH));
        RoundRect(memDC, 0, 0, w, h, 14, 14);
        SelectObject(memDC, oldPen);
        SelectObject(memDC, oldBr);
        DeleteObject(hBorderPen);

        // 3. 顶部卡片标题 "开始专注"
        SetBkMode(memDC, TRANSPARENT);
        SelectObject(memDC, m_hFontBold);
        SetTextColor(memDC, RGB(15, 23, 42)); // Slate-900
        RECT rcTitle = { 16, 8, 220, 32 };
        DrawTextW(memDC, L"开始专注", -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 4. 分隔细线
        HPEN hSepPen = CreatePen(PS_SOLID, 1, RGB(241, 245, 249)); // Slate-100
        HGDIOBJ oP = SelectObject(memDC, hSepPen);
        MoveToEx(memDC, 16, 36, nullptr);
        LineTo(memDC, w - 16, 36);
        SelectObject(memDC, oP);
        DeleteObject(hSepPen);

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = static_cast<short>(LOWORD(lParam));
        int y = static_cast<short>(HIWORD(lParam));
        if (y < 36 && x < 240) {
            ReleaseCapture();
            SendMessageW(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            return 0;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(71, 85, 105)); // Slate-600
        return reinterpret_cast<INT_PTR>(m_hBrushBg);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        SetTextColor(hdc, RGB(15, 23, 42));
        SetBkColor(hdc, RGB(255, 255, 255));
        return reinterpret_cast<INT_PTR>(m_hBrushBg);
    }
    case WM_DRAWITEM: {
        auto dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!dis) break;
        int id = static_cast<int>(dis->CtlID);
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        bool isDown = (dis->itemState & ODS_SELECTED) != 0;

        if (id == ID_BTN_CLOSE) {
            if (isDown) {
                HBRUSH hBr = CreateSolidBrush(RGB(241, 245, 249));
                FillRect(hdc, &rc, hBr);
                DeleteObject(hBr);
            }
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isDown ? RGB(239, 68, 68) : RGB(148, 163, 184));
            SelectObject(hdc, m_hFont);
            DrawTextW(hdc, L"✕", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_BTN_START) {
            // 开始专注主按钮：#4164DE 皇家蓝、纯白粗体、圆角药丸
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
            SelectObject(hdc, m_hFontBold ? m_hFontBold : m_hFont);
            DrawTextW(hdc, L"开始专注", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_BTN_25 || id == ID_BTN_45 || id == ID_BTN_60) {
            int presetVal = (id == ID_BTN_25) ? 25 : ((id == ID_BTN_45) ? 45 : 60);
            bool isSelected = (m_selectedPreset == presetVal);

            COLORREF bgCol = isSelected ? RGB(238, 242, 255) : (isDown ? RGB(241, 245, 249) : RGB(255, 255, 255));
            COLORREF borderCol = isSelected ? RGB(165, 180, 252) : RGB(226, 232, 240);
            COLORREF textCol = isSelected ? RGB(65, 100, 222) : RGB(71, 85, 105);

            HBRUSH hBr = CreateSolidBrush(bgCol);
            HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
            HGDIOBJ oldBr = SelectObject(hdc, hBr);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(hBr);
            DeleteObject(hPen);

            const wchar_t* txt = (id == ID_BTN_25) ? L"25m" : (id == ID_BTN_45 ? L"45m" : L"60m");
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, textCol);
            SelectObject(hdc, isSelected ? m_hFontBold : m_hFont);
            DrawTextW(hdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_BTN_ADD_CATEGORY) {
            // 新建分类按钮：白底微框，带蓝色加号
            COLORREF bgCol = isDown ? RGB(238, 242, 255) : RGB(248, 250, 252);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
            HGDIOBJ oldBr = SelectObject(hdc, hBr);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(hBr);
            DeleteObject(hPen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(65, 100, 222));
            SelectObject(hdc, m_hFontBold ? m_hFontBold : m_hFont);
            DrawTextW(hdc, L"+", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_COMBO_CATEGORY && code == CBN_SELCHANGE) {
            int curSel = static_cast<int>(SendMessage(m_hComboCategory, CB_GETCURSEL, 0, 0));
            if (curSel == static_cast<int>(m_categories.size())) {
                OnAddCategoryClicked();
            }
            return 0;
        } else if (id == ID_BTN_ADD_CATEGORY && code == BN_CLICKED) {
            OnAddCategoryClicked();
            return 0;
        } else if (id == ID_BTN_CLOSE && code == BN_CLICKED) {
            Hide();
            return 0;
        } else if (id == ID_BTN_25 && code == BN_CLICKED) {
            m_selectedPreset = 25;
            SetWindowTextW(m_hEditDuration, L"25");
            InvalidateRect(m_hBtn25, nullptr, FALSE);
            InvalidateRect(m_hBtn45, nullptr, FALSE);
            InvalidateRect(m_hBtn60, nullptr, FALSE);
            return 0;
        } else if (id == ID_BTN_45 && code == BN_CLICKED) {
            m_selectedPreset = 45;
            SetWindowTextW(m_hEditDuration, L"45");
            InvalidateRect(m_hBtn25, nullptr, FALSE);
            InvalidateRect(m_hBtn45, nullptr, FALSE);
            InvalidateRect(m_hBtn60, nullptr, FALSE);
            return 0;
        } else if (id == ID_BTN_60 && code == BN_CLICKED) {
            m_selectedPreset = 60;
            SetWindowTextW(m_hEditDuration, L"60");
            InvalidateRect(m_hBtn25, nullptr, FALSE);
            InvalidateRect(m_hBtn45, nullptr, FALSE);
            InvalidateRect(m_hBtn60, nullptr, FALSE);
            return 0;
        } else if (id == ID_EDIT_DURATION && code == EN_CHANGE) {
            wchar_t buf[16] = {0};
            GetWindowTextW(m_hEditDuration, buf, 16);
            int val = 0;
            try { val = std::stoi(buf); } catch (...) {}
            int newPreset = (val == 25 || val == 45 || val == 60) ? val : -1;
            if (newPreset != m_selectedPreset) {
                m_selectedPreset = newPreset;
                InvalidateRect(m_hBtn25, nullptr, FALSE);
                InvalidateRect(m_hBtn45, nullptr, FALSE);
                InvalidateRect(m_hBtn60, nullptr, FALSE);
            }
            return 0;
        } else if (id == ID_BTN_START && code == BN_CLICKED) {
            OnStartClicked();
            return 0;
        }
        break;
    }
    case WM_ACTIVATE: {
        if (LOWORD(wParam) == WA_INACTIVE) {
            HWND hGaining = reinterpret_cast<HWND>(lParam);
            bool isInternal = false;
            if (hGaining) {
                if (hGaining == hWnd || IsChild(hWnd, hGaining)) {
                    isInternal = true;
                } else {
                    wchar_t clsName[32] = {0};
                    GetClassNameW(hGaining, clsName, 32);
                    if (wcscmp(clsName, L"ComboLBox") == 0 || wcscmp(clsName, INPUT_BOX_CLASS) == 0) {
                        isInternal = true;
                    }
                }
            }
            if (!isInternal) {
                Hide();
            }
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            Hide();
            return 0;
        } else if (wParam == VK_RETURN) {
            OnStartClicked();
            return 0;
        }
        break;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

} // namespace yanlv
