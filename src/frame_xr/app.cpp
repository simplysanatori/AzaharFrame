// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// The frame loop thread and the public API of frame_xr.h.
//
// The loop re-submits the latest released image of each swapchain every display
// frame (the renderer releases at 60 Hz, the display runs at 90 / 120 Hz); it does
// no GPU work of its own. It holds the renderer's queue lock only around
// xrBeginFrame / xrEndFrame (XR_KHR_vulkan_enable2: the runtime may use the queue).

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include "common/logging/log.h"
#include "common/frame_flat.h"
#include "common/frame_vr_depth.h" // AzaharFrame vrsize
#include "common/settings.h" // AzaharFrame vrqol: Background
#include "common/stall_telemetry.h"
#include "common/thread.h"
#include "frame_xr/auto_shape.h" // AzaharFrame fixes7
#include "frame_xr/controls.h"
#include "frame_xr/dynamic_resolution.h" // AzaharFrame menuux
#include "frame_xr/immersive.h" // AzaharFrame immersive mode
#include "frame_xr/input/reticle.h" // AzaharFrame mouse
#include "frame_xr/input/screen_rig.h" // AzaharFrame vrsize
#include "frame_xr/layer_slots.h"     // AzaharFrame flash
#include "frame_xr/motion_clarity.h"   // AzaharFrame flash
#include "frame_xr/session.h"
#include "frame_xr/test_switches.h" // AzaharFrame integ9: the switch file, live
#include "frame_xr/toast_depth.h"   // AzaharFrame integ9: ScreenUse
#include "frame_xr/ui_sound.h" // AzaharFrame menuux
#include "frame_xr/vis/layer_plan.h" // AzaharFrame vis
#include "frame_xr/watchdog.h"
#include "frame_xr/xr_loader.h"
#include "video_core/vr_dynamic_resolution.h" // AzaharFrame menuux
#include "input_common/main.h" // AzaharFrame flat: SDL pump count

namespace FrameXr {

namespace {

std::unique_ptr<Runtime> g_runtime;

using Clock = std::chrono::steady_clock;

double Ms(Clock::duration d) {
    return std::chrono::duration<double, std::milli>(d).count();
}

std::string PhasesText(const LoopPhases& p) {
    return fmt::format("waitFrame {:.1f}, beginFrame {:.1f} + queue lock {:.1f}, frame work {:.1f}, "
                       "screens lock {:.1f}, layers {:.1f}, endFrame {:.1f} + queue lock {:.1f}, "
                       "events {:.1f}",
                       p.wait_ms, p.begin_ms, p.begin_lock_ms, p.frame_ms, p.screens_lock_ms,
                       p.layers_ms, p.end_ms, p.end_lock_ms, p.poll_ms);
}

const char* StateName(XrSessionState s) {
    switch (s) {
    case XR_SESSION_STATE_IDLE:
        return "IDLE";
    case XR_SESSION_STATE_READY:
        return "READY";
    case XR_SESSION_STATE_SYNCHRONIZED:
        return "SYNCHRONIZED";
    case XR_SESSION_STATE_VISIBLE:
        return "VISIBLE";
    case XR_SESSION_STATE_FOCUSED:
        return "FOCUSED";
    case XR_SESSION_STATE_STOPPING:
        return "STOPPING";
    case XR_SESSION_STATE_LOSS_PENDING:
        return "LOSS_PENDING";
    case XR_SESSION_STATE_EXITING:
        return "EXITING";
    default:
        return "UNKNOWN";
    }
}

XrCompositionLayerQuad MakeQuad(XrSpace space, const Quad& q) {
    XrCompositionLayerQuad layer{XR_TYPE_COMPOSITION_LAYER_QUAD};
    layer.layerFlags = q.flags;
    layer.space = space;
    layer.eyeVisibility = q.eye;
    layer.subImage.swapchain = q.swapchain;
    layer.subImage.imageRect = q.image_rect;
    layer.subImage.imageArrayIndex = 0;
    layer.pose = q.pose;
    layer.size = q.size;
    return layer;
}

// AzaharFrame curve: a game screen may be a cylinder layer; the rest are quads. Both start with
// the base header, so the list of layers points into this.
union ScreenOrQuadLayer {
    XrCompositionLayerQuad quad;
    XrCompositionLayerCylinderKHR cylinder;
    ScreenOrQuadLayer(const XrCompositionLayerQuad& q) : quad(q) {}
    ScreenOrQuadLayer(const XrCompositionLayerCylinderKHR& c) : cylinder(c) {}
    const XrCompositionLayerBaseHeader* Header() const {
        return reinterpret_cast<const XrCompositionLayerBaseHeader*>(this);
    }
};

/// The quad `q` bent as a section of a cylinder of `radius` (input/screen_rig.h CylinderFor).
XrCompositionLayerCylinderKHR MakeCylinder(XrSpace space, const Quad& q, float radius) {
    const Input::ScreenRig::Cylinder c = Input::ScreenRig::CylinderFor(
        {{q.pose.orientation.x, q.pose.orientation.y, q.pose.orientation.z, q.pose.orientation.w},
         {q.pose.position.x, q.pose.position.y, q.pose.position.z}},
        q.size.width, q.size.height, radius);
    XrCompositionLayerCylinderKHR layer{XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR};
    layer.layerFlags = q.flags;
    layer.space = space;
    layer.eyeVisibility = q.eye;
    layer.subImage.swapchain = q.swapchain;
    layer.subImage.imageRect = q.image_rect;
    layer.subImage.imageArrayIndex = 0;
    layer.pose = {{c.pose.orientation.x, c.pose.orientation.y, c.pose.orientation.z,
                   c.pose.orientation.w},
                  {c.pose.position.x, c.pose.position.y, c.pose.position.z}};
    layer.radius = c.radius;
    layer.centralAngle = c.central_angle;
    layer.aspectRatio = c.aspect_ratio;
    return layer;
}

// AzaharFrame flash: a quad after the screens and its space, for the fixed layer positions.
struct SlotQuad {
    XrSpace space = XR_NULL_HANDLE;
    Quad quad;
};

} // namespace

Runtime* GetRuntime() {
    return g_runtime.get();
}

// ---- Placement --------------------------------------------------------------

XrExtent2Df NormalTopSize(float scale) {
    // AzaharFrame: 240 px of the native screen per metre, so the top screen is 1 m high; its width
    // follows "VR Screen Shape" (Immersive::TopAspect, 5:3 = the 400x240 of the 3DS). vrsize:
    // times the scale the grip and the stick gave it.
    constexpr float kDensity = 240.0f;
    constexpr float kTopHeightPx = 240.0f;
    const float height = kTopHeightPx / kDensity * Input::ScreenRig::ClampScale(scale);
    return {height * Immersive::TopAspect(), height};
}

ScreenPlacement DefaultScreenPlacement() {
    // As CitraVR (GameSurfaceLayer.cpp): 240 px of the native screen per metre,
    // the top screen 2 m ahead at eye height; the bottom one 0.75 m lower, 0.5 m
    // nearer, 0.75^2 the scale, tilted 45 degrees back.
    constexpr float kPitch = -0.785398163f; // -45 degrees about +X
    ScreenPlacement p{};
    p.top_pose = {{0, 0, 0, 1}, {0.0f, 0.0f, -2.0f}};
    p.top_size = NormalTopSize(); // AzaharFrame: 400 / 240 m for the 3DS shape, wider otherwise
    p.bottom_pose = {{std::sin(kPitch / 2), 0, 0, std::cos(kPitch / 2)}, {0.0f, -0.75f, -1.5f}};
    p.bottom_size = NormalBottomSize();
    return p;
}

XrExtent2Df NormalBottomSize(float scale) {
    // CitraVR: 240 px per metre, 0.75^2 the scale. AzaharFrame move: times the second slot's
    // own scale (Detach Screens).
    constexpr float kDensity = 240.0f;
    const float s = 0.75f * 0.75f * Input::ScreenRig::ClampScale(scale);
    return {320.0f / kDensity * s, 240.0f / kDensity * s};
}

namespace {
// AzaharFrame layout: the second slot always follows the main one by the Screen Layout setting,
// so a switch, a grip move, a nudge, a size change and the reset all keep the layout.
// AzaharFrame move: unless it has a pose of its own (Detach Screens Secondary or Both, after a grab).
ScreenPlacement WithSecondSlot(ScreenPlacement place) {
    const uint32_t detach =
        Input::ScreenRig::ClampDetach(Settings::values.vr_move_screens.GetValue());
    if (place.second_free && detach != Input::ScreenRig::kDetachOff) {
        place.bottom_pose = place.second_pose;
        place.bottom_size = NormalBottomSize(place.second_scale);
        return place;
    }
    place.bottom_size = NormalBottomSize();
    const XrPosef& t = place.top_pose;
    const Input::Posef top{{t.orientation.x, t.orientation.y, t.orientation.z, t.orientation.w},
                           {t.position.x, t.position.y, t.position.z}};
    const Input::Posef b = Input::ScreenRig::SecondFromMain(
        top, place.top_scale, place.top_size.width, place.bottom_size.width,
        Settings::values.vr_screen_layout.GetValue());
    place.bottom_pose = {{b.orientation.x, b.orientation.y, b.orientation.z, b.orientation.w},
                         {b.position.x, b.position.y, b.position.z}};
    return place;
}
} // namespace

ScreenPlacement GetScreenPlacement() {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->placement_mutex};
        ScreenPlacement place = rt->placement;
        // The shape is per game; the stored size may be stale.
        place.top_size = NormalTopSize(place.top_scale);
        return WithSecondSlot(place);
    }
    return WithSecondSlot(DefaultScreenPlacement());
}

