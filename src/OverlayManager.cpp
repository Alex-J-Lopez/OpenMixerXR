#include "OverlayManager.h"
#include "MathHelpers.h"
#include "Config.h"
#include "Logger.h"

#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <utility>

// ── Static helper ─────────────────────────────────────────────────────────────

std::pair<uint32_t, uint32_t>
OverlayManager::computeTexDims(float scaleWidth, float scaleHeight) {
    if (scaleWidth <= 0.f || scaleHeight <= 0.f)
        return { MAX_TEX_DIM, MAX_TEX_DIM };

    uint32_t tw, th;
    if (scaleWidth >= scaleHeight) {
        tw = MAX_TEX_DIM;
        th = static_cast<uint32_t>(MAX_TEX_DIM * scaleHeight / scaleWidth);
    } else {
        tw = static_cast<uint32_t>(MAX_TEX_DIM * scaleWidth / scaleHeight);
        th = MAX_TEX_DIM;
    }
    tw = std::max(tw, MIN_TEX_DIM);
    th = std::max(th, MIN_TEX_DIM);
    return { tw, th };
}

// ── Private helpers ───────────────────────────────────────────────────────────

bool OverlayManager::createOverlayHandle(Entry& e) {
    const std::string key  = std::string(Config::OVERLAY_KEY_PREFIX) + e.box.id;
    const std::string name = e.box.name.empty() ? e.box.id : e.box.name;

    vr::EVROverlayError err =
        vr::VROverlay()->CreateOverlay(key.c_str(), name.c_str(), &e.handle);

    if (err != vr::VROverlayError_None) {
        LOG_ERROR("OverlayManager: CreateOverlay failed for '{}': {}",
            e.box.id, vr::VROverlay()->GetOverlayErrorNameFromEnum(err));
        e.handle = vr::k_ulOverlayHandleInvalid;
        return false;
    }
    return true;
}

void OverlayManager::destroyEntry(Entry& e) {
    if (e.handle != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->HideOverlay(e.handle);
        vr::VROverlay()->DestroyOverlay(e.handle);
        e.handle = vr::k_ulOverlayHandleInvalid;
    }
    e.chroma.shutdown();
}

// ── Public interface ──────────────────────────────────────────────────────────

