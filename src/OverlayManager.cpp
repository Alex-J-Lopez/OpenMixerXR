#include "OverlayManager.h"
#include "MathHelpers.h"
#include "Config.h"
#include "Logger.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <utility>

// ── Static geometry helpers ───────────────────────────────────────────────────

// Face physical width in meters (passed to SetOverlayWidthInMeters).
// Left/Right faces use depth as their width; all others use box width.
float OverlayManager::facePhysWidth(const PassthroughBox& box, int faceIdx) {
    return (faceIdx == 2 || faceIdx == 3) ? box.scaleDepth : box.scaleWidth;
}

// Face physical height in meters (used for texture aspect ratio).
// Top/Bottom faces use depth as their height; all others use box height.
float OverlayManager::facePhysHeight(const PassthroughBox& box, int faceIdx) {
    return (faceIdx == 4 || faceIdx == 5) ? box.scaleDepth : box.scaleHeight;
}

std::pair<uint32_t, uint32_t>
OverlayManager::computeTexDims(float physW, float physH) {
    if (physW <= 0.f || physH <= 0.f)
        return { MAX_TEX_DIM, MAX_TEX_DIM };

    uint32_t tw, th;
    if (physW >= physH) {
        tw = MAX_TEX_DIM;
        th = static_cast<uint32_t>(MAX_TEX_DIM * physH / physW);
    } else {
        tw = static_cast<uint32_t>(MAX_TEX_DIM * physW / physH);
        th = MAX_TEX_DIM;
    }
    tw = std::max(tw, MIN_TEX_DIM);
    th = std::max(th, MIN_TEX_DIM);
    return { tw, th };
}

