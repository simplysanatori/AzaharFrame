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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include <fmt/format.h>
#include "common/test_hooks.h"
#include "common/atomic_file.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/frame_log_gate.h" // AzaharFrame integ8
#include "common/frame_test_switches.h" // AzaharFrame dbl
#include "common/frame_vr_depth.h" // AzaharFrame stereo
#include "common/settings.h"
#include "frame_xr/auto_shape.h" // AzaharFrame fixes7
#include "frame_xr/immersive.h"
#include "frame_xr/immersive_layout.h"
#include "frame_xr/immersive_names.h" // AzaharFrame kiu
#include "frame_xr/ipd_smoother.h" // AzaharFrame ipd
#include "frame_xr/input/input_frame.h"
#include "frame_xr/presets.h" // AzaharFrame immersive: preset screen sizes
#include "frame_xr/test_switches.h" // AzaharFrame integ9
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
// Half of rasterizer_accelerated.cpp's VR_IPD, used until the headset's IPD is known (Ipd).
constexpr float kDefaultHalfIpd = Ipd::kFallbackM / 2.0f;
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
    case AutoShape::kAutomatic: // AzaharFrame fixes7: the measured shape (auto_shape.h)
        return AutoShape::Aspect();
    default:
        return kAspect3ds;
    }
}

struct Tuning {
    float distance = kDefaultDistance; ///< the default distance (a saved placement wins), mode 1
    /// AzaharFrame immersive2: modes 2 and 3 (L::kDefaultHeadDistance; the variable sets both).
    float head_distance = L::kDefaultHeadDistance;
    float game_fov_y_deg = kDefaultGameFovY;
    bool fov_from_env = false; ///< AZAHARFRAME_IMMERSIVE_FOV is set: it wins over the setting
    bool local_space = false; ///< head-locked screen placed in LOCAL from the head pose
    /// AzaharFrame depth: size the head-locked screen with the game's measured projection when
    /// Match Game Camera is on (AZAHARFRAME_IMMERSIVE_MEASURE=0 forces it off; the FOV variable
    /// wins over it too).
    bool measure = true;
    /// AzaharFrame immersive: AZAHARFRAME_IMMERSIVE_WIDER (0 to 4) wins over the Wider View
    /// setting (-1: unset), for comparing levels without changing a game's settings.
    int wider_level = -1;
    /// AzaharFrame doubling: AZAHARFRAME_IMMERSIVE_ANCHOR (default 1): the head-locked screen of
    /// modes 2 and 3 is placed where the head was when its picture was drawn (LOCAL space), not
    /// where the head is at display time. 0: VIEW space as before (or LOCAL from the newest head
    /// pose with AZAHARFRAME_IMMERSIVE_LOCAL=1).
    bool anchor = true;
    /// AZAHARFRAME_IMMERSIVE_ANCHOR_MAX_MS (default 250, 50..2000): a picture older than this
    /// (the game stopped drawing: loading, a stall) goes back to the head, so the screen never
    /// stays behind out of view.
    float anchor_max_ms = 250.0f;
    /// AzaharFrame turns: AZAHARFRAME_IMMERSIVE_ANCHOR_EYE (default 1): the anchored screen keeps
    /// the picture's orientation about where the eye is now (L::AnchoredQuadPose), so the eyes
    /// moving with a head turn (about the neck) no longer slide it against the scenery; 0: at the
    /// picture's head pose, position too.
    bool anchor_eye = true;
    /// AzaharFrame dbl: AZAHARFRAME_IMMERSIVE_ANCHOR_DEPTH (a test switch): the depth whose
    /// scenery the anchored screen keeps still while a picture is shown again as the eyes move
    /// (L::AnchorFollow): "far" (default, as ANCHOR_EYE on), "screen" (as ANCHOR_EYE off),
    /// "focus" (the game's zero-parallax plane, measured: Mario Kart 7's kart, Link), "mid"
    /// (halfway between far and focus: each steps by half), or metres.
    /// anchor_depth_m: -1 far, 0 screen, -2 focus, -3 mid, else metres.
    float anchor_depth_m = -1.0f;
    /// AzaharFrame fovonce: AZAHARFRAME_IMMERSIVE_FOV_TRACE=1 logs each frame's camera angles
    /// while they move (a measurement, the angle followed, the eased ones), for mocks.
    bool fov_trace = false;
    /// AzaharFrame oot: AZAHARFRAME_IMMERSIVE_MENU_EXACT (default 1): mode 2 shows a screen of
    /// the game's own in front of its world (an item screen) without the Render Margin
    /// (L::MenuExact); 0: always with it, as before.
    bool menu_exact = true;
    /// AzaharFrame integ9: AZAHARFRAME_IMMERSIVE_TAG_OFFSET (a test switch, live): the picture's
    /// pose tag one latch earlier (-1) or later (+1) (ContentTrack::SetTagOffset).
    int tag_offset = 0;
    /// AzaharFrame integ9: AZAHARFRAME_IMMERSIVE_POSITION_SCALE (a test switch, live): the head
    /// position's move in the game camera times this (0: the camera turns only), 3D Depth and the
    /// game's two eyes unchanged.
    float position_scale = 1.0f;
    /// AzaharFrame integ9: AZAHARFRAME_IMMERSIVE_WORLD_TAG (0: tag by the latch, as up to 1.2).
    bool world_tag = true;
    /// AzaharFrame heldquad: AZAHARFRAME_IMMERSIVE_HOLD (a test switch, live): "warp" (default)
    /// holds a picture shown again as the game would draw it now (L::HeldQuad), "rigid" fixed in
    /// the room where it was drawn (L::AnchoredQuadPose), as up to 1.2.
    bool hold_warp = true;
    /// AzaharFrame heldquad: AZAHARFRAME_IMMERSIVE_ANCHOR_LAG (a test switch, live): -1, 0, +1
    /// (TestSwitches::AnchorLagFrom).
    int anchor_lag = 0;
};

// AzaharFrame integ9: through the live test-switch file (common/frame_test_switches.h), so a
// switch changed in the file while running is read with its new value.
std::optional<std::string> Switch(const char* name) {
    return Common::FrameTestSwitches::Value(name);
}

bool SwitchIs(const char* name, const char* value) {
    const std::optional<std::string> v = Switch(name);
    return v && *v == value;
}

