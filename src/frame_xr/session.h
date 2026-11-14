// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Internal: the state behind frame_xr.h. session.cpp makes and destroys the
// instance, session, spaces and the screen swapchains (VideoCore::XrScreenSink);
// app.cpp runs the frame loop thread and the public extension points.
//
// VR <-> flat: the Vulkan device lives for the whole run; the session (with its spaces,
// swapchains and the UI's actions and overlay chains) is made by CreateSession and ended by
// DestroySession, both on the XR thread once it runs. AzaharFrame flat2: the XrInstance goes with
// its session and VR again makes a new one (RenewInstance; instance_policy.h says why). `chains_mutex` keeps the renderer's
// swapchain calls out while DestroySession destroys the swapchains, and `generation` tells
// the renderer to drop what it made of their images.

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include "frame_xr/frame_xr.h"
#include "frame_xr/hud.h" // AzaharFrame vrqol
#include "frame_xr/instance_policy.h" // AzaharFrame flat2
#include "frame_xr/pacing.h" // AzaharFrame pacing
#include "frame_xr/vulkan_bridge.h"

namespace FrameXr {

constexpr std::size_t kScreens = static_cast<std::size_t>(VideoCore::XrScreen::Count);

/// Guards the renderer's VkQueue for the OpenXR calls that may use it.
/// How long each call waited for the lock is kept for the caller's thread (LastWaitUs) and,
/// on the XR thread (MarkXrThread), as a per-second maximum for its summary line.
class QueueLock {
public:
    void Set(std::mutex* mutex) {
        std::scoped_lock lock{registry};
        queue = mutex;
    }
    template <typename F>
    auto With(F&& f) {
        const auto asked = std::chrono::steady_clock::now();
        std::scoped_lock lock{registry};
        if (queue) {
            std::scoped_lock queue_lock{*queue};
            NoteWait(asked);
            return f();
        }
        NoteWait(asked);
        return f();
    }
    /// The wait of this thread's last With call, in microseconds.
    static uint64_t LastWaitUs() {
        return last_wait_us;
    }
    /// The frame loop's thread: its waits count for TakeMaxWaitUs.
    static void MarkXrThread() {
        xr_thread = true;
    }
    uint64_t TakeMaxWaitUs() {
        return max_wait_us.exchange(0, std::memory_order_relaxed);
    }

private:
    void NoteWait(std::chrono::steady_clock::time_point asked) {
        last_wait_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - asked)
                .count());
        if (!xr_thread) {
            return;
        }
        uint64_t prev = max_wait_us.load(std::memory_order_relaxed);
        while (last_wait_us > prev &&
               !max_wait_us.compare_exchange_weak(prev, last_wait_us, std::memory_order_relaxed)) {
        }
    }

    std::mutex registry;
    std::mutex* queue = nullptr;
    std::atomic<uint64_t> max_wait_us{0};
    static inline thread_local uint64_t last_wait_us = 0;
    static inline thread_local bool xr_thread = false;
};

struct ScreenChain {
    XrSwapchain handle = XR_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<VkImage> images;
    uint32_t index = 0;      ///< the image last acquired
    bool acquired = false;   ///< an image is acquired and not yet waited for
    bool waited = false;     ///< an image is waited for and not yet released
    bool has_image = false;  ///< released at least once since made (may be shown)
    /// AzaharFrame doubling: the game frame (Runtime::release_serial) of the image released last.
    uint64_t frame_serial = 0;
};

/// Where one display frame's time went (milliseconds), from one xrWaitFrame return to the next.
struct LoopPhases {
    double period_ms = 0;
    double begin_lock_ms = 0, begin_ms = 0; ///< xrBeginFrame: waiting for the queue lock, the call
    double frame_ms = 0;                    ///< views, callbacks, controls, panels
    double screens_lock_ms = 0, layers_ms = 0;
    double end_lock_ms = 0, end_ms = 0;     ///< xrEndFrame: waiting for the queue lock, the call
    double poll_ms = 0, wait_ms = 0;        ///< xrPollEvent and the session, xrWaitFrame
};

struct Stats {
    std::atomic<uint32_t> display_frames{0};
    std::atomic<uint32_t> releases[kScreens]{};
    std::atomic<uint32_t> late[kScreens]{}; ///< frames drawn aside (spilled), not waited for
    std::atomic<uint32_t> spill_shown{0};    ///< spilled frames copied in and released
    std::atomic<uint32_t> spill_replaced{0}; ///< spilled frames dropped for a newer one
    std::atomic<uint32_t> emu_waits{0};      ///< the emu thread waited for the XR side
    std::atomic<uint32_t> emu_wait_us_max{0};
    std::atomic<uint32_t> unfinished[kScreens]{};
    std::atomic<uint32_t> skipped_acquires{0};
    std::atomic<uint32_t> end_frame_errors{0};
    /// AzaharFrame doubling: frames ended with the two eyes' top images from different game
    /// frames (separate swapchains only; never with set releases or a packed top screen).
    std::atomic<uint32_t> eyes_mixed{0};
    // Emulated frames: time between the renderer's XR frames (emu thread).
    std::atomic<uint32_t> emu_frames{0};
    std::atomic<uint64_t> emu_frame_us_sum{0};
    std::atomic<uint32_t> emu_frame_us_max{0};
    // The desktop window drawn and presented (once a second in VR; every game frame in flat).
    std::atomic<uint32_t> window_presents{0};
    std::atomic<uint64_t> window_us_sum{0};
    std::atomic<uint32_t> window_us_max{0};
};

