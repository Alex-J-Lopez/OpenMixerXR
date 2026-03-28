#include "ChromaRenderer.h"
#include "Logger.h"

bool ChromaRenderer::init(ID3D11Device* device, ID3D11DeviceContext* context,
                           uint32_t width, uint32_t height) {
    m_device  = device;
    m_context = context;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width              = width;
    desc.Height             = height;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count   = 1;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags          = D3D11_RESOURCE_MISC_SHARED; // mandatory: compositor is a separate process

    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, m_texture.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("ChromaRenderer: CreateTexture2D failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    hr = m_device->CreateRenderTargetView(m_texture.Get(), nullptr, m_rtv.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("ChromaRenderer: CreateRenderTargetView failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    m_ready = true;
    m_dirty = true;
    return true;
}

void ChromaRenderer::shutdown() {
    m_rtv.Reset();
    m_texture.Reset();
    m_device  = nullptr;
    m_context = nullptr;
    m_ready   = false;
}

ChromaRenderer::ChromaRenderer(ChromaRenderer&& other) noexcept
    : m_device (other.m_device)
    , m_context(other.m_context)
    , m_texture(std::move(other.m_texture))
    , m_rtv    (std::move(other.m_rtv))
    , m_r(other.m_r), m_g(other.m_g), m_b(other.m_b)
    , m_dirty(other.m_dirty)
    , m_ready(other.m_ready)
{
    other.m_device  = nullptr;
    other.m_context = nullptr;
    other.m_ready   = false;
}

ChromaRenderer& ChromaRenderer::operator=(ChromaRenderer&& other) noexcept {
    if (this != &other) {
        shutdown();
        m_device  = other.m_device;
        m_context = other.m_context;
        m_texture = std::move(other.m_texture);
        m_rtv     = std::move(other.m_rtv);
        m_r = other.m_r; m_g = other.m_g; m_b = other.m_b;
        m_dirty = other.m_dirty;
        m_ready = other.m_ready;
        other.m_device  = nullptr;
        other.m_context = nullptr;
        other.m_ready   = false;
    }
    return *this;
}

void ChromaRenderer::setColor(float r, float g, float b) {
    if (r != m_r || g != m_g || b != m_b) {
        m_r = r; m_g = g; m_b = b;
        m_dirty = true;
    }
}

void ChromaRenderer::clearIfDirty() {
    if (!m_ready || !m_dirty) return;
    const float color[4] = { m_r, m_g, m_b, 1.0f };
    m_context->ClearRenderTargetView(m_rtv.Get(), color);
    m_context->Flush();   // ensure compositor sees the update before reading the texture
    m_dirty = false;
}

HANDLE ChromaRenderer::getSharedHandle() const {
    if (!m_texture) return nullptr;
    Microsoft::WRL::ComPtr<IDXGIResource> dxgiRes;
    if (FAILED(m_texture.As(&dxgiRes))) return nullptr;
    HANDLE h = nullptr;
    dxgiRes->GetSharedHandle(&h);
    return h;
}
