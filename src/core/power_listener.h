#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace yanlv {

class PowerListener {
public:
    static bool HandleWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

} // namespace yanlv
