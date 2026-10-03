#include "src/ui/management_window.h"
#include "src/db/repository.h"
#include <commctrl.h>
#include <commdlg.h>
#include <ctime>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace yanlv {

namespace {
const wchar_t* const MANAGEMENT_WINDOW_CLASS = L"YanlvManagementWindowClass";

enum ControlId {
    ID_TAB_CONTROL = 3001,
    ID_COMBO_FILTER,
    ID_LIST_RECORDS,
    ID_BTN_CHANGE_CAT,
    ID_BTN_DELETE_RECORD,
    ID_LIST_CATEGORIES,
    ID_EDIT_NEW_CAT,
    ID_BTN_ADD_CAT,
    ID_BTN_RENAME_CAT,
    ID_RADIO_AUTO_BREAK,
    ID_RADIO_REMIND_BREAK,
    ID_EDIT_MEDIA_PATH,
    ID_BTN_BROWSE_MEDIA,
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
    auto t = static_cast<std::time_t>(timestamp);
    std::tm tmVal{};
    localtime_s(&tmVal, &t);
    wchar_t buf[64];
    wcsftime(buf, 64, L"%Y-%m-%d %H:%M", &tmVal);
    return buf;
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
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

bool ManagementWindow::Create() {
    if (m_hWnd) return true;

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = MANAGEMENT_WINDOW_CLASS;

    RegisterClassExW(&wc);

    m_hFont = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei"
    );

    m_hFontBold = CreateFontW(
        -14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei"
    );

    int w = 620;
    int h = 480;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    m_hWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        MANAGEMENT_WINDOW_CLASS,
        L"言律时钟 - 控制中心 (统计与记录)",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) return false;

    CreateTabs(m_hWnd);
    CreateStatsPage(m_hWnd);
    CreateRecordsPage(m_hWnd);
    CreateSettingsPage(m_hWnd);

    SwitchTab(0);
    RefreshAll();

    return true;
}

