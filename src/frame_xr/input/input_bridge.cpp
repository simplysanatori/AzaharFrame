// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/input/input_bridge.h"
#include "input_common/frame_xr.h"

// AzaharFrame
namespace FrameXr::Input {

InputBridge::InputBridge(const MapperConfig& config) : mapper_(config) {}

const MapperOutput& InputBridge::Process(const InputFrame& frame, const PanelGeometry& panels) {
    last_ = mapper_.Update(frame, panels);
    InputCommon::FrameXr::GetPad().SetControls(last_.controls);
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
    last_ = MapperOutput{};
}

} // namespace FrameXr::Input