class VrUi;

class Runtime final : public VideoCore::XrScreenSink {
public:
    Runtime();
    ~Runtime() override;

    // session.cpp
    /// The instance, the system and the Vulkan device (once per run).
    bool CreateInstance(const StartOptions& options);
    /// A session with its spaces and the UI's XR parts. False: none (all undone).
    bool CreateSession();
    /// Ends what CreateSession made; the instance and device stay. XR thread (or no thread).
    void DestroySession();
    /// AzaharFrame flat2: before a session, a new XrInstance when the policy asks for one (none,
    /// or the current one had its session), on the Vulkan device made at start. False: none.
    bool RenewInstance();
    /// AzaharFrame flat2: the XrInstance (no session may be left). The device stays.
    void DestroyXrInstance();
    /// Everything.
    void Destroy();
    bool HasExtension(const char* name) const;
    /// AzaharFrame integ9: AZAHARFRAME_XR_REFRESH_RATE (a test switch, live): asks the runtime
    /// for that refresh rate (XR_FB_display_refresh_rate), or, unset, for the one the session
    /// started with. XR thread, with a session.
    void RequestRefreshRate();

    // XrScreenSink (emu thread / renderer release thread)
    bool IsActive() const override;
    VkFormat SwapchainFormat() const override;
    bool EnsureSwapchain(VideoCore::XrScreen screen, uint32_t width, uint32_t height) override;
    uint32_t SwapchainImages(VideoCore::XrScreen screen, VkImage* images,
                             uint32_t capacity) override;
    int32_t Acquire(VideoCore::XrScreen screen) override;
    void Release(VideoCore::XrScreen screen, bool gpu_finished) override;
    void ReleaseFrame(bool gpu_finished, const VideoCore::XrContentPose& content) override;
    void WaitPresentSlot(uint64_t game_swaps) override; // AzaharFrame pacing
    bool PackedTop() const override;
    void SetQueueMutex(std::mutex* mutex) override;
    void SetScreensLive(bool live) override;
    void NoteLateRelease(VideoCore::XrScreen screen) override;
    void NoteEmuFrame() override;
    void NoteSpillShown() override;
    void NoteSpillReplaced() override;
    void NoteEmuWait(uint32_t us) override;
    uint64_t SwapchainGeneration() const override;
    void NoteWindowPresent(uint32_t cpu_us) override;

    // app.cpp
    void StartThread();
    void StopThread();

    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace local_space = XR_NULL_HANDLE;
    XrSpace view_space = XR_NULL_HANDLE;
    std::unique_ptr<VulkanBridge> vulkan;
    std::unique_ptr<VrUi> ui; ///< controllers, picker, pointer (controls.cpp)
    Hud hud;                  ///< passthrough background and toasts (hud.cpp)
    bool alpha_blend_supported = false; ///< the runtime offers XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND
    std::vector<std::string> enabled_extensions; ///< chosen once, the same for every instance
    StartOptions start_options;                  ///< AzaharFrame flat2: for a new instance
    InstanceSessions instance_sessions;          ///< AzaharFrame flat2 (XR thread once it runs)
    /// AzaharFrame flat2: where the frame loop is, for a stop that it does not answer.
    std::atomic<const char*> loop_phase{"not started"};
    VkFormat swapchain_format = VK_FORMAT_UNDEFINED;

    // Swapchains: screens_mutex guards the handles against recreation while the
    // XR thread submits them (held across xrEndFrame).
    mutable std::mutex screens_mutex;
    std::array<ScreenChain, kScreens> screens;
    std::atomic<bool> screens_live{false};

