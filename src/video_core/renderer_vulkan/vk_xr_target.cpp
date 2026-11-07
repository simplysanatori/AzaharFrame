// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <limits>

#include "common/assert.h"
#include "common/debug_stall.h"
#include "common/logging/log.h"
#include "common/thread.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_xr_target.h"

#include <vk_mem_alloc.h>

namespace Vulkan {

namespace {

// The present fragment shader's interface (vulkan_present.frag), with the output
// written as linear light when the swapchain is sRGB: the hardware encodes it
// back, so the stored bytes are the game's own colour values, which is what the
// compositor's sRGB decode expects. Alpha is opaque (quads are not blended).
constexpr std::string_view XR_PRESENT_FRAG = R"(
#version 450 core
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;

layout (push_constant, std140) uniform DrawInfo {
    mat4 modelview_matrix;
    vec4 i_resolution;
    vec4 o_resolution;
    int screen_id_l;
    int screen_id_r;
    int layer;
    int reverse_interlaced;
};

layout (set = 0, binding = 0) uniform sampler2D screen_textures[3];

vec4 GetScreen(int screen_id) {
    switch (screen_id) {
    case 0:
        return texture(screen_textures[0], frag_tex_coord);
    case 1:
        return texture(screen_textures[1], frag_tex_coord);
    default:
        return texture(screen_textures[2], frag_tex_coord);
    }
}

void main() {
    vec3 c = GetScreen(screen_id_l).rgb;
#ifdef XR_SRGB_TARGET
    c = mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), greaterThan(c, vec3(0.04045)));
#endif
    color = vec4(c, 1.0);
}
)";

bool IsSrgb(vk::Format format) {
    return format == vk::Format::eR8G8B8A8Srgb || format == vk::Format::eB8G8R8A8Srgb;
}

} // Anonymous namespace

XrPresentTarget::XrPresentTarget(const Instance& instance_, Scheduler& scheduler_,
                                 VideoCore::XrScreenSink& sink_, vk::ShaderModule vertex_shader,
                                 vk::PipelineLayout pipeline_layout)
    : instance{instance_}, scheduler{scheduler_}, sink{sink_}, device{instance.GetDevice()},
      format{static_cast<vk::Format>(sink.SwapchainFormat())} {
    const char* no_wait = std::getenv("AZAHARFRAME_XR_NO_GPU_WAIT");
    skip_gpu_wait = no_wait && no_wait[0] == '1';
    if (skip_gpu_wait) {
        LOG_WARNING(Render_Vulkan, "xr: AZAHARFRAME_XR_NO_GPU_WAIT=1, images are released "
                                   "without waiting for the GPU (test only)");
    }
    CreateRenderPass();
    CreatePipeline(vertex_shader, pipeline_layout);
    CreateCopyObjects();
    sink.SetScreensLive(true);
    release_thread = std::jthread([this](std::stop_token token) { ReleaseThread(token); });
    LOG_INFO(Render_Vulkan, "xr: present target ready ({})", vk::to_string(format));
}

XrPresentTarget::~XrPresentTarget() {
    Drain();
    release_thread.request_stop();
    cv.notify_all();
    release_thread.join();
    scheduler.Finish(); // the spill slots may still be drawn into
    sink.SetScreensLive(false);
    // Images waited for ahead of time go back (not shown: the screens are off), so that the
    // next game's target can wait for images of the same swapchains.
    for (std::size_t i = 0; i < kScreens; i++) {
        if (screens[i].armed >= 0) {
            sink.Release(static_cast<VideoCore::XrScreen>(i), true);
            screens[i].armed = -1;
        }
    }
    for (auto& screen : screens) {
        DestroyTargets(screen);
    }
    device.destroyFence(copy_fence);
    device.destroyCommandPool(copy_pool);
    device.destroyPipeline(pipeline);
    device.destroyShaderModule(fragment_shader);
    device.destroyRenderPass(render_pass);
}

