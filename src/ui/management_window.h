#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "src/core/timer_types.h"
#include <vector>
#include <string>

namespace yanlv {

class ManagementWindow {
public:
    static ManagementWindow& Instance();

    bool Create();
    void Show();
    void Hide();
    bool IsVisible() const;

    void RefreshAll();

private:
    ManagementWindow();
    ~ManagementWindow();
    ManagementWindow(const ManagementWindow&) = delete;
    ManagementWindow& operator=(const ManagementWindow&) = delete;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void CreateTabs(HWND hWnd);
    void CreateStatsPage(HWND hWnd);
    void CreateRecordsPage(HWND hWnd);
    void CreateSettingsPage(HWND hWnd);
    void SwitchTab(int tabIndex);

    void RefreshStats();
    void RefreshRecords();
    void RefreshSettings();

    void OnChangeRecordCategory();
    void OnDeleteRecord();
    void OnAddCategory();
    void OnRenameCategory();
    void OnBrowseMedia();
    void OnSaveSettings();

    HWND m_hWnd = nullptr;
    HWND m_hTab = nullptr;
    HFONT m_hFont = nullptr;
    HFONT m_hFontBold = nullptr;

    // 页容器面板
    HWND m_hPanelStats = nullptr;
    HWND m_hPanelRecords = nullptr;
    HWND m_hPanelSettings = nullptr;

    // 统计页控件
    HWND m_hStaticToday = nullptr;
    HWND m_hStaticTotal = nullptr;
    HWND m_hListCatStats = nullptr;

    // 记录页控件
    HWND m_hComboFilter = nullptr;
    HWND m_hListRecords = nullptr;
    HWND m_hBtnChangeCat = nullptr;
    HWND m_hBtnDeleteRecord = nullptr;

    // 设置与类别页控件
    HWND m_hListCategories = nullptr;
    HWND m_hEditNewCat = nullptr;
    HWND m_hBtnAddCat = nullptr;
    HWND m_hBtnRenameCat = nullptr;
    HWND m_hRadioAutoBreak = nullptr;
    HWND m_hRadioRemindBreak = nullptr;
    HWND m_hEditMediaPath = nullptr;
    HWND m_hBtnBrowseMedia = nullptr;
    HWND m_hRadioVideoMuted = nullptr;
    HWND m_hRadioVideoAudio = nullptr;
    HWND m_hBtnSaveSettings = nullptr;

    std::vector<Category> m_cachedCategories;
    std::vector<StudyRecord> m_cachedRecords;
};

} // namespace yanlv
