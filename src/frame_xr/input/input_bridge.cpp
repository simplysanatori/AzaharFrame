// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "common/settings.h"
#include "frame_xr/input/devices.h" // AzaharFrame pads
#include "frame_xr/input/input_bridge.h"
#include "input_common/frame_xr.h"
#include "input_common/main.h"

// AzaharFrame
namespace FrameXr::Input {

InputBridge::InputBridge(const MapperConfig& config) : mapper_(config) {}

const MapperOutput& InputBridge::Process(const InputFrame& frame, const PanelGeometry& panels,
                                         const InputFrame* gamepads, const MouseFrame* mouse) {
    const bool by_position = Settings::values.vr_face_buttons.GetValue() == 0;
    mapper_.SetFaceButtonsByPosition(by_position);
    last_ = mapper_.Update(frame, panels);
    // AzaharFrame pads: the other gamepads join the virtual pad (through the controls profile's
    // routes), and give Touchpad Touch its touchpad.
    const TouchpadMode touch_mode = TouchpadModeFrom(Settings::values.vr_touchpad_touch.GetValue());
    GamepadHub::MergeResult pads;
    const bool mouse_here = mouse && mouse->present; // AzaharFrame padsui
    if (gamepads || touch_mode != TouchpadMode::Off || mouse_here) {
        GamepadHub& hub = Devices::Hub();
        hub.SetFaceButtonsByPosition(by_position);
        pads = hub.Merge(gamepads ? *gamepads : frame, InputCommon::FrameXr::GetProfileRoutes(),
                         last_.controls, gamepads != nullptr);
    }
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
    touch.mode = touch_mode;
    touch.bottom_visible = panels.bottom.visible;
    touch.time_ns = frame.time_ns;
    touch.beam = last_.touch_events.data();
    touch.beam_count = last_.touch_event_count;
    if (touch.mode != TouchpadMode::Off) {
        touch.pad = pads.touchpad.pad; // AzaharFrame pads: the chosen pad's, or the touched one's
        touch.pad_recentre = pads.touchpad.recentre;
        // The pointer over the window (a touchpad as a mouse, Steam Input's); not while a real
        // mouse is read directly (AzaharFrame padsui), whose clicks the window also gets.
        if (!mouse_here) {
            const auto window_mouse = InputCommon::FrameXr::GetMouseState();
            touch.mouse = {window_mouse.inside, window_mouse.down, window_mouse.x, window_mouse.y};
        }
    }
    // AzaharFrame padsui: the mouse in Bottom Screen mode, a game running and the menu closed.
    if (mouse_here && gamepads && mouse->mode == MouseMode::BottomScreen) {
        touch.cursor = {true, mouse->motion, MouseScreenGain(mouse->speed), mouse->Touch(),
                        mouse->recentre};
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
    Devices::Hub().Release(); // AzaharFrame pads: what is held stays away until let go
    motion_.Reset();
    InputCommon::FrameXr::GetMotion().Reset();
    last_ = MapperOutput{};
}

} // namespace FrameXr::Input
