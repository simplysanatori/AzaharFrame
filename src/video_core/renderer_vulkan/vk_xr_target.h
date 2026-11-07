// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: the renderer's side of the XR output (VideoCore::XrScreenSink).
// RendererVulkan::RenderToXr (renderer_vulkan_xr.cpp) draws the three screens into
// the XR swapchains after the window; this class owns what that needs on the
// device (render pass, pipeline, framebuffers of the swapchain images, two spill
// slots per screen) and the handoff thread, which makes every swapchain call: it
// waits for the next images ahead of time, releases an image only once the GPU
// reached the tick of the submission that drew it (SteamVR shows a released image
// at once, finished or not), and copies a spilled frame in. The emu thread never
// waits for it (vk_xr_handoff.h).

#pragma once

#include <array>
#include <condition_variable>
#include <mutex>
#include <vector>

#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/renderer_vulkan/vk_xr_handoff.h"
#include "video_core/vulkan_hooks.h"

VK_DEFINE_HANDLE(VmaAllocation)

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
    /// handoff thread drained: the one wait on it left, counted as "emu waited on XR").
    /// False: no XR output this frame.
    bool Prepare(const std::array<vk::Extent2D, kScreens>& sizes);

    /// Where this frame's three screens are drawn: the swapchain images the handoff
    /// thread waited for, or a spill slot while they are still in use. Never waits.
    /// False: no XR output this frame (draw nothing, no EndFrame).
    bool BeginFrame(std::array<Target, kScreens>& targets);

    /// After Scheduler::Flush: the frame drawn since BeginFrame was submitted with
    /// `tick` (the tick of that submission).
    void EndFrame(u64 tick);

    vk::RenderPass RenderPass() const {
        return render_pass;
    }
    vk::Pipeline Pipeline() const {
        return pipeline;
    }

private:
    struct SpillSlot {
        vk::Image image;
        VmaAllocation allocation{};
        vk::ImageView view;
        vk::Framebuffer framebuffer;
    };
    struct Screen {
        vk::Extent2D size{};
        std::vector<vk::Image> images; ///< the swapchain's
        std::vector<vk::ImageView> views;
        std::vector<vk::Framebuffer> framebuffers;
        std::array<SpillSlot, XrHandoff::kSlots> spill{};
        bool spill_ok = false;
        int32_t armed = -1; ///< the image index the handoff thread waited for, or -1
    };

    void CreateRenderPass();
    void CreatePipeline(vk::ShaderModule vertex_shader, vk::PipelineLayout pipeline_layout);
    void CreateCopyObjects();
    bool CreateSpill(Screen& screen);
    void DestroySpill(Screen& screen);
    void DestroyTargets(Screen& screen);
    /// Emu thread: pauses the handoff thread and waits until nothing is in flight.
    void Drain();
    void ReleaseThread(std::stop_token token);
    void RunArm();
    void RunReleaseDirect(u64 tick);
    void RunCopySpill(std::size_t slot, u64 tick);
    bool SpillReady() const;

    const Instance& instance;
    Scheduler& scheduler;
    VideoCore::XrScreenSink& sink;
    vk::Device device;
    vk::Format format;
    vk::RenderPass render_pass;
    vk::ShaderModule fragment_shader;
    vk::Pipeline pipeline;
    std::array<Screen, kScreens> screens;
    // The handoff thread's copy of a spilled frame into the swapchain images.
    vk::CommandPool copy_pool;
    vk::CommandBuffer copy_cmdbuf;
    vk::Fence copy_fence;

    std::mutex mutex; ///< guards `handoff` and each Screen::armed
    std::condition_variable_any cv;
    XrHandoff handoff;
    XrHandoff::Frame frame{}; ///< emu thread: the frame between BeginFrame and EndFrame
    bool skip_gpu_wait = false; ///< AZAHARFRAME_XR_NO_GPU_WAIT=1: negative test only
    std::jthread release_thread;
};

/// Lends the scheduler's submit mutex to the XR thread for the renderer's whole
/// life: a member of RendererVulkan declared right after its Scheduler, so it is
/// in place before anything submits and gone only after everything has.
/// True when the instance's device is the XR runtime's (adopted through VulkanHooks): only
/// then may the renderer use the XR swapchain images and lend its queue lock.
bool UsesXrDevice(const Instance& instance);

class XrQueueRegistration {
public:
    XrQueueRegistration(const Instance& instance, Scheduler& scheduler);
    ~XrQueueRegistration();

    XrQueueRegistration(const XrQueueRegistration&) = delete;
    XrQueueRegistration& operator=(const XrQueueRegistration&) = delete;

private:
    VideoCore::XrScreenSink* sink;
};

} // namespace Vulkan
