#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>

namespace yanlv {

class TrayIcon {
public:
    static TrayIcon& Instance();

    bool Initialize(HWND hCallbackWnd, UINT uCallbackMsg);
    void Remove();

    void UpdateTooltip(const std::wstring& tooltip);

private:
    TrayIcon();
    ~TrayIcon();
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    NOTIFYICONDATAW m_nid{};
    bool m_installed = false;
};

} // namespace yanlv
