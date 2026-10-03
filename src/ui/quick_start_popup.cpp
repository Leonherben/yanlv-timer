#include "src/ui/quick_start_popup.h"
#include "src/core/timer_engine.h"
#include "src/db/repository.h"
#include "src/ui/floating_clock.h"
#include <commctrl.h>
#include <string>

namespace yanlv {

namespace {
const wchar_t* const POPUP_WINDOW_CLASS = L"YanlvQuickStartPopupClass";

enum ControlId {
    ID_COMBO_CATEGORY = 2001,
    ID_EDIT_DURATION,
    ID_BTN_25,
    ID_BTN_45,
    ID_BTN_60,
    ID_BTN_START
};
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
        0, 0, 260, 200,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hWnd) return false;

    // 创建子控件
    // 类别标签与下拉框
    CreateWindowW(L"STATIC", L"专注类别：", WS_CHILD | WS_VISIBLE, 16, 14, 80, 20, m_hWnd, nullptr, hInstance, nullptr);
    m_hComboCategory = CreateWindowW(
        L"COMBOBOX", L"", 
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 
        88, 12, 150, 200, m_hWnd, reinterpret_cast<HMENU>(ID_COMBO_CATEGORY), hInstance, nullptr
    );

    // 时长标签与快捷按钮
    CreateWindowW(L"STATIC", L"专注时长：", WS_CHILD | WS_VISIBLE, 16, 50, 80, 20, m_hWnd, nullptr, hInstance, nullptr);
    m_hBtn25 = CreateWindowW(L"BUTTON", L"25m", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 88, 48, 46, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_25), hInstance, nullptr);
    m_hBtn45 = CreateWindowW(L"BUTTON", L"45m", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 140, 48, 46, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_45), hInstance, nullptr);
    m_hBtn60 = CreateWindowW(L"BUTTON", L"60m", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 192, 48, 46, 26, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_60), hInstance, nullptr);

    // 自定义分钟输入
    CreateWindowW(L"STATIC", L"自定义(分)：", WS_CHILD | WS_VISIBLE, 16, 90, 80, 20, m_hWnd, nullptr, hInstance, nullptr);
    m_hEditDuration = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"25", 
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER | WS_TABSTOP, 
        88, 88, 70, 24, m_hWnd, reinterpret_cast<HMENU>(ID_EDIT_DURATION), hInstance, nullptr
    );

    // 开始按钮
    m_hBtnStart = CreateWindowW(
        L"BUTTON", L"开始专注", 
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 
        16, 136, 222, 36, m_hWnd, reinterpret_cast<HMENU>(ID_BTN_START), hInstance, nullptr
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
    SendMessage(m_hComboCategory, CB_SETCURSEL, selectIndex, 0);

    // 设置上次时长
    int lastMinutes = static_cast<int>(config.lastDurationSeconds / 60);
    if (lastMinutes <= 0) lastMinutes = 25;
    SetWindowTextW(m_hEditDuration, std::to_wstring(lastMinutes).c_str());
}

void QuickStartPopup::ShowNear(int anchorX, int anchorY) {
    if (!m_hWnd) Create();

    PopulateCategories();

    // 计算弹窗居中贴合位置
    int w = 260;
    int h = 190;
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

        if (id == ID_BTN_25 && code == BN_CLICKED) {
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
                    if (wcscmp(clsName, L"ComboLBox") == 0) {
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