void ManagementWindow::CreateTabs(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    m_hTab = CreateWindowW(
        WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        12, 12, 580, 416,
        hWnd, reinterpret_cast<HMENU>(ID_TAB_CONTROL), hInstance, nullptr
    );
    SendMessage(m_hTab, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    TCITEMW tie{};
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<LPWSTR>(L" 学习统计 ");
    TabCtrl_InsertItem(m_hTab, 0, &tie);

    tie.pszText = const_cast<LPWSTR>(L" 历史记录 ");
    TabCtrl_InsertItem(m_hTab, 1, &tie);

    tie.pszText = const_cast<LPWSTR>(L" 类别与设置 ");
    TabCtrl_InsertItem(m_hTab, 2, &tie);
}

void ManagementWindow::CreateStatsPage(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    m_hPanelStats = CreateWindowW(
        L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_BLACKFRAME,
        20, 48, 564, 370,
        hWnd, nullptr, hInstance, nullptr
    );

    // 概览卡片区域
    m_hStaticToday = CreateWindowW(
        L"STATIC", L"今日学习：0小时0分 (0次)",
        WS_CHILD | WS_VISIBLE,
        16, 16, 532, 28,
        m_hPanelStats, nullptr, hInstance, nullptr
    );
    SendMessage(m_hStaticToday, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontBold), TRUE);

    m_hStaticTotal = CreateWindowW(
        L"STATIC", L"总体累计：0小时0分 (0次)",
        WS_CHILD | WS_VISIBLE,
        16, 48, 532, 28,
        m_hPanelStats, nullptr, hInstance, nullptr
    );
    SendMessage(m_hStaticTotal, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontBold), TRUE);

    CreateWindowW(
        L"STATIC", L"各类别专注时长与次数占比：",
        WS_CHILD | WS_VISIBLE,
        16, 88, 532, 20,
        m_hPanelStats, nullptr, hInstance, nullptr
    );

    // 各类别统计列表
    m_hListCatStats = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        16, 114, 532, 240,
        m_hPanelStats, nullptr, hInstance, nullptr
    );
    ListView_SetExtendedListViewStyle(m_hListCatStats, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    SendMessage(m_hListCatStats, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 180;
    lvc.pszText = const_cast<LPWSTR>(L"类别名称");
    ListView_InsertColumn(m_hListCatStats, 0, &lvc);

    lvc.cx = 200;
    lvc.pszText = const_cast<LPWSTR>(L"累计时长");
    ListView_InsertColumn(m_hListCatStats, 1, &lvc);

    lvc.cx = 140;
    lvc.pszText = const_cast<LPWSTR>(L"专注次数");
    ListView_InsertColumn(m_hListCatStats, 2, &lvc);
}

void ManagementWindow::CreateRecordsPage(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    m_hPanelRecords = CreateWindowW(
        L"STATIC", L"",
        WS_CHILD | SS_BLACKFRAME,
        20, 48, 564, 370,
        hWnd, nullptr, hInstance, nullptr
    );

    CreateWindowW(L"STATIC", L"筛选类别：", WS_CHILD | WS_VISIBLE, 16, 16, 80, 20, m_hPanelRecords, nullptr, hInstance, nullptr);
    m_hComboFilter = CreateWindowW(
        L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        96, 12, 140, 200,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_COMBO_FILTER), hInstance, nullptr
    );
    SendMessage(m_hComboFilter, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hListRecords = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        16, 48, 532, 270,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_LIST_RECORDS), hInstance, nullptr
    );
    ListView_SetExtendedListViewStyle(m_hListRecords, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    SendMessage(m_hListRecords, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 100;
    lvc.pszText = const_cast<LPWSTR>(L"类别");
    ListView_InsertColumn(m_hListRecords, 0, &lvc);

    lvc.cx = 140;
    lvc.pszText = const_cast<LPWSTR>(L"开始时间");
    ListView_InsertColumn(m_hListRecords, 1, &lvc);

    lvc.cx = 90;
    lvc.pszText = const_cast<LPWSTR>(L"计划时长");
    ListView_InsertColumn(m_hListRecords, 2, &lvc);

    lvc.cx = 90;
    lvc.pszText = const_cast<LPWSTR>(L"实际时长");
    ListView_InsertColumn(m_hListRecords, 3, &lvc);

    lvc.cx = 100;
    lvc.pszText = const_cast<LPWSTR>(L"结束方式");
    ListView_InsertColumn(m_hListRecords, 4, &lvc);

    m_hBtnChangeCat = CreateWindowW(
        L"BUTTON", L"修改记录分类",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        16, 328, 120, 30,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_BTN_CHANGE_CAT), hInstance, nullptr
    );
    SendMessage(m_hBtnChangeCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hBtnDeleteRecord = CreateWindowW(
        L"BUTTON", L"删除选中记录",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        148, 328, 120, 30,
        m_hPanelRecords, reinterpret_cast<HMENU>(ID_BTN_DELETE_RECORD), hInstance, nullptr
    );
    SendMessage(m_hBtnDeleteRecord, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);
}

void ManagementWindow::CreateSettingsPage(HWND hWnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    m_hPanelSettings = CreateWindowW(
        L"STATIC", L"",
        WS_CHILD | SS_BLACKFRAME,
        20, 48, 564, 370,
        hWnd, nullptr, hInstance, nullptr
    );

    // 类别管理分组
    CreateWindowW(L"STATIC", L"【学习类别管理】", WS_CHILD | WS_VISIBLE, 16, 14, 180, 20, m_hPanelSettings, nullptr, hInstance, nullptr);

    m_hListCategories = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
        16, 38, 200, 130,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_LIST_CATEGORIES), hInstance, nullptr
    );
    SendMessage(m_hListCategories, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    CreateWindowW(L"STATIC", L"新类别名称：", WS_CHILD | WS_VISIBLE, 230, 40, 90, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    m_hEditNewCat = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE,
        230, 64, 180, 24,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_EDIT_NEW_CAT), hInstance, nullptr
    );
    SendMessage(m_hEditNewCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hBtnAddCat = CreateWindowW(
        L"BUTTON", L"添加类别",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        420, 62, 90, 28,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_ADD_CAT), hInstance, nullptr
    );
    SendMessage(m_hBtnAddCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hBtnRenameCat = CreateWindowW(
        L"BUTTON", L"重命名选中类别",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        230, 100, 130, 28,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_RENAME_CAT), hInstance, nullptr
    );
    SendMessage(m_hBtnRenameCat, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    // 休息设置分组
    CreateWindowW(L"STATIC", L"【五分钟全屏休息设置】", WS_CHILD | WS_VISIBLE, 16, 184, 200, 20, m_hPanelSettings, nullptr, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"学习结束后行为：", WS_CHILD | WS_VISIBLE, 16, 210, 120, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    m_hRadioAutoBreak = CreateWindowW(
        L"BUTTON", L"自动休息 (立即进入全屏休息)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        140, 208, 220, 20,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_RADIO_AUTO_BREAK), hInstance, nullptr
    );
    SendMessage(m_hRadioAutoBreak, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hRadioRemindBreak = CreateWindowW(
        L"BUTTON", L"提醒休息 (弹窗确认是否休息)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        370, 208, 180, 20,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_RADIO_REMIND_BREAK), hInstance, nullptr
    );
    SendMessage(m_hRadioRemindBreak, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    CreateWindowW(L"STATIC", L"自定义壁纸/视频：", WS_CHILD | WS_VISIBLE, 16, 246, 120, 20, m_hPanelSettings, nullptr, hInstance, nullptr);
    m_hEditMediaPath = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE,
        140, 244, 300, 24,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_EDIT_MEDIA_PATH), hInstance, nullptr
    );
    SendMessage(m_hEditMediaPath, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hBtnBrowseMedia = CreateWindowW(
        L"BUTTON", L"浏览...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        450, 242, 70, 28,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_BROWSE_MEDIA), hInstance, nullptr
    );
    SendMessage(m_hBtnBrowseMedia, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFont), TRUE);

    m_hBtnSaveSettings = CreateWindowW(
        L"BUTTON", L"保存设置",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        140, 310, 140, 34,
        m_hPanelSettings, reinterpret_cast<HMENU>(ID_BTN_SAVE_SETTINGS), hInstance, nullptr
    );
    SendMessage(m_hBtnSaveSettings, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontBold), TRUE);
}

