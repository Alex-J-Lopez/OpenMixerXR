#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class DeviceTracker;
class OverlayManager;

// Manages physical repositioning and resizing of a world-space box.
//
// Move mode state machine (Phase 3.5):
//
//   Move mode OFF  →  enableMoveMode(idx)  →  Move mode ON (idle)
//   Move mode ON   →  right grip pressed   →  Grabbing
//     Grabbing     →  each frame           →  box follows right controller
//                                              (position offset + rotation delta)
//     Grabbing     →  left grip pressed    →  Also Resizing
//       Resizing   →  each frame           →  scaleWidth  += leftDeltaX
//                                              scaleHeight += leftDeltaY
//       Resizing   →  left grip released   →  back to Grabbing only
//     Grabbing     →  right grip released  →  Move mode ON (idle) — auto re-arms
//   Move mode ON   →  disableMoveMode()    →  Move mode OFF
//
// Resize reference is re-latched on every left-grip press, so the user can
// release and re-grip the left hand without losing their previous size change.
//
// tick() must be called after DeviceTracker::update() and before
// OverlayManager::frame() so the updated transform is applied that same frame.
class GrabController {
public:
    void enableMoveMode(int boxIndex);
    void disableMoveMode();

    // Per-frame update. Handles all grip leading/trailing edges.
    void tick(const DeviceTracker& tracker, OverlayManager& mgr);

    bool isMoveMode()  const { return m_moveMode; }
    bool isGrabbing()  const { return m_grabbing; }   // right grip latched
    bool isResizing()  const { return m_resizing; }   // left grip latched while grabbing
    int  boxIndex()    const { return m_boxIndex; }

private:
    // Move mode
    bool      m_moveMode        = false;
    int       m_boxIndex        = -1;

    // Right grip (grab) state
    bool      m_grabbing        = false;
    bool      m_wasGripping     = false;
    glm::vec3 m_posOffset       = {};
    glm::quat m_boxRotStart     = glm::quat(1.f, 0.f, 0.f, 0.f);
    glm::quat m_ctrlRotStart    = glm::quat(1.f, 0.f, 0.f, 0.f);

    // Left grip (resize) state — only active while m_grabbing is true
    bool      m_resizing        = false;
    bool      m_wasLeftGripping = false;
    glm::vec3 m_leftCtrlStart   = {};
    float     m_startWidth      = 0.5f;
    float     m_startHeight     = 0.3f;
};
