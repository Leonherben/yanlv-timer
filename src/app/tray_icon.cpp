#include "src/app/tray_icon.h"

namespace yanlv {

TrayIcon& TrayIcon::Instance() {
    static TrayIcon instance;
    return instance;
}

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    Remove();
}

bool TrayIcon::Initialize(HWND hCallbackWnd, UINT uCallbackMsg) {
    if (m_installed) return true;

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = hCallbackWnd;
    m_nid.uID = 100;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = uCallbackMsg;
    m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // 系统默认时钟应用图标
    wcscpy_s(m_nid.szTip, L"言律时钟 - 待开始");

    m_installed = Shell_NotifyIconW(NIM_ADD, &m_nid);
    return m_installed;
}

void TrayIcon::Remove() {
    if (m_installed) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_installed = false;
    }
}

void TrayIcon::UpdateTooltip(const std::wstring& tooltip) {
    if (!m_installed) return;
    wcscpy_s(m_nid.szTip, tooltip.c_str());
    m_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

} // namespace yanlv
