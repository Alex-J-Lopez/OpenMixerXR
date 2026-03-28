// layout_serialization_test.cpp
// Round-trip tests for LayoutStore serialization (SRD §3.4, §11.1).
// No VR runtime, no D3D11, no SteamVR — pure JSON logic only.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <optional>

#include "LayoutStore.h"
#include "PassthroughBox.h"

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, name)                                               \
    do {                                                                \
        if (cond) { std::printf("[PASS] %s\n", (name)); ++g_passed; }  \
        else      { std::printf("[FAIL] %s\n", (name)); ++g_failed; }  \
    } while (0)

static bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

// Build a fully-populated PassthroughBox with non-default values for every field.
static PassthroughBox makeBox(int suffix) {
    PassthroughBox b;
    b.id             = "testbox" + std::to_string(suffix);
    b.name           = "Box " + std::to_string(suffix);
    b.posX           = 1.0f  * suffix;
    b.posY           = 2.0f  * suffix;
    b.posZ           = -3.0f * suffix;
    b.rotYaw         = 15.5f * suffix;
    b.rotPitch       = -7.25f;
    b.rotRoll        = 3.14f;
    b.scaleWidth     = 0.75f * suffix;
    b.scaleHeight    = 0.45f;
    b.chromaR        = 0.1f;
    b.chromaG        = 0.8f;
    b.chromaB        = 0.3f;
    b.minOpacity     = 0.15f;
    b.maxOpacity     = 0.95f;
    b.fadeNearMeters = 0.4f;
    b.fadeFarMeters  = 2.5f;
    b.visible        = (suffix % 2 == 0);
    return b;
}

// ── Basic round-trip ──────────────────────────────────────────────────────────

static void test_roundtrip_identity() {
    Layout original;
    original.name          = "test layout";
    original.globalChromaR = 0.12f;
    original.globalChromaG = 0.56f;
    original.globalChromaB = 0.89f;
    original.boxes.push_back(makeBox(1));
    original.boxes.push_back(makeBox(2));

    const std::string json = LayoutStore::serializeLayout(original);
    std::string err;
    const auto restored = LayoutStore::deserializeLayout(json, err);

    CHECK(restored.has_value(),                                  "round-trip: parse succeeds");
    if (!restored) return;

    CHECK(restored->name == "test layout",                       "round-trip: name");
    CHECK(approx(restored->globalChromaR, 0.12f),                "round-trip: globalChromaR");
    CHECK(approx(restored->globalChromaG, 0.56f),                "round-trip: globalChromaG");
    CHECK(approx(restored->globalChromaB, 0.89f),                "round-trip: globalChromaB");
    CHECK(restored->boxes.size() == 2,                           "round-trip: box count");
}

// ── All box fields preserved ──────────────────────────────────────────────────

static void test_all_box_fields() {
    Layout layout;
    layout.name = "fields test";
    layout.boxes.push_back(makeBox(3));

    const std::string json = LayoutStore::serializeLayout(layout);
    std::string err;
    const auto restored = LayoutStore::deserializeLayout(json, err);

    CHECK(restored.has_value(),                                  "fields: parse succeeds");
    if (!restored || restored->boxes.empty()) return;

    const PassthroughBox& orig = layout.boxes[0];
    const PassthroughBox& rest = restored->boxes[0];

    CHECK(rest.id             == orig.id,                        "fields: id");
    CHECK(rest.name           == orig.name,                      "fields: name");
    CHECK(approx(rest.posX,            orig.posX),               "fields: posX");
    CHECK(approx(rest.posY,            orig.posY),               "fields: posY");
    CHECK(approx(rest.posZ,            orig.posZ),               "fields: posZ");
    CHECK(approx(rest.rotYaw,          orig.rotYaw),             "fields: rotYaw");
    CHECK(approx(rest.rotPitch,        orig.rotPitch),           "fields: rotPitch");
    CHECK(approx(rest.rotRoll,         orig.rotRoll),            "fields: rotRoll");
    CHECK(approx(rest.scaleWidth,      orig.scaleWidth),         "fields: scaleWidth");
    CHECK(approx(rest.scaleHeight,     orig.scaleHeight),        "fields: scaleHeight");
    CHECK(approx(rest.chromaR,         orig.chromaR),            "fields: chromaR");
    CHECK(approx(rest.chromaG,         orig.chromaG),            "fields: chromaG");
    CHECK(approx(rest.chromaB,         orig.chromaB),            "fields: chromaB");
    CHECK(approx(rest.minOpacity,      orig.minOpacity),         "fields: minOpacity");
    CHECK(approx(rest.maxOpacity,      orig.maxOpacity),         "fields: maxOpacity");
    CHECK(approx(rest.fadeNearMeters,  orig.fadeNearMeters),     "fields: fadeNearMeters");
    CHECK(approx(rest.fadeFarMeters,   orig.fadeFarMeters),      "fields: fadeFarMeters");
    CHECK(rest.visible == orig.visible,                          "fields: visible");
}

