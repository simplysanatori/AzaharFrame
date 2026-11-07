// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/session.h"
#include "frame_xr/vis/async_overlay.h"
#include "frame_xr/vis/controller_models.h"
#include "frame_xr/vis/help_content.h"
#include "frame_xr/vis/panels.h"
#include "frame_xr/vis/visuals.h"
#include "frame_xr/xr_loader.h"

namespace FrameXr::Vis {

namespace {

using Input::Posef;
using Input::Vec3f;

// ---- What other threads hand over ---------------------------------------------------------

struct Shared {
    std::mutex mutex;
    EmuStats emu;
    uint64_t emu_seq = 0;
    XrSecond xr;
    uint64_t xr_seq = 0;
    HelpBindings bindings;
    uint64_t bindings_seq = 0;
    std::atomic<int> help_request{-1}; ///< -1 none, 0 hide, 1 show, 2 toggle
    std::atomic<bool> head_locked{false};
};
Shared g_shared;

// ---- Placement ------------------------------------------------------------------------------

// The help panel: 0.8 window widths wide, left of the window, turned 35 degrees toward you.
constexpr float kHelpWidth = 0.8f;
constexpr float kHelpTurn = 0.6108652f;
constexpr float kHelpGap = 0.04f; ///< window widths
// The buttons: 0.3 window widths wide, over the window's top left corner.
constexpr float kStripWidth = 0.3f;
constexpr float kStripGap = 0.012f;
// The stats: 0.42 top screen heights wide, under its bottom left corner; head-locked in the
// immersive Full modes, low on the left.
constexpr float kStatsTopWidth = 0.42f;
constexpr float kStatsGap = 0.03f;
constexpr Vec3f kStatsHeadOffset{-0.17f, -0.14f, -0.6f};
constexpr float kStatsHeadWidth = 0.16f;
// Beams.
constexpr float kBeamWidth = 0.008f;
constexpr float kStubLength = 0.35f;
constexpr float kStubStrength = 0.4f;
constexpr float kDotNear = 0.03f; ///< a dot this close to a ray is that ray's
// Trigger hysteresis, as the mapper's.
constexpr float kPress = 0.55f, kLetGo = 0.40f;

const char* kSeenFile = "frame-vr-controls-shown";

Posef ToInput(const XrPosef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef ToXr(const Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

Posef Compose(const Posef& a, const Posef& b) {
    return {Input::Normalized(Input::Multiply(a.orientation, b.orientation)),
            Input::Transform(a, b.position)};
}

Input::Quatf YawQuat(float angle) {
    return {0.0f, std::sin(angle * 0.5f), 0.0f, std::cos(angle * 0.5f)};
}

Quad AtlasQuad(XrSwapchain chain, const AtlasRect& r, const Posef& pose, float w, float h) {
    Quad q;
    q.swapchain = chain;
    q.image_rect = {{r.x, r.y}, {r.w, r.h}};
    q.pose = ToXr(pose);
    q.size = {w, h};
    q.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    return q;
}

// System-wide CPU use since the last call, from /proc/stat (0..100), or -1.
struct CpuSampler {
    uint64_t busy = 0, total = 0;
    float Sample() {
        std::ifstream f("/proc/stat");
        std::string cpu;
        uint64_t v[8]{};
        if (!(f >> cpu) || cpu != "cpu") {
            return -1.0f;
        }
        for (auto& x : v) {
            f >> x;
        }
        const uint64_t t = v[0] + v[1] + v[2] + v[3] + v[4] + v[5] + v[6] + v[7];
        const uint64_t b = t - v[3] - v[4]; // without idle and iowait
        const uint64_t dt = t - total, db = b - busy;
        const bool first = total == 0;
        total = t;
        busy = b;
        return first || dt == 0 ? -1.0f : 100.0f * static_cast<float>(db) / static_cast<float>(dt);
    }
};

// The GPU's clock (devfreq), in MHz, or -1.
int GpuClockMhz() {
    static std::string path = [] {
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator("/sys/class/devfreq", ec)) {
            if (e.path().filename().string().find("gpu") != std::string::npos) {
                return (e.path() / "cur_freq").string();
            }
        }
        return std::string{};
    }();
    if (path.empty()) {
        return -1;
    }
    std::ifstream f(path);
    uint64_t hz = 0;
    return f >> hz ? static_cast<int>(hz / 1'000'000) : -1;
}

bool HandUsed(const Input::InputFrame& in, int h) {
    const Input::HandInput& hand = in.hand[static_cast<std::size_t>(h)];
    bool used = hand.trigger > 0.1f || hand.grip > 0.1f || hand.bumper || hand.stick_click ||
                std::abs(hand.stick.x) > 0.2f || std::abs(hand.stick.y) > 0.2f;
    if (h == 1) {
        used = used || in.a || in.b || in.x || in.y || in.menu;
    } else {
        used = used || in.view || in.dpad_up || in.dpad_down || in.dpad_left || in.dpad_right;
    }
    return used;
}

ModelInputs ModelInputsFor(const Input::InputFrame& in, int h) {
    const Input::HandInput& hand = in.hand[static_cast<std::size_t>(h)];
    ModelInputs m{};
    m[kInTrigger] = hand.trigger;
    m[kInGrip] = hand.grip;
    m[kInStickX] = hand.stick.x;
    m[kInStickY] = hand.stick.y;
    m[kInStickClick] = hand.stick_click ? 1.0f : 0.0f;
    m[kInBumper] = hand.bumper ? 1.0f : 0.0f;
    const auto f = [](bool b) { return b ? 1.0f : 0.0f; };
    if (h == 1) {
        m[kInA] = f(in.a);
        m[kInB] = f(in.b);
        m[kInX] = f(in.x);
        m[kInY] = f(in.y);
        m[kInMenu] = f(in.menu);
    } else {
        m[kInView] = f(in.view);
        m[kInDpadUp] = f(in.dpad_up);
        m[kInDpadDown] = f(in.dpad_down);
        m[kInDpadLeft] = f(in.dpad_left);
        m[kInDpadRight] = f(in.dpad_right);
    }
    return m;
}

} // namespace

void NoteXrSecond(const XrSecond& second) {
    std::scoped_lock lock{g_shared.mutex};
    g_shared.xr = second;
    g_shared.xr_seq++;
}

void NoteTopHeadLocked(bool head_locked) {
    g_shared.head_locked = head_locked;
}

struct Visuals::Impl {
    Runtime* rt = nullptr;
    AsyncOverlay atlas;
    AsyncOverlay stats_chain;
    PanelArt atlas_art;
    PanelArt stats_art;
    ControllerModels models;
    bool atlas_dirty = false;

