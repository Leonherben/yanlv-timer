#include "src/ui/management_window.h"
#include "src/ui/floating_clock.h"
#include "src/db/repository.h"
#include <commctrl.h>
#include <commdlg.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace yanlv {

namespace {
const wchar_t* const MANAGEMENT_WINDOW_CLASS = L"YanlvManagementWindowClass";
const wchar_t* const TAB_PANEL_CLASS = L"YanlvTabPanelClass";

LRESULT CALLBACK TabPanelProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        // 白色面板背景
        HBRUSH hWhite = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
        FillRect(hdc, &rc, hWhite);
        // 精细 1px Slate 浅灰边框
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
        HGDIOBJ oldPen = SelectObject(hdc, hPen);
        HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(hPen);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_COMMAND:
    case WM_NOTIFY:
    case WM_DRAWITEM:
        return SendMessageW(GetParent(hWnd), uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

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
    ID_RADIO_IDLE_REALTIME,
    ID_RADIO_IDLE_DURATION,
    ID_COMBO_OPACITY,
    ID_COMBO_FONT_SIZE,
    ID_COMBO_TEXT_COLOR,
    ID_CHECK_ALWAYS_ON_TOP,
    ID_RADIO_AUTO_BREAK,
    ID_RADIO_REMIND_BREAK,
    ID_EDIT_MEDIA_PATH,
    ID_BTN_BROWSE_MEDIA,
    ID_RADIO_VIDEO_MUTED,
    ID_RADIO_VIDEO_AUDIO,
    ID_BTN_SAVE_SETTINGS
};

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

} // namespace

ManagementWindow& ManagementWindow::Instance() {
    static ManagementWindow instance;
    return instance;
}

ManagementWindow::ManagementWindow() = default;

