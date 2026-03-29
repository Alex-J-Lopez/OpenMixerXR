#pragma once
#include <glm/glm.hpp>
#include <openvr.h>

// Queries HMD and controller poses from SteamVR each frame.
//
// Controller grip state is read via the legacy GetControllerState API —
// no action manifest changes are required (Phase 3.5).
// Grip is used by GrabController to latch the world-space drag offset.
//
// Hot-plug robustness (Phase 5):
//   Device indices are cached each frame. handleEvent() immediately resets the
//   appropriate controller's state when TrackedDeviceDeactivated fires so that
//   GrabController sees the loss in the same event-pump pass rather than one
//   frame later.
class DeviceTracker {
public:
    // Call once per frame, before OverlayManager::frame() and GrabController::tick().
    void update(vr::IVRSystem* sys);

    // Feed VR system events for hot-plug handling.
    // Call for every event from VRSystem()->PollNextEvent().
    void handleEvent(const vr::VREvent_t& event);

    // ── HMD ──────────────────────────────────────────────────────────────────
    glm::vec3 getHmdPosition() const;
    glm::mat4 getHmdPose()     const { return m_hmdPose; }
    bool      isHmdTracked()   const { return m_tracked; }

    // ── Right controller ──────────────────────────────────────────────────────
    glm::mat4 getRightControllerPose()   const { return m_rightPose; }
    bool      isRightControllerTracked() const { return m_rightTracked; }
    // True while the physical grip button is held on the right controller.
    bool      isRightGripping()          const { return m_rightGripping; }

    // ── Left controller ───────────────────────────────────────────────────────
    glm::mat4 getLeftControllerPose()    const { return m_leftPose; }
    bool      isLeftControllerTracked()  const { return m_leftTracked; }
    bool      isLeftGripping()           const { return m_leftGripping; }

private:
    // HMD
    glm::mat4 m_hmdPose = glm::mat4(1.0f);
    bool      m_tracked = false;

    // Right controller
    glm::mat4 m_rightPose     = glm::mat4(1.0f);
    bool      m_rightTracked  = false;
    bool      m_rightGripping = false;
    uint32_t  m_rightIdx      = vr::k_unTrackedDeviceIndexInvalid;

    // Left controller
    glm::mat4 m_leftPose      = glm::mat4(1.0f);
    bool      m_leftTracked   = false;
    bool      m_leftGripping  = false;
    uint32_t  m_leftIdx       = vr::k_unTrackedDeviceIndexInvalid;
};