    // The help panel and the buttons.
    bool help_open = false;
    bool help_shown = false;  ///< last frame
    bool strip_shown = false; ///< last frame
    Posef help_pose{}, strip_pose{};
    float help_w = 0, help_h = 0, strip_w = 0, strip_h = 0;
    bool close_hot = false;
    bool help_drawn = false;
    StripView strip_view{};       ///< what the strip should show (hover from TakeInput)
    StripView strip_drawn_view{}; ///< what the atlas holds
    bool strip_drawn = false;
    struct HelpKey {
        uint64_t bindings_seq = 0;
        bool face = true, in_menu = false, game = false;
        bool operator==(const HelpKey&) const = default;
    };
    HelpKey help_key{};
    bool help_close_hot_drawn = false;
    std::array<bool, 2> trigger_down{};
    std::array<bool, 2> ours{}; ///< the hand is on our panels (taken from the window)

    // Stats.
    bool stats_on = false;
    bool stats_setting_seen = false;
    uint64_t stats_xr_seq = 0;
    CpuSampler cpu;
    std::mutex hook_mutex;
    std::function<void(bool)> stats_changed;

    // Beams, dots, models.
    std::array<HandActivity, 2> activity{};
    std::vector<Quad> top_beams, top_dots;
    const XrCompositionLayerBaseHeader* models_layer = nullptr;
    std::size_t max_layers = 16;

