#include "GrabController.h"
#include "DeviceTracker.h"
#include "OverlayManager.h"
#include "Logger.h"

void GrabController::arm(int boxIndex) {
    m_armed       = true;
    m_active      = false;
    m_boxIndex    = boxIndex;
    m_wasGripping = false;
    LOG_INFO("GrabController: armed for box index {} — squeeze right grip to latch", boxIndex);
}

void GrabController::cancel() {
    if (m_armed || m_active)
        LOG_INFO("GrabController: cancelled for box index {}", m_boxIndex);
    m_armed    = false;
    m_active   = false;
    m_boxIndex = -1;
}

void GrabController::tick(const DeviceTracker& tracker, OverlayManager& mgr) {
    if (!m_armed && !m_active) return;

    const bool ctrlTracked = tracker.isRightControllerTracked();
    const bool gripping    = ctrlTracked && tracker.isRightGripping();

    // ── Armed: wait for grip leading edge to latch ────────────────────────────
    if (m_armed && !m_active) {
        if (gripping && !m_wasGripping) {
            PassthroughBox* box = mgr.boxAt(static_cast<std::size_t>(m_boxIndex));
            if (box && ctrlTracked) {
                const glm::vec3 ctrlPos(tracker.getRightControllerPose()[3]);
                const glm::vec3 boxPos(box->posX, box->posY, box->posZ);
                m_offset = boxPos - ctrlPos;
                m_active = true;
                LOG_INFO("GrabController: latched box {} — offset ({:.3f}, {:.3f}, {:.3f})",
                    m_boxIndex, m_offset.x, m_offset.y, m_offset.z);
            }
        }
        m_wasGripping = gripping;
        return;
    }

    // ── Active: track grip; update position or release ────────────────────────
    if (m_active) {
        if (!gripping) {
            LOG_INFO("GrabController: grip released — box {} dropped at new position", m_boxIndex);
            m_active = false;
            m_armed  = false;
            m_wasGripping = false;
            return;
        }

        if (ctrlTracked) {
            PassthroughBox* box = mgr.boxAt(static_cast<std::size_t>(m_boxIndex));
            if (box) {
                const glm::vec3 ctrlPos(tracker.getRightControllerPose()[3]);
                const glm::vec3 newPos = ctrlPos + m_offset;
                box->posX = newPos.x;
                box->posY = newPos.y;
                box->posZ = newPos.z;
            } else {
                // Box was removed while grabbed.
                cancel();
            }
        }
    }

    m_wasGripping = gripping;
}