bool OverlayManager::init(ID3D11Device* device, ID3D11DeviceContext* context) {
    m_device      = device;
    m_context     = context;
    m_initialized = true;
    LOG_INFO("OverlayManager initialised (per-box aspect-ratio textures)");
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

    m_entries.emplace_back();
    Entry& e  = m_entries.back();
    e.box     = box;

    auto [tw, th] = computeTexDims(box.scaleWidth, box.scaleHeight);
    e.texW = tw;
    e.texH = th;

    if (!e.chroma.init(m_device, m_context, tw, th)) {
        LOG_ERROR("OverlayManager: ChromaRenderer init failed for '{}'", box.id);
        m_entries.pop_back();
        return false;
    }
    e.chroma.setColor(box.chromaR, box.chromaG, box.chromaB);
    e.chroma.clearIfDirty();

    if (!createOverlayHandle(e)) {
        e.chroma.shutdown();
        m_entries.pop_back();
        return false;
    }

    // Submit texture immediately to avoid first-frame black flash.
    vr::Texture_t vrTex;
    vrTex.handle      = e.chroma.getSharedHandle();
    vrTex.eType       = vr::TextureType_DXGISharedHandle;
    vrTex.eColorSpace = vr::ColorSpace_Auto;
    vr::VROverlay()->SetOverlayTexture(e.handle, &vrTex);

    if (box.visible && !m_dashboardOpen)
        vr::VROverlay()->ShowOverlay(e.handle);

    LOG_INFO("OverlayManager: box '{}' ('{}') added (handle {})",
        box.id, box.name, e.handle);
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

void OverlayManager::frame(const glm::vec3& hmdPos) {
    if (!m_initialized) return;

    for (auto& e : m_entries) {
        if (e.handle == vr::k_ulOverlayHandleInvalid) continue;

        if (!e.box.visible || (m_dashboardOpen && m_hideBoxesWhenDashboard)) {
            vr::VROverlay()->HideOverlay(e.handle);
            continue;
        }

        vr::VROverlay()->ShowOverlay(e.handle);

        // ── Rectangle support: resize texture if aspect ratio changed ─────────
        auto [dW, dH] = computeTexDims(e.box.scaleWidth, e.box.scaleHeight);
        if (dW != e.texW || dH != e.texH) {
            e.chroma.shutdown();
            if (e.chroma.init(m_device, m_context, dW, dH)) {
                e.texW = dW;
                e.texH = dH;
                e.chroma.setColor(e.box.chromaR, e.box.chromaG, e.box.chromaB);
                e.chroma.markDirty();
                LOG_DEBUG("OverlayManager: box '{}' texture resized to {}x{}",
                    e.box.id, dW, dH);
            } else {
                LOG_ERROR("OverlayManager: texture resize failed for '{}' ({}x{})",
                    e.box.id, dW, dH);
                // Keep stale texW/texH so we don't retry every frame.
            }
        }

        // World-space transform from box position + Euler rotation (YXZ order).
        const glm::vec3 pos(e.box.posX, e.box.posY, e.box.posZ);
        const glm::quat yaw   = glm::angleAxis(glm::radians(e.box.rotYaw),   glm::vec3(0.f, 1.f, 0.f));
        const glm::quat pitch = glm::angleAxis(glm::radians(e.box.rotPitch), glm::vec3(1.f, 0.f, 0.f));
        const glm::quat roll  = glm::angleAxis(glm::radians(e.box.rotRoll),  glm::vec3(0.f, 0.f, 1.f));
        const glm::quat rot   = yaw * pitch * roll;

        vr::HmdMatrix34_t transform = MathHelpers::buildTransform(pos, rot);
        vr::VROverlay()->SetOverlayTransformAbsolute(
            e.handle, vr::TrackingUniverseStanding, &transform);
        vr::VROverlay()->SetOverlayWidthInMeters(e.handle, e.box.scaleWidth);

        // Distance-based opacity (SRD §8.3).
        const float dist  = glm::distance(hmdPos, pos);
        const float alpha = MathHelpers::computeOpacity(e.box, dist);
        vr::VROverlay()->SetOverlayAlpha(e.handle, alpha);

        // Re-clear texture only when color changed (SRD §6.1).
        e.chroma.setColor(e.box.chromaR, e.box.chromaG, e.box.chromaB);
        e.chroma.clearIfDirty();

        vr::Texture_t vrTex;
        vrTex.handle      = e.chroma.getSharedHandle();
        vrTex.eType       = vr::TextureType_DXGISharedHandle;
        vrTex.eColorSpace = vr::ColorSpace_Auto;
        vr::VROverlay()->SetOverlayTexture(e.handle, &vrTex);
    }
}

void OverlayManager::handleEvent(const vr::VREvent_t& event) {
    switch (event.eventType) {
    case vr::VREvent_DashboardActivated:
        m_dashboardOpen = true;
        LOG_DEBUG("OverlayManager: dashboard opened — {} boxes hidden", m_entries.size());
        break;
    case vr::VREvent_DashboardDeactivated:
        m_dashboardOpen = false;
        LOG_DEBUG("OverlayManager: dashboard closed — visible boxes restored");
        break;
    default:
        break;
    }
}

void OverlayManager::closeOverlays() {
    for (auto& e : m_entries) {
        if (e.handle != vr::k_ulOverlayHandleInvalid) {
            vr::VROverlay()->HideOverlay(e.handle);
            vr::VROverlay()->DestroyOverlay(e.handle);
            e.handle = vr::k_ulOverlayHandleInvalid;
        }
    }
    LOG_INFO("OverlayManager: {} overlay handles closed for reconnect", m_entries.size());
}

bool OverlayManager::reopenOverlays() {    for (auto& e : m_entries) {
        if (!createOverlayHandle(e))
            return false;

        // Force texture re-upload since handle is new.
        e.chroma.markDirty();
        e.chroma.clearIfDirty();

        vr::Texture_t vrTex;
        vrTex.handle      = e.chroma.getSharedHandle();
        vrTex.eType       = vr::TextureType_DXGISharedHandle;
        vrTex.eColorSpace = vr::ColorSpace_Auto;
        vr::VROverlay()->SetOverlayTexture(e.handle, &vrTex);

        if (e.box.visible && !m_dashboardOpen)
            vr::VROverlay()->ShowOverlay(e.handle);
    }
    LOG_INFO("OverlayManager: {} overlays reopened", m_entries.size());
    return true;
}

PassthroughBox* OverlayManager::boxAt(std::size_t i) {
    return i < m_entries.size() ? &m_entries[i].box : nullptr;
}

const PassthroughBox* OverlayManager::boxAt(std::size_t i) const {
    return i < m_entries.size() ? &m_entries[i].box : nullptr;
}
