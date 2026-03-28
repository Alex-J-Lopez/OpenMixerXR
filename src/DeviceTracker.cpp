#include "DeviceTracker.h"
#include "MathHelpers.h"

void DeviceTracker::update(vr::IVRSystem* sys) {
    if (!sys) {
        m_tracked = false;
        return;
    }

    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    sys->GetDeviceToAbsoluteTrackingPose(
        vr::TrackingUniverseStanding,
        0.0f,   // seconds predicted ahead (0 = current)
        poses,
        vr::k_unMaxTrackedDeviceCount
    );

    const auto& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];
    m_tracked = hmd.bPoseIsValid &&
                hmd.eTrackingResult == vr::TrackingResult_Running_OK;

    if (m_tracked)
        m_hmdPose = MathHelpers::steamVRToGlm(hmd.mDeviceToAbsoluteTracking);
}

glm::vec3 DeviceTracker::getHmdPosition() const {
    // Column 3 of the 4×4 pose matrix is the world-space translation.
    return glm::vec3(m_hmdPose[3]);
}