// Compute world-space face transforms for a PassthroughBox.
//
// In OpenVR, the overlay's "visible" side faces in the local +Z direction of its
// transform.  Each face's rotation is chosen so its local +Z points outward.
//
// Face index legend:
//   0 = Front   (+Z/2 offset, no extra rotation — inherits master orientation)
//   1 = Back    (-Z/2 offset, 180° Y)
//   2 = Left    (-X/2 offset, -90° Y)
//   3 = Right   (+X/2 offset, +90° Y)
//   4 = Top     (+Y/2 offset, -90° X)
//   5 = Bottom  (-Y/2 offset, +90° X)
std::array<glm::mat4, 6>
OverlayManager::computeFaceWorldMatrices(const PassthroughBox& box) {
    const glm::vec3 center(box.posX, box.posY, box.posZ);

    const glm::quat yaw   = glm::angleAxis(glm::radians(box.rotYaw),   glm::vec3(0.f, 1.f, 0.f));
    const glm::quat pitch = glm::angleAxis(glm::radians(box.rotPitch), glm::vec3(1.f, 0.f, 0.f));
    const glm::quat roll  = glm::angleAxis(glm::radians(box.rotRoll),  glm::vec3(0.f, 0.f, 1.f));
    const glm::quat mRot  = yaw * pitch * roll;

    const float W = box.scaleWidth;
    const float H = box.scaleHeight;
    const float D = box.scaleDepth;

    // Local-space offsets (applied after master rotation to get world position).
    const glm::vec3 localOff[6] = {
        { 0.f,    0.f,    +D * 0.5f },  // [0] Front
        { 0.f,    0.f,    -D * 0.5f },  // [1] Back
        { -W * 0.5f, 0.f,  0.f      },  // [2] Left
        { +W * 0.5f, 0.f,  0.f      },  // [3] Right
        { 0.f,   +H * 0.5f, 0.f     },  // [4] Top
        { 0.f,   -H * 0.5f, 0.f     },  // [5] Bottom
    };

    // Local face rotations that orient each face's +Z outward from the box.
    const glm::quat I = glm::quat(1.f, 0.f, 0.f, 0.f);
    const glm::quat localRot[6] = {
        I,                                                           // [0] Front:   unchanged
        glm::angleAxis(glm::radians(180.f), glm::vec3(0,1,0)),      // [1] Back:    180° Y
        glm::angleAxis(glm::radians(-90.f), glm::vec3(0,1,0)),      // [2] Left:    -90° Y
        glm::angleAxis(glm::radians(+90.f), glm::vec3(0,1,0)),      // [3] Right:   +90° Y
        glm::angleAxis(glm::radians(-90.f), glm::vec3(1,0,0)),      // [4] Top:     -90° X
        glm::angleAxis(glm::radians(+90.f), glm::vec3(1,0,0)),      // [5] Bottom:  +90° X
    };

    std::array<glm::mat4, 6> matrices;
    for (int i = 0; i < 6; ++i) {
        const glm::vec3 facePos = center + mRot * localOff[i];
        const glm::quat faceRot = glm::normalize(mRot * localRot[i]);
        matrices[i]    = glm::mat4_cast(faceRot);
        matrices[i][3] = glm::vec4(facePos, 1.f);   // set translation column
    }
    return matrices;
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool OverlayManager::createFaceHandle(Entry& e, int faceIdx) {
    FaceSlot& f = e.faces[faceIdx];
    const std::string key = std::string(Config::OVERLAY_KEY_PREFIX)
                            + e.box.id + ":f" + std::to_string(faceIdx);
    const std::string nm  = e.box.name.empty() ? e.box.id : e.box.name;

    vr::EVROverlayError err =
        vr::VROverlay()->CreateOverlay(key.c_str(), nm.c_str(), &f.handle);

    if (err != vr::VROverlayError_None) {
        LOG_ERROR("OverlayManager: CreateOverlay failed for '{}' face {}: {}",
            e.box.id, faceIdx, vr::VROverlay()->GetOverlayErrorNameFromEnum(err));
        f.handle = vr::k_ulOverlayHandleInvalid;
        return false;
    }
    return true;
}

bool OverlayManager::initFace(Entry& e, int faceIdx) {
    FaceSlot& f = e.faces[faceIdx];

    const float pw = facePhysWidth(e.box, faceIdx);
    const float ph = facePhysHeight(e.box, faceIdx);
    auto [tw, th]  = computeTexDims(pw, ph);
    f.texW = tw;
    f.texH = th;

    if (!f.chroma.init(m_device, m_context, tw, th)) {
        LOG_ERROR("OverlayManager: ChromaRenderer init failed for '{}' face {}", e.box.id, faceIdx);
        return false;
    }
    f.chroma.setColor(e.box.chromaR, e.box.chromaG, e.box.chromaB);
    f.chroma.clearIfDirty();

    if (!createFaceHandle(e, faceIdx)) {
        f.chroma.shutdown();
        return false;
    }

    // Submit initial texture so the overlay is not black on first show.
    vr::Texture_t vrTex;
    vrTex.handle      = f.chroma.getSharedHandle();
    vrTex.eType       = vr::TextureType_DXGISharedHandle;
    vrTex.eColorSpace = vr::ColorSpace_Auto;
    vr::VROverlay()->SetOverlayTexture(f.handle, &vrTex);

    LOG_DEBUG("OverlayManager: '{}' face {} initialised ({}×{} px, {:.2f}×{:.2f} m)",
        e.box.id, faceIdx, tw, th, pw, ph);
    return true;
}

void OverlayManager::destroyFace(FaceSlot& f) {
    if (f.handle != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->HideOverlay(f.handle);
        vr::VROverlay()->DestroyOverlay(f.handle);
        f.handle = vr::k_ulOverlayHandleInvalid;
    }
    f.chroma.shutdown();
}

void OverlayManager::destroyEntry(Entry& e) {
    for (auto& f : e.faces)
        destroyFace(f);
}

// ── Public interface ──────────────────────────────────────────────────────────

bool OverlayManager::init(ID3D11Device* device, ID3D11DeviceContext* context) {
    m_device      = device;
    m_context     = context;
    m_initialized = true;
    LOG_INFO("OverlayManager initialised (6-face cuboid support, max {} boxes)", MAX_BOXES);
    return true;
}

void OverlayManager::clearBoxes() {
    for (auto& e : m_entries)
        destroyEntry(e);
    m_entries.clear();
    // m_initialized stays true so addBox() can be called immediately.
}

void OverlayManager::shutdown() {
    clearBoxes();
    m_initialized = false;
}

bool OverlayManager::addBox(const PassthroughBox& box) {
    if (!m_initialized) return false;

    if (m_entries.size() >= MAX_BOXES) {
        LOG_ERROR("OverlayManager: MAX_BOXES ({}) reached — cannot add '{}'", MAX_BOXES, box.id);
        return false;
    }

    m_entries.emplace_back();
    Entry& e = m_entries.back();
    e.box = box;

    // Face[0] (Front) is always created.
    if (!initFace(e, 0)) {
        m_entries.pop_back();
        return false;
    }

    if (box.visible && !m_dashboardOpen)
        vr::VROverlay()->ShowOverlay(e.faces[0].handle);

    // Depth faces created immediately when the box already has depth.
    if (box.scaleDepth >= MIN_DEPTH) {
        for (int fi = 1; fi < 6; ++fi) {
            if (initFace(e, fi) && box.visible && !m_dashboardOpen)
                vr::VROverlay()->ShowOverlay(e.faces[fi].handle);
        }
    }

    LOG_INFO("OverlayManager: box '{}' ('{}') added (depth={:.2f} m, {} faces)",
        box.id, box.name, box.scaleDepth, box.scaleDepth >= MIN_DEPTH ? 6 : 1);
    return true;
}

void OverlayManager::removeBox(const std::string& id) {
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&id](const Entry& e) { return e.box.id == id; });

    if (it == m_entries.end()) {
        LOG_WARN("OverlayManager: removeBox '{}' not found", id);
        return;
    }
    destroyEntry(*it);
    m_entries.erase(it);
    LOG_INFO("OverlayManager: box '{}' removed", id);
}

