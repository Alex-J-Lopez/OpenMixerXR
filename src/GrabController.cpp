#include "GrabController.h"
#include "DeviceTracker.h"
#include "OverlayManager.h"
#include "Logger.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <algorithm>   // std::clamp

// ── File-local math helpers ───────────────────────────────────────────────────

static glm::quat rotFromPose(const glm::mat4& pose) {
    return glm::quat_cast(glm::mat3(pose));
}

static glm::quat eulerToQuat(float yawDeg, float pitchDeg, float rollDeg) {
    const glm::quat yaw   = glm::angleAxis(glm::radians(yawDeg),   glm::vec3(0.f, 1.f, 0.f));
    const glm::quat pitch = glm::angleAxis(glm::radians(pitchDeg), glm::vec3(1.f, 0.f, 0.f));
    const glm::quat roll  = glm::angleAxis(glm::radians(rollDeg),  glm::vec3(0.f, 0.f, 1.f));
    return yaw * pitch * roll;
}

static void quatToEuler(const glm::quat& q, float& yawDeg, float& pitchDeg, float& rollDeg) {
    float y, p, r;
    glm::extractEulerAngleYXZ(glm::mat4_cast(q), y, p, r);
    yawDeg   = glm::degrees(y);
    pitchDeg = glm::degrees(p);
    rollDeg  = glm::degrees(r);
}

// ── Public interface ──────────────────────────────────────────────────────────

void GrabController::enableMoveMode(int boxIndex) {
    m_moveMode        = true;
    m_grabbing        = false;
    m_resizing        = false;
    m_boxIndex        = boxIndex;
    m_wasGripping     = false;
    m_wasLeftGripping = false;
    LOG_INFO("GrabController: move mode ON for box index {} — squeeze right grip to grab", boxIndex);
}

void GrabController::disableMoveMode() {
    if (m_moveMode)
        LOG_INFO("GrabController: move mode OFF for box index {}", m_boxIndex);
    m_moveMode        = false;
    m_grabbing        = false;
    m_resizing        = false;
    m_boxIndex        = -1;
    m_wasGripping     = false;
    m_wasLeftGripping = false;
}

void GrabController::tick(const DeviceTracker& tracker, OverlayManager& mgr) {
    if (!m_moveMode) return;

    const bool rightTracked = tracker.isRightControllerTracked();
    const bool gripping     = rightTracked && tracker.isRightGripping();
    const bool leftTracked  = tracker.isLeftControllerTracked();
    const bool leftGripping = leftTracked  && tracker.isLeftGripping();

    // ── Right grip leading edge: latch position + rotation ───────────────────
    if (gripping && !m_wasGripping) {
        PassthroughBox* box = mgr.boxAt(static_cast<std::size_t>(m_boxIndex));
        if (box) {
            const glm::mat4 pose   = tracker.getRightControllerPose();
            const glm::vec3 ctrlPos(pose[3]);
            m_posOffset    = glm::vec3(box->posX, box->posY, box->posZ) - ctrlPos;
            m_ctrlRotStart = rotFromPose(pose);
            m_boxRotStart  = eulerToQuat(box->rotYaw, box->rotPitch, box->rotRoll);
            m_grabbing     = true;
            LOG_INFO("GrabController: latched box {} — offset ({:.3f}, {:.3f}, {:.3f})",
                m_boxIndex, m_posOffset.x, m_posOffset.y, m_posOffset.z);
        }
    }

    // ── Right grip trailing edge: release latch, stay in move mode ───────────
    if (!gripping && m_wasGripping && m_grabbing) {
        m_grabbing        = false;
        m_resizing        = false;
        m_wasLeftGripping = false;
        LOG_INFO("GrabController: grip released — box {} repositioned, ready for next grab",
            m_boxIndex);
    }

    // ── While grabbing ────────────────────────────────────────────────────────
    if (m_grabbing && gripping) {
        PassthroughBox* box = mgr.boxAt(static_cast<std::size_t>(m_boxIndex));
        if (!box) {
            disableMoveMode();
        } else {
            // Position + rotation from right controller.
            if (rightTracked) {
                const glm::mat4 pose    = tracker.getRightControllerPose();
                const glm::vec3 ctrlPos(pose[3]);

                const glm::vec3 newPos  = ctrlPos + m_posOffset;
                box->posX = newPos.x;
                box->posY = newPos.y;
                box->posZ = newPos.z;

                const glm::quat rotDelta = rotFromPose(pose) * glm::inverse(m_ctrlRotStart);
                const glm::quat newRot   = glm::normalize(rotDelta * m_boxRotStart);
                quatToEuler(newRot, box->rotYaw, box->rotPitch, box->rotRoll);
            }
            // If right tracking is lost mid-grab: freeze box at current transform.

            // ── Left grip leading edge: latch resize reference ────────────────
            // Re-latches on every new left grip press so the user can release
            // and re-grip to start a fresh resize from the current size.
            if (leftGripping && !m_wasLeftGripping) {
                m_leftCtrlStart = glm::vec3(tracker.getLeftControllerPose()[3]);
                m_startWidth    = box->scaleWidth;
                m_startHeight   = box->scaleHeight;
                m_resizing      = true;
                LOG_INFO("GrabController: resize latched — start {:.3f} x {:.3f} m",
                    m_startWidth, m_startHeight);
            }

            // ── Left grip trailing edge: stop resize ──────────────────────────
            if (!leftGripping && m_wasLeftGripping && m_resizing) {
                m_resizing = false;
                LOG_INFO("GrabController: resize stopped — final {:.3f} x {:.3f} m",
                    box->scaleWidth, box->scaleHeight);
            }

            // ── While resizing: map left-hand world delta to width/height ─────
            // World X delta → scaleWidth,  World Y delta → scaleHeight.
            // Works naturally for boxes facing the user (most common case).
            if (m_resizing && leftGripping && leftTracked) {
                const glm::vec3 leftPos(tracker.getLeftControllerPose()[3]);
                const glm::vec3 delta  = leftPos - m_leftCtrlStart;
                box->scaleWidth  = std::clamp(m_startWidth  - delta.x, 0.05f, 5.0f);
                box->scaleHeight = std::clamp(m_startHeight + delta.y, 0.05f, 5.0f);
            }
            // If left tracking is lost mid-resize: freeze size at current values.
        }
    }

    m_wasGripping     = gripping;
    m_wasLeftGripping = leftGripping;
}
