#include "src/core/power_listener.h"
#include "src/core/timer_engine.h"

namespace yanlv {

bool PowerListener::HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_POWERBROADCAST) {
        if (wParam == PBT_APMSUSPEND) {
            // 系统进入休眠/睡眠，自动暂停学习计时
            TimerEngine::Instance().OnSystemSleep();
            return true;
        } else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            // 系统唤醒
            TimerEngine::Instance().OnSystemWake();
            return true;
        }
    }
    return false;
}

} // namespace yanlv
