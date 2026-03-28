#pragma once
#include <glm/glm.hpp>
#include <openvr.h>

// Queries the HMD pose from SteamVR each frame via GetDeviceToAbsoluteTrackingPose.
// Used by OverlayManager to compute per-box distance-based opacity (SRD §8.3).
// Also feeds Phase 3 "Snap to HMD" (FR-06/FR-10) — see phase-1-findings.md §6.
class DeviceTracker {
public:
    // Call once per frame, before OverlayManager::frame().
    void update(vr::IVRSystem* sys);

    glm::vec3 getHmdPosition() const;
    glm::mat4 getHmdPose()     const { return m_hmdPose; }
    bool      isHmdTracked()   const { return m_tracked; }

private:
    glm::mat4 m_hmdPose = glm::mat4(1.0f);
    bool      m_tracked = false;
};
