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

    // 绘制与状态公开字段（供自定义子窗口过程访问）
    HFONT m_hFont = nullptr;         // 14px 正文
    HFONT m_hFontBold = nullptr;     // 14px 粗体
    HFONT m_hFontSub = nullptr;      // 12px 辅助说明
    HFONT m_hFontSection = nullptr;  // 16px 分组标题粗体
    HFONT m_hFontTitle = nullptr;    // 22px 页面标题粗体
    int m_previewOpacity = 85;
    int m_previewFontSize = 22;
    std::string m_previewTextColorHex = "#0F172A";
    bool m_previewShowRealTime = true;
    bool m_isDirty = false;

private:
    ManagementWindow();
    ~ManagementWindow();
    ManagementWindow(const ManagementWindow&) = delete;
    ManagementWindow& operator=(const ManagementWindow&) = delete;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ClockPreviewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
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

    void UpdatePreview();
    void SetDirty(bool dirty);
    void UpdateMediaTooltip();

    HWND m_hWnd = nullptr;

    HBRUSH m_hBrushBg = nullptr;       // #F5F6F8 浅灰背景
    HBRUSH m_hBrushCard = nullptr;     // #FFFFFF 纯白卡片
    HBRUSH m_hBrushInput = nullptr;    // #FFFFFF 输入框背景
    HBRUSH m_hBrushAccent = nullptr;   // #4164DE 皇家蓝强调色
    HBRUSH m_hBrushBorder = nullptr;   // #E2E8F0 边框
    HBRUSH m_hBrushCardSub = nullptr;  // #F1F5F9 辅助浅灰底
    HPEN m_hPenBorder = nullptr;

    // 现代顶部切换标签与窗口控制
    HWND m_hBtnTabStats = nullptr;
    HWND m_hBtnTabRecords = nullptr;
    HWND m_hBtnTabSettings = nullptr;
    HWND m_hBtnWinMin = nullptr;
    HWND m_hBtnWinClose = nullptr;
    int m_currentTabIndex = 0;

    // 页容器面板
    HWND m_hPanelStats = nullptr;
    HWND m_hPanelRecords = nullptr;
    HWND m_hPanelSettings = nullptr;

    // 底部全局操作栏控件
    HWND m_hBtnSaveSettings = nullptr;
    HWND m_hStaticDirtyStatus = nullptr;
    HWND m_hBtnCheckUpdate = nullptr;
    HWND m_hBtnOpenDataDir = nullptr;

    // 统计看板控件
    HWND m_hStaticToday = nullptr;
    HWND m_hStaticTotal = nullptr;
    HWND m_hListCatStats = nullptr;

    // 专注记录控件
    HWND m_hComboFilter = nullptr;
    HWND m_hListRecords = nullptr;
    HWND m_hBtnChangeCat = nullptr;
    HWND m_hBtnDeleteRecord = nullptr;

    // 设置页控件：1. 悬浮时钟与实时预览
    HWND m_hSegIdleMode = nullptr;       // 待机显示分段切换：[当前时间 | 计划倒计时]
    HWND m_hSliderOpacity = nullptr;     // 现代滑块
    HWND m_hStaticOpacityVal = nullptr;  // 百分比数值
    HWND m_hComboFontSize = nullptr;     // 时间大小下拉
    HWND m_hComboTextColor = nullptr;    // 文字颜色下拉
    HWND m_hClockPreviewWnd = nullptr;   // 实时外观预览控件

    // 设置页控件：2. 休息设置
    HWND m_hSegBreakMode = nullptr;      // 专注结束行为分段切换：[自动全屏休息 | 先提醒我]
    HWND m_hEditMediaPath = nullptr;     // 背景媒体文件名展示
    HWND m_hBtnBrowseMedia = nullptr;    // 更换文件按钮
    HWND m_hToggleVideoMuted = nullptr;  // 静音播放切换开关
    HWND m_hTooltipMedia = nullptr;
    std::wstring m_fullMediaPath;

    // 设置页控件：3. 专注类别
    HWND m_hListCategories = nullptr;
    HWND m_hEditNewCat = nullptr;
    HWND m_hBtnAddCat = nullptr;
    HWND m_hBtnRenameCat = nullptr;
    HWND m_hBtnDeleteCat = nullptr;

    std::vector<Category> m_cachedCategories;
    std::vector<StudyRecord> m_cachedRecords;
};

} // namespace yanlv
