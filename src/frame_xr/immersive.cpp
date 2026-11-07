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
#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/immersive.h"
#include "frame_xr/input/input_frame.h"
#include "video_core/vr_immersive.h"

namespace FrameXr::Immersive {

namespace {

// CitraVR vr_main.cpp:84 immersiveScaleFactor: 3.0 for mode 1, 1.4 for modes 2 and 3.
constexpr float kWideFactor = 3.0f;
constexpr float kFullFactor = 1.4f;
// CitraVR puts the top screen 0.5 m ahead in immersive mode (vr_main.cpp:413 and
// GameSurfaceLayer.cpp kSuperImmersiveRadius).
constexpr float kDefaultDistance = 0.5f;
// The vertical field of view a game's camera is assumed to have (Ocarina of Time uses 60
// degrees); "Immersive Game FOV" (vr_immersive_game_fov) replaces it per game. The head-locked
// screen shows it 1.4x wider, so that turning the head turns the game camera by the same angle
// as the view moves (the world stays put).
constexpr float kDefaultGameFovY = 60.0f;
// Half of rasterizer_accelerated.cpp's VR_IPD, used when the eye poses are not known.
constexpr float kDefaultHalfIpd = 0.065f / 2.0f;
// The top screen's width over its height: the 3DS's 400x240, and the two wide shapes of "VR
// Screen Shape" (vr_top_aspect 0, 1, 2).
constexpr float kAspect3ds = 400.0f / 240.0f;
constexpr float kAspectWide = 16.0f / 9.0f;
constexpr float kAspectUltrawide = 21.0f / 9.0f;

// The latched aspect (TopAspect). Outside State: it is read from other threads.
std::atomic<float> g_top_aspect{kAspect3ds};

float AspectFor(u32 shape) {
    switch (shape) {
    case 1:
        return kAspectWide;
    case 2:
        return kAspectUltrawide;
    default:
        return kAspect3ds;
    }
}

struct Tuning {
    float distance = kDefaultDistance;
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
    t.distance = EnvFloat("AZAHARFRAME_IMMERSIVE_DISTANCE", kDefaultDistance, 0.2f, 5.0f);
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

// ---- State --------------------------------------------------------------------------

struct State {
    int callback_id = 0;
    Tuning tuning;
    // XR thread from here on.
    u32 mode = 0;
    bool live = false;           ///< a game shows its screens and the session is visible
    bool engaged = false;        ///< immersive this frame
    bool bottom_toggled = false; ///< CitraVR's lower menu toggle (starts off in immersive mode)
    bool menu_open = false;      ///< the in-game menu shows (immersive modes 2 and 3 pause)
    bool bottom_focus = false;   ///< the bottom screen is the main one (screen swap): all modes pause
    bool hide_bottom = false;    ///< "Hide Bottom Screen" (per game), read when the game starts
    float game_fov_y_deg = kDefaultGameFovY; ///< "Immersive Game FOV" (per game), read at start
    bool screens_were_live = false;
    bool prev_click = false;
    bool recentre = false;
    bool have_ref = false;
    XrPosef ref{{0, 0, 0, 1}, {0, 0, 0}}; ///< head position and heading when immersive began
    ViewPose view;
    float half_ipd = kDefaultHalfIpd;
    bool hw_shader_warned = false;
};

State g;

bool IsViewMode(u32 mode) {
    return mode >= 2;
}

/// "Hide Bottom Screen" is on and the bottom screen was not brought back with the bumper chord.
bool BottomHiddenByOption() {
    return g.hide_bottom && !g.bottom_toggled;
}

void OnFrame(const FrameInfo& info) {
    const u32 mode = Settings::values.vr_immersive_mode.GetValue();
    const bool visible =
        info.state == XR_SESSION_STATE_VISIBLE || info.state == XR_SESSION_STATE_FOCUSED;
    if (!info.screens_live) {
        // No game: the next one starts with the bottom screen hidden, as in CitraVR.
        g.bottom_toggled = false;
        g.have_ref = false;
    } else if (!g.screens_were_live) {
        // A game just started: its per-game settings are loaded; read "Hide Bottom Screen" now.
        g.hide_bottom = Settings::values.vr_hide_bottom_screen.GetValue();
        LOG_INFO(Frontend, "xr: bottom screen {} at start", g.hide_bottom ? "hidden" : "shown");
        // The screen shape and the game camera's angle, per game as well.
        const u32 shape = Settings::values.vr_top_aspect.GetValue();
        g_top_aspect = AspectFor(shape);
        g.game_fov_y_deg =
            g.tuning.fov_from_env
                ? g.tuning.game_fov_y_deg
                : static_cast<float>(Settings::values.vr_immersive_game_fov.GetValue());
        LOG_INFO(Frontend, "xr: top screen shape {} ({:.3f}), game camera {:.0f} degrees high",
                 shape, g_top_aspect.load(), g.game_fov_y_deg);
    }
    g.screens_were_live = info.screens_live;
    g.mode = mode;
    g.live = info.screens_live && visible;
    g.view = LatestViewPose();

    const bool was_engaged = g.engaged;
    g.engaged = mode != 0 && g.live && !g.bottom_focus &&
                (!IsViewMode(mode) || (!g.bottom_toggled && !g.menu_open && g.view.valid));

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
    }

