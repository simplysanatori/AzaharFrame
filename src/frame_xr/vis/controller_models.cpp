// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// The pass follows Moonlight XR Native's xr/xr_model.c (renderControllerModels, modelRecord;
// GPLv3, same author), as a Vulkan pass of its own.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "common/file_util.h"
#include "common/logging/log.h"
#include "frame_xr/session.h"
#include "frame_xr/vis/controller_models.h"
#include "frame_xr/vis/model_shaders.h"
#include "frame_xr/xr_loader.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"

namespace FrameXr::Vis {

namespace {

using Mat = std::array<float, 16>; // column major

constexpr float kNear = 0.05f;
constexpr float kFar = 20.0f;
constexpr uint32_t kMaxEyeHeight = 1000;
constexpr float kEyeScale = 0.5f;

Mat Multiply(const Mat& a, const Mat& b) {
    Mat m{};
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            float v = 0.0f;
            for (int k = 0; k < 4; k++) {
                v += a[k * 4 + r] * b[c * 4 + k];
            }
            m[c * 4 + r] = v;
        }
    }
    return m;
}

Mat FromPose(const XrQuaternionf& q, const XrVector3f& p) {
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    Mat m{};
    m[0] = 1 - 2 * (y * y + z * z);
    m[1] = 2 * (x * y + z * w);
    m[2] = 2 * (x * z - y * w);
    m[4] = 2 * (x * y - z * w);
    m[5] = 1 - 2 * (x * x + z * z);
    m[6] = 2 * (y * z + x * w);
    m[8] = 2 * (x * z + y * w);
    m[9] = 2 * (y * z - x * w);
    m[10] = 1 - 2 * (x * x + y * y);
    m[12] = p.x;
    m[13] = p.y;
    m[14] = p.z;
    m[15] = 1.0f;
    return m;
}

// The inverse of a rigid transform.
Mat InverseRigid(const Mat& m) {
    Mat r{};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r[j * 4 + i] = m[i * 4 + j];
        }
    }
    for (int i = 0; i < 3; i++) {
        r[12 + i] = -(r[i] * m[12] + r[4 + i] * m[13] + r[8 + i] * m[14]);
    }
    r[15] = 1.0f;
    return r;
}

// OpenGL style (y up, z -1..1); the vertex shader turns it into Vulkan's clip space.
Mat Projection(const XrFovf& fov) {
    const float l = std::tan(fov.angleLeft), r = std::tan(fov.angleRight);
    const float u = std::tan(fov.angleUp), d = std::tan(fov.angleDown);
    Mat m{};
    m[0] = 2.0f / (r - l);
    m[5] = 2.0f / (u - d);
    m[8] = (r + l) / (r - l);
    m[9] = (u + d) / (u - d);
    m[10] = -(kFar + kNear) / (kFar - kNear);
    m[11] = -1.0f;
    m[14] = -(2.0f * kFar * kNear) / (kFar - kNear);
    return m;
}

// A direction (w = 0) or point (w = 1) through m.
std::array<float, 4> Apply(const Mat& m, float x, float y, float z, float w) {
    return {m[0] * x + m[4] * y + m[8] * z + m[12] * w, m[1] * x + m[5] * y + m[9] * z + m[13] * w,
            m[2] * x + m[6] * y + m[10] * z + m[14] * w, w};
}

XrPosef ToXr(const Input::Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

struct PushConstants {
    Mat mvp;
    std::array<float, 4> eye;
    std::array<float, 4> key;
    std::array<float, 4> up;
    std::array<float, 4> params;
};
static_assert(sizeof(PushConstants) == 128);

} // namespace

std::string ControllerModelDir() {
    if (const char* env = std::getenv("AZAHARFRAME_MODELS"); env && *env) {
        std::string dir = env;
        return dir.back() == '/' ? dir : dir + "/";
    }
    return FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + "frame-models/";
}

bool ReadControllerModels(std::array<ModelFile, 2>& files, std::string& error) {
    const std::string dir = ControllerModelDir();
    const char* names[2] = {"frame_controller_left.model", "frame_controller_right.model"};
    for (int h = 0; h < 2; h++) {
        std::string why;
        if (!LoadModelFile(dir + names[h], files[h], why)) {
            error = dir + names[h] + ": " + why;
            return false;
        }
    }
    return true;
}

struct ControllerModels::Files {
    std::array<ModelFile, 2> hand;
    std::vector<uint32_t> vert_spv_linear, frag_spv_linear, vert_spv, frag_spv;
};

struct ControllerModels::Gpu {
    // 0 nothing, 1 uploading, 2 live, 3 failed (this session)
    int state = 0;
    QueueLock* queue_lock = nullptr;
    vk::Device device;
    vk::Queue queue;
    vk::CommandPool pool;
    vk::CommandBuffer cmd, upload_cmd;
    vk::Fence fence, upload_fence;
    std::vector<std::pair<vk::Buffer, vk::DeviceMemory>> buffers; // everything allocated
    std::vector<std::pair<vk::Image, vk::DeviceMemory>> owned_images;
    std::vector<vk::ImageView> views;
    vk::Buffer staging;
    vk::DeviceMemory staging_memory;