    // AzaharFrame doubling (all three read once, at construction; see DOUBLING in IMMERSIVE.md).
    /// AZAHARFRAME_XR_SET_RELEASE (default 1): ReleaseFrame releases every screen under
    /// screens_mutex, so an xrEndFrame sees all of one game frame or all of the one before.
    bool set_release = true;
    /// AZAHARFRAME_XR_PACKED (default 1): both top screen images in the TopLeft swapchain.
    bool packed_top = true;
    /// Game frames released (ReleaseFrame), the serial of the newest (the handoff thread's).
    uint64_t release_serial = 0;
    /// AzaharFrame flash: the game picture of the next release (ShownPicture::picture), from
    /// WaitPresentSlot's swap count; 0 when it was not called (the handoff thread's).
    uint64_t release_picture = 0;
    /// The head pose of the picture released last (ShownContentPose); guarded by content_mutex,
    /// written under screens_mutex too with set releases.
    mutable std::mutex content_mutex;
    VideoCore::XrContentPose shown_content{};
    /// AzaharFrame heldquad: the last pictures released, with their release serial (the
    /// chains' frame_serial), newest at shown_next - 1; guarded by content_mutex
    /// (ShownPictures, PictureOnView).
    std::array<ShownPicture, kShownPictures> shown_pictures{};
    std::size_t shown_next = 0;
    /// AzaharFrame imglag: the release serial the last xrEndFrame carried in its top screen
    /// layer, 0: none (written by the XR thread after each xrEndFrame; image_lag.h).
    std::atomic<uint64_t> sent_serial{0};
    /// AzaharFrame imglag: the fallbacks and silent mispairs, counted for the per-second line
    /// (ImageLagCounts): frames that left nothing sent, by reason (XR thread writes); refreshes the
    /// loop missed; xrEndFrames that may carry an image ahead of its serial (set releases off).
    std::array<std::atomic<uint32_t>, ImageLag::kNoneSentKinds> none_sent{};
    std::atomic<uint32_t> missed_refreshes{0};
    std::atomic<uint32_t> serial_trailing{0};
    /// AzaharFrame imglag: ReleaseFrame calls in progress outside screens_mutex (set releases off).
    std::atomic<uint32_t> releasing{0};
    XrTime last_display_time = 0; ///< AzaharFrame imglag: the last xrWaitFrame's (XR thread)
    /// AzaharFrame imglag: no xrEndFrame for this frame: nothing of it in the pipeline (XR thread).
    void NoEndFrame() {
        sent_serial = 0;
        none_sent[static_cast<std::size_t>(ImageLag::NoneSent::NoEndFrame)]++;
    }
    /// AzaharFrame imglag: the runtime's image lag in effect, 0 or 1 refresh (ImageLag::InEffect
    /// of the setting vr_xr_image_lag and the live switch AZAHARFRAME_XR_IMAGE_LAG).
    std::atomic<uint32_t> image_lag{ImageLag::kDefaultLag};
    std::string runtime_name; ///< AzaharFrame imglag: "SteamVR/OpenXR 2.19.3", for the log
    /// AzaharFrame imglag: the image lag from the setting and the switch, logged when it changes
    /// (with the runtime's name and version, as the lag is the runtime's).
    void UpdateImageLag(const char* why);
    /// AzaharFrame pacing: the parity gate and the release phase (pacing.h).
    Pacing pacing;

    QueueLock queue_lock;
    std::chrono::steady_clock::time_point last_emu_frame{}; // emu thread only
    Stats stats;
    /// AzaharFrame menuux: the runtime's display period at the last xrWaitFrame (XR thread).
    XrDuration display_period_ns = 0;
    float start_refresh_hz = 0.0f; ///< AzaharFrame integ9: the session's rate at its start

    // Session state (XR thread writes, others read).
    std::atomic<XrSessionState> state{XR_SESSION_STATE_UNKNOWN};
    std::atomic<bool> running{false}; ///< between xrBeginSession and xrEndSession
    std::atomic<bool> lost{false};    ///< the instance is lost (the frame loop goes on flat)
    std::atomic<bool> session_ended{false}; ///< EXITING / LOSS_PENDING: DestroySession next
    std::atomic<bool> detaching{false};     ///< the session is being ended: no new XR images

    // VR <-> flat.
    std::atomic<bool> want_vr{true};
    std::atomic<bool> has_session{false}; ///< `session` is valid (read from other threads)
    std::atomic<bool> loop_alive{false};
    bool loop_abandoned = false; ///< AzaharFrame flat2: StopThread gave up on a stuck loop
    std::atomic<uint64_t> generation{1};
    std::shared_mutex chains_mutex; ///< shared: renderer swapchain calls; unique: DestroySession

    // Extension points (app.cpp).
    std::mutex callbacks_mutex;
    int next_id = 1;
    std::map<int, FrameCallback> frame_callbacks;
    std::map<int, StateCallback> state_callbacks;
    std::mutex quads_mutex;
    std::map<int, Quad> quads;
    std::mutex pose_mutex;
    ViewPose view_pose;
    std::mutex placement_mutex;
    ScreenPlacement placement;

private:
    /// The XrInstance and its system; `renew`: the extensions chosen the first time.
    bool CreateXrInstance(bool renew);
    void FrameLoop();
    void FlatStep(double& log_seconds);
    void LogFlatSummary(double seconds);
    bool PollEvents();
    void OnState(XrSessionState new_state);
    void LocateViews(XrTime time);
    void LogSummary(double seconds, double wait_avg_ms, double wait_max_ms,
                    const LoopPhases& worst);
    void NoteLoopPeriod(const LoopPhases& phases, LoopPhases& worst, XrDuration period_ns);

    std::thread thread;
    std::atomic<bool> stop{false};
};

Runtime* GetRuntime();

} // namespace FrameXr