void SetScreenPlacement(const ScreenPlacement& placement) {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->placement_mutex};
        rt->placement = placement;
    }
}

// ---- Thread -----------------------------------------------------------------

void Runtime::StartThread() {
    stop = false;
    loop_alive = true; // SetVr works from now on
    Watchdog::Start();
    thread = std::thread([this] { FrameLoop(); });
}

void Runtime::StopThread() {
    stop = true;
    if (thread.joinable()) {
        // AzaharFrame flat2: a loop stuck in a runtime call never answers a stop, and a join
        // here kept the program from quitting until Steam killed it (2026-11-11, 23:26). Wait 6 s
        // (a session's end may take 3), say where it is, and leave it behind (Shutdown then leaves
        // what it may still use).
        const auto give_up = Clock::now() + std::chrono::seconds(6);
        while (loop_alive && Clock::now() < give_up) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (loop_alive) {
            LOG_ERROR(Frontend, "xr: the frame loop did not stop within 6 s ({}); left behind",
                      loop_phase.load());
            thread.detach();
            loop_abandoned = true;
        } else {
            thread.join();
        }
    }
    Watchdog::Stop();
}

void Runtime::OnState(XrSessionState new_state) {
    auto& xr = Xr();
    const XrSessionState old_state = state.exchange(new_state);
    LOG_INFO(Frontend, "xr: session {} -> {}", StateName(old_state), StateName(new_state));
    switch (new_state) {
    case XR_SESSION_STATE_READY: {
        XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
        bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        const XrResult r = xr.xrBeginSession(session, &bi);
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrBeginSession: {}", ResultName(r));
        } else {
            running = true;
        }
        break;
    }
    case XR_SESSION_STATE_STOPPING: {
        running = false;
        const XrResult r = xr.xrEndSession(session);
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrEndSession: {}", ResultName(r));
        }
        break;
    }
    case XR_SESSION_STATE_EXITING:
    case XR_SESSION_STATE_LOSS_PENDING:
        // The session is over (asked for, or the runtime ended it): the frame loop destroys it
        // and carries on flat. The instance stays.
        running = false;
        session_ended = true;
        break;
    default:
        break;
    }
    std::vector<StateCallback> callbacks;
    {
        std::scoped_lock lock{callbacks_mutex};
        for (auto& [id, cb] : state_callbacks) {
            callbacks.push_back(cb);
        }
    }
    for (auto& cb : callbacks) {
        cb(new_state);
    }
}

// Returns false once the instance is lost.
bool Runtime::PollEvents() {
    auto& xr = Xr();
    while (true) {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        const XrResult r = xr.xrPollEvent(instance, &event);
        if (r == XR_EVENT_UNAVAILABLE) {
            return true;
        }
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrPollEvent: {}", ResultName(r));
            lost = true;
            return false;
        }
        switch (event.type) {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            const auto& e = reinterpret_cast<const XrEventDataSessionStateChanged&>(event);
            OnState(e.state);
            break;
        }
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            LOG_ERROR(Frontend, "xr: instance loss pending (the runtime is going away)");
            running = false;
            lost = true;
            return false;
        case XR_TYPE_EVENT_DATA_EVENTS_LOST:
            LOG_WARNING(Frontend, "xr: events lost");
            break;
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING: {
            // SteamVR's recenter (hold the Steam button) moves the LOCAL space. Re-centre the
            // immersive view with it; AzaharFrame immersive: its screen keeps its placement.
            const auto& e =
                reinterpret_cast<const XrEventDataReferenceSpaceChangePending&>(event);
            if (e.referenceSpaceType == XR_REFERENCE_SPACE_TYPE_LOCAL) {
                LOG_INFO(Frontend, "xr: SteamVR recentred, re-centring the immersive view");
                Immersive::OnRecentred(e.changeTime);
                Vis::RecentreStats(); // AzaharFrame statspos: the stats in front of you again
                Input::RequestReticleRezero(); // AzaharFrame mouse: Bottom Screen Reticle
            }
            break;
        }
        default:
            // Interaction profile changes and the like: for the input code's
            // state callbacks to look up with their own calls.
            break;
        }
    }
}