    struct HandGpu {
        vk::Buffer vertices, indices;
        vk::ImageView colour, surface;
        vk::DescriptorSet set;
    };
    std::array<HandGpu, 2> hands;
    vk::Sampler sampler;
    vk::DescriptorSetLayout set_layout;
    vk::DescriptorPool descriptor_pool;
    vk::PipelineLayout pipeline_layout;
    vk::RenderPass render_pass;
    vk::Pipeline pipeline;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    vk::ImageView msaa_view, depth_view;
    std::vector<vk::ImageView> swap_views;
    std::vector<vk::Framebuffer> framebuffers;

    XrSwapchain swapchain = XR_NULL_HANDLE;
    std::vector<VkImage> swap_images;
    uint32_t eye_w = 0, eye_h = 0;
    bool acquired = false;
    uint32_t index = 0;
    bool in_flight = false;
    bool has_image = false;
    ViewPose drawn_views;  ///< the views of the image in flight
    ViewPose shown_views;  ///< the views of the image released last
    bool shown_fresh = false; ///< released since the models last went away

    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::array<XrCompositionLayerProjectionView, 2> layer_views{
        {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};

    vk::PhysicalDevice physical;

    uint32_t Memory(uint32_t bits, vk::MemoryPropertyFlags want, vk::MemoryPropertyFlags fallback) {
        const auto props = physical.getMemoryProperties();
        for (const auto flags : {want, fallback}) {
            for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
                if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) {
                    return i;
                }
            }
        }
        throw std::runtime_error("no memory type");
    }

    vk::Buffer Buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags mem,
                      vk::DeviceMemory* memory_out = nullptr) {
        const vk::Buffer b = device.createBuffer(
            {.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive});
        const auto req = device.getBufferMemoryRequirements(b);
        const vk::DeviceMemory m = device.allocateMemory(
            {.allocationSize = req.size, .memoryTypeIndex = Memory(req.memoryTypeBits, mem, mem)});
        device.bindBufferMemory(b, m, 0);
        if (memory_out) {
            *memory_out = m;
        } else {
            buffers.emplace_back(b, m);
        }
        return b;
    }

    vk::Image Image(const vk::ImageCreateInfo& info, bool lazy) {
        const vk::Image image = device.createImage(info);
        const auto req = device.getImageMemoryRequirements(image);
        const auto local = vk::MemoryPropertyFlagBits::eDeviceLocal;
        const uint32_t type =
            lazy ? Memory(req.memoryTypeBits, local | vk::MemoryPropertyFlagBits::eLazilyAllocated,
                          local)
                 : Memory(req.memoryTypeBits, local, {});
        const vk::DeviceMemory m =
            device.allocateMemory({.allocationSize = req.size, .memoryTypeIndex = type});
        device.bindImageMemory(image, m, 0);
        owned_images.emplace_back(image, m);
        return image;
    }

    vk::ImageView View(vk::Image image, vk::Format format, vk::ImageAspectFlags aspect,
                       uint32_t levels) {
        const vk::ImageView v = device.createImageView({
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {aspect, 0, levels, 0, 1},
        });
        views.push_back(v);
        return v;
    }
};

ControllerModels::ControllerModels() = default;

ControllerModels::~ControllerModels() {
    DestroyXr();
    if (loader.joinable()) {
        loader.join();
    }
}

const XrCompositionLayerBaseHeader* ControllerModels::Layer() const {
    return layer_ready ? reinterpret_cast<const XrCompositionLayerBaseHeader*>(&gpu->layer)
                       : nullptr;
}

