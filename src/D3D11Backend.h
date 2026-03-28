#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstdint>

class D3D11Backend {
public:
    D3D11Backend()  = default;
    ~D3D11Backend() { shutdown(); }

    D3D11Backend(const D3D11Backend&)            = delete;
    D3D11Backend& operator=(const D3D11Backend&) = delete;

    // adapter should be the DXGI adapter SteamVR is using (from GetOutputDevice).
    // Pass nullptr to fall back to the system default (single-GPU machines).
    bool init(uint32_t width, uint32_t height, IDXGIAdapter* adapter = nullptr);
    void shutdown();

    // Clears the texture to (r,g,b,1) only when the colour changed, then flushes
    // the GPU command buffer so the compositor sees the updated pixels immediately.
    bool clearChromaIfNeeded(float r, float g, float b);

    ID3D11Texture2D* getTexture() const { return m_texture.Get(); }

    // Returns the DXGI shared HANDLE for this texture.
    // Pass this as vr::Texture_t::handle with TextureType_DXGISharedHandle —
    // the type explicitly documented as "only supported for overlay render targets".
    HANDLE getSharedHandle() const;

    bool isInitialized() const { return m_initialized; }

private:
    Microsoft::WRL::ComPtr<ID3D11Device>           m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    m_context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>        m_texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;

    float m_lastR = -1.0f, m_lastG = -1.0f, m_lastB = -1.0f;
    bool  m_initialized = false;
};
