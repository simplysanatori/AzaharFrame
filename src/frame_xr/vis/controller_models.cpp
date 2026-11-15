// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// The pass follows Moonlight XR Native's xr/xr_model.c (renderControllerModels, modelRecord;
// GPLv3, same author), as a Vulkan pass of its own.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <map>
#include <stdexcept>
#include <string>

#include "common/file_util.h"
#include "common/frame_test_switches.h" // AzaharFrame kiuguns
#include "common/logging/log.h"
#include "common/settings.h" // AzaharFrame kiufix: Weapon Size
#include "frame_xr/retired_swapchains.h" // AzaharFrame projdef
#include "frame_xr/session.h"
#include "frame_xr/vis/controller_models.h"
#include "frame_xr/vis/model_shaders.h"
#include "frame_xr/vis/proj_screen.h" // AzaharFrame projscreen
#include "frame_xr/vis/weapon_fit.h" // AzaharFrame kiuguns
#include "core/frame_weapon_models.h" // AzaharFrame kiuguns
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

// AzaharFrame kiuguns: the inverse of an affine transform (a weapon's has a scale, maybe a mirror).
Mat InverseAffine(const Mat& m) {
    const double a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9], g = m[2], h = m[6],
                 i = m[10];
    const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (std::fabs(det) < 1e-12) {
        return InverseRigid(m);
    }
    // Row major inverse of the 3x3 [a b c; d e f; g h i].
    const double inv[9] = {(e * i - f * h) / det, (c * h - b * i) / det, (b * f - c * e) / det,
                           (f * g - d * i) / det, (a * i - c * g) / det, (c * d - a * f) / det,
                           (d * h - e * g) / det, (b * g - a * h) / det, (a * e - b * d) / det};
    Mat r{};
    for (int row = 0; row < 3; row++) {
        double t = 0.0;
        for (int col = 0; col < 3; col++) {
            r[col * 4 + row] = static_cast<float>(inv[row * 3 + col]);
            t -= inv[row * 3 + col] * m[12 + col];
        }
        r[12 + row] = static_cast<float>(t);
    }
    r[15] = 1.0f;
    return r;
}

// AzaharFrame projscreen: the screens may be further than kFar (World Scale up to 400 %: 12 m).
constexpr float kFarScreens = 100.0f;

