// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flat: which controller drives Flat (frame_xr/input/flat_pads.h).

#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/flat_pads.h"

using namespace FrameXr::Input;
using InputCommon::SDLPadState;

namespace {

constexpr int64_t kMs = 1'000'000;

SDLPadState Pad(int id, float left_x = 0.0f, bool a = false) {
    SDLPadState s;
    s.id = id;
    s.axes[0] = left_x;
    s.buttons[0] = a;
    return s;
}

} // namespace

TEST_CASE("FlatPadPicker: none, then the only pad", "[frame_xr]") {
    FlatPadPicker picker;
    REQUIRE(picker.Update({}, 0) == -1);
    REQUIRE(picker.Update({Pad(3)}, 8 * kMs) == 3);
    REQUIRE(picker.Update({Pad(3, 0.5f)}, 16 * kMs) == 3);
    REQUIRE(picker.Update({}, 24 * kMs) == -1); // gone: nothing drives, everything is let go
}

TEST_CASE("FlatPadPicker: a pad that stops reporting is dropped once another moves",
          "[frame_xr]") {
    FlatPadPicker picker;
    int64_t t = 0;
    // Pad 1 drives and holds the stick right; then it freezes (same state every frame).
    REQUIRE(picker.Update({Pad(1, 1.0f)}, t += 8 * kMs) == 1);
    REQUIRE(picker.Update({Pad(1, 1.0f), Pad(2)}, t += 8 * kMs) == 1); // a new pad: no takeover
    for (int i = 0; i < 100; i++) {
        REQUIRE(picker.Update({Pad(1, 1.0f), Pad(2)}, t += 8 * kMs) == 1);
    }
    // Pad 2 moves: it drives, pad 1's held stick is no longer read.
    REQUIRE(picker.Update({Pad(1, 1.0f), Pad(2, 0.0f, true)}, t += 8 * kMs) == 2);
    // Pad 1 unchanged: pad 2 keeps driving.
    REQUIRE(picker.Update({Pad(1, 1.0f), Pad(2)}, t += 8 * kMs) == 2);
    // Pad 1 moves again: back to it.
    REQUIRE(picker.Update({Pad(1, 0.0f), Pad(2)}, t += 8 * kMs) == 1);
}

TEST_CASE("FlatPadPicker: noise is no change", "[frame_xr]") {
    FlatPadPicker picker;
    REQUIRE(picker.Update({Pad(1, 0.5f), Pad(2)}, 8 * kMs) == 1);
    REQUIRE(picker.Update({Pad(1, 0.5f), Pad(2, 0.01f)}, 16 * kMs) == 1);
    REQUIRE(picker.Update({Pad(1, 0.5f), Pad(2, 0.30f)}, 24 * kMs) == 2);
}

TEST_CASE("FlatPadPicker: the pad in use goes away", "[frame_xr]") {
    FlatPadPicker picker;
    int64_t t = 0;
    REQUIRE(picker.Update({Pad(1), Pad(2), Pad(3)}, t += 8 * kMs) == 1); // none moved: the first
    REQUIRE(picker.Update({Pad(1), Pad(2), Pad(3, 0.4f)}, t += 8 * kMs) == 3);
    REQUIRE(picker.Update({Pad(1), Pad(2, 0.0f, true), Pad(3, 0.4f)}, t += 8 * kMs) == 2);
    // Pad 2 gone: the one that changed last before it (3) takes over.
    REQUIRE(picker.Update({Pad(1), Pad(3, 0.4f)}, t += 8 * kMs) == 3);
    // A pad that came back with a new id is new again.
    REQUIRE(picker.Update({Pad(1), Pad(4)}, t += 8 * kMs) == 1);
}
