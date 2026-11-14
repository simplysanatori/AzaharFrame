// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash3: a screen's swapchain made anew (a Dynamic Resolution step, Internal
// Resolution changed) has no picture until the renderer releases its first image, 3 to 5
// refreshes later. The screen's layer (and Motion Clarity's backing under it) was left out
// meanwhile, so the compositor's background showed where the screen was: a blink at every step
// (the owner's session 573, report 70: 6 times). Now the old swapchain is kept, with its last
// picture on view, until the new one has one (Runtime::EnsureSwapchain, Runtime::LayerChain); the
// layer list keeps its shape. The image the renderer had waited for ahead of time stays held (a
// release would put that image, drawn 3 pictures earlier, on view) and is released just before
// the old swapchain is destroyed. Pure (unit-tested: src/tests/frame_xr/screen_handover.cpp).

#pragma once

#include <cstdint>

namespace FrameXr::ScreenHandover {

/// Which swapchain a screen's layer shows this refresh.
enum class Use : uint8_t {
    Current, ///< the screen's own swapchain, which has a picture
    Old,     ///< the one it replaced, still with its last picture (the new one has none yet)
    None,    ///< no picture at all (the first swapchain of a session): no layer
};

inline Use Choose(bool current_has_image, bool old_has_image) {
    if (current_has_image) {
        return Use::Current;
    }
    return old_has_image ? Use::Old : Use::None;
}

/// A swapchain being replaced is kept when it has a picture on view; one that never had one
/// (made and replaced before its first release) goes at once.
inline bool KeepOnReplace(bool had_image) {
    return had_image;
}

/// At most this many refreshes (1 s at 120 Hz): a new swapchain that gets no picture (the game
/// paused as it was made) does not keep the old one's memory for long.
inline constexpr uint32_t kKeepRefreshes = 120;

/// Whether the kept swapchain goes now (each refresh, before the layers): the new one has a
/// picture, the screens are off (no game), or it was kept kKeepRefreshes already.
inline bool DropOld(bool current_has_image, bool screens_live, uint32_t refreshes_kept) {
    return current_has_image || !screens_live || refreshes_kept >= kKeepRefreshes;
}

} // namespace FrameXr::ScreenHandover
