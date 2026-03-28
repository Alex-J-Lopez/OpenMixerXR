#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>
#include <windows.h>

// Per-box D3D11 chroma texture.
// Each PassthroughBox owns one ChromaRenderer (via OverlayManager::Entry).
// The D3D11 device and context are shared across all renderers (owned by D3D11Backend).
//
// Texture is only re-cleared when the color changes — not every frame (SRD §6.1).
// Flush() is called after every clear so the SteamVR compositor sees the update (Phase 1 finding #4).
class ChromaRenderer {
public:
    ChromaRenderer()  = default;
    ~ChromaRenderer() { shutdown(); }

    ChromaRenderer(const ChromaRenderer&)            = delete;
    ChromaRenderer& operator=(const ChromaRenderer&) = delete;

    ChromaRenderer(ChromaRenderer&& other) noexcept;
    ChromaRenderer& operator=(ChromaRenderer&& other) noexcept;

    // device and context must outlive this object (owned by D3D11Backend).
    bool init(ID3D11Device* device, ID3D11DeviceContext* context,
              uint32_t width, uint32_t height);
    void shutdown();

    // Mark the desired chroma color. Does not clear the texture immediately.
    void setColor(float r, float g, float b);

    // Clears the render target to the current color only if setColor() introduced
    // a change since the last clear. Flushes GPU commands afterward.
    void clearIfDirty();

    // Force a re-clear on the next clearIfDirty() call (used after VR reconnect).
    void markDirty() { m_dirty = true; }

    // DXGI shared HANDLE — pass to OpenVR as TextureType_DXGISharedHandle.
    // Requires D3D11_RESOURCE_MISC_SHARED (set during init).
    HANDLE getSharedHandle() const;

    bool isReady() const { return m_ready; }

private:
    ID3D11Device*        m_device  = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>        m_texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;

    float m_r = 0.0f, m_g = 1.0f, m_b = 0.502f;
    bool  m_dirty = true;   // true on construction → initial clear on first clearIfDirty()
    bool  m_ready = false;
};
