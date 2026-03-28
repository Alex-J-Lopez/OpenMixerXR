#pragma once
#include <cstdint>

namespace Config {

    // ── App registration ──────────────────────────────────────────────────────
    inline constexpr const char* APP_KEY            = "openmixer.vr.overlay";
    inline constexpr const char* OVERLAY_KEY_PREFIX = "openmixer.box.";

    // ── Default chroma color (#00FF80 to match Virtual Desktop default) ────────
    // These are linear 0–1 float values passed to ClearRenderTargetView.
    inline constexpr float CHROMA_R = 0.000f;
    inline constexpr float CHROMA_G = 1.000f;
    inline constexpr float CHROMA_B = 0.502f;  // 128/255 ≈ #80

    // ── D3D11 texture resolution for chroma boxes ─────────────────────────────
    inline constexpr uint32_t TEXTURE_WIDTH  = 512;
    inline constexpr uint32_t TEXTURE_HEIGHT = 512;

    // ── Phase 1 test overlay — world position in standing universe (meters) ───
    inline constexpr float DEFAULT_BOX_X = 0.0f;
    inline constexpr float DEFAULT_BOX_Y = 1.0f;
    inline constexpr float DEFAULT_BOX_Z = -1.0f;

    // ── Default overlay dimensions ────────────────────────────────────────────
    inline constexpr float DEFAULT_BOX_WIDTH  = 0.5f;  // meters
    inline constexpr float DEFAULT_BOX_HEIGHT = 0.3f;  // meters (scale.y via SetOverlayWidthInMeters aspect)

    // ── Opacity fade defaults ─────────────────────────────────────────────────
    inline constexpr float DEFAULT_FADE_NEAR   = 0.3f;  // meters — at this dist: maxOpacity
    inline constexpr float DEFAULT_FADE_FAR    = 1.2f;  // meters — at this dist: minOpacity
    inline constexpr float DEFAULT_MIN_OPACITY = 0.0f;
    inline constexpr float DEFAULT_MAX_OPACITY = 1.0f;

}  // namespace Config
