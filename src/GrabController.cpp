#include "GrabController.h"
#include "DeviceTracker.h"
#include "OverlayManager.h"
#include "Logger.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

// ── helpers ───────────────────────────────────────────────────────────────────

// Extract the 3x3 rotation from a 4x4 pose matrix and return it as a unit quaternion.
static glm::quat rotFromPose(const glm::mat4& pose) {
    return glm::quat_cast(glm::mat3(pose));
}

// Build a box rotation quaternion from its YXZ Euler angles (degrees).
static glm::quat eulerToQuat(float yawDeg, float pitchDeg, float rollDeg) {
    const glm::quat yaw   = glm::angleAxis(glm::radians(yawDeg),   glm::vec3(0.f, 1.f, 0.f));
    const glm::quat pitch = glm::angleAxis(glm::radians(pitchDeg), glm::vec3(1.f, 0.f, 0.f));
    const glm::quat roll  = glm::angleAxis(glm::radians(rollDeg),  glm::vec3(0.f, 0.f, 1.f));
    return yaw * pitch * roll;
}

// Decompose a quaternion back to YXZ Euler angles (degrees).
// Uses glm::extractEulerAngleYXZ which matches the yaw*pitch*roll convention.
static void quatToEuler(const glm::quat& q, float& yawDeg, float& pitchDeg, float& rollDeg) {
    float y, p, r;
    glm::extractEulerAngleYXZ(glm::mat4_cast(q), y, p, r);
    yawDeg   = glm::degrees(y);
    pitchDeg = glm::degrees(p);
    rollDeg  = glm::degrees(r);
}

// ── public interface ──────────────────────────────────────────────────────────

void GrabController::enableMoveMode(int boxIndex) {
    m_moveMode    = true;
    m_grabbing    = false;
    m_boxIndex    = boxIndex;
    m_wasGripping = false;
    LOG_INFO("GrabController: move mode ON for box index {} — squeeze right grip to grab", boxIndex);
}

void GrabController::disableMoveMode() {
    if (m_moveMode)
        LOG_INFO("GrabController: move mode OFF for box index {}", m_boxIndex);
    m_moveMode = false;
    m_grabbing = false;
    m_boxIndex = -1;
}

void GrabController::tick(const DeviceTracker& tracker, OverlayManager& mgr) {
    if (!m_moveMode) return;

    const bool ctrlTracked = tracker.isRightControllerTracked();
    const bool gripping    = ctrlTracked && tracker.isRightGripping();

    // ── Leading edge: latch position and rotation offsets ────────────────────
    if (gripping && !m_wasGripping) {
        PassthroughBox* box = mgr.boxAt(static_cast<std::size_t>(m_boxIndex));
        if (box && ctrlTracked) {
            const glm::mat4 ctrlPose = tracker.getRightControllerPose();
            const glm::vec3 ctrlPos(ctrlPose[3]);
            const glm::vec3 boxPos(box->posX, box->posY, box->posZ);

            m_posOffset    = boxPos - ctrlPos;
            m_ctrlRotStart = rotFromPose(ctrlPose);
            m_boxRotStart  = eulerToQuat(box->rotYaw, box->rotPitch, box->rotRoll);
            m_grabbing     = true;

            LOG_INFO("GrabController: latched box {} — offset ({:.3f}, {:.3f}, {:.3f})",
                m_boxIndex, m_posOffset.x, m_posOffset.y, m_posOffset.z);
        }
        m_wasGripping = gripping;
        return;
    }

    // ── While grabbing: update position and rotation each frame ──────────────
    if (m_grabbing) {
        if (!gripping) {
            // Trailing edge: release latch, auto re-arm for next grip.
            m_grabbing = false;
            LOG_INFO("GrabController: grip released — box {} repositioned, ready for next grab",
                m_boxIndex);
        } else if (ctrlTracked) {
            PassthroughBox* box = mgr.boxAt(static_cast<std::size_t>(m_boxIndex));
            if (box) {
                const glm::mat4 ctrlPose       = tracker.getRightControllerPose();
                const glm::vec3 ctrlPos(ctrlPose[3]);
                const glm::quat ctrlRotCurrent = rotFromPose(ctrlPose);

                // Position: maintain the world-space offset from latch.
                const glm::vec3 newPos = ctrlPos + m_posOffset;
                box->posX = newPos.x;
                box->posY = newPos.y;
                box->posZ = newPos.z;

                // Rotation: apply the controller's rotation delta to the box's
                // starting rotation so the box "tumbles" with the hand.
                const glm::quat rotDelta = ctrlRotCurrent * glm::inverse(m_ctrlRotStart);
                const glm::quat newRot   = glm::normalize(rotDelta * m_boxRotStart);
                quatToEuler(newRot, box->rotYaw, box->rotPitch, box->rotRoll);
            } else {
                // Box was deleted while grabbed.
                disableMoveMode();
            }
        }
        // If controller tracking lost mid-grab: don't update, keep current box pose.
    }

    m_wasGripping = gripping;
}
