// Copyright 2026 CitraFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// Internal: the state behind frame_xr.h. session.cpp makes and destroys the
// instance, session, spaces and the screen swapchains (VideoCore::XrScreenSink);
// app.cpp runs the frame loop thread and the public extension points.

#pragma once

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

#include "frame_xr/frame_xr.h"
#include "frame_xr/vulkan_bridge.h"

namespace FrameXr {

constexpr std::size_t kScreens = static_cast<std::size_t>(VideoCore::XrScreen::Count);

/// Guards the renderer's VkQueue for the OpenXR calls that may use it.
class QueueLock {
public:
    void Set(std::mutex* mutex) {
        std::scoped_lock lock{registry};
        queue = mutex;
    }
    template <typename F>
    auto With(F&& f) {
        std::scoped_lock lock{registry};
        if (queue) {
            std::scoped_lock queue_lock{*queue};
            return f();
        }
        return f();
    }

private:
    std::mutex registry;
    std::mutex* queue = nullptr;
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
};

struct Stats {
    std::atomic<uint32_t> display_frames{0};
    std::atomic<uint32_t> releases[kScreens]{};
    std::atomic<uint32_t> late[kScreens]{};
    std::atomic<uint32_t> unfinished[kScreens]{};
    std::atomic<uint32_t> skipped_acquires{0};
    std::atomic<uint32_t> end_frame_errors{0};
};

class VrUi;

class Runtime final : public VideoCore::XrScreenSink {
public:
    Runtime();
    ~Runtime() override;

    // session.cpp
    bool Create(const StartOptions& options);
    void Destroy();
    bool HasExtension(const char* name) const;

    // XrScreenSink (emu thread / renderer release thread)
    bool IsActive() const override;
    VkFormat SwapchainFormat() const override;
    bool EnsureSwapchain(VideoCore::XrScreen screen, uint32_t width, uint32_t height) override;
    uint32_t SwapchainImages(VideoCore::XrScreen screen, VkImage* images,
                             uint32_t capacity) override;
    int32_t Acquire(VideoCore::XrScreen screen) override;
    void Release(VideoCore::XrScreen screen, bool gpu_finished) override;
    void SetQueueMutex(std::mutex* mutex) override;
    void SetScreensLive(bool live) override;
    void NoteLateRelease(VideoCore::XrScreen screen) override;

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
    std::vector<std::string> enabled_extensions;
    VkFormat swapchain_format = VK_FORMAT_UNDEFINED;

    // Swapchains: screens_mutex guards the handles against recreation while the
    // XR thread submits them (held across xrEndFrame).
    mutable std::mutex screens_mutex;
    std::array<ScreenChain, kScreens> screens;
    std::atomic<bool> screens_live{false};

    QueueLock queue_lock;
    Stats stats;

    // Session state (XR thread writes, others read).
    std::atomic<XrSessionState> state{XR_SESSION_STATE_UNKNOWN};
    std::atomic<bool> running{false}; ///< between xrBeginSession and xrEndSession
    std::atomic<bool> lost{false};    ///< exiting / loss pending / instance lost

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
    void FrameLoop();
    bool PollEvents();
    void OnState(XrSessionState new_state);
    void LocateViews(XrTime time);
    void LogSummary(double seconds, double wait_avg_ms, double wait_max_ms);

    std::thread thread;
    std::atomic<bool> stop{false};
};

Runtime* GetRuntime();

} // namespace FrameXr
