#pragma once
#include <d3d11.h>
#include <glm/glm.hpp>
#include <openvr.h>
#include <string>
#include <vector>
#include <cstdint>

#include "PassthroughBox.h"
#include "ChromaRenderer.h"

// Manages the full lifecycle of N passthrough box overlays.
//
// Ownership model:
//   - D3D11 device + context  →  owned by D3D11Backend, passed in via init().
//   - Per-box ChromaRenderer  →  owned here (one per Entry).
//   - VROverlayHandle_t       →  owned here; destroyed before VR_Shutdown.
//
// Threading: single main thread only (Phase 2 decision; see main.cpp).
class OverlayManager {
public:
    OverlayManager()  = default;
    ~OverlayManager() { shutdown(); }

    OverlayManager(const OverlayManager&)            = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;

    // device and context must outlive this manager (owned by D3D11Backend).
    bool init(ID3D11Device* device, ID3D11DeviceContext* context,
              uint32_t texW, uint32_t texH);

    // Destroy all overlay handles and chroma renderers.
    void shutdown();

    // Creates an OpenVR overlay + ChromaRenderer for the given box.
    // Returns false on overlay creation failure.
    bool addBox(const PassthroughBox& box);

    // Destroys a box's overlay handle and chroma renderer, then removes it.
    void removeBox(const std::string& id);

    // Per-frame update.
    //   - Updates world transform, distance-based opacity, and chroma texture for every
    //     visible box. No heap allocation occurs inside this loop (SRD §6.1).
    //   - hmdPos is the HMD world position (standing universe) for opacity computation.
    void frame(const glm::vec3& hmdPos);

    // Feed all VR events here.
    //   Handles: VREvent_DashboardActivated / VREvent_DashboardDeactivated.
    void handleEvent(const vr::VREvent_t& event);

    // Close VR overlay handles without destroying ChromaRenderers or box data.
    // Call before VR_Shutdown; call reopenOverlays() after VR_Init to restore.
    void closeOverlays();

    // Recreate VR overlay handles for all existing entries after VR_Init.
    // ChromaRenderers are reused; textures are marked dirty so they re-upload.
    bool reopenOverlays();

    std::size_t boxCount() const { return m_entries.size(); }

private:
    struct Entry {
        PassthroughBox        box;
        ChromaRenderer        chroma;
        vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
    };

    std::vector<Entry> m_entries;

    ID3D11Device*        m_device  = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    uint32_t m_texW    = 512;
    uint32_t m_texH    = 512;
    bool     m_initialized   = false;
    bool     m_dashboardOpen = false;

    void destroyEntry(Entry& e);
    bool createOverlayHandle(Entry& e);
};
