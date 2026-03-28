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
    // Texture dimensions are computed per-box from scaleWidth:scaleHeight (Phase 3.5).
    bool init(ID3D11Device* device, ID3D11DeviceContext* context);

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

    // Direct mutable access to box data by index for DashboardUI.
    // Pointer is valid while no addBox/removeBox is called.
    // Call reserveBoxes() at startup to prevent reallocation invalidating pointers.
    PassthroughBox*       boxAt(std::size_t i);
    const PassthroughBox* boxAt(std::size_t i) const;

    // Pre-allocate internal storage so addBox never reallocates (Phase 2 finding #9).
    void reserveBoxes(std::size_t n) { m_entries.reserve(n); }

    // When false (default): world boxes remain visible while the dashboard is open,
    // so the user can position them with the dashboard UI in view.
    // When true: boxes are hidden whenever the dashboard is active.
    void setHideBoxesWhenDashboard(bool hide) { m_hideBoxesWhenDashboard = hide; }
    bool getHideBoxesWhenDashboard() const     { return m_hideBoxesWhenDashboard; }

    // Compute D3D11 texture dimensions for a box with the given physical size.
    // The longest dimension is clamped to MAX_TEX_DIM (512 px); the shorter
    // dimension preserves the scaleWidth:scaleHeight aspect ratio.
    // Minimum dimension is 16 px to avoid D3D11 errors on extreme ratios.
    static std::pair<uint32_t, uint32_t> computeTexDims(float scaleWidth, float scaleHeight);

private:
    struct Entry {
        PassthroughBox        box;
        ChromaRenderer        chroma;
        vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
        // Tracks the texture dimensions currently allocated in chroma.
        // Compared against computeTexDims() each frame to detect aspect changes.
        uint32_t              texW = 512;
        uint32_t              texH = 512;
    };

    std::vector<Entry> m_entries;

    ID3D11Device*        m_device  = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    bool     m_initialized          = false;
    bool     m_dashboardOpen        = false;
    bool     m_hideBoxesWhenDashboard = false;

    static constexpr uint32_t MAX_TEX_DIM = 512;
    static constexpr uint32_t MIN_TEX_DIM = 16;

    void destroyEntry(Entry& e);
    bool createOverlayHandle(Entry& e);
};