bool OverlayManager::duplicateBox(const std::string& sourceId, const std::string& newId) {
    if (m_entries.size() >= MAX_BOXES) {
        LOG_WARN("OverlayManager: MAX_BOXES ({}) reached — cannot duplicate '{}'", MAX_BOXES, sourceId);
        return false;
    }
    for (const auto& e : m_entries) {
        if (e.box.id == sourceId) {
            PassthroughBox clone  = e.box;
            clone.id              = newId;
            clone.name            = clone.name + " Copy";
            clone.posX           += 0.1f;    // small offset so the clone doesn't overlap exactly
            return addBox(clone);
        }
    }
    LOG_WARN("OverlayManager: duplicateBox — source '{}' not found", sourceId);
    return false;
}

void OverlayManager::frame(const glm::vec3& hmdPos) {
    if (!m_initialized) return;

    for (auto& e : m_entries) {

        if (!e.box.visible || (m_dashboardOpen && m_hideBoxesWhenDashboard)) {
            for (auto& f : e.faces)
                if (f.handle != vr::k_ulOverlayHandleInvalid)
                    vr::VROverlay()->HideOverlay(f.handle);
            continue;
        }

        // ── Depth face lifecycle ───────────────────────────────────────────────
        // Allocation only occurs when scaleDepth crosses MIN_DEPTH — not every frame (§6.1).
        const bool wantDepth = e.box.scaleDepth >= MIN_DEPTH;
        const bool hasDepth  = e.faces[1].handle != vr::k_ulOverlayHandleInvalid;

        if (wantDepth && !hasDepth) {
            // Flat → 3-D transition: create depth faces.
            for (int fi = 1; fi < 6; ++fi)
                initFace(e, fi);
        } else if (!wantDepth && hasDepth) {
            // 3-D → flat transition: destroy depth faces.
            for (int fi = 1; fi < 6; ++fi)
                destroyFace(e.faces[fi]);
        }

        // ── Face world transforms ──────────────────────────────────────────────
        const auto faceMatrices = computeFaceWorldMatrices(e.box);

        // ── Distance-based opacity (shared by all faces of this box) ──────────
        const glm::vec3 boxPos(e.box.posX, e.box.posY, e.box.posZ);
        const float dist  = glm::distance(hmdPos, boxPos);
        const float alpha = MathHelpers::computeOpacity(e.box, dist);

        // ── Per-face update ────────────────────────────────────────────────────
        const int activeFaces = wantDepth ? 6 : 1;
        for (int fi = 0; fi < activeFaces; ++fi) {
            FaceSlot& f = e.faces[fi];
            if (f.handle == vr::k_ulOverlayHandleInvalid) continue;

            vr::VROverlay()->ShowOverlay(f.handle);

            // Texture resize when face physical dimensions changed.
            const float pw = facePhysWidth(e.box, fi);
            const float ph = facePhysHeight(e.box, fi);
            auto [dW, dH]  = computeTexDims(pw, ph);
            if (dW != f.texW || dH != f.texH) {
                f.chroma.shutdown();
                if (f.chroma.init(m_device, m_context, dW, dH)) {
                    f.texW = dW;
                    f.texH = dH;
                    f.chroma.setColor(e.box.chromaR, e.box.chromaG, e.box.chromaB);
                    f.chroma.markDirty();
                } else {
                    LOG_ERROR("OverlayManager: face {} texture resize failed for '{}' ({}×{})",
                        fi, e.box.id, dW, dH);
                    // Stale dimensions kept so we don't retry every frame.
                }
            }

            // World-space transform.
            const vr::HmdMatrix34_t t = MathHelpers::glmToSteamVR(faceMatrices[fi]);
            vr::VROverlay()->SetOverlayTransformAbsolute(
                f.handle, vr::TrackingUniverseStanding, &t);
            vr::VROverlay()->SetOverlayWidthInMeters(f.handle, pw);
            vr::VROverlay()->SetOverlayAlpha(f.handle, alpha);

            // Chroma texture (re-clears only when colour changed, §6.1).
            f.chroma.setColor(e.box.chromaR, e.box.chromaG, e.box.chromaB);
            f.chroma.clearIfDirty();

            vr::Texture_t vrTex;
            vrTex.handle      = f.chroma.getSharedHandle();
            vrTex.eType       = vr::TextureType_DXGISharedHandle;
            vrTex.eColorSpace = vr::ColorSpace_Auto;
            vr::VROverlay()->SetOverlayTexture(f.handle, &vrTex);
        }
    }
}

