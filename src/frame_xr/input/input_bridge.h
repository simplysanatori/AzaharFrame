// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include "frame_xr/input/mapper.h"
#include "frame_xr/input/motion.h"
#include "frame_xr/input/touchpad_touch.h"

// AzaharFrame
// Glue between the mapper and the emulator: runs the mapper on an InputFrame, writes the virtual
// pad that the "frame_xr" input engine reads, and sends touches to the touch sink. The panel
// commands and the cursor are returned for the frame loop to apply. XR thread only.
namespace FrameXr::Input {

class InputBridge {
public:
    explicit InputBridge(const MapperConfig& config = {});

    /// One display frame. `panels` is where the screens are now. The returned reference is valid
    /// until the next call.
    const MapperOutput& Process(const InputFrame& frame, const PanelGeometry& panels);

    /// Releases every button, centres the sticks and lifts a touch in progress. Call when the
    /// game ends, the session loses focus, or the picker takes over.
    void Release();

    Mapper& mapper() {
        return mapper_;
    }

    /// AzaharFrame touchpad: the pad's or the mouse's dot of the last Process (never visible
    /// while the beam holds the touch, or with the bottom screen hidden).
    const TouchpadCursor& touchpad_cursor() const {
        return touchpad_cursor_;
    }

private:
    Mapper mapper_;
    MotionTracker motion_;
    TouchpadTouch touchpad_;
    TouchpadCursor touchpad_cursor_;
    MapperOutput last_;
};

} // namespace FrameXr::Input
