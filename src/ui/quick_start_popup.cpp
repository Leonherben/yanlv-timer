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
    ID_BTN_ADD_CATEGORY
};

struct InputBoxData {
    const wchar_t* prompt;
    std::wstring result;
    bool confirmed = false;
};

LRESULT CALLBACK InputBoxProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    auto data = reinterpret_cast<InputBoxData*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    switch (uMsg) {
    case WM_CREATE: {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        data = reinterpret_cast<InputBoxData*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));

        HFONT hFont = CreateFontW(
            -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei"
        );

        CreateWindowW(L"STATIC", data->prompt, WS_CHILD | WS_VISIBLE, 18, 16, 260, 20, hWnd, nullptr, cs->hInstance, nullptr);
        HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 18, 42, 260, 24, hWnd, reinterpret_cast<HMENU>(101), cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 110, 78, 76, 26, hWnd, reinterpret_cast<HMENU>(IDOK), cs->hInstance, nullptr);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 202, 78, 76, 26, hWnd, reinterpret_cast<HMENU>(IDCANCEL), cs->hInstance, nullptr);

        EnumChildWindows(hWnd, [](HWND hChild, LPARAM lp) -> BOOL {
            SendMessage(hChild, WM_SETFONT, lp, TRUE);
            return TRUE;
        }, reinterpret_cast<LPARAM>(hFont));

        SetFocus(hEdit);
        return 0;
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
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = InputBoxProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = INPUT_BOX_CLASS;
        RegisterClassExW(&wc);
        registered = true;
    }

    InputBoxData data;
    data.prompt = prompt;

    RECT rcParent{0, 0, 0, 0};
    if (hParent) GetWindowRect(hParent, &rcParent);
    int w = 310, h = 150;
    int x = rcParent.left + (rcParent.right - rcParent.left - w) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - h) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        INPUT_BOX_CLASS, title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h,
        hParent, nullptr, hInstance, &data
    );

    if (!hDlg) return false;

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
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

bool QuickStartPopup::Create() {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = POPUP_WINDOW_CLASS;

    RegisterClassExW(&wc);

    m_hFont = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei"
    );

    m_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE,
        POPUP_WINDOW_CLASS,
        L"开启专注",
        WS_POPUP | WS_BORDER | WS_CLIPCHILDREN,
        0, 0, 276, 195,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) return false;

    // 创建子控件
    // 类别标签、下拉框与新建按钮
    CreateWindowW(L"STATIC", L"专注类别：", WS_CHILD | WS_VISIBLE, 16, 14, 72, 20, m_hWnd, nullptr, hInstance, nullptr);
    m_hComboCategory = CreateWindowW(
        L"COMBOBOX", L"", 
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 
        88, 12, 138, 200, m_hWnd, reinterpret_cast<HMENU>(ID_COMBO_CATEGORY), hInstance, nullptr
    );
    m_hBtnAddCategory = CreateWindowW(
        L"BUTTON", L"+",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
        230, 11, 28, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_ADD_CATEGORY), hInstance, nullptr
    );

    // 时长标签与快捷按钮
    CreateWindowW(L"STATIC", L"专注时长：", WS_CHILD | WS_VISIBLE, 16, 50, 72, 20, m_hWnd, nullptr, hInstance, nullptr);
    m_hBtn25 = CreateWindowW(L"BUTTON", L"25m", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 88, 48, 48, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_25), hInstance, nullptr);
    m_hBtn45 = CreateWindowW(L"BUTTON", L"45m", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 142, 48, 48, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_45), hInstance, nullptr);
    m_hBtn60 = CreateWindowW(L"BUTTON", L"60m", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 196, 48, 48, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_60), hInstance, nullptr);

    // 自定义分钟输入
    CreateWindowW(L"STATIC", L"自定义(分)：", WS_CHILD | WS_VISIBLE, 16, 90, 72, 20, m_hWnd, nullptr, hInstance, nullptr);
    m_hEditDuration = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"25", 
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER | WS_TABSTOP, 
        88, 88, 60, 24, m_hWnd, reinterpret_cast<HMENU>(ID_EDIT_DURATION), hInstance, nullptr
    );

    // 开始按钮
    m_hBtnStart = CreateWindowW(
        L"BUTTON", L"开始专注", 
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 
        16, 134, 242, 36, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_START), hInstance, nullptr
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
    SetWindowTextW(m_hEditDuration, std::to_wstring(lastMinutes).c_str());
}

void QuickStartPopup::ShowNear(int anchorX, int anchorY) {
    if (!m_hWnd) Create();

    PopulateCategories();

    int w = 276;
    int h = 195;
    int x = anchorX;
    int y = anchorY + 60; // 悬浮窗下方

    // 防止超出屏幕边界
    RECT rcWork;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &rcWork, 0);
    if (x + w > rcWork.right) x = rcWork.right - w - 10;
    if (y + h > rcWork.bottom) y = anchorY - h - 10; // 向上弹出
    if (x < rcWork.left) x = rcWork.left + 10;
    if (y < rcWork.top) y = rcWork.top + 10;

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
        } else if (id == ID_BTN_25 && code == BN_CLICKED) {
            SetWindowTextW(m_hEditDuration, L"25");
            return 0;
        } else if (id == ID_BTN_45 && code == BN_CLICKED) {
            SetWindowTextW(m_hEditDuration, L"45");
            return 0;
        } else if (id == ID_BTN_60 && code == BN_CLICKED) {
            SetWindowTextW(m_hEditDuration, L"60");
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
