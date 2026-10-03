#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "src/core/timer_types.h"
#include <vector>
#include <functional>

namespace yanlv {

class QuickStartPopup {
public:
    static QuickStartPopup& Instance();

    bool Create();
    void ShowNear(int anchorX, int anchorY);
    void Hide();
    bool IsVisible() const;

private:
    QuickStartPopup();
    ~QuickStartPopup();
    QuickStartPopup(const QuickStartPopup&) = delete;
    QuickStartPopup& operator=(const QuickStartPopup&) = delete;

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void PopulateCategories();
    void OnStartClicked();
    void OnAddCategoryClicked();

    HWND m_hWnd = nullptr;
    HWND m_hComboCategory = nullptr;
    HWND m_hBtnAddCategory = nullptr;
    HWND m_hEditDuration = nullptr;
    HWND m_hBtnStart = nullptr;
    HWND m_hBtn25 = nullptr;
    HWND m_hBtn45 = nullptr;
    HWND m_hBtn60 = nullptr;
    HWND m_hBtnClose = nullptr;
    int m_selectedPreset = 25;

    HFONT m_hFont = nullptr;
    HFONT m_hFontBold = nullptr;
    HBRUSH m_hBrushBg = nullptr;
    HBRUSH m_hBrushBorder = nullptr;
    std::vector<Category> m_categories;
};

} // namespace yanlv
