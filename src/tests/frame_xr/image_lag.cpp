// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame imglag: the picture on view by the serial each xrEndFrame carried
// (frame_xr/image_lag.h). A model compositor shows the image an xrEndFrame carried one refresh
// later (SteamVR) or at once; the screen must be anchored to the picture it shows on every
// refresh, through irregular cadences, hidden frames, pictures without a pose and recentres.
// The model compositor is the ASSUMPTION the app is built on (its "no layer before: the newest at
// once" rule included), so these tests check the bookkeeping against it, not SteamVR: only the
// steady case is measured (the headset A/B); the fallbacks are counted in the log for the next.

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "common/settings.h" // AzaharFrame flash: vr_xr_image_lag's default
#include "frame_xr/image_lag.h"
#include "frame_xr/immersive_layout.h"

namespace IL = FrameXr::ImageLag;
using FrameXr::kShownPictures;
using FrameXr::ShownPicture;

namespace {

constexpr int64_t kPeriod = 8'333'333; // 120 Hz

// The pictures released, newest first (as Runtime::ReleaseFrame keeps them).
struct Released {
    std::array<ShownPicture, kShownPictures> pics{};
    uint64_t serial = 0;
    void Release(int64_t pose_time, bool valid = true) {
        for (std::size_t i = kShownPictures - 1; i > 0; i--) {
            pics[i] = pics[i - 1];
        }
        pics[0].serial = ++serial;
        pics[0].content.valid = valid;
        pics[0].content.time = pose_time;
        pics[0].content.orientation[1] = static_cast<float>(serial); // tells pictures apart
    }
};

// The compositor: it shows the image of the last xrEndFrame that carried the layer, `lag`
// refreshes late; a layer absent on the frame before has no older image, so the new one at once.
struct Compositor {
    uint32_t lag = 1;
    uint64_t carried_before = 0; // 0: the frame before had no layer
    uint64_t Show(uint64_t carried_now) {
        const uint64_t shown = lag == 0 || carried_before == 0 ? carried_now : carried_before;
        carried_before = carried_now;
        return shown;
    }
};

struct Run {
    int frames = 0, shown = 0, mispaired = 0, unanchored = 0;
};

// One display refresh: the app anchors to PictureOnView, the frame carries the newest picture
// (unless hidden), the compositor shows what it shows. Mispaired: the anchor's picture is not
// the one on view.
void Refresh(Released& r, Compositor& c, uint64_t& sent, uint32_t app_lag, bool hidden, Run& run) {
    run.frames++;
    const IL::OnView on = IL::PictureOnView(r.pics, sent, app_lag);
    const bool carried = !hidden && r.pics[0].serial != 0;
    const uint64_t shown = c.Show(carried ? r.pics[0].serial : 0);
    if (carried) {
        run.shown++;
        if (shown != on.picture.serial) {
            run.mispaired++;
        }
    }
    sent = IL::SentAfterEnd(carried, true, true, r.pics[0].serial);
}

} // namespace

TEST_CASE("Image lag: the setting, the switch and what an xrEndFrame leaves", "[frame_xr][image_lag]") {
    // AzaharFrame flash: 0 by default (the owner's A/B of 2026-11-14), the setting's default too.
    CHECK(IL::kDefaultLag == 0);
    CHECK(Settings::values.vr_xr_image_lag.GetDefault() == IL::kDefaultLag);
    CHECK(IL::OverrideFrom(std::nullopt) == std::nullopt);
    CHECK(IL::OverrideFrom(std::string("0")) == 0u);
    CHECK(IL::OverrideFrom(std::string("1")) == 1u);
    CHECK(IL::OverrideFrom(std::string("")) == std::nullopt);
    CHECK(IL::OverrideFrom(std::string("-1")) == std::nullopt); // the old ANCHOR_LAG's value
    CHECK(IL::OverrideFrom(std::string("2")) == std::nullopt);
    CHECK(IL::InEffect(1, std::nullopt) == 1);
    CHECK(IL::InEffect(0, std::nullopt) == 0);
    CHECK(IL::InEffect(7, std::nullopt) == 1); // clamped
    CHECK(IL::InEffect(1, 0u) == 0);           // the switch wins
    CHECK(IL::InEffect(0, 1u) == 1);

    // Serial tracking: only a frame that carried the top layer, rendered and succeeded.
    CHECK(IL::SentAfterEnd(true, true, true, 42) == 42);
    CHECK(IL::SentAfterEnd(false, true, true, 42) == 0); // hidden, swapped out, no picture
    CHECK(IL::SentAfterEnd(true, false, true, 42) == 0); // shouldRender false
    CHECK(IL::SentAfterEnd(true, true, false, 42) == 0); // xrEndFrame failed

    CHECK(IL::SerialOnView(9, 8, 1) == 8);
    CHECK(IL::SerialOnView(9, 0, 1) == 9);
    CHECK(IL::SerialOnView(9, 8, 0) == 9);
}