void Runtime::LocateViews(XrTime time) {
    auto& xr = Xr();
    ViewPose pose;
    pose.time = time;
    XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = time;
    li.space = local_space;
    XrViewState vs{XR_TYPE_VIEW_STATE};
    XrView views[2]{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
    uint32_t count = 0;
    if (XR_SUCCEEDED(xr.xrLocateViews(session, &li, &vs, 2, &count, views)) && count == 2) {
        for (int e = 0; e < 2; e++) {
            pose.eye[e] = views[e].pose;
            pose.fov[e] = views[e].fov;
        }
        // AzaharFrame jump: views without a valid orientation are not a view to fit a screen to.
        pose.views_valid = (vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) != 0;
        constexpr XrViewStateFlags kTracked =
            XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_POSITION_TRACKED_BIT;
        pose.eye_positions_tracked = (vs.viewStateFlags & kTracked) == kTracked;
    }
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    if (XR_SUCCEEDED(xr.xrLocateSpace(view_space, local_space, time, &loc)) &&
        (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
        pose.head = loc.pose;
        pose.valid = true;
    }
    std::scoped_lock lock{pose_mutex};
    view_pose = pose;
}

void Runtime::NoteLoopPeriod(const LoopPhases& phases, LoopPhases& worst, XrDuration period_ns) {
    if (phases.period_ms > worst.period_ms) {
        worst = phases;
    }
    // A frame that took more than three display periods: where it went, at once (at most a few
    // lines a second; a stall shows as one long frame).
    const double period_ms = period_ns > 0 ? period_ns / 1e6 : 8.33;
    if (phases.period_ms > 3.0 * period_ms + 2.0) {
        static auto window = Clock::time_point{};
        static int in_window = 0;
        const auto now = Clock::now();
        if (now - window > std::chrono::seconds(1)) {
            window = now;
            in_window = 0;
        }
        if (in_window++ < 3) {
            LOG_WARNING(Frontend, "xr: display frame took {:.1f} ms: {}", phases.period_ms,
                        PhasesText(phases));
        }
    }
}

void Runtime::LogSummary(double seconds, double wait_avg_ms, double wait_max_ms,
                         const LoopPhases& worst) {
    const auto take = [](std::atomic<uint32_t>& a) { return a.exchange(0); };
    const uint32_t frames = take(stats.display_frames);
    uint32_t rel[kScreens], late[kScreens], unfinished[kScreens];
    for (std::size_t i = 0; i < kScreens; i++) {
        rel[i] = take(stats.releases[i]);
        late[i] = take(stats.late[i]);
        unfinished[i] = take(stats.unfinished[i]);
    }
    const uint32_t spill_shown = take(stats.spill_shown);
    const uint32_t spill_replaced = take(stats.spill_replaced);
    const uint32_t emu_waits = take(stats.emu_waits);
    const double emu_wait_ms = take(stats.emu_wait_us_max) / 1000.0;
    const uint32_t skipped = take(stats.skipped_acquires);
    const uint32_t end_errors = take(stats.end_frame_errors);
    const uint32_t eyes_mixed = take(stats.eyes_mixed); // AzaharFrame doubling
    const uint32_t emu_frames = take(stats.emu_frames);
    const uint64_t emu_sum_us = stats.emu_frame_us_sum.exchange(0);
    const uint32_t emu_max_us = take(stats.emu_frame_us_max);
    const uint32_t window_presents = take(stats.window_presents);
    // The queue: the XR thread's longest wait for it, and the emulator's longest submit holding it.
    auto& telemetry = Common::StallTelemetry::Get();
    const double queue_wait_ms = queue_lock.TakeMaxWaitUs() / 1000.0;
    const double emu_submit_ms = telemetry.queue_submit_max_us.exchange(0) / 1000.0;
    const double emu_submit_wait_ms = telemetry.queue_submit_wait_max_us.exchange(0) / 1000.0;
    const double window_present_ms = telemetry.window_present_max_us.exchange(0) / 1000.0;
    const double alloc_ms = telemetry.device_alloc_max_us.exchange(0) / 1000.0;
    const double alloc_mib = telemetry.device_alloc_max_bytes.exchange(0) / 1048576.0;
    Vis::NoteXrSecond({seconds, frames, late[0], wait_max_ms}); // AzaharFrame vrvis: stats panel
    // AzaharFrame menuux: Dynamic Resolution judges the second (the headset's frames).
    const auto gpu = VideoCore::DynamicResolution::TakeGpuSecond();
    DynamicResolution::NoteSecond({seconds, frames, display_period_ns / 1e6, emu_frames,
                                   gpu.frames, gpu.avg_ms, screens_live.load()});
    stats.window_us_sum.exchange(0);
    take(stats.window_us_max);
    // AzaharFrame ui2: and what Dynamic Resolution judged it by, every second (the logs of report
    // 14 had it only on a step).
    const std::string emu =
        (emu_frames ? fmt::format("emu frame {:.1f} ms avg ({:.1f} max)",
                                  emu_sum_us / 1000.0 / emu_frames, emu_max_us / 1000.0)
                    : std::string{"emu frame -"}) +
        fmt::format(", GPU behind {:.1f} ms avg ({:.1f} max) over {} frames, resolution -{}",
                    gpu.avg_ms, gpu.max_ms, gpu.frames, DynamicResolution::Drop());
    LOG_INFO(Frontend,
             "xr: {:.1f}s display {} frames, waitFrame period {:.2f} ms (max {:.2f}), releases "
             "TL/TR/B {}/{}/{}, late {}/{}/{} (copied in {}, replaced {}), emu waited on XR {} (max "
             "{:.2f} ms), before GPU done {}/{}/{}, acquire timeouts {}, "
             "endFrame errors {}, state {}, screens {}, {}, window presents {}, XR queue lock wait "
             "max {:.2f} ms, emu submit max {:.2f} ms (lock wait {:.2f}), window present max "
             "{:.2f} ms, vkAllocateMemory max {:.1f} ms (largest {:.0f} MiB), slowest frame "
             "{:.1f} ms ({}), eyes from different game frames {}",
             seconds, frames, wait_avg_ms, wait_max_ms, rel[0], rel[1], rel[2], late[0], late[1],
             late[2], spill_shown, spill_replaced, emu_waits, emu_wait_ms,
             unfinished[0], unfinished[1], unfinished[2], skipped, end_errors,
             StateName(state.load()), screens_live.load() ? "live" : "off", emu, window_presents,
             queue_wait_ms, emu_submit_ms, emu_submit_wait_ms, window_present_ms, alloc_ms, alloc_mib,
             worst.period_ms,
             PhasesText(worst), eyes_mixed);
}

void Runtime::LogFlatSummary(double seconds) {
    const auto take = [](std::atomic<uint32_t>& a) { return a.exchange(0); };
    if (DynamicResolution::Drop() != 0) { // AzaharFrame menuux: flat draws at the setting
        DynamicResolution::Reset();
    }
    const uint32_t presents = take(stats.window_presents);
    const uint64_t us_sum = stats.window_us_sum.exchange(0);
    const uint32_t us_max = take(stats.window_us_max);
    // AzaharFrame flat: what reaches the program from the gamepad, and whether input_common's
    // SDL thread still pumps SDL's events (no pumps: no controller state changes at all).
    static uint64_t last_pumps = 0;
    const uint64_t pumps = InputCommon::GetSDLPumpCount();
    const uint64_t pumped = pumps - std::exchange(last_pumps, pumps);
    LOG_INFO(Frontend,
             "xr: flat {:.1f}s, window presents {} ({:.2f} ms avg, {:.2f} max on the emu thread), "
             "screens {}, {}, SDL pumped {} times{}",
             seconds, presents, presents ? us_sum / 1000.0 / presents : 0.0, us_max / 1000.0,
             screens_live.load() ? "live" : "off",
             ui ? ui->TakeFlatInputSummary() : std::string{"no input"}, pumped,
             lost ? ", OpenXR gone" : "");
}

// Flat: no session. The gamepad (FlatFrame) about 120 times a second, a line a second.
void Runtime::FlatStep(double& log_seconds) {
    constexpr auto kPeriod = std::chrono::milliseconds(8);
    const auto now = Clock::now();
    Watchdog::FlatTick(); // AzaharFrame flatfix: a loop that stops here for 3 s is logged
    if (ui) {
        ui->FlatFrame(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch())
                          .count());
    }
    log_seconds += std::chrono::duration<double>(kPeriod).count();
    if (log_seconds >= 1.0) {
        Watchdog::FlatPhase("the flat summary line");
        LogFlatSummary(log_seconds);
        log_seconds = 0.0;
    }
    Watchdog::FlatPhase("the sleep between Flat passes");
    std::this_thread::sleep_for(kPeriod);
}

void Runtime::FrameLoop() {
    Common::SetCurrentThreadName("FrameXR");
    QueueLock::MarkXrThread();
    auto& xr = Xr();
    auto last_log = Clock::now();
    auto last_wait = Clock::time_point{};
    double wait_sum_ms = 0.0, wait_max_ms = 0.0;
    uint32_t wait_count = 0;
    // Where the time between two xrWaitFrame returns went: `phases` fills during a frame,
    // `worst` keeps the slowest period of the second for the summary line.
    LoopPhases phases, worst;
    auto loop_top = Clock::now();
    bool exit_requested = false;
    auto exit_deadline = Clock::time_point::max();
    double flat_seconds = 0.0;
    std::vector<ScreenOrQuadLayer> quad_layers; // AzaharFrame curve: the screens may be cylinders
    std::vector<Input::PanelQuad> toast_screens; // AzaharFrame kiu2: the screens, for the toast
    std::vector<Quad> ui_quads;
    std::vector<const XrCompositionLayerBaseHeader*> layer_ptrs;
    std::vector<Quad> top_quads; // AzaharFrame vrvis: over the controller models
    // AzaharFrame ctrlfront: the screens and panels under the models, for the next picture of
    // them (vis/model_plane.h).
    std::vector<Vis::Occluder> occluders;
    // AzaharFrame flash: every quad after the screens keeps its position in the list
    // (layer_slots.h), absent ones collapsed in place, so SteamVR never gives an overlay another
    // quad's texture.
    LayerSlots::Map<SlotQuad> layer_slots;
    std::vector<LayerSlots::Map<SlotQuad>::Out> slot_out;
    uint64_t slots_logged_added = 0, slots_logged_dropped = 0, slots_logged_resets = 0;
    // flash2: the most layers a frame carried (and its screens' layers) in the last second, and
    // the most logged for the list now in use.
    std::size_t layers_most = 0, layers_most_screens = 0, layers_logged_most = 0;
    // AzaharFrame flash: Motion Clarity (motion_clarity.h): the setting vr_motion_clarity with
    // AZAHARFRAME_XR_SINGLE_FLASH as its override, AZAHARFRAME_XR_FLASH_SLOW for slow games and
    // AZAHARFRAME_XR_FLASH_BACKING for an A/B of the backing, all live (test_switches.h).
    MotionClarity::Scheduler clarity;
    MotionClarity::Log clarity_log;
    std::optional<bool> clarity_override;
    bool flash_backing = true;
    const auto read_clarity_switches = [&] {
        clarity_override = MotionClarity::OverrideFrom(
            Common::FrameTestSwitches::Value("AZAHARFRAME_XR_SINGLE_FLASH"));
        clarity.SetSlow(
            MotionClarity::SlowFrom(Common::FrameTestSwitches::Value("AZAHARFRAME_XR_FLASH_SLOW")));
        flash_backing = TestSwitches::BackingFrom(
            Common::FrameTestSwitches::Value("AZAHARFRAME_XR_FLASH_BACKING"));
    };
    read_clarity_switches();
    // AzaharFrame integ9: the test-switch file read again when it changed (once a second, after
    // xrEndFrame): the live switches applied, each change logged and shown in a toast.
    const auto poll_test_switches = [&] {
        const std::vector<Common::FrameTestSwitches::Change> changes =
            Common::FrameTestSwitches::Live().Poll();
        if (changes.empty()) {
            return;
        }
        bool immersive = false;
        for (const Common::FrameTestSwitches::Change& c : changes) {
            LOG_INFO(Frontend, "xr: {}", TestSwitches::LogLine(c));
            if (c.env_wins || TestSwitches::ApplyFor(c.name) != TestSwitches::Apply::Live) {
                continue;
            }
            if (c.name == "AZAHARFRAME_XR_SINGLE_FLASH" || c.name == "AZAHARFRAME_XR_FLASH_SLOW" ||
                c.name == "AZAHARFRAME_XR_FLASH_BACKING") {
                read_clarity_switches(); // AzaharFrame flash
            } else if (c.name == "AZAHARFRAME_XR_IMAGE_LAG") {
                UpdateImageLag("test switch changed"); // AzaharFrame imglag
            } else if (c.name == "AZAHARFRAME_XR_REFRESH_RATE") {
                RequestRefreshRate();
            } else {
                immersive = true;
            }
        }
        if (immersive) {
            Immersive::ReloadTestSwitches();
        }
        const std::string toast = TestSwitches::ToastText(changes);
        if (!toast.empty()) {
            hud.ShowToastText(toast, ToastTime::Long);
        }
    };

    while (true) {
        loop_top = Clock::now();
        Watchdog::Beat("events");
        loop_phase = "xrPollEvent";
        if (instance && !lost && !PollEvents()) { // AzaharFrame flat2: no instance while flat
            // AzaharFrame flat: the runtime is gone (SteamVR quit or crashed). This loop used to
            // end here, which left the game holding whatever the controllers held last, flat
            // with no input at all and no way back. Now any session ends, the game gets nothing
            // held, and the loop carries on flat (the gamepad), with VR refused from here on.
            LOG_ERROR(Frontend, "xr: the OpenXR runtime is gone; flat from now on");
            want_vr = false;
            if (session) {
                exit_requested = false;
                DestroySession();
                flat_seconds = 0.0;
                if (ui) {
                    ui->NotifyVr(false);
                }
            }
            if (instance_sessions.EndInstanceAfterSession()) {
                DestroyXrInstance(); // AzaharFrame flat2: VR may ask for a new one later
            }
        }

        // ---- Flat: no session ----
        if (!session) {
            Watchdog::Idle();
            if (stop) {
                Watchdog::FlatEnd(); // AzaharFrame flatfix
                break;
            }
            if (want_vr) {
                Watchdog::FlatEnd(); // AzaharFrame flatfix: making a session may take a while
                LOG_INFO(Frontend, "xr: VR again: making a session");
                // AzaharFrame flat2: on a new XR instance (instance_policy.h).
                loop_phase = "a new XR instance and session";
                const bool made = RenewInstance() && CreateSession();
                if (!made) {
                    LOG_ERROR(Frontend, "xr: no session, staying flat");
                    want_vr = false;
                    if (instance_sessions.EndInstanceAfterSession()) {
                        DestroyXrInstance();
                    }
                }
                if (ui) {
                    ui->NotifyVr(made);
                }
                last_wait = Clock::time_point{};
                last_log = Clock::now();
                continue;
            }
            loop_phase = "flat";
            FlatStep(flat_seconds);
            continue;
        }

        // ---- Ending the session: shutdown, or flat asked for ----
        const bool end_wanted = stop || !want_vr;
        if (end_wanted && !exit_requested) {
            // Let the runtime take the session through STOPPING (xrEndSession) and EXITING, so
            // it ends cleanly. From here the renderer starts no new swapchain image.
            exit_requested = true;
            detaching = true;
            exit_deadline = Clock::now() + std::chrono::seconds(3);
            LOG_INFO(Frontend, "xr: ending the session ({})", stop ? "shutdown" : "flat");
            if (running) {
                const XrResult r = xr.xrRequestExitSession(session);
                LOG_INFO(Frontend, "xr: xrRequestExitSession: {}", ResultName(r));
                if (XR_FAILED(r)) {
                    exit_deadline = Clock::now(); // destroy it right away
                }
            } else {
                exit_deadline = Clock::now(); // not begun (idle): nothing to end first
            }
        }
        const bool timed_out = exit_requested && Clock::now() >= exit_deadline;
        if (session_ended || timed_out) {
            if (timed_out && !session_ended && running) {
                LOG_WARNING(Frontend, "xr: the runtime did not end the session in time");
            }
            const bool asked = exit_requested;
            exit_requested = false;
            exit_deadline = Clock::time_point::max();
            loop_phase = "destroying the session";
            DestroySession();
            if (stop) {
                break;
            }
            if (instance_sessions.EndInstanceAfterSession()) {
                // AzaharFrame flat2: the instance goes with its session; VR again makes a new one,
                // whose first session SteamVR gives the controllers to.
                loop_phase = "destroying the XR instance";
                DestroyXrInstance();
            }
            if (!asked) {
                // SteamVR ended it (quit from its dashboard, the headset gone): carry on flat.
                LOG_WARNING(Frontend, "xr: the runtime ended the session; flat now");
                want_vr = false;
            }
            flat_seconds = 0.0;
            if (ui) {
                ui->NotifyVr(false);
            }
            continue;
        }
        if (!running) {
            sent_serial = 0; // AzaharFrame imglag: nothing of ours in the pipeline meanwhile
            last_display_time = 0;
            Watchdog::Idle();
            loop_phase = "waiting for the session to run or end";
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        loop_phase = "VR frames";

        XrFrameState fs{XR_TYPE_FRAME_STATE};
        const auto wait_start = Clock::now();
        phases.poll_ms = Ms(wait_start - loop_top);
        Watchdog::Beat("xrWaitFrame");
        XrResult r = xr.xrWaitFrame(session, nullptr, &fs);
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrWaitFrame: {}", ResultName(r));
            NoEndFrame(); // AzaharFrame imglag
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        // AzaharFrame imglag: refreshes the loop missed (the compositor showed the last frame's
        // layers again: its image with the pose meant for the picture before it).
        missed_refreshes += ImageLag::MissedRefreshes(last_display_time, fs.predictedDisplayTime,
                                                      fs.predictedDisplayPeriod);
        last_display_time = fs.predictedDisplayTime;
        const auto now = Clock::now();
        phases.wait_ms = Ms(now - wait_start);
        if (last_wait != Clock::time_point{}) {
            const double ms = Ms(now - last_wait);
            wait_sum_ms += ms;
            wait_max_ms = std::max(wait_max_ms, ms);
            wait_count++;
            phases.period_ms = ms;
            NoteLoopPeriod(phases, worst, fs.predictedDisplayPeriod);
        }
        last_wait = now;
        phases = {};
        display_period_ns = fs.predictedDisplayPeriod; // AzaharFrame menuux

        auto mark = Clock::now();
        Watchdog::Beat("xrBeginFrame (with the queue lock)");
        r = queue_lock.With([&] { return xr.xrBeginFrame(session, nullptr); });
        phases.begin_lock_ms = QueueLock::LastWaitUs() / 1000.0;
        phases.begin_ms = Ms(Clock::now() - mark) - phases.begin_lock_ms;
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrBeginFrame: {}", ResultName(r));
            NoEndFrame(); // AzaharFrame imglag
            continue;
        }
        mark = Clock::now();
        Watchdog::Beat("frame work (views, callbacks, controls, panels)");

        LocateViews(fs.predictedDisplayTime);

        FrameInfo info{instance,
                       session,
                       local_space,
                       view_space,
                       fs.predictedDisplayTime,
                       fs.predictedDisplayPeriod,
                       state.load(),
                       fs.shouldRender == XR_TRUE,
                       screens_live.load()};
        // AzaharFrame fixes7: Screen Shape: Automatic, before the callbacks read the shape.
        AutoShape::Frame(info.screens_live,
                         Settings::values.vr_top_aspect.GetValue() == AutoShape::kAutomatic);
        {
            std::vector<FrameCallback> callbacks;
            {
                std::scoped_lock lock{callbacks_mutex};
                for (auto& [id, cb] : frame_callbacks) {
                    callbacks.push_back(cb);
                }
            }
            for (auto& cb : callbacks) {
                cb(info);
            }
        }
        ui_quads.clear();
        if (ui) {
            ui->Frame(*this, info, ui_quads);
        }

        // Layers. screens_mutex keeps the swapchains alive through xrEndFrame.
        phases.frame_ms = Ms(Clock::now() - mark);
        mark = Clock::now();
        Watchdog::Beat("the screens lock");
        std::scoped_lock screens_lock{screens_mutex};
        DropOldScreens(false); // AzaharFrame flash3: a replaced swapchain kept until the new one shows
        pacing.NoteLatchTime(); // AzaharFrame pacing: releases from now on are for the next frame
        phases.screens_lock_ms = Ms(Clock::now() - mark);
        mark = Clock::now();
        Watchdog::Beat("layers");
        quad_layers.clear();
        toast_screens.clear();
        occluders.clear();
        // AzaharFrame ctrlfront: a layer that hides the parts of a controller behind it, in LOCAL
        // space (a VIEW one placed with this frame's head); `always`: wherever they are (a
        // screen "Controllers in Front of Screen" leaves out).
        const auto add_occluder = [&](XrSpace space, const XrPosef& pose, XrExtent2Df size,
                                      float curve, XrEyeVisibility eye, bool always) {
            Input::Posef p{{pose.orientation.x, pose.orientation.y, pose.orientation.z,
                            pose.orientation.w},
                           {pose.position.x, pose.position.y, pose.position.z}};
            if (space == view_space) {
                const XrPosef h = LatestViewPose().head;
                const Input::Posef head{
                    {h.orientation.x, h.orientation.y, h.orientation.z, h.orientation.w},
                    {h.position.x, h.position.y, h.position.z}};
                p = {Input::Multiply(head.orientation, p.orientation),
                     Input::Transform(head, p.position)};
            } else if (space != local_space) {
                return;
            }
            const uint8_t eyes = eye == XR_EYE_VISIBILITY_LEFT    ? 1
                                 : eye == XR_EYE_VISIBILITY_RIGHT ? 2
                                                                  : 3;
            occluders.push_back({p, size.width, size.height, curve, eyes, always});
        };
        const Vis::ControllersInFront in_front =
            Vis::ControllersInFrontFrom(Settings::values.vr_controllers_in_front.GetValue());
        // The immersive top screen, when the controllers may go in front of it, hides nothing:
        // what it shows is far away (or all round).
        bool top_immersive = false;
        bool screens_swapped = false;
        // AzaharFrame doubling: the two eyes' top images of separate swapchains from different
        // game frames (a release of the set came between); never with set releases or packed.
        if (!packed_top && screens_live) {
            const ScreenChain& l = screens[static_cast<std::size_t>(VideoCore::XrScreen::TopLeft)];
            const ScreenChain& r = screens[static_cast<std::size_t>(VideoCore::XrScreen::TopRight)];
            if (l.has_image && r.has_image && l.frame_serial != r.frame_serial) {
                stats.eyes_mixed++;
            }
        }
        // AzaharFrame autosave: the normal top screen, for the "Autosaved" label over its corner
        bool label_top_ok = false;
        XrPosef label_top_pose{{0, 0, 0, 1}, {0, 0, 0}};
        XrExtent2Df label_top_size{0.0f, 0.0f};
        Hud::BadgeAnchor badge_anchor; // AzaharFrame shaderbadge: the main screen, if any
        // AzaharFrame imglag: whether this frame's layers carry the top screen, and its picture's
        // serial (what the compositor shows on the next refresh, with the image lag).
        bool top_carried = false;
        uint64_t top_serial = 0;
        if (fs.shouldRender && screens_live) {
            ScreenPlacement place;
            {
                std::scoped_lock lock{placement_mutex};
                place = placement;
            }
            place.top_size = NormalTopSize(place.top_scale); // AzaharFrame: "VR Screen Shape"
            place = WithSecondSlot(place); // AzaharFrame layout: as GetScreenPlacement
            // Immersive mode (immersive.cpp) may move the top screen and hide the bottom one.
            const Immersive::ScreenLayers shown = Immersive::Screens(local_space, view_space, place);
            // AzaharFrame flash: Motion Clarity (motion_clarity.h; it began as the test switch
            // AZAHARFRAME_XR_SINGLE_FLASH): each game picture of the top screen on one display
            // refresh only, in the Full modes while the game is played, at its full rate. Judged
            // on the picture this refresh would show (with the image lag, the one the last frame
            // carried), so no picture is seen on two refreshes. Held (as without it) for a game
            // under the full rate. flash2: with the VR menu, the game's keyboard, a pause or the
            // bottom screen in focus (immersive paused, the game still in a Full mode), while the
            // rate is learnt and for a picture late past its time, the newest picture is lit on
            // the same beat, so the brightness never steps; the backing stays all that time.
            const VrUi::ScreenDraw draw = ui ? ui->ScreenDrawFor(shown) : VrUi::ScreenDraw{};
            bool hide_top = false;
            MotionClarity::TopLayers top_layers;
            Quad backing; // AzaharFrame flash: the backing's image (Hud::BackingImage)
            {
                // AzaharFrame flash3: the swapchain the layer shows (the old one while a new one
                // has no picture: the same picture again, a late one, not a gap).
                const ScreenChain& t =
                    LayerChain(static_cast<std::size_t>(VideoCore::XrScreen::TopLeft));
                MotionClarity::Refresh rf;
                rf.on = MotionClarity::InEffect(Settings::values.vr_motion_clarity.GetValue(),
                                                clarity_override, shown.full_mode);
                // flash2: in a Full mode, played only while immersive is engaged (paused, the
                // top screen is not the head-driven one: on the beat).
                rf.steady = ui && ui->PlayOnView() && !draw.swapped && shown.show_top &&
                            (shown.head_driven || !shown.full_mode);
                rf.time_ns = fs.predictedDisplayTime; // flash2: how long a gap lasts
                rf.period_ns = fs.predictedDisplayPeriod; // flash2: the beat while learning
                if (t.handle != XR_NULL_HANDLE && t.has_image) {
                    const uint64_t on_view = ImageLag::SerialOnView(
                        t.frame_serial, sent_serial.load(), image_lag.load());
                    const std::array<ShownPicture, kShownPictures> pics = ShownPictures();
                    const ShownPicture* p = ImageLag::FindPicture(pics, on_view);
                    rf.picture = MotionClarity::PictureId(p ? p->picture : 0, on_view);
                }
                const MotionClarity::Decision d = clarity.Next(rf);
                // AzaharFrame flash: SteamVR makes each quad layer the overlay of its index and
                // sets its texture, bounds, transform and size in separate calls; leaving the
                // screens out hid and showed their overlays 60 times a second with nothing of ours
                // over the view, and borrowing another swapchain for the dark refreshes showed
                // that swapchain's other content in the screen's place. So the backing is a layer
                // of its own under each eye's screen while Motion Clarity is in effect, and on a
                // dark refresh the screen is only shrunk to nothing (TopLayersFor).
                top_layers = MotionClarity::TopLayersFor(rf.on, flash_backing,
                                                         hud.BackingImage(backing), !d.show);
                hide_top = top_layers.omit || top_layers.collapse;
                clarity_log.Note(hide_top ? MotionClarity::Why::Dark
                                 : d.show ? d.why
                                          : MotionClarity::Why::Off);
            }
            const auto add_screen = [&](VideoCore::XrScreen s, XrSpace space, const XrPosef& pose,
                                        XrExtent2Df size, XrEyeVisibility eye, float curve) {
                // AzaharFrame doubling: a packed top screen is one swapchain, the left eye in
                // the upper half and the right eye in the lower one (kXrPackedGap between).
                const bool top = s != VideoCore::XrScreen::Bottom;
                // AzaharFrame flash3: LayerChain: while a swapchain made anew has no picture, the
                // old one with its last picture (no blink of the compositor's background).
                const ScreenChain& chain = LayerChain(static_cast<std::size_t>(
                    packed_top && top ? VideoCore::XrScreen::TopLeft : s));
                // AzaharFrame integ9: the toast's inputs take the screen whether or not its layer
                // is left out this refresh (SINGLE_FLASH), so the toast's depth holds still.
                const ToastDepth::ScreenUse use =
                    ToastDepth::UseOf(chain.handle != XR_NULL_HANDLE && chain.has_image,
                                      top && top_layers.omit);
                if (use.toast_input) {
                    // AzaharFrame kiu2: where it is in LOCAL space, for the toast (toast_depth.h).
                    Input::PanelQuad tq;
                    tq.pose = {{pose.orientation.x, pose.orientation.y, pose.orientation.z,
                                pose.orientation.w},
                               {pose.position.x, pose.position.y, pose.position.z}};
                    if (space == view_space) {
                        const XrPosef h = LatestViewPose().head;
                        const Input::Posef head{
                            {h.orientation.x, h.orientation.y, h.orientation.z, h.orientation.w},
                            {h.position.x, h.position.y, h.position.z}};
                        tq.pose = {Input::Multiply(head.orientation, tq.pose.orientation),
                                   Input::Transform(head, tq.pose.position)};
                    }
                    tq.width_m = size.width;
                    tq.height_m = size.height;
                    tq.visible = space == view_space || space == local_space;
                    tq.curve_radius = curve;
                    toast_screens.push_back(tq);
                }
                // AzaharFrame ctrlfront: the screen hides the controllers behind it whether or
                // not its layer is left out or shrunk this refresh (Motion Clarity: the backing
                // is in its place), as the toast's input: the next picture of the controllers is
                // drawn against it, and one drawn without it showed through the screen every
                // other picture. By depth when the choice
                // includes this screen's slot (Primary: the top screen, or the bottom one swapped
                // in), else in front of the controllers wherever they are.
                if (use.toast_input) {
                    const bool by_depth = Vis::InFrontOfScreen(in_front, top != screens_swapped);
                    if (!by_depth || !top || !top_immersive) {
                        add_occluder(space, pose, size, curve, eye, !by_depth);
                    }
                }
                if (!use.layer) {
                    return; // no picture, or (AZAHARFRAME_XR_FLASH_BACKING=0) shown once already
                }
                if (s == VideoCore::XrScreen::TopLeft) { // AzaharFrame imglag
                    top_carried = true;
                    top_serial = chain.frame_serial;
                }
                // AzaharFrame flash: the black backing, a layer of its own under the screen, the
                // same pose, size, shape and eye, on every refresh while Motion Clarity is on.
                if (top && top_layers.backing) {
                    Quad b = backing;
                    b.pose = pose;
                    b.size = size;
                    b.eye = eye;
                    if (curve > 0.0f) {
                        quad_layers.push_back(MakeCylinder(space, b, curve));
                    } else {
                        quad_layers.push_back(MakeQuad(space, b));
                    }
                }
                Quad q;
                q.swapchain = chain.handle;
                q.image_rect = {{0, 0},
                                {static_cast<int32_t>(chain.width),
                                 static_cast<int32_t>(chain.height)}};
                if (packed_top && top) {
                    const int32_t half =
                        static_cast<int32_t>((chain.height - VideoCore::kXrPackedGap) / 2);
                    q.image_rect.extent.height = half;
                    if (s == VideoCore::XrScreen::TopRight) {
                        q.image_rect.offset.y =
                            half + static_cast<int32_t>(VideoCore::kXrPackedGap);
                    }
                }
                q.pose = pose;
                q.size = size;
                if (top && top_layers.collapse) { // AzaharFrame flash: a dark refresh
                    q.size = {size.width * MotionClarity::kCollapse,
                              size.height * MotionClarity::kCollapse};
                }
                q.eye = eye;
                if (curve > 0.0f) {
                    quad_layers.push_back(MakeCylinder(space, q, curve)); // AzaharFrame curve
                } else {
                    quad_layers.push_back(MakeQuad(space, q));
                }
            };
            // AzaharFrame screenswap: with the bottom screen in focus the screens trade places
            // (immersive is paused then); a hidden screen is not added. (AzaharFrame flash: `draw`
            // is taken above, for Motion Clarity.)
            top_immersive = !draw.swapped && !shown.top_pointable; // AzaharFrame ctrlfront
            screens_swapped = draw.swapped;
            label_top_ok = !draw.swapped && shown.show_top && shown.top_pointable &&
                           shown.top_space == local_space;
            label_top_pose = shown.top_pose[0];
            label_top_size = shown.top_size[0];
            // AzaharFrame shaderbadge: the screen in the main slot (the bottom one while it is in
            // focus), else the immersive top screen.
            // AzaharFrame fixes7: and whether that screen is on view with a picture (the badge
            // waits for it, never floating in a black view at boot).
            const auto has_picture = [&](VideoCore::XrScreen s) {
                const bool top = s != VideoCore::XrScreen::Bottom;
                const ScreenChain& chain = LayerChain(static_cast<std::size_t>( // flash3
                    packed_top && top ? VideoCore::XrScreen::TopLeft : s));
                return chain.handle != XR_NULL_HANDLE && chain.has_image;
            };
            badge_anchor.live = true;
            if (draw.swapped) {
                badge_anchor.pictures = has_picture(VideoCore::XrScreen::Bottom);
                badge_anchor.screen = true;
                badge_anchor.screen_pose = draw.bottom_pose;
                badge_anchor.screen_size = draw.bottom_size;
                badge_anchor.curve = draw.bottom_curve;
            } else if (shown.show_top && shown.top_pointable && shown.top_space == local_space) {
                badge_anchor.pictures = has_picture(VideoCore::XrScreen::TopLeft);
                badge_anchor.screen = true;
                badge_anchor.screen_pose = shown.top_pose[0];
                badge_anchor.screen_size = shown.top_size[0];
                badge_anchor.curve = draw.top_curve;
            } else if (shown.show_top) {
                badge_anchor.pictures = has_picture(VideoCore::XrScreen::TopLeft);
                badge_anchor.immersive = true;
                badge_anchor.immersive_in_view = shown.top_space == view_space;
                badge_anchor.immersive_pose[0] = shown.top_pose[0];
                badge_anchor.immersive_pose[1] = shown.top_pose[1];
                badge_anchor.immersive_size = shown.top_size[0]; // AzaharFrame heldquad: eye 0
            }
            // AzaharFrame vrsize: a normal top screen bigger than its normal size gets that much
            // less 3D depth (common/frame_vr_depth.h); the immersive and the swapped one do not.
            // AzaharFrame move: swapped, the top screen is in the second slot at its own scale.
            Settings::frame_vr_depth_scale =
                !shown.top_pointable ? 1.0f
                : draw.swapped
                    ? Input::ScreenRig::DepthScale(place.second_free ? place.second_scale : 1.0f)
                    : Input::ScreenRig::DepthScale(place.top_scale);
            // Quad layers are drawn in the order they are submitted, with no depth test: the
            // farther screen goes first, so the nearer one is never drawn through.
            if (draw.swapped) {
                // The bottom screen in the main slot, the top one in the nearer secondary slot.
                add_screen(VideoCore::XrScreen::Bottom, local_space, draw.bottom_pose,
                           draw.bottom_size, XR_EYE_VISIBILITY_BOTH, draw.bottom_curve);
                if (draw.top_visible) {
                    add_screen(VideoCore::XrScreen::TopLeft, local_space, draw.top_pose,
                               draw.top_size, XR_EYE_VISIBILITY_LEFT, draw.top_curve);
                    add_screen(VideoCore::XrScreen::TopRight, local_space, draw.top_pose,
                               draw.top_size, XR_EYE_VISIBILITY_RIGHT, draw.top_curve);
                }
            } else {
                // Each eye sees its own top image on the same quad.
                const auto add_bottom = [&] {
                    if (shown.show_bottom && draw.bottom_visible) {
                        add_screen(VideoCore::XrScreen::Bottom, local_space, shown.bottom_pose,
                                   shown.bottom_size, XR_EYE_VISIBILITY_BOTH, draw.bottom_curve);
                    }
                };
                if (shown.bottom_first) {
                    add_bottom();
                }
                if (shown.show_top) {
                    // AzaharFrame curve: draw.top_curve is 0 for the immersive screen.
                    // AzaharFrame heldquad: each eye's pose and size (a held picture is resized
                    // about each eye); the toast takes eye 0's first.
                    add_screen(VideoCore::XrScreen::TopLeft, shown.top_space, shown.top_pose[0],
                               shown.top_size[0], XR_EYE_VISIBILITY_LEFT, draw.top_curve);
                    add_screen(VideoCore::XrScreen::TopRight, shown.top_space, shown.top_pose[1],
                               shown.top_size[1], XR_EYE_VISIBILITY_RIGHT, draw.top_curve);
                }
                if (!shown.bottom_first) {
                    add_bottom();
                }
            }
        }
        // AzaharFrame flash: the quads after the screens go through the slot map.
        const std::size_t screen_layers = quad_layers.size();
        const auto slot_add = [&](LayerSlots::Group group, XrSpace space, const Quad& q) {
            // The rect's size tells an atlas's regions apart (the vis atlas: help, buttons,
            // beams); the toast, the hint chip, the label and the badge are one quad each whose
            // rect changes with its text, so they key on their group alone.
            const bool one = group != LayerSlots::Group::Ui && group != LayerSlots::Group::Top &&
                             group != LayerSlots::Group::Extra;
            layer_slots.Add(group, reinterpret_cast<uint64_t>(q.swapchain),
                            one ? 0 : q.image_rect.extent.width,
                            one ? 0 : q.image_rect.extent.height, SlotQuad{space, q});
        };
        if (fs.shouldRender) {
            // A new list when the panels' context changes or the screens' layers change in number
            // (a game starting or stopping, Motion Clarity's backing coming or going): the slots
            // after them would all move anyway.
            const uint32_t screens_n = static_cast<uint32_t>(std::min<std::size_t>(screen_layers, 15));
            layer_slots.Begin(((ui ? ui->LayerContext() : 0) * 2 + (screens_live ? 1 : 0)) * 16 +
                              screens_n);
            // integ13: the HUD's quads have their slots from the start of the list, in their draw
            // order (as slot_add keys them: their group and the HUD's one swapchain), so a toast
            // or a label that first shows mid-game fills its own slot and never moves the Top
            // group (the controllers' eye quads, beams, dots) to other overlays.
            // flash2: the hint chip only in the game's list: it shows when a grip takes a screen
            // in play, never in the VR menu or the keyboard, whose lists keep that layer for
            // Motion Clarity's backing (16 in all with both controllers pointing at the menu).
            Quad hud_stand_in;
            if (hud.HudPlaceholder(hud_stand_in)) {
                const uint64_t hud_chain = reinterpret_cast<uint64_t>(hud_stand_in.swapchain);
                const bool in_game = !ui || ui->LayerContext() == VrUi::kGameContext;
                for (const LayerSlots::Group g : {LayerSlots::Group::Toast, LayerSlots::Group::Hint,
                                                  LayerSlots::Group::Label,
                                                  LayerSlots::Group::Badge}) {
                    if (g == LayerSlots::Group::Hint && !in_game) {
                        continue;
                    }
                    layer_slots.Reserve(g, hud_chain, 0, 0, SlotQuad{local_space, hud_stand_in});
                }
            }
        }
        if (fs.shouldRender) {
            std::scoped_lock lock{quads_mutex};
            std::vector<const Quad*> extra;
            for (const auto& [id, q] : quads) {
                if (q.visible && q.swapchain && (q.with_game || !screens_live)) {
                    extra.push_back(&q);
                }
            }
            std::stable_sort(extra.begin(), extra.end(),
                             [](const Quad* a, const Quad* b) { return a->order < b->order; });
            // AzaharFrame ctrlfront: every panel hides a controller behind it but the
            // see-through ones (the dim quad, the pointer dots, the mouse ring). The toast, the
            // hint chip, the Autosaved label and the shader badge below never do: small, brief,
            // with clear margins round their art, and in front of everything anyway.
            const auto panel_occluder = [&](const Quad& q) {
                if (!ui || ui->HidesControllers(q.swapchain)) {
                    add_occluder(local_space, q.pose, q.size, 0.0f, q.eye, false);
                }
            };
            for (const Quad* q : extra) {
                slot_add(LayerSlots::Group::Extra, local_space, *q);
                panel_occluder(*q);
            }
            // The picker and the pointer dot last (the dot on top of everything).
            for (const Quad& q : ui_quads) {
                slot_add(LayerSlots::Group::Ui, local_space, q);
                panel_occluder(q);
            }
            // AzaharFrame vrqol: a toast (Quick Saved ...) in front of everything.
            Quad toast;
            if (hud.ToastQuad(LatestViewPose(), fs.predictedDisplayTime, toast, &toast_screens)) {
                slot_add(LayerSlots::Group::Toast, local_space, toast);
            }
            // AzaharFrame controls: the grip hint chip under the held screen.
            Quad hint;
            if (hud.HintQuad(fs.predictedDisplayTime, hint, LatestViewPose(), &toast_screens)) {
                slot_add(LayerSlots::Group::Hint, local_space, hint);
            }
            // AzaharFrame autosave: "Autosaved" over the top screen's corner
            Quad label;
            if (hud.LabelQuad(LatestViewPose(), label_top_ok, label_top_pose, label_top_size,
                              fs.predictedDisplayTime, label)) {
                slot_add(LayerSlots::Group::Label, local_space, label);
            }
            // AzaharFrame shaderbadge: "Preparing Shaders 3 / 9" in the main screen's corner
            Quad badge;
            bool badge_in_view = false;
            if (hud.BadgeQuad(LatestViewPose(), badge_anchor, fs.predictedDisplayTime, badge,
                              badge_in_view)) {
                slot_add(LayerSlots::Group::Badge, badge_in_view ? view_space : local_space, badge);
            }
        }
        // AzaharFrame vrqol: "Background: Passthrough". The room shows behind a transparent
        // projection layer when the frame is ended with the alpha blend mode.
        XrCompositionLayerProjection passthrough_layer{};
        XrCompositionLayerProjectionView passthrough_views[2]{};
        bool passthrough = false;
        if (fs.shouldRender && hud.PassthroughActive(*this)) {
            passthrough = hud.PassthroughLayer(LatestViewPose(), local_space, passthrough_layer,
                                               passthrough_views);
        }
        // AzaharFrame vrvis: the controller models' projection layer over all that, and the
        // beams and pointer dots over the models (frame_xr/vis/visuals.h).
        const XrCompositionLayerBaseHeader* models_layer = nullptr;
        std::size_t models_at = quad_layers.size();
        top_quads.clear();
        if (ui) {
            ui->SetOccluders(std::move(occluders)); // AzaharFrame ctrlfront: for the next picture
        }
        if (fs.shouldRender && ui) {
            // flash2: what comes before the Top group (its own slots from earlier frames are its
            // room, not in use).
            models_layer = ui->TopLayers(
                top_quads, screen_layers + layer_slots.SizeBefore(LayerSlots::Group::Top) +
                               (passthrough ? 1 : 0));
            for (const Quad& q : top_quads) {
                slot_add(LayerSlots::Group::Top, local_space, q);
            }
        }
        if (fs.shouldRender) {
            // AzaharFrame flash: the slots in their fixed order, absent quads collapsed in place
            // (their own swapchain and rect, size ~0: the compositor draws nothing of them). The
            // models' layer goes before the first Top slot. Within the runtime's limit, less the
            // screens and the one projection layer.
            const std::size_t projection = models_layer || passthrough ? 1 : 0;
            const std::size_t limit = ui ? ui->MaxLayers() : 64;
            const std::size_t room =
                limit > screen_layers + projection ? limit - screen_layers - projection : 0;
            layer_slots.End(room, slot_out, [](SlotQuad& s) {
                s.quad.size = {s.quad.size.width * LayerSlots::kCollapse,
                               s.quad.size.height * LayerSlots::kCollapse};
            });
            models_at = screen_layers;
            for (const auto& o : slot_out) {
                quad_layers.push_back(MakeQuad(o.quad.space, o.quad.quad));
                if (o.group < LayerSlots::Group::Top) {
                    models_at++;
                }
            }
        }
        // AzaharFrame vis: one projection layer a frame (vis/layer_plan.h). With the models
        // showing, their layer stands in for the passthrough square; the blend mode stays.
        Vis::AssembleLayers(
            layer_ptrs,
            passthrough ? reinterpret_cast<const XrCompositionLayerBaseHeader*>(&passthrough_layer)
                        : nullptr,
            models_layer, models_at, quad_layers.size(),
            [&](std::size_t i) { return quad_layers[i].Header(); });
        if (ui && layer_ptrs.size() > ui->MaxLayers()) {
            // Never more than the runtime takes: xrEndFrame would drop the whole frame.
            static int said = 0;
            if (said++ < 5) {
                LOG_WARNING(Frontend, "xr: {} layers, the runtime takes {}: the last ones dropped",
                            layer_ptrs.size(), ui->MaxLayers());
            }
            layer_ptrs.resize(ui->MaxLayers());
        }
        if (fs.shouldRender && layer_ptrs.size() >= layers_most) { // flash2: for the log
            layers_most = layer_ptrs.size();
            layers_most_screens = screen_layers;
        }
        XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
        ei.displayTime = fs.predictedDisplayTime;
        ei.environmentBlendMode = passthrough ? XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND
                                              : XR_ENVIRONMENT_BLEND_MODE_OPAQUE; // AzaharFrame vrqol
        ei.layerCount = static_cast<uint32_t>(layer_ptrs.size());
        ei.layers = layer_ptrs.empty() ? nullptr : layer_ptrs.data();
        phases.layers_ms = Ms(Clock::now() - mark);
        mark = Clock::now();
        Watchdog::Beat("xrEndFrame (with the queue lock)");
        r = queue_lock.With([&] { return xr.xrEndFrame(session, &ei); });
        phases.end_lock_ms = QueueLock::LastWaitUs() / 1000.0;
        phases.end_ms = Ms(Clock::now() - mark) - phases.end_lock_ms;
        if (XR_FAILED(r)) {
            stats.end_frame_errors++;
            static int logged = 0;
            if (logged++ < 10) {
                LOG_ERROR(Frontend, "xr: xrEndFrame: {}", ResultName(r));
            }
        }
        stats.display_frames++;
        // AzaharFrame imglag: what this xrEndFrame carried (still under the screens lock, so no
        // release came between): with the image lag, the picture on view on the next refresh.
        sent_serial =
            ImageLag::SentAfterEnd(top_carried, fs.shouldRender, XR_SUCCEEDED(r), top_serial);
        if (sent_serial == 0 && screens_live) { // counted while a game is on view
            none_sent[static_cast<std::size_t>(
                ImageLag::WhyNoneSent(fs.shouldRender, XR_SUCCEEDED(r)))]++;
        }
        if (!set_release && releasing.load() > 0) {
            serial_trailing++; // a release outside the lock was in progress
        }
        {
            // AzaharFrame pacing: the game frame this frame showed (the parity gate, the log).
            uint64_t shown_serial = 0;
            for (std::size_t i = 0; i < kScreens; i++) {
                const ScreenChain& chain = LayerChain(i); // AzaharFrame flash3: as the layers
                if (chain.handle && chain.has_image) {
                    shown_serial = std::max(shown_serial, chain.frame_serial);
                }
            }
            pacing.NoteLatch(shown_serial, fs.predictedDisplayPeriod,
                             fs.shouldRender && screens_live && XR_SUCCEEDED(r));
        }

        const auto log_now = Clock::now();
        const double since = std::chrono::duration<double>(log_now - last_log).count();
        if (since >= 1.0) {
            LogSummary(since, wait_count ? wait_sum_ms / wait_count : 0.0, wait_max_ms, worst);
            pacing.Second(since); // AzaharFrame pacing
            if (layer_slots.Resets() != slots_logged_resets) {
                layers_logged_most = 0; // flash2: a new list
            }
            if (layer_slots.Added() != slots_logged_added ||
                layer_slots.Dropped() != slots_logged_dropped ||
                layer_slots.Resets() != slots_logged_resets ||
                layers_most > layers_logged_most) { // AzaharFrame flash
                // flash2: and the most layers a frame carried in the last second (the screens
                // with Motion Clarity's backing, the slots, the controllers' projection layer)
                // against the runtime's limit; again when a list carries more than it said.
                LOG_INFO(Frontend,
                         "xr: layer slots: {} after the screens ({} made, {} dropped over the "
                         "runtime's limit, {} lists since the start); at most {} layers of {} "
                         "({} of them screens) in the last second",
                         layer_slots.Size(), layer_slots.Added(), layer_slots.Dropped(),
                         layer_slots.Resets(), layers_most, ui ? ui->MaxLayers() : 0,
                         layers_most_screens);
                slots_logged_added = layer_slots.Added();
                slots_logged_dropped = layer_slots.Dropped();
                slots_logged_resets = layer_slots.Resets();
                layers_logged_most = std::max(layers_logged_most, layers_most);
            }
            layers_most = layers_most_screens = 0;
            if (const std::string line = clarity_log.Second(
                    clarity.Mean(), fs.predictedDisplayPeriod, flash_backing,
                    clarity.Hitches()); // AzaharFrame flash3
                !line.empty()) {
                LOG_INFO(Frontend, "xr: {}", line); // AzaharFrame flash
            }
            poll_test_switches(); // AzaharFrame integ9
            last_log = log_now;
            wait_sum_ms = wait_max_ms = 0.0;
            wait_count = 0;
            worst = {};
        }
    }
    Watchdog::Idle();
    running = false;
    loop_alive = false;
    LOG_INFO(Frontend, "xr: frame loop ended (state {})", StateName(state.load()));
}

// ---- Public API ------------------------------------------------------------

bool Start(const StartOptions& options) {
    if (g_runtime) {
        return true;
    }
    auto rt = std::make_unique<Runtime>();
    if (!rt->CreateInstance(options)) {
        LOG_ERROR(Frontend, "xr: no XR instance; running flat");
        return false;
    }
    if (options.vr && !rt->CreateSession()) {
        LOG_ERROR(Frontend, "xr: no VR session now; flat (the VR / flat chord tries again)");
        rt->want_vr = false;
    } else if (!options.vr) {
        LOG_INFO(Frontend, "xr: starting flat (instance and device made, no session)");
    }
    if (!rt->has_session) {
        Settings::frame_flat_mono = true; // games draw one eye for the window
    }
    g_runtime = std::move(rt);
    VideoCore::SetXrScreenSink(g_runtime.get());
    Immersive::Install(); // AzaharFrame immersive mode; before the XR thread reads its state
    DynamicResolution::Install(); // AzaharFrame menuux
    g_runtime->StartThread();
    LOG_INFO(Frontend, "xr: started");
    return true;
}

void Shutdown() {
    if (!g_runtime) {
        return;
    }
    LOG_INFO(Frontend, "xr: shutting down");
    VideoCore::SetXrScreenSink(nullptr);
    g_runtime->StopThread();
    if (g_runtime->loop_abandoned) {
        // AzaharFrame flat2: the stuck loop may still use all of it: left as it is (the process
        // is ending).
        static_cast<void>(g_runtime.release());
        Settings::frame_flat_mono = false;
        return;
    }
    Immersive::Uninstall(); // AzaharFrame immersive mode
    DynamicResolution::Uninstall(); // AzaharFrame menuux
    UiSound::Shutdown();
    g_runtime->Destroy();
    g_runtime.reset();
    Settings::frame_flat_mono = false;
}

bool IsStarted() {
    return g_runtime != nullptr;
}

bool HasExtension(const char* name) {
    return g_runtime && g_runtime->HasExtension(name);
}

bool SetVr(bool vr) {
    auto* rt = GetRuntime();
    if (!rt || !rt->loop_alive || (vr && rt->lost)) { // AzaharFrame flat: no runtime, no VR
        return false;
    }
    if (rt->want_vr.exchange(vr) != vr) {
        LOG_INFO(Frontend, "xr: {} asked for", vr ? "VR" : "flat");
    } else if (vr && !rt->has_session) {
        // AzaharFrame flatfix: asked again before the loop made the session: say why it waits.
        const double stalled = Watchdog::FlatStalledSeconds();
        if (stalled >= 3.0) {
            LOG_ERROR(Frontend, "xr: VR asked for again, but the frame loop has not ticked for "
                                "{:.1f} s ({})",
                      stalled, rt->loop_phase.load());
        } else {
            LOG_INFO(Frontend, "xr: VR asked for again; the frame loop is making the session ({})",
                     rt->loop_phase.load());
        }
    }
    return true;
}

bool WantsVr() {
    auto* rt = GetRuntime();
    return rt && rt->want_vr;
}

bool VrShowing() {
    auto* rt = GetRuntime();
    return rt && rt->loop_alive && rt->has_session;
}

void SetFlatInput(FlatInputSource source) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetFlatInput(std::move(source));
    }
}