    VideoCore::Immersive::VRData data; // CitraVR's "off" values
    if (g.engaged) {
        data.mode = static_cast<s32>(mode);
        data.factor = mode < 2 ? kWideFactor : kFullFactor;
        data.uoffset = -1;
        if (g.view.valid) {
            const XrQuaternionf q =
                Normalized(Mul(Conj(g.ref.orientation), g.view.head.orientation));
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
            LOG_INFO(Frontend, "xr: immersive mode {} on (factor {:.1f}, {})", mode, data.factor,
                     IsViewMode(mode) ? "head-locked top screen" : "wide top screen");
            if (IsViewMode(mode) && !Settings::values.use_hw_shader.GetValue() &&
                !g.hw_shader_warned) {
                g.hw_shader_warned = true;
                LOG_WARNING(Frontend, "xr: immersive mode {} needs hardware shaders (Graphics, "
                                      "Enable Hardware Shader); the view will not follow the head",
                            mode);
            }
        } else {
            LOG_INFO(Frontend, "xr: immersive mode off ({})",
                     mode == 0 ? "setting off" : (!g.live ? "no game on view" : "bottom screen"));
        }
    }
}

} // namespace

void Install() {
    g = State{};
    g_top_aspect = kAspect3ds;
    g.tuning = ReadTuning();
    g.game_fov_y_deg = g.tuning.game_fov_y_deg;
    VideoCore::Immersive::g_xr_active = true;
    g.callback_id = AddFrameCallback(OnFrame);
    LOG_INFO(Frontend,
             "xr: immersive: screen {:.2f} m ahead, game camera {}, {} space", g.tuning.distance,
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
    out.show_bottom = !BottomHiddenByOption();
    if (!g.engaged) {
        return out;
    }
    const float d = g.tuning.distance;

    if (!IsViewMode(g.mode)) {
        // Mode 1: fixed in the room, ahead of where the head was when the game started (or the
        // last re-centre). The game's own frame, the middle third, keeps the angular size of
        // the normal top screen (whatever its shape); the rest is the wider view.
        const ScreenPlacement normal = DefaultScreenPlacement();
        const float normal_tan = normal.top_size.width / 2 / std::abs(normal.top_pose.position.z);
        const float width = 2 * d * kWideFactor * normal_tan;
        const XrPosef ahead{{0, 0, 0, 1}, {0.0f, 0.0f, -d}};
        out.top_pose[0] = out.top_pose[1] = Compose(g.ref, ahead);
        out.top_size = {width, width / TopAspect()};
        out.show_bottom = g.bottom_toggled;
        return out;
    }

    // Modes 2 and 3: head-locked. Mode 3 centres each eye's screen on that eye (CitraVR
    // GameSurfaceLayer.cpp GetTopPanelFromHeadPose), mode 2 uses one pose for both.
    constexpr float kDegToRad = 3.14159265f / 180.0f;
    // The game draws its view with its vertical angle (Immersive Game FOV) and the aspect of the
    // top screen's shape, so the horizontal half angle has tan = aspect x tan(fov_y / 2). The
    // quad is as wide as it is high times that aspect, which keeps both angles right.
    const float height = 2 * d * kFullFactor * std::tan(g.game_fov_y_deg * kDegToRad / 2);
    out.top_size = {height * TopAspect(), height};
    for (int eye = 0; eye < 2; eye++) {
        const float x = g.mode == 3 ? (eye == 0 ? -g.half_ipd : g.half_ipd) : 0.0f;
        const XrPosef in_view{{0, 0, 0, 1}, {x, 0.0f, -d}};
        if (g.tuning.local_space && g.view.valid) {
            out.top_space = local_space;
            out.top_pose[eye] = Compose(g.view.head, in_view);
        } else {
            out.top_space = view_space;
            out.top_pose[eye] = in_view;
        }
    }
    out.show_bottom = false;
    return out;
}

bool CanToggleBottom() {
    return g.live && (g.mode != 0 || g.hide_bottom);
}

void SetMenuOpen(bool open) {
    g.menu_open = open;
}

void SetBottomFocus(bool focus) {
    g.bottom_focus = focus;
}

void ToggleBottom() {
    if (!CanToggleBottom()) {
        return;
    }
    g.bottom_toggled = !g.bottom_toggled;
    LOG_INFO(Frontend, "xr: bottom screen {}", g.bottom_toggled ? "shown" : "hidden");
}

void OnGameInput(const Input::InputFrame& in, bool& top_pointable, bool& bottom_pointable) {
    const bool click = in.hand[1].stick_click;
    if (click && !g.prev_click) {
        g.recentre = true;
    }
    g.prev_click = click;

    if (g.engaged) {
        top_pointable = false;
        if (IsViewMode(g.mode) || !g.bottom_toggled) {
            bottom_pointable = false;
        }
    }
    // Hidden by the setting (no touch, no grab); immersive modes above already hide it.
    if (BottomHiddenByOption()) {
        bottom_pointable = false;
    }
}

} // namespace FrameXr::Immersive
