#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <openvr.h>

#include "PassthroughBox.h"

// Math conversions between SteamVR and GLM types.
//
// HmdMatrix34_t layout (§8.2): row-major 3×4, m[row][col].
//   Column 3 carries the translation (tx = m[0][3], ty = m[1][3], tz = m[2][3]).
// glm::mat4 layout: column-major 4×4, mat[col][row].
//
// These conversions were verified correct during Phase 1 bring-up.
// Do NOT transpose or invert — the round-trip test in box_transform_test.cpp confirms them.

namespace MathHelpers {

// HmdMatrix34_t  →  glm::mat4
inline glm::mat4 steamVRToGlm(const vr::HmdMatrix34_t& m) {
    return glm::mat4(
        m.m[0][0], m.m[1][0], m.m[2][0], 0.0f,   // col 0
        m.m[0][1], m.m[1][1], m.m[2][1], 0.0f,   // col 1
        m.m[0][2], m.m[1][2], m.m[2][2], 0.0f,   // col 2
        m.m[0][3], m.m[1][3], m.m[2][3], 1.0f    // col 3  (translation)
    );
}

// glm::mat4  →  HmdMatrix34_t
inline vr::HmdMatrix34_t glmToSteamVR(const glm::mat4& m) {
    vr::HmdMatrix34_t r = {};
    r.m[0][0] = m[0][0]; r.m[0][1] = m[1][0]; r.m[0][2] = m[2][0]; r.m[0][3] = m[3][0];
    r.m[1][0] = m[0][1]; r.m[1][1] = m[1][1]; r.m[1][2] = m[2][1]; r.m[1][3] = m[3][1];
    r.m[2][0] = m[0][2]; r.m[2][1] = m[1][2]; r.m[2][2] = m[2][2]; r.m[2][3] = m[3][2];
    return r;
}

// Build a world-space HmdMatrix34_t from a position and a rotation quaternion.
inline vr::HmdMatrix34_t buildTransform(const glm::vec3& pos, const glm::quat& rot) {
    glm::mat4 m = glm::mat4_cast(rot);
    m[3] = glm::vec4(pos, 1.0f);   // set translation column
    return glmToSteamVR(m);
}

// Distance-based opacity per SRD §8.3.
//   dist <= fadeNear  →  maxOpacity
//   dist >= fadeFar   →  minOpacity
//   in between        →  smoothstep blend
inline float computeOpacity(float dist,
                             float fadeNear, float fadeFar,
                             float minOpacity, float maxOpacity) {
    if (dist <= fadeNear) return maxOpacity;
    if (dist >= fadeFar)  return minOpacity;
    float t = glm::smoothstep(fadeNear, fadeFar, dist);
    return glm::mix(maxOpacity, minOpacity, t);
}

inline float computeOpacity(const PassthroughBox& box, float dist) {
    return computeOpacity(dist,
                          box.fadeNearMeters, box.fadeFarMeters,
                          box.minOpacity,     box.maxOpacity);
}

}  // namespace MathHelpers