int AddFrameCallback(FrameCallback callback) {
    auto* rt = GetRuntime();
    if (!rt) {
        return 0;
    }
    std::scoped_lock lock{rt->callbacks_mutex};
    const int id = rt->next_id++;
    rt->frame_callbacks.emplace(id, std::move(callback));
    return id;
}

void RemoveFrameCallback(int id) {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->callbacks_mutex};
        rt->frame_callbacks.erase(id);
    }
}

int AddStateCallback(StateCallback callback) {
    auto* rt = GetRuntime();
    if (!rt) {
        return 0;
    }
    std::scoped_lock lock{rt->callbacks_mutex};
    const int id = rt->next_id++;
    rt->state_callbacks.emplace(id, std::move(callback));
    return id;
}

void RemoveStateCallback(int id) {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->callbacks_mutex};
        rt->state_callbacks.erase(id);
    }
}

int AddQuad(const Quad& quad) {
    auto* rt = GetRuntime();
    if (!rt) {
        return 0;
    }
    std::scoped_lock lock{rt->quads_mutex};
    static int next = 1;
    const int id = next++;
    rt->quads.emplace(id, quad);
    return id;
}

bool SetQuad(int id, const Quad& quad) {
    auto* rt = GetRuntime();
    if (!rt) {
        return false;
    }
    std::scoped_lock lock{rt->quads_mutex};
    auto it = rt->quads.find(id);
    if (it == rt->quads.end()) {
        return false;
    }
    it->second = quad;
    return true;
}

