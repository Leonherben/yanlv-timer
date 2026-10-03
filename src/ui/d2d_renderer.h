#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>

namespace yanlv {

class D2DRenderer {
public:
    static D2DRenderer& Instance();

    bool Initialize();
    void Shutdown();

    ID2D1Factory* GetD2DFactory() const { return m_d2dFactory; }
    IDWriteFactory* GetDWriteFactory() const { return m_dwriteFactory; }
    IWICImagingFactory* GetWICFactory() const { return m_wicFactory; }

    // 创建文字格式化对象
    IDWriteTextFormat* CreateTextFormat(const std::wstring& fontFamily, 
                                        float fontSize, 
                                        DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL,
                                        DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_CENTER);

private:
    D2DRenderer();
    ~D2DRenderer();
    D2DRenderer(const D2DRenderer&) = delete;
    D2DRenderer& operator=(const D2DRenderer&) = delete;

    ID2D1Factory* m_d2dFactory = nullptr;
    IDWriteFactory* m_dwriteFactory = nullptr;
    IWICImagingFactory* m_wicFactory = nullptr;
};

} // namespace yanlv
