// Copyright 2026 CitraFrame contributors
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
#include <vector>

#include "common/logging/log.h"
#include "common/thread.h"
#include "frame_xr/controls.h"
#include "frame_xr/session.h"
#include "frame_xr/xr_loader.h"

namespace FrameXr {

namespace {

std::unique_ptr<Runtime> g_runtime;

using Clock = std::chrono::steady_clock;

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

} // namespace

Runtime* GetRuntime() {
    return g_runtime.get();
}

// ---- Placement --------------------------------------------------------------

ScreenPlacement DefaultScreenPlacement() {
    // As CitraVR (GameSurfaceLayer.cpp): 240 px of the native screen per metre,
    // the top screen 2 m ahead at eye height; the bottom one 0.75 m lower, 0.5 m
    // nearer, 0.75^2 the scale, tilted 45 degrees back.
    constexpr float kDensity = 240.0f;
    constexpr float kBottomScale = 0.75f * 0.75f;
    constexpr float kPitch = -0.785398163f; // -45 degrees about +X
    ScreenPlacement p{};
    p.top_pose = {{0, 0, 0, 1}, {0.0f, 0.0f, -2.0f}};
    p.top_size = {400.0f / kDensity, 240.0f / kDensity};
    p.bottom_pose = {{std::sin(kPitch / 2), 0, 0, std::cos(kPitch / 2)}, {0.0f, -0.75f, -1.5f}};
    p.bottom_size = {320.0f / kDensity * kBottomScale, 240.0f / kDensity * kBottomScale};
    return p;
}

ScreenPlacement GetScreenPlacement() {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->placement_mutex};
        return rt->placement;
    }
    return DefaultScreenPlacement();
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
    thread = std::thread([this] { FrameLoop(); });
}

