// Copyright 2026 CitraFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "frame_xr/input/mapper.h"

// CitraFrame
namespace FrameXr::Input {

namespace {

bool Hysteresis(bool was_down, float value, float press, float release) {
    return was_down ? value > release : value >= press;
}

struct PanelHit {
    Panel panel = Panel::None;
    QuadHit hit;
};

/// The nearest panel the ray enters inside its rectangle.
PanelHit NearestInBounds(const Ray& ray, const PanelGeometry& panels) {
    PanelHit best;
    const QuadHit top = IntersectQuad(ray, panels.top);
    const QuadHit bottom = IntersectQuad(ray, panels.bottom);
    if (top.in_bounds) {
        best = {Panel::Top, top};
    }
    if (bottom.in_bounds && (best.panel == Panel::None || bottom.t < best.hit.t)) {
        best = {Panel::Bottom, bottom};
    }
    return best;
}

float Clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

} // namespace

Mapper::Mapper(const MapperConfig& config) : config_(config) {}

bool Mapper::Reset() {
    const bool was_touching = touch_active_;
    hands_ = {};
    view_down_ = false;
    stop_fired_ = false;
    stick_click_r_prev_ = false;
    touch_active_ = false;
    drag_active_ = false;
    have_last_time_ = false;
    return was_touching;
}

Vec2f Mapper::ApplyStickDeadzone(Vec2f stick) const {
    const float magnitude = std::sqrt(stick.x * stick.x + stick.y * stick.y);
    if (magnitude <= config_.stick_deadzone) {
        return {};
    }
    const float clamped = std::min(magnitude, 1.0f);
    const float scaled = (clamped - config_.stick_deadzone) / (1.0f - config_.stick_deadzone);
    const float k = scaled / magnitude;
    return {stick.x * k, stick.y * k};
}

MapperOutput Mapper::Update(const InputFrame& frame, const PanelGeometry& panels) {
    MapperOutput out;
    ControlState& controls = out.controls;

    float dt = 0.0f;
    if (have_last_time_ && frame.time_ns > last_time_ns_) {
        dt = std::min(static_cast<float>(frame.time_ns - last_time_ns_) * 1e-9f, 0.1f);
    }
    last_time_ns_ = frame.time_ns;
    have_last_time_ = true;

    const auto push_touch = [&out](TouchEventType type, Vec2f px) {
        if (out.touch_event_count < static_cast<int>(out.touch_events.size())) {
            out.touch_events[static_cast<std::size_t>(out.touch_event_count++)] = {type, px.x,
                                                                                    px.y};
        }
    };

    // ---- Plain buttons ----
    controls.Set(Control::A, frame.a);
    controls.Set(Control::B, frame.b);
    controls.Set(Control::X, frame.x);
    controls.Set(Control::Y, frame.y);
    controls.Set(Control::DpadUp, frame.dpad_up);
    controls.Set(Control::DpadDown, frame.dpad_down);
    controls.Set(Control::DpadLeft, frame.dpad_left);
    controls.Set(Control::DpadRight, frame.dpad_right);
    controls.Set(Control::Menu, frame.menu);
    controls.Set(Control::BumperL, frame.hand[0].bumper);
    controls.Set(Control::BumperR, frame.hand[1].bumper);
    controls.Set(Control::StickClickL, frame.hand[0].stick_click);
    controls.Set(Control::StickClickR, frame.hand[1].stick_click);

    // ---- Grips (the 3DS shoulder buttons) ----
    for (int h = 0; h < kNumHands; ++h) {
        HandState& state = hands_[static_cast<std::size_t>(h)];
        state.grip_down = Hysteresis(state.grip_down, frame.hand[static_cast<std::size_t>(h)].grip,
                                     config_.grip_press, config_.grip_release);
    }
    controls.Set(Control::GripL, hands_[0].grip_down);
    controls.Set(Control::GripR, hands_[1].grip_down);

    // ---- Sticks ----
    controls.SetStick(AnalogControl::StickL, ApplyStickDeadzone(frame.hand[0].stick));
    controls.SetStick(AnalogControl::StickR, ApplyStickDeadzone(frame.hand[1].stick));

    // ---- View: Select, or held to stop the game ----
    if (frame.view) {
        if (!view_down_) {
            view_down_ = true;
            view_down_since_ns_ = frame.time_ns;
            stop_fired_ = false;
        }
        const int64_t held = frame.time_ns - view_down_since_ns_;
        if (!stop_fired_) {
            out.commands.stop_hold_progress =
                config_.stop_hold_ns > 0
                    ? Clamp01(static_cast<float>(held) / static_cast<float>(config_.stop_hold_ns))
                    : 1.0f;
            if (held >= config_.stop_hold_ns) {
                out.commands.stop_game = true;
                stop_fired_ = true;
                out.commands.stop_hold_progress = 0.0f;
            }
        }
    } else {
        view_down_ = false;
        stop_fired_ = false;
    }
    // Once the stop fired, the game no longer sees View (it would still be Select-down).
    controls.Set(Control::View, frame.view && !stop_fired_);

    // ---- Right stick click: reset panel positions (once per click) ----
    const bool click_r = frame.hand[1].stick_click;
    if (click_r && !stick_click_r_prev_) {
        out.commands.reset_panels = true;
    }
    stick_click_r_prev_ = click_r;

    // ---- Triggers: ZL/ZR, or touch, or drag, depending on what the ray points at ----
    const Hand order[kNumHands] = {preferred_hand_, OtherHand(preferred_hand_)};
    for (const Hand hand : order) {
        const int h = HandIndex(hand);
        HandState& state = hands_[static_cast<std::size_t>(h)];
        const HandInput& input = frame.hand[static_cast<std::size_t>(h)];
        const bool pressed = input.active && Hysteresis(state.trigger_down, input.trigger,
                                                        config_.trigger_press,
                                                        config_.trigger_release);
        const bool was_pressed = state.trigger_down;
        state.trigger_down = pressed;
        const Ray ray = RayFromPose(input.aim);

        if (pressed && !was_pressed) {
            // The hand that pulled last is the pointing hand.
            preferred_hand_ = hand;
            const PanelHit target = input.active ? NearestInBounds(ray, panels) : PanelHit{};
            if (target.panel == Panel::Bottom && !touch_active_) {
                state.gesture = Gesture::Touch;
                touch_active_ = true;
                touch_last_px_ = HitToPixels(target.hit, kBottomScreenWidthPx,
                                             kBottomScreenHeightPx, true);
                push_touch(TouchEventType::Pressed, touch_last_px_);
            } else if (target.panel == Panel::Top && !drag_active_) {
                state.gesture = Gesture::Drag;
                drag_active_ = true;
                drag_grab_offset_ = target.hit.point_world - panels.top.pose.position;
                drag_last_target_ = panels.top.pose.position;
            } else if (target.panel != Panel::None) {
                state.gesture = Gesture::Blocked; // a second touch or drag: not ZL/ZR either
            } else {
                state.gesture = Gesture::Passthrough;
            }
        } else if (!pressed && was_pressed) {
            if (state.gesture == Gesture::Touch) {
                touch_active_ = false;
                push_touch(TouchEventType::Released, touch_last_px_);
            } else if (state.gesture == Gesture::Drag) {
                drag_active_ = false;
            }
            state.gesture = Gesture::None;
        } else if (!pressed) {
            state.gesture = Gesture::None;
        }

        switch (state.gesture) {
        case Gesture::Passthrough:
            controls.Set(hand == Hand::Left ? Control::TriggerL : Control::TriggerR, true);
            break;
        case Gesture::Touch: {
            const QuadHit hit = IntersectQuad(ray, panels.bottom);
            if (hit.on_plane) {
                const Vec2f px = HitToPixels(hit, kBottomScreenWidthPx, kBottomScreenHeightPx,
                                             true);
                if (std::abs(px.x - touch_last_px_.x) >= config_.touch_move_epsilon_px ||
                    std::abs(px.y - touch_last_px_.y) >= config_.touch_move_epsilon_px) {
                    touch_last_px_ = px;
                    push_touch(TouchEventType::Moved, px);
                }
            }
            break;
        }
        case Gesture::Drag: {
            const QuadHit hit = IntersectQuad(ray, panels.top);
            if (hit.on_plane) {
                drag_last_target_ = hit.point_world - drag_grab_offset_;
            }
            // The dragging hand's stick is the depth control and does not move the game.
            controls.SetStick(hand == Hand::Left ? AnalogControl::StickL : AnalogControl::StickR,
                              {});
            const float stick_y = input.stick.y;
            if (std::abs(stick_y) > config_.depth_threshold) {
                out.commands.top_depth_delta_z = -stick_y * config_.depth_speed_mps * dt;
            }
            break;
        }
        case Gesture::Blocked:
        case Gesture::None:
            break;
        }
    }

    if (drag_active_) {
        out.commands.top_drag_active = true;
        out.commands.top_drag_target = drag_last_target_;
        if (config_.horizontal_lock) {
            out.commands.top_drag_target.x = 0.0f;
        }
    }

    // ---- Cursor ----
    {
        // A hand in a gesture owns the cursor; otherwise the pointing hand, then the other one.
        const Hand* owner = nullptr;
        for (const Hand& hand : order) {
            const Gesture g = hands_[static_cast<std::size_t>(HandIndex(hand))].gesture;
            if (g == Gesture::Touch || g == Gesture::Drag) {
                owner = &hand;
                break;
            }
        }
        const auto fill = [&out](Hand hand, Panel panel, const QuadHit& hit,
                                 const PanelQuad& quad, bool pressed) {
            Cursor& cursor = out.cursor;
            cursor.visible = true;
            cursor.hand = hand;
            cursor.panel = panel;
            cursor.position = hit.point_world;
            cursor.orientation = quad.pose.orientation;
            cursor.scale_m = 0.01f + 0.003f * hit.t;
            cursor.u = Clamp01(hit.u);
            cursor.v = Clamp01(hit.v);
            if (panel == Panel::Bottom) {
                const Vec2f px = HitToPixels(hit, kBottomScreenWidthPx, kBottomScreenHeightPx, true);
                cursor.px = px.x;
                cursor.py = px.y;
            }
            cursor.pressed = pressed;
        };
        if (owner != nullptr) {
            const HandInput& input = frame.hand[static_cast<std::size_t>(HandIndex(*owner))];
            const Gesture g = hands_[static_cast<std::size_t>(HandIndex(*owner))].gesture;
            const Panel panel = g == Gesture::Touch ? Panel::Bottom : Panel::Top;
            const PanelQuad& quad = g == Gesture::Touch ? panels.bottom : panels.top;
            const QuadHit hit = IntersectQuad(RayFromPose(input.aim), quad);
            if (hit.on_plane) {
                fill(*owner, panel, hit, quad, true);
            }
        } else {
            for (const Hand& hand : order) {
                const HandInput& input = frame.hand[static_cast<std::size_t>(HandIndex(hand))];
                if (!input.active) {
                    continue;
                }
                const PanelHit target = NearestInBounds(RayFromPose(input.aim), panels);
                if (target.panel != Panel::None) {
                    fill(hand, target.panel, target.hit,
                         target.panel == Panel::Top ? panels.top : panels.bottom, false);
                    break;
                }
            }
        }
    }

    return out;
}

} // namespace FrameXr::Input
