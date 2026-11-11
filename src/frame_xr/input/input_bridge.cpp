// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "common/settings.h"
#include "frame_xr/input/input_bridge.h"
#include "input_common/frame_xr.h"
#include "input_common/main.h"

// AzaharFrame
namespace FrameXr::Input {

InputBridge::InputBridge(const MapperConfig& config) : mapper_(config) {}

const MapperOutput& InputBridge::Process(const InputFrame& frame, const PanelGeometry& panels) {
    mapper_.SetFaceButtonsByPosition(Settings::values.vr_face_buttons.GetValue() == 0);
    last_ = mapper_.Update(frame, panels);
    InputCommon::FrameXr::GetPad().SetControls(last_.controls);
    {
        // 0 = off, 1 = right controller, 2 = left controller, 3 = headset
        const u32 choice = Settings::values.vr_motion_controls.GetValue();
        const Hand hand = choice == 2 ? Hand::Left : Hand::Right;
        const HandInput source = choice == 3
                                     ? HeadAsMotionSource(frame)
                                     : frame.hand[static_cast<std::size_t>(HandIndex(hand))];
        const MotionSample motion = motion_.Update(source, frame.time_ns, choice != 0);
        InputCommon::FrameXr::GetMotion().Set(
            Common::Vec3f{motion.accel.x, motion.accel.y, motion.accel.z},
            Common::Vec3f{motion.gyro.x, motion.gyro.y, motion.gyro.z});
    }
    // AzaharFrame touchpad: the beam's touch and the touchpad's (or the mouse's) share the sink;
    // the last one to touch wins (touchpad_touch.h). With Touchpad Touch off the beam's events
    // pass through as they were.
    TouchpadFrame touch;
    touch.mode = TouchpadModeFrom(Settings::values.vr_touchpad_touch.GetValue());
    touch.bottom_visible = panels.bottom.visible;
    touch.time_ns = frame.time_ns;
    touch.beam = last_.touch_events.data();
    touch.beam_count = last_.touch_event_count;
    if (touch.mode != TouchpadMode::Off) {
        InputCommon::TouchpadSample pad;
        if (InputCommon::GetSDLTouchpad(pad)) {
            touch.pad = {pad.present, pad.contact, pad.click, pad.x, pad.y};
        }
        const auto mouse = InputCommon::FrameXr::GetMouseState();
        touch.mouse = {mouse.inside, mouse.down, mouse.x, mouse.y};
    }
    const TouchpadResult result = touchpad_.Update(touch);
    touchpad_cursor_ = result.cursor;
    for (int i = 0; i < result.event_count; ++i) {
        const TouchEvent& event = result.events[static_cast<std::size_t>(i)];
        switch (event.type) {
        case TouchEventType::Pressed:
            InputCommon::FrameXr::SendTouchPressed(event.x, event.y);
            break;
        case TouchEventType::Moved:
            InputCommon::FrameXr::SendTouchMoved(event.x, event.y);
            break;
        case TouchEventType::Released:
            InputCommon::FrameXr::SendTouchReleased();
            break;
        }
    }
    return last_;
}

void InputBridge::Release() {
    const bool mapper_touching = mapper_.Reset();
    if (touchpad_.Reset() || mapper_touching) { // AzaharFrame touchpad
        InputCommon::FrameXr::SendTouchReleased();
    }
    touchpad_cursor_ = {};
    InputCommon::FrameXr::GetPad().ReleaseAll();
    motion_.Reset();
    InputCommon::FrameXr::GetMotion().Reset();
    last_ = MapperOutput{};
}

} // namespace FrameXr::Input