// ── Version checks ────────────────────────────────────────────────────────────

static void test_correct_version_accepted() {
    const std::string json =
        R"({"version":1,"name":"ok","globalChromaColor":[0,1,0.5],"boxes":[]})";
    std::string err;
    const auto result = LayoutStore::deserializeLayout(json, err);
    CHECK(result.has_value(),   "version: v1 accepted");
    CHECK(err.empty(),          "version: no error for v1");
}

static void test_unknown_version_rejected() {
    const std::string json =
        R"({"version":99,"name":"bad","globalChromaColor":[0,0,0],"boxes":[]})";
    std::string err;
    const auto result = LayoutStore::deserializeLayout(json, err);
    CHECK(!result.has_value(),  "version: unknown version rejected");
    CHECK(!err.empty(),         "version: error message set for unknown version");
}

static void test_missing_version_rejected() {
    const std::string json = R"({"name":"no-version","boxes":[]})";
    std::string err;
    const auto result = LayoutStore::deserializeLayout(json, err);
    CHECK(!result.has_value(),  "version: missing version rejected");
}

// ── Corrupt JSON handling (§6.2) ──────────────────────────────────────────────

static void test_corrupt_json_returns_nullopt() {
    const std::string json = "{ this is not valid JSON !!";
    std::string err;
    const auto result = LayoutStore::deserializeLayout(json, err);
    CHECK(!result.has_value(),  "corrupt: nullopt returned");
    CHECK(!err.empty(),         "corrupt: error message populated");
}

static void test_empty_string_returns_nullopt() {
    std::string err;
    const auto result = LayoutStore::deserializeLayout("", err);
    CHECK(!result.has_value(),  "corrupt: empty string returns nullopt");
}

// ── Edge cases ────────────────────────────────────────────────────────────────

static void test_empty_boxes_array() {
    Layout layout;
    layout.name = "empty boxes";
    // no boxes
    const std::string json = LayoutStore::serializeLayout(layout);
    std::string err;
    const auto result = LayoutStore::deserializeLayout(json, err);
    CHECK(result.has_value() && result->boxes.empty(),   "edge: empty boxes array round-trips");
}

static void test_version_field_preserved() {
    Layout layout;
    layout.name = "ver check";
    const std::string json = LayoutStore::serializeLayout(layout);
    std::string err;
    const auto result = LayoutStore::deserializeLayout(json, err);
    CHECK(result.has_value() && result->version == Layout::CURRENT_VERSION,
          "edge: version field value preserved");
}

static void test_visible_false_round_trips() {
    Layout layout;
    layout.name = "visible false";
    PassthroughBox b = makeBox(1);
    b.visible = false;
    layout.boxes.push_back(b);
    const std::string json = LayoutStore::serializeLayout(layout);
    std::string err;
    const auto result = LayoutStore::deserializeLayout(json, err);
    CHECK(result.has_value() && !result->boxes.empty() &&
          result->boxes[0].visible == false,
          "edge: visible=false round-trips correctly");
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    std::printf("=== layout_serialization_test ===\n\n");

    test_roundtrip_identity();
    std::printf("\n");

    test_all_box_fields();
    std::printf("\n");

    test_correct_version_accepted();
    test_unknown_version_rejected();
    test_missing_version_rejected();
    std::printf("\n");

    test_corrupt_json_returns_nullopt();
    test_empty_string_returns_nullopt();
    std::printf("\n");

    test_empty_boxes_array();
    test_version_field_preserved();
    test_visible_false_round_trips();

    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
