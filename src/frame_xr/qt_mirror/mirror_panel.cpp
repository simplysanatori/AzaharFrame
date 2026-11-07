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

// ---- The panel's controls (SteamOS-like), all in this one place ----------------------------
//   Trigger of the pointing hand, or A   left click (hold to drag)
//   Right Stick Click, short             right click (context menus)
//   Right Stick Click held kRecentreNs   the panel back where it was placed (in front)
//   B                                    back (Escape: closes a menu or a dialog)
//   Grip on the panel                    move the panel
//   Stick Y (either stick)               scroll
//   Y                                    the headset keyboard (frame_xr/keyboard)
//   X, the bumpers                       nothing
bool LeftButton(const Input::InputFrame& in) {
    return in.a; // and the pointing hand's trigger (with hysteresis, in Pointer)
}
bool BackButton(const Input::InputFrame& in) {
    return in.b;
}
bool RightClickButton(const Input::InputFrame& in) {
    return in.hand[1].stick_click;
}
constexpr bool kTriggerClicks = true;
constexpr bool kGripMoves = true;
constexpr XrTime kRecentreNs = 1'000'000'000;
// A stick click while the stick is pushed over further than this is ignored (as in a game).
constexpr float kClickMaxDeflection = 0.5f;

// A press keeps its point while the ray stays within this many metres of it on the panel
// (1.2 cm, about 8 window pixels on the home panel), and a second press within
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
    if (pointer_in) {
        Send({HomePanelEvent::Type::Leave});
        pointer_in = false;
    }
    sticky = false;
    drag_hand = -1;
    wheel_acc = 0.0f;
    last_time = 0;
}

void MirrorPanel::Place(const XrPosef& new_pose, float new_width) {
    pose = placed_pose = new_pose;
    base_width_m = new_width;
    width_m = base_width_m * panel_scale;
    height_m = width_m * aspect;
    drag_hand = -1;
}

float MirrorPanel::HeightFor(float width) const {
    return width * aspect;
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
            aspect = static_cast<float>(img->height) / img->width;
            panel_scale = std::clamp(img->panel_scale, 1.0f, 3.0f);
            width_m = base_width_m * panel_scale;
            height_m = width_m * aspect;
        }
    }
    if (!chain.handle || !chain.has_image) {
        return;
    }
    if (!shown) {
        shown = true;
        // Nothing from a button already held when the panel appears.
        prev_a = prev_b = prev_stick_click = true;
        stick_click_valid = false;
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
    const XrTime now = info.display_time;
    const float dt =
        last_time ? std::min(0.1f, static_cast<float>(now - last_time) * 1e-9f) : 0.0f;
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
        const bool t = kTriggerClicks && Hysteresis(trigger_down[h], hand.trigger, 0.55f, 0.40f);
        const bool g = kGripMoves && Hysteresis(grip_down[h], hand.grip, 0.60f, 0.40f);
        trigger_edge[h] = t && !trigger_down[h];
        grip_edge[h] = g && !grip_down[h];
        trigger_down[h] = t;
        grip_down[h] = g;
        // The hand that pulls its trigger (or grip) on the panel points from now on.
        if ((trigger_edge[h] || grip_edge[h]) && hit[h].in_bounds && !left_down) {
            active_hand = h;
        }
    }

    // Grip on the panel: move it around you (it keeps its distance and faces you), as the
    // grips move the game screens.
    if (drag_hand >= 0 && (!in.hand[drag_hand].active || !grip_down[drag_hand])) {
        drag_hand = -1;
    }
    if (drag_hand < 0 && !left_down) {
        for (int h : {active_hand, 1 - active_hand}) {
            if (grip_edge[h] && hit[h].in_bounds) {
                drag_hand = h;
                drag_t = hit[h].t;
                drag_offset = ToInput(pose).position - hit[h].point_world;
                break;
            }
        }
    }
    if (drag_hand >= 0) {
        const Input::Vec3f grabbed = ray[drag_hand].origin + ray[drag_hand].direction * drag_t;
        Input::DragLimits limits;
        limits.max_z = -0.3f;
        if (auto moved = Input::SolveSphereDrag(ToInput(pose), grabbed + drag_offset, {0, 0, 0},
                                                nullptr, limits)) {
            pose = ToXr(*moved);
        }
    }

    // The pointing hand: the active one, else the other one if only it is on the panel.
    // A held button keeps its hand (and the pointer clamps to the edge off the panel).
    int p = -1;
    if (left_down) {
        p = active_hand;
    } else if (drag_hand < 0) {
        if (hit[active_hand].in_bounds) {
            p = active_hand;
        } else if (hit[1 - active_hand].in_bounds) {
            p = 1 - active_hand;
        }
    }
    const bool held = left_down;
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
            if (dist < kSlop && (held || now < sticky_until)) {
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
    const bool on_panel = on && hit[p].in_bounds;
    const auto keep_point = [&] {
        if (!sticky) {
            sticky = true;
            sticky_u = u;
            sticky_v = v;
        }
    };

    // Left button: the pointing hand's trigger, or A. A press needs the pointer on the panel.
    const bool a = LeftButton(in);
    const bool a_edge = a && !prev_a;
    prev_a = a;
    const bool left_press = (p >= 0 && trigger_edge[p]) || a_edge;
    const bool left_hold = (p >= 0 && trigger_down[p]) || a;
    if (left_down && !left_hold) {
        Send({Type::Release, Button::Left, u, v});
        left_down = false;
        sticky_until = now + kDoubleClickNs;
    }
    if (on_panel && left_press && !left_down) {
        keep_point();
        Send({Type::Press, Button::Left, u, v});
        left_down = true;
    }

    // Right Stick Click: a short click is a right click (sent when let go), held: the panel back.
    const bool click = RightClickButton(in);
    if (click && !prev_stick_click) {
        stick_click_valid =
            std::hypot(in.hand[1].stick.x, in.hand[1].stick.y) <= kClickMaxDeflection;
        stick_click_since = now;
    }
    if (click && stick_click_valid && now - stick_click_since >= kRecentreNs) {
        stick_click_valid = false;
        pose = placed_pose;
        drag_hand = -1;
    }
    if (!click && prev_stick_click && stick_click_valid) {
        stick_click_valid = false;
        if (on_panel && !left_down) {
            keep_point();
            Send({Type::Press, Button::Right, u, v});
            Send({Type::Release, Button::Right, u, v});
            sticky_until = now + kDoubleClickNs;
        }
    }
    prev_stick_click = click;

    // B: back (Escape closes a menu or a dialog).
    const bool b = BackButton(in);
    if (b && !prev_b) {
        Send({Type::Escape});
    }
    prev_b = b;

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
