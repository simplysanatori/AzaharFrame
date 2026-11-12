// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame autosave: the "Autosaved" label's place over the top screen, the test that picks it
// or the toast, and its fade.

#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/geometry.h"
#include "frame_xr/vis/autosave_label.h"

using namespace FrameXr;
using namespace FrameXr::Vis;

namespace {

constexpr float kDeg = 3.14159265f / 180.0f;

/// The Frame's eyes at about its field of view (left and right 50°, up 45°, down 50°), 63 mm
/// apart, at eye height, the head pitched by `pitch_deg` (down negative).
void Eyes(float pitch_deg, EyeFov& l, EyeFov& r) {
    const float a = pitch_deg * kDeg * 0.5f;
    const Input::Quatf q{std::sin(a), 0.0f, 0.0f, std::cos(a)};
    for (EyeFov* e : {&l, &r}) {
        e->pose.orientation = q;
        e->left = -50.0f * kDeg;
        e->right = 50.0f * kDeg;
        e->up = 45.0f * kDeg;
        e->down = -50.0f * kDeg;
    }
    l.pose.position = {-0.0315f, 0.0f, 0.0f};
    r.pose.position = {0.0315f, 0.0f, 0.0f};
}

/// The default top screen: 2 m ahead, 1 m high, 5:3.
Input::Posef Top() {
    Input::Posef p;
    p.position = {0.0f, 0.0f, -2.0f};
    return p;
}

} // namespace

TEST_CASE("Autosaved label: over the top screen's top right corner", "[autosave][frame_xr]") {
    const float w = 5.0f / 3.0f, h = 1.0f;
    const Input::Posef p = LabelPose(Top(), w, h);
    // Its right edge on the screen's, its bottom 3 cm over the screen's top, in its plane
    CHECK(std::fabs(p.position.x + kLabelW * 0.5f - w * 0.5f) < 1e-4f);
    CHECK(std::fabs(p.position.y - kLabelH * 0.5f - (h * 0.5f + 0.03f)) < 1e-4f);
    CHECK(std::fabs(p.position.z + 2.0f) < 1e-4f);
    // A screen turned to the side: the label turns with it
    Input::Posef turned = Top();
    turned.orientation = {0.0f, std::sin(0.5f * 30.0f * kDeg), 0.0f, std::cos(0.5f * 30.0f * kDeg)};
    const Input::Posef q = LabelPose(turned, w, h);
    CHECK(q.orientation.y == turned.orientation.y);
    CHECK(q.position.z < -2.0f); // the right edge goes back
}

TEST_CASE("Autosaved label: shown when in view, else the toast", "[autosave][frame_xr]") {
    const Input::Posef label = LabelPose(Top(), 5.0f / 3.0f, 1.0f);
    EyeFov l, r;
    Eyes(0.0f, l, r);
    CHECK(LabelInView(label, l, r)); // looking at the screen
    Eyes(-30.0f, l, r);              // looking down at the bottom screen (round 2's t3)
    CHECK(!LabelInView(label, l, r));
    Eyes(0.0f, l, r);
    for (EyeFov* e : {&l, &r}) { // turned 70° to the left: the corner is out of view
        e->pose.orientation = {0.0f, std::sin(35.0f * kDeg), 0.0f, std::cos(35.0f * kDeg)};
    }
    CHECK(!LabelInView(label, l, r));
    // Behind the head
    Eyes(0.0f, l, r);
    CHECK(!PointInView(l, {0.0f, 0.0f, 1.0f}));
    // The margin: 10° inside the edge
    CHECK(PointInView(l, {std::tan(39.0f * kDeg) * 2.0f - 0.0315f, 0.0f, -2.0f}));
    CHECK(!PointInView(l, {std::tan(41.0f * kDeg) * 2.0f - 0.0315f, 0.0f, -2.0f}));
}

TEST_CASE("Autosaved label: 1.5 s with a 0.2 s fade", "[autosave][frame_xr]") {
    CHECK(LabelOpacity(-1) == 0.0f);
    CHECK(LabelOpacity(0) == 0.0f);
    CHECK(LabelOpacity(100'000'000) > 0.45f);
    CHECK(LabelOpacity(100'000'000) < 0.55f);
    CHECK(LabelOpacity(200'000'000) == 1.0f);
    CHECK(LabelOpacity(750'000'000) == 1.0f);
    CHECK(LabelOpacity(1'400'000'000) < 0.55f);
    CHECK(LabelOpacity(1'500'000'000) == 0.0f);
    CHECK(LabelLevel(0.0f) == 0);
    CHECK(LabelLevel(0.1f) == 1);
    CHECK(LabelLevel(0.5f) == 2);
    CHECK(LabelLevel(1.0f) == kLabelLevels);
}