void Runtime::StopThread() {
    stop = true;
    if (thread.joinable()) {
        thread.join();
    }
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
        running = false;
        lost = true;
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
        default:
            // Interaction profile changes and the like: for the input lane's
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

void Runtime::LogSummary(double seconds, double wait_avg_ms, double wait_max_ms) {
    const auto take = [](std::atomic<uint32_t>& a) { return a.exchange(0); };
    const uint32_t frames = take(stats.display_frames);
    uint32_t rel[kScreens], late[kScreens], unfinished[kScreens];
    for (std::size_t i = 0; i < kScreens; i++) {
        rel[i] = take(stats.releases[i]);
        late[i] = take(stats.late[i]);
        unfinished[i] = take(stats.unfinished[i]);
    }
    const uint32_t skipped = take(stats.skipped_acquires);
    const uint32_t end_errors = take(stats.end_frame_errors);
    LOG_INFO(Frontend,
             "xr: {:.1f}s display {} frames, waitFrame period {:.2f} ms (max {:.2f}), releases "
             "TL/TR/B {}/{}/{}, late {}/{}/{}, before GPU done {}/{}/{}, acquire timeouts {}, "
             "endFrame errors {}, state {}, screens {}",
             seconds, frames, wait_avg_ms, wait_max_ms, rel[0], rel[1], rel[2], late[0], late[1],
             late[2], unfinished[0], unfinished[1], unfinished[2], skipped, end_errors,
             StateName(state.load()), screens_live.load() ? "live" : "off");
}

void Runtime::FrameLoop() {
    Common::SetCurrentThreadName("FrameXR");
    auto& xr = Xr();
    auto last_log = Clock::now();
    auto last_wait = Clock::time_point{};
    double wait_sum_ms = 0.0, wait_max_ms = 0.0;
    uint32_t wait_count = 0;
    bool exit_requested = false;
    auto exit_deadline = Clock::time_point::max();
    std::vector<XrCompositionLayerQuad> quad_layers;
    std::vector<Quad> ui_quads;
    std::vector<const XrCompositionLayerBaseHeader*> layer_ptrs;

    while (true) {
        if (!PollEvents() || lost) {
            break;
        }
        if (stop && !exit_requested) {
            // Shutdown: let the runtime take the session through STOPPING
            // (xrEndSession) and EXITING, so it ends cleanly.
            exit_requested = true;
            exit_deadline = Clock::now() + std::chrono::seconds(3);
            if (running) {
                const XrResult r = xr.xrRequestExitSession(session);
                if (XR_FAILED(r)) {
                    LOG_ERROR(Frontend, "xr: xrRequestExitSession: {}", ResultName(r));
                    break;
                }
            } else {
                break;
            }
        }
        if (exit_requested && (!running || Clock::now() > exit_deadline)) {
            if (running) {
                LOG_WARNING(Frontend, "xr: the runtime did not end the session in time");
            }
            break;
        }
        if (!running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        XrFrameState fs{XR_TYPE_FRAME_STATE};
        XrResult r = xr.xrWaitFrame(session, nullptr, &fs);
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrWaitFrame: {}", ResultName(r));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        const auto now = Clock::now();
        if (last_wait != Clock::time_point{}) {
            const double ms = std::chrono::duration<double, std::milli>(now - last_wait).count();
            wait_sum_ms += ms;
            wait_max_ms = std::max(wait_max_ms, ms);
            wait_count++;
        }
        last_wait = now;

        r = queue_lock.With([&] { return xr.xrBeginFrame(session, nullptr); });
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrBeginFrame: {}", ResultName(r));
            continue;
        }

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
        std::scoped_lock screens_lock{screens_mutex};
        quad_layers.clear();
        if (fs.shouldRender && screens_live) {
            ScreenPlacement place;
            {
                std::scoped_lock lock{placement_mutex};
                place = placement;
            }
            const auto add_screen = [&](VideoCore::XrScreen s, const XrPosef& pose,
                                        XrExtent2Df size, XrEyeVisibility eye) {
                const ScreenChain& chain = screens[static_cast<std::size_t>(s)];
                if (!chain.handle || !chain.has_image) {
                    return;
                }
                Quad q;
                q.swapchain = chain.handle;
                q.image_rect = {{0, 0},
                                {static_cast<int32_t>(chain.width),
                                 static_cast<int32_t>(chain.height)}};
                q.pose = pose;
                q.size = size;
                q.eye = eye;
                quad_layers.push_back(MakeQuad(local_space, q));
            };
            // Each eye sees its own top image on the same quad.
            add_screen(VideoCore::XrScreen::TopLeft, place.top_pose, place.top_size,
                       XR_EYE_VISIBILITY_LEFT);
            add_screen(VideoCore::XrScreen::TopRight, place.top_pose, place.top_size,
                       XR_EYE_VISIBILITY_RIGHT);
            add_screen(VideoCore::XrScreen::Bottom, place.bottom_pose, place.bottom_size,
                       XR_EYE_VISIBILITY_BOTH);
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
            for (const Quad* q : extra) {
                quad_layers.push_back(MakeQuad(local_space, *q));
            }
            // The picker and the pointer dot last (the dot on top of everything).
            for (const Quad& q : ui_quads) {
                quad_layers.push_back(MakeQuad(local_space, q));
            }
        }
        layer_ptrs.clear();
        for (const auto& l : quad_layers) {
            layer_ptrs.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&l));
        }
        XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
        ei.displayTime = fs.predictedDisplayTime;
        ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        ei.layerCount = static_cast<uint32_t>(layer_ptrs.size());
        ei.layers = layer_ptrs.empty() ? nullptr : layer_ptrs.data();
        r = queue_lock.With([&] { return xr.xrEndFrame(session, &ei); });
        if (XR_FAILED(r)) {
            stats.end_frame_errors++;
            static int logged = 0;
            if (logged++ < 10) {
                LOG_ERROR(Frontend, "xr: xrEndFrame: {}", ResultName(r));
            }
        }
        stats.display_frames++;

        const auto log_now = Clock::now();
        const double since = std::chrono::duration<double>(log_now - last_log).count();
        if (since >= 1.0) {
            LogSummary(since, wait_count ? wait_sum_ms / wait_count : 0.0, wait_max_ms);
            last_log = log_now;
            wait_sum_ms = wait_max_ms = 0.0;
            wait_count = 0;
        }
    }
    running = false;
    LOG_INFO(Frontend, "xr: frame loop ended (state {})", StateName(state.load()));
}

// ---- Public API ------------------------------------------------------------

bool Start(const StartOptions& options) {
    if (g_runtime) {
        return true;
    }
    auto rt = std::make_unique<Runtime>();
    if (!rt->Create(options)) {
        LOG_ERROR(Frontend, "xr: no VR session; running flat");
        return false;
    }
    g_runtime = std::move(rt);
    VideoCore::SetXrScreenSink(g_runtime.get());
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
    g_runtime->Destroy();
    g_runtime.reset();
}

bool IsStarted() {
    return g_runtime != nullptr;
}

bool HasExtension(const char* name) {
    return g_runtime && g_runtime->HasExtension(name);
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

ViewPose LatestViewPose() {
    if (auto* rt = GetRuntime()) {
        std::scoped_lock lock{rt->pose_mutex};
        return rt->view_pose;
    }
    return {};
}

void SetFrontendHooks(FrontendHooks hooks) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetFrontendHooks(std::move(hooks));
    }
}

void SetGameRunning(bool running) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetGameRunning(running);
    }
}

void SetLibraryDirs(const std::vector<LibraryDir>& dirs) {
    if (auto* rt = GetRuntime(); rt && rt->ui) {
        rt->ui->SetLibraryDirs(dirs);
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
