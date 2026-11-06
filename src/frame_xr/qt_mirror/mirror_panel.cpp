// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
#include <algorithm>
#include <cmath>

#include "common/logging/log.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/qt_mirror/mirror_panel.h"

namespace FrameXr {

namespace {

// 1.6 m wide, 2 m ahead at eye height, like the picker; the height follows the image.
constexpr float kPanelWidth = 1.6f;
const XrPosef kDefaultPose{{0, 0, 0, 1}, {0, 0, -2}};
// A press keeps its point while the ray stays within this many metres of it on the panel
// (1.2 cm, about 9 window pixels at the default size), and a second press within
// kDoubleClickNs of the release lands on the same point: double clicks work.
constexpr float kSlop = 0.012f;
constexpr XrTime kDoubleClickNs = 450'000'000;
// Stick Y scrolls: nothing below kWheelDead, up to kWheelRate notches a second at full tilt.
constexpr float kWheelDead = 0.2f;
constexpr float kWheelRate = 12.0f;

Input::Posef ToInput(const XrPosef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef ToXr(const Input::Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

bool Hysteresis(bool down, float value, float on, float off) {
    return down ? value > off : value > on;
}

} // namespace

void MirrorPanel::SetHandler(std::function<void(const HomePanelEvent&)> new_handler) {
    std::scoped_lock lock{mutex};
    handler = std::move(new_handler);
}

void MirrorPanel::SetImage(std::shared_ptr<const HomePanelImage> new_image) {
    std::scoped_lock lock{mutex};
    image = std::move(new_image);
}

bool MirrorPanel::Enabled() const {
    std::scoped_lock lock{mutex};
    return static_cast<bool>(handler);
}

void MirrorPanel::Send(const HomePanelEvent& e) {
    std::function<void(const HomePanelEvent&)> h;
    {
        std::scoped_lock lock{mutex};
        h = handler;
    }
    if (h) {
        h(e);
    }
}

void MirrorPanel::Hide() {
    if (!shown) {
        return;
    }
    shown = false;
    if (left_down) {
        Send({HomePanelEvent::Type::Release, HomePanelEvent::Button::Left, last_u, last_v});
        left_down = false;
    }
    if (right_down) {
        Send({HomePanelEvent::Type::Release, HomePanelEvent::Button::Right, last_u, last_v});
        right_down = false;
    }
    if (pointer_in) {
        Send({HomePanelEvent::Type::Leave});
        pointer_in = false;
    }
    sticky = false;
    drag_hand = -1;
    wheel_acc = 0.0f;
    last_time = 0;
}

void MirrorPanel::Destroy(OverlayUploader& uploader) {
    uploader.DestroyChain(chain);
    uploaded.reset();
}

void MirrorPanel::Frame(XrSession session, VkFormat format, OverlayUploader& uploader,
                        const FrameInfo& info, const Input::InputFrame& in, bool have_input,
                        std::vector<Quad>& layers, std::optional<XrPosef>& cursor) {
    std::shared_ptr<const HomePanelImage> img;
    {
        std::scoped_lock lock{mutex};
        img = image;
    }
    if (!info.should_render) {
        return;
    }
    if (img && img != uploaded && img->width > 0 && img->height > 0 &&
        img->rgba.size() >= std::size_t{img->width} * img->height * 4) {
        if (chain.handle && (chain.width != img->width || chain.height != img->height)) {
            uploader.DestroyChain(chain);
        }
        if (!chain.handle) {
            if (uploader.CreateChain(session, format, img->width, img->height, chain)) {
                LOG_INFO(Frontend, "xr: home panel {}x{}", img->width, img->height);
            }
        }
        if (chain.handle && uploader.Upload(chain, img->rgba.data())) {
            uploaded = img;
            width_m = kPanelWidth;
            height_m = kPanelWidth * static_cast<float>(img->height) / img->width;
        }
    }
    if (!chain.handle || !chain.has_image) {
        return;
    }
    if (!shown) {
        shown = true;
        // Nothing from a button already held when the panel appears.
        prev_a = prev_b = prev_x = prev_reset = true;
        for (int h = 0; h < Input::kNumHands; h++) {
            trigger_down[h] = in.hand[h].trigger > 0.40f;
            grip_down[h] = in.hand[h].grip > 0.40f;
        }
    }
    cursor_pose.reset();
    if (have_input) {
        Pointer(info, in);
    }
    last_time = info.display_time;

    Quad q;
    q.swapchain = chain.handle;
    q.image_rect = {{0, 0}, {static_cast<int32_t>(chain.width), static_cast<int32_t>(chain.height)}};
    q.pose = pose;
    q.size = {width_m, height_m};
    layers.push_back(q);
    cursor = cursor_pose;
}

void MirrorPanel::Pointer(const FrameInfo& info, const Input::InputFrame& in) {
    using Type = HomePanelEvent::Type;
    using Button = HomePanelEvent::Button;
    const float dt =
        last_time ? std::min(0.1f, static_cast<float>(info.display_time - last_time) * 1e-9f)
                  : 0.0f;
    const Input::PanelQuad quad{ToInput(pose), width_m, height_m, true};
    Input::Ray ray[2]{};
    Input::QuadHit hit[2]{};
    bool trigger_edge[2]{}, grip_edge[2]{};
    for (int h = 0; h < Input::kNumHands; h++) {
        const Input::HandInput& hand = in.hand[h];
        if (hand.active) {
            ray[h] = Input::RayFromPose(hand.aim);
            hit[h] = Input::IntersectQuad(ray[h], quad);
        }
        const bool t = Hysteresis(trigger_down[h], hand.trigger, 0.55f, 0.40f);
        const bool g = Hysteresis(grip_down[h], hand.grip, 0.60f, 0.40f);
        trigger_edge[h] = t && !trigger_down[h];
        grip_edge[h] = g && !grip_down[h];
        trigger_down[h] = t;
        grip_down[h] = g;
        // The hand that pulls its trigger (or grip) on the panel points from now on.
        if ((trigger_edge[h] || grip_edge[h]) && hit[h].in_bounds && !left_down && !right_down) {
            active_hand = h;
        }
    }

    // Bumper on the panel: drag it around you (it keeps its distance and faces you).
    if (drag_hand >= 0 && (!in.hand[drag_hand].active || !in.hand[drag_hand].bumper)) {
        drag_hand = -1;
    }
    if (drag_hand < 0 && !left_down && !right_down) {
        for (int h : {active_hand, 1 - active_hand}) {
            if (in.hand[h].bumper && !bumper_prev[h] && hit[h].in_bounds) {
                drag_hand = h;
                drag_t = hit[h].t;
                drag_offset = ToInput(pose).position - hit[h].point_world;
                break;
            }
        }
    }
    for (int h = 0; h < Input::kNumHands; h++) {
        bumper_prev[h] = in.hand[h].bumper;
    }
    if (drag_hand >= 0) {
        const Input::Vec3f grabbed = ray[drag_hand].origin + ray[drag_hand].direction * drag_t;
        Input::DragLimits limits;
        limits.max_z = -0.5f;
        if (auto moved = Input::SolveSphereDrag(ToInput(pose), grabbed + drag_offset, {0, 0, 0},
                                                nullptr, limits)) {
            pose = ToXr(*moved);
        }
    }
    // Right stick click: the panel back in front.
    const bool reset = in.hand[1].stick_click;
    if (reset && !prev_reset) {
        pose = kDefaultPose;
        drag_hand = -1;
    }
    prev_reset = reset;

    // The pointing hand: the active one, else the other one if only it is on the panel.
    // A held button keeps its hand (and the pointer clamps to the edge off the panel).
    int p = -1;
    if (left_down || right_down) {
        p = active_hand;
    } else if (drag_hand < 0) {
        if (hit[active_hand].in_bounds) {
            p = active_hand;
        } else if (hit[1 - active_hand].in_bounds) {
            p = 1 - active_hand;
        }
    }
    const bool held = left_down || right_down;
    const bool on = p >= 0 && (hit[p].in_bounds || (held && hit[p].on_plane));
    float u = last_u, v = last_v;
    if (on) {
        u = std::clamp(hit[p].u, 0.0f, 1.0f);
        v = std::clamp(hit[p].v, 0.0f, 1.0f);
        if (hit[p].in_bounds) {
            cursor_pose = XrPosef{pose.orientation,
                                  {hit[p].point_world.x, hit[p].point_world.y,
                                   hit[p].point_world.z}};
        }
        if (sticky) {
            const float dist = std::hypot((u - sticky_u) * width_m, (v - sticky_v) * height_m);
            if (dist < kSlop && (held || info.display_time < sticky_until)) {
                u = sticky_u;
                v = sticky_v;
            } else {
                sticky = false;
            }
        }
        if (!pointer_in || u != last_u || v != last_v) {
            Send({Type::Move, Button::Left, u, v});
            pointer_in = true;
            last_u = u;
            last_v = v;
        }
    } else if (pointer_in && !held) {
        Send({Type::Leave});
        pointer_in = false;
        sticky = false;
    }

    // Buttons: trigger or A = left, grip or X = right. A press needs the pointer on the panel.
    const bool a_edge = in.a && !prev_a;
    const bool x_edge = in.x && !prev_x;
    prev_a = in.a;
    prev_x = in.x;
    const bool left_press = (p >= 0 && trigger_edge[p]) || a_edge;
    const bool right_press = (p >= 0 && grip_edge[p]) || x_edge;
    const bool left_hold = (p >= 0 && trigger_down[p]) || in.a;
    const bool right_hold = (p >= 0 && grip_down[p]) || in.x;
    const auto press = [&](Button b) {
        if (!sticky) {
            sticky = true;
            sticky_u = u;
            sticky_v = v;
        }
        Send({Type::Press, b, u, v});
    };
    if (left_down && !left_hold) {
        Send({Type::Release, Button::Left, u, v});
        left_down = false;
        sticky_until = info.display_time + kDoubleClickNs;
    }
    if (right_down && !right_hold) {
        Send({Type::Release, Button::Right, u, v});
        right_down = false;
        sticky_until = info.display_time + kDoubleClickNs;
    }
    if (on && hit[p].in_bounds && left_press && !left_down && !right_down) {
        press(Button::Left);
        left_down = true;
    } else if (on && hit[p].in_bounds && right_press && !right_down && !left_down) {
        press(Button::Right);
        right_down = true;
    }

    // B: Escape (closes a menu or a dialog).
    if (in.b && !prev_b) {
        Send({Type::Escape});
    }
    prev_b = in.b;

    // Stick Y (whichever stick is pushed further): the wheel, while on the panel.
    float y = in.hand[0].stick.y;
    if (std::abs(in.hand[1].stick.y) > std::abs(y)) {
        y = in.hand[1].stick.y;
    }
    if (on && !held && std::abs(y) > kWheelDead) {
        const float k = (std::abs(y) - kWheelDead) / (1.0f - kWheelDead);
        wheel_acc += std::copysign(kWheelRate * k * k, y) * dt;
        if (std::abs(wheel_acc) >= 0.25f) {
            HomePanelEvent e{Type::Wheel, Button::Left, u, v};
            e.wheel = wheel_acc;
            Send(e);
            wheel_acc = 0.0f;
        }
    } else {
        wheel_acc = 0.0f;
    }
}

} // namespace FrameXr