void XrPresentTarget::CreateRenderPass() {
    const vk::AttachmentReference color_ref = {
        .attachment = 0,
        .layout = vk::ImageLayout::eColorAttachmentOptimal,
    };
    const vk::SubpassDescription subpass = {
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1u,
        .pColorAttachments = &color_ref,
    };
    // Every pixel is drawn: the old contents are not needed. The image goes back
    // to the runtime in COLOR_ATTACHMENT_OPTIMAL (XR_KHR_vulkan_enable2).
    const vk::AttachmentDescription color_attachment = {
        .format = format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::eColorAttachmentOptimal,
    };
    const std::array dependencies = {
        // The screen textures were written by earlier passes / transfers.
        vk::SubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                            vk::PipelineStageFlagBits::eTransfer |
                            vk::PipelineStageFlagBits::eComputeShader,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                            vk::PipelineStageFlagBits::eFragmentShader,
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                             vk::AccessFlagBits::eTransferWrite |
                             vk::AccessFlagBits::eShaderWrite,
            .dstAccessMask =
                vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eShaderRead,
        },
        vk::SubpassDependency{
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = {},
        },
    };
    const vk::RenderPassCreateInfo info = {
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = static_cast<u32>(dependencies.size()),
        .pDependencies = dependencies.data(),
    };
    render_pass = device.createRenderPass(info);
}

void XrPresentTarget::CreatePipeline(vk::ShaderModule vertex_shader,
                                     vk::PipelineLayout pipeline_layout) {
    fragment_shader = Compile(XR_PRESENT_FRAG, vk::ShaderStageFlagBits::eFragment, device,
                              IsSrgb(format) ? "#define XR_SRGB_TARGET\n" : "");

    // Vertex layout of RendererVulkan's ScreenRectVertex: position, tex_coord.
    const vk::VertexInputBindingDescription binding = {
        .binding = 0,
        .stride = sizeof(float) * 4,
        .inputRate = vk::VertexInputRate::eVertex,
    };
    const std::array attributes = {
        vk::VertexInputAttributeDescription{
            .location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = 0},
        vk::VertexInputAttributeDescription{.location = 1,
                                            .binding = 0,
                                            .format = vk::Format::eR32G32Sfloat,
                                            .offset = sizeof(float) * 2},
    };
    const vk::PipelineVertexInputStateCreateInfo vertex_input = {
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = static_cast<u32>(attributes.size()),
        .pVertexAttributeDescriptions = attributes.data(),
    };
    const vk::PipelineInputAssemblyStateCreateInfo input_assembly = {
        .topology = vk::PrimitiveTopology::eTriangleStrip,
        .primitiveRestartEnable = false,
    };
    const vk::PipelineRasterizationStateCreateInfo raster_state = {
        .depthClampEnable = false,
        .rasterizerDiscardEnable = false,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = false,
        .lineWidth = 1.0f,
    };
    const vk::PipelineMultisampleStateCreateInfo multisampling = {
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = false,
    };
    const vk::PipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = false,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    const vk::PipelineColorBlendStateCreateInfo color_blending = {
        .logicOpEnable = false,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
    };
    const vk::Viewport placeholder_viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    const vk::Rect2D placeholder_scissor = {{0, 0}, {1, 1}};
    const vk::PipelineViewportStateCreateInfo viewport_info = {
        .viewportCount = 1,
        .pViewports = &placeholder_viewport,
        .scissorCount = 1,
        .pScissors = &placeholder_scissor,
    };
    const std::array dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    const vk::PipelineDynamicStateCreateInfo dynamic_info = {
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };
    const vk::PipelineDepthStencilStateCreateInfo depth_info = {
        .depthTestEnable = false,
        .depthWriteEnable = false,
        .depthCompareOp = vk::CompareOp::eAlways,
        .depthBoundsTestEnable = false,
        .stencilTestEnable = false,
    };
    const std::array stages = {
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertex_shader,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragment_shader,
            .pName = "main",
        },
    };
    const vk::GraphicsPipelineCreateInfo info = {
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_info,
        .pRasterizationState = &raster_state,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_info,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_info,
        .layout = pipeline_layout,
        .renderPass = render_pass,
    };
    const auto [result, made] = device.createGraphicsPipeline({}, info);
    ASSERT_MSG(result == vk::Result::eSuccess, "Unable to build the XR present pipeline");
    pipeline = made;
}

void XrPresentTarget::CreateCopyObjects() {
    copy_pool = device.createCommandPool({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = instance.GetGraphicsQueueFamilyIndex(),
    });
    copy_cmdbuf = device
                      .allocateCommandBuffers({
                          .commandPool = copy_pool,
                          .level = vk::CommandBufferLevel::ePrimary,
                          .commandBufferCount = 1,
                      })
                      .front();
    copy_fence = device.createFence({});
}