ManagementWindow::~ManagementWindow() {
    if (m_hFont) DeleteObject(m_hFont);
    if (m_hFontBold) DeleteObject(m_hFontBold);
    if (m_hFontTitle) DeleteObject(m_hFontTitle);
    if (m_hFontSection) DeleteObject(m_hFontSection);

    if (m_hBrushBg) DeleteObject(m_hBrushBg);
    if (m_hBrushCard) DeleteObject(m_hBrushCard);
    if (m_hBrushInput) DeleteObject(m_hBrushInput);
    if (m_hBrushAccent) DeleteObject(m_hBrushAccent);
    if (m_hBrushBorder) DeleteObject(m_hBrushBorder);
    if (m_hBrushCardSub) DeleteObject(m_hBrushCardSub);

    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

bool ManagementWindow::Create() {
    if (m_hWnd) return true;

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // 极简现代浅白主题画刷
    m_hBrushBg = CreateSolidBrush(RGB(248, 250, 252));       // Slate-50 窗口浅色底
    m_hBrushCard = CreateSolidBrush(RGB(255, 255, 255));     // Pure White 卡片/面板底
    m_hBrushInput = CreateSolidBrush(RGB(255, 255, 255));    // 白色输入框底
    m_hBrushAccent = CreateSolidBrush(RGB(37, 99, 235));     // 现代极光蓝
    m_hBrushBorder = CreateSolidBrush(RGB(226, 232, 240));   // Slate-200 边框
    m_hBrushCardSub = CreateSolidBrush(RGB(241, 245, 249));  // Slate-100 指标副底色

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_hBrushBg;
    wc.lpszClassName = MANAGEMENT_WINDOW_CLASS;
    RegisterClassExW(&wc);

    WNDCLASSEXW wcPanel{};
    wcPanel.cbSize = sizeof(WNDCLASSEXW);
    wcPanel.style = CS_HREDRAW | CS_VREDRAW;
    wcPanel.lpfnWndProc = TabPanelProc;
    wcPanel.hInstance = hInstance;
    wcPanel.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcPanel.hbrBackground = m_hBrushCard;
    wcPanel.lpszClassName = TAB_PANEL_CLASS;
    RegisterClassExW(&wcPanel);

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

    m_hFontSection = CreateFontW(
        -14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    m_hFontTitle = CreateFontW(
        -18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI"
    );

    int w = 680;
    int h = 570;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    m_hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        MANAGEMENT_WINDOW_CLASS,
        L"言律时钟 - 控制中心 (统计分析与外观设置)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) return false;

    CreateModernTabs(m_hWnd);
    CreateStatsPage(m_hWnd);
    CreateRecordsPage(m_hWnd);
    CreateSettingsPage(m_hWnd);

    SwitchTab(0);
    return true;
}

void ManagementWindow::CreateModernTabs(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    m_hBtnTabStats = CreateWindowW(
        L"BUTTON", L"📊 学习看板",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        16, 12, 130, 36,
        hWnd, reinterpret_cast<HMENU>(ID_TAB_STATS), hInstance, nullptr
    );

    m_hBtnTabRecords = CreateWindowW(
        L"BUTTON", L"📋 专注记录",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        152, 12, 130, 36,
        hWnd, reinterpret_cast<HMENU>(ID_TAB_RECORDS), hInstance, nullptr
    );

    m_hBtnTabSettings = CreateWindowW(
        L"BUTTON", L"⚙️ 时钟与系统设置",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        288, 12, 160, 36,
        hWnd, reinterpret_cast<HMENU>(ID_TAB_SETTINGS), hInstance, nullptr
    );
}

void ManagementWindow::CreateStatsPage(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    m_hPanelStats = CreateWindowExW(
        0, TAB_PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE,
        16, 56, 648, 470,
        hWnd, nullptr, hInstance, nullptr
    );

    // 今日学习卡片
    HWND hGrpToday = CreateWindowW(L"STATIC", L"今日专注", WS_CHILD | WS_VISIBLE, 16, 16, 298, 20, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(hGrpToday, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSection), TRUE);

    m_hStaticToday = CreateWindowW(L"STATIC", L"今日学习：0分 0秒 (0次)", WS_CHILD | WS_VISIBLE, 16, 42, 298, 30, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(m_hStaticToday, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontTitle), TRUE);

    // 总体累计卡片
    HWND hGrpTotal = CreateWindowW(L"STATIC", L"累计专注", WS_CHILD | WS_VISIBLE, 330, 16, 302, 20, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(hGrpTotal, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSection), TRUE);

    m_hStaticTotal = CreateWindowW(L"STATIC", L"总体累计：0分 0秒 (0次)", WS_CHILD | WS_VISIBLE, 330, 42, 302, 30, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(m_hStaticTotal, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontTitle), TRUE);

    // 类别分布列表
    HWND hGrpCat = CreateWindowW(L"STATIC", L"专注类别分布明细", WS_CHILD | WS_VISIBLE, 16, 90, 616, 20, m_hPanelStats, nullptr, hInstance, nullptr);
    SendMessage(hGrpCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSection), TRUE);

    m_hListCatStats = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        16, 116, 616, 338,
        m_hPanelStats, nullptr, hInstance, nullptr
    );
    SendMessage(m_hListCatStats, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    ListView_SetExtendedListViewStyle(m_hListCatStats, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(m_hListCatStats, RGB(255, 255, 255));
    ListView_SetTextBkColor(m_hListCatStats, RGB(255, 255, 255));
    ListView_SetTextColor(m_hListCatStats, RGB(15, 23, 42));

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.iSubItem = 0; lvc.cx = 200; lvc.pszText = const_cast<LPWSTR>(L"类别名称");
    ListView_InsertColumn(m_hListCatStats, 0, &lvc);

    lvc.iSubItem = 1; lvc.cx = 230; lvc.pszText = const_cast<LPWSTR>(L"累计专注时长");
    ListView_InsertColumn(m_hListCatStats, 1, &lvc);

    lvc.iSubItem = 2; lvc.cx = 180; lvc.pszText = const_cast<LPWSTR>(L"专注次数");
    ListView_InsertColumn(m_hListCatStats, 2, &lvc);
}

void ManagementWindow::CreateRecordsPage(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    m_hPanelRecords = CreateWindowExW(
        0, TAB_PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE,
        16, 56, 648, 470,
        hWnd, nullptr, hInstance, nullptr
    );

    HWND hLblFilter = CreateWindowW(L"STATIC", L"类别筛选：", WS_CHILD | WS_VISIBLE, 16, 16, 75, 22, m_hPanelRecords, nullptr, hInstance, nullptr);
    SendMessage(hLblFilter, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hComboFilter = CreateWindowW(
        L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        96, 13, 160, 200,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_COMBO_FILTER), hInstance, nullptr
    );
    SendMessage(m_hComboFilter, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hListRecords = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        16, 48, 616, 372,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_LIST_RECORDS), hInstance, nullptr
    );
    SendMessage(m_hListRecords, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    ListView_SetExtendedListViewStyle(m_hListRecords, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    ListView_SetBkColor(m_hListRecords, RGB(255, 255, 255));
    ListView_SetTextBkColor(m_hListRecords, RGB(255, 255, 255));
    ListView_SetTextColor(m_hListRecords, RGB(15, 23, 42));

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.iSubItem = 0; lvc.cx = 110; lvc.pszText = const_cast<LPWSTR>(L"学习分类");
    ListView_InsertColumn(m_hListRecords, 0, &lvc);

    lvc.iSubItem = 1; lvc.cx = 145; lvc.pszText = const_cast<LPWSTR>(L"开始时间");
    ListView_InsertColumn(m_hListRecords, 1, &lvc);

    lvc.iSubItem = 2; lvc.cx = 105; lvc.pszText = const_cast<LPWSTR>(L"计划时长");
    ListView_InsertColumn(m_hListRecords, 2, &lvc);

    lvc.iSubItem = 3; lvc.cx = 115; lvc.pszText = const_cast<LPWSTR>(L"实际有效时长");
    ListView_InsertColumn(m_hListRecords, 3, &lvc);

    lvc.iSubItem = 4; lvc.cx = 120; lvc.pszText = const_cast<LPWSTR>(L"结束状态");
    ListView_InsertColumn(m_hListRecords, 4, &lvc);

    m_hBtnChangeCat = CreateWindowW(
        L"BUTTON", L"修改记录分类",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        380, 428, 120, 32,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_BTN_CHANGE_CAT), hInstance, nullptr
    );

    m_hBtnDeleteRecord = CreateWindowW(
        L"BUTTON", L"删除此记录",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        512, 428, 120, 32,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_BTN_DELETE_RECORD), hInstance, nullptr
    );
}

void ManagementWindow::CreateSettingsPage(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    m_hPanelSettings = CreateWindowExW(
        0, TAB_PANEL_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE,
        16, 56, 648, 470,
        hWnd, nullptr, hInstance, nullptr
    );

    // ==========================================
    // 1. 悬浮时钟外观与行为
    // ==========================================
    HWND hSecClock = CreateWindowW(L"STATIC", L"【悬浮时钟外观与行为】", WS_CHILD | WS_VISIBLE, 16, 10, 240, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hSecClock, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSection), TRUE);

    HWND hLblIdle = CreateWindowW(L"STATIC", L"待机显示模式：", WS_CHILD | WS_VISIBLE, 16, 34, 110, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblIdle, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hRadioIdleRealTime = CreateWindowW(
        L"BUTTON", L"显示当前实际时间 (北京时间)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        130, 32, 220, 20,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_RADIO_IDLE_REALTIME), hInstance, nullptr
    );
    SendMessage(m_hRadioIdleRealTime, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hRadioIdleDuration = CreateWindowW(
        L"BUTTON", L"显示计划倒计时 (如 25:00)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        360, 32, 220, 20,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_RADIO_IDLE_DURATION), hInstance, nullptr
    );
    SendMessage(m_hRadioIdleDuration, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    // 背景透明度
    HWND hLblOpac = CreateWindowW(L"STATIC", L"背景透明度：", WS_CHILD | WS_VISIBLE, 16, 64, 90, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblOpac, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hComboOpacity = CreateWindowW(
        L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        110, 61, 140, 200,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_COMBO_OPACITY), hInstance, nullptr
    );
    SendMessage(m_hComboOpacity, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    SendMessageW(m_hComboOpacity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"100% 完全不透明"));
    SendMessageW(m_hComboOpacity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"85% 默认微透 (推荐)"));
    SendMessageW(m_hComboOpacity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"65% 半透明磨砂"));
    SendMessageW(m_hComboOpacity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"45% 高透明"));
    SendMessageW(m_hComboOpacity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"20% 极轻微透"));
    SendMessageW(m_hComboOpacity, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"0% 全透 (只留数字)"));

    // 时间字号
    HWND hLblFont = CreateWindowW(L"STATIC", L"时间大小：", WS_CHILD | WS_VISIBLE, 264, 64, 75, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblFont, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hComboFontSize = CreateWindowW(
        L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        342, 61, 120, 200,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_COMBO_FONT_SIZE), hInstance, nullptr
    );
    SendMessage(m_hComboFontSize, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    SendMessageW(m_hComboFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"微型 (18pt)"));
    SendMessageW(m_hComboFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"标准 (22pt)"));
    SendMessageW(m_hComboFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"醒目大 (26pt)"));
    SendMessageW(m_hComboFontSize, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"特大 (32pt)"));

    // 时间颜色
    HWND hLblCol = CreateWindowW(L"STATIC", L"文字颜色：", WS_CHILD | WS_VISIBLE, 474, 64, 75, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblCol, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hComboTextColor = CreateWindowW(
        L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        542, 61, 92, 200,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_COMBO_TEXT_COLOR), hInstance, nullptr
    );
    SendMessage(m_hComboTextColor, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"曜石黑 (推荐)"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"晨曦蓝"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"翡翠绿"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"活力橙"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"优雅紫"));
    SendMessageW(m_hComboTextColor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"极净白 (暗底)"));

    // 始终置顶
    m_hCheckAlwaysOnTop = CreateWindowW(
        L"BUTTON", L"悬浮时钟始终保持在最前 (置顶显示，即使点击任务栏也不被遮挡)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        16, 94, 460, 20,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_CHECK_ALWAYS_ON_TOP), hInstance, nullptr
    );
    SendMessage(m_hCheckAlwaysOnTop, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    // ==========================================
    // 2. 五分钟全屏休息设置
    // ==========================================
    HWND hSecBreak = CreateWindowW(L"STATIC", L"【五分钟全屏休息与视频】", WS_CHILD | WS_VISIBLE, 16, 126, 240, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hSecBreak, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSection), TRUE);

    HWND hLblFinish = CreateWindowW(L"STATIC", L"学习结束后行为：", WS_CHILD | WS_VISIBLE, 16, 150, 110, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblFinish, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hRadioAutoBreak = CreateWindowW(
        L"BUTTON", L"自动休息 (立即全屏休息)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        130, 148, 190, 20,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_RADIO_AUTO_BREAK), hInstance, nullptr
    );
    SendMessage(m_hRadioAutoBreak, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hRadioRemindBreak = CreateWindowW(
        L"BUTTON", L"提醒休息 (弹窗确认是否休息)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        330, 148, 220, 20,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_RADIO_REMIND_BREAK), hInstance, nullptr
    );
    SendMessage(m_hRadioRemindBreak, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    // 自定义壁纸/视频
    HWND hLblMedia = CreateWindowW(L"STATIC", L"壁纸或视频文件：", WS_CHILD | WS_VISIBLE, 16, 178, 110, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblMedia, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hEditMediaPath = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        130, 176, 400, 26,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_EDIT_MEDIA_PATH), hInstance, nullptr
    );
    SendMessage(m_hEditMediaPath, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hBtnBrowseMedia = CreateWindowW(
        L"BUTTON", L"浏览...",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        540, 174, 90, 28,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_BROWSE_MEDIA), hInstance, nullptr
    );

    // 视频声音
    HWND hLblAudio = CreateWindowW(L"STATIC", L"视频休息声音：", WS_CHILD | WS_VISIBLE, 16, 208, 110, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblAudio, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hRadioVideoMuted = CreateWindowW(
        L"BUTTON", L"静音播放 (推荐，安静休息)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        130, 206, 200, 20,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_RADIO_VIDEO_MUTED), hInstance, nullptr
    );
    SendMessage(m_hRadioVideoMuted, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hRadioVideoAudio = CreateWindowW(
        L"BUTTON", L"保留声音 (原声播放)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        340, 206, 180, 20,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_RADIO_VIDEO_AUDIO), hInstance, nullptr
    );
    SendMessage(m_hRadioVideoAudio, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    // ==========================================
    // 3. 专注类别管理
    // ==========================================
    HWND hSecCat = CreateWindowW(L"STATIC", L"【专注类别管理】", WS_CHILD | WS_VISIBLE, 16, 238, 200, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hSecCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontSection), TRUE);

    m_hListCategories = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
        16, 260, 260, 140,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_LIST_CATEGORIES), hInstance, nullptr
    );
    SendMessage(m_hListCategories, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    HWND hLblNewCat = CreateWindowW(L"STATIC", L"类别名称：", WS_CHILD | WS_VISIBLE, 290, 262, 80, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    SendMessage(hLblNewCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hEditNewCat = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE,
        370, 260, 260, 26,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_EDIT_NEW_CAT), hInstance, nullptr
    );
    SendMessage(m_hEditNewCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hBtnAddCat = CreateWindowW(
        L"BUTTON", L"添加新类别",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        370, 296, 120, 30,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_ADD_CAT), hInstance, nullptr
    );

    m_hBtnRenameCat = CreateWindowW(
        L"BUTTON", L"重命名选中类别",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        500, 296, 130, 30,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_RENAME_CAT), hInstance, nullptr
    );

    // ==========================================
    // 4. 保存设置主按钮
    // ==========================================
    m_hBtnSaveSettings = CreateWindowW(
        L"BUTTON", L"★ 保存并应用所有设置",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        16, 420, 220, 38,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_SAVE_SETTINGS), hInstance, nullptr
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

    std::wstring todayStr = L"今日专注：" + FormatDuration(stats.todayDurationSeconds) +
                           L" (" + std::to_wstring(stats.todayCount) + L"次)";
    SetWindowTextW(m_hStaticToday, todayStr.c_str());

    std::wstring totalStr = L"总体累计：" + FormatDuration(stats.totalDurationSeconds) +
                           L" (" + std::to_wstring(stats.totalCount) + L"次)";
    SetWindowTextW(m_hStaticTotal, totalStr.c_str());

    ListView_DeleteAllItems(m_hListCatStats);
    auto catStats = Repository::Instance().GetCategoryStatistics();
    for (size_t i = 0; i < catStats.size(); ++i) {
        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = static_cast<int>(i);
        lvi.pszText = const_cast<LPWSTR>(catStats[i].categoryName.c_str());
        ListView_InsertItem(m_hListCatStats, &lvi);

        std::wstring durStr = FormatDuration(catStats[i].totalDurationSeconds);
        ListView_SetItemText(m_hListCatStats, static_cast<int>(i), 1, const_cast<LPWSTR>(durStr.c_str()));

        std::wstring cntStr = std::to_wstring(catStats[i].totalCount) + L" 次";
        ListView_SetItemText(m_hListCatStats, static_cast<int>(i), 2, const_cast<LPWSTR>(cntStr.c_str()));
    }
}

void ManagementWindow::RefreshRecords() {
    m_cachedCategories = Repository::Instance().GetAllCategories();
    int currentSel = static_cast<int>(SendMessage(m_hComboFilter, CB_GETCURSEL, 0, 0));
    int64_t currentFilterCatId = 0;
    if (currentSel > 0 && currentSel <= static_cast<int>(m_cachedCategories.size())) {
        currentFilterCatId = m_cachedCategories[currentSel - 1].id;
    }

    SendMessage(m_hComboFilter, CB_RESETCONTENT, 0, 0);
    SendMessageW(m_hComboFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"全部分类"));
    int newSel = 0;
    for (size_t i = 0; i < m_cachedCategories.size(); ++i) {
        SendMessageW(m_hComboFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(m_cachedCategories[i].name.c_str()));
        if (m_cachedCategories[i].id == currentFilterCatId) {
            newSel = static_cast<int>(i) + 1;
        }
    }
    SendMessage(m_hComboFilter, CB_SETCURSEL, newSel, 0);

    ListView_DeleteAllItems(m_hListRecords);
    m_cachedRecords = Repository::Instance().GetRecords(currentFilterCatId, 300);

    for (size_t i = 0; i < m_cachedRecords.size(); ++i) {
        const auto& rec = m_cachedRecords[i];
        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = static_cast<int>(i);
        lvi.pszText = const_cast<LPWSTR>(rec.categoryName.c_str());
        ListView_InsertItem(m_hListRecords, &lvi);

        std::wstring timeStr = FormatTimestamp(rec.startTime);
        ListView_SetItemText(m_hListRecords, static_cast<int>(i), 1, const_cast<LPWSTR>(timeStr.c_str()));

        std::wstring planStr = FormatDuration(rec.plannedDuration);
        ListView_SetItemText(m_hListRecords, static_cast<int>(i), 2, const_cast<LPWSTR>(planStr.c_str()));

        std::wstring actStr = FormatDuration(rec.actualDuration);
        ListView_SetItemText(m_hListRecords, static_cast<int>(i), 3, const_cast<LPWSTR>(actStr.c_str()));

        std::wstring statusStr = L"正常完成";
        if (rec.finishType == FinishType::Aborted) statusStr = L"提前结束";
        else if (rec.finishType == FinishType::Interrupted) statusStr = L"中断恢复";
        ListView_SetItemText(m_hListRecords, static_cast<int>(i), 4, const_cast<LPWSTR>(statusStr.c_str()));
    }
}

