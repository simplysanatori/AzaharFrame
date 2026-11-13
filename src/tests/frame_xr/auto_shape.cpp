// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fixes7: Screen Shape: Automatic: the shape from the measured game camera, and the
// emulation thread's sampling of the projection in the uniforms.

#include <array>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/auto_shape.h"
#include "video_core/vr_auto_shape.h"

using Catch::Approx;
using namespace FrameXr::AutoShape;

namespace {
constexpr float k169 = 16.0f / 9.0f;
constexpr float k219 = 21.0f / 9.0f;

/// Feeds `aspect` every quarter second from `t` for `ms`; returns the time after.
std::int64_t Feed(Decider& d, float aspect, std::int64_t t, std::int64_t ms, bool* changed = nullptr) {
    for (std::int64_t e = 0; e < ms; e += 250) {
        t += 250;
        if (d.Add(aspect, t) && changed) {
            *changed = true;
        }
    }
    return t;
}

/// Uniform registers (rows of 4) with a perspective matrix at `base` as the 3DS games keep it:
/// the top screen is turned, so the first row scales y and the second x.
using Regs = std::array<std::array<float, 4>, 96>;
Regs WithProjection(std::size_t base, float tan_half_y, float aspect) {
    Regs f{};
    const float sy = 1.0f / tan_half_y;
    const float sx = sy / aspect;
    f[base + 0] = {0.0f, sy, 0.0f, 0.0f};
    f[base + 1] = {-sx, 0.0f, 0.0f, 0.0f};
    f[base + 2] = {0.0f, 0.0f, 0.5f, -1.0f};
    f[base + 3] = {0.0f, 0.0f, -1.0f, 0.0f};
    return f;
}
} // namespace

TEST_CASE("Auto shape: the nearest shape", "[frame_xr][autoshape]") {
    CHECK(NearestShape(400.0f / 240.0f)->value == 0);
    CHECK(NearestShape(1.78f)->value == 1);
    CHECK(NearestShape(2.33f)->value == 2);
    CHECK(NearestShape(1.6f)->value == 5);
    CHECK(NearestShape(1.71f)->value == 0); // 2.6 % over 5:3
    CHECK_FALSE(NearestShape(4.0f / 3.0f)); // not a shape Automatic picks
    CHECK_FALSE(NearestShape(2.0f));
    CHECK_FALSE(NearestShape(0.0f));
}

TEST_CASE("Auto shape: switches after 2 s of the same shape, eased", "[frame_xr][autoshape]") {
    Decider d;
    d.Reset();
    CHECK(d.Current().value == 0);
    std::int64_t t = 10'000;
    // 1.5 s of 16:9: not yet.
    bool changed = false;
    t = Feed(d, 1.778f, t, 1500, &changed);
    CHECK_FALSE(changed);
    CHECK(d.Current().value == 0);
    t = Feed(d, 1.778f, t, 750, &changed);
    CHECK(changed);
    CHECK(d.Current().value == 1);
    // Eased: in between at first, there after kEaseMs.
    const float mid = d.AspectAt(t + Decider::kEaseMs / 2);
    CHECK(mid > 400.0f / 240.0f);
    CHECK(mid < k169);
    CHECK(d.AspectAt(t + Decider::kEaseMs) == Approx(k169));
}

