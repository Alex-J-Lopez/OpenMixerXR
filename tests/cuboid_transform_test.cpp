// cuboid_transform_test.cpp
// Unit tests for OverlayManager::computeFaceWorldMatrices (Phase 4.5).
// Verifies face positions and outward-normal directions for an axis-aligned box.
// No VR runtime is started — only openvr.h type definitions are used.

#include <cassert>
#include <cmath>
#include <cstdio>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <openvr.h>

#include "OverlayManager.h"
#include "PassthroughBox.h"

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, name)                                               \
    do {                                                                \
        if (cond) { std::printf("[PASS] %s\n", (name)); ++g_passed; }  \
        else      { std::printf("[FAIL] %s\n", (name)); ++g_failed; }  \
    } while (0)

static bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

// Helpers to extract position and outward-normal (+Z axis) from a face matrix.
static glm::vec3 facePos(const glm::mat4& m)    { return glm::vec3(m[3]); }
static glm::vec3 faceNormal(const glm::mat4& m) { return glm::normalize(glm::vec3(m[2])); }

// ── Axis-aligned box at origin ────────────────────────────────────────────────
// W=2, H=2, D=2 → face centers at ±1 on each axis.

static PassthroughBox makeUnitCuboid() {
    PassthroughBox b;
    b.posX        =  0.f;  b.posY  =  0.f;  b.posZ  =  0.f;
    b.rotYaw      =  0.f;  b.rotPitch = 0.f; b.rotRoll = 0.f;
    b.scaleWidth  =  2.f;
    b.scaleHeight =  2.f;
    b.scaleDepth  =  2.f;
    return b;
}

static void test_face_positions() {
    const auto m = OverlayManager::computeFaceWorldMatrices(makeUnitCuboid());

    CHECK(approx(facePos(m[0]).z, +1.f),   "position: Front   z = +depth/2");
    CHECK(approx(facePos(m[1]).z, -1.f),   "position: Back    z = -depth/2");
    CHECK(approx(facePos(m[2]).x, -1.f),   "position: Left    x = -width/2");
    CHECK(approx(facePos(m[3]).x, +1.f),   "position: Right   x = +width/2");
    CHECK(approx(facePos(m[4]).y, +1.f),   "position: Top     y = +height/2");
    CHECK(approx(facePos(m[5]).y, -1.f),   "position: Bottom  y = -height/2");
}

static void test_face_normals() {
    const auto m = OverlayManager::computeFaceWorldMatrices(makeUnitCuboid());

    // Each face's local +Z (column 2) must point outward from the box centre.
    CHECK(approx(faceNormal(m[0]).z, +1.f), "normal: Front   → +Z");
    CHECK(approx(faceNormal(m[1]).z, -1.f), "normal: Back    → -Z");
    CHECK(approx(faceNormal(m[2]).x, -1.f), "normal: Left    → -X");
    CHECK(approx(faceNormal(m[3]).x, +1.f), "normal: Right   → +X");
    CHECK(approx(faceNormal(m[4]).y, +1.f), "normal: Top     → +Y");
    CHECK(approx(faceNormal(m[5]).y, -1.f), "normal: Bottom  → -Y");
}

static void test_off_centre_box() {
    PassthroughBox b = makeUnitCuboid();
    b.posX = 3.f;  b.posY = -1.f;  b.posZ = 2.f;
    const auto m = OverlayManager::computeFaceWorldMatrices(b);

    CHECK(approx(facePos(m[0]).x, 3.f) && approx(facePos(m[0]).y, -1.f)
          && approx(facePos(m[0]).z, 3.f),   "offset: Front face at correct world pos");
    CHECK(approx(facePos(m[2]).x, 2.f),      "offset: Left face at posX - width/2");
    CHECK(approx(facePos(m[3]).x, 4.f),      "offset: Right face at posX + width/2");
}

static void test_zero_depth_front_only() {
    PassthroughBox b = makeUnitCuboid();
    b.scaleDepth = 0.f;
    const auto m = OverlayManager::computeFaceWorldMatrices(b);

    // Front face should be at z = centre_z + 0 = posZ.
    CHECK(approx(facePos(m[0]).z, b.posZ),   "flat: Front face at posZ when depth=0");
    // Normal still correct.
    CHECK(approx(faceNormal(m[0]).z, +1.f),  "flat: Front normal still +Z when depth=0");
}

static void test_rotated_box_90y() {
    // 90° yaw rotates the box: its local Z becomes world -X.
    // The front face (at +depth/2 along local Z) should be at world X = posX - depth/2.
    PassthroughBox b = makeUnitCuboid();
    b.rotYaw = 90.f;   // 90° yaw
    const auto m = OverlayManager::computeFaceWorldMatrices(b);

    // Front face position: rotated +Z local offset → -X world offset (approx -1,0,0)
    CHECK(approx(facePos(m[0]).x, -1.f, 1e-3f), "rotated: Front face offset along world -X");
    CHECK(approx(facePos(m[0]).z,  0.f, 1e-3f), "rotated: Front face z ≈ 0");

    // Front face normal should point along world -X (local +Z rotated 90° yaw).
    CHECK(approx(faceNormal(m[0]).x, -1.f, 1e-3f), "rotated: Front normal → world -X");
}

static void test_face_physical_dimensions() {
    PassthroughBox b = makeUnitCuboid();  // W=2, H=2, D=2
    b.scaleWidth  = 1.0f;
    b.scaleHeight = 0.5f;
    b.scaleDepth  = 0.3f;

    CHECK(approx(OverlayManager::facePhysWidth(b, 0),  1.0f), "physW: Front  = scaleWidth");
    CHECK(approx(OverlayManager::facePhysWidth(b, 1),  1.0f), "physW: Back   = scaleWidth");
    CHECK(approx(OverlayManager::facePhysWidth(b, 2),  0.3f), "physW: Left   = scaleDepth");
    CHECK(approx(OverlayManager::facePhysWidth(b, 3),  0.3f), "physW: Right  = scaleDepth");
    CHECK(approx(OverlayManager::facePhysWidth(b, 4),  1.0f), "physW: Top    = scaleWidth");
    CHECK(approx(OverlayManager::facePhysWidth(b, 5),  1.0f), "physW: Bottom = scaleWidth");

    CHECK(approx(OverlayManager::facePhysHeight(b, 0), 0.5f), "physH: Front  = scaleHeight");
    CHECK(approx(OverlayManager::facePhysHeight(b, 1), 0.5f), "physH: Back   = scaleHeight");
    CHECK(approx(OverlayManager::facePhysHeight(b, 2), 0.5f), "physH: Left   = scaleHeight");
    CHECK(approx(OverlayManager::facePhysHeight(b, 3), 0.5f), "physH: Right  = scaleHeight");
    CHECK(approx(OverlayManager::facePhysHeight(b, 4), 0.3f), "physH: Top    = scaleDepth");
    CHECK(approx(OverlayManager::facePhysHeight(b, 5), 0.3f), "physH: Bottom = scaleDepth");
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    std::printf("=== cuboid_transform_test ===\n\n");

    test_face_positions();
    std::printf("\n");

    test_face_normals();
    std::printf("\n");

    test_off_centre_box();
    std::printf("\n");

    test_zero_depth_front_only();
    std::printf("\n");

    test_rotated_box_90y();
    std::printf("\n");

    test_face_physical_dimensions();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
