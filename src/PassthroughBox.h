#pragma once
#include <string>
#include <cstdint>

// Per-box persistent state.
// Persistent fields (id, name, pos*, rot*, scale*, chroma*, opacity*, visible)
// will be serialized to JSON in Phase 4.
// Runtime fields (overlayHandle) are NOT serialized.
// The D3D11 texture is owned by ChromaRenderer (inside OverlayManager::Entry).
struct PassthroughBox {

    // ── Identity ──────────────────────────────────────────────────────────────
    std::string id;    // UUID string — used as the IVROverlay key suffix
    std::string name;  // Human-readable label shown in dashboard UI (§7.1)

    // ── World-space transform (standing universe, meters / degrees) ───────────
    float posX = 0.0f, posY = 1.0f, posZ = -1.0f;
    float rotPitch = 0.0f, rotYaw = 0.0f, rotRoll = 0.0f;  // Euler, degrees, YXZ order
    float scaleWidth  = 0.5f;   // overlay width  in meters
    float scaleHeight = 0.3f;   // overlay height in meters (aspect driven by SetOverlayWidthInMeters)

    // ── Appearance ────────────────────────────────────────────────────────────
    float chromaR = 0.000f;   // solid fill color (0–1); matches Virtual Desktop default
    float chromaG = 1.000f;
    float chromaB = 0.502f;

    // ── Opacity fade (distance-based, SRD §8.3) ───────────────────────────────
    float minOpacity     = 0.0f;   // opacity when dist >= fadeFarMeters
    float maxOpacity     = 1.0f;   // opacity when dist <= fadeNearMeters
    float fadeNearMeters = 0.3f;
    float fadeFarMeters  = 1.2f;

    bool visible = true;

    // ── Runtime only (not serialized, not persisted) ──────────────────────────
    uint64_t overlayHandle = 0;   // vr::VROverlayHandle_t — managed by OverlayManager
};