bool XrPresentTarget::CreateSpill(Screen& screen) {
    for (SpillSlot& slot : screen.spill) {
        const vk::ImageCreateInfo image_info = {
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = {screen.size.width, screen.size.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .usage = vk::ImageUsageFlagBits::eColorAttachment |
                     vk::ImageUsageFlagBits::eTransferSrc,
        };
        const VmaAllocationCreateInfo alloc_info = {
            .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        };
        VkImage image{};
        const VkImageCreateInfo unsafe_info = static_cast<VkImageCreateInfo>(image_info);
        const VkResult result = vmaCreateImage(instance.GetAllocator(), &unsafe_info, &alloc_info,
                                               &image, &slot.allocation, nullptr);
        if (result != VK_SUCCESS) {
            LOG_ERROR(Render_Vulkan, "xr: no spill image {}x{} ({}): a frame drawn while the "
                                     "last one is in flight is dropped",
                      screen.size.width, screen.size.height, static_cast<int>(result));
            slot.allocation = {};
            DestroySpill(screen);
            return false;
        }
        slot.image = vk::Image{image};
        slot.view = device.createImageView({
            .image = slot.image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        });
        slot.framebuffer = device.createFramebuffer({
            .renderPass = render_pass,
            .attachmentCount = 1,
            .pAttachments = &slot.view,
            .width = screen.size.width,
            .height = screen.size.height,
            .layers = 1,
        });
    }
    return true;
}

void XrPresentTarget::DestroySpill(Screen& screen) {
    for (SpillSlot& slot : screen.spill) {
        if (slot.framebuffer) {
            device.destroyFramebuffer(slot.framebuffer);
        }
        if (slot.view) {
            device.destroyImageView(slot.view);
        }
        if (slot.image) {
            vmaDestroyImage(instance.GetAllocator(), static_cast<VkImage>(slot.image),
                            slot.allocation);
        }
        slot = {};
    }
    screen.spill_ok = false;
}

void XrPresentTarget::DestroyTargets(Screen& screen) {
    for (auto fb : screen.framebuffers) {
        device.destroyFramebuffer(fb);
    }
    for (auto view : screen.views) {
        device.destroyImageView(view);
    }
    DestroySpill(screen);
    screen.framebuffers.clear();
    screen.views.clear();
    screen.images.clear();
    screen.size = vk::Extent2D{};
}

bool XrPresentTarget::SpillReady() const {
    return std::all_of(screens.begin(), screens.end(),
                       [](const Screen& screen) { return screen.spill_ok; });
}

void XrPresentTarget::Drain() {
    std::unique_lock lock{mutex};
    handoff.SetPaused(true);
    if (handoff.DropQueued()) {
        sink.NoteSpillReplaced();
    }
    if (handoff.Idle()) {
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    cv.notify_all();
    cv.wait(lock, [this] { return handoff.Idle(); });
    sink.NoteEmuWait(static_cast<u32>(std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::steady_clock::now() - start)
                                          .count()));
}

bool XrPresentTarget::Prepare(const std::array<vk::Extent2D, kScreens>& sizes) {
    bool drained = false;
    bool ok = true;
    for (std::size_t i = 0; i < kScreens; i++) {
        Screen& screen = screens[i];
        const vk::Extent2D size = sizes[i];
        if (screen.size == size && !screen.framebuffers.empty()) {
            continue;
        }
        // A new size (first frame, or the resolution factor changed): nothing of
        // this screen may be in flight while its swapchain is remade.
        if (!drained) {
            Drain();
            scheduler.Finish();
            drained = true;
        }
        DestroyTargets(screen);
        {
            std::scoped_lock lock{mutex};
            screen.armed = -1; // a waited image of the old swapchain goes with it
            handoff.Unarm();
        }
        const auto which = static_cast<VideoCore::XrScreen>(i);
        if (!sink.EnsureSwapchain(which, size.width, size.height)) {
            ok = false;
            break;
        }
        // As many images as the runtime made (an index past them would leave a waited
        // image that is never released).
        std::vector<VkImage> images(sink.SwapchainImages(which, nullptr, 0));
        const u32 count = std::min<u32>(
            sink.SwapchainImages(which, images.data(), static_cast<u32>(images.size())),
            static_cast<u32>(images.size()));
        for (u32 n = 0; n < count; n++) {
            const vk::ImageViewCreateInfo view_info = {
                .image = vk::Image{images[n]},
                .viewType = vk::ImageViewType::e2D,
                .format = format,
                .subresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            const vk::ImageView view = device.createImageView(view_info);
            const vk::FramebufferCreateInfo fb_info = {
                .renderPass = render_pass,
                .attachmentCount = 1,
                .pAttachments = &view,
                .width = size.width,
                .height = size.height,
                .layers = 1,
            };
            screen.images.push_back(vk::Image{images[n]});
            screen.views.push_back(view);
            screen.framebuffers.push_back(device.createFramebuffer(fb_info));
        }
        screen.size = size;
        screen.spill_ok = CreateSpill(screen);
    }
    if (drained) {
        std::scoped_lock lock{mutex};
        handoff.SetPaused(false);
        cv.notify_all();
    }
    return ok;
}

bool XrPresentTarget::BeginFrame(std::array<Target, kScreens>& targets) {
    sink.NoteEmuFrame();
    {
        std::scoped_lock lock{mutex};
        frame = handoff.BeginFrame(SpillReady());
        if (frame.path == XrHandoff::Path::Direct) {
            for (std::size_t i = 0; i < kScreens; i++) {
                const Screen& screen = screens[i];
                targets[i] = Target{
                    .framebuffer = screen.framebuffers[static_cast<std::size_t>(screen.armed)],
                    .width = screen.size.width,
                    .height = screen.size.height,
                };
            }
        }
        cv.notify_all(); // arm when the images are not armed yet
    }
    if (frame.path == XrHandoff::Path::None) {
        return false;
    }
    if (frame.path == XrHandoff::Path::Spill) {
        // The swapchain images are still in use (the GPU has not finished the frame
        // before, or its copy is in flight): drawn aside, copied in by the handoff thread.
        for (std::size_t i = 0; i < kScreens; i++) {
            const Screen& screen = screens[i];
            sink.NoteLateRelease(static_cast<VideoCore::XrScreen>(i));
            targets[i] = Target{
                .framebuffer = screen.spill[frame.slot].framebuffer,
                .width = screen.size.width,
                .height = screen.size.height,
            };
        }
        if (frame.replaced) {
            sink.NoteSpillReplaced();
        }
    }
    return true;
}

void XrPresentTarget::EndFrame(u64 tick) {
    std::scoped_lock lock{mutex};
    handoff.EndFrame(frame, tick);
    frame = {};
    cv.notify_all();
}

void XrPresentTarget::RunArm() {
    std::array<int32_t, kScreens> got{};
    {
        std::scoped_lock lock{mutex};
        for (std::size_t i = 0; i < kScreens; i++) {
            got[i] = screens[i].armed;
        }
    }
    for (std::size_t i = 0; i < kScreens; i++) {
        if (got[i] >= 0) {
            continue;
        }
        const auto which = static_cast<VideoCore::XrScreen>(i);
        const int32_t index = sink.Acquire(which);
        if (index >= 0 && static_cast<std::size_t>(index) >= screens[i].framebuffers.size()) {
            // Acquired and waited, so it must go back or the swapchain is stuck.
            LOG_ERROR(Render_Vulkan, "xr: image index {} past the {} framebuffers", index,
                      screens[i].framebuffers.size());
            sink.Release(which, true);
            continue;
        }
        got[i] = index;
    }
    std::scoped_lock lock{mutex};
    bool all = true;
    for (std::size_t i = 0; i < kScreens; i++) {
        screens[i].armed = got[i];
        all &= got[i] >= 0;
    }
    handoff.ArmDone(all);
    cv.notify_all();
}

void XrPresentTarget::RunReleaseDirect(u64 tick) {
    MasterSemaphore* master = scheduler.GetMasterSemaphore();
    if (!skip_gpu_wait) {
        master->Wait(tick);
    }
    const bool finished = master->IsFree(tick);
    Common::DebugStall::Maybe(Common::DebugStall::Where::Release); // test only
    for (std::size_t i = 0; i < kScreens; i++) {
        sink.Release(static_cast<VideoCore::XrScreen>(i), finished);
    }
    std::scoped_lock lock{mutex};
    for (Screen& screen : screens) {
        screen.armed = -1;
    }
    handoff.ReleaseDone();
    cv.notify_all();
}

void XrPresentTarget::RunCopySpill(std::size_t slot, u64 tick) {
    // The frame's draw into the slot was submitted before `tick` signals; the copy's own
    // submission comes after, and the queue runs them in that order.
    scheduler.GetMasterSemaphore()->Wait(tick);
    copy_cmdbuf.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    constexpr vk::ImageSubresourceRange range = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    std::array<vk::ImageMemoryBarrier, kScreens * 2> pre{};
    std::array<vk::ImageMemoryBarrier, kScreens> post{};
    for (std::size_t i = 0; i < kScreens; i++) {
        const Screen& screen = screens[i];
        const vk::Image dst = screen.images[static_cast<std::size_t>(screen.armed)];
        pre[i * 2] = vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eTransferRead,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::eTransferSrcOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = screen.spill[slot].image,
            .subresourceRange = range,
        };
        // The runtime is done with a waited image; every pixel is written.
        pre[i * 2 + 1] = vk::ImageMemoryBarrier{
            .srcAccessMask = {},
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst,
            .subresourceRange = range,
        };
        // Back in COLOR_ATTACHMENT_OPTIMAL for the runtime (XR_KHR_vulkan_enable2).
        post[i] = vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = {},
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst,
            .subresourceRange = range,
        };
    }
    copy_cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                    vk::PipelineStageFlagBits::eTopOfPipe,
                                vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, pre);
    for (std::size_t i = 0; i < kScreens; i++) {
        const Screen& screen = screens[i];
        const vk::ImageCopy region = {
            .srcSubresource{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .srcOffset = {},
            .dstSubresource{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
            .dstOffset = {},
            .extent = {screen.size.width, screen.size.height, 1},
        };
        copy_cmdbuf.copyImage(screen.spill[slot].image, vk::ImageLayout::eTransferSrcOptimal,
                              screen.images[static_cast<std::size_t>(screen.armed)],
                              vk::ImageLayout::eTransferDstOptimal, region);
    }
    copy_cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {}, post);
    copy_cmdbuf.end();
    const vk::SubmitInfo submit = {
        .commandBufferCount = 1,
        .pCommandBuffers = &copy_cmdbuf,
    };
    bool finished = false;
    try {
        {
            std::scoped_lock queue_lock{scheduler.submit_mutex};
            instance.GetGraphicsQueue().submit(submit, copy_fence);
        }
        finished = device.waitForFences(copy_fence, true, std::numeric_limits<u64>::max()) ==
                   vk::Result::eSuccess;
        device.resetFences(copy_fence);
    } catch (vk::DeviceLostError& err) {
        UNREACHABLE_MSG("Device lost during the XR spill copy: {}", err.what());
    }
    copy_cmdbuf.reset();
    Common::DebugStall::Maybe(Common::DebugStall::Where::Release); // test only
    for (std::size_t i = 0; i < kScreens; i++) {
        sink.Release(static_cast<VideoCore::XrScreen>(i), finished);
    }
    sink.NoteSpillShown();
    std::scoped_lock lock{mutex};
    for (Screen& screen : screens) {
        screen.armed = -1;
    }
    handoff.CopyDone(slot);
    cv.notify_all();
}