void RemoveQuad(int id) {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->quads_mutex};
        rt->quads.erase(id);
    }
}

VideoCore::XrContentPose ShownContentPose() {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->content_mutex};
        return rt->shown_content;
    }
    return {};
}

ImageLag::OnView PictureOnView() { // AzaharFrame imglag
    if (auto* rt = GetRuntime()) {
        return ImageLag::PictureOnView(ShownPictures(), rt->sent_serial.load(),
                                       rt->image_lag.load());
    }
    return {};
}

ImageLag::Counts ImageLagCounts() { // AzaharFrame imglag
    ImageLag::Counts c;
    if (auto* rt = GetRuntime()) {
        for (std::size_t i = 0; i < ImageLag::kNoneSentKinds; i++) {
            c.none_sent[i] = rt->none_sent[i].load();
        }
        c.missed_refreshes = rt->missed_refreshes.load();
        c.serial_trailing = rt->serial_trailing.load();
    }
    return c;
}

uint32_t ImageLagInEffect() { // AzaharFrame imglag
    if (auto* rt = GetRuntime()) {
        return rt->image_lag.load();
    }
    return ImageLag::kDefaultLag;
}

std::array<ShownPicture, kShownPictures> ShownPictures() { // AzaharFrame heldquad
    std::array<ShownPicture, kShownPictures> out{};
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->content_mutex};
        for (std::size_t i = 0; i < kShownPictures; i++) {
            out[i] = rt->shown_pictures[(rt->shown_next + kShownPictures - 1 - i) % kShownPictures];
        }
    }
    return out;
}

