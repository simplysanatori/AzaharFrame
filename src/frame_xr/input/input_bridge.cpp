// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "common/settings.h"
#include "frame_xr/input/input_bridge.h"
#include "input_common/frame_xr.h"

// AzaharFrame
namespace FrameXr::Input {

InputBridge::InputBridge(const MapperConfig& config) : mapper_(config) {}

const MapperOutput& InputBridge::Process(const InputFrame& frame, const PanelGeometry& panels) {
    mapper_.SetFaceButtonsByPosition(Settings::values.vr_face_buttons.GetValue() == 0);
    last_ = mapper_.Update(frame, panels);
    InputCommon::FrameXr::GetPad().SetControls(last_.controls);
    {
        // 0 = off, 1 = right controller, 2 = left controller
        const u32 choice = Settings::values.vr_motion_controls.GetValue();
        const Hand hand = choice == 2 ? Hand::Left : Hand::Right;
        const MotionSample motion = motion_.Update(
            frame.hand[static_cast<std::size_t>(HandIndex(hand))], frame.time_ns, choice != 0);
        InputCommon::FrameXr::GetMotion().Set(
            Common::Vec3f{motion.accel.x, motion.accel.y, motion.accel.z},
            Common::Vec3f{motion.gyro.x, motion.gyro.y, motion.gyro.z});
    }
    for (int i = 0; i < last_.touch_event_count; ++i) {
        const TouchEvent& event = last_.touch_events[static_cast<std::size_t>(i)];
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
    if (mapper_.Reset()) {
        InputCommon::FrameXr::SendTouchReleased();
    }
    InputCommon::FrameXr::GetPad().ReleaseAll();
    motion_.Reset();
    InputCommon::FrameXr::GetMotion().Reset();
    last_ = MapperOutput{};
}

} // namespace FrameXr::Input