void ManagementWindow::SwitchTab(int tabIndex) {
    ShowWindow(m_hPanelStats, tabIndex == 0 ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hPanelRecords, tabIndex == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(m_hPanelSettings, tabIndex == 2 ? SW_SHOW : SW_HIDE);

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

    std::wstring todayStr = L"今日学习：" + FormatDuration(stats.todayDurationSeconds) +
                           L" (" + std::to_wstring(stats.todayCount) + L"次)";
    SetWindowTextW(m_hStaticToday, todayStr.c_str());

    std::wstring totalStr = L"总体累计：" + FormatDuration(stats.totalDurationSeconds) +
                           L" (" + std::to_wstring(stats.totalCount) + L"次)";
    SetWindowTextW(m_hStaticTotal, totalStr.c_str());

    // 刷新类别占比列表
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
    // 刷新类别筛选框
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

    // 查询记录列表
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
    SendMessage(m_hListCategories, LB_RESETCONTENT, 0, 0);
    m_cachedCategories = Repository::Instance().GetAllCategories();
    for (const auto& cat : m_cachedCategories) {
        SendMessageW(m_hListCategories, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(cat.name.c_str()));
    }

    AppConfig config;
    Repository::Instance().LoadConfig(config);

    SendMessage(m_hRadioAutoBreak, BM_SETCHECK, config.breakMode == BreakMode::Auto ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessage(m_hRadioRemindBreak, BM_SETCHECK, config.breakMode == BreakMode::Remind ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(m_hEditMediaPath, config.customMediaPath.c_str());
}

void ManagementWindow::OnChangeRecordCategory() {
    int selIndex = ListView_GetNextItem(m_hListRecords, -1, LVNI_SELECTED);
    if (selIndex < 0 || selIndex >= static_cast<int>(m_cachedRecords.size())) {
        MessageBoxW(m_hWnd, L"请先在记录列表中选择一条记录！", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }

    const auto& rec = m_cachedRecords[selIndex];

    // 弹出快捷菜单让用户选择新类别
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
        L"确定删除此记录吗？\n删除后其时长和次数将从所属类别及总体统计中同步扣除。",
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
    ofn.lpstrFilter = L"图片与视频文件\0*.jpg;*.jpeg;*.png;*.bmp;*.webp;*.mp4;*.mkv;*.wmv\0所有文件\0*.*\0";
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

    if (SendMessage(m_hRadioAutoBreak, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        config.breakMode = BreakMode::Auto;
    } else {
        config.breakMode = BreakMode::Remind;
    }

    wchar_t buf[MAX_PATH];
    GetWindowTextW(m_hEditMediaPath, buf, MAX_PATH);
    config.customMediaPath = buf;

    Repository::Instance().SaveConfig(config);
    MessageBoxW(m_hWnd, L"设置保存成功！", L"成功", MB_OK | MB_ICONINFORMATION);
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
    case WM_NOTIFY: {
        auto nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (nmhdr->idFrom == ID_TAB_CONTROL && nmhdr->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(m_hTab);
            SwitchTab(sel);
            return 0;
        }
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == ID_COMBO_FILTER && code == CBN_SELCHANGE) {
            RefreshRecords();
            return 0;
        } else if (id == ID_BTN_CHANGE_CAT && code == BN_CLICKED) {
            OnChangeRecordCategory();
            return 0;
        } else if (id == ID_BTN_DELETE_RECORD && code == BN_CLICKED) {
            OnDeleteRecord();
            return 0;
        } else if (id == ID_BTN_ADD_CAT && code == BN_CLICKED) {
            OnAddCategory();
            return 0;
        } else if (id == ID_BTN_RENAME_CAT && code == BN_CLICKED) {
            OnRenameCategory();
            return 0;
        } else if (id == ID_BTN_BROWSE_MEDIA && code == BN_CLICKED) {
            OnBrowseMedia();
            return 0;
        } else if (id == ID_BTN_SAVE_SETTINGS && code == BN_CLICKED) {
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