TEST_CASE("Auto shape: short cameras, gaps and wobble do not switch", "[frame_xr][autoshape]") {
    Decider d;
    d.Reset();
    std::int64_t t = 10'000;
    bool changed = false;
    // A 1.5 s cutscene at 21:9 between the game's 5:3.
    t = Feed(d, 2.33f, t, 1500, &changed);
    t = Feed(d, 1.667f, t, 2000, &changed);
    CHECK_FALSE(changed);
    // 16:9 seen in bits with gaps of over a second (a menu in between): never 2 s in a row.
    for (int i = 0; i < 4; i++) {
        t = Feed(d, 1.778f, t, 1250, &changed);
        t += 1500;
    }
    CHECK_FALSE(changed);
    // Something that is no shape (a 4:3 close-up) breaks the run too.
    t = Feed(d, 1.778f, t, 1500, &changed);
    t = Feed(d, 1.333f, t, 250, &changed);
    t = Feed(d, 1.778f, t, 1500, &changed);
    CHECK_FALSE(changed);
    // Hysteresis: at 16:9, a camera a little nearer 5:3 but within 3.5 % of 16:9 stays 16:9.
    d.Reset(k169);
    CHECK(d.Current().value == 1);
    t = Feed(d, 1.72f, t, 5000, &changed);
    CHECK_FALSE(changed);
    CHECK(d.Current().value == 1);
    // A 2D game or a menu: nothing measured, the shape stays.
    CHECK(d.AspectAt(t + 60'000) == Approx(k169));
}

TEST_CASE("Auto shape: starts at the hint, then follows the camera", "[frame_xr][autoshape]") {
    Decider d;
    d.Reset(k219);
    CHECK(d.Current().value == 2);
    CHECK(d.AspectAt(1) == Approx(k219));
    bool changed = false;
    Feed(d, 1.667f, 10'000, 2500, &changed);
    CHECK(changed);
    CHECK(d.Current().value == 0);
    d.Reset(1.0f); // a hint that is no shape: 5:3
    CHECK(d.Current().value == 0);
}

TEST_CASE("Auto shape: the projection in the uniforms", "[frame_xr][autoshape]") {
    using namespace VideoCore::Immersive;
    Projection p;
    // Found at any register; the lowest perspective block wins.
    Regs f = WithProjection(20, 0.5f, k169);
    REQUIRE(FindProjection(f, f.size(), p));
    CHECK(p.aspect == Approx(k169).margin(0.001));
    CHECK(p.tan_half_y == Approx(0.5f).margin(0.001));
    Regs g = WithProjection(40, 0.5f, k219);
    const Regs low = WithProjection(8, 0.6f, 400.0f / 240.0f);
    for (int i = 0; i < 4; i++) {
        g[8 + i] = low[8 + i];
    }
    REQUIRE(FindProjection(g, g.size(), p));
    CHECK(p.aspect == Approx(400.0f / 240.0f).margin(0.001));
    // An orthographic matrix (a HUD's), an identity, nothing: none.
    Regs o{};
    o[0] = {0.0f, 2.0f / 240.0f, 0.0f, -1.0f};
    o[1] = {-2.0f / 400.0f, 0.0f, 0.0f, 1.0f};
    o[2] = {0.0f, 0.0f, 1.0f, 0.0f};
    o[3] = {0.0f, 0.0f, 0.0f, 1.0f};
    CHECK_FALSE(FindProjection(o, o.size(), p));
    Regs zero{};
    CHECK_FALSE(FindProjection(zero, zero.size(), p));
}

TEST_CASE("Auto shape: sampling is rate-limited and publishes each window's winner",
          "[frame_xr][autoshape]") {
    using namespace VideoCore::Immersive;
    ShapeSampler sampler;
    MeasuredProjection out;
    const Regs wide = WithProjection(4, 0.5f, k169);
    const Regs hud = WithProjection(4, 0.5f, k219);
    std::int64_t us = 1'000'000'000;
    int read = 0;
    // 1000 uploads a second for 300 ms: at most one sample every 2 ms; the HUD's camera in one
    // upload of five does not win.
    for (int i = 0; i < 300; i++) {
        us += 1000;
        read += sampler.Sample(i % 5 == 0 ? hud : wide, wide.size(), us, out) ? 1 : 0;
    }
    CHECK(read <= 151);
    CHECK(read >= 140);
    float tan_half_y = 0.0f, aspect = 0.0f;
    std::int64_t stamp = 0;
    REQUIRE(out.Read(0, tan_half_y, aspect, stamp));
    CHECK(aspect == Approx(k169).margin(0.001));
}