void ControllerModels::DestroyXr() {
    layer_ready = false;
    if (!gpu) {
        return;
    }
    Gpu& g = *gpu;
    if (g.device) {
        if (g.in_flight) {
            static_cast<void>(g.device.waitForFences(g.fence, true, 1'000'000'000));
            XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            g.queue_lock->With([&] { return Xr().xrReleaseSwapchainImage(g.swapchain, &ri); });
        }
        if (g.state == 1) {
            static_cast<void>(g.device.waitForFences(g.upload_fence, true, 1'000'000'000));
        }
        for (auto fb : g.framebuffers) {
            g.device.destroyFramebuffer(fb);
        }
        for (auto v : g.swap_views) {
            g.device.destroyImageView(v);
        }
        for (auto v : g.views) {
            g.device.destroyImageView(v);
        }
        for (auto& [image, memory] : g.owned_images) {
            g.device.destroyImage(image);
            g.device.freeMemory(memory);
        }
        for (auto& [buffer, memory] : g.buffers) {
            g.device.destroyBuffer(buffer);
            g.device.freeMemory(memory);
        }
        if (g.staging) {
            g.device.destroyBuffer(g.staging);
            g.device.freeMemory(g.staging_memory);
        }
        if (g.pipeline) {
            g.device.destroyPipeline(g.pipeline);
        }
        if (g.pipeline_layout) {
            g.device.destroyPipelineLayout(g.pipeline_layout);
        }
        if (g.render_pass) {
            g.device.destroyRenderPass(g.render_pass);
        }
        if (g.descriptor_pool) {
            g.device.destroyDescriptorPool(g.descriptor_pool);
        }
        if (g.set_layout) {
            g.device.destroyDescriptorSetLayout(g.set_layout);
        }
        if (g.sampler) {
            g.device.destroySampler(g.sampler);
        }
        if (g.fence) {
            g.device.destroyFence(g.fence);
        }
        if (g.upload_fence) {
            g.device.destroyFence(g.upload_fence);
        }
        if (g.pool) {
            g.device.destroyCommandPool(g.pool);
        }
    }
    if (g.swapchain != XR_NULL_HANDLE) {
        const auto destroy = [&] { return Xr().xrDestroySwapchain(g.swapchain); };
        g.queue_lock ? g.queue_lock->With(destroy) : destroy();
    }
    gpu.reset();
}

namespace {

// Reads both hands (the loader thread).
bool ReadFiles(std::array<ModelFile, 2>& files) {
    std::string error;
    if (!ReadControllerModels(files, error)) {
        LOG_WARNING(Frontend, "xr: controller models: {} (only the beams are drawn; "
                              "steam-frame/tools/frame-controller-models.py makes them)",
                    error);
        return false;
    }
    return true;
}

} // namespace

void ControllerModels::Frame(Runtime& rt, const ViewPose& views, const std::array<Hand, 2>& hands) {
    layer_ready = false;
    const bool any = hands[0].shown || hands[1].shown;

    // ---- The files, once a run, on a thread ----
    if (load_state == 0) {
        if (!any) {
            return; // nothing wanted yet: read nothing
        }
        files = std::make_unique<Files>();
        load_state = 1;
        loader = std::thread([this] {
            Files& f = *files;
            if (!ReadFiles(f.hand)) {
                load_state = 3;
                return;
            }
            const std::string vert = std::string{"#version 450\n"} + Shaders::kModelVert;
            const std::string frag_lin =
                std::string{"#version 450\n#define OUT_LINEAR true\n"} + Shaders::kModelFrag;
            const std::string frag_srgb =
                std::string{"#version 450\n#define OUT_LINEAR false\n"} + Shaders::kModelFrag;
            f.vert_spv = Vulkan::CompileGLSL(vert, vk::ShaderStageFlagBits::eVertex);
            f.frag_spv_linear = Vulkan::CompileGLSL(frag_lin, vk::ShaderStageFlagBits::eFragment);
            f.frag_spv = Vulkan::CompileGLSL(frag_srgb, vk::ShaderStageFlagBits::eFragment);
            if (f.vert_spv.empty() || f.frag_spv.empty() || f.frag_spv_linear.empty()) {
                LOG_ERROR(Frontend, "xr: controller models: the shaders did not compile");
                load_state = 3;
                return;
            }
            LOG_INFO(Frontend, "xr: controller models read ({} and {} triangles, textures {}x{})",
                     f.hand[0].indices.size() / 3, f.hand[1].indices.size() / 3,
                     f.hand[0].tex_width, f.hand[0].tex_height);
            load_state = 2;
        });
        return;
    }
    if (load_state != 2) {
        return;
    }
    if (loader.joinable()) {
        loader.join();
    }

    // ---- The GPU side, once a session ----
    if (!gpu) {
        if (!any) {
            return;
        }
        gpu = std::make_unique<Gpu>();
        try {
            Setup(rt);
        } catch (const std::exception& e) {
            LOG_ERROR(Frontend, "xr: controller models: setting up the pass failed: {}", e.what());
            gpu->state = 3;
        }
    }
    Gpu& g = *gpu;
    if (g.state == 3) {
        return;
    }
    auto& xr = Xr();
    if (g.state == 1) {
        if (g.device.getFenceStatus(g.upload_fence) != vk::Result::eSuccess) {
            return;
        }
        g.device.destroyBuffer(g.staging);
        g.device.freeMemory(g.staging_memory);
        g.staging = nullptr;
        g.staging_memory = nullptr;
        g.state = 2;
        LOG_INFO(Frontend, "xr: controller models on the GPU");
    }

    // ---- Release the image drawn on an earlier frame once the GPU is done with it ----
    if (g.in_flight && g.device.getFenceStatus(g.fence) == vk::Result::eSuccess) {
        g.device.resetFences(g.fence);
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        const XrResult r =
            g.queue_lock->With([&] { return xr.xrReleaseSwapchainImage(g.swapchain, &ri); });
        g.in_flight = false;
        if (XR_SUCCEEDED(r)) {
            g.has_image = true;
            g.shown_views = g.drawn_views;
            g.shown_fresh = true;
        }
    }
    if (!any) {
        g.shown_fresh = false; // when they come back, a new image first
        return;
    }

    // ---- Draw the next one ----
    if (!g.in_flight && views.valid) {
        if (!g.acquired) {
            XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            if (XR_SUCCEEDED(g.queue_lock->With(
                    [&] { return xr.xrAcquireSwapchainImage(g.swapchain, &ai, &g.index); }))) {
                g.acquired = true;
            }
        }
        XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wi.timeout = 0;
        if (g.acquired && xr.xrWaitSwapchainImage(g.swapchain, &wi) == XR_SUCCESS) {
            g.acquired = false;
            Record(views, hands);
            const vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &g.cmd};
            g.queue_lock->With([&] {
                g.queue.submit(submit, g.fence);
                return 0;
            });
            g.in_flight = true;
            g.drawn_views = views;
        }
    }

    // ---- The layer: the image released last, at the eyes it was drawn for ----
    if (g.has_image && g.shown_fresh) {
        for (int e = 0; e < 2; e++) {
            auto& v = g.layer_views[static_cast<std::size_t>(e)];
            v.pose = g.shown_views.eye[e];
            v.fov = g.shown_views.fov[e];
            v.subImage.swapchain = g.swapchain;
            v.subImage.imageRect = {{static_cast<int32_t>(g.eye_w * e), 0},
                                    {static_cast<int32_t>(g.eye_w), static_cast<int32_t>(g.eye_h)}};
            v.subImage.imageArrayIndex = 0;
        }
        g.layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        g.layer.space = rt.local_space;
        g.layer.viewCount = 2;
        g.layer.views = g.layer_views.data();
        layer_ready = true;
    }
}


void ControllerModels::Setup(Runtime& rt) {
    Gpu& g = *gpu;
    Files& f = *files;
    auto& xr = Xr();
    g.queue_lock = &rt.queue_lock;
    g.device = vk::Device{rt.vulkan->Device()};
    g.physical = vk::PhysicalDevice{rt.vulkan->PhysicalDevice()};
    g.queue = g.device.getQueue(rt.vulkan->QueueFamily(), 0);
    const vk::Format format = static_cast<vk::Format>(rt.swapchain_format);
    const bool srgb = rt.swapchain_format == VK_FORMAT_R8G8B8A8_SRGB ||
                      rt.swapchain_format == VK_FORMAT_B8G8R8A8_SRGB;

    // The swapchain: both eyes side by side, about half the recommended size, at most 1000 high.
    uint32_t count = 0;
    xr.xrEnumerateViewConfigurationViews(rt.instance, rt.system,
                                         XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count,
                                         nullptr);
    std::vector<XrViewConfigurationView> config(count, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    xr.xrEnumerateViewConfigurationViews(rt.instance, rt.system,
                                         XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count, &count,
                                         config.data());
    uint32_t rec_w = count ? config[0].recommendedImageRectWidth : 2000;
    uint32_t rec_h = count ? config[0].recommendedImageRectHeight : 2000;
    uint32_t max_h = kMaxEyeHeight;
    if (const char* env = std::getenv("AZAHARFRAME_MODEL_EYE_PX"); env && std::atoi(env) >= 200) {
        max_h = static_cast<uint32_t>(std::atoi(env));
    }
    g.eye_h = std::min(max_h, std::max(200u, static_cast<uint32_t>(rec_h * kEyeScale)));
    g.eye_w = std::max(200u, static_cast<uint32_t>(
                                 static_cast<float>(g.eye_h) * rec_w / std::max(1u, rec_h)));
    XrSwapchainCreateInfo sci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    sci.format = rt.swapchain_format;
    sci.sampleCount = 1;
    sci.width = g.eye_w * 2;
    sci.height = g.eye_h;
    sci.faceCount = 1;
    sci.arraySize = 1;
    sci.mipCount = 1;
    const XrResult r =
        g.queue_lock->With([&] { return xr.xrCreateSwapchain(rt.session, &sci, &g.swapchain); });
    if (XR_FAILED(r)) {
        g.swapchain = XR_NULL_HANDLE;
        throw std::runtime_error(std::string{"xrCreateSwapchain: "} + ResultName(r));
    }
    xr.xrEnumerateSwapchainImages(g.swapchain, 0, &count, nullptr);
    std::vector<XrSwapchainImageVulkan2KHR> list(count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    xr.xrEnumerateSwapchainImages(g.swapchain, count, &count,
                                  reinterpret_cast<XrSwapchainImageBaseHeader*>(list.data()));
    for (const auto& image : list) {
        g.swap_images.push_back(image.image);
    }

    // MSAA where colour and depth both have 4 samples; a depth format that can be attached.
    const auto limits = g.physical.getProperties().limits;
    if ((limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e4) &&
        (limits.framebufferDepthSampleCounts & vk::SampleCountFlagBits::e4)) {
        g.samples = vk::SampleCountFlagBits::e4;
    }
    vk::Format depth = vk::Format::eD32Sfloat;
    if (!(g.physical.getFormatProperties(depth).optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eDepthStencilAttachment)) {
        depth = vk::Format::eD16Unorm;
    }
    const bool msaa = g.samples != vk::SampleCountFlagBits::e1;

    // The render pass: clear to nothing, draw, resolve (MSAA) into the swapchain image.
    std::vector<vk::AttachmentDescription> atts;
    atts.push_back({.format = format,
                    .samples = g.samples,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = msaa ? vk::AttachmentStoreOp::eDontCare : vk::AttachmentStoreOp::eStore,
                    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eUndefined,
                    .finalLayout = vk::ImageLayout::eColorAttachmentOptimal});
    atts.push_back({.format = depth,
                    .samples = g.samples,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eDontCare,
                    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                    .initialLayout = vk::ImageLayout::eUndefined,
                    .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal});
    if (msaa) {
        atts.push_back({.format = format,
                        .samples = vk::SampleCountFlagBits::e1,
                        .loadOp = vk::AttachmentLoadOp::eDontCare,
                        .storeOp = vk::AttachmentStoreOp::eStore,
                        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                        .initialLayout = vk::ImageLayout::eUndefined,
                        .finalLayout = vk::ImageLayout::eColorAttachmentOptimal});
    }
    const vk::AttachmentReference colour_ref{0, vk::ImageLayout::eColorAttachmentOptimal};
    const vk::AttachmentReference depth_ref{1, vk::ImageLayout::eDepthStencilAttachmentOptimal};
    const vk::AttachmentReference resolve_ref{2, vk::ImageLayout::eColorAttachmentOptimal};
    const vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colour_ref,
        .pResolveAttachments = msaa ? &resolve_ref : nullptr,
        .pDepthStencilAttachment = &depth_ref,
    };
    const vk::PipelineStageFlags stages = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                          vk::PipelineStageFlagBits::eEarlyFragmentTests |
                                          vk::PipelineStageFlagBits::eLateFragmentTests;
    const vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = stages,
        .dstStageMask = stages,
        .srcAccessMask = {},
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                         vk::AccessFlagBits::eDepthStencilAttachmentWrite,
    };
    g.render_pass = g.device.createRenderPass({
        .attachmentCount = static_cast<uint32_t>(atts.size()),
        .pAttachments = atts.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    });

    // The targets the pass draws into besides the swapchain image.
    const vk::Extent3D extent{g.eye_w * 2, g.eye_h, 1};
    const vk::ImageUsageFlags transient = vk::ImageUsageFlagBits::eTransientAttachment;
    const vk::Image depth_image = g.Image(
        {.imageType = vk::ImageType::e2D,
         .format = depth,
         .extent = extent,
         .mipLevels = 1,
         .arrayLayers = 1,
         .samples = g.samples,
         .tiling = vk::ImageTiling::eOptimal,
         .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | transient},
        true);
    g.depth_view = g.View(depth_image, depth, vk::ImageAspectFlagBits::eDepth, 1);
    if (msaa) {
        const vk::Image msaa_image = g.Image(
            {.imageType = vk::ImageType::e2D,
             .format = format,
             .extent = extent,
             .mipLevels = 1,
             .arrayLayers = 1,
             .samples = g.samples,
             .tiling = vk::ImageTiling::eOptimal,
             .usage = vk::ImageUsageFlagBits::eColorAttachment | transient},
            true);
        g.msaa_view = g.View(msaa_image, format, vk::ImageAspectFlagBits::eColor, 1);
    }
    for (const VkImage image : g.swap_images) {
        const vk::ImageView v = g.device.createImageView({
            .image = vk::Image{image},
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
        });
        g.swap_views.push_back(v);
        std::vector<vk::ImageView> fb_views;
        if (msaa) {
            fb_views = {g.msaa_view, g.depth_view, v};
        } else {
            fb_views = {v, g.depth_view};
        }
        g.framebuffers.push_back(g.device.createFramebuffer({
            .renderPass = g.render_pass,
            .attachmentCount = static_cast<uint32_t>(fb_views.size()),
            .pAttachments = fb_views.data(),
            .width = extent.width,
            .height = extent.height,
            .layers = 1,
        }));
    }

    // Descriptors: the two textures of a hand.
    const std::array<vk::DescriptorSetLayoutBinding, 2> bindings{{
        {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
    }};
    g.set_layout = g.device.createDescriptorSetLayout(
        {.bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()});
    const vk::DescriptorPoolSize pool_size{vk::DescriptorType::eCombinedImageSampler, 4};
    g.descriptor_pool =
        g.device.createDescriptorPool({.maxSets = 2, .poolSizeCount = 1, .pPoolSizes = &pool_size});
    const vk::PushConstantRange push{
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
        sizeof(PushConstants)};
    g.pipeline_layout = g.device.createPipelineLayout({.setLayoutCount = 1,
                                                       .pSetLayouts = &g.set_layout,
                                                       .pushConstantRangeCount = 1,
                                                       .pPushConstantRanges = &push});

    // The pipeline.
    const vk::ShaderModule vert = Vulkan::CompileSPV(f.vert_spv, g.device);
    const vk::ShaderModule frag =
        Vulkan::CompileSPV(srgb ? f.frag_spv_linear : f.frag_spv, g.device);
    if (!vert || !frag) {
        throw std::runtime_error("shader modules");
    }
    const std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages{{
        {.stage = vk::ShaderStageFlagBits::eVertex, .module = vert, .pName = "main"},
        {.stage = vk::ShaderStageFlagBits::eFragment, .module = frag, .pName = "main"},
    }};
    const vk::VertexInputBindingDescription vb{0, kModelVertexFloats * 4,
                                               vk::VertexInputRate::eVertex};
    const std::array<vk::VertexInputAttributeDescription, 4> va{{
        {0, 0, vk::Format::eR32G32B32Sfloat, 0},
        {1, 0, vk::Format::eR32G32B32Sfloat, 12},
        {2, 0, vk::Format::eR32G32B32A32Sfloat, 24},
        {3, 0, vk::Format::eR32G32Sfloat, 40},
    }};
    const vk::PipelineVertexInputStateCreateInfo vertex_input{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vb,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(va.size()),
        .pVertexAttributeDescriptions = va.data(),
    };
    const vk::PipelineInputAssemblyStateCreateInfo assembly{
        .topology = vk::PrimitiveTopology::eTriangleList};
    const vk::PipelineViewportStateCreateInfo viewport{.viewportCount = 1, .scissorCount = 1};
    const vk::PipelineRasterizationStateCreateInfo raster{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .lineWidth = 1.0f,
    };
    const vk::PipelineMultisampleStateCreateInfo multisample{.rasterizationSamples = g.samples};
    const vk::PipelineDepthStencilStateCreateInfo depth_state{
        .depthTestEnable = true,
        .depthWriteEnable = true,
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
    };
    const vk::PipelineColorBlendAttachmentState blend_att{
        .blendEnable = false,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    const vk::PipelineColorBlendStateCreateInfo blend{.attachmentCount = 1,
                                                      .pAttachments = &blend_att};
    const std::array<vk::DynamicState, 2> dynamic_states{vk::DynamicState::eViewport,
                                                         vk::DynamicState::eScissor};
    const vk::PipelineDynamicStateCreateInfo dynamic{
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data()};
    const auto made = g.device.createGraphicsPipeline({}, {
                                                              .stageCount = 2,
                                                              .pStages = shader_stages.data(),
                                                              .pVertexInputState = &vertex_input,
                                                              .pInputAssemblyState = &assembly,
                                                              .pViewportState = &viewport,
                                                              .pRasterizationState = &raster,
                                                              .pMultisampleState = &multisample,
                                                              .pDepthStencilState = &depth_state,
                                                              .pColorBlendState = &blend,
                                                              .pDynamicState = &dynamic,
                                                              .layout = g.pipeline_layout,
                                                              .renderPass = g.render_pass,
                                                              .subpass = 0,
                                                          });
    g.device.destroyShaderModule(vert);
    g.device.destroyShaderModule(frag);
    if (made.result != vk::Result::eSuccess) {
        throw std::runtime_error("pipeline");
    }
    g.pipeline = made.value;

    g.sampler = g.device.createSampler({
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
        .maxLod = VK_LOD_CLAMP_NONE,
    });

    // Commands and fences.
    g.pool = g.device.createCommandPool({
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = rt.vulkan->QueueFamily(),
    });
    const auto cmds = g.device.allocateCommandBuffers(
        {.commandPool = g.pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 2});
    g.cmd = cmds[0];
    g.upload_cmd = cmds[1];
    g.fence = g.device.createFence({});
    g.upload_fence = g.device.createFence({});

    // ---- The meshes and textures: one staging buffer, one submit, a fence polled later ----
    vk::DeviceSize total = 0;
    for (const auto& m : f.hand) {
        total += m.vertices.size() * 4 + m.indices.size() * 4 + m.colour.size() + m.surface.size();
    }
    g.staging = g.Buffer(total, vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible |
                             vk::MemoryPropertyFlagBits::eHostCoherent,
                         &g.staging_memory);
    auto* map = static_cast<uint8_t*>(g.device.mapMemory(g.staging_memory, 0, total));
    vk::DeviceSize at = 0;
    const auto put = [&](const void* data, std::size_t size) {
        std::memcpy(map + at, data, size);
        const vk::DeviceSize offset = at;
        at += size;
        return offset;
    };
    vk::CommandBuffer& u = g.upload_cmd;
    u.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    const vk::FormatFeatureFlags blit = vk::FormatFeatureFlagBits::eBlitSrc |
                                        vk::FormatFeatureFlagBits::eBlitDst |
                                        vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
    std::vector<vk::DescriptorSetLayout> layouts(2, g.set_layout);
    const auto sets = g.device.allocateDescriptorSets(
        {.descriptorPool = g.descriptor_pool, .descriptorSetCount = 2, .pSetLayouts = layouts.data()});
    for (int h = 0; h < 2; h++) {
        const ModelFile& m = f.hand[static_cast<std::size_t>(h)];
        Gpu::HandGpu& hg = g.hands[static_cast<std::size_t>(h)];
        const auto local = vk::MemoryPropertyFlagBits::eDeviceLocal;
        hg.vertices = g.Buffer(m.vertices.size() * 4,
                               vk::BufferUsageFlagBits::eVertexBuffer |
                                   vk::BufferUsageFlagBits::eTransferDst,
                               local);
        hg.indices = g.Buffer(m.indices.size() * 4,
                              vk::BufferUsageFlagBits::eIndexBuffer |
                                  vk::BufferUsageFlagBits::eTransferDst,
                              local);
        const auto voff = put(m.vertices.data(), m.vertices.size() * 4);
        const auto ioff = put(m.indices.data(), m.indices.size() * 4);
        u.copyBuffer(g.staging, hg.vertices, vk::BufferCopy{voff, 0, m.vertices.size() * 4});
        u.copyBuffer(g.staging, hg.indices, vk::BufferCopy{ioff, 0, m.indices.size() * 4});

        for (int t = 0; t < 2; t++) {
            const vk::Format tf = t == 0 ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;
            const auto& pixels = t == 0 ? m.colour : m.surface;
            uint32_t levels = 1;
            if ((g.physical.getFormatProperties(tf).optimalTilingFeatures & blit) == blit) {
                levels = static_cast<uint32_t>(std::floor(
                             std::log2(static_cast<float>(std::max(m.tex_width, m.tex_height))))) +
                         1;
            }
            const vk::Image image = g.Image(
                {.imageType = vk::ImageType::e2D,
                 .format = tf,
                 .extent = {m.tex_width, m.tex_height, 1},
                 .mipLevels = levels,
                 .arrayLayers = 1,
                 .samples = vk::SampleCountFlagBits::e1,
                 .tiling = vk::ImageTiling::eOptimal,
                 .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                          vk::ImageUsageFlagBits::eTransferSrc},
                false);
            const auto toff = put(pixels.data(), pixels.size());
            const auto barrier = [&](uint32_t level, uint32_t n, vk::ImageLayout from,
                                     vk::ImageLayout to, vk::AccessFlags src, vk::AccessFlags dst,
                                     vk::PipelineStageFlags src_stage,
                                     vk::PipelineStageFlags dst_stage) {
                const vk::ImageMemoryBarrier b{
                    .srcAccessMask = src,
                    .dstAccessMask = dst,
                    .oldLayout = from,
                    .newLayout = to,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = image,
                    .subresourceRange = {vk::ImageAspectFlagBits::eColor, level, n, 0, 1},
                };
                u.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, b);
            };
            const auto transfer = vk::PipelineStageFlagBits::eTransfer;
            barrier(0, levels, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, {},
                    vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eTopOfPipe,
                    transfer);
            u.copyBufferToImage(
                g.staging, image, vk::ImageLayout::eTransferDstOptimal,
                vk::BufferImageCopy{toff, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                    {0, 0, 0}, {m.tex_width, m.tex_height, 1}});
            int32_t w = static_cast<int32_t>(m.tex_width), hgt = static_cast<int32_t>(m.tex_height);
            for (uint32_t level = 1; level < levels; level++) {
                barrier(level - 1, 1, vk::ImageLayout::eTransferDstOptimal,
                        vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits::eTransferWrite,
                        vk::AccessFlagBits::eTransferRead, transfer, transfer);
                const int32_t nw = std::max(1, w / 2), nh = std::max(1, hgt / 2);
                const vk::ImageBlit region{
                    .srcSubresource = {vk::ImageAspectFlagBits::eColor, level - 1, 0, 1},
                    .srcOffsets = {{vk::Offset3D{0, 0, 0}, vk::Offset3D{w, hgt, 1}}},
                    .dstSubresource = {vk::ImageAspectFlagBits::eColor, level, 0, 1},
                    .dstOffsets = {{vk::Offset3D{0, 0, 0}, vk::Offset3D{nw, nh, 1}}},
                };
                u.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image,
                            vk::ImageLayout::eTransferDstOptimal, region, vk::Filter::eLinear);
                w = nw;
                hgt = nh;
            }
            const auto shader = vk::PipelineStageFlagBits::eFragmentShader;
            if (levels > 1) {
                barrier(0, levels - 1, vk::ImageLayout::eTransferSrcOptimal,
                        vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eTransferRead,
                        vk::AccessFlagBits::eShaderRead, transfer, shader);
            }
            barrier(levels - 1, 1, vk::ImageLayout::eTransferDstOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eShaderRead, transfer, shader);
            (t == 0 ? hg.colour : hg.surface) =
                g.View(image, tf, vk::ImageAspectFlagBits::eColor, levels);
        }
        hg.set = sets[static_cast<std::size_t>(h)];
        const std::array<vk::DescriptorImageInfo, 2> infos{{
            {g.sampler, hg.colour, vk::ImageLayout::eShaderReadOnlyOptimal},
            {g.sampler, hg.surface, vk::ImageLayout::eShaderReadOnlyOptimal},
        }};
        const std::array<vk::WriteDescriptorSet, 2> writes{{
            {.dstSet = hg.set,
             .dstBinding = 0,
             .descriptorCount = 1,
             .descriptorType = vk::DescriptorType::eCombinedImageSampler,
             .pImageInfo = &infos[0]},
            {.dstSet = hg.set,
             .dstBinding = 1,
             .descriptorCount = 1,
             .descriptorType = vk::DescriptorType::eCombinedImageSampler,
             .pImageInfo = &infos[1]},
        }};
        g.device.updateDescriptorSets(writes, {});
    }
    const vk::MemoryBarrier to_draws{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead | vk::AccessFlagBits::eIndexRead,
    };
    u.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput,
                      {}, to_draws, {}, {});
    u.end();
    g.device.unmapMemory(g.staging_memory);
    const vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &u};
    g.queue_lock->With([&] {
        g.queue.submit(submit, g.upload_fence);
        return 0;
    });
    g.state = 1;
    LOG_INFO(Frontend,
             "xr: controller models: {}x{} per eye, {}x MSAA, {} MB uploading; one swapchain",
             g.eye_w, g.eye_h, static_cast<int>(g.samples), total >> 20);
}

void ControllerModels::Record(const ViewPose& views, const std::array<Hand, 2>& hands) {
    Gpu& g = *gpu;
    const Files& f = *files;
    vk::CommandBuffer& c = g.cmd;
    c.reset();
    c.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    std::array<vk::ClearValue, 3> clears{}; // colour: zero, nothing round the models
    clears[1].depthStencil = vk::ClearDepthStencilValue{1.0f, 0};
    c.beginRenderPass({.renderPass = g.render_pass,
                       .framebuffer = g.framebuffers[g.index],
                       .renderArea = {{0, 0}, {g.eye_w * 2, g.eye_h}},
                       .clearValueCount = static_cast<uint32_t>(clears.size()),
                       .pClearValues = clears.data()},
                      vk::SubpassContents::eInline);
    c.bindPipeline(vk::PipelineBindPoint::eGraphics, g.pipeline);
    const Mat head = FromPose(views.head.orientation, views.head.position);
    for (int e = 0; e < 2; e++) {
        const vk::Viewport vp{static_cast<float>(g.eye_w * e), 0.0f, static_cast<float>(g.eye_w),
                              static_cast<float>(g.eye_h), 0.0f, 1.0f};
        const vk::Rect2D sc{{static_cast<int32_t>(g.eye_w * e), 0}, {g.eye_w, g.eye_h}};
        c.setViewport(0, vp);
        c.setScissor(0, sc);
        const Mat view_proj =
            Multiply(Projection(views.fov[e]),
                     InverseRigid(FromPose(views.eye[e].orientation, views.eye[e].position)));
        for (int h = 0; h < 2; h++) {
            const Hand& hand = hands[static_cast<std::size_t>(h)];
            if (!hand.shown) {
                continue;
            }
            const ModelFile& m = f.hand[static_cast<std::size_t>(h)];
            const Gpu::HandGpu& hg = g.hands[static_cast<std::size_t>(h)];
            const XrPosef aim = ToXr(hand.aim);
            const Mat model = FromPose(aim.orientation, aim.position);
            const Mat to_model = InverseRigid(model);
            PushConstants pc{};
            const auto eye = Apply(to_model, views.eye[e].position.x, views.eye[e].position.y,
                                   views.eye[e].position.z, 1.0f);
            pc.eye = {eye[0], eye[1], eye[2], 1.0f};
            // The key light from above and behind the head (world), then into model space.
            const auto back = Apply(head, 0.0f, 0.0f, 1.0f, 0.0f);
            const float kx = back[0] * 0.6f, ky = 1.0f + back[1] * 0.6f, kz = back[2] * 0.6f;
            const auto key = Apply(to_model, kx, ky, kz, 0.0f);
            pc.key = {key[0], key[1], key[2], 0.0f};
            const auto up = Apply(to_model, 0.0f, 1.0f, 0.0f, 0.0f);
            pc.up = {up[0], up[1], up[2], 0.0f};
            pc.params = {0.0f, 1.0f, std::clamp(hand.fade, 0.0f, 1.0f), 0.0f};
            c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, g.pipeline_layout, 0, hg.set, {});
            c.bindVertexBuffers(0, hg.vertices, vk::DeviceSize{0});
            c.bindIndexBuffer(hg.indices, 0, vk::IndexType::eUint32);
            const Mat mvp_model = Multiply(view_proj, model);
            for (const ModelPart& part : PoseModel(m, hand.input)) {
                pc.mvp = Multiply(mvp_model, part.transform);
                c.pushConstants(g.pipeline_layout,
                                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                                0, sizeof(pc), &pc);
                c.drawIndexed(part.index_count, 1, part.first_index, 0, 0);
            }
        }
    }
    c.endRenderPass();
    c.end();
}

} // namespace FrameXr::Vis
