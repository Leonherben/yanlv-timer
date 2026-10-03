#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

namespace yanlv {

struct ModernMenuItem {
    int id = 0;
    std::wstring text;
    bool isSeparator = false;
    bool isChecked = false;
    bool isDanger = false;
    bool isDisabled = false;

    static ModernMenuItem Item(int id, const std::wstring& text, bool isChecked = false, bool isDanger = false, bool isDisabled = false) {
        ModernMenuItem m;
        m.id = id;
        m.text = text;
        m.isChecked = isChecked;
        m.isDanger = isDanger;
        m.isDisabled = isDisabled;
        return m;
    }

    static ModernMenuItem Separator() {
        ModernMenuItem m;
        m.isSeparator = true;
        return m;
    }
};

class ModernMenu {
public:
    // 弹出具有无边框、圆角、现代微交互的右键菜单，返回选中的指令 ID (0 表示取消)
    static int Show(HWND hParent, int screenX, int screenY, const std::vector<ModernMenuItem>& items);
};

} // namespace yanlv