TEST_CASE("Image lag: the picture on view by serial", "[frame_xr][image_lag]") {
    Released r;
    CHECK(IL::PictureOnView(r.pics, 0, 1).source == IL::Source::None);
    CHECK(IL::PictureOnView(r.pics, 0, 1).picture.serial == 0);
    r.Release(100);
    r.Release(200);
    r.Release(300); // serials 1, 2, 3
    SECTION("lag 0: the newest") {
        const IL::OnView o = IL::PictureOnView(r.pics, 2, 0);
        CHECK(o.picture.serial == 3);
        CHECK(o.source == IL::Source::Newest);
    }
    SECTION("lag 1: the one sent, whatever came after it") {
        const IL::OnView o = IL::PictureOnView(r.pics, 1, 1);
        CHECK(o.picture.serial == 1); // picture 2 was never on view
        CHECK(o.picture.content.time == 100);
        CHECK(o.source == IL::Source::Sent);
    }
    SECTION("lag 1, the frame before had no top layer: the newest at once") {
        const IL::OnView o = IL::PictureOnView(r.pics, 0, 1);
        CHECK(o.picture.serial == 3);
        CHECK(o.source == IL::Source::NoSent);
    }
    SECTION("lag 1, the one sent no longer kept: the newest") {
        for (int i = 0; i < 10; i++) {
            r.Release(400 + i);
        }
        const IL::OnView o = IL::PictureOnView(r.pics, 1, 1);
        CHECK(o.picture.serial == r.serial);
        CHECK(o.source == IL::Source::Gone);
    }
    SECTION("a gap in the serials is no matter") {
        Released g;
        g.Release(100);
        g.serial += 3; // releases not kept (another session's), a gap
        g.Release(500);
        const IL::OnView o = IL::PictureOnView(g.pics, 1, 1);
        CHECK(o.picture.serial == 1);
        CHECK(o.picture.content.time == 100);
    }
    SECTION("a picture without a pose is on view as it is (never its neighbour's pose)") {
        Released v;
        v.Release(100);
        v.Release(0, false); // drawn outside the Full modes (a menu)
        v.Release(300);
        const IL::OnView o = IL::PictureOnView(v.pics, 2, 1);
        CHECK(o.picture.serial == 2);
        CHECK_FALSE(o.picture.content.valid);
        CHECK_FALSE(IL::AnchorUsable(o.picture.content, 400, 0, 250.0));
    }
}

TEST_CASE("Image lag: the anchor's limits judged on the picture on view", "[frame_xr][image_lag]") {
    const int64_t ms = 1'000'000;
    Released r;
    r.Release(0);        // serial 1, drawn at 0
    r.Release(260 * ms); // serial 2, fresh
    const int64_t display = 270 * ms;
    // The newest is 10 ms old, but the picture on view (sent, serial 1) is 270 ms old: rigid.
    const IL::OnView o = IL::PictureOnView(r.pics, 1, 1);
    CHECK(o.picture.serial == 1);
    CHECK_FALSE(IL::AnchorUsable(o.picture.content, display, 0, 250.0));
    CHECK(IL::AnchorUsable(r.pics[0].content, display, 0, 250.0));
    // The other way round: a fresh picture on view while a stale one is the newest cannot
    // happen (serials only grow), but the age of the one on view is what counts.
    CHECK(IL::AnchorUsable(o.picture.content, 200 * ms, 0, 250.0));
    // A recentre after the picture on view was drawn: not anchored until a picture drawn after it
    // is on view, even when the newest is.
    const int64_t recentre = 100 * ms;
    CHECK_FALSE(IL::AnchorUsable(o.picture.content, display, recentre, 250.0));
    CHECK(IL::AnchorUsable(r.pics[0].content, display, recentre, 250.0));
    CHECK(IL::PictureOnView(r.pics, 2, 1).picture.serial == 2);
    // Not more than 100 ms ahead of the display.
    CHECK_FALSE(IL::AnchorUsable(r.pics[0].content, 150 * ms, 0, 250.0));
}

TEST_CASE("Image lag: the held quad's pose ring is keyed by the picture on view",
          "[frame_xr][image_lag]") {
    namespace L = FrameXr::Immersive::Layout;
    L::PoseRing ring;
    ring.Keep(3, {});
    Released r;
    for (int k = 1; k <= 6; k++) {
        L::HeldPose p;
        p.time = k * kPeriod;
        p.q_cam = {0.0f, static_cast<float>(k), 0.0f, 1.0f};
        ring.Push(p);
    }
    r.Release(2 * kPeriod); // serial 1, drawn with the pose of frame 2
    r.Release(4 * kPeriod); // serial 2, with frame 4's
    const IL::OnView on = IL::PictureOnView(r.pics, 1, 1);
    const L::HeldPose* held = ring.Find(on.picture.content.time);
    REQUIRE(held != nullptr);
    CHECK(held->time == 2 * kPeriod); // the picture on view's camera, not the newest's
    CHECK(held->q_cam.y == 2.0f);
    const IL::OnView now = IL::PictureOnView(r.pics, 1, 0);
    CHECK(ring.Find(now.picture.content.time)->time == 4 * kPeriod);
}

