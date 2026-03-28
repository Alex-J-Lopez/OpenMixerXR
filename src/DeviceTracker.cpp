#include "DeviceTracker.h"
#include "MathHelpers.h"

void DeviceTracker::update(vr::IVRSystem* sys) {
    m_tracked       = false;
    m_rightTracked  = false;
    m_leftTracked   = false;
    m_rightGripping = false;
    m_leftGripping  = false;

    if (!sys) return;

    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    sys->GetDeviceToAbsoluteTrackingPose(
        vr::TrackingUniverseStanding,
        0.0f,   // 0 = current pose, not predicted
        poses,
        vr::k_unMaxTrackedDeviceCount
    );

    // ── HMD ──────────────────────────────────────────────────────────────────
    const auto& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
    m_tracked = hmd.bPoseIsValid &&
                hmd.eTrackingResult == vr::TrackingResult_Running_OK;
    if (m_tracked)
        m_hmdPose = MathHelpers::steamVRToGlm(hmd.mDeviceToAbsoluteTracking);

    // ── Right controller ──────────────────────────────────────────────────────
    const uint32_t rightIdx = sys->GetTrackedDeviceIndexForControllerRole(
        vr::TrackedControllerRole_RightHand);
    if (rightIdx < vr::k_unMaxTrackedDeviceCount) {
        const auto& rp = poses[rightIdx];
        m_rightTracked = rp.bPoseIsValid &&
                         rp.eTrackingResult == vr::TrackingResult_Running_OK;
        if (m_rightTracked) {
            m_rightPose = MathHelpers::steamVRToGlm(rp.mDeviceToAbsoluteTracking);
            vr::VRControllerState_t state;
            if (sys->GetControllerState(rightIdx, &state, sizeof(state)))
                m_rightGripping =
                    (state.ulButtonPressed &
                     vr::ButtonMaskFromId(vr::k_EButton_Grip)) != 0;
        }
    }

    // ── Left controller ───────────────────────────────────────────────────────
    const uint32_t leftIdx = sys->GetTrackedDeviceIndexForControllerRole(
        vr::TrackedControllerRole_LeftHand);
    if (leftIdx < vr::k_unMaxTrackedDeviceCount) {
        const auto& lp = poses[leftIdx];
        m_leftTracked = lp.bPoseIsValid &&
                        lp.eTrackingResult == vr::TrackingResult_Running_OK;
        if (m_leftTracked) {
            m_leftPose = MathHelpers::steamVRToGlm(lp.mDeviceToAbsoluteTracking);
            vr::VRControllerState_t state;
            if (sys->GetControllerState(leftIdx, &state, sizeof(state)))
                m_leftGripping =
                    (state.ulButtonPressed &
                     vr::ButtonMaskFromId(vr::k_EButton_Grip)) != 0;
        }
    }
}

glm::vec3 DeviceTracker::getHmdPosition() const {
    return glm::vec3(m_hmdPose[3]);
}
