// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// See keyboard_panel.h. The sizes, the placement under the window, the move bar, the ring on the
// key under the ray and the press rules follow Moonlight XR's Compact keyboard (xr_ui.c kbPlace,
// kbBarGeometry, kbKeyAt; xr_input.c pressKey, dismissKeyboard; xr_layers.c addKeyboardLayers).

#include "frame_xr/keyboard/keyboard_panel.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>

#include "common/logging/log.h"
#include "frame_xr/keyboard/keyboard.h"

namespace FrameXr {

namespace {

namespace Kb = Keyboard;

// The keyboard is this share of the window's width (Moonlight XR's KB_WIDTH_FRAC of its screen),
// its top edge this share of the window's width under the window's bottom edge (BAR_GAP_FRAC),
// and a little nearer than the window.
constexpr float kWidthShare = 0.55f;
constexpr float kGapShare = 0.035f;
constexpr float kNearerShare = 0.02f;
// The move bar's pill (BAR_WIDTH_FRAC, BAR_HEIGHT_FRAC) and its zone (BAR_HOVER), in widths of
// the window it was laid out for.
constexpr float kPillWidthShare = 0.14f;
constexpr float kPillHeightShare =
    kPillWidthShare * static_cast<float>(Kb::Art::kPillHeight) / Kb::Art::kPillWidth;
constexpr float kPillHover = 2.0f;
// Pushed or pulled by the stick while its bar is held: metres a second at full tilt, and how
// near and far it may go.
constexpr float kPushRate = 1.0f;
constexpr float kNearest = 0.4f;
constexpr float kFarthest = 4.0f;
constexpr float kStickDead = 0.2f;

Input::Posef ToInput(const XrPosef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef ToXr(const Input::Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef Offset(const Input::Posef& base, const Input::Vec3f& local) {
    Input::Posef p = base;
    p.position = base.position + Input::Rotate(base.orientation, local);
    return ToXr(p);
}

bool Hysteresis(bool down, float value, float on, float off) {
    return down ? value > off : value > on;
}

// The outside's handle on the keyboard (keyboard.h): set from any thread, read on the XR thread.
struct Inbox {
    enum class Request { None, Open, Lost };
    std::mutex mutex;
    std::function<void(const KeyboardEvent&)> handler;
    Request request = Request::None; // the newest one wins
};

Inbox& TheInbox() {
    static Inbox inbox;
    return inbox;
}

} // namespace

void SetKeyboardHandler(std::function<void(const KeyboardEvent&)> on_key) {
    Inbox& inbox = TheInbox();
    std::scoped_lock lock{inbox.mutex};
    inbox.handler = std::move(on_key);
}

void OpenKeyboardForTextField() {
    Inbox& inbox = TheInbox();
    std::scoped_lock lock{inbox.mutex};
    inbox.request = Inbox::Request::Open;
}

void TextFieldFocusLost() {
    Inbox& inbox = TheInbox();
    std::scoped_lock lock{inbox.mutex};
    inbox.request = Inbox::Request::Lost;
}

KeyboardPanel::KeyboardPanel() {
    offset = {0.0f, -(kGapShare + kWidthShare * Kb::kTexHeight / Kb::kTexWidth * 0.5f),
              kNearerShare};
}

KeyboardPanel::~KeyboardPanel() = default;

void KeyboardPanel::Destroy(OverlayUploader& uploader) {
    uploader.DestroyChain(key_chain);
    uploader.DestroyChain(ring_chain);
    uploader.DestroyChain(pill_chain);
    art_dirty = true;
}

void KeyboardPanel::Hide() {
    if (!shown) {
        return;
    }
    shown = false;
    if (open) {
        Close("the window went away");
    }
    Inbox& inbox = TheInbox();
    std::scoped_lock lock{inbox.mutex};
    inbox.request = Inbox::Request::None; // a text field of a window that is gone
}

void KeyboardPanel::Open(bool by_text_field) {
    if (open) {
        return;
    }
    open = true;
    opened_by_text_field = by_text_field;
    // Always comes up on its small letters, so the first key is where the eye expects it.
    sheet = Kb::kSheetLower;
    mods = 0;
    sticky.Reset();
    sticky_source = -1;
    hover_key = -1;
    key_down = false;
    drag_hand = -1;
    for (int h = 0; h < Input::kNumHands; h++) {
        trigger_ours[h] = grip_ours[h] = false;
    }
    a_ours = false;
    LOG_INFO(Frontend, "xr: keyboard open ({})", by_text_field ? "a text field" : "Y");
}

void KeyboardPanel::Close(const char* why) {
    if (!open) {
        return;
    }
    open = false;
    opened_by_text_field = false;
    mods = 0;
    sticky.Reset();
    sticky_source = -1;
    hover_key = -1;
    key_down = false;
    drag_hand = -1;
    bar_hot = false;
    LOG_INFO(Frontend, "xr: keyboard closed ({})", why);
}

void KeyboardPanel::Place(const Window& window) {
    const Input::Posef w = ToInput(window.pose);
    width_m = kWidthShare * window.width_m;
    height_m = width_m * Kb::kTexHeight / Kb::kTexWidth;
    const Input::Vec3f bottom{0.0f, -window.height_m * 0.5f, 0.0f};
    pose.position = w.position + Input::Rotate(w.orientation, bottom + offset * window.width_m);
    pose.orientation = Input::Normalized(Input::Multiply(w.orientation, turn));
}

void KeyboardPanel::Keep(const Window& window) {
    // Where it was moved to, against the window, for the frames after (and the next opening).
    const Input::Posef w = ToInput(window.pose);
    const Input::Quatf inv = Input::Conjugate(w.orientation);
    const Input::Vec3f bottom{0.0f, -window.height_m * 0.5f, 0.0f};
    const Input::Vec3f local = Input::Rotate(inv, pose.position - w.position);
    offset = (local - bottom) * (1.0f / std::max(window.width_m, 0.01f));
    turn = Input::Normalized(Input::Multiply(inv, pose.orientation));
}

void KeyboardPanel::PressKey(int key, int source, int64_t now) {
    const int code = Kb::KeyCode(sheet, key);
    if (code == 0) {
        return;
    }
    // Hold to lock: a modifier pressed starts its hold clock, a tap on a locked one lets it go.
    sticky_source = source;
    sticky.OnPress(code, now);
    const Kb::Press p = Kb::ApplyPress(sheet, mods, code);
    if (p.code > 0) {
        KeyboardEvent e;
        if (Kb::MakeEvent(p.code, p.code_mods, e)) {
            std::function<void(const KeyboardEvent&)> handler;
            {
                Inbox& inbox = TheInbox();
                std::scoped_lock lock{inbox.mutex};
                handler = inbox.handler;
            }
            if (handler) {
                handler(e);
            }
        }
    }
    sheet = p.sheet;
    mods = p.mods;
    if (p.hide) {
        Close("Hide");
    }
}

void KeyboardPanel::Frame(XrSession session, VkFormat format, OverlayUploader& uploader,
                          const FrameInfo& info, const Input::InputFrame& in, bool have_input,
                          const Window& window, Input::InputFrame& window_in,
                          std::vector<Quad>& layers, std::optional<XrPosef>& cursor) {
    if (!info.should_render) {
        return;
    }
    const XrTime now = info.display_time;
    const float dt =
        last_time ? std::min(0.1f, static_cast<float>(now - last_time) * 1e-9f) : 0.0f;
    last_time = now;

    Inbox::Request request = Inbox::Request::None;
    bool enabled = false;
    {
        Inbox& inbox = TheInbox();
        std::scoped_lock lock{inbox.mutex};
        request = inbox.request;
        inbox.request = Inbox::Request::None;
        enabled = static_cast<bool>(inbox.handler);
    }
    if (!shown) {
        shown = true;
        // Nothing from a button already held when the window appears.
        prev_a = prev_y = true;
        for (int h = 0; h < Input::kNumHands; h++) {
            trigger_down[h] = in.hand[h].trigger > 0.40f;
            grip_down[h] = in.hand[h].grip > 0.40f;
            trigger_ours[h] = grip_ours[h] = false;
        }
    }
    if (!enabled) {
        Close("no handler");
        return;
    }
    if (request == Inbox::Request::Open) {
        Open(true);
    } else if (request == Inbox::Request::Lost && opened_by_text_field) {
        Close("the text field lost the keys");
    }

    // Y shows or hides it (Moonlight XR's default for app:keyboard).
    if (have_input) {
        if (in.y && !prev_y) {
            if (open) {
                Close("Y");
            } else {
                Open(false);
            }
        }
        prev_y = in.y;
    }

    // The triggers and grips, with the window's thresholds, and whose press each one is.
    bool trigger_edge[2]{}, grip_edge[2]{};
    for (int h = 0; h < Input::kNumHands; h++) {
        const Input::HandInput& hand = in.hand[h];
        const bool t = have_input && Hysteresis(trigger_down[h], hand.trigger, 0.55f, 0.40f);
        const bool g = have_input && Hysteresis(grip_down[h], hand.grip, 0.60f, 0.40f);
        trigger_edge[h] = t && !trigger_down[h];
        grip_edge[h] = g && !grip_down[h];
        trigger_down[h] = t;
        grip_down[h] = g;
        if (!t) {
            trigger_ours[h] = false;
        }
        if (!g) {
            grip_ours[h] = false;
        }
    }
    const bool a = have_input && in.a;
    const bool a_edge = a && !prev_a;
    prev_a = a;
    if (!a) {
        a_ours = false;
    }
    if (a_ours) {
        window_in.a = false; // A pressed a key: not a click, also after Hide closed the keyboard
    }
    if (!open) {
        return;
    }

    // Where it hangs this frame: under the window, unless its bar is being held.
    if (drag_hand < 0) {
        Place(window);
    }
    const float frame_w = width_m / kWidthShare; // the window width it is laid out for
    const float pill_w = frame_w * kPillWidthShare;
    const float pill_h = frame_w * kPillHeightShare;
    const float drop = height_m * 0.5f + frame_w * (kGapShare + kPillHeightShare * 0.5f);
    const auto on_bar_zone = [&](float u, float v) {
        const float reach = (kGapShare + kPillHeightShare * 3.0f) * frame_w;
        const float bar_u = pill_w * kPillHover * 0.5f / width_m;
        return v > 1.0f && v < 1.0f + reach / height_m && std::abs(u - 0.5f) < bar_u;
    };

    // Where each ray lands. The keyboard owns the ground it covers: a ray on it does not reach
    // the window behind.
    const Input::PanelQuad kb_quad{pose, width_m, height_m, true};
    const Input::PanelQuad window_quad{ToInput(window.pose), window.width_m, window.height_m, true};
    Input::Ray ray[2]{};
    Input::QuadHit hit[2]{};
    bool on_kb[2]{}, on_bar[2]{}, on_window[2]{};
    for (int h = 0; h < Input::kNumHands; h++) {
        if (!have_input || !in.hand[h].active) {
            continue;
        }
        ray[h] = Input::RayFromPose(in.hand[h].aim);
        hit[h] = Input::IntersectQuad(ray[h], kb_quad);
        on_kb[h] = hit[h].in_bounds;
        on_bar[h] = hit[h].on_plane && !hit[h].in_bounds && on_bar_zone(hit[h].u, hit[h].v);
        on_window[h] = Input::IntersectQuad(ray[h], window_quad).in_bounds;
        // A press that starts on the keyboard or its bar is the keyboard's until it is let go.
        if (trigger_edge[h]) {
            trigger_ours[h] = on_kb[h] || on_bar[h];
        }
        if (grip_edge[h]) {
            grip_ours[h] = on_kb[h] || on_bar[h];
        }
    }

    // The bar: grip or trigger on it takes hold, and the keyboard follows the ray around you.
    if (drag_hand >= 0 && (!in.hand[drag_hand].active ||
                           !(drag_by_grip ? grip_down[drag_hand] : trigger_down[drag_hand]))) {
        drag_hand = -1;
    }
    if (drag_hand < 0) {
        for (int h : {pointing, 1 - pointing}) {
            if (on_bar[h] && (grip_edge[h] || trigger_edge[h])) {
                drag_hand = h;
                drag_by_grip = grip_edge[h];
                drag_t = hit[h].t;
                drag_offset = pose.position - hit[h].point_world;
                pointing = h;
                break;
            }
        }
    }
    if (drag_hand >= 0) {
        const Input::Vec3f grabbed = ray[drag_hand].origin + ray[drag_hand].direction * drag_t;
        Input::DragLimits limits;
        limits.max_z = -0.3f;
        if (auto moved = Input::SolveSphereDrag(pose, grabbed + drag_offset, {0, 0, 0}, nullptr,
                                                limits)) {
            pose = *moved;
        }
        // The stick of that hand pushes it away (up) and pulls it nearer (down).
        const float s = in.hand[drag_hand].stick.y;
        const float r = Input::Length(pose.position);
        if (std::abs(s) > kStickDead && r > 0.01f) {
            const float k = (std::abs(s) - kStickDead) / (1.0f - kStickDead);
            const float to = std::clamp(r + std::copysign(kPushRate * k, s) * dt, kNearest, kFarthest);
            const float scale = to / r;
            pose.position = pose.position * scale;
            drag_t *= scale;
            drag_offset = drag_offset * scale;
        }
        Keep(window);
    }

    // The pointing hand: the one that pressed on it last, else the other one if only it is on it.
    if (!on_kb[pointing] && on_kb[1 - pointing]) {
        pointing = 1 - pointing;
    }
    // Keys: each hand's trigger on a key presses it; A presses the key under the pointing ray.
    for (int h = 0; h < Input::kNumHands; h++) {
        if (open && trigger_edge[h] && on_kb[h] && drag_hand != h) {
            pointing = h;
            const int key = Kb::KeyAt(sheet, hit[h].u, hit[h].v);
            if (key >= 0) {
                PressKey(key, h, now);
            }
        }
    }
    if (open && a_edge && on_kb[pointing]) {
        a_ours = true;
        window_in.a = false;
        const int key = Kb::KeyAt(sheet, hit[pointing].u, hit[pointing].v);
        if (key >= 0) {
            PressKey(key, 2, now);
        }
    }
    // A press that lands on nothing at all puts the keyboard away. A press on the window is a
    // click and stays one.
    for (int h = 0; h < Input::kNumHands && open; h++) {
        if (trigger_edge[h] && in.hand[h].active && !on_kb[h] && !on_bar[h] && !on_window[h]) {
            Close("a press on nothing");
        }
    }
    if (!open) {
        return;
    }

    // Held to lock: the hold clock of the modifier pressed runs while its press is held.
    const bool still_down = sticky_source == 2 ? a
                            : sticky_source >= 0 ? trigger_down[sticky_source]
                                                 : false;
    if (const int bit = sticky.Tick(still_down, now, Kb::kDefaultHoldNs); bit != 0) {
        LOG_INFO(Frontend, "xr: keyboard modifier {:#x} locked", bit);
    }
    mods = sticky.Mods(mods);
    sheet = sticky.SheetFor(sheet);

    hover_key = drag_hand < 0 && on_kb[pointing]
                    ? Kb::KeyAt(sheet, hit[pointing].u, hit[pointing].v)
                    : -1;
    key_down = hover_key >= 0 && (trigger_down[pointing] || (a && a_ours));
    bar_hot = drag_hand >= 0 || on_bar[0] || on_bar[1];
    if (drag_hand < 0 && (on_kb[pointing] || on_bar[pointing])) {
        cursor = XrPosef{ToXr(pose).orientation,
                         {hit[pointing].point_world.x, hit[pointing].point_world.y,
                          hit[pointing].point_world.z}};
    }

    // What the window does not see: the hands on the keyboard or its bar (unless they hold a
    // press that began on the window), the ones holding a press that began here, and A pressing
    // a key.
    for (int h = 0; h < Input::kNumHands; h++) {
        const bool window_press =
            (trigger_down[h] && !trigger_ours[h]) || (grip_down[h] && !grip_ours[h]);
        const bool ours = drag_hand == h || (trigger_down[h] && trigger_ours[h]) ||
                          (grip_down[h] && grip_ours[h]) ||
                          (!window_press && (on_kb[h] || on_bar[h]));
        if (ours) {
            window_in.hand[h].active = false;
            window_in.hand[h].stick = {};
        }
    }

    // The art: the keys again when what they show changed, the ring and the pill once.
    if (!key_chain.handle) {
        if (!uploader.CreateChain(session, format, Kb::kTexWidth, Kb::kTexHeight, key_chain)) {
            return;
        }
        art_dirty = true;
    }
    if (!ring_chain.handle &&
        uploader.CreateChain(session, format, Kb::Art::kRingSize, Kb::Art::kRingSize, ring_chain)) {
        const auto ring = Kb::Art::RingArt();
        uploader.Upload(ring_chain, ring.data());
    }
    if (!pill_chain.handle && uploader.CreateChain(session, format, Kb::Art::kPillWidth,
                                                   Kb::Art::kPillHeight, pill_chain)) {
        const auto pill = Kb::Art::PillArt();
        uploader.Upload(pill_chain, pill.data());
    }
    const Kb::View view{sheet, mods, sticky.locked, hover_key};
    if ((art.Draw(view) || art_dirty || !key_chain.has_image) &&
        uploader.Upload(key_chain, art.Pixels())) {
        art_dirty = false;
    }
    if (!key_chain.has_image) {
        return;
    }

    Quad q;
    q.swapchain = key_chain.handle;
    q.image_rect = {{0, 0}, {Kb::kTexWidth, Kb::kTexHeight}};
    q.pose = ToXr(pose);
    q.size = {width_m, height_m};
    q.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layers.push_back(q);

    // The move bar, the window's pill under the keyboard; it swells a little under the ray or
    // while held, which is the lighting a quad layer can give.
    if (pill_chain.has_image) {
        const float grow = bar_hot ? 1.25f : 1.0f;
        Quad bar;
        bar.swapchain = pill_chain.handle;
        bar.image_rect = {{0, 0}, {Kb::Art::kPillWidth, Kb::Art::kPillHeight}};
        bar.pose = Offset(pose, {0.0f, -drop, 0.005f});
        bar.size = {pill_w * grow, pill_h * grow};
        bar.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        layers.push_back(bar);
    }

    // The ring on the key under the ray; it swells while the key is held, the only press feedback
    // a quad layer can give. Off the keys its place in the list is held by a clear speck behind
    // the keyboard (Moonlight XR: a layer that comes and goes ahead of others can make SteamVR
    // draw a frame wrong).
    if (ring_chain.has_image) {
        Quad ring;
        ring.swapchain = ring_chain.handle;
        ring.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        if (hover_key >= 0) {
            const Kb::Rect r = Kb::KeyRect(hover_key);
            const float grow = key_down ? 1.12f : 1.0f;
            ring.image_rect = {{0, 0}, {Kb::Art::kRingSize, Kb::Art::kRingSize}};
            ring.pose = Offset(pose, {((r.x0 + r.x1) * 0.5f - 0.5f) * width_m,
                                      (0.5f - (r.y0 + r.y1) * 0.5f) * height_m, 0.004f});
            ring.size = {(r.x1 - r.x0) * width_m * grow, (r.y1 - r.y0) * height_m * grow};
        } else {
            constexpr int32_t c = Kb::Art::kRingSize / 2 - 2;
            ring.image_rect = {{c, c}, {4, 4}};
            ring.pose = Offset(pose, {0.0f, 0.0f, -0.01f});
            ring.size = {0.001f, 0.001f};
        }
        layers.push_back(ring);
    }
}

} // namespace FrameXr
