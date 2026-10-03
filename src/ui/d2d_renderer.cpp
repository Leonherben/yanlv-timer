#include "src/ui/d2d_renderer.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace yanlv {

D2DRenderer& D2DRenderer::Instance() {
    static D2DRenderer instance;
    return instance;
}

D2DRenderer::D2DRenderer() = default;

D2DRenderer::~D2DRenderer() {
    Shutdown();
}

bool D2DRenderer::Initialize() {
    if (m_d2dFactory) return true;

    // 创建 D2D 工厂 (单线程模型以获取最低开销)
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
    if (FAILED(hr)) return false;

    // 创建 DirectWrite 工厂
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&m_dwriteFactory)
    );
    if (FAILED(hr)) return false;

    // 创建 WIC 图像工厂
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&m_wicFactory)
    );
    if (FAILED(hr)) return false;

    return true;
}

void D2DRenderer::Shutdown() {
    if (m_wicFactory) {
        m_wicFactory->Release();
        m_wicFactory = nullptr;
    }
    if (m_dwriteFactory) {
        m_dwriteFactory->Release();
        m_dwriteFactory = nullptr;
    }
    if (m_d2dFactory) {
        m_d2dFactory->Release();
        m_d2dFactory = nullptr;
    }
}

IDWriteTextFormat* D2DRenderer::CreateTextFormat(
    const std::wstring& fontFamily, 
    float fontSize, 
    DWRITE_FONT_WEIGHT weight,
    DWRITE_TEXT_ALIGNMENT align) 
{
    if (!m_dwriteFactory) return nullptr;

    IDWriteTextFormat* format = nullptr;
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        fontFamily.c_str(),
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"zh-CN",
        &format
    );

    if (SUCCEEDED(hr) && format) {
        format->SetTextAlignment(align);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    return format;
}

} // namespace yanlv
