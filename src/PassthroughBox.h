#pragma once
#include <string>
#include <cstdint>

// Per-box persistent state.
// Persistent fields (id, name, pos*, rot*, scale*, chroma*, opacity*, visible, scaleDepth)
// are serialized to JSON by LayoutStore.
// Runtime fields (overlay handles, textures) are NOT persisted — they live in OverlayManager::Entry.
struct PassthroughBox {

    // ── Identity ──────────────────────────────────────────────────────────────
    std::string id;    // unique key suffix used for VROverlay key strings
    std::string name;  // human-readable label shown in the dashboard (§7.1)

    // ── World-space transform (standing universe, meters / degrees) ───────────
    float posX = 0.0f, posY = 1.0f, posZ = -1.0f;
    float rotPitch = 0.0f, rotYaw = 0.0f, rotRoll = 0.0f;  // YXZ Euler, degrees
    float scaleWidth  = 0.5f;   // physical width  in meters
    float scaleHeight = 0.3f;   // physical height in meters
    float scaleDepth  = 0.0f;   // physical depth  in meters  (Phase 4.5)
                                 // 0 = flat 2-D quad (front face only, backward-compatible)
                                 // > 0 = full 6-face cuboid; depth faces created live in OverlayManager

    // ── Appearance ────────────────────────────────────────────────────────────
    float chromaR = 0.000f;   // solid fill colour (0–1); matches Virtual Desktop default
    float chromaG = 1.000f;
    float chromaB = 0.502f;

    // ── Opacity fade (distance-based, SRD §8.3) ───────────────────────────────
    float minOpacity     = 0.0f;   // opacity when dist >= fadeFarMeters
    float maxOpacity     = 1.0f;   // opacity when dist <= fadeNearMeters
    float fadeNearMeters = 0.3f;
    float fadeFarMeters  = 1.2f;

    bool visible = true;
};