TEST_CASE("Image lag: every refresh anchored to the picture the assumed compositor shows",
          "[frame_xr][image_lag]") {
    // Cadences: 60 pictures a second (2 refreshes each), 30 (4), 40 (3), and random ones with two
    // releases inside a refresh (a picture never on view), refreshes without a release, hidden
    // frames (SINGLE_FLASH, the bottom screen in focus) and pictures without a pose.
    for (const uint32_t comp_lag : {0u, 1u}) {
        for (const int every : {2, 3, 4}) {
            Released r;
            Compositor c{comp_lag};
            uint64_t sent = 0;
            Run run;
            for (int k = 0; k < 600; k++) {
                if (k % every == 0) {
                    r.Release(k * kPeriod);
                }
                Refresh(r, c, sent, comp_lag, false, run);
            }
            INFO("compositor lag " << comp_lag << ", a picture every " << every);
            CHECK(run.mispaired == 0);
            CHECK(run.shown > 590);
        }
        std::mt19937 rng(1234 + comp_lag);
        for (int trial = 0; trial < 20; trial++) {
            Released r;
            Compositor c{comp_lag};
            uint64_t sent = 0;
            Run run;
            for (int k = 0; k < 2000; k++) {
                const uint32_t roll = rng() % 100;
                const int releases = roll < 10 ? 2 : roll < 55 ? 1 : 0; // 2: one never on view
                for (int i = 0; i < releases; i++) {
                    r.Release(k * kPeriod, rng() % 50 != 0);
                }
                const bool hidden = rng() % 20 == 0;
                Refresh(r, c, sent, comp_lag, hidden, run);
            }
            INFO("compositor lag " << comp_lag << ", random cadence " << trial);
            CHECK(run.mispaired == 0);
        }
    }
}

TEST_CASE("Image lag: the wrong lag mispairs (the check can fail)", "[frame_xr][image_lag]") {
    // Lag 0 on a compositor that shows images a refresh late: every new picture's first refresh
    // shows the one before with the new one's anchor (the doubling on head turns).
    for (const auto [app, comp] : {std::pair{0u, 1u}, std::pair{1u, 0u}}) {
        Released r;
        Compositor c{comp};
        uint64_t sent = 0;
        Run run;
        for (int k = 0; k < 120; k++) {
            if (k % 2 == 0) {
                r.Release(k * kPeriod);
            }
            Refresh(r, c, sent, app, false, run);
        }
        INFO("app lag " << app << ", compositor lag " << comp);
        CHECK(run.mispaired >= 55); // one refresh of each of the 60 pictures
    }
}

TEST_CASE("Image lag: the counts of fallbacks and missed refreshes", "[frame_xr][image_lag]") {
    // Why nothing was sent, in the order the frame loop can tell.
    CHECK(IL::WhyNoneSent(false, true) == IL::NoneSent::NotRendered);
    CHECK(IL::WhyNoneSent(false, false) == IL::NoneSent::NotRendered);
    CHECK(IL::WhyNoneSent(true, false) == IL::NoneSent::EndFailed);
    CHECK(IL::WhyNoneSent(true, true) == IL::NoneSent::NoTopLayer);
    // Refreshes the loop missed: none without a frame before, under 1.5 periods, or going back.
    CHECK(IL::MissedRefreshes(0, 5 * kPeriod, kPeriod) == 0);
    CHECK(IL::MissedRefreshes(kPeriod, 2 * kPeriod, kPeriod) == 0);
    CHECK(IL::MissedRefreshes(kPeriod, kPeriod + kPeriod * 14 / 10, kPeriod) == 0);
    CHECK(IL::MissedRefreshes(kPeriod, 3 * kPeriod, kPeriod) == 1);
    CHECK(IL::MissedRefreshes(kPeriod, 4 * kPeriod + 100'000, kPeriod) == 2);
    CHECK(IL::MissedRefreshes(5 * kPeriod, 4 * kPeriod, kPeriod) == 0);
    CHECK(IL::MissedRefreshes(kPeriod, 3 * kPeriod, 0) == 0);
    // Lag 1 holds for every SteamVR version (no warning); only another runtime or none known is
    // warned about in the log.
    CHECK(IL::SteamVrRuntime("SteamVR/OpenXR 2.19.3"));
    CHECK(IL::SteamVrRuntime("SteamVR/OpenXR 2.19.4"));
    CHECK(IL::SteamVrRuntime("SteamVR/OpenXR 3.0.0"));
    CHECK_FALSE(IL::SteamVrRuntime("xrmock 0.1.0"));
    CHECK_FALSE(IL::SteamVrRuntime("Monado 25.0.0"));
    CHECK_FALSE(IL::SteamVrRuntime(""));
}
