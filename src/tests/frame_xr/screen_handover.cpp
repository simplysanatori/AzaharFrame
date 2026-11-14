// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash3: a swapchain made anew keeps the old one on view until it has a picture
// (frame_xr/screen_handover.h).

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/screen_handover.h"

namespace SH = FrameXr::ScreenHandover;

TEST_CASE("Screen handover: the old swapchain shows until the new one has a picture",
          "[frame_xr][flash]") {
    // A Dynamic Resolution step: the new swapchain has no picture for 3-5 refreshes (session
    // 573: the compositor's background showed then, 6 times). The old one with its last picture
    // fills them.
    CHECK(SH::KeepOnReplace(true));
    CHECK(SH::Choose(false, true) == SH::Use::Old);
    CHECK(SH::Choose(true, true) == SH::Use::Current);
    CHECK(SH::Choose(true, false) == SH::Use::Current);
    // The first swapchain of a session (nothing before it): no layer, as before.
    CHECK(SH::Choose(false, false) == SH::Use::None);
    // One replaced before it ever had a picture is not kept (nothing of it to show).
    CHECK_FALSE(SH::KeepOnReplace(false));
    // Dropped as soon as the new one has a picture, or the screens go off, or after a second.
    CHECK_FALSE(SH::DropOld(false, true, 0));
    CHECK_FALSE(SH::DropOld(false, true, SH::kKeepRefreshes - 1));
    CHECK(SH::DropOld(false, true, SH::kKeepRefreshes));
    CHECK(SH::DropOld(true, true, 3));
    CHECK(SH::DropOld(false, false, 3));
    // A step's refreshes: kept 4 refreshes, the 5th has the new picture.
    int shown_old = 0, none = 0;
    uint32_t kept = 0;
    bool have_old = true;
    for (int refresh = 0; refresh < 8; refresh++) {
        const bool new_has = refresh >= 4;
        if (have_old && SH::DropOld(new_has, true, kept)) {
            have_old = false;
        } else if (have_old) {
            kept++;
        }
        const SH::Use u = SH::Choose(new_has, have_old);
        shown_old += u == SH::Use::Old;
        none += u == SH::Use::None;
    }
    CHECK(shown_old == 4);
    CHECK(none == 0); // never a refresh without the screen
}