void XrPresentTarget::ReleaseThread(std::stop_token token) {
    Common::SetCurrentThreadName("XrRelease");
    while (true) {
        XrHandoff::Job job;
        {
            std::unique_lock lock{mutex};
            Common::CondvarWait(cv, lock, token, [&] {
                job = handoff.NextJob();
                return job.kind != XrHandoff::JobKind::None;
            });
            if (job.kind == XrHandoff::JobKind::None) {
                return; // stop requested (the destructor drained first)
            }
        }
        switch (job.kind) {
        case XrHandoff::JobKind::ReleaseDirect:
            RunReleaseDirect(job.tick);
            break;
        case XrHandoff::JobKind::Arm:
            RunArm();
            break;
        case XrHandoff::JobKind::CopySpill:
            RunCopySpill(job.slot, job.tick);
            break;
        case XrHandoff::JobKind::None:
            break;
        }
    }
}

bool UsesXrDevice(const Instance& instance) {
    const VideoCore::VulkanHooks* hooks = VideoCore::GetVulkanHooks();
    return hooks && hooks->OwnsDevice(static_cast<VkDevice>(instance.GetDevice()));
}

XrQueueRegistration::XrQueueRegistration(const Instance& instance, Scheduler& scheduler)
    : sink{UsesXrDevice(instance) ? VideoCore::GetXrScreenSink() : nullptr} {
    if (sink) {
        sink->SetQueueMutex(&scheduler.submit_mutex);
    }
}

XrQueueRegistration::~XrQueueRegistration() {
    if (sink) {
        sink->SetQueueMutex(nullptr);
    }
}

} // namespace Vulkan
