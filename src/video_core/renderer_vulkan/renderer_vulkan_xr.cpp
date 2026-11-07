// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: RendererVulkan::RenderToXr, called from SwapBuffers after the
// window was drawn. Draws each of the three screens full size into its own XR
// swapchain image (top-left eye, top-right eye, bottom), submits once, and hands
// the images to the release thread (vk_xr_target.cpp), which releases them to
// the runtime once the GPU is done.

#include <algorithm>
#include <utility>

#include "common/debug_stall.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#include "video_core/vulkan_hooks.h"

namespace Vulkan {

namespace {

// Native sizes of the 3DS screens (landscape).
constexpr u32 kTopWidth = 400;
constexpr u32 kBottomWidth = 320;
constexpr u32 kScreenHeight = 240;
// SteamVR on the Frame downsamples layers wider than 3458 px: 8x (3200) at most.
constexpr u32 kMaxScale = 8;

std::array<f32, 4 * 4> Orthographic(u32 width, u32 height) {
    // clang-format off
    return {2.f / width, 0.f,          0.f, -1.f,
            0.f,         2.f / height, 0.f, -1.f,
            0.f,         0.f,          1.f,  0.f,
            0.f,         0.f,          0.f,  1.f};
    // clang-format on
}

} // Anonymous namespace

bool RendererVulkan::RenderToXr() {
    VideoCore::XrScreenSink* sink = VideoCore::GetXrScreenSink();
    if (!sink || !sink->IsActive()) {
        return false;
    }
    if (!UsesXrDevice(instance)) {
        // The swapchain images belong to the runtime's device; never draw into them
        // from another one.
        static bool logged = false;
        if (!std::exchange(logged, true)) {
            LOG_ERROR(Render_Vulkan, "xr: the renderer is not on the XR device, no XR screens");
        }
        return false;
    }
    if (xr_target && xr_target_generation != sink->SwapchainGeneration()) {
        // The session ended and a new one made new swapchains (VR -> flat -> VR): what was
        // made of the old images goes (after its releases drained, in the destructor).
        xr_target.reset();
    }
    if (!xr_target) {
        xr_target_generation = sink->SwapchainGeneration();
        xr_target = std::make_unique<XrPresentTarget>(instance, scheduler, *sink,
                                                      present_vertex_shader,
                                                      *present_pipeline_layout);
    }

    const u32 scale = std::clamp<u32>(GetResolutionScaleFactor(), 1, kMaxScale);
    const std::array<vk::Extent2D, XrPresentTarget::kScreens> sizes = {
        vk::Extent2D{kTopWidth * scale, kScreenHeight * scale},
        vk::Extent2D{kTopWidth * scale, kScreenHeight * scale},
        vk::Extent2D{kBottomWidth * scale, kScreenHeight * scale},
    };
    if (!xr_target->Prepare(sizes)) {
        return false;
    }

    const auto sampler = present_samplers[!Settings::values.filter_mode.GetValue()];
    const auto present_set = present_heap.Commit();
    for (u32 index = 0; index < screen_infos.size(); index++) {
        update_queue.AddImageSampler(present_set, 0, index, screen_infos[index].image_view,
                                     sampler);
    }
    renderpass_cache.EndRendering();

    bool any = false;
    for (u32 screen_id = 0; screen_id < XrPresentTarget::kScreens; screen_id++) {
        const XrPresentTarget::Target* target =
            xr_target->Acquire(static_cast<VideoCore::XrScreen>(screen_id));
        if (!target) {
            continue;
        }
        any = true;
        scheduler.Record([target = *target, present_set, renderpass = xr_target->RenderPass(),
                          pipeline = xr_target->Pipeline(),
                          layout = *present_pipeline_layout](vk::CommandBuffer cmdbuf) {
            const vk::Viewport viewport = {
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(target.width),
                .height = static_cast<float>(target.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            const vk::Rect2D scissor = {
                .offset = {0, 0},
                .extent = {target.width, target.height},
            };
            const vk::ClearValue clear{
                .color = vk::ClearColorValue{.float32 = std::array{0.f, 0.f, 0.f, 1.f}}};
            const vk::RenderPassBeginInfo begin_info = {
                .renderPass = renderpass,
                .framebuffer = target.framebuffer,
                .renderArea = scissor,
                .clearValueCount = 1,
                .pClearValues = &clear,
            };
            cmdbuf.beginRenderPass(begin_info, vk::SubpassContents::eInline);
            cmdbuf.setViewport(0, viewport);
            cmdbuf.setScissor(0, scissor);
            cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
            cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, present_set,
                                      {});
        });
        draw_info.modelview = Orthographic(target->width, target->height);
        draw_info.layer = 0;
        DrawSingleScreen(screen_id, 0.0f, 0.0f, static_cast<float>(target->width),
                         static_cast<float>(target->height),
                         Layout::DisplayOrientation::Landscape);
        scheduler.Record([](vk::CommandBuffer cmdbuf) { cmdbuf.endRenderPass(); });
    }
    if (!any) {
        return false;
    }
    Common::DebugStall::Maybe(Common::DebugStall::Where::Acquire); // AzaharFrame: test only
    // The tick this submission signals (Scheduler::SubmitExecution).
    const u64 tick = scheduler.CurrentTick();
    scheduler.Flush();
    xr_target->QueueRelease(tick);
    return true;
}

} // namespace Vulkan
