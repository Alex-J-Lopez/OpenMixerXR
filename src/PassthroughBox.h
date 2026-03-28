#pragma once
#include <string>
#include <cstdint>

// Forward declare D3D type to avoid pulling in d3d11.h in every TU.
struct ID3D11Texture2D;

// PassthroughBox holds all per-box state.
// Persistent fields (posX/Y/Z, rot*, scale*, chroma*, opacity*, visible, id, name)
// are serialized to JSON in Phase 4.
// Runtime fields (overlayHandle, renderTexture) are NOT serialized.
struct PassthroughBox {

    // ── Identity ──────────────────────────────────────────────────────────────
    std::string id;    // UUID string — used as the IVROverlay key suffix
    std::string name;  // Human-readable label shown in dashboard UI

    // ── World-space transform (standing universe, meters / degrees) ───────────
    float posX = 0.0f, posY = 1.0f, posZ = -1.0f;
    float rotPitch = 0.0f, rotYaw = 0.0f, rotRoll = 0.0f;  // Euler, degrees
    float scaleWidth  = 0.5f;   // overlay width  in meters (height derived from aspect)
    float scaleHeight = 0.3f;   // overlay height in meters

    // ── Appearance ────────────────────────────────────────────────────────────
    float chromaR = 0.000f;   // solid fill color (0–1); should match Virtual Desktop setting
    float chromaG = 1.000f;
    float chromaB = 0.502f;

    // ── Opacity fade (distance-based, SRD §8.3) ───────────────────────────────
    float minOpacity     = 0.0f;   // opacity when dist >= fadeFarMeters
    float maxOpacity     = 1.0f;   // opacity when dist <= fadeNearMeters
    float fadeNearMeters = 0.3f;
    float fadeFarMeters  = 1.2f;

    bool visible = true;  // false = HideOverlay without destroying handle

    // ── Runtime only (not persisted) ──────────────────────────────────────────
    uint64_t       overlayHandle  = 0;        // vr::VROverlayHandle_t
    ID3D11Texture2D* renderTexture = nullptr;  // owned by ChromaRenderer (Phase 2)
};
