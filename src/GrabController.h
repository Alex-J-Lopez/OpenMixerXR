#pragma once
#include <glm/glm.hpp>

class DeviceTracker;
class OverlayManager;

// Manages the "arm → grip-to-latch → drag → release" workflow for physically
// repositioning a world-space box with the right controller (Phase 3.5).
//
// State machine:
//   idle   → arm(idx)     → armed
//   armed  → grip pressed → active  (offset latched: boxPos - ctrlPos)
//   active → grip released → idle   (box stays at new world position)
//   armed or active → cancel()  → idle
//
// Only translation is applied; rotation remains under dashboard slider control.
// GrabController::tick() is called from main.cpp after DeviceTracker::update()
// and before OverlayManager::frame(), so the updated position is read
// immediately that same frame.
class GrabController {
public:
    // Arm the grab for the given box index.
    // The grab does not latch until the right controller grip button is pressed.
    void arm(int boxIndex);

    // Abort (armed or active) — box stays at its current world position.
    void cancel();

    // Per-frame update. Call after DeviceTracker::update().
    void tick(const DeviceTracker& tracker, OverlayManager& mgr);

    bool isArmed()  const { return m_armed; }
    bool isActive() const { return m_active; }  // grip currently latched
    int  boxIndex() const { return m_boxIndex; }

private:
    bool      m_armed       = false;
    bool      m_active      = false;
    int       m_boxIndex    = -1;
    bool      m_wasGripping = false;  // for leading-edge detection
    glm::vec3 m_offset      = {};     // boxPos - ctrlPos at latch time
};