// OpenGL style (y up, z -1..1); the vertex shader turns it into Vulkan's clip space.
Mat Projection(const XrFovf& fov, float far_z = kFar) {
    const float kFar = far_z;
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

Input::Posef FromXr(const XrPosef& p) { // AzaharFrame ctrlfront
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

// AzaharFrame ctrlfront: how often the pass's GPU time is logged while the models show.
constexpr int64_t kGpuLogNs = 10'000'000'000;

struct PushConstants {
    Mat mvp;
    std::array<float, 4> eye;
    std::array<float, 4> key;
    std::array<float, 4> up;
    std::array<float, 4> params;
};
static_assert(sizeof(PushConstants) == 128);

// AzaharFrame projscreen: the screen's push constants (proj_screen.h kScreenVert).
struct ScreenPushConstants {
    Mat mvp;
    std::array<float, 4> shape;
    std::array<float, 4> rect;
    std::array<float, 4> tex;
    std::array<float, 4> extra;
};
static_assert(sizeof(ScreenPushConstants) == 128);

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
    std::vector<uint32_t> occluder_vert_spv; ///< AzaharFrame ctrlfront (empty: no occluders)
    /// AzaharFrame ctrlfront: each hand's model inside a sphere (aim space centre, radius).
    std::array<ModelSphere, 2> sphere{};
    std::vector<uint32_t> screen_vert_spv, screen_frag_spv; ///< AzaharFrame projscreen
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
    vk::Pipeline pipeline_add; ///< AzaharFrame kiuguns: glows: added, no depth written
    vk::Pipeline pipeline_occluder; ///< AzaharFrame ctrlfront: depth only (null: none)
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
    // AzaharFrame ctrlfront: the eye quads' distance for the image in flight and the one shown
    // (model_plane.h ModelPlaneDistance), and the occluders the one in flight was drawn with.
    float drawn_plane = kModelPlaneDefault;
    float shown_plane = kModelPlaneDefault;
    std::size_t drawn_occluders = 0;
    // AzaharFrame ctrlglitch: the eye quads' world-locked canvases (model_plane.h), the last few
    // by number; the canvas the image in flight and the one released last were drawn for (0:
    // drawn for the eyes' own views); the move between canvases; this refresh's step.
    struct Canvas {
        uint32_t id = 0;
        std::array<EyePlane, 2> eye{};
    };
    std::array<Canvas, 4> canvases{};
    uint32_t next_canvas = 1;
    uint32_t drawn_canvas = 0;
    uint32_t shown_canvas = 0;
    CanvasHandover handover;
    CanvasFrame step;
    bool canvas_on = false; ///< this refresh's quads are on a canvas (AZAHARFRAME_XR_CANVAS)
    uint32_t canvas_moves = 0, canvas_projection = 0; ///< for the log
    const Canvas* FindCanvas(uint32_t id) const {
        for (const Canvas& c : canvases) {
            if (id != 0 && c.id == id) {
                return &c;
            }
        }
        return nullptr;
    }
    // AzaharFrame ctrlfront: the pass's GPU time: two timestamps around it (null pool: the queue
    // has none), summed for the log.
    vk::QueryPool queries;
    double timestamp_ns = 1.0;
    uint64_t timestamp_mask = ~0ull;
    double gpu_sum_ms = 0.0, gpu_max_ms = 0.0;
    uint32_t gpu_images = 0;
    uint64_t occluder_sum = 0;
    int64_t gpu_log_ns = 0;

    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    std::array<XrCompositionLayerProjectionView, 2> layer_views{
        {{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};

    vk::PhysicalDevice physical;

    // ---- AzaharFrame projscreen: the top screen drawn first (screens mode) ----
    bool screens = false; ///< made for screens mode (full size, no MSAA by default)
    float scale = 1.0f;
    vk::DescriptorSetLayout screen_set_layout;
    vk::DescriptorPool screen_pool;
    std::array<vk::DescriptorSet, 2> screen_sets{};
    vk::PipelineLayout screen_layout;
    vk::Pipeline screen_pipeline;
    vk::Sampler screen_sampler;
    vk::Format screen_format = vk::Format::eUndefined;
    std::vector<vk::ImageView> screen_views; ///< made for the draw in flight; destroyed after it
    /// The pass's GPU time each second (ctrlfront's timestamps): screens mode, or
    /// AZAHARFRAME_XR_MODELS_GPU_TIME=1 for the default pass.
    bool time_log = false;
    bool drawn_screen = false, shown_screen = false;
    uint64_t drawn_serial = 0, shown_serial = 0;
    // Per second, for the log line.
    std::chrono::steady_clock::time_point stats_since{};
    uint32_t stat_draws = 0, stat_screen_draws = 0, stat_busy = 0, stat_layers = 0, stat_gpu_n = 0;
    double stat_gpu_sum = 0.0, stat_gpu_max = 0.0;
    uint32_t gap_held = 0; ///< projb: refreshes the last image was kept over a swapchain change
    uint32_t stat_gap = 0;

    void DestroyScreenViews() {
        for (auto v : screen_views) {
            device.destroyImageView(v);
        }
        screen_views.clear();
    }

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

    vk::Image Image(const vk::ImageCreateInfo& info, bool lazy,
                    std::vector<std::pair<vk::Image, vk::DeviceMemory>>* list = nullptr) {
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
        (list ? *list : owned_images).emplace_back(image, m);
        return image;
    }

    vk::ImageView View(vk::Image image, vk::Format format, vk::ImageAspectFlags aspect,
                       uint32_t levels, std::vector<vk::ImageView>* list = nullptr) {
        const vk::ImageView v = device.createImageView({
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {aspect, 0, levels, 0, 1},
        });
        (list ? *list : views).push_back(v);
        return v;
    }

    /// A texture from `pixels` (RGBA8) put in the staging buffer at `offset`: its mip chain made
    /// by blits where the format allows, ready to sample when `u` has run.
    vk::ImageView UploadImage(vk::CommandBuffer u, vk::DeviceSize offset, uint32_t width,
                              uint32_t height, vk::Format tf,
                              std::vector<std::pair<vk::Image, vk::DeviceMemory>>* images,
                              std::vector<vk::ImageView>* view_list) {
        const vk::FormatFeatureFlags blit = vk::FormatFeatureFlagBits::eBlitSrc |
                                            vk::FormatFeatureFlagBits::eBlitDst |
                                            vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
        uint32_t levels = 1;
        if ((physical.getFormatProperties(tf).optimalTilingFeatures & blit) == blit) {
            levels = static_cast<uint32_t>(
                         std::floor(std::log2(static_cast<float>(std::max(width, height))))) +
                     1;
        }
        const vk::Image image = Image(
            {.imageType = vk::ImageType::e2D,
             .format = tf,
             .extent = {width, height, 1},
             .mipLevels = levels,
             .arrayLayers = 1,
             .samples = vk::SampleCountFlagBits::e1,
             .tiling = vk::ImageTiling::eOptimal,
             .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst |
                      vk::ImageUsageFlagBits::eTransferSrc},
            false, images);
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
        u.copyBufferToImage(staging_for_upload, image, vk::ImageLayout::eTransferDstOptimal,
                            vk::BufferImageCopy{offset, 0, 0,
                                                {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                                {0, 0, 0}, {width, height, 1}});
        int32_t w = static_cast<int32_t>(width), hgt = static_cast<int32_t>(height);
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
        return View(image, tf, vk::ImageAspectFlagBits::eColor, levels, view_list);
    }
    vk::Buffer staging_for_upload; ///< UploadImage's source

    // ---- AzaharFrame kiuguns: the weapon in hand ----
    struct WeaponPart {
        vk::Buffer vertices, indices;
        vk::DescriptorSet set;
        std::vector<ModelComponent> components;
        bool glow = false;
        /// AzaharFrame kiuflash: a muzzle flash's cards (components), and each card's extent.
        bool flash = false;
        std::vector<FlashCardExtent> card_size;
        FrameWeapons::UvMotion uv; ///< AzaharFrame kiumodels: how its texture moves
    };
    struct Weapon {
        int id = -1;
        uint32_t generation = 0;
        FrameWeapons::Weapon info;
        std::vector<WeaponPart> parts;
        std::vector<std::pair<vk::Buffer, vk::DeviceMemory>> buffers;
        std::vector<std::pair<vk::Image, vk::DeviceMemory>> images;
        std::vector<vk::ImageView> views;
        std::vector<vk::DescriptorSet> sets;
        uint32_t triangles = 0;
        uint64_t gpu_bytes = 0; ///< buffers and images (the mip chains' too)
        uint64_t flash_bytes = 0; ///< AzaharFrame kiuflash: of them, the flash cards'
        float flash_push = 0.0f;  ///< AzaharFrame kiuflash: FlashPush, the class's own fit
        int pack_textures = 0;
    };
    std::unique_ptr<Weapon> weapon_live, weapon_uploading;
    std::vector<std::unique_ptr<Weapon>> weapon_retired;
    vk::DescriptorPool weapon_pool;
    vk::CommandBuffer weapon_cmd;
    vk::Fence weapon_fence;
    vk::Buffer weapon_staging;
    vk::DeviceMemory weapon_staging_memory;
    std::map<uint32_t, vk::Sampler> weapon_samplers; ///< by wrap modes (u | v << 4)
    int weapon_failed_id = -1;
    uint32_t weapon_failed_generation = 0;
    std::string weapon_fit_logged;

    void FreeWeapon(Weapon& w) {
        if (!w.sets.empty()) {
            device.freeDescriptorSets(weapon_pool, w.sets);
        }
        for (auto v : w.views) {
            device.destroyImageView(v);
        }
        for (auto& [image, memory] : w.images) {
            device.destroyImage(image);
            device.freeMemory(memory);
        }
        for (auto& [buffer, memory] : w.buffers) {
            device.destroyBuffer(buffer);
            device.freeMemory(memory);
        }
        w = {};
    }
};

struct ControllerModels::WeaponLoad { // AzaharFrame kiuguns
    int id = -1;
    uint32_t generation = 0;
    bool ok = false;
    std::string error;
    FrameWeapons::Weapon info;
    std::vector<ModelFile> files;
    std::size_t part_files = 0; ///< AzaharFrame kiuflash: the parts' (the flash cards' after)
    float flash_push = 0.0f;    ///< AzaharFrame kiuflash: FlashPush (metres, the class's fit)
    int pack_textures = 0; ///< colour images taken from the texture pack
};

ControllerModels::ControllerModels() = default;

ControllerModels::~ControllerModels() {
    DestroyXr();
    if (loader.joinable()) {
        loader.join();
    }
    if (weapon_loader.joinable()) { // AzaharFrame kiuguns
        weapon_loader.join();
    }
}

// AzaharFrame projscreen: in screens mode the image is always the projection layer (the top
// screen is in it; as eye quads it would be an overlay again), whatever the in-front choice.
bool ControllerModels::AsEyeQuads() const {
    return ModelsAsQuads(over_) && !(gpu && gpu->screens);
}

const XrCompositionLayerBaseHeader* ControllerModels::Layer() const {
    // AzaharFrame ctrlglitch: with the quads, too, while their canvas moves (model_plane.h).
    // test3d: projscreen's AsEyeQuads first: in screens mode always the projection layer.
    const bool wanted = !AsEyeQuads() || (gpu && gpu->canvas_on && gpu->step.projection);
    return layer_ready && wanted
               ? reinterpret_cast<const XrCompositionLayerBaseHeader*>(&gpu->layer)
               : nullptr;
}

// AzaharFrame ctrlfront: the image released last as each eye's quad: the eye's view at the
// distance it was drawn for, so it shows just as the projection layer would (model_plane.h).
bool ControllerModels::EyeQuads(std::array<Quad, 2>& out) const {
    if (!layer_ready || !AsEyeQuads()) {
        return false;
    }
    const Gpu& g = *gpu;
    // AzaharFrame ctrlglitch: the canvas's place in the room, which changes only through the
    // handover (model_plane.h), shrunk to nothing in place while it is not shown.
    if (g.canvas_on) {
        const Gpu::Canvas* canvas = g.FindCanvas(g.step.canvas);
        if (!canvas) {
            return false;
        }
        for (std::size_t e = 0; e < 2; e++) {
            const EyePlane& plane = canvas->eye[e];
            const float k = g.step.quads_shown ? 1.0f : kCanvasShrink;
            Quad& q = out[e];
            q.swapchain = g.swapchain;
            q.image_rect = {{static_cast<int32_t>(g.eye_w * e), 0},
                            {static_cast<int32_t>(g.eye_w), static_cast<int32_t>(g.eye_h)}};
            q.pose = ToXr(plane.pose);
            q.size = {plane.width_m * k, plane.height_m * k};
            q.eye = e == 0 ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
            q.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        }
        return true;
    }
    for (int e = 0; e < 2; e++) {
        const XrFovf& fov = g.shown_views.fov[e];
        const EyePlane plane =
            PlaneForEye(FromXr(g.shown_views.eye[e]), fov.angleLeft, fov.angleRight, fov.angleUp,
                        fov.angleDown, g.shown_plane);
        Quad& q = out[static_cast<std::size_t>(e)];
        q.swapchain = g.swapchain;
        q.image_rect = g.layer_views[static_cast<std::size_t>(e)].subImage.imageRect;
        q.pose = ToXr(plane.pose);
        q.size = {plane.width_m, plane.height_m};
        q.eye = e == 0 ? XR_EYE_VISIBILITY_LEFT : XR_EYE_VISIBILITY_RIGHT;
        q.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    }
    return true;
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
        // AzaharFrame kiuguns: the weapon in hand (an upload under way finishes first).
        if (g.weapon_uploading) {
            static_cast<void>(g.device.waitForFences(g.weapon_fence, true, 1'000'000'000));
        }
        for (auto* w : {g.weapon_live.get(), g.weapon_uploading.get()}) {
            if (w) {
                g.FreeWeapon(*w);
            }
        }
        for (auto& w : g.weapon_retired) {
            g.FreeWeapon(*w);
        }
        if (g.weapon_staging) {
            g.device.destroyBuffer(g.weapon_staging);
            g.device.freeMemory(g.weapon_staging_memory);
        }
        for (auto& [key, sampler] : g.weapon_samplers) {
            g.device.destroySampler(sampler);
        }
        if (g.weapon_pool) {
            g.device.destroyDescriptorPool(g.weapon_pool);
        }
        if (g.weapon_fence) {
            g.device.destroyFence(g.weapon_fence);
        }
        // AzaharFrame projscreen
        g.DestroyScreenViews();
        if (g.screen_pipeline) {
            g.device.destroyPipeline(g.screen_pipeline);
        }
        if (g.screen_layout) {
            g.device.destroyPipelineLayout(g.screen_layout);
        }
        if (g.screen_pool) {
            g.device.destroyDescriptorPool(g.screen_pool);
        }
        if (g.screen_set_layout) {
            g.device.destroyDescriptorSetLayout(g.screen_set_layout);
        }
        if (g.screen_sampler) {
            g.device.destroySampler(g.screen_sampler);
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
        if (g.pipeline_add) {
            g.device.destroyPipeline(g.pipeline_add);
        }
        if (g.pipeline_occluder) { // AzaharFrame ctrlfront
            g.device.destroyPipeline(g.pipeline_occluder);
        }
        if (g.queries) {
            g.device.destroyQueryPool(g.queries);
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
        // AzaharFrame projdef: its eye quads' slots go before the next frame's list
        // (retired_swapchains.h): a live switch to the projection layer sent the dead handle.
        RetiredSwapchains::Note(reinterpret_cast<uint64_t>(g.swapchain));
    }
    gpu.reset();
}

namespace {

// Reads both hands (the loader thread).
bool ReadFiles(std::array<ModelFile, 2>& files) {
    std::string error;
    if (!ReadControllerModels(files, error)) {
        LOG_WARNING(Frontend, "xr: controller models: {} (only the beams are drawn; "
                              "azahar --frame-controller-models makes them)",
                    error);
        return false;
    }
    return true;
}

} // namespace

void ControllerModels::Frame(Runtime& rt, const ViewPose& views, const std::array<Hand, 2>& hands,
                             ControllersInFront choice, const std::vector<Occluder>& occluders) {
    layer_ready = false;
    over_ = choice; // AzaharFrame ctrlfront
    muzzle_hand = -1; // AzaharFrame kiuguns: FitWeapon sets it again
    // AzaharFrame projscreen: in screens mode the pass is wanted for the screen too.
    const bool any = hands[0].shown || hands[1].shown || screens_mode;
    if (gpu && (gpu->screens != screens_mode || (screens_mode && gpu->scale != screens_scale))) {
        LOG_INFO(Frontend, "xr: projection screens {} (scale {:.2f}): the models' pass is made again",
                 screens_mode ? "on" : "off", screens_scale);
        DestroyXr();
    }

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
            // AzaharFrame ctrlfront: without it the models go over every screen and panel.
            f.occluder_vert_spv = Vulkan::CompileGLSL(
                std::string{"#version 450\n"} + Shaders::kOccluderVert,
                vk::ShaderStageFlagBits::eVertex);
            if (f.occluder_vert_spv.empty()) {
                LOG_WARNING(Frontend, "xr: controller models: the occluder shader did not compile; "
                                      "the controllers go over every screen");
            }
            for (std::size_t h = 0; h < 2; h++) { // AzaharFrame ctrlfront
                f.sphere[h] = ModelBoundingSphere(f.hand[h].vertices, kModelVertexFloats,
                                                  kMovingPartsMargin);
            }
            // AzaharFrame projscreen: the screen's shaders (used in screens mode only).
            f.screen_vert_spv = Vulkan::CompileGLSL(
                std::string{"#version 450\n"} + Shaders::kScreenVert, vk::ShaderStageFlagBits::eVertex);
            f.screen_frag_spv = Vulkan::CompileGLSL(std::string{"#version 450\n"} +
                                                        Shaders::kScreenFrag,
                                                    vk::ShaderStageFlagBits::eFragment);
            if (f.screen_vert_spv.empty() || f.screen_frag_spv.empty()) {
                LOG_ERROR(Frontend, "xr: projection screens: the shaders did not compile");
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
    TryRelease();
    // AzaharFrame kiuguns: the weapon in hand; a weapon swapped out is freed once no image
    // drawn with it is in flight.
    if (!g.in_flight && !g.weapon_retired.empty()) {
        for (auto& w : g.weapon_retired) {
            g.FreeWeapon(*w);
        }
        g.weapon_retired.clear();
    }
    {
        int weapon = -1;
        for (const Hand& hand : hands) {
            if (hand.shown && hand.weapon >= 0) {
                weapon = hand.weapon;
            }
        }
        UpdateWeapon(weapon);
        FitWeapon(views, hands);
    }
    if (!any) {
        g.shown_fresh = false; // when they come back, a new image first
        g.handover.Absent();   // AzaharFrame ctrlglitch: no quads go out
        g.canvas_on = false;
        return;
    }
    if (screens_mode) {
        // AzaharFrame projscreen: drawn by FrameScreens, once the screens are placed.
        // test3d: no eye quads in screens mode, so no canvas (ctrlglitch) and none going out.
        g.canvas_on = false;
        g.handover.Absent();
        pending_views = views;
        pending_hands = hands;
        return;
    }
    // AzaharFrame ctrlglitch: the quads' canvas (model_plane.h): moved when it no longer serves
    // the view, through the handover; then this refresh's step. AZAHARFRAME_XR_CANVAS=0 (live)
    // puts the quads at each image's eye views again, as before, for an A/B.
    const bool canvas_wanted =
        ModelsAsQuads(choice) &&
        Common::FrameTestSwitches::Value("AZAHARFRAME_XR_CANVAS").value_or("1") != "0";
    const bool canvas_was_on = g.canvas_on;
    g.canvas_on = canvas_wanted;
    if (!g.canvas_on) {
        // The quads at the eye views (the A/B) are on view elsewhere; Off has none.
        ModelsAsQuads(choice) ? g.handover.Elsewhere() : g.handover.Absent();
    } else {
        // Back from the A/B: a new canvas, through the handover.
        bool serves = g.handover.Canvas() != 0 && canvas_was_on;
        const Gpu::Canvas* current = g.FindCanvas(g.handover.Canvas());
        const std::array<ModelSphere, 2> spheres = Spheres(hands);
        const std::size_t n = SphereCount(hands);
        for (int e = 0; e < 2 && serves; e++) {
            const XrFovf& fov = views.fov[e];
            const Input::Posef eye = FromXr(views.eye[e]);
            const auto models = ModelsViewBounds(eye, fov.angleLeft, fov.angleRight, fov.angleUp,
                                                 fov.angleDown, spheres.data(), n, kNear);
            serves = current && CanvasServes(current->eye[static_cast<std::size_t>(e)], eye,
                                              fov.angleLeft, fov.angleRight, fov.angleUp,
                                              fov.angleDown, models);
        }
        if (views.valid && (g.handover.Canvas() == 0 || (!serves && !g.handover.Busy()))) {
            std::array<std::optional<Input::Vec3f>, 2> at{};
            for (std::size_t h = 0; h < hands.size(); h++) {
                if (hands[h].shown) {
                    at[h] = hands[h].aim.position;
                }
            }
            const float distance = ModelPlaneDistance(FromXr(views.head), at);
            Gpu::Canvas& c = g.canvases[g.next_canvas % g.canvases.size()];
            c.id = g.next_canvas++;
            for (std::size_t e = 0; e < 2; e++) {
                const XrFovf& fov = views.fov[e];
                c.eye[e] = PlaceCanvas(FromXr(views.eye[e]), fov.angleLeft, fov.angleRight,
                                       fov.angleUp, fov.angleDown, distance);
            }
            g.handover.Request(c.id);
            g.canvas_moves++;
        }
        g.step = g.handover.Step(g.has_image && g.shown_fresh, g.shown_canvas);
        if (g.step.projection) {
            g.canvas_projection++;
        }
    }

    // AzaharFrame ctrlglitch: the views the next image is drawn with: onto the quads' canvas,
    // seen from where the eyes are now (CanvasView); the eyes' own views without one (Off). With
    // a canvas the eye is not in front of, none is drawn (the canvas is about to move).
    std::optional<ViewPose> draw_views;
    uint32_t draw_canvas = 0;
    if (!g.canvas_on) {
        draw_views = views;
    } else if (const Gpu::Canvas* c = g.FindCanvas(g.step.canvas); c && views.valid) {
        std::array<std::optional<CanvasEye>, 2> ce{};
        for (std::size_t e = 0; e < 2; e++) {
            const XrVector3f& p = views.eye[e].position;
            ce[e] = CanvasView(c->eye[e], {p.x, p.y, p.z});
        }
        if (ce[0] && ce[1]) {
            draw_views = views;
            for (std::size_t e = 0; e < 2; e++) {
                draw_views->eye[e] = ToXr(ce[e]->pose);
                draw_views->fov[e] = {ce[e]->angle_left, ce[e]->angle_right, ce[e]->angle_up,
                                      ce[e]->angle_down};
            }
            draw_canvas = c->id;
        }
    }

    // ---- Draw the next one ----
    if (!g.in_flight && views.valid && draw_views) {
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
            // AzaharFrame ctrlfront: the screens' and panels' depth while the image goes out as
            // the eye quads (and with the shader); the eye quads at the nearer controller's depth.
            static const std::vector<Occluder> kNone;
            const std::vector<Occluder>& occ =
                ModelsAsQuads(choice) && g.pipeline_occluder ? occluders : kNone;
            std::array<std::optional<Input::Vec3f>, 2> at{};
            for (std::size_t h = 0; h < hands.size(); h++) {
                if (hands[h].shown) {
                    at[h] = hands[h].aim.position;
                }
            }
            g.drawn_plane = ModelPlaneDistance(FromXr(views.head), at);
            g.drawn_occluders = occ.size();
            g.drawn_canvas = draw_canvas; // AzaharFrame ctrlglitch
            Record(*draw_views, hands, occ);
            const vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &g.cmd};
            g.queue_lock->With([&] {
                g.queue.submit(submit, g.fence);
                return 0;
            });
            g.in_flight = true;
            g.drawn_views = *draw_views; // AzaharFrame ctrlglitch: the views it was drawn with
        }
    }

    // AzaharFrame projscreen: AZAHARFRAME_XR_MODELS_GPU_TIME=1, the models' pass alone, timed.
    if (g.time_log) {
        const auto now = std::chrono::steady_clock::now();
        if (g.stats_since == std::chrono::steady_clock::time_point{}) {
            g.stats_since = now;
        }
        if (std::chrono::duration<double>(now - g.stats_since).count() >= 1.0) {
            LOG_INFO(Frontend, "xr: controller models: GPU {:.2f} ms a draw (max {:.2f}, {} timed; "
                               "{}x{} per eye, {}x MSAA)",
                     g.stat_gpu_n ? g.stat_gpu_sum / g.stat_gpu_n : 0.0, g.stat_gpu_max,
                     g.stat_gpu_n, g.eye_w, g.eye_h, static_cast<int>(g.samples));
            g.stats_since = now;
            g.stat_gpu_n = 0;
            g.stat_gpu_sum = g.stat_gpu_max = 0.0;
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

void ControllerModels::TryRelease() {
    Gpu& g = *gpu;
    if (!g.in_flight || g.device.getFenceStatus(g.fence) != vk::Result::eSuccess) {
        return;
    }
    g.device.resetFences(g.fence);
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const XrResult r =
        g.queue_lock->With([&] { return Xr().xrReleaseSwapchainImage(g.swapchain, &ri); });
    g.in_flight = false;
    if (XR_SUCCEEDED(r)) {
        g.has_image = true;
        g.shown_views = g.drawn_views;
        g.shown_fresh = true;
        g.shown_plane = g.drawn_plane; // AzaharFrame ctrlfront
        g.shown_canvas = g.drawn_canvas; // AzaharFrame ctrlglitch
        g.shown_screen = g.drawn_screen; // AzaharFrame projscreen
        g.shown_serial = g.drawn_serial;
    }
    // AzaharFrame projscreen: the views made for the draw that is done; ctrlfront: its GPU time
    // (no wait: the fence has signalled).
    g.DestroyScreenViews();
    NoteGpuTime(std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
}

void ControllerModels::SetScreensMode(bool on, float scale) {
    screens_mode = on;
    screens_scale = scale;
}

ProjLayer ControllerModels::FrameScreens(Runtime& rt, const ProjScreens& screens) {
    ProjLayer out;
    layer_ready = false;
    if (!screens_mode || !gpu || gpu->state != 2 || !gpu->screens || !gpu->screen_pipeline) {
        return out;
    }
    Gpu& g = *gpu;
    auto& xr = Xr();
    TryRelease(); // the image drawn on the last refresh is usually done by now
    const auto now = std::chrono::steady_clock::now();
    if (g.stats_since == std::chrono::steady_clock::time_point{}) {
        g.stats_since = now;
    }
    const bool hands = pending_hands[0].shown || pending_hands[1].shown;
    // projb: the top screen's swapchain was made again and has no picture yet: the last image
    // with the screen stays (at its own views, so it stays put in the room) for a few refreshes,
    // rather than an image without the screen.
    const bool hold = screens.gap && !screens.Any() && g.has_image && g.shown_fresh &&
                      g.shown_screen && g.gap_held < kGapHoldRefreshes;
    if (hold) {
        g.gap_held++;
        g.stat_gap++;
    } else if (!screens.gap) {
        g.gap_held = 0;
    }
    if (!hold && !hands && !screens.Any()) {
        g.shown_fresh = false;
    } else {
        // ---- Draw this refresh's image: the screen, then the models ----
        if (hold) {
            // nothing new drawn: the layer below shows the last image again
        } else if (g.in_flight) {
            g.stat_busy++; // the layer shows the last image again, at its own views
        } else if (pending_views.valid && pending_views.views_valid) {
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
                // No occluders: the top screen's own depth is in the image, and every quad
                // (the bottom screen, panels) goes over the projection layer anyway.
                static const std::vector<Occluder> kNoOccluders;
                g.drawn_occluders = 0;
                Record(pending_views, pending_hands, kNoOccluders, &screens);
                const vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &g.cmd};
                g.queue_lock->With([&] {
                    g.queue.submit(submit, g.fence);
                    return 0;
                });
                g.in_flight = true;
                g.drawn_views = pending_views;
                g.drawn_screen = screens.Any();
                g.drawn_serial = screens.eye[0].shown ? screens.eye[0].serial : screens.eye[1].serial;
                g.stat_draws++;
                g.stat_screen_draws += g.drawn_screen ? 1 : 0;
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
            out.layer = Layer();
            out.has_screen = g.shown_screen;
            g.stat_layers += out.has_screen ? 1 : 0;
        }
    }
    const double since = std::chrono::duration<double>(now - g.stats_since).count();
    if (since >= 1.0) {
        LOG_INFO(Frontend,
                 "xr: projection screens: {:.1f}s {} draws ({} with the top screen), {} refreshes "
                 "showed the last image again (GPU busy), {} kept it over a new swapchain, {} "
                 "layers with the screen; GPU {:.2f} ms "
                 "a draw (max {:.2f}, {} timed; screen and models, {}x{} per eye, {}x MSAA, scale "
                 "{:.2f})",
                 since, g.stat_draws, g.stat_screen_draws, g.stat_busy, g.stat_gap, g.stat_layers,
                 g.stat_gpu_n ? g.stat_gpu_sum / g.stat_gpu_n : 0.0, g.stat_gpu_max, g.stat_gpu_n,
                 g.eye_w, g.eye_h, static_cast<int>(g.samples), g.scale);
        g.stats_since = now;
        g.stat_draws = g.stat_screen_draws = g.stat_busy = g.stat_layers = g.stat_gpu_n = 0;
        g.stat_gap = 0;
        g.stat_gpu_sum = g.stat_gpu_max = 0.0;
    }
    return out;
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
    // AzaharFrame projscreen: the screen needs the eye buffer's full size (times the scale); then
    // the pass is over 1000 pixels high, so Valve's foveation layer may lower the edges.
    g.screens = screens_mode;
    g.scale = screens_scale;
    if (g.screens) {
        g.eye_w = std::max(200u, static_cast<uint32_t>(static_cast<float>(rec_w) * g.scale));
        g.eye_h = std::max(200u, static_cast<uint32_t>(static_cast<float>(rec_h) * g.scale));
    }
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
    // AzaharFrame projscreen: no MSAA at full size (the screen has no edges inside it; the models
    // are drawn at twice the size), unless AZAHARFRAME_XR_PROJ_MSAA=4.
    const char* proj_msaa = std::getenv("AZAHARFRAME_XR_PROJ_MSAA");
    const bool want_msaa = !g.screens || (proj_msaa && std::atoi(proj_msaa) == 4);
    if (want_msaa && (limits.framebufferColorSampleCounts & vk::SampleCountFlagBits::e4) &&
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
    // AzaharFrame kiuguns: the same with the colour added and no depth written (weapon glows).
    const vk::PipelineColorBlendAttachmentState add_att{
        .blendEnable = true,
        .srcColorBlendFactor = vk::BlendFactor::eOne,
        .dstColorBlendFactor = vk::BlendFactor::eOne,
        .colorBlendOp = vk::BlendOp::eAdd,
        // AzaharFrame kiufix: the glow's coverage (the shader's alpha) adds up too, so the layer
        // holds it as premultiplied light the compositor keeps; with alpha 0 everywhere a
        // runtime may drop the pixels as clear, and the palm's flames did not show.
        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
        .dstAlphaBlendFactor = vk::BlendFactor::eOne,
        .alphaBlendOp = vk::BlendOp::eAdd,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };
    const vk::PipelineColorBlendStateCreateInfo add_blend{.attachmentCount = 1,
                                                          .pAttachments = &add_att};
    const vk::PipelineDepthStencilStateCreateInfo add_depth{
        .depthTestEnable = true,
        .depthWriteEnable = false,
        .depthCompareOp = vk::CompareOp::eLessOrEqual,
    };
    const auto made_add = g.device.createGraphicsPipeline({}, {
                                                                  .stageCount = 2,
                                                                  .pStages = shader_stages.data(),
                                                                  .pVertexInputState = &vertex_input,
                                                                  .pInputAssemblyState = &assembly,
                                                                  .pViewportState = &viewport,
                                                                  .pRasterizationState = &raster,
                                                                  .pMultisampleState = &multisample,
                                                                  .pDepthStencilState = &add_depth,
                                                                  .pColorBlendState = &add_blend,
                                                                  .pDynamicState = &dynamic,
                                                                  .layout = g.pipeline_layout,
                                                                  .renderPass = g.render_pass,
                                                                  .subpass = 0,
                                                              });
    g.device.destroyShaderModule(vert);
    g.device.destroyShaderModule(frag);
    if (made.result != vk::Result::eSuccess || made_add.result != vk::Result::eSuccess) {
        throw std::runtime_error("pipeline");
    }
    g.pipeline = made.value;
    g.pipeline_add = made_add.value;

    // AzaharFrame ctrlfront: the screens and panels into the depth buffer, before the models
    // (unless Off): a strip of triangles made in the vertex shader, no vertex buffer, no
    // fragment shader, no colour written.
    // AZAHARFRAME_CTRL_OCCLUDERS=0 (a test switch, read here once a session): no occluders, the
    // controllers over every screen, for A/B timing of the pass.
    const char* occ_env = std::getenv("AZAHARFRAME_CTRL_OCCLUDERS");
    if (occ_env && occ_env[0] == '0') {
        LOG_INFO(Frontend, "xr: controller models: occluders off (AZAHARFRAME_CTRL_OCCLUDERS=0)");
    } else if (!f.occluder_vert_spv.empty()) {
        const vk::ShaderModule occ_vert = Vulkan::CompileSPV(f.occluder_vert_spv, g.device);
        if (occ_vert) {
            const vk::PipelineShaderStageCreateInfo occ_stage{
                .stage = vk::ShaderStageFlagBits::eVertex, .module = occ_vert, .pName = "main"};
            const vk::PipelineVertexInputStateCreateInfo no_input{};
            const vk::PipelineInputAssemblyStateCreateInfo strip{
                .topology = vk::PrimitiveTopology::eTriangleStrip};
            const vk::PipelineColorBlendAttachmentState no_colour{.blendEnable = false,
                                                                  .colorWriteMask = {}};
            const vk::PipelineColorBlendStateCreateInfo occ_blend{.attachmentCount = 1,
                                                                  .pAttachments = &no_colour};
            const auto made_occ = g.device.createGraphicsPipeline(
                {}, {
                        .stageCount = 1,
                        .pStages = &occ_stage,
                        .pVertexInputState = &no_input,
                        .pInputAssemblyState = &strip,
                        .pViewportState = &viewport,
                        .pRasterizationState = &raster,
                        .pMultisampleState = &multisample,
                        .pDepthStencilState = &depth_state,
                        .pColorBlendState = &occ_blend,
                        .pDynamicState = &dynamic,
                        .layout = g.pipeline_layout,
                        .renderPass = g.render_pass,
                        .subpass = 0,
                    });
            g.device.destroyShaderModule(occ_vert);
            if (made_occ.result == vk::Result::eSuccess) {
                g.pipeline_occluder = made_occ.value;
            }
        }
        if (!g.pipeline_occluder) {
            LOG_WARNING(Frontend, "xr: controller models: no occluder pipeline; the controllers "
                                  "go over every screen");
        }
    }
    // AzaharFrame ctrlfront: two timestamps a picture, where the queue has them.
    {
        const auto families = g.physical.getQueueFamilyProperties();
        const uint32_t family = rt.vulkan->QueueFamily();
        const uint32_t bits = family < families.size() ? families[family].timestampValidBits : 0;
        if (bits > 0) {
            g.queries = g.device.createQueryPool(
                {.queryType = vk::QueryType::eTimestamp, .queryCount = 2});
            g.timestamp_ns = g.physical.getProperties().limits.timestampPeriod;
            g.timestamp_mask = bits >= 64 ? ~0ull : ((1ull << bits) - 1);
        }
    }
    // AzaharFrame projscreen: the screen's pipeline (its quad made in the vertex shader), drawn
    // first with depth so the models are tested against it.
    if (g.screens && !f.screen_vert_spv.empty() && !f.screen_frag_spv.empty()) {
        g.screen_format = static_cast<vk::Format>(rt.swapchain_format);
        const vk::DescriptorSetLayoutBinding sb{0, vk::DescriptorType::eCombinedImageSampler, 1,
                                                vk::ShaderStageFlagBits::eFragment};
        g.screen_set_layout =
            g.device.createDescriptorSetLayout({.bindingCount = 1, .pBindings = &sb});
        const vk::DescriptorPoolSize sp{vk::DescriptorType::eCombinedImageSampler, 2};
        g.screen_pool =
            g.device.createDescriptorPool({.maxSets = 2, .poolSizeCount = 1, .pPoolSizes = &sp});
        const std::array<vk::DescriptorSetLayout, 2> sl{g.screen_set_layout, g.screen_set_layout};
        const auto ss = g.device.allocateDescriptorSets(
            {.descriptorPool = g.screen_pool, .descriptorSetCount = 2, .pSetLayouts = sl.data()});
        g.screen_sets = {ss[0], ss[1]};
        const vk::PushConstantRange spush{
            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
            sizeof(ScreenPushConstants)};
        g.screen_layout = g.device.createPipelineLayout({.setLayoutCount = 1,
                                                         .pSetLayouts = &g.screen_set_layout,
                                                         .pushConstantRangeCount = 1,
                                                         .pPushConstantRanges = &spush});
        const vk::ShaderModule sv = Vulkan::CompileSPV(f.screen_vert_spv, g.device);
        const vk::ShaderModule sf = Vulkan::CompileSPV(f.screen_frag_spv, g.device);
        if (!sv || !sf) {
            throw std::runtime_error("screen shader modules");
        }
        const std::array<vk::PipelineShaderStageCreateInfo, 2> sstages{{
            {.stage = vk::ShaderStageFlagBits::eVertex, .module = sv, .pName = "main"},
            {.stage = vk::ShaderStageFlagBits::eFragment, .module = sf, .pName = "main"},
        }};
        const vk::PipelineVertexInputStateCreateInfo no_input{};
        const auto made_screen =
            g.device.createGraphicsPipeline({}, {
                                                    .stageCount = 2,
                                                    .pStages = sstages.data(),
                                                    .pVertexInputState = &no_input,
                                                    .pInputAssemblyState = &assembly,
                                                    .pViewportState = &viewport,
                                                    .pRasterizationState = &raster,
                                                    .pMultisampleState = &multisample,
                                                    .pDepthStencilState = &depth_state,
                                                    .pColorBlendState = &blend,
                                                    .pDynamicState = &dynamic,
                                                    .layout = g.screen_layout,
                                                    .renderPass = g.render_pass,
                                                    .subpass = 0,
                                                });
        g.device.destroyShaderModule(sv);
        g.device.destroyShaderModule(sf);
        if (made_screen.result != vk::Result::eSuccess) {
            throw std::runtime_error("screen pipeline");
        }
        g.screen_pipeline = made_screen.value;
        // The picture has one mip level (SteamVR allocates level 0 only): the fragment shader
        // takes up to 4 x 4 bilinear taps over each pixel's footprint instead.
        g.screen_sampler = g.device.createSampler({
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eNearest,
            .addressModeU = vk::SamplerAddressMode::eClampToEdge,
            .addressModeV = vk::SamplerAddressMode::eClampToEdge,
            .addressModeW = vk::SamplerAddressMode::eClampToEdge,
            .maxLod = 0.0f,
        });
    }
    // AzaharFrame projscreen: AZAHARFRAME_XR_MODELS_GPU_TIME=1 logs the default pass's GPU time
    // every second (ctrlfront's timestamps above); screens mode always does, in its own line.
    const char* time_env = std::getenv("AZAHARFRAME_XR_MODELS_GPU_TIME");
    g.time_log = g.queries && (g.screens || (time_env && time_env[0] == '1'));

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
    // AzaharFrame kiuguns: the weapon in hand: its own sets (freed one weapon at a time), command
    // buffer and fence.
    const vk::DescriptorPoolSize weapon_size{vk::DescriptorType::eCombinedImageSampler, 64};
    g.weapon_pool = g.device.createDescriptorPool(
        {.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
         .maxSets = 32,
         .poolSizeCount = 1,
         .pPoolSizes = &weapon_size});
    g.weapon_cmd = g.device.allocateCommandBuffers(
        {.commandPool = g.pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1})[0];
    g.weapon_fence = g.device.createFence({});

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
    g.staging_for_upload = g.staging;
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
            const auto toff = put(pixels.data(), pixels.size());
            (t == 0 ? hg.colour : hg.surface) =
                g.UploadImage(u, toff, m.tex_width, m.tex_height, tf, nullptr, nullptr);
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
             "xr: controller models: {}x{} per eye, {}x MSAA, {} MB uploading; one swapchain{}",
             g.eye_w, g.eye_h, static_cast<int>(g.samples), total >> 20,
             g.screens ? fmt::format(" (projection screens: the top screen drawn into it, scale "
                                     "{:.2f}, recommended {}x{}{}{})",
                                     g.scale, rec_w, rec_h,
                                     g.screen_pipeline ? "" : ", NO screen pipeline",
                                     g.time_log ? ", GPU timed" : "")
                       : std::string{});
}

namespace {

// AzaharFrame kiuguns: a weapon's parts at the aim pose times each fit (weapon_fit.h).
template <typename GpuT, typename WeaponT, typename HandT>
void RecordWeapon(vk::CommandBuffer& c, GpuT& g, const WeaponT& w, const Mat& view_proj,
                  const XrVector3f& eye_pos, const Mat& head, const HandT& hand, bool left,
                  const std::vector<FitPart>& fits, float clip_x, const FitMat* forearm_turn) {
    (void)left;
    const XrPosef aim = ToXr(hand.aim);
    const Mat aim_m = FromPose(aim.orientation, aim.position);
    // The glow and effect parts (WpnEfe ...) are cards the game blends and animates; drawn plain
    // they float about, so only with AZAHARFRAME_KIU_WEAPON_GLOW=1 (live).
    // A palm's flames are its look: kept (in place: FitMotion; moving: AnimateGlow).
    const bool glow =
        w.info.cls == FrameWeapons::kPalm ||
        Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_WEAPON_GLOW").value_or("0") == "1";
    const double now_s =
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    // AzaharFrame kiumodels: AZAHARFRAME_KIU_GLOW_MOTION=0 (live): the glows still (as before).
    const bool glow_motion =
        Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_GLOW_MOTION").value_or("1") != "0";
    for (const FitPart& fit : fits) {
      for (const bool glow_pass : {false, true}) {
        if (glow_pass && !glow) {
            continue;
        }
        c.bindPipeline(vk::PipelineBindPoint::eGraphics, glow_pass ? g.pipeline_add : g.pipeline);
        Mat fit_m{};
        std::copy(fit.transform.begin(), fit.transform.end(), fit_m.begin());
        // The orbitars' and detached parts' float (weapon_fit.h FitMotion), across the ray.
        const auto motion = FitMotion(w.info.cls, fit.side, glow_pass, std::fmod(now_s, 3600.0));
        for (int k = 0; k < 3; k++) {
            fit_m[static_cast<std::size_t>(12 + k)] += motion[static_cast<std::size_t>(k)];
        }
        // A palm in elbow mode: the hand (cut at the wrist), then the forearm turned toward the
        // elbow (from the wrist up to the cut); else one draw.
        for (int pass = 0; pass < (forearm_turn ? 2 : 1); pass++) {
        Mat model = Multiply(aim_m, fit_m);
        // AzaharFrame kiufix: a palm's flames run the length of Pit's forearm, past the wrist
        // mode's cut: they are drawn whole (the player's own forearm is there), so they show.
        float cut = glow_pass && w.info.cls == FrameWeapons::kPalm && !forearm_turn ? 0.0f : clip_x;
        float w_flags = fit.mirrored ? 1.0f : 0.0f;
        if (glow_pass) {
            w_flags += 4.0f; // added, unlit
        }
        if (forearm_turn && pass == 0) {
            cut = 1e-5f; // the hand: nothing past the wrist
        } else if (forearm_turn) {
            Mat turn{};
            std::copy(forearm_turn->begin(), forearm_turn->end(), turn.begin());
            model = Multiply(turn, model);
            w_flags += 2.0f; // the forearm: nothing before the wrist
        }
        const Mat to_model = InverseAffine(model);
        PushConstants pc{};
        const auto eye = Apply(to_model, eye_pos.x, eye_pos.y, eye_pos.z, 1.0f);
        pc.eye = {eye[0], eye[1], eye[2], cut};
        const auto back = Apply(head, 0.0f, 0.0f, 1.0f, 0.0f);
        const auto key = Apply(to_model, back[0] * 0.6f, 1.0f + back[1] * 0.6f, back[2] * 0.6f, 0.0f);
        pc.key = {key[0], key[1], key[2], 0.0f};
        const auto up = Apply(to_model, 0.0f, 1.0f, 0.0f, 0.0f);
        pc.up = {up[0], up[1], up[2], 0.0f};
        pc.params = {0.0f, 1.0f, std::clamp(hand.fade, 0.0f, 1.0f), w_flags};
        pc.mvp = Multiply(view_proj, model);
        const char* side = fit.side == FitSide::Right  ? "R"
                           : fit.side == FitSide::Left ? "L"
                                                       : "C";
        for (const auto& part : w.parts) {
            if (part.glow != glow_pass || part.flash) {
                continue;
            }
            // AzaharFrame kiumodels: a glow moves (its texture slides, it flickers, a palm's
            // flames sway): its motion in key and up, which a glow's light does not use.
            if (glow_pass && glow_motion) {
                const GlowMotion m = AnimateGlow(w.info.cls, static_cast<int>(part.uv.source),
                                                 part.uv.frames, part.uv.du, part.uv.dv,
                                                 std::fmod(now_s, 3600.0), FrameWeapons::kGameFps);
                pc.key = {m.u, m.v, m.gain, m.sway};
                pc.up = {m.phase_a, m.phase_b, m.wave, 0.0f};
                pc.params[3] = w_flags + 8.0f;
            }
            bool bound = false;
            for (const ModelComponent& comp : part.components) {
                if (comp.name != side || comp.index_count == 0) {
                    continue;
                }
                if (!bound) {
                    c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, g.pipeline_layout, 0,
                                         part.set, {});
                    c.bindVertexBuffers(0, part.vertices, vk::DeviceSize{0});
                    c.bindIndexBuffer(part.indices, 0, vk::IndexType::eUint32);
                    c.pushConstants(g.pipeline_layout,
                                    vk::ShaderStageFlagBits::eVertex |
                                        vk::ShaderStageFlagBits::eFragment,
                                    0, sizeof(pc), &pc);
                    bound = true;
                }
                c.drawIndexed(comp.index_count, 1, comp.first_index, 0, 0);
            }
        }
        }
      }
    }
    c.bindPipeline(vk::PipelineBindPoint::eGraphics, g.pipeline); // for what is drawn next
}

// AzaharFrame kiuflash: the muzzle flash's cards of its kind (muzzle_flash.h), added to what is
// behind (the glow pipeline), every card placed from the centre eye so both eyes see one card.
template <typename GpuT, typename WeaponT>
void RecordFlash(vk::CommandBuffer& c, GpuT& g, const WeaponT& w, const Mat& view_proj,
                 const XrPosef& head_pose, const Input::Posef& hand_aim, float fade,
                 const std::array<float, 3>& muzzle, float fit_scale, const FlashFrame& flash,
                 float tune_size) {
    const XrPosef aim = ToXr(hand_aim);
    const Mat aim_m = FromPose(aim.orientation, aim.position);
    const auto o = Apply(aim_m, muzzle[0], muzzle[1], muzzle[2], 1.0f);
    const auto f = Apply(aim_m, 0.0f, 0.0f, -1.0f, 0.0f);
    const Mat head = FromPose(head_pose.orientation, head_pose.position);
    const auto up = Apply(head, 0.0f, 1.0f, 0.0f, 0.0f);
    const std::array<float, 3> origin{o[0], o[1], o[2]}, forward{f[0], f[1], f[2]},
        eye{head_pose.position.x, head_pose.position.y, head_pose.position.z},
        head_up{up[0], up[1], up[2]};
    const char* kind = flash.charged ? "charged" : "rapid";
    const float strength = FlashIntensity(flash.age) * std::clamp(fade, 0.0f, 1.0f);
    if (!(strength > 0.0f)) {
        return;
    }
    bool pipeline = false;
    uint32_t n = 0;
    for (const auto& part : w.parts) {
        if (!part.flash) {
            continue;
        }
        bool bound = false;
        for (std::size_t k = 0; k < part.components.size(); k++) {
            const ModelComponent& comp = part.components[k];
            if (comp.name != kind || comp.index_count == 0) {
                continue;
            }
            if (!pipeline) {
                c.bindPipeline(vk::PipelineBindPoint::eGraphics, g.pipeline_add);
                pipeline = true;
            }
            if (!bound) {
                c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, g.pipeline_layout, 0,
                                     part.set, {});
                c.bindVertexBuffers(0, part.vertices, vk::DeviceSize{0});
                c.bindIndexBuffer(part.indices, 0, vk::IndexType::eUint32);
                bound = true;
            }
            const float* v = comp.motion.data();
            const bool fan = comp.path == "fan";
            const FlashCardExtent size =
                k < part.card_size.size() ? part.card_size[k] : FlashCardExtent{};
            const float metres = FlashCardMetres(fan, size.Units(), fit_scale, tune_size) *
                                 FlashGrowth(flash.age, v[FrameWeapons::kFlashGrow]);
            const auto card = FlashCardTransform(fan, origin, forward, eye, head_up, metres,
                                                 v[FrameWeapons::kFlashOffset],
                                                 FlashSpin(flash.shot * 7u + n++));
            Mat model{};
            std::copy(card.begin(), card.end(), model.begin());
            PushConstants pc{};
            pc.mvp = Multiply(view_proj, model);
            pc.key = {FlashLinear(v[FrameWeapons::kFlashRed]),
                      FlashLinear(v[FrameWeapons::kFlashGreen]),
                      FlashLinear(v[FrameWeapons::kFlashBlue]), v[FrameWeapons::kFlashCore]};
            pc.up = {v[FrameWeapons::kFlashMode], size.half_width, size.half_length, size.middle};
            // A glow (+4) and a flash card (+16: +8 is kiumodels' moving glow); x: a fan.
            pc.params = {fan ? 1.0f : 0.0f, 1.0f, strength, 20.0f};
            c.pushConstants(g.pipeline_layout,
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                            0, sizeof(pc), &pc);
            c.drawIndexed(comp.index_count, 1, comp.first_index, 0, 0);
        }
    }
    if (pipeline) {
        c.bindPipeline(vk::PipelineBindPoint::eGraphics, g.pipeline);
    }
}

} // namespace

void ControllerModels::FitWeapon(const ViewPose& views, const std::array<Hand, 2>& hands) { // AzaharFrame kiuguns
    // The weapon's fit, with the class's live tune (test switches), and its muzzle.
    weapon_fits.clear();
    muzzle_hand = -1;
    muzzle.reset();
    weapon_clip = 0.0f;
    palm_elbow = false;
    if (!gpu) {
        return;
    }
    Gpu& g = *gpu;
    for (int h = 0; h < 2; h++) {
        const Hand& hand = hands[static_cast<std::size_t>(h)];
        if (hand.shown && hand.weapon >= 0 && g.weapon_live && g.weapon_live->id == hand.weapon) {
            const FrameWeapons::Weapon& info = g.weapon_live->info;
            const std::string name = FitSwitchName(info.cls);
            const auto value = Common::FrameTestSwitches::Value(name);
            FitOverride tune = ParseFitOverride(value);
            // AzaharFrame kiufix: Weapon Size and Weapon Scale (per game): a factor on the fit's
            // scale, about the grip (the grip stays in the palm, the muzzle on the ray).
            const WeaponSize size_mode = static_cast<WeaponSize>(
                std::min<u32>(Settings::values.vr_light_gun_weapon_size.GetValue(), 1));
            const int size_pct =
                static_cast<int>(Settings::values.vr_light_gun_weapon_scale.GetValue());
            const float size_factor =
                WeaponSizeFactor(info.cls, info.bounds_min, info.bounds_max,
                                 FitClassScale(info.cls) * tune.scale, size_mode, size_pct);
            tune.scale *= size_factor;
            std::vector<FitSide> sides;
            for (const auto& part : g.weapon_live->parts) {
                for (const auto& comp : part.components) {
                    const FitSide side = comp.name == "R"   ? FitSide::Right
                                         : comp.name == "L" ? FitSide::Left
                                                            : FitSide::Centre;
                    if (std::find(sides.begin(), sides.end(), side) == sides.end()) {
                        sides.push_back(side);
                    }
                }
            }
            const bool left = h == Input::HandIndex(Input::Hand::Left);
            weapon_fits = FitWeaponToHand({info.cls, info.muzzle, info.muzzle_valid}, left, sides, tune);
            weapon_scale = FitClassScale(info.cls) * tune.scale; // AzaharFrame kiuflash
            muzzle_hand = h;
            muzzle = FitMuzzle({info.cls, info.muzzle, info.muzzle_valid}, left, sides, tune);

            const auto forearm = Common::FrameTestSwitches::Value(kPalmForearmSwitch);
            const PalmMode mode =
                PalmModeFrom(Common::FrameTestSwitches::Value(kPalmModeSwitch));
            weapon_clip = FitClipX(info.cls, tune, forearm, mode);
            palm_elbow = info.cls == FrameWeapons::kPalm && mode == PalmMode::Elbow &&
                         weapon_fits.size() == 1 && views.valid;
            if (palm_elbow) {
                // The wrist where the fit puts it, the forearm turned from where it points to
                // the estimated elbow (smoothed), about the wrist.
                const XrPosef aim = ToXr(hand.aim);
                Mat fit_m{};
                std::copy(weapon_fits[0].transform.begin(), weapon_fits[0].transform.end(),
                          fit_m.begin());
                const Mat world = Multiply(FromPose(aim.orientation, aim.position), fit_m);
                const auto wrist = Apply(world, 0.0f, 0.0f, 0.0f, 1.0f);
                const auto along = Apply(world, 1.0f, 0.0f, 0.0f, 0.0f);
                const Mat head = FromPose(views.head.orientation, views.head.position);
                const auto fwd = Apply(head, 0.0f, 0.0f, -1.0f, 0.0f);
                const std::array<float, 3> w3{wrist[0], wrist[1], wrist[2]};
                const auto elbow = EstimateElbow(
                    {views.head.position.x, views.head.position.y, views.head.position.z},
                    {fwd[0], fwd[1], fwd[2]}, left, w3);
                const std::array<float, 3> target{elbow[0] - w3[0], elbow[1] - w3[1],
                                                  elbow[2] - w3[2]};
                const int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count();
                const float dt = palm_time_ns ? static_cast<float>(now - palm_time_ns) * 1e-9f : -1.0f;
                palm_time_ns = now;
                palm_dir = SmoothDirection(palm_dir, target, dt);
                palm_turn = RotationBetween(w3, {along[0], along[1], along[2]}, palm_dir);
            } else {
                palm_time_ns = 0;
                palm_dir = {};
            }
            const std::string logged =
                fmt::format("{} {} {} {} {} {} {:.3f}", info.id, left ? "left" : "right", name,
                            value.value_or("default"), forearm.value_or("default"),
                            palm_elbow ? "elbow" : "wrist", size_factor);
            if (logged != g.weapon_fit_logged) {
                g.weapon_fit_logged = logged;
                LOG_INFO(Frontend,
                         "xr: weapon in hand: {} (a {}) in the {} hand, fit {} (scale {:.3f} m "
                         "per unit; Weapon Size {}, {} %: x{:.2f}){}",
                         info.id, FrameWeapons::ClassName(info.cls), left ? "left" : "right",
                         value ? name + "=" + *value : std::string{"default"},
                         FitClassScale(info.cls) * tune.scale,
                         size_mode == WeaponSize::Hand ? "Hand Size" : "Game Size", size_pct,
                         size_factor,
                         weapon_clip > 0.0f
                             ? fmt::format(", forearm cut {:.0f} cm from the wrist{}",
                                           weapon_clip * FitClassScale(info.cls) * tune.scale * 100.0f,
                                           palm_elbow ? ", turned toward the elbow" : "")
                             : std::string{});
            }
        }
    }
}

// AzaharFrame ctrlfront: each shown hand's model inside a sphere (LOCAL), the shown ones first.
std::array<ModelSphere, 2> ControllerModels::Spheres(const std::array<Hand, 2>& hands) const {
    const Files& f = *files;
    std::array<ModelSphere, 2> spheres{};
    std::size_t n = 0;
    for (std::size_t h = 0; h < hands.size(); h++) {
        const Hand& hand = hands[h];
        if (hand.shown) {
            spheres[n++] = hand.weapon >= 0
                               ? ModelSphere{hand.aim.position, kWeaponSphere}
                               : ModelSphere{Input::Transform(hand.aim, f.sphere[h].centre),
                                             f.sphere[h].radius};
        }
    }
    return spheres;
}

std::size_t ControllerModels::SphereCount(const std::array<Hand, 2>& hands) {
    return static_cast<std::size_t>(hands[0].shown) + static_cast<std::size_t>(hands[1].shown);
}

void ControllerModels::Record(const ViewPose& views, const std::array<Hand, 2>& hands,
                              const std::vector<Occluder>& occluders,
                              const ProjScreens* screens) {
    Gpu& g = *gpu;
    const Files& f = *files;
    vk::CommandBuffer& c = g.cmd;
    c.reset();
    c.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    if (g.queries) { // AzaharFrame ctrlfront
        c.resetQueryPool(g.queries, 0, 2);
        c.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, g.queries, 0);
    }
    // AzaharFrame projscreen: the top screen's images, sampled from the swapchain the renderer
    // released them to (never given to the runtime in screens mode): a view and a descriptor set
    // for each eye, and the images moved to SHADER_READ_ONLY for the pass and back after it.
    std::vector<vk::Image> screen_images;
    const bool draw_screens = screens && screens->Any() && g.screen_pipeline;
    if (draw_screens) {
        g.DestroyScreenViews();
        std::array<vk::ImageView, 2> eye_view{};
        for (int e = 0; e < 2; e++) {
            const ProjScreenEye& se = screens->eye[static_cast<std::size_t>(e)];
            if (!se.shown || se.image == VK_NULL_HANDLE) {
                continue;
            }
            const vk::Image image{se.image};
            const auto seen = std::find(screen_images.begin(), screen_images.end(), image);
            if (seen != screen_images.end()) {
                eye_view[static_cast<std::size_t>(e)] =
                    g.screen_views[static_cast<std::size_t>(seen - screen_images.begin())];
            } else {
                screen_images.push_back(image);
                g.screen_views.push_back(g.device.createImageView({
                    .image = image,
                    .viewType = vk::ImageViewType::e2D,
                    .format = g.screen_format,
                    .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
                }));
                eye_view[static_cast<std::size_t>(e)] = g.screen_views.back();
            }
            const vk::DescriptorImageInfo info{g.screen_sampler, eye_view[static_cast<std::size_t>(e)],
                                               vk::ImageLayout::eShaderReadOnlyOptimal};
            g.device.updateDescriptorSets(
                vk::WriteDescriptorSet{.dstSet = g.screen_sets[static_cast<std::size_t>(e)],
                                       .dstBinding = 0,
                                       .descriptorCount = 1,
                                       .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                       .pImageInfo = &info},
                {});
        }
        std::vector<vk::ImageMemoryBarrier> to_read;
        for (const vk::Image image : screen_images) {
            to_read.push_back({
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                 vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
            });
        }
        c.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput |
                              vk::PipelineStageFlagBits::eTransfer,
                          vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, to_read);
    }
    const bool marks = draw_screens &&
                       Common::FrameTestSwitches::Value(kProjMarksSwitch).value_or("0") == "1";
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
    // AzaharFrame kiuflash: AZAHARFRAME_KIU_FLASH's size factor (live), only while one shows.
    float flash_size = 1.0f;
    for (const Hand& hand : hands) {
        if (hand.flash.shown) {
            flash_size =
                ParseFlashTune(Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_FLASH")).size;
        }
    }
    for (int e = 0; e < 2; e++) {
        const vk::Viewport vp{static_cast<float>(g.eye_w * e), 0.0f, static_cast<float>(g.eye_w),
                              static_cast<float>(g.eye_h), 0.0f, 1.0f};
        const vk::Rect2D sc{{static_cast<int32_t>(g.eye_w * e), 0}, {g.eye_w, g.eye_h}};
        c.setViewport(0, vp);
        c.setScissor(0, sc);
        const Mat view_proj =
            Multiply(Projection(views.fov[e], g.screens ? kFarScreens : kFar),
                     InverseRigid(FromPose(views.eye[e].orientation, views.eye[e].position)));
        // AzaharFrame ctrlfront: the depth of the screens and panels this eye sees first (a
        // screen the choice leaves out at the nearest depth): a controller's parts behind one are
        // not drawn, and the screen shows there.
        // Only where the models can be in this eye's image (model_plane.h ModelsViewBounds).
        std::optional<std::array<float, 4>> bounds;
        if (!occluders.empty()) {
            const std::array<ModelSphere, 2> spheres = Spheres(hands);
            const XrFovf& fov = views.fov[e];
            bounds = ModelsViewBounds(FromXr(views.eye[e]), fov.angleLeft, fov.angleRight,
                                      fov.angleUp, fov.angleDown, spheres.data(),
                                      SphereCount(hands), kNear);
        }
        bool occluded = false;
        for (const Occluder& o : occluders) {
            if (!bounds || !(o.eyes & (1u << e)) || o.width_m <= 0.0f || o.height_m <= 0.0f) {
                continue;
            }
            if (!occluded) {
                c.bindPipeline(vk::PipelineBindPoint::eGraphics, g.pipeline_occluder);
                const auto px = [](float t, uint32_t size) {
                    return static_cast<int32_t>(std::clamp(t, 0.0f, 1.0f) * static_cast<float>(size));
                };
                const int32_t x0 = px((*bounds)[0], g.eye_w), y0 = px((*bounds)[1], g.eye_h);
                const int32_t x1 = std::min<int32_t>(px((*bounds)[2], g.eye_w) + 1,
                                                     static_cast<int32_t>(g.eye_w));
                const int32_t y1 = std::min<int32_t>(px((*bounds)[3], g.eye_h) + 1,
                                                     static_cast<int32_t>(g.eye_h));
                c.setScissor(0, vk::Rect2D{{static_cast<int32_t>(g.eye_w * e) + x0, y0},
                                           {static_cast<uint32_t>(std::max(x1 - x0, 0)),
                                            static_cast<uint32_t>(std::max(y1 - y0, 0))}});
                occluded = true;
            }
            PushConstants pc{};
            const XrPosef op = ToXr(o.pose);
            pc.mvp = Multiply(view_proj, FromPose(op.orientation, op.position));
            const int segments = OccluderSegments(o);
            pc.params = {o.width_m, o.height_m, o.curve_radius, static_cast<float>(segments)};
            pc.eye = {o.always ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f};
            c.pushConstants(g.pipeline_layout,
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                            0, sizeof(pc), &pc);
            c.draw(static_cast<uint32_t>(2 * (segments + 1)), 1, 0, 0);
        }
        if (occluded) {
            c.bindPipeline(vk::PipelineBindPoint::eGraphics, g.pipeline);
            c.setScissor(0, sc);
        }
        // AzaharFrame projscreen: this eye's top screen first, where its quad would have been.
        const ProjScreenEye* se = draw_screens ? &screens->eye[static_cast<std::size_t>(e)] : nullptr;
        if (se && se->shown && se->image != VK_NULL_HANDLE && se->image_width && se->image_height) {
            ScreenPushConstants sp{};
            sp.mvp = Multiply(view_proj, FromPose(se->pose.orientation, se->pose.position));
            const float columns = se->curve > 0.0f ? 32.0f : 1.0f;
            sp.shape = {se->size.width * 0.5f, se->size.height * 0.5f, se->curve, columns};
            const float iw = static_cast<float>(se->image_width);
            const float ih = static_cast<float>(se->image_height);
            sp.rect = {se->rect.offset.x / iw, se->rect.offset.y / ih, se->rect.extent.width / iw,
                       se->rect.extent.height / ih};
            sp.tex = {iw, ih, marks ? 1.0f : 0.0f, 0.0f};
            sp.extra = {ProjDepthCode(se->depth), 0.0f, 0.0f, 0.0f}; // projb
            c.bindPipeline(vk::PipelineBindPoint::eGraphics, g.screen_pipeline);
            c.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, g.screen_layout, 0,
                                 g.screen_sets[static_cast<std::size_t>(e)], {});
            c.pushConstants(g.screen_layout,
                            vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
                            sizeof(sp), &sp);
            c.draw(static_cast<uint32_t>(columns) * 6, 1, 0, 0);
            c.bindPipeline(vk::PipelineBindPoint::eGraphics, g.pipeline);
            if (marks) {
                // The picture's serial in this eye's top left corner (proj_screen.h kProjCodeBits).
                std::vector<vk::ClearRect> ones, zeros;
                for (uint32_t b = 0; b < kProjCodeBits + 2; b++) {
                    const bool one = b < kProjCodeBits ? ((se->serial >> b) & 1) != 0
                                                       : b == kProjCodeBits;
                    const vk::ClearRect rect{
                        {{static_cast<int32_t>(g.eye_w * e + b * kProjCodeCell), 0},
                         {kProjCodeCell, kProjCodeCell}},
                        0,
                        1};
                    (one ? ones : zeros).push_back(rect);
                }
                const vk::ClearAttachment white{vk::ImageAspectFlagBits::eColor, 0,
                                                vk::ClearColorValue{std::array<float, 4>{1, 1, 1, 1}}};
                const vk::ClearAttachment black{vk::ImageAspectFlagBits::eColor, 0,
                                                vk::ClearColorValue{std::array<float, 4>{0, 0, 0, 1}}};
                c.clearAttachments(white, ones);
                c.clearAttachments(black, zeros);
            }
        }
        for (int h = 0; h < 2; h++) {
            const Hand& hand = hands[static_cast<std::size_t>(h)];
            if (!hand.shown) {
                continue;
            }
            // AzaharFrame kiuguns: the weapon in hand in place of the controller, once it is on
            // the GPU.
            if (hand.weapon >= 0 && g.weapon_live && g.weapon_live->id == hand.weapon) {
                RecordWeapon(c, g, *g.weapon_live, view_proj, views.eye[e].position, head, hand,
                             h == Input::HandIndex(Input::Hand::Left), weapon_fits, weapon_clip,
                             palm_elbow ? &palm_turn : nullptr);
                // AzaharFrame kiuflash: its muzzle flash after a real shot.
                if (hand.flash.shown && muzzle && muzzle_hand == h) {
                    const std::array<float, 3> from{
                        (*muzzle)[0], (*muzzle)[1],
                        (*muzzle)[2] - g.weapon_live->flash_push * weapon_scale /
                                           std::max(FitClassScale(g.weapon_live->info.cls), 1e-6f)};
                    RecordFlash(c, g, *g.weapon_live, view_proj, views.head, hand.aim, hand.fade,
                                from, weapon_scale, hand.flash, flash_size);
                }
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
            pc.eye = {eye[0], eye[1], eye[2], 0.0f}; // AzaharFrame kiuguns: w: no clip
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
    // AzaharFrame projscreen: the top screen's images back to the layout the renderer expects.
    if (!screen_images.empty()) {
        std::vector<vk::ImageMemoryBarrier> back;
        for (const vk::Image image : screen_images) {
            back.push_back({
                .srcAccessMask = {},
                .dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
                .oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
                .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1},
            });
        }
        c.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                          vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, back);
    }
    if (g.queries) { // AzaharFrame ctrlfront
        c.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, g.queries, 1);
    }
    c.end();
}

void ControllerModels::NoteGpuTime(int64_t now_ns) { // AzaharFrame ctrlfront
    Gpu& g = *gpu;
    if (!g.queries) {
        return;
    }
    std::array<uint64_t, 2> t{};
    // The image's fence has signalled, so both are written; never waits.
    if (g.device.getQueryPoolResults(g.queries, 0, 2, sizeof(t), t.data(), sizeof(uint64_t),
                                     vk::QueryResultFlagBits::e64) == vk::Result::eSuccess) {
        const uint64_t ticks = ((t[1] & g.timestamp_mask) - (t[0] & g.timestamp_mask)) &
                               g.timestamp_mask;
        const double ms = static_cast<double>(ticks) * g.timestamp_ns / 1e6;
        if (ms >= 0.0 && ms < 1000.0) {
            g.gpu_sum_ms += ms;
            g.gpu_max_ms = std::max(g.gpu_max_ms, ms);
            g.gpu_images++;
            g.occluder_sum += g.drawn_occluders;
            g.stat_gpu_sum += ms; // AzaharFrame projscreen: the per-second line
            g.stat_gpu_max = std::max(g.stat_gpu_max, ms);
            g.stat_gpu_n++;
        }
    }
    if (g.gpu_log_ns == 0) {
        g.gpu_log_ns = now_ns;
    }
    if (now_ns - g.gpu_log_ns >= kGpuLogNs && g.gpu_images > 0) {
        LOG_INFO(Frontend,
                 "xr: controller models: GPU {:.3f} ms a picture on average, {:.3f} at most, {} "
                 "pictures in {:.0f} s, {:.1f} occluders a picture, {}{}",
                 g.gpu_sum_ms / g.gpu_images, g.gpu_max_ms, g.gpu_images,
                 static_cast<double>(now_ns - g.gpu_log_ns) / 1e9,
                 static_cast<double>(g.occluder_sum) / g.gpu_images,
                 g.screens ? "projection screens (a projection layer with the top screen)"
                 : over_ == ControllersInFront::Off          ? "Off (a projection layer)"
                 : over_ == ControllersInFront::Primary      ? "Primary (eye quads)"
                 : over_ == ControllersInFront::Secondary    ? "Secondary (eye quads)"
                                                             : "Both (eye quads)",
                 // AzaharFrame ctrlglitch
                 !ModelsAsQuads(over_) ? std::string{}
                 : g.canvas_on
                     ? fmt::format("; on a world-locked canvas, moved {} times, {} refreshes "
                                   "also as the projection layer",
                                   g.canvas_moves, g.canvas_projection)
                     : std::string{"; at each image's eye views (AZAHARFRAME_XR_CANVAS=0)"});
        g.canvas_moves = g.canvas_projection = 0;
        g.gpu_sum_ms = g.gpu_max_ms = 0.0;
        g.gpu_images = 0;
        g.occluder_sum = 0;
        g.gpu_log_ns = now_ns;
    }
}

// ---- AzaharFrame kiuguns: the weapon in hand ----

void ControllerModels::UpdateWeapon(int id) {
    Gpu& g = *gpu;
    // An upload done: it goes live; the one before is freed once no image uses it.
    if (g.weapon_uploading && g.device.getFenceStatus(g.weapon_fence) == vk::Result::eSuccess) {
        g.device.resetFences(g.weapon_fence);
        g.device.destroyBuffer(g.weapon_staging);
        g.device.freeMemory(g.weapon_staging_memory);
        g.weapon_staging = nullptr;
        g.weapon_staging_memory = nullptr;
        if (g.weapon_live) {
            g.weapon_retired.push_back(std::move(g.weapon_live));
        }
        g.weapon_live = std::move(g.weapon_uploading);
        LOG_INFO(Frontend,
                 "xr: weapon in hand: {} (a {}), {} parts, {} triangles, {} KB on the GPU{}",
                 g.weapon_live->id, FrameWeapons::ClassName(g.weapon_live->info.cls),
                 g.weapon_live->parts.size(), g.weapon_live->triangles,
                 g.weapon_live->gpu_bytes >> 10,
                 g.weapon_live->pack_textures ? " (textures from the texture pack)" : "");
        // AzaharFrame kiuflash: its flashes (in those numbers).
        const FrameWeapons::FlashInfo& fl = g.weapon_live->info.flash;
        std::size_t cards = 0;
        for (const auto& part : g.weapon_live->parts) {
            cards += part.flash ? part.components.size() : 0;
        }
        LOG_INFO(Frontend,
                 "xr: muzzle flash: {} cards ({} KB on the GPU), {:.0f} cm out of the muzzle: "
                 "rapid {}, charged {}{}",
                 cards, g.weapon_live->flash_bytes >> 10, g.weapon_live->flash_push * 100.0f,
                 fl.rapid.empty() ? "none" : fl.rapid, fl.charged.empty() ? "none" : fl.charged,
                 fl.source >= 0 && fl.source != g.weapon_live->id
                     ? fmt::format(" (weapon {}'s, neutral)", fl.source)
                     : std::string{});
    }
    // A read done: to the GPU (or noted as failed, not tried again until the files change).
    if (weapon_load_state == 2) {
        if (weapon_loader.joinable()) {
            weapon_loader.join();
        }
        WeaponLoad& l = *weapon_load;
        bool ok = l.ok;
        if (ok && !g.weapon_uploading) {
            try {
                UploadWeapon();
            } catch (const std::exception& e) {
                l.error = std::string{"the upload failed: "} + e.what();
                ok = false;
            }
        }
        if (!ok) {
            g.weapon_failed_id = l.id;
            g.weapon_failed_generation = l.generation;
            LOG_WARNING(Frontend, "xr: weapon in hand: {}: {} (the controller is drawn)", l.id,
                        l.error);
        }
        weapon_load.reset();
        weapon_load_state = 0;
    }
    // A read to start: a weapon wanted that is not the one there, once its files are made (only
    // the weapons asked for are: core/frame_weapon_models.h Request(id)).
    if (id < 0 || weapon_load_state != 0 || g.weapon_uploading ||
        (g.weapon_live && g.weapon_live->id == id) || id == g.weapon_failed_id) {
        return;
    }
    const FrameWeapons::State made = FrameWeapons::WeaponState(id);
    if (made == FrameWeapons::State::None || made == FrameWeapons::State::Working) {
        FrameWeapons::Request(id);
        return;
    }
    if (made == FrameWeapons::State::Failed) {
        g.weapon_failed_id = id;
        LOG_WARNING(Frontend, "xr: weapon in hand: {}: its model could not be made (the "
                              "controller is drawn)",
                    id);
        return;
    }
    const uint32_t generation = FrameWeapons::Generation();
    if (weapon_loader.joinable()) {
        weapon_loader.join();
    }
    weapon_load = std::make_unique<WeaponLoad>();
    weapon_load->id = id;
    weapon_load->generation = generation;
    weapon_load_state = 1;
    const std::string dir = FrameWeapons::WeaponDir(FrameWeapons::CurrentTitle());
    weapon_loader = std::thread([this, dir] {
        WeaponLoad& l = *weapon_load;
        FrameWeapons::Manifest manifest;
        std::string error;
        if (!FrameWeapons::ReadManifest(dir + "weapons.txt", manifest, error)) {
            l.error = error;
        } else if (const FrameWeapons::Weapon* w = manifest.Find(l.id)) {
            l.info = *w;
            for (const FrameWeapons::Part& part : w->parts) {
                ModelFile file;
                std::string why;
                if (!LoadModelFile(dir + part.file, file, why, kModelFlagWeaponSpace)) {
                    l.error = part.file + ": " + why;
                    break;
                }
                // A texture pack's replacement, read from the pack itself (never copied).
                if (!part.replaced.empty() && (file.flags & kModelFlagFlatSurface)) {
                    std::vector<u8> rgba;
                    u32 w = 0, h = 0;
                    if (FrameWeapons::LoadPackTexture(part.replaced, part.replaced_flip, rgba, w, h)) {
                        file.colour = std::move(rgba);
                        file.tex_width = w;
                        file.tex_height = h;
                        l.pack_textures++;
                    }
                }
                l.files.push_back(std::move(file));
            }
            l.part_files = l.files.size();
            // AzaharFrame kiuflash: where the flash starts: past what of the weapon is on the
            // ray in front of its muzzle (a cannon's barrel), with the class's own fit (the right
            // hand's: the left is its mirror).
            if (!w->flash.parts.empty() && l.error.empty()) {
                std::vector<FitSide> sides;
                for (const ModelFile& f : l.files) {
                    for (const ModelComponent& comp : f.components) {
                        const FitSide side = comp.name == "R"   ? FitSide::Right
                                             : comp.name == "L" ? FitSide::Left
                                                                : FitSide::Centre;
                        if (std::find(sides.begin(), sides.end(), side) == sides.end()) {
                            sides.push_back(side);
                        }
                    }
                }
                const ::FrameXr::Vis::FitWeapon fw{w->cls, w->muzzle, w->muzzle_valid};
                const auto fits = FitWeaponToHand(fw, false, sides);
                const auto at = FitMuzzle(fw, false, sides);
                for (std::size_t i = 0; at && !fits.empty() && i < l.files.size(); i++) {
                    if (i < w->parts.size() && !w->parts[i].glow) {
                        l.flash_push =
                            std::max(l.flash_push, FlashPush(l.files[i].vertices,
                                                             fits[0].transform, *at));
                    }
                }
            }
            // AzaharFrame kiuflash: the flash cards (a file that cannot be read: no flash).
            for (const FrameWeapons::FlashPart& part : w->flash.parts) {
                if (!l.error.empty()) {
                    break;
                }
                ModelFile file;
                std::string why;
                if (!LoadModelFile(dir + part.file, file, why, kModelFlagWeaponSpace) ||
                    !(file.flags & kModelFlagFlash)) {
                    LOG_WARNING(Frontend, "xr: muzzle flash: {}: {}", part.file,
                                why.empty() ? "not a flash" : why);
                    continue;
                }
                if (!part.replaced.empty()) {
                    std::vector<u8> rgba;
                    u32 w = 0, h = 0;
                    if (FrameWeapons::LoadPackTexture(part.replaced, part.replaced_flip, rgba, w,
                                                      h, true)) {
                        FlashTextureFit(rgba, w, h); // a card needs no more
                        file.colour = std::move(rgba);
                        file.tex_width = w;
                        file.tex_height = h;
                        l.pack_textures++;
                    }
                }
                l.files.push_back(std::move(file));
            }
            l.ok = l.error.empty() && l.part_files > 0;
        } else {
            l.error = "not among the weapons made";
        }
        weapon_load_state = 2;
    });
}

void ControllerModels::UploadWeapon() {
    Gpu& g = *gpu;
    WeaponLoad& l = *weapon_load;
    auto w = std::make_unique<Gpu::Weapon>();
    w->id = l.id;
    w->generation = l.generation;
    w->info = l.info;
    w->pack_textures = l.pack_textures;
    w->flash_push = l.flash_push; // AzaharFrame kiuflash
    vk::DeviceSize total = 0;
    for (const ModelFile& m : l.files) {
        total += m.vertices.size() * 4 + m.indices.size() * 4 + m.colour.size() + m.surface.size();
    }
    g.weapon_staging = g.Buffer(total, vk::BufferUsageFlagBits::eTransferSrc,
                                vk::MemoryPropertyFlagBits::eHostVisible |
                                    vk::MemoryPropertyFlagBits::eHostCoherent,
                                &g.weapon_staging_memory);
    auto* map = static_cast<uint8_t*>(g.device.mapMemory(g.weapon_staging_memory, 0, total));
    vk::DeviceSize at = 0;
    const auto put = [&](const void* data, std::size_t size) {
        std::memcpy(map + at, data, size);
        const vk::DeviceSize offset = at;
        at += size;
        return offset;
    };
    vk::CommandBuffer& u = g.weapon_cmd;
    try {
    u.reset();
    u.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    g.staging_for_upload = g.weapon_staging;
    const auto local = vk::MemoryPropertyFlagBits::eDeviceLocal;
    for (std::size_t i = 0; i < l.files.size(); i++) {
        const ModelFile& m = l.files[i];
        Gpu::WeaponPart part;
        vk::DeviceMemory memory;
        part.vertices = g.Buffer(m.vertices.size() * 4,
                                 vk::BufferUsageFlagBits::eVertexBuffer |
                                     vk::BufferUsageFlagBits::eTransferDst,
                                 local, &memory);
        w->buffers.emplace_back(part.vertices, memory);
        part.indices = g.Buffer(m.indices.size() * 4,
                                vk::BufferUsageFlagBits::eIndexBuffer |
                                    vk::BufferUsageFlagBits::eTransferDst,
                                local, &memory);
        w->buffers.emplace_back(part.indices, memory);
        const auto voff = put(m.vertices.data(), m.vertices.size() * 4);
        const auto ioff = put(m.indices.data(), m.indices.size() * 4);
        u.copyBuffer(g.weapon_staging, part.vertices, vk::BufferCopy{voff, 0, m.vertices.size() * 4});
        u.copyBuffer(g.weapon_staging, part.indices, vk::BufferCopy{ioff, 0, m.indices.size() * 4});
        const vk::ImageView colour =
            g.UploadImage(u, put(m.colour.data(), m.colour.size()), m.tex_width, m.tex_height,
                          vk::Format::eR8G8B8A8Srgb, &w->images, &w->views);
        const vk::ImageView surface =
            g.UploadImage(u, put(m.surface.data(), m.surface.size()), m.SurfaceWidth(),
                          m.SurfaceHeight(), vk::Format::eR8G8B8A8Unorm, &w->images, &w->views);
        // The texture's own wrap modes (77 of the 108 weapons repeat their UVs).
        const uint32_t key = ModelWrapU(m.flags) | ModelWrapV(m.flags) << 4;
        vk::Sampler& sampler = g.weapon_samplers[key];
        if (!sampler) {
            const auto mode = [](uint32_t pica) {
                switch (pica) {
                case 2:
                    return vk::SamplerAddressMode::eRepeat;
                case 3:
                    return vk::SamplerAddressMode::eMirroredRepeat;
                default:
                    return vk::SamplerAddressMode::eClampToEdge;
                }
            };
            sampler = g.device.createSampler({
                .magFilter = vk::Filter::eLinear,
                .minFilter = vk::Filter::eLinear,
                .mipmapMode = vk::SamplerMipmapMode::eLinear,
                .addressModeU = mode(ModelWrapU(m.flags)),
                .addressModeV = mode(ModelWrapV(m.flags)),
                .addressModeW = vk::SamplerAddressMode::eClampToEdge,
                .maxLod = VK_LOD_CLAMP_NONE,
            });
        }
        part.set = g.device.allocateDescriptorSets(
            {.descriptorPool = g.weapon_pool, .descriptorSetCount = 1, .pSetLayouts = &g.set_layout})[0];
        w->sets.push_back(part.set);
        const std::array<vk::DescriptorImageInfo, 2> infos{{
            {sampler, colour, vk::ImageLayout::eShaderReadOnlyOptimal},
            {sampler, surface, vk::ImageLayout::eShaderReadOnlyOptimal},
        }};
        const std::array<vk::WriteDescriptorSet, 2> writes{{
            {.dstSet = part.set,
             .dstBinding = 0,
             .descriptorCount = 1,
             .descriptorType = vk::DescriptorType::eCombinedImageSampler,
             .pImageInfo = &infos[0]},
            {.dstSet = part.set,
             .dstBinding = 1,
             .descriptorCount = 1,
             .descriptorType = vk::DescriptorType::eCombinedImageSampler,
             .pImageInfo = &infos[1]},
        }};
        g.device.updateDescriptorSets(writes, {});
        part.components = m.components;
        part.glow = i < l.part_files && i < l.info.parts.size() && l.info.parts[i].glow;
        if (i < l.part_files && i < l.info.parts.size()) { // AzaharFrame kiumodels
            part.uv = l.info.parts[i].uv;
        }
        const uint64_t bytes = m.vertices.size() * 4 + m.indices.size() * 4 +
                               (m.colour.size() + m.surface.size()) * 4 / 3; // with the mips
        if (i >= l.part_files) { // AzaharFrame kiuflash: a flash's cards
            part.flash = true;
            for (const ModelComponent& comp : m.components) {
                part.card_size.push_back(
                    FlashCardSize(m.vertices, m.indices, comp.first_index, comp.index_count));
            }
            w->flash_bytes += bytes;
        }
        w->triangles += static_cast<uint32_t>(m.indices.size() / 3);
        w->gpu_bytes += bytes;
        w->parts.push_back(std::move(part));
    }
    const vk::MemoryBarrier to_draws{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead | vk::AccessFlagBits::eIndexRead,
    };
    u.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput,
                      {}, to_draws, {}, {});
    u.end();
    g.device.unmapMemory(g.weapon_staging_memory);
    map = nullptr;
    const vk::SubmitInfo submit{.commandBufferCount = 1, .pCommandBuffers = &u};
    g.queue_lock->With([&] {
        g.queue.submit(submit, g.weapon_fence);
        return 0;
    });
    } catch (...) {
        // Nothing was submitted: what was made goes.
        g.FreeWeapon(*w);
        if (map) {
            g.device.unmapMemory(g.weapon_staging_memory);
        }
        g.device.destroyBuffer(g.weapon_staging);
        g.device.freeMemory(g.weapon_staging_memory);
        g.weapon_staging = nullptr;
        g.weapon_staging_memory = nullptr;
        throw;
    }
    g.weapon_uploading = std::move(w);
}

} // namespace FrameXr::Vis
