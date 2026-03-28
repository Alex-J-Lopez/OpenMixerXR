// box_transform_test.cpp
// Tests for MathHelpers: matrix round-trip and computeOpacity edge cases (SRD §3.4).
// No VR runtime is started — only the type definitions from openvr.h are used.

#include <cassert>
#include <cmath>
#include <cstdio>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <openvr.h>

#include "MathHelpers.h"
#include "PassthroughBox.h"

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, name)                                       \
    do {                                                        \
        if (cond) { std::printf("[PASS] %s\n", (name)); ++g_passed; } \
        else      { std::printf("[FAIL] %s\n", (name)); ++g_failed; } \
    } while (0)

static bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

// ── Matrix round-trip ─────────────────────────────────────────────────────────

static void test_identity_roundtrip() {
    vr::HmdMatrix34_t vr = {};
    vr.m[0][0] = 1.f; vr.m[1][1] = 1.f; vr.m[2][2] = 1.f;

    auto glm  = MathHelpers::steamVRToGlm(vr);
    auto back = MathHelpers::glmToSteamVR(glm);

    CHECK(approx(back.m[0][0], 1.f) && approx(back.m[1][1], 1.f) &&
          approx(back.m[2][2], 1.f) && approx(back.m[0][3], 0.f),
          "identity round-trip");
}

static void test_translation_roundtrip() {
    vr::HmdMatrix34_t vr = {};
    vr.m[0][0] = 1.f; vr.m[1][1] = 1.f; vr.m[2][2] = 1.f;
    vr.m[0][3] = 1.5f; vr.m[1][3] = -2.0f; vr.m[2][3] = 3.25f;

    auto back = MathHelpers::glmToSteamVR(MathHelpers::steamVRToGlm(vr));

    CHECK(approx(back.m[0][3],  1.5f) &&
          approx(back.m[1][3], -2.0f) &&
          approx(back.m[2][3],  3.25f),
          "translation round-trip");
}

static void test_rotation_roundtrip() {
    // 90° rotation around Y axis.
    vr::HmdMatrix34_t vr = {};
    vr.m[0][0] =  0.f; vr.m[0][2] =  1.f;
    vr.m[1][1] =  1.f;
    vr.m[2][0] = -1.f; vr.m[2][2] =  0.f;

    auto back = MathHelpers::glmToSteamVR(MathHelpers::steamVRToGlm(vr));

    CHECK(approx(back.m[0][0], 0.f) && approx(back.m[0][2], 1.f) &&
          approx(back.m[2][0], -1.f) && approx(back.m[2][2], 0.f),
          "rotation round-trip (90° Y)");
}

static void test_buildTransform_translation() {
    const glm::vec3 pos(1.f, 2.f, -3.f);
    const glm::quat identity = glm::quat(1.f, 0.f, 0.f, 0.f);

    auto t = MathHelpers::buildTransform(pos, identity);

    CHECK(approx(t.m[0][3],  1.f) &&
          approx(t.m[1][3],  2.f) &&
          approx(t.m[2][3], -3.f),
          "buildTransform translation");
}

static void test_buildTransform_identity_rotation() {
    const glm::quat identity = glm::quat(1.f, 0.f, 0.f, 0.f);
    auto t = MathHelpers::buildTransform(glm::vec3(0.f), identity);

    CHECK(approx(t.m[0][0], 1.f) && approx(t.m[1][1], 1.f) && approx(t.m[2][2], 1.f),
          "buildTransform identity rotation");
}

// ── computeOpacity ────────────────────────────────────────────────────────────

static PassthroughBox makeBox(float fadeNear, float fadeFar,
                               float minOp, float maxOp) {
    PassthroughBox b;
    b.fadeNearMeters = fadeNear;
    b.fadeFarMeters  = fadeFar;
    b.minOpacity     = minOp;
    b.maxOpacity     = maxOp;
    return b;
}

static void test_opacity_at_zero_dist() {
    auto b = makeBox(0.3f, 1.2f, 0.f, 1.f);
    CHECK(approx(MathHelpers::computeOpacity(b, 0.0f), 1.0f),
          "opacity: dist=0  → maxOpacity");
}

static void test_opacity_at_fade_near() {
    auto b = makeBox(0.3f, 1.2f, 0.f, 1.f);
    CHECK(approx(MathHelpers::computeOpacity(b, 0.3f), 1.0f),
          "opacity: dist=fadeNear → maxOpacity");
}

static void test_opacity_at_fade_far() {
    auto b = makeBox(0.3f, 1.2f, 0.f, 1.f);
    CHECK(approx(MathHelpers::computeOpacity(b, 1.2f), 0.0f),
          "opacity: dist=fadeFar → minOpacity");
}

static void test_opacity_beyond_fade_far() {
    auto b = makeBox(0.3f, 1.2f, 0.f, 1.f);
    CHECK(approx(MathHelpers::computeOpacity(b, 5.0f), 0.0f),
          "opacity: dist >> fadeFar → minOpacity");
}

static void test_opacity_midpoint_in_range() {
    auto b = makeBox(0.3f, 1.2f, 0.f, 1.f);
    float mid = MathHelpers::computeOpacity(b, 0.75f);
    CHECK(mid > 0.0f && mid < 1.0f,
          "opacity: midpoint dist → value in (minOpacity, maxOpacity)");
}

static void test_opacity_custom_bounds() {
    auto b = makeBox(0.3f, 1.2f, 0.2f, 0.8f);
    CHECK(approx(MathHelpers::computeOpacity(b, 0.0f), 0.8f),
          "opacity: custom maxOpacity at dist=0");
    CHECK(approx(MathHelpers::computeOpacity(b, 5.0f), 0.2f),
          "opacity: custom minOpacity at dist=5");
}

static void test_opacity_monotone_decreasing() {
    auto b = makeBox(0.3f, 1.2f, 0.f, 1.f);
    float prev = MathHelpers::computeOpacity(b, 0.0f);
    bool ok = true;
    for (int i = 1; i <= 20; ++i) {
        float dist = i * 0.1f;
        float cur  = MathHelpers::computeOpacity(b, dist);
        if (cur > prev + 1e-5f) { ok = false; break; }
        prev = cur;
    }
    CHECK(ok, "opacity: monotonically non-increasing with distance");
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    std::printf("=== box_transform_test ===\n\n");

    test_identity_roundtrip();
    test_translation_roundtrip();
    test_rotation_roundtrip();
    test_buildTransform_translation();
    test_buildTransform_identity_rotation();

    std::printf("\n");

    test_opacity_at_zero_dist();
    test_opacity_at_fade_near();
    test_opacity_at_fade_far();
    test_opacity_beyond_fade_far();
    test_opacity_midpoint_in_range();
    test_opacity_custom_bounds();
    test_opacity_monotone_decreasing();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
