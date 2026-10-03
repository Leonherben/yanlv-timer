#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlobj.h>
#include <string>
#include <thread>

#include "src/core/timer_engine.h"
#include "src/core/power_listener.h"
#include "src/db/repository.h"
#include "src/ui/floating_clock.h"
#include "src/ui/quick_start_popup.h"
#include "src/ui/break_fullscreen.h"
#include "src/ui/management_window.h"
#include "src/app/tray_icon.h"
#include "src/utils/updater.h"

namespace yanlv {

namespace {
const wchar_t* const SINGLE_INSTANCE_MUTEX = L"Global\\YanlvTimer_SingleInstance_Mutex";
const wchar_t* const HIDDEN_MSG_WINDOW_CLASS = L"YanlvMessageWindowClass";
constexpr UINT WM_TRAY_CALLBACK = WM_USER + 101;
constexpr UINT_PTR TIMER_ID_ENGINE_TICK = 1001;

enum TrayMenuId {
    ID_TRAY_TOGGLE_CLOCK = 4001,
    ID_TRAY_START_STUDY,
    ID_TRAY_MANAGEMENT,
    ID_TRAY_CHECK_UPDATE,
    ID_TRAY_EXIT
};

std::wstring GetDatabasePath() {
    wchar_t appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        std::wstring dir = std::wstring(appDataPath) + L"\\YanlvTimer";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir + L"\\yanlv.db";
    }
    return L"yanlv.db";
}

LRESULT CALLBACK MessageWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (PowerListener::HandleWindowMessage(hWnd, uMsg, wParam, lParam)) {
        return 0;
    }

