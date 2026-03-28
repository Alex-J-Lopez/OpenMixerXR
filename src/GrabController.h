#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class DeviceTracker;
class OverlayManager;

// Manages physical repositioning of a world-space box with the right controller.
//
// State machine (Phase 3.5 revised):
//
//   Move mode OFF → enableMoveMode(idx) → Move mode ON (idle)
//   Move mode ON idle  → grip pressed  → Grabbing (latch position + rotation)
//   Grabbing           → each frame    → box follows controller (pos + rot delta)
//   Grabbing           → grip released → Move mode ON (idle) — auto re-arms
//   Move mode ON (any) → disableMoveMode() → Move mode OFF
//
// The user enables move mode once from the dashboard, then grips as many times
// as needed to reposition the box, and finally disables from the dashboard.
//
// Both position AND rotation are applied during a grab:
//   - Position:  newPos = ctrlPos + (boxPos - ctrlPos at latch)
//   - Rotation:  newRot = (ctrlRot * inverse(ctrlRot_at_latch)) * boxRot_at_latch
//
// tick() must be called after DeviceTracker::update() and before
// OverlayManager::frame() so the updated transform is applied that same frame.
class GrabController {
public:
    // Enable persistent move mode for the given box index.
    // The box will not move until the user squeezes the right grip.
    void enableMoveMode(int boxIndex);

    // Disable move mode — box stays at its current transform.
    void disableMoveMode();

    // Per-frame update. Handles latch, position+rotation update, and release.
    void tick(const DeviceTracker& tracker, OverlayManager& mgr);

    // True while move mode is on (idle or grabbing).
    bool isMoveMode()   const { return m_moveMode; }

    // True while the grip is physically held and the box is being dragged.
    bool isGrabbing()   const { return m_grabbing; }

    // Index of the box currently in move mode (-1 if none).
    int  boxIndex()     const { return m_boxIndex; }

private:
    bool      m_moveMode    = false;
    bool      m_grabbing    = false;   // grip currently latched
    int       m_boxIndex    = -1;
    bool      m_wasGripping = false;   // for leading/trailing edge detection

    // State captured at latch time (leading edge of grip press).
    glm::vec3 m_posOffset    = {};             // boxPos - ctrlPos
    glm::quat m_boxRotStart  = glm::quat(1.f, 0.f, 0.f, 0.f);
    glm::quat m_ctrlRotStart = glm::quat(1.f, 0.f, 0.f, 0.f);
};