ViewPose LatestViewPose() {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->pose_mutex};
        return rt->view_pose;
    }
    return {};
}

void ShowToast(Toast kind) {
    if (auto* rt = GetRuntime()) {
        rt->hud.ShowToast(kind);
    }
}

void ShowAutosaved() { // AzaharFrame autosave
    if (auto* rt = GetRuntime()) {
        rt->hud.ShowAutosaved();
    }
}

void SetShaderBadge(const std::string& text, float progress) { // AzaharFrame shaderbadge
    if (auto* rt = GetRuntime()) {
        rt->hud.SetShaderBadge(text, progress);
    }
}

void ShowToastText(const std::string& text, ToastTime time) {
    if (auto* rt = GetRuntime()) {
        rt->hud.ShowToastText(text, time);
    }
}

void HideToastText(const std::string& text) {
    if (auto* rt = GetRuntime()) {
        rt->hud.HideToastText(text);
    }
}

void ShowHint(const std::string& text, const XrPosef& pose, float width_m) {
    if (auto* rt = GetRuntime()) {
        rt->hud.ShowHint(text, pose, width_m);
    }
}

void SetFastForwardShown(bool on) {
    if (auto* rt = GetRuntime()) {
        rt->hud.ShowToast(on ? Toast::FastForward : Toast::NormalSpeed);
    }
}

