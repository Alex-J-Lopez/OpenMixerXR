#pragma once
#include <d3d11.h>
#include <glm/glm.hpp>
#include <openvr.h>
#include <array>
#include <string>
#include <vector>
#include <cstdint>

#include "PassthroughBox.h"
#include "ChromaRenderer.h"

// Manages the full lifecycle of N passthrough box overlays.
//
// Ownership model:
//   - D3D11 device + context  →  owned by D3D11Backend, passed in via init().
//   - Per-face ChromaRenderer →  owned here (up to 6 per box).
//   - VROverlayHandle_t       →  owned here; all destroyed before VR_Shutdown.
//
// 3-D cuboid support (Phase 4.5):
//   When scaleDepth == 0 a box is a flat quad — only faces[0] (Front) is active.
//   This is identical to Phase 3.5 behaviour.
//   When scaleDepth >= MIN_DEPTH all six faces are active:
//     [0] Front   (at +depth/2 along box local Z, normal +Z)
//     [1] Back    (at -depth/2 along box local Z, normal -Z, 180° Y)
//     [2] Left    (at -width/2 along box local X, normal -X, -90° Y)
//     [3] Right   (at +width/2 along box local X, normal +X, +90° Y)
//     [4] Top     (at +height/2 along box local Y, normal +Y, -90° X)
//     [5] Bottom  (at -height/2 along box local Y, normal -Y, +90° X)
//   Faces 1-5 are created/destroyed in frame() when depth crosses MIN_DEPTH.
//   The SteamVR overlay limit (~64) is enforced by MAX_BOXES.
//
// Threading: single main thread only (Phase 2 decision; see main.cpp).
class OverlayManager {
public:
    OverlayManager()  = default;
    ~OverlayManager() { shutdown(); }

    OverlayManager(const OverlayManager&)            = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;

    bool init(ID3D11Device* device, ID3D11DeviceContext* context);

    // Destroy all overlay handles and chroma renderers, keeping the manager initialised.
    // Call when replacing all boxes (e.g. layout load).
    void clearBoxes();

    // Full teardown — marks the manager uninitialised.
    void shutdown();

    // Creates overlay handles + ChromaRenderers for a new box.
    // Returns false if the MAX_BOXES limit is reached or VR overlay creation fails.
    bool addBox(const PassthroughBox& box);

    // Destroys the given box's handles and renderers then removes it.
    void removeBox(const std::string& id);

    // Per-frame: update transforms, opacity, textures for all visible boxes.
    // Creates or destroys depth faces when scaleDepth crosses MIN_DEPTH.
    // hmdPos is the HMD world position (standing universe) for opacity.
    void frame(const glm::vec3& hmdPos);

    void handleEvent(const vr::VREvent_t& event);

    // Close all VR overlay handles (call before VR_Shutdown).
    // ChromaRenderers and box data are preserved for reopenOverlays().
    void closeOverlays();

    // Recreate all VR overlay handles after VR_Init.
    bool reopenOverlays();

    std::size_t boxCount() const { return m_entries.size(); }

    PassthroughBox*       boxAt(std::size_t i);
    const PassthroughBox* boxAt(std::size_t i) const;

    void reserveBoxes(std::size_t n) { m_entries.reserve(n); }

    void setHideBoxesWhenDashboard(bool hide) { m_hideBoxesWhenDashboard = hide; }
    bool getHideBoxesWhenDashboard() const     { return m_hideBoxesWhenDashboard; }

    // ── Static geometry helpers (also used by cuboid_transform_test) ─────────

    // Returns 6 column-major world-space matrices (one per face).
    // Face local +Z is the face's outward normal (the "visible" side in OpenVR).
    // Position is the face centre in world space.
    static std::array<glm::mat4, 6> computeFaceWorldMatrices(const PassthroughBox& box);

    // Physical overlay width (meters) for SetOverlayWidthInMeters.
    static float facePhysWidth(const PassthroughBox& box, int faceIdx);

    // Physical height (meters) for the texture aspect ratio calculation.
    static float facePhysHeight(const PassthroughBox& box, int faceIdx);

    // Texture dimension helper (unchanged from Phase 3.5).
    static std::pair<uint32_t, uint32_t> computeTexDims(float physW, float physH);

    // ── Constants ─────────────────────────────────────────────────────────────
    // Depth below this threshold: flat mode (front face only).
    static constexpr float       MIN_DEPTH = 0.01f;
    // Hard limit enforced in addBox() to stay within SteamVR's overlay budget:
    //   2 dashboard handles + MAX_BOXES * 6 = 62 ≤ 64.
    static constexpr std::size_t MAX_BOXES = 10;

private:
    // One OpenVR overlay + one ChromaRenderer.
    // Used for each of the 6 faces of a PassthroughBox.
    struct FaceSlot {
        vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
        ChromaRenderer        chroma;
        uint32_t              texW   = 512;
        uint32_t              texH   = 512;
    };

    struct Entry {
        PassthroughBox box;
        FaceSlot       faces[6];
        // faces[0] = Front (always active).
        // faces[1..5] = Back/Left/Right/Top/Bottom (active when scaleDepth >= MIN_DEPTH).
    };

    std::vector<Entry> m_entries;

    ID3D11Device*        m_device              = nullptr;
    ID3D11DeviceContext* m_context             = nullptr;
    bool                 m_initialized         = false;
    bool                 m_dashboardOpen        = false;
    bool                 m_hideBoxesWhenDashboard = false;

    static constexpr uint32_t MAX_TEX_DIM = 512;
    static constexpr uint32_t MIN_TEX_DIM = 16;

    // Create the VR overlay handle for a single face slot.
    bool createFaceHandle(Entry& e, int faceIdx);

    // Initialise a face slot: create ChromaRenderer + VR handle + initial texture submit.
    bool initFace(Entry& e, int faceIdx);

    // Destroy a face slot's VR handle and ChromaRenderer.
    void destroyFace(FaceSlot& f);

    // Destroy all face slots of an Entry.
    void destroyEntry(Entry& e);
};
