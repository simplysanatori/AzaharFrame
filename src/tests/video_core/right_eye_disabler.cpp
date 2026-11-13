// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame feshimmer: the right eye disabler is for the flat screen only.

#include <catch2/catch_test_macros.hpp>
#include "video_core/right_eye_disabler.h"

using VideoCore::RightEyeDisabler;

TEST_CASE("RightEyeDisabler never runs while an XR session exists", "[video_core][frame_xr]") {
    // In VR the right eye is the 3DS's own right image: both eyes always render.
    REQUIRE_FALSE(RightEyeDisabler::WantedForFrame(true, true));
    REQUIRE_FALSE(RightEyeDisabler::WantedForFrame(false, true));
    // On the flat screen the setting decides, as upstream.
    REQUIRE(RightEyeDisabler::WantedForFrame(true, false));
    REQUIRE_FALSE(RightEyeDisabler::WantedForFrame(false, false));
}