float EnvFloat(const char* name, float fallback, float lo, float hi) {
    const std::optional<std::string> s = Switch(name);
    if (!s || s->empty()) {
        return fallback;
    }
    const char* v = s->c_str();
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
    t.head_distance = EnvFloat("AZAHARFRAME_IMMERSIVE_DISTANCE", L::kDefaultHeadDistance,
                               L::kMinDistance, L::kMaxDistance);
    const std::optional<std::string> fov_env = Switch("AZAHARFRAME_IMMERSIVE_FOV");
    t.fov_from_env = fov_env && !fov_env->empty();
    t.game_fov_y_deg = EnvFloat("AZAHARFRAME_IMMERSIVE_FOV", kDefaultGameFovY, 20.0f, 120.0f);
    t.local_space = SwitchIs("AZAHARFRAME_IMMERSIVE_LOCAL", "1");
    t.measure = !SwitchIs("AZAHARFRAME_IMMERSIVE_MEASURE", "0");
    t.anchor = !SwitchIs("AZAHARFRAME_IMMERSIVE_ANCHOR", "0");
    t.anchor_max_ms = EnvFloat("AZAHARFRAME_IMMERSIVE_ANCHOR_MAX_MS", 250.0f, 50.0f, 2000.0f);
    t.anchor_eye = !SwitchIs("AZAHARFRAME_IMMERSIVE_ANCHOR_EYE", "0");
    if (const std::optional<std::string> depth = Switch("AZAHARFRAME_IMMERSIVE_ANCHOR_DEPTH");
        depth && !depth->empty()) {
        const std::string& v = *depth;
        t.anchor_depth_m = v == "far"    ? -1.0f
                           : v == "screen" ? 0.0f
                           : v == "focus"  ? -2.0f
                           : v == "mid"    ? -3.0f
                                           : std::clamp(static_cast<float>(std::atof(v.c_str())),
                                                        0.05f, 1000.0f);
    }
    t.fov_trace = SwitchIs("AZAHARFRAME_IMMERSIVE_FOV_TRACE", "1");
    t.menu_exact = !SwitchIs("AZAHARFRAME_IMMERSIVE_MENU_EXACT", "0");
    if (const std::optional<std::string> wider = Switch("AZAHARFRAME_IMMERSIVE_WIDER");
        wider && !wider->empty()) {
        t.wider_level = static_cast<int>(EnvFloat("AZAHARFRAME_IMMERSIVE_WIDER", 0.0f, 0.0f,
                                                  static_cast<float>(L::kMaxWiderLevel)));
    }
    t.tag_offset = TestSwitches::TagOffsetFrom(Switch("AZAHARFRAME_IMMERSIVE_TAG_OFFSET"));
    t.position_scale =
        TestSwitches::PositionScaleFrom(Switch("AZAHARFRAME_IMMERSIVE_POSITION_SCALE"));
    const std::optional<std::string> world_tag = Switch("AZAHARFRAME_IMMERSIVE_WORLD_TAG");
    t.world_tag = !(world_tag && !world_tag->empty() && (*world_tag)[0] == '0');
    t.hold_warp = TestSwitches::HoldWarpFrom(Switch("AZAHARFRAME_IMMERSIVE_HOLD"));
    t.anchor_lag = TestSwitches::AnchorLagFrom(Switch("AZAHARFRAME_IMMERSIVE_ANCHOR_LAG"));
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

// AzaharFrame doubling: the head pose a picture was drawn with, as an XrPosef.
XrPosef ToXr(const VideoCore::XrContentPose& c) {
    return {{c.orientation[0], c.orientation[1], c.orientation[2], c.orientation[3]},
            {c.position[0], c.position[1], c.position[2]}};
}

/// The angle between two orientations, in degrees.
float AngleDeg(const XrQuaternionf& a, const XrQuaternionf& b) {
    const float d = std::abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
    return 2.0f * std::acos(std::min(d, 1.0f)) * 57.29578f;
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

// AzaharFrame doubling: the picture's head pose when the immersive screen may be anchored to it
// now: a Full mode is engaged, the picture was drawn in it, after the last recenter, and it is
// not older than the limit (`display` is this frame's display time). AzaharFrame heldquad:
// `held`, when given, gets that pose's game camera from the pose ring (nullptr: not in it).
bool AnchorPose(XrTime display, XrPosef& head, const Layout::HeldPose** held = nullptr);

// The title id of the game being started (SetGameTitleId, GUI thread).
std::atomic<uint64_t> g_title_id{0};
// AzaharFrame fovonce: Set Once chosen again (MeasureCameraAgain, any thread), and the angle Set
// Once wrote, for the game's file (TakeMeasuredFov; 0 = none).
std::atomic<uint32_t> g_remeasure{0};
std::atomic<int> g_fov_written{0};

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
    /// AzaharFrame depth: the game camera as measured from its projection (modes 2 and 3);
    /// AzaharFrame immersive: split into the screen's camera and the live angle (CameraFilter).
    L::CameraFilter camera;
    int64_t camera_stamp = 0; ///< when the last measurement read was made
    /// AzaharFrame fovonce: Match Game Camera's Set Once (the estimate, its measuring time) and
    /// Always (the angle followed); both eased (the screen's camera and the live angle).
    L::FovEstimate estimate;
    int64_t estimate_tick_ms = 0;
    u32 match_seen = 0;          ///< the choice last frame (Set Once chosen: measure anew)
    uint32_t remeasure_seen = 0; ///< g_remeasure handled
    L::FovFollow follow;
    L::CameraAngles angles; ///< what the solver gets: Off as is, Set Once eased, Always slewed
    float traced[4]{}; ///< the trace's last line (AZAHARFRAME_IMMERSIVE_FOV_TRACE)
    int64_t game_start_ms = 0;
    bool shape_logged = false; ///< both logged for this game
    /// Render Margin (was Wider View), Horizontal Scale and Vertical Scale (AzaharFrame kiu: were
    /// Narrow Screen and Narrow Height), per game, every frame.
    L::Options options;
    u32 wider_level = 0; ///< AzaharFrame widerview: the level options.wider comes from
    int scale_x = 0;     ///< AzaharFrame kiu: the scales (percent) options.squeeze(_y) are
    int scale_y = 0;
    bool screens_were_live = false;
    /// AzaharFrame immersive: when `ref` is taken (Layout::RestPose; SteamVR's recenter moves the
    /// LOCAL space at its change time, and a head pose located before it is still in the old
    /// space).
    L::RestPose rest;
    XrPosef ref{{0, 0, 0, 1}, {0, 0, 0}}; ///< head position and heading when immersive began
    ViewPose view;
    float half_ipd = kDefaultHalfIpd;
    /// AzaharFrame ipd: the headset's IPD, from the located eyes; half_ipd is half of it.
    Ipd::Smoother ipd;
    Ipd::Notice ipd_notice;
    bool ipd_started = false;   ///< the saved IPD read (the first frame)
    bool ipd_had_saved = false; ///< an IPD was saved before this run: never a toast
    Ipd::Saver ipd_saver;
    bool ipd_saved_now = false; ///< TakeIpdSaved
    bool hw_shader_warned = false;
    // The placement (immersive_layout.h): per game, head-locked (modes 2, 3) and wide (mode 1).
    uint64_t title_id = 0;
    L::Store store;
    bool store_loaded = false;
    L::Placement head_place;
    L::Placement wide_place;
    L::ViewFov view_fov;
    /// AzaharFrame jump: the view the screen is fitted to, held steady (ViewSettle); view_fov
    /// and half_ipd are its values.
    L::ViewSettle view_settle;
    L::ViewFov view_raw; ///< this frame's view as the runtime located it
    bool view_raw_valid = false;
    int view_fov_changes = 0; ///< the settled view's changes (the log line's note)
    L::HeadLocked solved;
    /// AzaharFrame stereo: mode 2's cap on the game's 3D slider (replaced by natural 3D).
    L::StereoCap stereo_cap;
    /// AzaharFrame natural: mode 2's slider (3D Depth x the natural limit); the game's K in use
    /// (eased); the Full modes' screen distance from World Scale (eased, at most a factor 4 a
    /// second); the leaning made from both; mode 3's own stereo.
    L::NaturalSlider natural;
    L::EyePerFarInUse eye_k;
    L::RatioRamp full_distance{1.3862944f}; // ln 4
    float full_distance_target = -1.0f; ///< where full_distance is going (m)
    float leaning = -1.0f; ///< game units per metre, this frame
    /// World Scale's migration from 1.2.0 this game run: 0 not looked at yet, 1 done (or not
    /// needed), 2 mode 3 waiting for the game's eyes (the saved distance and 1.2.0's scale hold).
    int world_migration = 0;
    bool k_saved = false;        ///< the game's K went to its file this run
    bool grab_active = false;    ///< the grips hold the immersive screen
    float grab_size = -1.0f;     ///< the screen's size when they took it
    long world_toasted = -1;     ///< the World Scale the grip's toast shows (rounded)
    L::Mode3Stereo mode3;
    float quad_half = L::kQuadsOnEyes; ///< mode 3's eye quads from the middle (NaN: on the eyes)
    float natural_logged_limit = -1.0f;
    uint32_t natural_logged_depth = 1000;
    int natural_logged_source = -1; ///< L::EyePerFarInUse::Source
    float natural_logged_leaning = -1.0f;
    float natural_logged_world = -1.0f;
    /// AzaharFrame oot: mode 2's menu rule (L::MenuExact), and whether it was last on (the log).
    L::MenuExact menu_exact;
    bool menu_exact_on = false;
    float stereo_cap_logged = -1.0f; ///< the cap last logged (below 0: none)
    /// AzaharFrame integ8: the "stereo far" line, on a meaningful change, else every 10 s: what it
    /// last said, and the far angle's range over the seconds since.
    Common::FrameLog::ChangeGate far_gate;
    float far_said_deg = -1.0f, far_said_allow = -1.0f;
    bool far_said_diverges = false;
    float far_min_deg = 0.0f, far_max_deg = 0.0f;
    int far_secs = 0;
    L::Adjuster adjuster;
    /// The global Immersive Offset (vr_immersive_offset_x, _y), read every frame.
    L::LensOffset offset;
    bool offset_logged = false;
    int64_t last_input_ns = 0;
    /// AzaharFrame doubling: SteamVR's last recenter (the LOCAL space moved then): a picture
    /// drawn with a head pose located before it is not anchored.
    XrTime recentre_time = 0;
    /// AzaharFrame doubling: per second, while a Full mode is engaged: how old the picture on
    /// view is against this frame's display time, how far the head turned since it was drawn
    /// (what a head-locked screen shows as the world swinging with the head), and how many
    /// frames the screen was anchored.
    struct AgeStats {
        int64_t since_ns = 0;
        uint32_t frames = 0, anchored = 0, no_pose = 0;
        double age_sum_ms = 0, age_max_ms = 0, turn_sum_deg = 0, turn_max_deg = 0;
        uint32_t pictures = 0; ///< different pictures seen (by their pose time)
        int64_t last_picture = 0;
        uint32_t run = 0;           ///< display frames the current picture has been on view
        uint32_t runs[4]{};         ///< pictures on view for 1, 2, 3, 4 or more display frames
        /// AzaharFrame jump: the middle of the view as located each frame (lowest, highest),
        /// and the frames whose view was more than kViewSettleChangeDeg from the fitted one.
        float mid_min_deg = 1e9f, mid_max_deg = -1e9f;
        uint32_t view_off = 0;
        /// AzaharFrame turns: in the anchored frames, how far the screen's middle is, seen from
        /// the eye now, from the direction its picture was drawn in (what scenery far away is
        /// off by), and how far that moved since the frame before (a double image's spacing).
        uint32_t slip_frames = 0, step_frames = 0;
        double slip_sum_deg = 0, slip_max_deg = 0, step_sum_deg = 0, step_max_deg = 0;
        /// AzaharFrame turns: the head from the rest pose (yaw, pitch) and the game camera given
        /// (yaw, pitch), lowest and highest, degrees.
        float head_yaw[2]{1e9f, -1e9f}, head_pitch[2]{1e9f, -1e9f};
        float cam_yaw[2]{1e9f, -1e9f}, cam_pitch[2]{1e9f, -1e9f};
        uint32_t cam_frames = 0;
        /// AzaharFrame heldquad: anchored frames whose picture's camera was in the pose ring
        /// (held as the game would draw it now), and those whose was not (held rigid).
        uint32_t held = 0, held_missed = 0;
    } age;
    /// AzaharFrame turns: the last anchored frame's offsets (degrees across, up; per eye).
    bool slip_prev_valid = false;
    float slip_prev[2][2]{};
    /// AzaharFrame heldquad: each display frame's head pose and game camera (OnFrame), for
    /// holding a picture shown again as the game would draw it now (Screens, L::HeldQuad).
    L::PoseRing poses;
    /// AzaharFrame heldquad: AZAHARFRAME_IMMERSIVE_ANCHOR_LAG's count of the refreshes each
    /// picture (its release serial) was on view: the display frame last seen and the picture it
    /// showed at its last look (Screens runs more than once a frame; the frame loop's, under the
    /// release lock, is the last), and up to the frame before it, the picture counted, its
    /// refreshes and those of the picture before it.
    struct Lag {
        int64_t frame = 0;
        uint64_t frame_serial = 0;
        uint64_t serial = 0;
        uint32_t run = 0, last_run = 0;
    } lag;
};

State g;

bool IsViewMode(u32 mode) {
    return mode >= 2;
}

// AzaharFrame depth: the game camera's projection, measured on the emulation thread
// (vr_view_matrix.h) every quarter second, in the last second and since this game started.
// AzaharFrame immersive: CameraFilter splits it into the camera the screen is sized and shaped
// with (it changes only for a new scene, never for a zoom) and the live angle the gain follows.
// Gone for a second (a menu, the bottom screen in focus), the last one is kept until the next.
// AzaharFrame widerview: the level in use (the setting, or AZAHARFRAME_IMMERSIVE_WIDER).
u32 WiderLevel() {
    if (g.tuning.wider_level >= 0) {
        return static_cast<u32>(g.tuning.wider_level);
    }
    return std::min(Settings::values.vr_immersive_wider_view_level.GetValue(), L::kMaxWiderLevel);
}

// AzaharFrame fovonce: Match Game Camera's choice in effect (the environment variables win:
// AZAHARFRAME_IMMERSIVE_MEASURE=0 or AZAHARFRAME_IMMERSIVE_FOV make it Off).
Settings::MatchGameCamera MatchMode() {
    if (!g.tuning.measure || g.tuning.fov_from_env) {
        return Settings::MatchGameCamera::Off;
    }
    return static_cast<Settings::MatchGameCamera>(
        std::min<u32>(Settings::values.vr_immersive_match_camera_mode.GetValue(), 2));
}

// Always: the screen and the gain follow the measured camera. Off and Set Once: the settings.
bool MatchCamera() {
    return MatchMode() == Settings::MatchGameCamera::Always;
}

// AzaharFrame fovonce: whether the game camera is measured: Always, or Set Once until it has a
// value for this game (or gave up for this run).
bool Measuring() {
    switch (MatchMode()) {
    case Settings::MatchGameCamera::Always:
        return true;
    case Settings::MatchGameCamera::SetOnce:
        return !Settings::values.vr_immersive_fov_measured.GetValue() &&
               g.estimate.GetState() == L::FovEstimate::State::Measuring;
    default:
        return false;
    }
}

float DegFromTan(float tan_half_y) {
    return std::clamp(2.0f * std::atan(tan_half_y) / L::kDegToRad, 30.0f, 110.0f);
}

// AzaharFrame fovonce: Set Once has a value (or none came): written into this game's own
// Immersive Game FOV, which is used from then on; the Qt side saves it (TakeMeasuredFov).
void FinishSetOnce() {
    const int64_t secs = g.estimate.MeasuringMs() / 1000;
    if (g.estimate.GetState() == L::FovEstimate::State::Done) {
        const int fov = g.estimate.Value();
        auto& game_fov = Settings::values.vr_immersive_game_fov;
        game_fov.SetGlobal(false);
        game_fov.SetValue(fov);
        auto& measured = Settings::values.vr_immersive_fov_measured;
        measured.SetGlobal(false);
        measured.SetValue(true);
        g_fov_written = fov;
        LOG_INFO(Frontend,
                 "xr: immersive: Detect Game FOV Once: the game camera is {} degrees high "
                 "({:.0f} % of {:.1f} s that held, after {} s); Immersive Game FOV set to {} "
                 "for this game, nothing measured from now on",
                 fov, 100.0f * g.estimate.Share(), g.estimate.CountedMs() / 1000.0, secs, fov);
        ShowToastText(fmt::format("Game FOV Detected: {}°", fov), ToastTime::Long);
    } else {
        const int fov = static_cast<int>(std::lround(g.game_fov_y_deg));
        LOG_INFO(Frontend,
                 "xr: immersive: Detect Game FOV Once: no steady game camera in {} s "
                 "({:.1f} s held, the most at one angle {:.0f} %); Immersive Game FOV stays {}",
                 secs, g.estimate.CountedMs() / 1000.0, 100.0f * g.estimate.Share(), fov);
        ShowToastText(fmt::format("Couldn't Detect Game FOV \u00b7 Kept {}°", fov),
                      ToastTime::Long);
    }
}

void UpdateMeasuredCamera() {
    const Settings::MatchGameCamera match = MatchMode();
    const u32 match_now = static_cast<u32>(match);
    const uint32_t remeasure = g_remeasure.load();
    // Set Once chosen (from another choice, or again): this game measures anew.
    if ((match == Settings::MatchGameCamera::SetOnce && g.match_seen != match_now) ||
        remeasure != g.remeasure_seen) {
        g.estimate.Reset();
    }
    g.match_seen = match_now;
    g.remeasure_seen = remeasure;
    const int64_t now = VideoCore::Immersive::SteadyMs();
    if (!Measuring()) { // AzaharFrame depth: Match Game Camera off, the settings as they are
        g.camera.Reset();
        g.follow.Reset();
        return;
    }
    if (match == Settings::MatchGameCamera::SetOnce) {
        // Only the time a game in a Full mode is on view counts (not the menu, not a pause).
        const int64_t dt = g.engaged ? std::clamp<int64_t>(now - g.estimate_tick_ms, 0, 100) : 0;
        g.estimate.Tick(dt);
    }
    g.estimate_tick_ms = now;
    float tan_half_y = 0.0f, aspect = 0.0f;
    int64_t stamp = 0;
    if (VideoCore::Immersive::g_measured_projection.Read(std::max(now - 1000, g.game_start_ms),
                                                        tan_half_y, aspect, stamp) &&
        stamp != g.camera_stamp) {
        g.camera_stamp = stamp;
        const bool first = g.camera.ScreenFovDeg() <= 0.0f;
        const unsigned change = g.camera.Add(tan_half_y, aspect);
        if (change & L::CameraFilter::Screen) {
            LOG_INFO(Frontend,
                     "xr: immersive: game camera measured {:.1f} degrees high, {:.3f} wide{}",
                     g.camera.ScreenFovDeg(), g.camera.ScreenAspect(), first ? "" : " (changed)");
        } else if (change & L::CameraFilter::Live) {
            LOG_DEBUG(Frontend,
                      "xr: immersive: game camera now {:.1f} degrees high (the screen keeps "
                      "its size and shape)",
                      g.camera.LiveFovDeg());
        }
        if (std::isfinite(tan_half_y) && tan_half_y > 0.0f) {
            const float deg = 2.0f * std::atan(tan_half_y) / L::kDegToRad;
            if (match == Settings::MatchGameCamera::Always) {
                g.follow.Add(stamp, DegFromTan(tan_half_y));
            } else if (g.engaged) {
                g.estimate.Add(stamp, deg);
            }
            if (g.tuning.fov_trace) {
                LOG_INFO(Frontend, "xr: immersive: fov trace: measured {:.2f} at {} ms", deg,
                         stamp - g.game_start_ms);
            }
        }
    }
    if (match == Settings::MatchGameCamera::SetOnce &&
        g.estimate.GetState() != L::FovEstimate::State::Measuring) {
        FinishSetOnce();
    }
}

// AzaharFrame immersive: the immersive mode in use, the setting's (every game can turn it on).
u32 ModeInUse() {
    return Settings::values.vr_immersive_mode.GetValue();
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

// AzaharFrame immersive2: the head-locked default is the world at its own scale (gain 1), 3 m
// ahead, and grows with Wider View; a preset can make a game's screen a little bigger or smaller
// (presets.h screen_size).
L::Placement DefaultHeadPlacement() {
    L::Placement p;
    p.distance = g.tuning.head_distance;
    if (const ImmersivePreset* preset = FindImmersivePreset(g.title_id)) {
        if (preset->screen_size > 0.0f) {
            p.size = preset->screen_size;
        }
        if (preset->own_placement) { // AzaharFrame presets: the owner's tuned pitch and distance
            p.pitch_deg = preset->place_pitch_deg;
            p.distance = preset->place_distance;
        }
    }
    return L::Clamped(p);
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
    const std::string text = g.store.Format();
    if (!Common::WriteFileAtomic(path, text)) { // AzaharFrame harden: fsynced, atomic
        LOG_WARNING(Frontend, "xr: immersive: could not save the screen placement");
    }
}

void LogPlacement(const char* what, const L::Placement& p, bool wide) {
    LOG_INFO(Frontend, "xr: immersive {} placement ({}): {:.1f} degrees from the view's middle, "
                       "{:.2f} m, size {:.2f}",
             wide ? "wide" : "head-locked", what, p.pitch_deg, p.distance, p.size);
}

// AzaharFrame immersive: AZAHARFRAME_TEST_RESOLUTION="<seconds>:<factor>" sets Internal
// Resolution that long after the game started, once, as the menu does (mock runs that check a
// resolution change mid-game).
void TestResolutionHook() {
    static const char* env = Common::TestHookEnv("AZAHARFRAME_TEST_RESOLUTION");
    static bool done = false;
    if (!env || done || g.game_start_ms == 0) {
        return;
    }
    float at = 0.0f;
    unsigned factor = 0;
    if (std::sscanf(env, "%f:%u", &at, &factor) != 2 || factor < 1 || factor > 10) {
        done = true;
        return;
    }
    if (VideoCore::Immersive::SteadyMs() - g.game_start_ms >= static_cast<int64_t>(at * 1000)) {
        done = true;
        Settings::values.resolution_factor.SetValue(factor);
        LOG_INFO(Frontend, "xr: immersive: test: Internal Resolution {}x", factor);
    }
}

// AzaharFrame heldquad: the picture the screen is anchored to on this display frame: the one on
// view; with AZAHARFRAME_IMMERSIVE_ANCHOR_LAG=-1 on its first refresh the one before it, with +1
// on its last refresh (as long as the one before was on view) the next one, foreseen from the
// pose ring: the head pose as long after this picture's as this one's was after the one before
// (a picture's tag spacing). `held_time`: the pose ring's key for it (0: none).
VideoCore::XrContentPose AnchorContent(int64_t& held_time) {
    const std::array<ShownPicture, kShownPictures> pics = ShownPictures();
    const ShownPicture& on = pics[0];
    held_time = on.content.time;
    auto& l = g.lag;
    if (l.frame != g.view.time) {
        // The display frame before is over: count the picture it showed.
        if (l.frame != 0 && l.frame_serial != 0) {
            if (l.frame_serial == l.serial) {
                l.run++;
            } else {
                l.last_run = l.run;
                l.run = 1;
                l.serial = l.frame_serial;
            }
        }
        l.frame = g.view.time;
    }
    l.frame_serial = on.serial;
    if (g.tuning.anchor_lag == 0) {
        return on.content;
    }
    // This frame's count of the picture on view, and the refreshes of the one before.
    const bool same = on.serial == l.serial;
    const uint32_t run = same ? l.run + 1 : 1;
    const uint32_t last_run = same ? l.last_run : l.run;
    const ShownPicture& before = pics[1];
    const bool have_before = before.serial != 0 && before.serial + 1 == on.serial &&
                             before.content.valid && on.content.valid;
    if (!have_before) {
        return on.content;
    }
    if (g.tuning.anchor_lag < 0 && run == 1) {
        held_time = before.content.time;
        return before.content;
    }
    if (g.tuning.anchor_lag > 0 && last_run >= 2 && run >= last_run) {
        const int64_t next = on.content.time + (on.content.time - before.content.time);
        const L::HeldPose* p = g.poses.Nearest(next);
        // Within half a 120 Hz refresh of the foreseen time, and not after this frame.
        if (p && std::abs(p->time - next) <= 4'200'000 && p->time <= g.view.time) {
            VideoCore::XrContentPose c;
            c.valid = true;
            c.orientation[0] = p->head.orientation.x;
            c.orientation[1] = p->head.orientation.y;
            c.orientation[2] = p->head.orientation.z;
            c.orientation[3] = p->head.orientation.w;
            c.position[0] = p->head.position.x;
            c.position[1] = p->head.position.y;
            c.position[2] = p->head.position.z;
            c.time = p->time;
            held_time = p->time;
            return c;
        }
    }
    return on.content;
}

bool AnchorPose(XrTime display, XrPosef& head, const Layout::HeldPose** held) {
    if (held) {
        *held = nullptr;
    }
    if (!g.engaged || !IsViewMode(g.mode)) {
        return false;
    }
    int64_t held_time = 0;
    const VideoCore::XrContentPose shown = AnchorContent(held_time);
    if (!shown.valid || shown.time < g.recentre_time) {
        return false;
    }
    const double age_ms = (display - shown.time) / 1e6;
    if (age_ms > g.tuning.anchor_max_ms || age_ms < -100.0) {
        return false;
    }
    head = ToXr(shown);
    if (held) {
        *held = g.poses.Find(held_time);
    }
    return true;
}

// AzaharFrame stereo: no cap (another mode, 3D Depth 0, no game): the game sees the user's
// slider again.
void StopStereoCap() {
    if (g.stereo_cap_logged >= 0.0f) {
        LOG_INFO(Frontend, "xr: 3D Depth {} % no longer limited",
                 Settings::values.factor_3d.GetValue());
    }
    g.stereo_cap.Reset();
    g.stereo_cap_logged = -1.0f;
    Settings::frame_vr_stereo_cap.store(-1.0f, std::memory_order_relaxed);
}

// AzaharFrame natural: 3D Depth in the Full modes, percent of the natural 3D.
uint32_t NaturalDepth() {
    const auto& v = Settings::values;
    return L::NaturalDepthPercent(!v.vr_immersive_depth.UsingGlobal(), v.vr_immersive_depth.GetValue(),
                                  !v.factor_3d.UsingGlobal(), v.factor_3d.GetValue());
}

// AzaharFrame natural: what goes to the game's file while it plays (TakeWorldScaleSave).
std::mutex g_world_save_mutex;
std::optional<WorldScaleSave> g_world_save;

void QueueWorldScaleSave(const WorldScaleSave& save) {
    std::scoped_lock lock{g_world_save_mutex};
    if (g_world_save) { // one waiting already: merge
        if (save.world_scale > 0.0f) {
            g_world_save->world_scale = save.world_scale;
            g_world_save->migrated = g_world_save->migrated || save.migrated;
        }
        if (save.eye_per_far > 0.0f) {
            g_world_save->eye_per_far = save.eye_per_far;
        }
        return;
    }
    g_world_save = save;
}

// AzaharFrame natural: World Scale, percent (10 to 400; 100 Automatic).
float WorldScaleSetting() {
    return L::ClampWorldScale(Settings::values.vr_immersive_world_scale.GetValue());
}

// AzaharFrame natural: World Scale set while the game plays (the grips, the migration): this
// game's own value at once, its file through Qt.
void SetWorldScale(float percent, bool migrated) {
    auto& s = Settings::values.vr_immersive_world_scale;
    const float p = L::ClampWorldScale(percent);
    if (g.title_id) {
        s.SetGlobal(false);
    }
    s.SetValue(p);
    if (g.title_id) {
        Settings::values.vr_immersive_world_set.SetGlobal(false);
        Settings::values.vr_immersive_world_set.SetValue(true);
        QueueWorldScaleSave({p, -1.0f, migrated});
    }
}

// AzaharFrame natural: 1.2.0's head-motion scale from the old settings: the fallback where the
// game's 3D cannot be read, and what the migration reproduces in mode 3.
float OldLeaning() {
    const auto& v = Settings::values;
    return L::GamePositionScale(
        static_cast<int>(std::min<u32>(v.vr_immersive_positional_game_scaler.GetValue(), 2)),
        v.factor_3d.GetValue());
}

// AzaharFrame natural: World Scale's migration, once a game (immersive_layout.h, design: the
// look and the head movement as in 1.2.0). Mode 2 keeps the saved distance; mode 3 waits for
// the game's eyes and keeps 1.2.0's leaning and eye offset (the screen's distance changes, on
// the eyes, so the picture does not).
void MigrateWorldScale(u32 mode) {
    const auto& v = Settings::values;
    if (g.world_migration == 1) {
        return;
    }
    const bool follow = v.vr_immersive_follow_screen.GetValue();
    const float size = L::Clamped(g.head_place).size;
    if (g.world_migration == 0) {
        const bool done =
            !g.title_id || !L::WorldScaleToMigrate(!v.vr_immersive_world_scale.UsingGlobal(),
                                                   v.vr_immersive_world_set.GetValue() &&
                                                       !v.vr_immersive_world_set.UsingGlobal());
        if (done) {
            g.world_migration = 1;
            return;
        }
        if (mode == 2) {
            const float p =
                L::MigrateWorldScaleMode2(L::Clamped(g.head_place).distance, size, follow);
            LOG_INFO(Frontend,
                     "xr: World Scale migrated for mode 2: {:.1f} % keeps the screen at {:.2f} m "
                     "(1.2.0 leaned {:.1f} game units per metre; now matched to the game's 3D)",
                     p, L::Clamped(g.head_place).distance, OldLeaning());
            SetWorldScale(p, true);
            g.world_migration = 1;
            return;
        }
        LOG_INFO(Frontend, "xr: World Scale: migrating mode 3 once the game's eyes are read; "
                           "until then 1.2.0's {:.1f} game units per metre at {:.2f} m",
                 OldLeaning(), L::Clamped(g.head_place).distance);
        g.world_migration = 2;
        return;
    }
    // Mode 3 waiting: the game's own eyes, read.
    if (mode != 3) {
        return; // mode 2 now: its own rule on the next game start
    }
    const float k = VideoCore::Immersive::g_stereo_eye_per_far.load(std::memory_order_relaxed);
    const int samples = VideoCore::Immersive::g_stereo_eye_samples.load(std::memory_order_relaxed);
    if (!(k > 0.0f) || samples < L::EyePerFarInUse::kSaveAfter) {
        return;
    }
    const float old = OldLeaning();
    bool exact = false;
    const float p = L::MigrateWorldScaleMode3(g.solved, k, old, size, follow, &exact);
    if (exact) {
        // At once, both: the leaning and our eyes stay exactly as they were (the screen moves on
        // the eyes, so the picture does not).
        g.full_distance.Set(L::FullDistance(p, size, follow));
        g.eye_k.Set(k);
        LOG_INFO(Frontend,
                 "xr: World Scale migrated for mode 3: {:.1f} % (the screen from {:.2f} to {:.2f} "
                 "m) keeps 1.2.0's {:.1f} game units per metre (game K {:.1f})",
                 p, L::Clamped(g.head_place).distance, L::FullDistance(p, size, follow), old, k);
    } else {
        // Clamped to World Scale's range, or Automatic (1.2.0 did not move the camera): the
        // leaning and our eyes change, so the distance and K ease there (their ramps).
        LOG_INFO(Frontend,
                 "xr: World Scale migrated for mode 3: {:.1f} % (the screen from {:.2f} to {:.2f} "
                 "m, eased); {} (game K {:.1f})",
                 p, L::Clamped(g.head_place).distance, L::FullDistance(p, size, follow),
                 old > 0.0f ? fmt::format("1.2.0's {:.1f} game units per metre is out of World "
                                          "Scale's range, so the nearest end",
                                          old)
                            : std::string("1.2.0 did not move the camera, so Automatic"),
                 k);
    }
    SetWorldScale(p, true);
    g.world_migration = 1;
}

// AzaharFrame natural: the Full modes' screen distance: World Scale's (eased), or the saved one
// while mode 3's migration waits.
float FullScreenDistance(u32 mode) {
    const auto& v = Settings::values;
    const L::Placement p = L::Clamped(g.head_place);
    const float target = g.world_migration == 2 && mode == 3
                             ? p.distance
                             : L::FullDistance(WorldScaleSetting(), p.size,
                                               v.vr_immersive_follow_screen.GetValue());
    g.full_distance_target = target;
    return g.full_distance.Update(target, VideoCore::Immersive::SteadyMs());
}

// AzaharFrame natural: the leaning scale in game units per metre: the game's K (measured, saved
// from an earlier run, or 1.2.0's scale at the screen as saved; L::EyePerFarInUse, eased) over
// the picture's width at the World Scale distance. The game's K goes to its file once measured.
float NaturalLeaningNow() {
    const auto& v = Settings::values;
    const float measured =
        VideoCore::Immersive::g_stereo_eye_per_far.load(std::memory_order_relaxed);
    const int samples = VideoCore::Immersive::g_stereo_eye_samples.load(std::memory_order_relaxed);
    const float saved = v.vr_immersive_eye_per_far.UsingGlobal()
                            ? -1.0f
                            : v.vr_immersive_eye_per_far.GetValue();
    L::HeadLocked at_saved = g.solved;
    at_saved.distance = L::Clamped(g.head_place).distance;
    const float k = g.eye_k.Update(measured, samples, saved, L::EyePerFarFor(at_saved, OldLeaning()),
                                   VideoCore::Immersive::SteadyMs());
    if (g.eye_k.From() == L::EyePerFarInUse::kMeasured && !g.k_saved && g.title_id &&
        g.world_migration != 2) {
        g.k_saved = true;
        if (!(saved > 0.0f) || std::abs(g.eye_k.Target() / saved - 1.0f) > 0.02f) {
            Settings::values.vr_immersive_eye_per_far.SetGlobal(false);
            Settings::values.vr_immersive_eye_per_far.SetValue(g.eye_k.Target());
            QueueWorldScaleSave({-1.0f, g.eye_k.Target(), false});
        }
    }
    return L::NaturalLeaning(g.solved, k);
}

// AzaharFrame natural: mode 2 shows the game 3D Depth x the natural limit (the slider at which its
// far scenery is parallel on this screen, from its far width per slider unit measured live, the
// screen's size, distance and factor and the IPD, every frame); mode 3 makes the same with its
// own eyes (L::Mode3Natural). Both lean by the same scale. While the in-game menu shows (not
// engaged) the slider holds as it is.
void UpdateNatural(u32 mode) {
    if (mode != 2 && mode != 3) {
        if (Settings::frame_vr_natural_slider.load(std::memory_order_relaxed) >= 0.0f) {
            Settings::frame_vr_natural_slider.store(-1.0f, std::memory_order_relaxed);
        }
        g.natural.Reset();
        g.quad_half = L::kQuadsOnEyes;
        g.natural_logged_limit = -1.0f;
        g.natural_logged_depth = 1000;
        return;
    }
    Settings::frame_vr_stereo_cap.store(-1.0f, std::memory_order_relaxed); // replaced by this
    const u32 depth = NaturalDepth();
    const float f = static_cast<float>(depth) / 100.0f;
    const float leaning = NaturalLeaningNow();
    g.leaning = leaning;
    const int source = static_cast<int>(g.eye_k.From());
    const float per = VideoCore::Immersive::g_stereo_far_per_slider.load(std::memory_order_relaxed);
    if (mode == 2) {
        if (g.engaged) {
            // The screen moving farther (World Scale eased up) makes the far scenery wider
            // apart on it before the game draws with a lower slider: the limit at the distance
            // it is going to as well, so it never goes past parallel on the way.
            L::HeadLocked going = g.solved;
            going.distance = std::max(g.solved.distance, g.full_distance_target);
            const float limit = std::min(L::StereoSliderLimit(g.solved, g.half_ipd, per),
                                         L::StereoSliderLimit(going, g.half_ipd, per));
            const float sent = g.natural.Update(f, limit, VideoCore::Immersive::SteadyMs());
            Settings::frame_vr_natural_slider.store(sent, std::memory_order_relaxed);
        }
        g.quad_half = L::kQuadsOnEyes;
    } else {
        Settings::frame_vr_natural_slider.store(-1.0f, std::memory_order_relaxed);
        const float far_width =
            VideoCore::Immersive::g_stereo_far_width.load(std::memory_order_relaxed);
        g.mode3 = L::Mode3Natural(f, leaning, g.half_ipd, L::GameFarOnQuad(g.solved, far_width));
        g.quad_half = g.mode3.quad_half;
    }
    // The log: a new 3D Depth, World Scale or source, the limit 2 % or the leaning 10 % away.
    const float limit = g.natural.Limit();
    const float world = WorldScaleSetting();
    const bool limit_moved =
        mode == 2 && limit >= 0.0f &&
        (g.natural_logged_limit < 0.0f || std::abs(limit - g.natural_logged_limit) > 0.02f * limit);
    const bool leaning_moved =
        leaning > 0.0f &&
        (g.natural_logged_leaning < 0.0f ||
         std::abs(leaning - g.natural_logged_leaning) > 0.1f * g.natural_logged_leaning);
    if (depth != g.natural_logged_depth || source != g.natural_logged_source || limit_moved ||
        leaning_moved || std::abs(world - g.natural_logged_world) > 0.5f) {
        g.natural_logged_depth = depth;
        g.natural_logged_source = source;
        g.natural_logged_leaning = leaning;
        g.natural_logged_world = world;
        if (mode == 2) {
            g.natural_logged_limit = limit;
        }
        static constexpr const char* kSource[] = {
            "from the game's own 3D", "from the game's own 3D as measured in an earlier run",
            "1.2.0's scale (the game's eyes not read yet)", "none yet"};
        const float screen_deg = Deg(2 * std::atan(g.solved.tan_half_y * g.solved.aspect));
        const std::string world_text = fmt::format(
            "World Scale {:.1f} %{} (screen at {:.2f} m, Follow Screen Size {})", world,
            g.world_migration == 2 ? " waiting for the migration" : "", g.solved.distance,
            Settings::values.vr_immersive_follow_screen.GetValue() ? "on" : "off");
        if (mode == 2) {
            LOG_INFO(Frontend,
                     "xr: natural 3D: 3D Depth {} % = the game's 3D at {} (natural {} for the "
                     "screen {:.0f} degrees wide at {:.2f} m, factor {:.2f}, IPD {:.1f} mm{}); "
                     "{}: the head moves the camera {:.1f} game units per metre, {}",
                     depth,
                     limit >= 0.0f ? fmt::format("{:.2f} %", 100.0f * std::min(f * limit, 1.0f))
                                   : fmt::format("{:.2f} % (probing)",
                                                 100.0f * f * L::NaturalSlider::kProbe),
                     limit >= 0.0f ? fmt::format("{:.2f} %", 100.0f * limit)
                                   : std::string("not measured yet"),
                     screen_deg, g.solved.distance, g.solved.factor, 2000.0f * g.half_ipd,
                     g.natural.Clamped() ? "; above the 3DS's full 3D, so 100 % of it" : "",
                     world_text, leaning, kSource[source]);
        } else {
            LOG_INFO(Frontend,
                     "xr: natural 3D: 3D Depth {} %, mode 3's eyes {:.4f} game units apart "
                     "({:.1f} per metre of IPD), eye quads {:.1f} mm from the middle (the screen "
                     "{:.0f} degrees wide at {:.2f} m, factor {:.2f}, IPD {:.1f} mm); {}: the "
                     "head moves the camera {:.1f} game units per metre, {}",
                     depth, g.mode3.eye_scaler * 2.0f * g.half_ipd, g.mode3.eye_scaler,
                     1000.0f * g.mode3.quad_half, screen_deg, g.solved.distance, g.solved.factor,
                     2000.0f * g.half_ipd, world_text, leaning, kSource[source]);
        }
    }
}

void OnFrame(const FrameInfo& info) {
    const u32 mode = ModeInUse(); // AzaharFrame immersive
    TestResolutionHook();
    const bool visible =
        info.state == XR_SESSION_STATE_VISIBLE || info.state == XR_SESSION_STATE_FOCUSED;
    if (!info.screens_live) {
        // No game: the next one starts with the bottom screen hidden, as in CitraVR.
        g.bottom_toggled = false;
        g.focus_bottom = false;
        g.rest.Forget();
        g.adjuster.Cancel();
    } else if (!g.screens_were_live) {
        // A game just started: its per-game settings are loaded; read "Hide Bottom Screen" now.
        g.game_start_ms = VideoCore::Immersive::SteadyMs(); // AzaharFrame depth
        // AzaharFrame stereo: another game's 3D is measured afresh and its cap starts from
        // nothing. A quick save loaded in the same game (the screens go off for a moment) keeps
        // both, so the cap never lifts for the frames it takes to measure again. AzaharFrame oot:
        // the menu rule's "two eyes earlier" likewise.
        if (g_title_id.load() != g.title_id) {
            VideoCore::Immersive::g_stereo_generation.fetch_add(1, std::memory_order_relaxed);
            VideoCore::Immersive::g_stereo_far_per_slider.store(-1.0f, std::memory_order_relaxed);
            StopStereoCap();
            // AzaharFrame natural: the game's eyes are measured afresh, the slider and leaning too.
            VideoCore::Immersive::g_stereo_eye_per_far.store(-1.0f, std::memory_order_relaxed);
            VideoCore::Immersive::g_stereo_eye_samples.store(0, std::memory_order_relaxed);
            g.natural.Reset();
            g.eye_k.Reset();
            g.full_distance.Reset();
            g.full_distance_target = -1.0f;
            g.leaning = -1.0f;
            g.world_migration = 0;
            g.k_saved = false;
            g.natural_logged_leaning = -1.0f;
            g.natural_logged_world = -1.0f;
        }
        VideoCore::Immersive::g_world_once.store(false, std::memory_order_relaxed); // AzaharFrame oot
        g.menu_exact.Reset(); // AzaharFrame oot
        g.menu_exact_on = false;
        g.camera.Reset();
        g.estimate.Reset(); // AzaharFrame fovonce
        g.follow.Reset();
        g.angles.Reset();
        g.hide_bottom = Settings::values.vr_hide_bottom_screen.GetValue();
        LOG_INFO(Frontend, "xr: bottom screen {} at start", g.hide_bottom ? "hidden" : "shown");
        // The screen shape and the game camera's angle (per game as well) are read below.
        g.shape_logged = false;
        g.wider_level = WiderLevel();
        g.options.wider = L::WiderFactor(g.wider_level);
        g.scale_x = L::ScalePercent(Settings::values.vr_immersive_scale_x.GetValue());
        g.scale_y = L::ScalePercent(Settings::values.vr_immersive_scale_y.GetValue());
        g.options.squeeze = L::SqueezeForScale(g.scale_x);
        g.options.squeeze_y = L::SqueezeForScale(g.scale_y);
        LOG_INFO(Frontend,
                 "xr: immersive: Render Margin {:.1f}x, Horizontal Scale {}%, Vertical Scale {}%",
                 g.options.wider, g.scale_x, g.scale_y);
        // The saved placement of this game.
        g.title_id = g_title_id.load();
        LoadStore();
        const auto head = g.title_id ? g.store.Get(g.title_id, false) : std::nullopt;
        const auto wide = g.title_id ? g.store.Get(g.title_id, true) : std::nullopt;
        g.head_place = head.value_or(DefaultHeadPlacement());
        g.wide_place = wide.value_or(DefaultPlacement());
        if (head && g.store.OldSize(g.title_id)) {
            // AzaharFrame immersive: saved by an earlier build, against another default: the
            // pitch and distance stay, the size is this build's default (true scale).
            g.head_place.size = DefaultHeadPlacement().size;
            g.store.Set(g.title_id, false, g.head_place);
            SaveStore();
            LogPlacement("earlier build's size replaced by the default", g.head_place, false);
        }
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
        const u32 wider = WiderLevel();
        if (wider != g.wider_level) {
            g.wider_level = wider;
            g.options.wider = L::WiderFactor(wider);
            LOG_INFO(Frontend, "xr: immersive: Render Margin {:.1f}x (changed)", g.options.wider);
        }
        // AzaharFrame immersive2: Narrow Screen and Narrow Height are live too (the menu, the
        // game's Properties, or both grips and the D-pad in StepScale): only the quad's shape
        // and the gains change, nothing in the game.
        const int scale_x = L::ScalePercent(Settings::values.vr_immersive_scale_x.GetValue());
        const int scale_y = L::ScalePercent(Settings::values.vr_immersive_scale_y.GetValue());
        if (scale_x != g.scale_x || scale_y != g.scale_y) {
            g.scale_x = scale_x;
            g.scale_y = scale_y;
            g.options.squeeze = L::SqueezeForScale(scale_x);
            g.options.squeeze_y = L::SqueezeForScale(scale_y);
            LOG_INFO(Frontend, "xr: immersive: Horizontal Scale {}%, Vertical Scale {}% (changed)",
                     scale_x, scale_y);
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
        if (shape == AutoShape::kAutomatic) { // AzaharFrame fixes7: eased, every frame
            g_top_aspect = AutoShape::Aspect();
        }
    }
    g.mode = mode;
    g.live = info.screens_live && visible;
    // AzaharFrame depth: the emulation thread measures the game camera only while this is set
    // (Match Game Camera on, a game in a Full mode); off, it does no work at all.
    // AzaharFrame fovonce: Always, or Set Once until it has a value.
    VideoCore::Immersive::g_measure_camera.store(info.screens_live && IsViewMode(mode) &&
                                                     Measuring(),
                                                 std::memory_order_relaxed);
    g.view = LatestViewPose();
    // AzaharFrame ipd: the IPD from the eyes SteamVR located (the dial), for the screens and the
    // game's eyes alike; frames whose eye positions are not tracked are ignored.
    {
        if (!g.ipd_started) { // the last IPD read (an earlier run) until the headset's is
            g.ipd_started = true;
            const float saved = Ipd::SavedM(Settings::values.vr_last_ipd_mm.GetValue());
            g.ipd_had_saved = saved > 0.0f;
            g.ipd_saver = Ipd::Saver(saved);
            g.ipd.SetFallback(saved);
            LOG_INFO(Frontend, "xr: IPD {:.1f} mm until the headset's is read ({})",
                     g.ipd.Value() * 1000.0f, g.ipd_had_saved ? "saved" : "default");
        }
        const XrVector3f d = Sub(g.view.eye[1].position, g.view.eye[0].position);
        g.half_ipd = g.ipd.Add(g.view.time, std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z),
                               g.view.views_valid && g.view.eye_positions_tracked) /
                     2.0f;
        float ipd_m = 0.0f;
        if (g.ipd.TakeLog(ipd_m)) {
            LOG_INFO(Frontend, "xr: IPD {:.1f} mm (from the headset)", ipd_m * 1000.0f);
        }
        if (g.ipd_saver.Want(g.ipd, ipd_m)) { // the first value, then moves over 0.5 mm
            Settings::values.vr_last_ipd_mm.SetValue(ipd_m * 1000.0f);
            g.ipd_saved_now = true;
        }
        // Only when no IPD was ever saved and SteamVR gave no eye positions for 10 s: say so,
        // then say when it did.
        const std::string toast = g.ipd_notice.Update(g.view.time, g.ipd, g.ipd_had_saved);
        if (!toast.empty()) {
            LOG_INFO(Frontend, "xr: IPD toast \"{}\"", toast);
            ShowToastText(toast, ToastTime::Long);
        }
    }

    const bool was_engaged = g.engaged;
    g.engaged = Decision().immersive;

    // AzaharFrame immersive: the rest pose at the start, after each pause and after a re-centre
    // (from a head pose located in the re-centred space), never from the render size.
    if (g.rest.Update(g.engaged, g.view.valid, g.view.time)) {
        g.ref = {YawOnly(g.view.head.orientation), g.view.head.position};
        if (was_engaged) {
            LOG_INFO(Frontend, "xr: immersive: view re-centred");
        }
    }
    // AzaharFrame heldquad: the poses kept are about this mode and rest pose only.
    g.poses.Keep(mode, ToIn(g.ref));

    if (g.view.valid) {
        const XrVector3f between = Rotate(Conj(g.view.head.orientation),
                                          Sub(g.view.eye[1].position, g.view.eye[0].position));
        const float half = std::abs(between.x) / 2;
        g.view_raw = L::BothEyes(EyeInHead(g.view.head, g.view.eye[0], g.view.fov[0]),
                                 EyeInHead(g.view.head, g.view.eye[1], g.view.fov[1]));
        g.view_raw_valid = g.view.views_valid;
        // AzaharFrame jump: the screen is fitted to a settled view, not to each frame's: the
        // runtime's per-frame eye views moved the screen and the game's camera up and down.
        if (g.view_raw_valid && g.view_settle.Add(g.view_raw, (half > 0.02f && half < 0.045f) ? half
                                                                            : kDefaultHalfIpd)) {
            g.view_fov = g.view_settle.View();
            LOG_INFO(Frontend,
                     "xr: immersive: both eyes see {:.1f} to {:.1f} degrees across, {:.1f} to "
                     "{:.1f} up, either eye {:.1f} to {:.1f} across, {:.1f} to {:.1f} up, eyes "
                     "{:.1f} mm apart (left eye now {:.1f}/{:.1f}/{:.1f}/{:.1f}); the screen is "
                     "fitted to this{}",
                     Deg(g.view_fov.left), Deg(g.view_fov.right), Deg(g.view_fov.down),
                     Deg(g.view_fov.up), Deg(g.view_fov.outer_left), Deg(g.view_fov.outer_right),
                     Deg(g.view_fov.outer_down), Deg(g.view_fov.outer_up),
                     2000.0f * g.half_ipd, Deg(g.view.fov[0].angleLeft),
                     Deg(g.view.fov[0].angleRight), Deg(g.view.fov[0].angleDown),
                     Deg(g.view.fov[0].angleUp),
                     g.view_fov_changes++ == 0 ? " (first frame)"
                     : g.view_fov_changes == 2 ? " (mean of the first frames)"
                                               : " (changed for good)");
        }
    } else {
        g.view_raw_valid = false;
    }
    if (IsViewMode(mode)) {
        // Mode 2 shows one quad to both eyes: each eye sees it from half the IPD to the side.
        // AzaharFrame natural: at World Scale's distance (last frame's, eased).
        const float eye_shift =
            mode == 2 && g.full_distance.Value() > 0.0f
                ? g.half_ipd / g.full_distance.Value()
                : L::SharedQuadEyeShift(mode, g.head_place, g.half_ipd);
        // AzaharFrame depth: the game's own angle and shape when its projection was found
        // (each change logged), else the settings. AzaharFrame immersive: the screen is sized
        // and shaped with the settled camera; a zoom only changes the gain.
        UpdateMeasuredCamera();
        const L::CameraShape cam =
            L::CameraInUse(MatchCamera(), g.camera.ScreenFovDeg(), g.camera.ScreenAspect(),
                           g.game_fov_y_deg, TopAspect());
        L::Options options = g.options;
        // AzaharFrame fovonce: Always follows the angle that held half a second (FovFollow),
        // not each measurement.
        options.live_fov_y_deg = MatchCamera() && g.follow.Target() > 0.0f ? g.follow.Target()
                                                                           : cam.fov_y_deg;
        // AzaharFrame immersive2: the screen's size follows the camera angle at true scale, so a
        // game with its own Immersive Game FOV (a preset's measured play camera, or the player's)
        // is sized with that: the screen keeps one size through cutscenes and menus, and the
        // gain follows the measured angle. Otherwise the settled measurement (or the setting).
        const bool own_fov =
            g.tuning.fov_from_env || !Settings::values.vr_immersive_game_fov.UsingGlobal();
        float screen_fov = own_fov ? g.game_fov_y_deg : cam.fov_y_deg;
        // AzaharFrame fovonce: never in one step: Always moves both angles at most 12 degrees a
        // second, timed by the frames' display times and ramped at both ends (FovSlew); Set Once
        // eases in the value it wrote; Off uses the settings exactly as they are
        // (L::CameraAngles).
        const int64_t now = VideoCore::Immersive::SteadyMs();
        const L::CameraAngles::Out angles =
            g.angles.Update(static_cast<unsigned>(MatchMode()), screen_fov,
                            options.live_fov_y_deg, now, static_cast<int64_t>(g.view.time));
        screen_fov = angles.screen_deg;
        options.live_fov_y_deg = angles.live_deg;
        // AzaharFrame oot: mode 2's menu rule: a screen of the game's own in front of its world
        // (its world drawn once after two eyes) is drawn without the Render Margin.
        const bool menu = g.tuning.menu_exact && mode == 2 &&
                          VideoCore::Immersive::g_world_once.load(std::memory_order_relaxed);
        if (menu != g.menu_exact_on) {
            g.menu_exact_on = menu;
            if (menu) {
                LOG_INFO(Frontend,
                         "xr: immersive: the game shows a screen of its own (its world drawn "
                         "once): at its own size, {} {:.1f}x eased to 1.0x",
                         Names::kRenderMargin, options.wider);
            } else {
                LOG_INFO(Frontend,
                         "xr: immersive: the game's world in both eyes again: {} {:.1f}x",
                         Names::kRenderMargin, options.wider);
            }
        }
        // AzaharFrame kiu2: the screen keeps the padded size meanwhile, so the game's own frame
        // (the menu) fills the same area, as big as gameplay; eased with the factor.
        options.size_wider = options.wider;
        options.wider = L::MenuExact::Factor(options.wider, g.menu_exact.Update(menu, now));
        if (MatchMode() != Settings::MatchGameCamera::Off) {
            if (g.tuning.fov_trace) {
                const float row[4]{g.follow.Target(), screen_fov, options.live_fov_y_deg,
                                   cam.fov_y_deg};
                if (!std::equal(row, row + 4, g.traced)) {
                    std::copy(row, row + 4, g.traced);
                    LOG_INFO(Frontend,
                             "xr: immersive: fov trace: at {} ms followed {:.4f}, screen {:.4f}, "
                             "live {:.4f} (settled camera {:.2f})",
                             now - g.game_start_ms, row[0], row[1], row[2], row[3]);
                }
            }
        }
        g.solved = L::SolveHeadLocked(g.head_place, screen_fov, cam.aspect, g.view_fov,
                                      eye_shift, options, g.offset);
        // AzaharFrame natural: World Scale is the screen's distance at the angle as set (eased),
        // so the game's own 3D, its natural limit and the leaning all follow it.
        if (info.screens_live) {
            MigrateWorldScale(mode);
            g.solved.distance = FullScreenDistance(mode);
        }
    }
    // AzaharFrame stereo: without a game on view the slider holds (a quick save loading); another
    // game resets it as it starts (above). AzaharFrame natural: UpdateNatural (was UpdateStereoCap).
    if (info.screens_live) {
        UpdateNatural(mode);
    }

    VideoCore::Immersive::VRData data; // CitraVR's "off" values
    data.ipd = g.ipd.Value(); // AzaharFrame ipd
    // AzaharFrame doubling: the pose the game camera is made from goes with it (the immersive
    // screen is placed at it once the picture drawn with it is on view).
    if (g.engaged && IsViewMode(mode) && g.view.valid) {
        const XrPosef& h = g.view.head;
        data.pose.valid = true;
        data.pose.orientation[0] = h.orientation.x;
        data.pose.orientation[1] = h.orientation.y;
        data.pose.orientation[2] = h.orientation.z;
        data.pose.orientation[3] = h.orientation.w;
        data.pose.position[0] = h.position.x;
        data.pose.position[1] = h.position.y;
        data.pose.position[2] = h.position.z;
        data.pose.time = g.view.time;
    }
    if (g.engaged) {
        data.mode = static_cast<s32>(mode);
        data.factor = mode < 2 ? kWideFactor : g.solved.factor;
        data.factor_y = mode < 2 ? kWideFactor : g.solved.factor_y;
        data.uoffset = -1;
        if (g.view.valid) {
            XrQuaternionf q = Normalized(Mul(Conj(g.ref.orientation), g.view.head.orientation));
            if (IsViewMode(mode)) {
                // The screen's pitch (and the gain of a resized screen) go into the game camera
                // too, so the world stays where it is (immersive_layout.h).
                const Input::Quatf c =
                    L::GameCameraRotation({q.x, q.y, q.z, q.w}, g.solved);
                // AzaharFrame turns: the head and the camera given, for the per-second line.
                const auto note = [](const XrQuaternionf& r, float (&yaw)[2], float (&pitch)[2]) {
                    const XrVector3f f = Rotate(r, {0.0f, 0.0f, -1.0f});
                    const float y = Deg(std::atan2(-f.x, -f.z));
                    const float pt = Deg(std::atan2(f.y, std::hypot(f.x, f.z)));
                    yaw[0] = std::min(yaw[0], y);
                    yaw[1] = std::max(yaw[1], y);
                    pitch[0] = std::min(pitch[0], pt);
                    pitch[1] = std::max(pitch[1], pt);
                };
                note(q, g.age.head_yaw, g.age.head_pitch);
                q = {c.x, c.y, c.z, c.w};
                note(q, g.age.cam_yaw, g.age.cam_pitch);
                g.age.cam_frames++;
                // AzaharFrame heldquad: this frame's camera, kept for the picture drawn with it
                // (keyed by the pose's time, which the picture carries back with it).
                if (data.pose.valid) {
                    g.poses.Push({data.pose.time, ToIn(g.view.head), c, g.solved});
                }
            }
            const XrVector3f p =
                Rotate(Conj(g.ref.orientation), Sub(g.view.head.position, g.ref.position));
            // AzaharFrame natural: leaning by the scale the 3D shows (UpdateNatural: the game's
            // own 3D over the picture's width at World Scale's distance, else 1.2.0's scale),
            // no longer 3D Depth's; mode 3's eyes 3D Depth x that.
            const float leaning = std::max(g.leaning, 0.0f);
            // AzaharFrame integ9: POSITION_SCALE moves the head position's share only; the
            // game's two eyes (game_pos_scaler, 3D Depth) stay as they are.
            FillInverseView(q, p, leaning * g.tuning.position_scale, data.inv_view);
            data.game_pos_scaler = -(mode == 3 ? g.mode3.eye_scaler : leaning);
        }
    }
    VideoCore::Immersive::Publish(data);

    // AzaharFrame doubling: the per-second picture age line (Full modes, engaged).
    if (g.engaged && IsViewMode(mode) && g.view.valid) {
        auto& a = g.age;
        const VideoCore::XrContentPose shown = ShownContentPose();
        a.frames++;
        if (g.view_raw_valid) { // AzaharFrame jump: the view as located, against the fitted one
            const float mid = Deg(L::ViewMiddlePitch(g.view_raw));
            a.mid_min_deg = std::min(a.mid_min_deg, mid);
            a.mid_max_deg = std::max(a.mid_max_deg, mid);
            if (g.view_settle.Distance(g.view_raw) > L::kViewSettleChangeDeg * L::kDegToRad) {
                a.view_off++;
            }
        }
        if (shown.valid) {
            const double age_ms = (g.view.time - shown.time) / 1e6;
            const double turn = AngleDeg(g.view.head.orientation, ToXr(shown).orientation);
            a.age_sum_ms += age_ms;
            a.age_max_ms = std::max(a.age_max_ms, age_ms);
            a.turn_sum_deg += turn;
            a.turn_max_deg = std::max(a.turn_max_deg, turn);
            if (shown.time != a.last_picture) {
                if (a.run > 0) {
                    a.runs[std::min<uint32_t>(a.run, 4) - 1]++;
                }
                a.run = 0;
                a.last_picture = shown.time;
                a.pictures++;
            }
            a.run++;
            XrPosef unused;
            const L::HeldPose* held = nullptr;
            if (g.tuning.anchor && AnchorPose(g.view.time, unused, &held)) {
                a.anchored++;
                (held ? a.held : a.held_missed)++; // AzaharFrame heldquad
            }
        } else {
            a.no_pose++;
        }
        if (a.since_ns == 0) {
            a.since_ns = g.view.time;
        } else if (g.view.time - a.since_ns >= 1'000'000'000) {
            const uint32_t n = a.frames - a.no_pose;
            LOG_INFO(Frontend,
                     "xr: immersive: {} frames, {} pictures, picture drawn {:.1f} ms (max {:.1f}) "
                     "before display, head turned {:.2f} degrees (max {:.2f}) since it was drawn, "
                     "screen anchored to the picture in {} frames, {} without a pose ({}); "
                     "pictures on view 1/2/3/4+ frames {}/{}/{}/{}; view's middle as located "
                     "{:.1f} to {:.1f} degrees (screen fitted to {:.1f}), {} frames' view more "
                     "than {:.1f} degrees off",
                     a.frames, a.pictures, n ? a.age_sum_ms / n : 0.0, a.age_max_ms,
                     n ? a.turn_sum_deg / n : 0.0, a.turn_max_deg, a.anchored, a.no_pose,
                     g.tuning.anchor ? "AZAHARFRAME_IMMERSIVE_ANCHOR on" : "anchor off",
                     a.runs[0], a.runs[1], a.runs[2], a.runs[3],
                     a.mid_min_deg <= a.mid_max_deg ? a.mid_min_deg : 0.0f,
                     a.mid_min_deg <= a.mid_max_deg ? a.mid_max_deg : 0.0f,
                     Deg(L::ViewMiddlePitch(g.view_fov)), a.view_off, L::kViewSettleChangeDeg);
            if (a.anchored > 0) { // AzaharFrame heldquad
                LOG_INFO(Frontend,
                         "xr: immersive: a picture shown again held {} (AZAHARFRAME_IMMERSIVE_HOLD) "
                         "in {} anchored frames, its camera not found in {}; anchor lag {:+d}",
                         g.tuning.hold_warp ? "as drawn for the head now" : "rigid", a.held,
                         a.held_missed, g.tuning.anchor_lag);
            }
            if (a.cam_frames > 0) {
                LOG_INFO(Frontend,
                         "xr: immersive: head {:.0f} to {:.0f} degrees across from straight "
                         "ahead, {:.0f} to {:.0f} up; game camera given {:.0f} to {:.0f} across, "
                         "{:.0f} to {:.0f} up",
                         a.head_yaw[0], a.head_yaw[1], a.head_pitch[0], a.head_pitch[1],
                         a.cam_yaw[0], a.cam_yaw[1], a.cam_pitch[0], a.cam_pitch[1]);
            }
            if (a.slip_frames > 0) {
                LOG_INFO(Frontend,
                         "xr: immersive: far scenery on the anchored screen {:.2f} degrees (max "
                         "{:.2f}) from where its picture was drawn, moving {:.3f} degrees (max "
                         "{:.3f}) from one frame to the next, {} frames (screen {})",
                         a.slip_sum_deg / a.slip_frames, a.slip_max_deg,
                         a.step_frames ? a.step_sum_deg / a.step_frames : 0.0, a.step_max_deg,
                         a.slip_frames,
                         g.tuning.anchor_eye ? "about the eye now, AZAHARFRAME_IMMERSIVE_ANCHOR_EYE on"
                                             : "at the picture's head pose, ANCHOR_EYE off");
            }
            // AzaharFrame stereo: the game's own 3D on this screen (no change, only the log).
            const float far_width =
                VideoCore::Immersive::g_stereo_far_width.load(std::memory_order_relaxed);
            if (far_width >= 0.0f) {
                const L::StereoFar st = L::StereoFarOnQuad(g.solved, mode, g.half_ipd, far_width,
                                                             g.quad_half);
                // AzaharFrame integ8: on a meaningful change (the far angle 0.1 degrees, what the
                // eyes allow 0.05 degrees, doubling or not), else every 10 s with the range since.
                if (g.far_secs == 0) {
                    g.far_min_deg = g.far_max_deg = st.far_deg;
                }
                g.far_min_deg = std::min(g.far_min_deg, st.far_deg);
                g.far_max_deg = std::max(g.far_max_deg, st.far_deg);
                ++g.far_secs;
                using Common::FrameLog::Moved;
                const bool changed = Moved(g.far_said_deg, st.far_deg, 0.0f, 0.1f) ||
                                     Moved(g.far_said_allow, st.allow_deg, 0.0f, 0.05f) ||
                                     st.diverges != g.far_said_diverges;
                const auto say = g.far_gate.Update(g.view.time / 1'000'000, changed);
                if (say != Common::FrameLog::ChangeGate::Say::Nothing) {
                    LOG_INFO(Frontend,
                             "xr: stereo far {:.2f} deg, eyes allow {:.2f} deg (the game's far "
                             "scenery {:.2f} % of its picture's width apart, {:.3f} m on the "
                             "screen {:.0f} degrees wide at {:.2f} m, factor {:.2f}, IPD {:.1f} "
                             "mm; the eyes {:.2f} degrees inwards; the game's 3D x {:.2f} would "
                             "be parallel) ({:.2f} to {:.2f} deg over {} s, {})",
                             st.far_deg, st.allow_deg, far_width * 100.0f, st.separation_m,
                             Deg(2 * std::atan(g.solved.tan_half_y * g.solved.aspect)),
                             g.solved.distance, g.solved.factor, 2000.0f * g.half_ipd,
                             st.vergence_deg, st.natural_scale, g.far_min_deg, g.far_max_deg,
                             g.far_secs, Common::FrameLog::Why(say));
                    g.far_said_deg = st.far_deg;
                    g.far_said_allow = st.allow_deg;
                    g.far_said_diverges = st.diverges;
                    g.far_secs = 0;
                }
                if (st.diverges) {
                    LOG_WARNING(Frontend,
                                "xr: stereo: the far scenery needs the eyes to diverge {:.2f} "
                                "degrees, which they cannot: it doubles (the game's 3D, 3D "
                                "Depth {} %, is too deep for this screen)",
                                -st.vergence_deg, Settings::values.factor_3d.GetValue());
                }
            } else if (g.far_said_deg >= 0.0f) {
                g.far_gate.Reset(); // AzaharFrame integ8: not seen; logged at once when it is again
                g.far_said_deg = g.far_said_allow = -1.0f;
                g.far_said_diverges = false;
                g.far_secs = 0;
            }
            const int64_t keep_picture = a.last_picture;
            const uint32_t keep_run = a.run;
            a = {};
            a.since_ns = g.view.time;
            a.last_picture = keep_picture;
            a.run = keep_run;
        }
    } else {
        g.age = {};
    }

    if (g.engaged != was_engaged) {
        if (g.engaged) {
            if (IsViewMode(mode)) {
                LOG_INFO(Frontend,
                         "xr: immersive mode {} on (factor {:.2f} across, {:.2f} up and down, "
                         "head-locked top screen "
                         "{:.0f} x {:.0f} degrees, {:.1f} degrees from straight ahead, {:.1f} "
                         "to the right, gain {:.2f}, yaw gain {:.2f}, size {:.2f})",
                         mode, data.factor, data.factor_y,
                         Deg(2 * std::atan(g.solved.tan_half_y * g.solved.aspect)),
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

// AzaharFrame integ9: AZAHARFRAME_IMMERSIVE_POSITION_SCALE and what it gives now. AzaharFrame
// natural: the game camera's head position scale is the leaning scale (UpdateNatural), not known
// before a Full mode's first frame: the log says so.
static std::string PositionScaleLine() {
    const float base = g.leaning;
    if (!(base > 0.0f)) {
        return fmt::format("head position x{:.2f} (of the leaning scale, not known yet)",
                           g.tuning.position_scale);
    }
    return fmt::format("head position x{:.2f} (the game camera's head position scale {:.2f} x "
                       "{:.2f} = {:.2f})",
                       g.tuning.position_scale, base, g.tuning.position_scale,
                       base * g.tuning.position_scale);
}

void ReloadTestSwitches() {
    const Tuning t = ReadTuning();
    // Only the live ones (TestSwitches::ApplyFor); the distance, the FOV, Match Game Camera's
    // measure and the Wider level keep the values the screen was placed and sized with.
    g.tuning.anchor = t.anchor;
    g.tuning.anchor_max_ms = t.anchor_max_ms;
    g.tuning.anchor_eye = t.anchor_eye;
    g.tuning.anchor_depth_m = t.anchor_depth_m;
    g.tuning.fov_trace = t.fov_trace;
    g.tuning.menu_exact = t.menu_exact;
    g.tuning.local_space = t.local_space;
    g.tuning.tag_offset = t.tag_offset;
    g.tuning.position_scale = t.position_scale;
    g.tuning.world_tag = t.world_tag;
    g.tuning.hold_warp = t.hold_warp;   // AzaharFrame heldquad
    g.tuning.anchor_lag = t.anchor_lag; // AzaharFrame heldquad
    VideoCore::Immersive::g_content.SetFollowWorld(t.world_tag);
    VideoCore::Immersive::g_content.SetTagOffset(t.tag_offset);
    LOG_INFO(Frontend,
             "xr: immersive: test switches now: anchor {}, max {:.0f} ms, about the eye {}, anchor "
             "depth {:.2f}, world tag {}, tag offset {:+d}, {}, hold {}, anchor lag {:+d}",
             t.anchor ? "on" : "off", t.anchor_max_ms, t.anchor_eye ? "on" : "off",
             t.anchor_depth_m, t.world_tag ? "on" : "off", t.tag_offset, PositionScaleLine(),
             t.hold_warp ? "warp" : "rigid", t.anchor_lag);
}

void Install() {
    g = State{};
    g_top_aspect = kAspect3ds;
    g.tuning = ReadTuning();
    // AzaharFrame dbl: the test switches in effect (common/frame_test_switches.h).
    for (const std::string& sw : Common::FrameTestSwitches::InEffect()) {
        LOG_INFO(Frontend, "xr: test switch {}", sw);
    }
    VideoCore::Immersive::g_content.SetFollowWorld(g.tuning.world_tag);
    VideoCore::Immersive::g_content.SetTagOffset(g.tuning.tag_offset); // AzaharFrame integ9
    if (g.tuning.tag_offset != 0 || g.tuning.position_scale != 1.0f) {
        LOG_INFO(Frontend, "xr: immersive: picture tag offset {:+d} latch, {}", g.tuning.tag_offset,
                 PositionScaleLine());
    }
    g.game_fov_y_deg = g.tuning.game_fov_y_deg;
    g.wide_place = DefaultPlacement();
    g.head_place = DefaultHeadPlacement();
    VideoCore::Immersive::g_xr_active = true;
    g.callback_id = AddFrameCallback(OnFrame);
    LOG_INFO(Frontend,
             "xr: immersive: screen {:.2f} m ahead by default ({:.2f} m in mode 1), game "
             "camera {}, {} space",
             g.tuning.head_distance, g.tuning.distance,
             g.tuning.fov_from_env
                 ? fmt::format("{:.0f} degrees high (environment)", g.tuning.game_fov_y_deg)
                 : std::string{"angle from the game's setting"},
             g.tuning.local_space ? "LOCAL" : "VIEW");
    LOG_INFO(Frontend,
             "xr: immersive: Full modes' screen {} (AZAHARFRAME_IMMERSIVE_ANCHOR), pictures older "
             "than {:.0f} ms go back to the head",
             g.tuning.anchor ? "placed where the head was when its picture was drawn"
                             : "locked to the head at display time",
             g.tuning.anchor_max_ms);
    LOG_INFO(Frontend,
             "xr: immersive: the anchored screen {} (AZAHARFRAME_IMMERSIVE_ANCHOR_EYE)",
             g.tuning.anchor_eye ? "keeps its picture's direction about where the eye is now"
                                 : "stays at its picture's head pose, position too");
    LOG_INFO(Frontend,
             "xr: immersive: a picture shown again is held {} (AZAHARFRAME_IMMERSIVE_HOLD), "
             "anchor lag {:+d} refresh (AZAHARFRAME_IMMERSIVE_ANCHOR_LAG)",
             g.tuning.hold_warp ? "as the game would draw it now, through the screen's gains"
                                : "rigid in the room where it was drawn",
             g.tuning.anchor_lag);
}

void Uninstall() {
    if (g.callback_id) {
        RemoveFrameCallback(g.callback_id);
        g.callback_id = 0;
    }
    // The XR thread has stopped: this thread is the only writer now.
    VideoCore::Immersive::Publish({});
    VideoCore::Immersive::g_xr_active = false;
    VideoCore::Immersive::g_measure_camera = false; // AzaharFrame depth
}

float TopAspect() {
    return g_top_aspect.load();
}

// AzaharFrame turns: the per-second slip figures (AgeStats) for this frame's anchored screen
// (Screens, after OnFrame): for each eye as located, how far far scenery at the screen's middle is
// from where that eye saw it when the picture was drawn (AzaharFrame turns2: against the picture's
// own placement, so mode 2's shared quad, seen from half the IPD to the side, counts as 0 there).
void NoteSlip(bool anchored, const XrPosef& picture, const XrPosef (&top_pose)[2]) {
    auto& a = g.age;
    if (!anchored || !g.view.valid) {
        g.slip_prev_valid = false;
        return;
    }
    float err[2][2]{};
    float slip = 0.0f, step = 0.0f;
    for (int eye = 0; eye < 2; eye++) {
        const XrVector3f from =
            g.view.views_valid
                ? g.view.eye[eye].position
                : Add(g.view.head.position,
                      Rotate(g.view.head.orientation, {L::EyeX(eye, g.half_ipd), 0.0f, 0.0f}));
        const Input::Vec2f e =
            L::FarSceneryError(ToIn(picture), L::FullQuadInView(g.solved, g.mode, eye, g.half_ipd, g.quad_half),
                               L::EyeX(eye, g.half_ipd), ToIn(top_pose[eye]),
                               {from.x, from.y, from.z});
        err[eye][0] = Deg(e.x);
        err[eye][1] = Deg(e.y);
        slip = std::max(slip, std::hypot(err[eye][0], err[eye][1]));
        if (g.slip_prev_valid) {
            step = std::max(step, std::hypot(err[eye][0] - g.slip_prev[eye][0],
                                             err[eye][1] - g.slip_prev[eye][1]));
        }
    }
    a.slip_frames++;
    a.slip_sum_deg += slip;
    a.slip_max_deg = std::max<double>(a.slip_max_deg, slip);
    if (g.slip_prev_valid) {
        a.step_frames++;
        a.step_sum_deg += step;
        a.step_max_deg = std::max<double>(a.step_max_deg, step);
    }
    std::memcpy(g.slip_prev, err, sizeof(err));
    g.slip_prev_valid = true;
}

// AzaharFrame dbl: how much of the eyes' move the anchored screen follows while its picture is
// shown again (L::AnchorFollow), from AZAHARFRAME_IMMERSIVE_ANCHOR_DEPTH and the live terms.
float AnchorFollowNow(bool follow_eye) {
    if (!follow_eye) {
        return 0.0f;
    }
    const float depth = g.tuning.anchor_depth_m;
    if (depth == -1.0f) {
        return 1.0f;
    }
    if (depth >= 0.0f) {
        return depth == 0.0f ? 0.0f : std::clamp(1.0f - g.solved.distance / depth, -20.0f, 1.0f);
    }
    // AzaharFrame natural: the head moves the game camera by the leaning scale.
    const float scaler = std::max(g.leaning, 0.0f) * g.tuning.position_scale; // integ9
    const float focus = L::AnchorFollow(
        g.solved.distance, scaler,
        VideoCore::Immersive::g_stereo_zero_distance.load(std::memory_order_relaxed),
        g.solved.gain_x);
    // "mid": far scenery and the plane each step by half the plane's parallax.
    const float follow = depth == -3.0f ? 0.5f * (1.0f + focus) : focus;
    static float said = 2.0f;
    static int64_t said_ms = 0;
    const int64_t now = VideoCore::Immersive::SteadyMs();
    if (std::abs(follow - said) > 0.1f * std::max(1.0f, std::abs(said)) && now - said_ms > 5000) {
        said = follow;
        said_ms = now;
        LOG_INFO(Frontend,
                 "xr: anchor depth {}: the screen follows the eyes' move x{:.2f} (the game's "
                 "zero-parallax plane {:.1f} units ahead, head motion x{:.1f}, yaw gain {:.2f}, "
                 "screen at {:.2f} m)",
                 depth == -3.0f ? "mid" : "focus", follow,
                 VideoCore::Immersive::g_stereo_zero_distance.load(std::memory_order_relaxed),
                 scaler, g.solved.gain_x, g.solved.distance);
    }
    return follow;
}

ScreenLayers Screens(XrSpace local_space, XrSpace view_space, const ScreenPlacement& placement) {
    ScreenLayers out;
    out.top_space = local_space;
    out.top_pose[0] = out.top_pose[1] = placement.top_pose;
    out.top_size[0] = out.top_size[1] = placement.top_size;
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
        out.top_size[0] = out.top_size[1] = {width, width / TopAspect()};
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
        out.top_size[0] = out.top_size[1] = {g.solved.width(), g.solved.height()};
        // AzaharFrame doubling: where the head was when the picture on view was drawn. The game
        // camera followed that pose, so the screen placed there keeps the world where it is
        // while the head moves on; head-locked, the world swung with the head until the next
        // picture (2 to 4 display frames, 25 to 70 ms old) and jumped back: a double image on
        // fast head turns. SteamVR re-projects the layer for the head at display time.
        // AzaharFrame turns: about the eye where it is now (AnchoredQuadPose), so the eyes moving
        // with a turn do not slide the screen against its scenery.
        XrPosef anchor_head{{0, 0, 0, 1}, {0, 0, 0}};
        const L::HeldPose* held = nullptr;
        const bool anchored = g.tuning.anchor && AnchorPose(g.view.time, anchor_head, &held);
        const bool follow_eye = g.tuning.anchor_eye && g.view.valid;
        const float follow = AnchorFollowNow(follow_eye);
        // AzaharFrame heldquad: held as the game would draw the picture for the head now (the
        // picture is magnified, so held rigid only its middle stays with the scenery: the rest
        // stepped at the next picture, a double image while turning). The game camera now with
        // the picture's own gains and screen (the pose ring), so only the head's turn counts;
        // not in the ring (or AZAHARFRAME_IMMERSIVE_HOLD=rigid): rigid, as before.
        const L::HeldPose* hold =
            anchored && g.tuning.hold_warp && g.view.valid ? held : nullptr;
        // Mode 2's pictures come from the game's own two cameras: their far scenery says where
        // on the quad each eye's rays come from (the eye at parallel, the middle at 3D Depth 0).
        // Mode 3: from the eye (also with its eye quads moved in, Mode3Natural).
        const float eye_share = L::EyeShareOnQuad(
            g.solved, g.mode, g.half_ipd,
            VideoCore::Immersive::g_stereo_far_width.load(std::memory_order_relaxed));
        for (int eye = 0; eye < 2; eye++) {
            const Input::Posef in_view =
                L::FullQuadInView(g.solved, g.mode, eye, g.half_ipd, g.quad_half);
            if (anchored) {
                out.top_space = local_space;
                const L::HeldScreen h = L::HeldFullQuad(
                    g.solved, g.mode, eye, g.half_ipd, ToIn(anchor_head), ToIn(g.view.head),
                    follow, hold, ToIn(g.ref).orientation, eye_share, g.quad_half);
                out.top_pose[eye] = ToXr(h.pose);
                out.top_size[eye] = {h.width, h.height};
            } else if (g.tuning.local_space && g.view.valid) {
                out.top_space = local_space;
                out.top_pose[eye] = Compose(g.view.head, ToXr(in_view));
            } else {
                out.top_space = view_space;
                out.top_pose[eye] = ToXr(in_view);
            }
        }
        NoteSlip(anchored, anchor_head, out.top_pose);
        out.top_pointable = false;
        out.show_bottom = false;
    }
    const float top_distance = out.top_space == view_space || (g.engaged && IsViewMode(g.mode))
                                   ? g.solved.distance
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

bool TakeIpdSaved() { // AzaharFrame ipd
    return std::exchange(g.ipd_saved_now, false);
}

WiderStep StepWiderView(int dir) {
    if (!g.adjuster.Active() || !g.engaged || dir == 0) {
        return WiderStep::None;
    }
    if (!IsViewMode(g.mode)) {
        ShowToastText(std::string(Names::kRenderMargin) + ": Full Modes Only", ToastTime::Long);
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
    // AzaharFrame immersive2: the default size grows and shrinks with the view the game draws,
    // so the size stays: the picture keeps its scale.
    const float to = L::WiderFactor(after);
    g.wider_level = after;
    g.options.wider = to;
    auto& level = Settings::values.vr_immersive_wider_view_level;
    if (level.UsingGlobal()) {
        level.SetGlobal(false); // per game, as in the game's Properties
    }
    level.SetValue(after);
    LOG_INFO(Frontend, "xr: immersive: Render Margin {:.1f}x by the D-pad, size {:.2f}", to,
             g.head_place.size);
    return WiderStep::Changed;
}

WiderStep StepScale(int wider_steps, int taller_steps) {
    if (!g.adjuster.Active() || !g.engaged || (wider_steps == 0 && taller_steps == 0)) {
        return WiderStep::None;
    }
    const bool vertical = wider_steps == 0;
    if (!IsViewMode(g.mode)) {
        ShowToastText(std::string(vertical ? Names::kVerticalScale : Names::kHorizontalScale) +
                          ": Full Modes Only",
                      ToastTime::Long);
        return WiderStep::None;
    }
    // AzaharFrame kiu: both grips + D-pad: right wider, left narrower (Horizontal Scale); up
    // taller, down shorter (Vertical Scale). Per game, live (OnFrame reads them every frame).
    auto& setting =
        vertical ? Settings::values.vr_immersive_scale_y : Settings::values.vr_immersive_scale_x;
    const int before = L::ScalePercent(setting.GetValue());
    const int after = L::SteppedScale(before, vertical ? taller_steps : wider_steps);
    ShowToastText(L::ScaleToast(after, vertical)); // also at the ends, so the press shows
    if (after == before && setting.GetValue() == after) {
        LOG_INFO(Frontend, "xr: immersive: {} by the D-pad, at its limit", // AzaharFrame kiu2
                 L::ScaleToast(after, vertical));
        return WiderStep::None;
    }
    if (setting.UsingGlobal()) {
        setting.SetGlobal(false); // per game, as in the game's Properties
    }
    setting.SetValue(after);
    (vertical ? g.scale_y : g.scale_x) = after;
    (vertical ? g.options.squeeze_y : g.options.squeeze) = L::SqueezeForScale(after);
    LOG_INFO(Frontend, "xr: immersive: {} by the D-pad", L::ScaleToast(after, vertical));
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

void OnRecentred(XrTime change_time) {
    g.recentre_time = change_time; // AzaharFrame doubling: older pictures are not anchored
    g.poses.Clear();               // AzaharFrame heldquad
    // AzaharFrame immersive: SteamVR's recenter only re-centres. The screen keeps its size,
    // distance and pitch (the owner's report: each recenter after a resize put the screen back
    // to the default size, "farther away"); the rest pose is taken from the first head pose
    // located at or after the change.
    g.rest.Recentre(change_time);
}

bool OnResetPanels(bool stick_hold) {
    // AzaharFrame controls: the view is re-centred. AzaharFrame immersive: the immersive screen
    // of this mode goes back to its default placement for Reset Position, and for the stick hold
    // only while a grip holds the screen; a plain stick hold keeps its size, distance and pitch,
    // as SteamVR's recenter does (the owner: a hold after a resize lost the size). Reset
    // Position also puts the Immersive Offset back to the middle: everything to the defaults.
    g.rest.Recentre(0);
    g.poses.Clear(); // AzaharFrame heldquad
    bool offset_changed = false;
    if (!stick_hold && !(g.offset == L::LensOffset{})) {
        g.offset = {};
        Settings::values.vr_immersive_offset_x.SetValue(0.0f);
        Settings::values.vr_immersive_offset_y.SetValue(0.0f);
        LOG_INFO(Frontend, "xr: immersive offset reset (Reset Position)");
        offset_changed = true;
    }
    if (g.mode == 0 || !g.live || (stick_hold && !g.adjuster.Active())) {
        return offset_changed;
    }
    g.adjuster.Cancel();
    const bool wide = !IsViewMode(g.mode);
    L::Placement& p = wide ? g.wide_place : g.head_place;
    p = wide ? DefaultPlacement() : DefaultHeadPlacement();
    if (g.title_id && g.store.Get(g.title_id, wide)) {
        g.store.Erase(g.title_id, wide);
        SaveStore();
    }
    LogPlacement("reset", p, wide);
    return offset_changed;
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
    // AzaharFrame natural: in the Full modes the stick forward/back is World Scale (the screen's
    // distance at the same angle, and the world's size with it).
    float world_ratio = 1.0f;
    const L::Adjuster::Result r =
        g.adjuster.Update(in, frame, dt, enabled, place, wide ? nullptr : &world_ratio);
    if (!wide && r.active && world_ratio != 1.0f) {
        const L::Placement saved = L::Clamped(place);
        const float before =
            g.world_migration == 2
                ? L::ClampWorldScale(L::WorldScaleForDistance(
                      saved.distance, saved.size,
                      Settings::values.vr_immersive_follow_screen.GetValue()))
                : WorldScaleSetting();
        const float after = L::ClampWorldScale(before * world_ratio);
        if (after != before) {
            auto& ws = Settings::values.vr_immersive_world_scale;
            if (g.title_id) {
                ws.SetGlobal(false);
            }
            ws.SetValue(after);
            g.world_migration = 1; // the player's own value from now on
        }
        const long shown = std::lround(after);
        if (shown != g.world_toasted) {
            g.world_toasted = shown;
            ShowToastText(L::WorldScaleToast(after));
        }
    }
    if (!wide && r.active && !g.grab_active) {
        g.grab_size = place.size;
        g.world_toasted = -1;
    }
    const bool grabbed = g.grab_active;
    g.grab_active = r.active;
    if (!wide && r.released && grabbed) {
        if (g.world_toasted >= 0) {
            SetWorldScale(WorldScaleSetting(), false); // to the game's file
            LOG_INFO(Frontend, "xr: World Scale {:.1f} % by the grip (screen at {:.2f} m)",
                     WorldScaleSetting(), g.solved.distance);
        } else if (g.grab_size > 0.0f && std::abs(place.size / g.grab_size - 1.0f) > 0.02f &&
                   Settings::values.vr_immersive_follow_screen.GetValue()) {
            ShowToastText(Names::kWorldScaleFollowsToast);
        }
        g.world_toasted = -1;
    }
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
        if (g.title_id) {
            g.store.Set(g.title_id, wide, place);
            SaveStore();
        }
    }
}

void MeasureCameraAgain() { // AzaharFrame fovonce
    g_remeasure++;
}

std::optional<WorldScaleSave> TakeWorldScaleSave() { // AzaharFrame natural
    std::scoped_lock lock{g_world_save_mutex};
    std::optional<WorldScaleSave> out = g_world_save;
    g_world_save.reset();
    return out;
}

std::optional<int> TakeMeasuredFov() { // AzaharFrame fovonce
    const int fov = g_fov_written.exchange(0);
    return fov > 0 ? std::optional<int>{fov} : std::nullopt;
}

} // namespace FrameXr::Immersive

namespace FrameXr {

void SetGameTitleId(uint64_t title_id) {
    Immersive::g_title_id = title_id;
}

uint64_t GameTitleId() { // AzaharFrame move: the normal screens' per-game placement
    return Immersive::g_title_id.load();
}

} // namespace FrameXr