void ManagementWindow::RefreshSettings() {
    AppConfig config;
    Repository::Instance().LoadConfig(config);

    // 1. 待机显示模式
    SendMessage(m_hRadioIdleRealTime, BM_SETCHECK, config.showRealTimeWhenIdle ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(m_hRadioIdleDuration, BM_SETCHECK, !config.showRealTimeWhenIdle ? BST_CHECKED : BST_UNCHECKED, 0);

    // 2. 透明度下拉
    int opSel = 1;
    if (config.clockOpacityPercent >= 95) opSel = 0;
    else if (config.clockOpacityPercent >= 75) opSel = 1;
    else if (config.clockOpacityPercent >= 55) opSel = 2;
    else if (config.clockOpacityPercent >= 35) opSel = 3;
    else if (config.clockOpacityPercent >= 10) opSel = 4;
    else opSel = 5;
    SendMessage(m_hComboOpacity, CB_SETCURSEL, opSel, 0);

    // 3. 字号下拉
    int fontSel = 1;
    if (config.clockFontSize <= 18) fontSel = 0;
    else if (config.clockFontSize <= 22) fontSel = 1;
    else if (config.clockFontSize <= 26) fontSel = 2;
    else fontSel = 3;
    SendMessage(m_hComboFontSize, CB_SETCURSEL, fontSel, 0);

    // 4. 文字颜色
    const char* const colors[] = { "#0F172A", "#2563EB", "#059669", "#EA580C", "#7C3AED", "#FFFFFF" };
    int colSel = 0;
    for (int i = 0; i < 6; ++i) {
        if (config.clockTextColor == colors[i]) {
            colSel = i;
            break;
        }
    }
    SendMessage(m_hComboTextColor, CB_SETCURSEL, colSel, 0);

    // 5. 始终置顶
    SendMessage(m_hCheckAlwaysOnTop, BM_SETCHECK, config.alwaysOnTop ? BST_CHECKED : BST_UNCHECKED, 0);

    // 6. 休息设置
    SendMessage(m_hRadioAutoBreak, BM_SETCHECK, config.breakMode == BreakMode::Auto ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(m_hRadioRemindBreak, BM_SETCHECK, config.breakMode == BreakMode::Remind ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(m_hEditMediaPath, config.customMediaPath.c_str());
    SendMessage(m_hRadioVideoMuted, BM_SETCHECK, config.videoMuted ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(m_hRadioVideoAudio, BM_SETCHECK, !config.videoMuted ? BST_CHECKED : BST_UNCHECKED, 0);

    // 7. 类别列表
    SendMessage(m_hListCategories, LB_RESETCONTENT, 0, 0);
    m_cachedCategories = Repository::Instance().GetAllCategories();
    for (const auto& cat : m_cachedCategories) {
        SendMessageW(m_hListCategories, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(cat.name.c_str()));
    }
}

void ManagementWindow::OnChangeRecordCategory() {
    int selIndex = ListView_GetNextItem(m_hListRecords, -1, LVNI_SELECTED);
    if (selIndex < 0 || selIndex >= static_cast<int>(m_cachedRecords.size())) {
        MessageBoxW(m_hWnd, L"请先在记录列表中选择一条记录！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    const auto& rec = m_cachedRecords[selIndex];

    HMENU hMenu = CreatePopupMenu();
    for (size_t i = 0; i < m_cachedCategories.size(); ++i) {
        AppendMenuW(hMenu, MF_STRING, 5000 + i, m_cachedCategories[i].name.c_str());
    }

    POINT pt;
    GetCursorPos(&pt);
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd >= 5000 && cmd < 5000 + static_cast<int>(m_cachedCategories.size())) {
        int catIndex = cmd - 5000;
        int64_t newCatId = m_cachedCategories[catIndex].id;
        if (newCatId != rec.categoryId) {
            Repository::Instance().UpdateRecordCategory(rec.id, newCatId);
            RefreshRecords();
            RefreshStats();
        }
    }
}

void ManagementWindow::OnDeleteRecord() {
    int selIndex = ListView_GetNextItem(m_hListRecords, -1, LVNI_SELECTED);
    if (selIndex < 0 || selIndex >= static_cast<int>(m_cachedRecords.size())) {
        MessageBoxW(m_hWnd, L"请先在记录列表中选择一条记录！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    int ret = MessageBoxW(
        m_hWnd,
        L"确定删除此记录吗？\n删除后其时长和次数将从所属类别及总体统计中扣除。",
        L"确认删除",
        MB_YESNO | MB_ICONQUESTION
    );

    if (ret == IDYES) {
        Repository::Instance().DeleteRecord(m_cachedRecords[selIndex].id);
        RefreshRecords();
        RefreshStats();
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

    int64_t newId = Repository::Instance().AddCategory(name);
    if (newId > 0) {
        SetWindowTextW(m_hEditNewCat, L"");
        RefreshSettings();
        RefreshStats();
    } else {
        MessageBoxW(m_hWnd, L"添加失败，可能已存在同名类别！", L"错误", MB_OK | MB_ICONERROR);
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
        SetWindowTextW(m_hEditMediaPath, fileName);
    }
}

void ManagementWindow::OnSaveSettings() {
    AppConfig config;
    Repository::Instance().LoadConfig(config);

    // 1. 待机显示模式
    config.showRealTimeWhenIdle = (SendMessage(m_hRadioIdleRealTime, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // 2. 透明度
    int opacityIdx = static_cast<int>(SendMessage(m_hComboOpacity, CB_GETCURSEL, 0, 0));
    const int opacities[] = { 100, 85, 65, 45, 20, 0 };
    if (opacityIdx >= 0 && opacityIdx < 6) {
        config.clockOpacityPercent = opacities[opacityIdx];
    }

    // 3. 字体大小
    int fontIdx = static_cast<int>(SendMessage(m_hComboFontSize, CB_GETCURSEL, 0, 0));
    const int fontSizes[] = { 18, 22, 26, 32 };
    if (fontIdx >= 0 && fontIdx < 4) {
        config.clockFontSize = fontSizes[fontIdx];
    }

    // 4. 文字颜色
    int colorIdx = static_cast<int>(SendMessage(m_hComboTextColor, CB_GETCURSEL, 0, 0));
    const char* const colors[] = { "#0F172A", "#2563EB", "#059669", "#EA580C", "#7C3AED", "#FFFFFF" };
    if (colorIdx >= 0 && colorIdx < 6) {
        config.clockTextColor = colors[colorIdx];
    }

    // 5. 始终置顶
    config.alwaysOnTop = (SendMessage(m_hCheckAlwaysOnTop, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // 6. 休息设置
    if (SendMessage(m_hRadioAutoBreak, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        config.breakMode = BreakMode::Auto;
    } else {
        config.breakMode = BreakMode::Remind;
    }

    wchar_t buf[MAX_PATH];
    GetWindowTextW(m_hEditMediaPath, buf, MAX_PATH);
    config.customMediaPath = buf;
    config.videoMuted = (SendMessage(m_hRadioVideoMuted, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // 保持当前时钟实时坐标
    int curX, curY;
    FloatingClock::Instance().GetPosition(curX, curY);
    config.clockPosX = curX;
    config.clockPosY = curY;

    // 持久化保存
    Repository::Instance().SaveConfig(config);

    // 立即生效到悬浮时钟
    FloatingClock::Instance().ApplyConfig(config);

    MessageBoxW(m_hWnd, L"设置已成功保存并立即应用！", L"设置成功", MB_OK | MB_ICONINFORMATION);
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
    case WM_DRAWITEM: {
        auto dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (!dis) break;
        int id = static_cast<int>(dis->CtlID);
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;

        if (id == ID_TAB_STATS || id == ID_TAB_RECORDS || id == ID_TAB_SETTINGS) {
            bool isActive = (id == ID_TAB_STATS && m_currentTabIndex == 0) ||
                            (id == ID_TAB_RECORDS && m_currentTabIndex == 1) ||
                            (id == ID_TAB_SETTINGS && m_currentTabIndex == 2);

            COLORREF bgCol = isActive ? RGB(255, 255, 255) : RGB(241, 245, 249);
            COLORREF borderCol = isActive ? RGB(203, 213, 225) : RGB(226, 232, 240);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
            HGDIOBJ oldBr = SelectObject(hdc, hBr);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(hBr);
            DeleteObject(hPen);

            if (isActive) {
                // 活动 Tab 底部极光蓝指示条
                RECT rcIndicator = { rc.left + 20, rc.bottom - 3, rc.right - 20, rc.bottom - 1 };
                HBRUSH hBrInd = CreateSolidBrush(RGB(37, 99, 235));
                FillRect(hdc, &rcIndicator, hBrInd);
                DeleteObject(hBrInd);
            }

            const wchar_t* title = L"";
            if (id == ID_TAB_STATS) title = L"📊 学习看板";
            else if (id == ID_TAB_RECORDS) title = L"📋 专注记录";
            else if (id == ID_TAB_SETTINGS) title = L"⚙️ 时钟与系统设置";

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, isActive ? RGB(15, 23, 42) : RGB(100, 116, 139));
            SelectObject(hdc, isActive ? m_hFontBold : m_hFont);
            DrawTextW(hdc, title, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_BTN_SAVE_SETTINGS) {
            // 保存设置主按钮：曜石黑底、纯白文字、圆角胶囊
            bool isDown = (dis->itemState & ODS_SELECTED) != 0;
            COLORREF bgCol = isDown ? RGB(30, 41, 59) : RGB(15, 23, 42);
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
            DrawTextW(hdc, L"★ 保存并应用所有设置", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        } else if (id == ID_BTN_CHANGE_CAT || id == ID_BTN_ADD_CAT || id == ID_BTN_RENAME_CAT || id == ID_BTN_BROWSE_MEDIA) {
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
        } else if (id == ID_BTN_DELETE_RECORD) {
            bool isDown = (dis->itemState & ODS_SELECTED) != 0;
            COLORREF bgCol = isDown ? RGB(254, 242, 242) : RGB(255, 255, 255);
            COLORREF borderCol = isDown ? RGB(248, 113, 113) : RGB(254, 202, 202);
            HBRUSH hBr = CreateSolidBrush(bgCol);
            HPEN hPen = CreatePen(PS_SOLID, 1, borderCol);
            HGDIOBJ oldBr = SelectObject(hdc, hBr);
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
            SelectObject(hdc, oldBr);
            SelectObject(hdc, oldPen);
            DeleteObject(hBr);
            DeleteObject(hPen);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(220, 38, 38));
            SelectObject(hdc, m_hFont);
            DrawTextW(hdc, L"删除此记录", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        WORD code = HIWORD(wParam);

        if (id == ID_TAB_STATS) {
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
        } else if (id == ID_BTN_BROWSE_MEDIA) {
            OnBrowseMedia();
            return 0;
        } else if (id == ID_BTN_SAVE_SETTINGS) {
            OnSaveSettings();
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