    switch (uMsg) {
    case WM_TIMER: {
        if (wParam == TIMER_ID_ENGINE_TICK) {
            TimerEngine::Instance().Update();
        }
        return 0;
    }
    case WM_TRAY_CALLBACK: {
        if (lParam == WM_LBUTTONUP) {
            FloatingClock::Instance().ToggleVisibility();
        } else if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);

            HMENU hMenu = CreatePopupMenu();
            bool clockVisible = FloatingClock::Instance().IsVisible();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_TOGGLE_CLOCK, clockVisible ? L"隐藏悬浮时钟" : L"显示悬浮时钟");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_START_STUDY, L"开始专注...");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_MANAGEMENT, L"控制中心 (统计与记录)...");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_CHECK_UPDATE, L"检查更新...");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出言律时钟");

            SetForegroundWindow(hWnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
            DestroyMenu(hMenu);

            switch (cmd) {
            case ID_TRAY_TOGGLE_CLOCK:
                FloatingClock::Instance().ToggleVisibility();
                break;
            case ID_TRAY_START_STUDY: {
                int x, y;
                FloatingClock::Instance().GetPosition(x, y);
                FloatingClock::Instance().Show();
                QuickStartPopup::Instance().ShowNear(x, y);
                break;
            }
            case ID_TRAY_MANAGEMENT:
                ManagementWindow::Instance().Show();
                break;
            case ID_TRAY_CHECK_UPDATE:
                Updater::CheckForUpdatesAsync(nullptr, false);
                break;
            case ID_TRAY_EXIT:
                PostQuitMessage(0);
                break;
            }
        }
        return 0;
    }
    case WM_CLOSE: {
        DestroyWindow(hWnd);
        return 0;
    }
    case WM_DESTROY: {
        KillTimer(hWnd, TIMER_ID_ENGINE_TICK);
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

} // namespace
} // namespace yanlv

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    // 0. 解析命令行参数，支持开发调试模式父进程绑定 (--parent-pid <PID>)
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    DWORD parentPid = 0;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (std::wcscmp(argv[i], L"--parent-pid") == 0 && i + 1 < argc) {
                parentPid = std::wcstoul(argv[i + 1], nullptr, 10);
            }
        }
        LocalFree(argv);
    }

    // 1. 单实例保证
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, yanlv::SINGLE_INSTANCE_MUTEX);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"YanlvFloatingClockClass", nullptr);
        if (hExisting) {
            ShowWindow(hExisting, SW_SHOW);
            SetForegroundWindow(hExisting);
        }
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // 2. 初始化 COM (用于 Direct2D, WIC, Media Foundation)
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // 3. 初始化数据库
    std::wstring dbPath = yanlv::GetDatabasePath();
    yanlv::Repository::Instance().Initialize(dbPath);

    // 4. 加载用户配置
    yanlv::AppConfig config;
    yanlv::Repository::Instance().LoadConfig(config);

    // 5. 初始化核心计时引擎与状态恢复
    yanlv::TimerEngine::Instance().Initialize();

    // 6. 创建全局消息宿主窗口
    WNDCLASSEXW wcMsg{};
    wcMsg.cbSize = sizeof(WNDCLASSEXW);
    wcMsg.lpfnWndProc = yanlv::MessageWindowProc;
    wcMsg.hInstance = hInstance;
    wcMsg.lpszClassName = yanlv::HIDDEN_MSG_WINDOW_CLASS;
    RegisterClassExW(&wcMsg);

    HWND hMsgWnd = CreateWindowExW(
        0, yanlv::HIDDEN_MSG_WINDOW_CLASS, L"YanlvMessageHost",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr
    );

    // 如果指定了父进程 PID (例如由开发启动脚本启动)，监控父进程存活状态。
    // 父进程一旦退出 (比如用户 X 掉控制台窗口)，本程序立即自毁退出
    if (parentPid != 0) {
        HANDLE hParent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
        if (hParent) {
            std::thread([hParent, hMsgWnd]() {
                WaitForSingleObject(hParent, INFINITE);
                CloseHandle(hParent);
                if (hMsgWnd && IsWindow(hMsgWnd)) {
                    PostMessageW(hMsgWnd, WM_CLOSE, 0, 0);
                }
                Sleep(300);
                ExitProcess(0);
            }).detach();
        }
    }

    // 7. 初始化系统托盘
    yanlv::TrayIcon::Instance().Initialize(hMsgWnd, yanlv::WM_TRAY_CALLBACK);

    // 8. 创建悬浮时钟
    auto& clock = yanlv::FloatingClock::Instance();
    clock.SetPosition(config.clockPosX, config.clockPosY);
    clock.SetAlwaysOnTop(config.alwaysOnTop);
    clock.Create();
    clock.ApplyConfig(config);
    clock.Show();
    clock.UpdateDisplay(config.lastDurationSeconds, yanlv::TimerState::Idle);

    // 9. 连接交互回调
    clock.SetOnQuickStartRequested([&]() {
        int x, y;
        clock.GetPosition(x, y);
        yanlv::QuickStartPopup::Instance().ShowNear(x, y);
    });

    clock.SetOnOpenManagementRequested([&]() {
        yanlv::ManagementWindow::Instance().Show();
    });

    // 10. 绑定计时器事件响应
    auto& engine = yanlv::TimerEngine::Instance();

    engine.SetOnTick([&](int64_t remaining, int64_t /*elapsed*/) {
        yanlv::TimerState st = engine.GetState();
        clock.UpdateDisplay(remaining, st);

        if (yanlv::BreakFullscreen::Instance().IsActive()) {
            yanlv::BreakFullscreen::Instance().UpdateCountdown(remaining);
        }

        // 更新托盘提示信息
        wchar_t tipBuf[64];
        int64_t mins = remaining / 60;
        int64_t secs = remaining % 60;
        if (st == yanlv::TimerState::Studying) {
            swprintf_s(tipBuf, L"言律时钟 - FOCUS (%02lld:%02lld)", mins, secs);
        } else if (st == yanlv::TimerState::Paused) {
            swprintf_s(tipBuf, L"言律时钟 - PAUSE (%02lld:%02lld)", mins, secs);
        } else if (st == yanlv::TimerState::Breaking) {
            swprintf_s(tipBuf, L"言律时钟 - REST (%02lld:%02lld)", mins, secs);
        } else {
            swprintf_s(tipBuf, L"言律时钟 - IDLE");
        }
        yanlv::TrayIcon::Instance().UpdateTooltip(tipBuf);
    });

    engine.SetOnStateChange([&](yanlv::TimerState /*oldState*/, yanlv::TimerState newState) {
        int64_t rem = engine.GetRemainingSeconds();
        clock.UpdateDisplay(rem, newState);

        if (newState == yanlv::TimerState::Breaking) {
            yanlv::AppConfig curConfig;
            yanlv::Repository::Instance().LoadConfig(curConfig);
            clock.Hide();
            yanlv::BreakFullscreen::Instance().ShowBreak(curConfig.customMediaPath, curConfig.videoMuted);
        }
    });

    engine.SetOnBreakPrompt([&]() {
        // 提醒模式下倒计时结束：弹窗询问用户是否休息
        int res = MessageBoxW(
            clock.GetHwnd(),
            L"太棒了，本轮学习已顺利完成！\n是否立即开启五分钟全屏放松休息？",
            L"学习完成提醒",
            MB_YESNO | MB_ICONQUESTION | MB_TOPMOST
        );

        if (res == IDYES) {
            engine.ConfirmBreak(true);
        } else {
            engine.ConfirmBreak(false);
        }
    });

    engine.SetOnBreakFinished([&]() {
        yanlv::BreakFullscreen::Instance().CloseBreak();
        clock.Show();
        clock.UpdateDisplay(25 * 60, yanlv::TimerState::Idle);
    });

    // 11. 启动引擎驱动定时器 (150ms 轮询)
    SetTimer(hMsgWnd, yanlv::TIMER_ID_ENGINE_TICK, 150, nullptr);

    // 12. 消息循环
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 13. 程序正常退出前仅保存时钟坐标与置顶状态 (绝不覆盖用户通过控制中心保存的各项个性化设置)
    int saveX, saveY;
    clock.GetPosition(saveX, saveY);
    yanlv::Repository::Instance().SaveClockPosition(saveX, saveY, clock.IsAlwaysOnTop());

    // 14. 资源清理
    yanlv::TrayIcon::Instance().Remove();
    yanlv::Repository::Instance().Close();
    CoUninitialize();

    if (hMutex) {
        CloseHandle(hMutex);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR, int nShowCmd) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nShowCmd);
}
