// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "frame_xr/input/mapper.h"

// AzaharFrame
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
    grip_was_down_ = {};
    view_down_ = false;
    stop_fired_ = false;
    menu_down_ = false;
    menu_fired_ = false;
    start_pulse_ = false;
    click_l_.Reset();
    click_r_.Reset();
    home_pulse_ = false;
    swallow_view_ = false;
    view_prev_ = false;
    view_held_back_ = false;
    select_pulse_ = false;
    quick_armed_ = false;
    view_quick_wait_ = false;
    bumper_prev_ = {};
    quick_done_ = false;
    swallow_menu_ = false;
    swallow_bumper_ = {};
    touch_active_ = false;
    drag_active_ = false;
    drag_panel_ = Panel::None;
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
    if (config_.face_buttons_by_position) {
        // The Frame has Xbox labels (A bottom, B right, X left, Y top), the 3DS has A right, B
        // bottom, X top, Y left: swap by position. The picker reads frame.a / frame.b itself.
        controls.Set(Control::A, frame.b);
        controls.Set(Control::B, frame.a);
        controls.Set(Control::X, frame.y);
        controls.Set(Control::Y, frame.x);
    } else {
        controls.Set(Control::A, frame.a);
        controls.Set(Control::B, frame.b);
        controls.Set(Control::X, frame.x);
        controls.Set(Control::Y, frame.y);
    }
    controls.Set(Control::DpadUp, frame.dpad_up);
    controls.Set(Control::DpadDown, frame.dpad_down);
    controls.Set(Control::DpadLeft, frame.dpad_left);
    controls.Set(Control::DpadRight, frame.dpad_right);
    for (int h = 0; h < kNumHands; ++h) {
        if (!frame.hand[static_cast<std::size_t>(h)].bumper) {
            swallow_bumper_[static_cast<std::size_t>(h)] = false;
        }
    }

    // ---- Quick Save / Quick Load: View first (no bumper down), then one bumper ----
    // The Left Bumper loads, the Right Bumper saves. Neither bumper, nor View, reaches the game.
    {
        const bool view_pressed_now = frame.view && !view_prev_;
        const bool any_bumper = frame.hand[0].bumper || frame.hand[1].bumper;
        if (!frame.view) {
            if (view_quick_wait_) { // a tap, no bumper came: Select, a little late
                select_pulse_ = true;
                select_pulse_until_ns_ = frame.time_ns + config_.start_pulse_ns;
            }
            quick_armed_ = false;
            view_quick_wait_ = false;
        } else if (view_pressed_now && !any_bumper) {
            quick_armed_ = true;
            view_quick_wait_ = true;
            view_quick_since_ns_ = frame.time_ns;
        }
        const bool edge_l = frame.hand[0].bumper && !bumper_prev_[0];
        const bool edge_r = frame.hand[1].bumper && !bumper_prev_[1];
        if (quick_armed_ && (edge_l || edge_r)) {
            if (edge_l && edge_r) {
                quick_armed_ = false; // both at once: not this gesture
                view_quick_wait_ = false;
            } else {
                const bool cooled = !quick_done_ ||
                                    frame.time_ns - quick_last_ns_ >= config_.quick_cooldown_ns;
                if (cooled) {
                    (edge_l ? out.commands.quick_load : out.commands.quick_save) = true;
                    quick_done_ = true;
                    quick_last_ns_ = frame.time_ns;
                }
                swallow_bumper_[edge_l ? 0 : 1] = true; // until let go
                swallow_view_ = true;                    // until let go: no Select, no stop hold
                view_quick_wait_ = false;
                select_pulse_ = false;
            }
        }
        bumper_prev_ = {frame.hand[0].bumper, frame.hand[1].bumper};
    }
    controls.Set(Control::BumperL, frame.hand[0].bumper && !swallow_bumper_[0]);
    controls.Set(Control::BumperR, frame.hand[1].bumper && !swallow_bumper_[1]);
    controls.Set(Control::StickClickR, frame.hand[1].stick_click); // unbound unless the player binds it

    // ---- Grips: grab a screen (no 3DS button; see the drag below) ----
    for (int h = 0; h < kNumHands; ++h) {
        HandState& state = hands_[static_cast<std::size_t>(h)];
        state.grip_down = Hysteresis(state.grip_down, frame.hand[static_cast<std::size_t>(h)].grip,
                                     config_.grip_press, config_.grip_release);
    }
    // Reported so the Controls dialog can bind them; unbound by default.
    controls.Set(Control::GripL, hands_[0].grip_down);
    controls.Set(Control::GripR, hands_[1].grip_down);

    // ---- Sticks ----
    controls.SetStick(AnalogControl::StickL, ApplyStickDeadzone(frame.hand[0].stick));
    controls.SetStick(AnalogControl::StickR, ApplyStickDeadzone(frame.hand[1].stick));

    // ---- Both bumpers + View + Menu: VR <-> flat. Both bumpers + View: the bottom screen ----
    // View pressed with both bumpers held waits up to chord_grace_ns for Menu (Menu already down
    // counts too): with Menu it is the VR / flat switch, else the bottom screen toggle (games
    // that hide it) or Select (a tap shorter than the wait is sent late, as a pulse).
    const bool bumpers = frame.hand[0].bumper && frame.hand[1].bumper;
    const bool view_edge = frame.view && !view_prev_;
    if (!frame.menu) {
        swallow_menu_ = false;
    }
    if (view_edge && bumpers) {
        view_held_back_ = true;
        view_held_back_since_ns_ = frame.time_ns;
    }
    if (view_held_back_) {
        if (frame.menu && bumpers && frame.view && !swallow_menu_) {
            out.commands.toggle_vr = true;
            view_held_back_ = false;
            swallow_view_ = true;
            swallow_menu_ = true;
            swallow_bumper_ = {true, true};
            controls.Set(Control::BumperL, false); // set above, before the chord was known
            controls.Set(Control::BumperR, false);
            menu_down_ = false; // no Start when Menu is let go, no in-game menu hold
            menu_fired_ = false;
            start_pulse_ = false;
        } else if (!frame.view || !bumpers ||
                   frame.time_ns - view_held_back_since_ns_ >= config_.chord_grace_ns) {
            view_held_back_ = false;
            if (panels.bottom_toggle_enabled) {
                out.commands.toggle_bottom = true;
                swallow_view_ = frame.view;
            } else if (!frame.view) {
                select_pulse_ = true;
                select_pulse_until_ns_ = frame.time_ns + config_.start_pulse_ns;
            }
        }
    }
    if (!frame.view) {
        swallow_view_ = false;
    }
    if (view_quick_wait_) {
        // View pressed with no bumper down: held back while a bumper may still follow.
        if (frame.time_ns - view_quick_since_ns_ >= config_.chord_grace_ns) {
            view_quick_wait_ = false; // a plain View hold: Select from here on
        }
    }
    view_prev_ = frame.view;
    if (select_pulse_ && (frame.time_ns >= select_pulse_until_ns_ || frame.view)) {
        select_pulse_ = false;
    }
    const bool view = frame.view && !swallow_view_ && !view_held_back_ && !view_quick_wait_;
    if (view) {
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
    controls.Set(Control::View, (view && !stop_fired_) || select_pulse_);

    // ---- Menu: Start when tapped (sent as it is let go), held 1 s: the in-game menu ----
    // The game never sees a held Menu, so opening the menu sends no Start.
    if (frame.menu && !swallow_menu_) {
        if (!menu_down_) {
            menu_down_ = true;
            menu_down_since_ns_ = frame.time_ns;
            menu_fired_ = false;
        }
        const int64_t held = frame.time_ns - menu_down_since_ns_;
        if (!menu_fired_) {
            out.commands.menu_hold_progress =
                config_.menu_hold_ns > 0
                    ? Clamp01(static_cast<float>(held) / static_cast<float>(config_.menu_hold_ns))
                    : 1.0f;
            if (held >= config_.menu_hold_ns) {
                out.commands.menu_toggle = true;
                menu_fired_ = true;
                out.commands.menu_hold_progress = 0.0f;
            }
        }
    } else if (menu_down_) {
        menu_down_ = false;
        if (!menu_fired_) {
            start_pulse_ = true;
            start_pulse_until_ns_ = frame.time_ns + config_.start_pulse_ns;
        }
        menu_fired_ = false;
    }
    if (start_pulse_ && (frame.time_ns >= start_pulse_until_ns_ || frame.menu)) {
        start_pulse_ = false;
    }
    controls.Set(Control::Menu, start_pulse_);

    // ---- Stick clicks: a tap acts when it is let go, a hold of 1 s acts once ----
    // Right: tap swaps the screens, hold resets them. Left: tap hides or shows the other screen,
    // hold is Home (a short pulse; the game sees nothing else of the click). A click that starts
    // while its stick is pushed over is ignored, so the free camera cannot trigger it.
    {
        const auto deflection = [](Vec2f stick) {
            return std::sqrt(stick.x * stick.x + stick.y * stick.y);
        };
        const auto left = click_l_.Update(frame.hand[0].stick_click, deflection(frame.hand[0].stick),
                                          frame.time_ns, config_.stick_click_hold_ns,
                                          config_.reset_click_max_deflection);
        const auto right = click_r_.Update(frame.hand[1].stick_click, deflection(frame.hand[1].stick),
                                           frame.time_ns, config_.stick_click_hold_ns,
                                           config_.reset_click_max_deflection);
        out.commands.toggle_other_screen = left == ClickGesture::Result::Tap;
        out.commands.swap_screens = right == ClickGesture::Result::Tap;
        out.commands.reset_panels = right == ClickGesture::Result::Hold;
        if (left == ClickGesture::Result::Hold) {
            home_pulse_ = true;
            home_pulse_until_ns_ = frame.time_ns + config_.start_pulse_ns;
        }
        if (home_pulse_ && frame.time_ns >= home_pulse_until_ns_) {
            home_pulse_ = false;
        }
        controls.Set(Control::StickClickL, home_pulse_);
    }

    // ---- Triggers: ZL/ZR, or touch when the bottom screen is the nearest thing under the ray ----
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
            } else if (target.panel == Panel::Bottom) {
                state.gesture = Gesture::Blocked; // a second touch: not ZL/ZR either
            } else {
                // Over the top screen or over nothing: always ZL/ZR.
                state.gesture = Gesture::Passthrough;
            }
        } else if (!pressed && was_pressed) {
            if (state.gesture == Gesture::Touch) {
                touch_active_ = false;
                push_touch(TouchEventType::Released, touch_last_px_);
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
        case Gesture::Blocked:
        case Gesture::None:
            break;
        }
    }

    // ---- Grips: grab the screen under the ray and move it (one drag at a time) ----
    for (int h = 0; h < kNumHands; ++h) {
        HandState& state = hands_[static_cast<std::size_t>(h)];
        const HandInput& input = frame.hand[static_cast<std::size_t>(h)];
        const Hand hand = h == 0 ? Hand::Left : Hand::Right;
        const Ray ray = RayFromPose(input.aim);

        if (state.dragging && (!state.grip_down || !input.active)) {
            state.dragging = false;
            drag_active_ = false;
            drag_panel_ = Panel::None;
        }
        if (state.grip_down && !state.dragging && !drag_active_ && input.active &&
            !grip_was_down_[static_cast<std::size_t>(h)]) {
            const PanelHit target = NearestInBounds(ray, panels);
            if (target.panel != Panel::None) {
                state.dragging = true;
                drag_active_ = true;
                drag_panel_ = target.panel;
                const PanelQuad& grabbed = target.panel == Panel::Top ? panels.top : panels.bottom;
                drag_grab_offset_ = target.hit.point_world - grabbed.pose.position;
                drag_last_target_ = panels.top.pose.position;
            }
        }
        grip_was_down_[static_cast<std::size_t>(h)] = state.grip_down;

        if (state.dragging) {
            const PanelQuad& grabbed = drag_panel_ == Panel::Top ? panels.top : panels.bottom;
            const QuadHit hit = IntersectQuad(ray, grabbed);
            if (hit.on_plane) {
                const Vec3f wanted = hit.point_world - drag_grab_offset_;
                if (drag_panel_ == Panel::Top) {
                    drag_last_target_ = wanted;
                } else {
                    // The bottom screen follows the top one rigidly: move the top screen by the
                    // same amount the grabbed bottom point should move.
                    drag_last_target_ = {
                        panels.top.pose.position.x + wanted.x - panels.bottom.pose.position.x,
                        panels.top.pose.position.y + wanted.y - panels.bottom.pose.position.y,
                        panels.top.pose.position.z + wanted.z - panels.bottom.pose.position.z};
                }
            }
            // The dragging hand's stick is the depth control and does not move the game.
            controls.SetStick(hand == Hand::Left ? AnalogControl::StickL : AnalogControl::StickR,
                              {});
            const float stick_y = input.stick.y;
            if (std::abs(stick_y) > config_.depth_threshold) {
                out.commands.top_depth_delta_z = -stick_y * config_.depth_speed_mps * dt;
            }
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
        // A hand in a touch or a drag owns the cursor; otherwise the dot only hovers over the
        // bottom screen (the top screen's trigger is ZL/ZR, nothing to aim at there).
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
        const Hand* owner = nullptr;
        Panel owner_panel = Panel::None;
        for (const Hand& hand : order) {
            const HandState& state = hands_[static_cast<std::size_t>(HandIndex(hand))];
            if (state.gesture == Gesture::Touch) {
                owner = &hand;
                owner_panel = Panel::Bottom;
                break;
            }
            if (state.dragging) {
                owner = &hand;
                owner_panel = drag_panel_;
                break;
            }
        }
        if (owner != nullptr) {
            const HandInput& input = frame.hand[static_cast<std::size_t>(HandIndex(*owner))];
            const PanelQuad& quad = owner_panel == Panel::Bottom ? panels.bottom : panels.top;
            const QuadHit hit = IntersectQuad(RayFromPose(input.aim), quad);
            if (hit.on_plane) {
                fill(*owner, owner_panel, hit, quad, true);
            }
        } else {
            for (const Hand& hand : order) {
                const HandInput& input = frame.hand[static_cast<std::size_t>(HandIndex(hand))];
                if (!input.active) {
                    continue;
                }
                const PanelHit target = NearestInBounds(RayFromPose(input.aim), panels);
                if (target.panel == Panel::Bottom) {
                    fill(hand, target.panel, target.hit, panels.bottom, false);
                    break;
                }
            }
        }
    }

    return out;
}

} // namespace FrameXr::Input
