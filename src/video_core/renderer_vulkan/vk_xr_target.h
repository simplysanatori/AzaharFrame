// Copyright 2026 CitraFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// CitraFrame: the renderer's side of the XR output (VideoCore::XrScreenSink).
// RendererVulkan::RenderToXr (renderer_vulkan_xr.cpp) draws the three screens into
// the XR swapchains after the window; this class owns what that needs on the
// device (render pass, pipeline, framebuffers of the swapchain images) and the
// release thread: an image is released to the runtime only once the GPU reached
// the tick of the submission that drew it (SteamVR shows a released image at
// once, finished or not).

#pragma once

#include <array>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/vulkan_hooks.h"

namespace Vulkan {

class Instance;
class Scheduler;

class XrPresentTarget {
public:
    static constexpr std::size_t kScreens = static_cast<std::size_t>(VideoCore::XrScreen::Count);

    struct Target {
        vk::Framebuffer framebuffer;
        u32 width;
        u32 height;
    };

    XrPresentTarget(const Instance& instance, Scheduler& scheduler, VideoCore::XrScreenSink& sink,
                    vk::ShaderModule vertex_shader, vk::PipelineLayout pipeline_layout);
    ~XrPresentTarget();

    XrPresentTarget(const XrPresentTarget&) = delete;
    XrPresentTarget& operator=(const XrPresentTarget&) = delete;

    /// Makes sure each screen's swapchain has this size (recreated after the
    /// release thread drained). False: no XR output this frame.
    bool Prepare(const std::array<vk::Extent2D, kScreens>& sizes);

    /// Acquires the next image of a screen: waits for the release of the last one
    /// first (a late release when it had to). Null: skip the screen this frame.
    const Target* Acquire(VideoCore::XrScreen screen);

    /// After Scheduler::Flush: the screens acquired since the last call are released
    /// once the GPU reached `tick` (the tick of that submission).
    void QueueRelease(u64 tick);

    vk::RenderPass RenderPass() const {
        return render_pass;
    }
    vk::Pipeline Pipeline() const {
        return pipeline;
    }

private:
    struct Screen {
        vk::Extent2D size{};
        std::vector<vk::ImageView> views;
        std::vector<vk::Framebuffer> framebuffers;
        Target current{};
        bool acquired = false; ///< acquired this frame, release not queued yet
        bool pending = false;  ///< release queued, not done yet
    };
    struct PendingRelease {
        u64 tick;
        std::array<bool, kScreens> screens;
    };

    void CreateRenderPass();
    void CreatePipeline(vk::ShaderModule vertex_shader, vk::PipelineLayout pipeline_layout);
    void DestroyTargets(Screen& screen);
    void WaitIdle();
    void ReleaseThread(std::stop_token token);

    const Instance& instance;
    Scheduler& scheduler;
    VideoCore::XrScreenSink& sink;
    vk::Device device;
    vk::Format format;
    vk::RenderPass render_pass;
    vk::ShaderModule fragment_shader;
    vk::Pipeline pipeline;
    std::array<Screen, kScreens> screens;

    std::mutex mutex;
    std::condition_variable_any cv;
    std::deque<PendingRelease> queue;
    bool skip_gpu_wait = false; ///< CITRAFRAME_XR_NO_GPU_WAIT=1: negative test only
    std::jthread release_thread;
};

/// Lends the scheduler's submit mutex to the XR thread for the renderer's whole
/// life: a member of RendererVulkan declared right after its Scheduler, so it is
/// in place before anything submits and gone only after everything has.
class XrQueueRegistration {
public:
    explicit XrQueueRegistration(Scheduler& scheduler);
    ~XrQueueRegistration();

    XrQueueRegistration(const XrQueueRegistration&) = delete;
    XrQueueRegistration& operator=(const XrQueueRegistration&) = delete;

private:
    VideoCore::XrScreenSink* sink;
};

} // namespace Vulkan
