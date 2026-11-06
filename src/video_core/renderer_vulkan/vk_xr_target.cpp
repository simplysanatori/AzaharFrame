// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cstdlib>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/thread.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_xr_target.h"

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
    sink.SetScreensLive(true);
    release_thread = std::jthread([this](std::stop_token token) { ReleaseThread(token); });
    LOG_INFO(Render_Vulkan, "xr: present target ready ({})", vk::to_string(format));
}

XrPresentTarget::~XrPresentTarget() {
    WaitIdle();
    release_thread.request_stop();
    cv.notify_all();
    release_thread.join();
    sink.SetScreensLive(false);
    for (auto& screen : screens) {
        DestroyTargets(screen);
    }
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

void XrPresentTarget::DestroyTargets(Screen& screen) {
    for (auto fb : screen.framebuffers) {
        device.destroyFramebuffer(fb);
    }
    for (auto view : screen.views) {
        device.destroyImageView(view);
    }
    screen.framebuffers.clear();
    screen.views.clear();
    screen.size = vk::Extent2D{};
}

void XrPresentTarget::WaitIdle() {
    std::unique_lock lock{mutex};
    cv.wait(lock, [this] { return queue.empty(); });
}

bool XrPresentTarget::Prepare(const std::array<vk::Extent2D, kScreens>& sizes) {
    for (std::size_t i = 0; i < kScreens; i++) {
        Screen& screen = screens[i];
        const vk::Extent2D size = sizes[i];
        if (screen.size == size && !screen.framebuffers.empty()) {
            continue;
        }
        // A new size (first frame, or the resolution factor changed): nothing of
        // this screen may be in flight while its swapchain is remade.
        WaitIdle();
        scheduler.Finish();
        DestroyTargets(screen);
        const auto which = static_cast<VideoCore::XrScreen>(i);
        if (!sink.EnsureSwapchain(which, size.width, size.height)) {
            return false;
        }
        std::array<VkImage, 8> images{};
        const u32 count = std::min<u32>(sink.SwapchainImages(which, images.data(), 8), 8);
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
            screen.views.push_back(view);
            screen.framebuffers.push_back(device.createFramebuffer(fb_info));
        }
        screen.size = size;
    }
    return true;
}

const XrPresentTarget::Target* XrPresentTarget::Acquire(VideoCore::XrScreen which) {
    Screen& screen = screens[static_cast<std::size_t>(which)];
    {
        std::unique_lock lock{mutex};
        if (screen.pending) {
            sink.NoteLateRelease(which);
            cv.wait(lock, [&] { return !screen.pending; });
        }
    }
    const int32_t index = sink.Acquire(which);
    if (index < 0 || static_cast<std::size_t>(index) >= screen.framebuffers.size()) {
        return nullptr;
    }
    screen.acquired = true;
    screen.current = Target{
        .framebuffer = screen.framebuffers[index],
        .width = screen.size.width,
        .height = screen.size.height,
    };
    return &screen.current;
}

void XrPresentTarget::QueueRelease(u64 tick) {
    PendingRelease release{.tick = tick, .screens = {}};
    bool any = false;
    std::scoped_lock lock{mutex};
    for (std::size_t i = 0; i < kScreens; i++) {
        if (screens[i].acquired) {
            screens[i].acquired = false;
            screens[i].pending = true;
            release.screens[i] = true;
            any = true;
        }
    }
    if (any) {
        queue.push_back(release);
        cv.notify_all();
    }
}

void XrPresentTarget::ReleaseThread(std::stop_token token) {
    Common::SetCurrentThreadName("XrRelease");
    MasterSemaphore* master = scheduler.GetMasterSemaphore();
    while (true) {
        PendingRelease release;
        {
            std::unique_lock lock{mutex};
            Common::CondvarWait(cv, lock, token, [this] { return !queue.empty(); });
            if (queue.empty()) {
                return; // stop requested and drained
            }
            release = queue.front();
        }
        if (!skip_gpu_wait) {
            master->Wait(release.tick);
        }
        const bool finished = master->IsFree(release.tick);
        for (std::size_t i = 0; i < kScreens; i++) {
            if (release.screens[i]) {
                sink.Release(static_cast<VideoCore::XrScreen>(i), finished);
            }
        }
        std::scoped_lock lock{mutex};
        queue.pop_front();
        for (std::size_t i = 0; i < kScreens; i++) {
            if (release.screens[i]) {
                screens[i].pending = false;
            }
        }
        cv.notify_all();
    }
}

XrQueueRegistration::XrQueueRegistration(Scheduler& scheduler)
    : sink{VideoCore::GetXrScreenSink()} {
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