bool PassthroughSupported() {
    auto* rt = GetRuntime();
    return rt && rt->alpha_blend_supported;
}

void SetFrontendHooks(FrontendHooks hooks) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetFrontendHooks(std::move(hooks));
    }
}

void SetGameRunning(bool running) {
    // AzaharFrame ui2: each game (Restart too, which never stops the screens) starts from the
    // setting, without what the last one learnt.
    DynamicResolution::Reset();
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetGameRunning(running);
    }
}

void SetLibraryDirs(const std::vector<LibraryDir>& dirs) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetLibraryDirs(dirs);
    }
}

void SetHomePanel(std::function<void(const HomePanelEvent&)> on_event) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetHomePanel(std::move(on_event));
    }
}

void SetHomePanelImage(std::shared_ptr<const HomePanelImage> image) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetHomePanelImage(std::move(image));
    }
}

void SetHomePanelListNav(bool enabled) { // AzaharFrame gamelist
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetHomePanelListNav(enabled);
    }
}

void SetInGameMenu(bool open) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetInGameMenu(open);
    }
}

void SetFlatVrPrompt(bool open) { // AzaharFrame flat
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetFlatVrPrompt(open);
    }
}

void SetInGameMenuWindow(bool window) { // AzaharFrame tabmenu
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetInGameMenuWindow(window);
    }
}

void SetInGameMenuInfo(const std::string& title, const std::vector<MenuSaveSlot>& slots,
                       bool per_game) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetInGameMenuInfo(title, slots, per_game);
    }
}

XrInstance Instance() {
    return g_runtime ? g_runtime->instance : XR_NULL_HANDLE;
}

XrSystemId System() {
    return g_runtime ? g_runtime->system : XR_NULL_SYSTEM_ID;
}

XrSession Session() {
    return g_runtime ? g_runtime->session : XR_NULL_HANDLE;
}

XrSwapchain ScreenSwapchain(VideoCore::XrScreen screen) {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->screens_mutex};
        return rt->screens[static_cast<std::size_t>(screen)].handle;
    }
    return XR_NULL_HANDLE;
}

} // namespace FrameXr
