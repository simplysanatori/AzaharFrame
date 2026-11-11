// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: CitraVR's immersive mode (vr_main.cpp UpdateImmersiveModeIfNeeded and
// GameSurfaceLayer.cpp FrameTopPanel) for SteamVR, which has no cylinder layers: the top screen
// is a quad, head-locked in VIEW space for modes 2 and 3, fixed in the room for mode 1. See
// IMMERSIVE.md for the behaviour and the differences from CitraVR.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <string>

#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/immersive.h"
#include "frame_xr/immersive_layout.h"
#include "frame_xr/input/input_frame.h"
#include "video_core/vr_immersive.h"

namespace FrameXr::Immersive {

namespace {

namespace L = Layout;

// CitraVR vr_main.cpp:84 immersiveScaleFactor: 3.0 for mode 1; modes 2 and 3 use 1.0 to 1.4
// (CitraVR's 1.4), see immersive_layout.h.
constexpr float kWideFactor = L::kWideFactor;
// CitraVR put the screen 0.5 m ahead (vr_main.cpp:413, GameSurfaceLayer.cpp
// kSuperImmersiveRadius); here 0.75 m, and the grips move it (immersive_layout.h).
constexpr float kDefaultDistance = L::kDefaultDistance;
// The vertical field of view a game's camera is assumed to have (Ocarina of Time uses 60
// degrees); "Immersive Game FOV" (vr_immersive_game_fov) replaces it per game.
constexpr float kDefaultGameFovY = 60.0f;
// The file the placement is saved in, per game, in the user directory.
constexpr const char* kPlacementFile = "immersive-placement.txt";
// Half of rasterizer_accelerated.cpp's VR_IPD, used when the eye poses are not known.
constexpr float kDefaultHalfIpd = 0.065f / 2.0f;
// The top screen's width over its height: the 3DS's 400x240, and the other shapes of "VR
// Screen Shape" (vr_top_aspect 0 to 5).
constexpr float kAspect3ds = 400.0f / 240.0f;
constexpr float kAspectWide = 16.0f / 9.0f;
constexpr float kAspectUltrawide = 21.0f / 9.0f;
constexpr float kAspect4x3 = 4.0f / 3.0f;
constexpr float kAspect3x2 = 3.0f / 2.0f;
constexpr float kAspect16x10 = 16.0f / 10.0f;

// The aspect in use (TopAspect). Outside State: it is read from other threads.
std::atomic<float> g_top_aspect{kAspect3ds};

float AspectFor(u32 shape) {
    switch (shape) {
    case 1:
        return kAspectWide;
    case 2:
        return kAspectUltrawide;
    case 3:
        return kAspect4x3;
    case 4:
        return kAspect3x2;
    case 5:
        return kAspect16x10;
    default:
        return kAspect3ds;
    }
}

struct Tuning {
    float distance = kDefaultDistance; ///< the default distance (a saved placement wins)
    float game_fov_y_deg = kDefaultGameFovY;
    bool fov_from_env = false; ///< AZAHARFRAME_IMMERSIVE_FOV is set: it wins over the setting
    bool local_space = false; ///< head-locked screen placed in LOCAL from the head pose
};

float EnvFloat(const char* name, float fallback, float lo, float hi) {
    const char* v = std::getenv(name);
    if (!v || !*v) {
        return fallback;
    }
    char* end = nullptr;
    const float f = std::strtof(v, &end);
    if (end == v || !std::isfinite(f)) {
        LOG_WARNING(Frontend, "xr: immersive: {}={} is not a number; using {}", name, v, fallback);
        return fallback;
    }
    return std::clamp(f, lo, hi);
}

// Headset tuning without a rebuild (IMMERSIVE.md).
Tuning ReadTuning() {
    Tuning t;
    t.distance = EnvFloat("AZAHARFRAME_IMMERSIVE_DISTANCE", kDefaultDistance, L::kMinDistance,
                          L::kMaxDistance);
    const char* fov_env = std::getenv("AZAHARFRAME_IMMERSIVE_FOV");
    t.fov_from_env = fov_env && *fov_env;
    t.game_fov_y_deg = EnvFloat("AZAHARFRAME_IMMERSIVE_FOV", kDefaultGameFovY, 20.0f, 120.0f);
    const char* local = std::getenv("AZAHARFRAME_IMMERSIVE_LOCAL");
    t.local_space = local && std::string(local) == "1";
    return t;
}

// ---- Pose math (OpenXR conventions: -Z forward, +Y up) ----------------------------------

XrQuaternionf Mul(const XrQuaternionf& a, const XrQuaternionf& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

XrQuaternionf Conj(const XrQuaternionf& q) {
    return {-q.x, -q.y, -q.z, q.w};
}

XrQuaternionf Normalized(const XrQuaternionf& q) {
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (n < 1e-6f) {
        return {0, 0, 0, 1};
    }
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}

// v' = q v q* (as CitraVR's XrMath::Quatf::Rotate).
XrVector3f Rotate(const XrQuaternionf& q, const XrVector3f& v) {
    const float vx = 2 * (q.y * v.z - q.z * v.y);
    const float vy = 2 * (q.z * v.x - q.x * v.z);
    const float vz = 2 * (q.x * v.y - q.y * v.x);
    return {v.x + q.w * vx + (q.y * vz - q.z * vy), v.y + q.w * vy + (q.z * vx - q.x * vz),
            v.z + q.w * vz + (q.x * vy - q.y * vx)};
}

XrVector3f Sub(const XrVector3f& a, const XrVector3f& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

XrVector3f Add(const XrVector3f& a, const XrVector3f& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

/// The heading of a pose: rotation about +Y only.
XrQuaternionf YawOnly(const XrQuaternionf& q) {
    const XrVector3f f = Rotate(q, {0.0f, 0.0f, -1.0f});
    const float yaw = std::atan2(-f.x, -f.z);
    return {0.0f, std::sin(yaw / 2), 0.0f, std::cos(yaw / 2)};
}

XrPosef Compose(const XrPosef& a, const XrPosef& b) {
    return {Normalized(Mul(a.orientation, b.orientation)),
            Add(a.position, Rotate(a.orientation, b.position))};
}

/// CitraVR's UpdateImmersiveModeIfNeeded (vr_main.cpp:1310-1360) for a head pose relative to
/// the reference: the inverse head rotation (CitraVR's ToRotationMatrix + ToInverse; the
/// inverse of a rotation is its transpose) with the inverse head position, rotated into the
/// head frame and scaled, in elements 12..14.
void FillInverseView(const XrQuaternionf& q, const XrVector3f& position, float scaler,
                     std::array<float, 16>& inv) {
    const float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
    const float xx2 = q.x * x2, xy2 = q.x * y2, xz2 = q.x * z2;
    const float yy2 = q.y * y2, yz2 = q.y * z2, zz2 = q.z * z2;
    const float sx2 = q.w * x2, sy2 = q.w * y2, sz2 = q.w * z2;
    // XrMath::Quatf::ToRotationMatrix, element order kept.
    const float r[16] = {1 - (yy2 + zz2), xy2 + sz2,       xz2 - sy2,       0.f,
                         xy2 - sz2,       1 - (xx2 + zz2), yz2 + sx2,       0.f,
                         xz2 + sy2,       yz2 - sx2,       1 - (xx2 + yy2), 0.f,
                         0.f,             0.f,             0.f,             1.f};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            inv[i * 4 + j] = r[j * 4 + i];
        }
    }
    const XrVector3f p = Rotate(Conj(q), position);
    inv[12] = -p.x * scaler;
    inv[13] = -p.y * scaler;
    inv[14] = -p.z * scaler;
}

Input::Posef ToIn(const XrPosef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef ToXr(const Input::Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

/// The eye's FOV in head angles: its own angles plus how far the eye is turned (canted displays).
L::EyeFov EyeInHead(const XrPosef& head, const XrPosef& eye, const XrFovf& fov) {
    const XrVector3f f = Rotate(Conj(head.orientation), Rotate(eye.orientation, {0.0f, 0.0f, -1.0f}));
    L::EyeFov e{fov.angleLeft, fov.angleRight, fov.angleUp, fov.angleDown};
    e.yaw = std::atan2(-f.x, -f.z);
    e.pitch = std::atan2(f.y, std::hypot(f.x, f.z));
    return e;
}

std::string PlacementPath() {
    return FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + kPlacementFile;
}

float Deg(float rad) {
    return rad / L::kDegToRad;
}

// The title id of the game being started (SetGameTitleId, GUI thread).
std::atomic<uint64_t> g_title_id{0};

// ---- State --------------------------------------------------------------------------

struct State {
    int callback_id = 0;
    Tuning tuning;
    // XR thread from here on.
    u32 mode = 0;
    bool live = false;           ///< a game shows its screens and the session is visible
    bool engaged = false;        ///< immersive this frame
    bool bottom_toggled = false; ///< the bottom screen's visibility flipped (the bumper chord)
    bool focus_bottom = false;   ///< the bottom screen is in focus (SetBottomFocused)
    bool menu_open = false;      ///< the in-game menu shows (immersive modes 2 and 3 pause)
    bool hide_bottom = false;    ///< "Hide Bottom Screen" (per game), read when the game starts
    float game_fov_y_deg = kDefaultGameFovY; ///< "Immersive Game FOV" (per game), every frame
    u32 shape = 0;             ///< "Screen Shape" (per game), every frame (g_top_aspect)
    bool shape_logged = false; ///< both logged for this game
    L::Options options; ///< "Wider View" (every frame) and "Narrow Screen" (at start), per game
    u32 wider_level = 0; ///< AzaharFrame widerview: the level options.wider comes from
    bool screens_were_live = false;
    bool recentre = false;
    bool have_ref = false;
    XrPosef ref{{0, 0, 0, 1}, {0, 0, 0}}; ///< head position and heading when immersive began
    ViewPose view;
    float half_ipd = kDefaultHalfIpd;
    bool hw_shader_warned = false;
    // The placement (immersive_layout.h): per game, head-locked (modes 2, 3) and wide (mode 1).
    uint64_t title_id = 0;
    L::Store store;
    bool store_loaded = false;
    L::Placement head_place;
    bool head_old_size = false; ///< head_place.size is an earlier build's (converted once seen)
    L::Placement wide_place;
    L::ViewFov view_fov;
    bool view_fov_logged = false;
    L::HeadLocked solved;
    L::Adjuster adjuster;
    /// The global Immersive Offset (vr_immersive_offset_x, _y), read every frame.
    L::LensOffset offset;
    bool offset_logged = false;
    int64_t last_input_ns = 0;
};

State g;

bool IsViewMode(u32 mode) {
    return mode >= 2;
}

L::ScreenDecision Decision() {
    L::ScreenState s;
    s.mode = g.mode;
    s.live = g.live;
    s.hide_bottom = g.hide_bottom;
    s.bottom_toggled = g.bottom_toggled;
    s.bottom_focused = g.focus_bottom;
    s.menu_open = g.menu_open;
    s.view_valid = g.view.valid;
    return L::Decide(s);
}

bool BottomShown() {
    return Decision().show_bottom;
}

L::Placement DefaultPlacement() {
    L::Placement p;
    p.distance = g.tuning.distance;
    return p;
}

// AzaharFrame widerview: the head-locked default grows with Wider View, so the picture has the
// scale (and the gain) it has with Wider View off; only more shows around it.
L::Placement DefaultHeadPlacement() {
    L::Placement p = DefaultPlacement();
    p.size = L::SizeForWider(p.size, L::kFactorMin, g.options.wider);
    return p;
}

void LoadStore() {
    if (g.store_loaded) {
        return;
    }
    g.store_loaded = true;
    std::string text;
    if (FileUtil::ReadFileToString(true, PlacementPath(), text) > 0) {
        g.store.Parse(text);
    }
}

void SaveStore() {
    const std::string path = PlacementPath();
    const std::string tmp = path + ".new";
    const std::string text = g.store.Format();
    if (FileUtil::WriteStringToFile(true, tmp, text) != text.size() ||
        !FileUtil::Rename(tmp, path)) {
        LOG_WARNING(Frontend, "xr: immersive: could not save the screen placement");
    }
}

void LogPlacement(const char* what, const L::Placement& p, bool wide) {
    LOG_INFO(Frontend, "xr: immersive {} placement ({}): {:.1f} degrees from the view's middle, "
                       "{:.2f} m, size {:.2f}",
             wide ? "wide" : "head-locked", what, p.pitch_deg, p.distance, p.size);
}

void OnFrame(const FrameInfo& info) {
    const u32 mode = Settings::values.vr_immersive_mode.GetValue();
    const bool visible =
        info.state == XR_SESSION_STATE_VISIBLE || info.state == XR_SESSION_STATE_FOCUSED;
    if (!info.screens_live) {
        // No game: the next one starts with the bottom screen hidden, as in CitraVR.
        g.bottom_toggled = false;
        g.focus_bottom = false;
        g.have_ref = false;
        g.adjuster.Cancel();
    } else if (!g.screens_were_live) {
        // A game just started: its per-game settings are loaded; read "Hide Bottom Screen" now.
        g.hide_bottom = Settings::values.vr_hide_bottom_screen.GetValue() ||
                        Settings::values.vr_bottom_screen_window.GetValue(); // AzaharFrame window
        LOG_INFO(Frontend, "xr: bottom screen {} at start", g.hide_bottom ? "hidden" : "shown");
        // The screen shape and the game camera's angle (per game as well) are read below.
        g.shape_logged = false;
        g.wider_level = std::min(Settings::values.vr_immersive_wider_view_level.GetValue(),
                                 L::kMaxWiderLevel);
        g.options.wider = L::WiderFactor(g.wider_level);
        g.options.squeeze = L::SqueezeFor(Settings::values.vr_immersive_squeeze.GetValue());
        LOG_INFO(Frontend, "xr: immersive: Wider View {:.1f}x, Narrow Screen {:.0f}%",
                 g.options.wider, g.options.squeeze * 100.0f);
        // The saved placement of this game.
        g.title_id = g_title_id.load();
        LoadStore();
        const auto head = g.title_id ? g.store.Get(g.title_id, false) : std::nullopt;
        const auto wide = g.title_id ? g.store.Get(g.title_id, true) : std::nullopt;
        g.head_place = head.value_or(DefaultHeadPlacement());
        g.head_old_size = head && g.store.OldSize(g.title_id);
        g.wide_place = wide.value_or(DefaultPlacement());
        if (mode != 0) {
            const bool is_wide = !IsViewMode(mode);
            LogPlacement((is_wide ? wide : head) ? "saved" : "default",
                         is_wide ? g.wide_place : g.head_place, is_wide);
        }
    }
    g.screens_were_live = info.screens_live;
    {
        // AzaharFrame vrsize: global (the player's lenses), every frame, so a reset in Configure
        // shows at once. NudgeOffset writes it on this thread.
        const L::LensOffset offset = L::ClampedOffset(
            {Settings::values.vr_immersive_offset_x.GetValue(),
             Settings::values.vr_immersive_offset_y.GetValue()});
        if (!g.offset_logged || !(offset == g.offset)) {
            LOG_INFO(Frontend, "xr: immersive offset {:.1f} degrees right, {:.1f} up{}",
                     offset.right_deg, offset.up_deg, g.offset_logged ? " (changed)" : "");
            g.offset = offset;
            g.offset_logged = true;
        }
    }
    if (info.screens_live) {
        // AzaharFrame widerview: every frame too (Properties, or the D-pad in StepWiderView).
        const u32 wider = std::min(Settings::values.vr_immersive_wider_view_level.GetValue(),
                                   L::kMaxWiderLevel);
        if (wider != g.wider_level) {
            g.wider_level = wider;
            g.options.wider = L::WiderFactor(wider);
            LOG_INFO(Frontend, "xr: immersive: Wider View {:.1f}x (changed)", g.options.wider);
        }
    }
    if (info.screens_live) {
        // Every frame, so that a change in Configure or a game's Properties shows at once.
        const u32 shape = Settings::values.vr_top_aspect.GetValue();
        const float fov = g.tuning.fov_from_env
                              ? g.tuning.game_fov_y_deg
                              : static_cast<float>(Settings::values.vr_immersive_game_fov.GetValue());
        if (!g.shape_logged || shape != g.shape || fov != g.game_fov_y_deg) {
            g.shape = shape;
            g_top_aspect = AspectFor(shape);
            g.game_fov_y_deg = fov;
            LOG_INFO(Frontend, "xr: top screen shape {} ({:.3f}), game camera {:.0f} degrees high{}",
                     shape, g_top_aspect.load(), g.game_fov_y_deg,
                     g.shape_logged ? " (changed)" : "");
            g.shape_logged = true;
        }
    }
    g.mode = mode;
    g.live = info.screens_live && visible;
    g.view = LatestViewPose();

    const bool was_engaged = g.engaged;
    g.engaged = Decision().immersive;

    if (g.engaged && g.view.valid && (!g.have_ref || g.recentre)) {
        g.ref = {YawOnly(g.view.head.orientation), g.view.head.position};
        g.have_ref = true;
    }
    g.recentre = false;

    if (g.view.valid) {
        const XrVector3f between = Rotate(Conj(g.view.head.orientation),
                                          Sub(g.view.eye[1].position, g.view.eye[0].position));
        const float half = std::abs(between.x) / 2;
        g.half_ipd = (half > 0.02f && half < 0.045f) ? half : kDefaultHalfIpd;
        g.view_fov = L::BothEyes(EyeInHead(g.view.head, g.view.eye[0], g.view.fov[0]),
                                 EyeInHead(g.view.head, g.view.eye[1], g.view.fov[1]));
        if (!g.view_fov_logged) {
            g.view_fov_logged = true;
            LOG_INFO(Frontend,
                     "xr: immersive: both eyes see {:.1f} to {:.1f} degrees across, {:.1f} to "
                     "{:.1f} up (left eye {:.1f}/{:.1f}/{:.1f}/{:.1f})",
                     Deg(g.view_fov.left), Deg(g.view_fov.right), Deg(g.view_fov.down),
                     Deg(g.view_fov.up), Deg(g.view.fov[0].angleLeft),
                     Deg(g.view.fov[0].angleRight), Deg(g.view.fov[0].angleDown),
                     Deg(g.view.fov[0].angleUp));
        }
    }
    if (IsViewMode(mode)) {
        // Mode 2 shows one quad to both eyes: each eye sees it from half the IPD to the side.
        const float d = L::Clamped(g.head_place).distance;
        const float eye_shift = mode == 2 ? g.half_ipd / d : 0.0f;
        if (g.head_old_size && g.view_fov_logged) {
            // Saved by an earlier build, relative to its larger default: keep the screen's angle.
            g.head_old_size = false;
            g.head_place.size =
                L::SizeFromOldDefault(g.head_place.size, g.head_place, g.game_fov_y_deg,
                                      TopAspect(), g.view_fov, eye_shift);
            LogPlacement("converted from an earlier build", g.head_place, false);
            if (g.title_id) {
                g.store.Set(g.title_id, false, g.head_place);
                SaveStore();
            }
        }
        g.solved = L::SolveHeadLocked(g.head_place, g.game_fov_y_deg, TopAspect(), g.view_fov,
                                      eye_shift, g.options, g.offset);
    }

    VideoCore::Immersive::VRData data; // CitraVR's "off" values
    if (g.engaged) {
        data.mode = static_cast<s32>(mode);
        data.factor = mode < 2 ? kWideFactor : g.solved.factor;
        data.uoffset = -1;
        if (g.view.valid) {
            XrQuaternionf q = Normalized(Mul(Conj(g.ref.orientation), g.view.head.orientation));
            if (IsViewMode(mode)) {
                // The screen's pitch (and the gain of a resized screen) go into the game camera
                // too, so the world stays where it is (immersive_layout.h).
                const Input::Quatf c =
                    L::GameCameraRotation({q.x, q.y, q.z, q.w}, g.solved);
                q = {c.x, c.y, c.z, c.w};
            }
            const XrVector3f p =
                Rotate(Conj(g.ref.orientation), Sub(g.view.head.position, g.ref.position));
            // CitraVR: 10^multiplier x (the 3D slider / 10, an integer).
            const float scaler =
                std::pow(10.0f, static_cast<float>(
                                    Settings::values.vr_immersive_positional_game_scaler.GetValue())) *
                static_cast<float>(Settings::values.factor_3d.GetValue() / 10);
            FillInverseView(q, p, scaler, data.inv_view);
            data.game_pos_scaler = -scaler;
        }
    }
    VideoCore::Immersive::Publish(data);

    if (g.engaged != was_engaged) {
        if (g.engaged) {
            if (IsViewMode(mode)) {
                LOG_INFO(Frontend,
                         "xr: immersive mode {} on (factor {:.2f}, head-locked top screen "
                         "{:.0f} x {:.0f} degrees, {:.1f} degrees from straight ahead, {:.1f} "
                         "to the right, gain {:.2f}, yaw gain {:.2f}, size {:.2f})",
                         mode, data.factor, Deg(2 * std::atan(g.solved.tan_half_y * g.solved.aspect)),
                         Deg(2 * std::atan(g.solved.tan_half_y)), Deg(g.solved.pitch_rad),
                         -Deg(g.solved.yaw_rad), g.solved.gain, g.solved.gain_x,
                         L::Clamped(g.head_place).size);
            } else {
                LOG_INFO(Frontend, "xr: immersive mode {} on (factor {:.1f}, wide top screen)",
                         mode, data.factor);
            }
            if (IsViewMode(mode) && !Settings::values.use_hw_shader.GetValue() &&
                !g.hw_shader_warned) {
                g.hw_shader_warned = true;
                LOG_WARNING(Frontend, "xr: immersive mode {} needs hardware shaders (Graphics, "
                                      "Enable Hardware Shader); the view will not follow the head",
                            mode);
            }
        } else {
            LOG_INFO(Frontend, "xr: immersive mode off ({})",
                     mode == 0      ? "setting off"
                     : !g.live      ? "no game on view"
                     : g.focus_bottom ? "bottom screen in focus"
                     : g.menu_open  ? "in-game menu"
                                    : "bottom screen");
        }
    }
}

float DistanceTo(const XrPosef& pose) {
    const XrVector3f eye = g.view.valid ? g.view.head.position : XrVector3f{0, 0, 0};
    const XrVector3f d = Sub(pose.position, eye);
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

} // namespace

void Install() {
    g = State{};
    g_top_aspect = kAspect3ds;
    g.tuning = ReadTuning();
    g.game_fov_y_deg = g.tuning.game_fov_y_deg;
    g.head_place = g.wide_place = DefaultPlacement();
    VideoCore::Immersive::g_xr_active = true;
    g.callback_id = AddFrameCallback(OnFrame);
    LOG_INFO(Frontend,
             "xr: immersive: screen {:.2f} m ahead by default, game camera {}, {} space",
             g.tuning.distance,
             g.tuning.fov_from_env ? fmt::format("{:.0f} degrees high (environment)",
                                                 g.tuning.game_fov_y_deg)
                                   : std::string{"angle from the game's setting"},
             g.tuning.local_space ? "LOCAL" : "VIEW");
}

void Uninstall() {
    if (g.callback_id) {
        RemoveFrameCallback(g.callback_id);
        g.callback_id = 0;
    }
    // The XR thread has stopped: this thread is the only writer now.
    VideoCore::Immersive::Publish({});
    VideoCore::Immersive::g_xr_active = false;
}

float TopAspect() {
    return g_top_aspect.load();
}

ScreenLayers Screens(XrSpace local_space, XrSpace view_space, const ScreenPlacement& placement) {
    ScreenLayers out;
    out.top_space = local_space;
    out.top_pose[0] = out.top_pose[1] = placement.top_pose;
    out.top_size = placement.top_size;
    out.bottom_pose = placement.bottom_pose;
    out.bottom_size = placement.bottom_size;
    out.show_bottom = BottomShown();

    if (g.engaged && !IsViewMode(g.mode)) {
        // Mode 1: fixed in the room, ahead of where the head was when the game started (or the
        // last re-centre). The game's own frame, the middle third, keeps the angular size of
        // the normal top screen (whatever its shape) times the size; the rest is the wider view.
        const L::Placement p = L::Clamped(g.wide_place);
        const ScreenPlacement normal = DefaultScreenPlacement();
        const float normal_tan = normal.top_size.width / 2 / std::abs(normal.top_pose.position.z);
        const float width = 2 * p.distance * kWideFactor * normal_tan * p.size;
        // The Immersive Offset turns it about the rest pose too (immersive_layout.h).
        const float pitch = L::ViewMiddlePitch(g.view_fov) +
                            (p.pitch_deg + g.offset.up_deg) * L::kDegToRad;
        const float yaw = -g.offset.right_deg * L::kDegToRad;
        const Input::Posef rest = ToIn(g.ref);
        out.top_pose[0] = out.top_pose[1] = ToXr(L::WideQuadPose(rest, pitch, p.distance, yaw));
        out.top_size = {width, width / TopAspect()};
        out.top_pointable = false;
        // The bottom screen (when shown) below the wide screen's middle and nearer than it,
        // so it is drawn over it and never through it.
        const L::BottomQuad b = L::WideBottomQuad(rest, pitch, p.distance, yaw);
        out.bottom_pose = ToXr(b.pose);
        out.bottom_size = {b.width, b.height};
    } else if (g.engaged) {
        // Modes 2 and 3: head-locked. Mode 3 centres each eye's screen on that eye (CitraVR
        // GameSurfaceLayer.cpp GetTopPanelFromHeadPose), mode 2 uses one pose for both. The
        // size, pitch and distance come from the placement (immersive_layout.h).
        out.top_size = {g.solved.width(), g.solved.height()};
        for (int eye = 0; eye < 2; eye++) {
            const float x = g.mode == 3 ? (eye == 0 ? -g.half_ipd : g.half_ipd) : 0.0f;
            const XrPosef in_view = ToXr(L::HeadLockedQuadPose(g.solved, x));
            if (g.tuning.local_space && g.view.valid) {
                out.top_space = local_space;
                out.top_pose[eye] = Compose(g.view.head, in_view);
            } else {
                out.top_space = view_space;
                out.top_pose[eye] = in_view;
            }
        }
        out.top_pointable = false;
        out.show_bottom = false;
    }
    const float top_distance = out.top_space == view_space ? g.solved.distance
                                                           : DistanceTo(out.top_pose[0]);
    out.bottom_first = DistanceTo(out.bottom_pose) > top_distance;
    return out;
}

bool CanToggleBottom() {
    return g.live && (g.mode != 0 || g.hide_bottom);
}

void SetMenuOpen(bool open) {
    g.menu_open = open;
}

void ToggleBottom() {
    if (!CanToggleBottom()) {
        return;
    }
    g.bottom_toggled = !g.bottom_toggled;
    LOG_INFO(Frontend, "xr: bottom screen {}", BottomShown() ? "shown" : "hidden");
}

void SetBottomFocused(bool focused) {
    if (focused != g.focus_bottom) {
        g.focus_bottom = focused;
        g.adjuster.Cancel();
    }
}

bool HoldingScreen() {
    return g.adjuster.Active();
}

bool NudgeOffset(int right_steps, int up_steps) {
    if (!g.adjuster.Active() || !g.engaged || (right_steps == 0 && up_steps == 0)) {
        return false;
    }
    const L::LensOffset before = g.offset;
    const L::LensOffset after = L::NudgedOffset(before, right_steps, up_steps);
    // The toast for the axis pressed, also at the limit (where nothing changes).
    ShowToastText(L::OffsetToast(after, right_steps != 0));
    if (after == before) {
        return false;
    }
    g.offset = after;
    Settings::values.vr_immersive_offset_x.SetValue(after.right_deg);
    Settings::values.vr_immersive_offset_y.SetValue(after.up_deg);
    LOG_INFO(Frontend, "xr: immersive offset nudged to {:.1f} degrees right, {:.1f} up",
             after.right_deg, after.up_deg);
    return true;
}

WiderStep StepWiderView(int dir) {
    if (!g.adjuster.Active() || !g.engaged || dir == 0) {
        return WiderStep::None;
    }
    if (!IsViewMode(g.mode)) {
        ShowToastText("Wider View: Full Modes Only");
        return WiderStep::None;
    }
    const u32 before = g.wider_level;
    const u32 after = static_cast<u32>(
        std::clamp(static_cast<int>(before) + (dir > 0 ? 1 : -1), 0,
                   static_cast<int>(L::kMaxWiderLevel)));
    ShowToastText(L::WiderToast(after)); // also at the ends, so the press shows
    if (after == before) {
        return WiderStep::None;
    }
    // The screen grows or shrinks with the view the game draws: the picture keeps its scale.
    const float from = L::WiderFactor(before);
    const float to = L::WiderFactor(after);
    g.head_place.size = L::SizeForWider(g.head_place.size, from, to);
    g.head_old_size = false;
    g.wider_level = after;
    g.options.wider = to;
    auto& level = Settings::values.vr_immersive_wider_view_level;
    if (level.UsingGlobal()) {
        level.SetGlobal(false); // per game, as in the game's Properties
    }
    level.SetValue(after);
    if (g.title_id) {
        g.store.Set(g.title_id, false, g.head_place);
        SaveStore();
    }
    LOG_INFO(Frontend, "xr: immersive: Wider View {:.1f}x by the D-pad, size {:.2f}", to,
             g.head_place.size);
    return WiderStep::Changed;
}

std::optional<float> SavedHeadSize(uint64_t title_id) {
    L::Store store;
    std::string text;
    if (title_id == 0 || FileUtil::ReadFileToString(true, PlacementPath(), text) == 0) {
        return std::nullopt;
    }
    store.Parse(text);
    const auto p = store.Get(title_id, false);
    if (!p || store.OldSize(title_id)) {
        return std::nullopt; // an earlier build's size is not comparable
    }
    return p->size;
}

void OnResetPanels() {
    // AzaharFrame controls: one reset rule. The view is re-centred and the immersive screen of
    // this mode goes back to its default placement, held by a grip or not.
    g.recentre = true;
    if (g.mode == 0 || !g.live) {
        return;
    }
    g.adjuster.Cancel();
    const bool wide = !IsViewMode(g.mode);
    L::Placement& p = wide ? g.wide_place : g.head_place;
    p = wide ? DefaultPlacement() : DefaultHeadPlacement();
    if (!wide) {
        g.head_old_size = false;
    }
    if (g.title_id && g.store.Get(g.title_id, wide)) {
        g.store.Erase(g.title_id, wide);
        SaveStore();
    }
    LogPlacement("reset", p, wide);
}

void OnGameInput(Input::InputFrame& in) {
    const float dt = g.last_input_ns && in.time_ns > g.last_input_ns
                         ? static_cast<float>(in.time_ns - g.last_input_ns) * 1e-9f
                         : 0.0f;
    g.last_input_ns = in.time_ns;
    // The immersive screen on view: the grips hold it (mode 1 with the bottom screen shown too,
    // whose place follows the wide screen). Not while it is paused: the normal screens are
    // grabbed as usual then.
    const bool enabled = g.engaged && g.view.valid;
    const bool wide = !IsViewMode(g.mode);
    L::Placement& place = wide ? g.wide_place : g.head_place;
    // Modes 2 and 3 move with the head; mode 1 is fixed around the rest pose.
    const Input::Posef frame = ToIn(wide ? g.ref : g.view.head);
    const L::Adjuster::Result r = g.adjuster.Update(in, frame, dt, enabled, place);
    if (enabled) {
        for (auto& hand : in.hand) {
            hand.grip = 0.0f; // the mapper grabs nothing
        }
        if (r.active) {
            in.hand[static_cast<std::size_t>(r.hand)].stick = {};
        }
    }
    if (r.released) {
        LogPlacement("moved", place, wide);
        if (!wide) {
            g.head_old_size = false;
        }
        if (g.title_id) {
            g.store.Set(g.title_id, wide, place);
            SaveStore();
        }
    }
}

} // namespace FrameXr::Immersive

namespace FrameXr {

void SetGameTitleId(uint64_t title_id) {
    Immersive::g_title_id = title_id;
}

} // namespace FrameXr