void OverlayManager::handleEvent(const vr::VREvent_t& event) {
    switch (event.eventType) {
    case vr::VREvent_DashboardActivated:
        m_dashboardOpen = true;
        break;
    case vr::VREvent_DashboardDeactivated:
        m_dashboardOpen = false;
        break;
    default:
        break;
    }
}

void OverlayManager::closeOverlays() {
    for (auto& e : m_entries) {
        for (auto& f : e.faces) {
            if (f.handle != vr::k_ulOverlayHandleInvalid) {
                vr::VROverlay()->HideOverlay(f.handle);
                vr::VROverlay()->DestroyOverlay(f.handle);
                f.handle = vr::k_ulOverlayHandleInvalid;
            }
        }
    }
    LOG_INFO("OverlayManager: {} box overlay handles closed for reconnect", m_entries.size());
}

bool OverlayManager::reopenOverlays() {
    for (auto& e : m_entries) {
        const int activeFaces = (e.box.scaleDepth >= MIN_DEPTH) ? 6 : 1;
        for (int fi = 0; fi < activeFaces; ++fi) {
            FaceSlot& f = e.faces[fi];
            if (!f.chroma.isReady()) continue;   // not initialised (depth face that was never created)

            if (!createFaceHandle(e, fi)) return false;

            f.chroma.markDirty();
            f.chroma.clearIfDirty();

            vr::Texture_t vrTex;
            vrTex.handle      = f.chroma.getSharedHandle();
            vrTex.eType       = vr::TextureType_DXGISharedHandle;
            vrTex.eColorSpace = vr::ColorSpace_Auto;
            vr::VROverlay()->SetOverlayTexture(f.handle, &vrTex);

            if (e.box.visible && !m_dashboardOpen)
                vr::VROverlay()->ShowOverlay(f.handle);
        }
    }
    LOG_INFO("OverlayManager: {} box overlays reopened", m_entries.size());
    return true;
}

PassthroughBox* OverlayManager::boxAt(std::size_t i) {
    return i < m_entries.size() ? &m_entries[i].box : nullptr;
}

const PassthroughBox* OverlayManager::boxAt(std::size_t i) const {
    return i < m_entries.size() ? &m_entries[i].box : nullptr;
}
