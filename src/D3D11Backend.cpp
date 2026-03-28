#include "D3D11Backend.h"
#include "Logger.h"

bool D3D11Backend::init(uint32_t width, uint32_t height, IDXGIAdapter* adapter) {
    UINT createFlags = 0;
#if defined(_DEBUG)
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        adapter,                   // null = default; pass VR adapter when known
        adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        nullptr, 0,
        D3D11_SDK_VERSION,
        m_device.GetAddressOf(),
        &featureLevel,
        m_context.GetAddressOf()
    );
    if (FAILED(hr)) {
        LOG_ERROR("D3D11CreateDevice failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }
    LOG_DEBUG("D3D11 device created (feature level 0x{:04X})", static_cast<unsigned>(featureLevel));

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width              = width;
    desc.Height             = height;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count   = 1;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags          = D3D11_RESOURCE_MISC_SHARED; // required for cross-process compositor access

    hr = m_device->CreateTexture2D(&desc, nullptr, m_texture.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("CreateTexture2D failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    hr = m_device->CreateRenderTargetView(m_texture.Get(), nullptr, m_rtv.GetAddressOf());
    if (FAILED(hr)) {
        LOG_ERROR("CreateRenderTargetView failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    m_initialized = true;
    LOG_INFO("D3D11Backend ready ({}x{} RGBA)", width, height);
    return true;
}

void D3D11Backend::shutdown() {
    m_rtv.Reset();
    m_texture.Reset();
    m_context.Reset();
    m_device.Reset();
    m_initialized = false;
}

bool D3D11Backend::clearChromaIfNeeded(float r, float g, float b) {
    if (!m_initialized) return false;
    if (r == m_lastR && g == m_lastG && b == m_lastB) return false;

    const float colour[4] = { r, g, b, 1.0f };
    m_context->ClearRenderTargetView(m_rtv.Get(), colour);
    // Flush so the compositor sees the updated pixels when it reads the texture.
    m_context->Flush();
    m_lastR = r; m_lastG = g; m_lastB = b;
    return true;
}

HANDLE D3D11Backend::getSharedHandle() const {
    if (!m_texture) return nullptr;

    Microsoft::WRL::ComPtr<IDXGIResource> dxgiResource;
    if (FAILED(m_texture.As(&dxgiResource))) {
        return nullptr;
    }

    HANDLE sharedHandle = nullptr;
    dxgiResource->GetSharedHandle(&sharedHandle);
    return sharedHandle;
}
