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

    void CreateModernTabs(HWND hWnd);
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
    void OnDeleteCategory();
    void OnBrowseMedia();
    void OnSaveSettings();

    HWND m_hWnd = nullptr;
    HFONT m_hFont = nullptr;
    HFONT m_hFontBold = nullptr;
    HFONT m_hFontTitle = nullptr;
    HFONT m_hFontSection = nullptr;

    HBRUSH m_hBrushBg = nullptr;
    HBRUSH m_hBrushCard = nullptr;
    HBRUSH m_hBrushInput = nullptr;
    HBRUSH m_hBrushAccent = nullptr;
    HBRUSH m_hBrushBorder = nullptr;
    HBRUSH m_hBrushCardSub = nullptr;

    // 现代顶部切换标签
    HWND m_hBtnTabStats = nullptr;
    HWND m_hBtnTabRecords = nullptr;
    HWND m_hBtnTabSettings = nullptr;
    int m_currentTabIndex = 0;

    // 页容器面板
    HWND m_hPanelStats = nullptr;
    HWND m_hPanelRecords = nullptr;
    HWND m_hPanelSettings = nullptr;

    // 统计看板控件
    HWND m_hStaticToday = nullptr;
    HWND m_hStaticTotal = nullptr;
    HWND m_hListCatStats = nullptr;

    // 专注记录控件
    HWND m_hComboFilter = nullptr;
    HWND m_hListRecords = nullptr;
    HWND m_hBtnChangeCat = nullptr;
    HWND m_hBtnDeleteRecord = nullptr;

    // 设置页控件：时钟外观与行为
    HWND m_hRadioIdleRealTime = nullptr;
    HWND m_hRadioIdleDuration = nullptr;
    HWND m_hComboOpacity = nullptr;
    HWND m_hComboFontSize = nullptr;
    HWND m_hComboTextColor = nullptr;
    HWND m_hCheckAlwaysOnTop = nullptr;

    // 设置页控件：休息与视频
    HWND m_hRadioAutoBreak = nullptr;
    HWND m_hRadioRemindBreak = nullptr;
    HWND m_hEditMediaPath = nullptr;
    HWND m_hBtnBrowseMedia = nullptr;
    HWND m_hRadioVideoMuted = nullptr;
    HWND m_hRadioVideoAudio = nullptr;

    // 设置页控件：分类管理与保存
    HWND m_hListCategories = nullptr;
    HWND m_hEditNewCat = nullptr;
    HWND m_hBtnAddCat = nullptr;
    HWND m_hBtnRenameCat = nullptr;
    HWND m_hBtnDeleteCat = nullptr;
    HWND m_hBtnSaveSettings = nullptr;
    HWND m_hBtnCheckUpdate = nullptr;
    HWND m_hBtnOpenDataDir = nullptr;

    std::vector<Category> m_cachedCategories;
    std::vector<StudyRecord> m_cachedRecords;
};

} // namespace yanlv