    void LoadSeen();
    void ToggleStats();
    std::vector<StatsRow> StatsRows();
};

Visuals::Visuals() : impl(std::make_unique<Impl>()) {}
Visuals::~Visuals() {
    DestroyXr();
}

void Visuals::SetStatsChanged(std::function<void(bool)> callback) {
    std::scoped_lock lock{impl->hook_mutex};
    impl->stats_changed = std::move(callback);
}

void Visuals::Impl::LoadSeen() {
    const std::string path = FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + kSeenFile;
    if (FileUtil::Exists(path)) {
        return;
    }
    help_open = true; // the first time VR starts
    std::ofstream f(path);
    f << "The controls help was shown in VR once.\n";
    LOG_INFO(Frontend, "xr: first VR start: the controls help is shown");
}

void Visuals::Create(Runtime& rt) {
    Impl& m = *impl;
    m.rt = &rt;
    const VkDevice device = rt.vulkan->Device();
    const VkPhysicalDevice physical = rt.vulkan->PhysicalDevice();
    const uint32_t family = rt.vulkan->QueueFamily();
    m.atlas.Create(rt.session, device, physical, family, rt.queue_lock, rt.swapchain_format,
                   kAtlasWidth, kAtlasHeight);
    m.stats_chain.Create(rt.session, device, physical, family, rt.queue_lock, rt.swapchain_format,
                         kStatsWidth, kStatsHeight);
    m.atlas_art.Resize(kAtlasWidth, kAtlasHeight);
    m.stats_art.Resize(kStatsWidth, kStatsHeight);
    DrawBeam(m.atlas_art);
    m.help_drawn = m.strip_drawn = false;
    m.atlas_dirty = true;
    m.stats_xr_seq = 0;
    m.stats_setting_seen = Settings::values.vr_show_stats.GetValue();
    m.stats_on = m.stats_setting_seen;
    m.LoadSeen();
    XrSystemProperties sys{XR_TYPE_SYSTEM_PROPERTIES};
    if (XR_SUCCEEDED(Xr().xrGetSystemProperties(rt.instance, rt.system, &sys)) &&
        sys.graphicsProperties.maxLayerCount > 0) {
        m.max_layers = sys.graphicsProperties.maxLayerCount;
    }
    LOG_INFO(Frontend, "xr: vis: 2 swapchains (help, buttons and beams; stats), and 1 for the "
                       "controller models once they show");
}

void Visuals::DestroyXr() {
    Impl& m = *impl;
    m.models.DestroyXr();
    m.atlas.Destroy();
    m.stats_chain.Destroy();
    m.models_layer = nullptr;
    m.top_beams.clear();
    m.top_dots.clear();
    m.help_shown = m.strip_shown = false;
    m.ours = {};
    m.trigger_down = {};
    m.rt = nullptr;
}

void Visuals::Impl::ToggleStats() {
    stats_on = !stats_on;
    LOG_INFO(Frontend, "xr: Stats button: {}", stats_on ? "on" : "off");
    std::scoped_lock lock{hook_mutex};
    if (stats_changed) {
        stats_changed(stats_on);
    }
}

void Visuals::TakeInput(const FrameInfo& info, Where where, Input::InputFrame& in,
                        bool have_input) {
    Impl& m = *impl;
    static_cast<void>(info);
    int hover = -1;
    bool close_hot = false;
    for (int h = 0; h < Input::kNumHands; h++) {
        auto& hand = in.hand[static_cast<std::size_t>(h)];
        const bool pressed = hand.trigger > (m.trigger_down[h] ? kLetGo : kPress);
        const bool edge = pressed && !m.trigger_down[h];
        if (!have_input || where == Where::Game || !hand.active) {
            m.trigger_down[h] = pressed;
            m.ours[h] = false;
            continue;
        }
        const Input::Ray ray = Input::RayFromPose(hand.aim);
        std::optional<Input::QuadHit> strip_hit, help_hit;
        if (m.strip_shown) {
            const auto hit = Input::IntersectQuad(ray, {m.strip_pose, m.strip_w, m.strip_h, true});
            if (hit.in_bounds) {
                strip_hit = hit;
            }
        }
        if (m.help_shown) {
            const auto hit = Input::IntersectQuad(ray, {m.help_pose, m.help_w, m.help_h, true});
            if (hit.in_bounds && (!strip_hit || hit.t < strip_hit->t)) {
                help_hit = hit;
                strip_hit.reset();
            }
        }
        // A trigger held down from Azahar's window stays the window's.
        const bool on_us = (strip_hit || help_hit) && (!m.trigger_down[h] || m.ours[h]);
        if (on_us) {
            if (strip_hit) {
                const float x = strip_hit->u * kStripRect.w;
                const int b = x < (kStripControls1 + kStripStats0) * 0.5f ? 0 : 1;
                hover = b;
                if (edge) {
                    if (b == 0) {
                        m.help_open = !m.help_open;
                        LOG_INFO(Frontend, "xr: Controls button: help {}",
                                 m.help_open ? "shown" : "hidden");
                    } else {
                        m.ToggleStats();
                    }
                }
            } else {
                const float px = help_hit->u * kHelpRect.w, py = help_hit->v * kHelpRect.h;
                const bool on_close =
                    std::hypot(px - kCloseX, py - kCloseY) < kCloseR + 8.0f;
                close_hot = close_hot || on_close;
                if (edge && on_close) {
                    m.help_open = false;
                    LOG_INFO(Frontend, "xr: controls help closed");
                }
            }
            hand.active = false; // the window and the keyboard do not see this hand
            hand.trigger = 0.0f;
            hand.grip = 0.0f;
            hand.stick = {};
            hand.stick_click = false;
        }
        m.ours[h] = on_us || (m.ours[h] && pressed);
        m.trigger_down[h] = pressed;
    }
    m.strip_view.hover = hover;
    m.close_hot = close_hot;
}

std::vector<StatsRow> Visuals::Impl::StatsRows() {
    EmuStats e;
    XrSecond x;
    {
        std::scoped_lock lock{g_shared.mutex};
        e = g_shared.emu;
        x = g_shared.xr;
    }
    std::vector<StatsRow> rows;
    rows.push_back({"App FPS", fmt::format("{:.0f}", e.game_fps)});
    rows.push_back({"Speed", fmt::format("{:.0f}%", e.emulation_speed * 100.0)});
    rows.push_back({"Frame Time", fmt::format("{:.2f} ms", e.frame_ms)});
    rows.push_back({"Peak Frame Time", fmt::format("{:.2f} ms", e.frame_peak_ms)});
    rows.push_back({"Slow Frames", fmt::format("{}", e.slow_frames)});
    rows.push_back({"Display", x.seconds > 0.0
                                   ? fmt::format("{:.0f} Hz", x.display_frames / x.seconds)
                                   : std::string{"-"}});
    rows.push_back({"Late Frames", fmt::format("{}", x.late)});
    const float load = cpu.Sample();
    rows.push_back({"CPU", load >= 0.0f ? fmt::format("{:.0f}%", load) : std::string{"-"}});
    const int mhz = GpuClockMhz();
    rows.push_back({"GPU Clock", mhz >= 0 ? fmt::format("{} MHz", mhz) : std::string{"-"}});
    return rows;
}

void Visuals::Frame(Runtime& rt, const FrameInfo& info, Where where, const Window& window,
                    const Input::InputFrame& in, bool have_input, std::vector<Quad>& layers,
                    XrSwapchain dot, XrSwapchain dim) {
    Impl& m = *impl;
    m.top_beams.clear();
    m.top_dots.clear();
    m.models_layer = nullptr;
    m.atlas.Poll();
    m.stats_chain.Poll();

    // ---- Requests from other threads, the settings ----
    if (const int req = g_shared.help_request.exchange(-1); req >= 0) {
        m.help_open = req == 2 ? !m.help_open : req == 1;
    }
    const bool stats_setting = Settings::values.vr_show_stats.GetValue();
    if (stats_setting != m.stats_setting_seen) {
        m.stats_setting_seen = stats_setting;
        m.stats_on = stats_setting;
    }

    // ---- The help panel and the buttons, next to Azahar's window ----
    const bool with_window = window.shown && where != Where::Game;
    m.strip_shown = m.help_shown = false;
    if (with_window && m.atlas.Valid()) {
        const Posef wp = ToInput(window.pose);
        const float W = window.width, H = window.height;
        m.strip_w = kStripWidth * W;
        m.strip_h = m.strip_w * kStripRect.h / kStripRect.w;
        m.strip_pose = Compose(wp, {{0, 0, 0, 1},
                                    {-W * 0.5f + m.strip_w * 0.5f,
                                     H * 0.5f + kStripGap * W + m.strip_h * 0.5f, 0.0f}});
        m.help_w = kHelpWidth * W;
        m.help_h = m.help_w * kHelpRect.h / kHelpRect.w;
        const Input::Quatf turn = YawQuat(kHelpTurn);
        const Vec3f edge{-W * 0.5f - kHelpGap * W, H * 0.5f - m.help_h * 0.5f, 0.0f};
        const Vec3f centre = edge + Input::Rotate(turn, {-m.help_w * 0.5f, 0.0f, 0.0f});
        m.help_pose = Compose(wp, {turn, centre});

        m.strip_view.help_on = m.help_open;
        m.strip_view.stats_on = m.stats_on;
        if (!m.strip_drawn || !(m.strip_drawn_view == m.strip_view)) {
            DrawStrip(m.atlas_art, m.strip_view);
            m.strip_drawn_view = m.strip_view;
            m.strip_drawn = true;
            m.atlas_dirty = true;
        }
        if (m.help_open) {
            Impl::HelpKey key;
            HelpBindings bindings;
            {
                std::scoped_lock lock{g_shared.mutex};
                key.bindings_seq = g_shared.bindings_seq;
                bindings = g_shared.bindings;
            }
            key.face = Settings::values.vr_face_buttons.GetValue() == 0;
            key.in_menu = where == Where::Menu;
            key.game = where != Where::Home;
            if (!m.help_drawn || !(key == m.help_key)) {
                // A few milliseconds of drawing, only when what it says changes.
                HelpContext ctx;
                ctx.face_by_position = key.face;
                ctx.in_menu = key.in_menu;
                ctx.game_running = key.game;
                DrawHelp(m.atlas_art, BuildHelp(bindings, ctx));
                m.help_key = key;
                m.help_close_hot_drawn = false;
                m.help_drawn = true;
                m.atlas_dirty = true;
            }
            if (m.close_hot != m.help_close_hot_drawn) {
                DrawHelpClose(m.atlas_art, m.close_hot);
                m.help_close_hot_drawn = m.close_hot;
                m.atlas_dirty = true;
            }
        }
        if (m.atlas_dirty) {
            m.atlas.Upload(m.atlas_art.Pixels());
            m.atlas_dirty = false;
        }
        if (m.atlas.HasImage() && info.should_render) {
            if (m.help_open && m.help_drawn) {
                layers.push_back(
                    AtlasQuad(m.atlas.Handle(), kHelpRect, m.help_pose, m.help_w, m.help_h));
                m.help_shown = true;
            }
            layers.push_back(
                AtlasQuad(m.atlas.Handle(), kStripRect, m.strip_pose, m.strip_w, m.strip_h));
            m.strip_shown = true;
        }
    } else if (m.atlas.Valid() && m.atlas_dirty) {
        m.atlas.Upload(m.atlas_art.Pixels()); // the beam art, before any window shows
        m.atlas_dirty = false;
    }

    // ---- Stats, under the top screen, while a game runs ----
    if (m.stats_on && where != Where::Home && info.screens_live && m.stats_chain.Valid()) {
        uint64_t seq;
        {
            std::scoped_lock lock{g_shared.mutex};
            seq = g_shared.xr_seq;
        }
        if (seq != m.stats_xr_seq) {
            m.stats_xr_seq = seq;
            DrawStats(m.stats_art, m.StatsRows());
            m.stats_chain.Upload(m.stats_art.Pixels()); // never waits (async_overlay.h)
        }
        if (m.stats_chain.HasImage() && info.should_render) {
            Posef pose;
            float w;
            if (g_shared.head_locked) {
                const ViewPose v = LatestViewPose();
                const Posef head = ToInput(v.head);
                w = kStatsHeadWidth;
                pose = Compose(head, {{0, 0, 0, 1}, kStatsHeadOffset});
            } else {
                const ScreenPlacement place = GetScreenPlacement();
                const float tw = place.top_size.width, th = place.top_size.height;
                w = kStatsTopWidth * th;
                const float h = w * kStatsHeight / kStatsWidth;
                pose = Compose(ToInput(place.top_pose),
                               {{0, 0, 0, 1}, {-tw * 0.5f + w * 0.5f, -th * 0.5f - kStatsGap - h * 0.5f, 0}});
            }
            Quad q;
            q.swapchain = m.stats_chain.Handle();
            q.image_rect = {{0, 0}, {kStatsWidth, kStatsHeight}};
            q.pose = ToXr(pose);
            q.size = {w, w * kStatsHeight / kStatsWidth};
            q.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            layers.push_back(q);
        }
    }

    // ---- Each controller: activity, beam, dot, model ----
    const ViewPose view = LatestViewPose();
    const Posef head = ToInput(view.head);
    std::array<std::optional<Vec3f>, 2> beam_end{};
    std::array<bool, 2> stub{};
    for (int h = 0; h < Input::kNumHands; h++) {
        const Input::HandInput& hand = in.hand[static_cast<std::size_t>(h)];
        Input::HandInput tracked = hand;
        tracked.active = have_input && hand.active;
        m.activity[h].Update(info.display_time, tracked, have_input && HandUsed(in, h));
    }
    // Where each ray ends: a pointer dot on it, else (by the window) the nearest panel it hits.
    std::vector<Quad> dots;
    std::vector<Quad> kept;
    kept.reserve(layers.size());
    for (const Quad& q : layers) {
        (q.swapchain == dot ? dots : kept).push_back(q);
    }
    std::array<int, 16> dot_hand{};
    dot_hand.fill(-1);
    for (int h = 0; h < Input::kNumHands; h++) {
        const Input::HandInput& hand = in.hand[static_cast<std::size_t>(h)];
        if (!have_input || !hand.active) {
            continue;
        }
        const Input::Ray ray = Input::RayFromPose(hand.aim);
        float best = 1e9f;
        for (std::size_t d = 0; d < dots.size() && d < dot_hand.size(); d++) {
            const Vec3f p{dots[d].pose.position.x, dots[d].pose.position.y,
                          dots[d].pose.position.z};
            const Vec3f to = p - ray.origin;
            const float t = Input::Dot(to, ray.direction);
            if (t > 0.0f && Input::Length(to - ray.direction * t) < kDotNear && t < best) {
                best = t;
                beam_end[h] = p;
                dot_hand[d] = h;
            }
        }
        if (where != Where::Game) {
            for (const Quad& q : kept) {
                if (q.swapchain == dim || q.swapchain == m.stats_chain.Handle()) {
                    continue;
                }
                const auto hit = Input::IntersectQuad(
                    ray, {ToInput(q.pose), q.size.width, q.size.height, true});
                if (hit.in_bounds && hit.t < best) {
                    best = hit.t;
                    beam_end[h] = hit.point_world;
                }
            }
            if (!beam_end[h]) {
                beam_end[h] = ray.origin + ray.direction * kStubLength;
                stub[h] = true;
            }
        }
    }
    layers = std::move(kept);
    if (info.should_render && view.valid) {
        for (int h = 0; h < Input::kNumHands; h++) {
            const float strength = m.activity[h].Beam() * (stub[h] ? kStubStrength : 1.0f);
            const auto level = BeamLevel(strength);
            if (!beam_end[h] || !level || !m.atlas.HasImage()) {
                continue;
            }
            const Vec3f start = in.hand[static_cast<std::size_t>(h)].aim.position;
            if (const auto b = PlaceBeam(start, *beam_end[h], head, kBeamWidth)) {
                const AtlasRect r{kBeamRect.x + *level * kBeamTexWidth, kBeamRect.y, kBeamTexWidth,
                                  kBeamTexHeight};
                m.top_beams.push_back(
                    AtlasQuad(m.atlas.Handle(), r, b->pose, b->width, b->length));
            }
        }
        // The dots, over the beams; a dot of an idle controller goes with its beam.
        for (std::size_t d = 0; d < dots.size(); d++) {
            const int h = d < dot_hand.size() ? dot_hand[d] : -1;
            if (h >= 0 && m.activity[h].Beam() <= 0.02f) {
                continue;
            }
            m.top_dots.push_back(dots[d]);
        }
    } else {
        m.top_dots = dots;
    }

    // ---- The controllers ----
    std::array<ControllerModels::Hand, 2> hands{};
    if (Settings::values.vr_show_controllers.GetValue() && info.should_render) {
        for (int h = 0; h < Input::kNumHands; h++) {
            auto& out = hands[static_cast<std::size_t>(h)];
            out.shown = m.activity[h].Tracked() && m.activity[h].Model() > 0.02f;
            out.aim = m.activity[h].Pose();
            out.fade = m.activity[h].Model();
            out.input = ModelInputsFor(in, h);
        }
    }
    m.models.Frame(rt, view, hands);
    m.models_layer = m.models.Layer();
}

const XrCompositionLayerBaseHeader* Visuals::TopLayers(std::vector<Quad>& top,
                                                       std::size_t used) const {
    const Impl& m = *impl;
    std::size_t room = m.max_layers > used ? m.max_layers - used : 0;
    const std::size_t dots = std::min(room, m.top_dots.size());
    room -= dots;
    const XrCompositionLayerBaseHeader* models = room > 0 ? m.models_layer : nullptr;
    room -= models ? 1 : 0;
    const std::size_t beams = std::min(room, m.top_beams.size());
    if (beams < m.top_beams.size() || dots < m.top_dots.size() || (m.models_layer && !models)) {
        static int said = 0;
        if (said++ < 5) {
            LOG_WARNING(Frontend, "xr: vis: {} layers allowed, {} used: beams {}/{}, models {}",
                        m.max_layers, used, beams, m.top_beams.size(), models ? "on" : "off");
        }
    }
    top.insert(top.end(), m.top_beams.begin(), m.top_beams.begin() + beams);
    top.insert(top.end(), m.top_dots.begin(), m.top_dots.begin() + dots);
    return models;
}

std::size_t Visuals::MaxLayers() const {
    return impl->max_layers;
}

} // namespace FrameXr::Vis

// ---- frame_xr.h ---------------------------------------------------------------------------------

namespace FrameXr {

void SetEmuStats(const EmuStats& stats) {
    std::scoped_lock lock{Vis::g_shared.mutex};
    Vis::g_shared.emu = stats;
    Vis::g_shared.emu_seq++;
}

void SetControlBindings(const std::vector<std::string>& buttons,
                        const std::vector<std::string>& analogs, const std::string& profile) {
    Vis::HelpBindings b;
    for (std::size_t i = 0; i < b.buttons.size() && i < buttons.size(); i++) {
        b.buttons[i] = buttons[i];
    }
    for (std::size_t i = 0; i < b.analogs.size() && i < analogs.size(); i++) {
        b.analogs[i] = analogs[i];
    }
    b.profile_name = profile;
    b.valid = true;
    std::scoped_lock lock{Vis::g_shared.mutex};
    Vis::g_shared.bindings = std::move(b);
    Vis::g_shared.bindings_seq++;
}

void ShowControlsHelp(bool show) {
    Vis::g_shared.help_request = show ? 1 : 0;
}

void ToggleControlsHelp() {
    Vis::g_shared.help_request = 2;
}

} // namespace FrameXr
