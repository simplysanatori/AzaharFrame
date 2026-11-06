// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#include "common/logging/log.h"
#include "frame_xr/controls.h"
#include "frame_xr/session.h"
#include "frame_xr/xr_loader.h"

namespace FrameXr {

namespace {

constexpr VkFormat kFormatPreference[] = {
    // sRGB: the compositor decodes; the renderer writes linear values
    // (vk_xr_target.cpp) so the stored bytes are the game's own colours.
    VK_FORMAT_R8G8B8A8_SRGB,
    VK_FORMAT_B8G8R8A8_SRGB,
    VK_FORMAT_R8G8B8A8_UNORM,
    VK_FORMAT_B8G8R8A8_UNORM,
};

const char* ScreenName(std::size_t i) {
    static constexpr const char* names[] = {"top-left", "top-right", "bottom"};
    return names[i];
}

} // namespace

Runtime::Runtime() {
    placement = DefaultScreenPlacement();
}

Runtime::~Runtime() {
    Destroy();
}

bool Runtime::HasExtension(const char* name) const {
    return std::find(enabled_extensions.begin(), enabled_extensions.end(), name) !=
           enabled_extensions.end();
}

bool Runtime::Create(const StartOptions& options) {
    if (!LoadLoader()) {
        return false;
    }
    auto& xr = Xr();

    uint32_t count = 0;
    XrResult r = xr.xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr);
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: no OpenXR runtime ({}): is SteamVR installed and "
                            "XR_RUNTIME_JSON right?",
                  ResultName(r));
        return false;
    }
    std::vector<XrExtensionProperties> offered(count, {XR_TYPE_EXTENSION_PROPERTIES});
    xr.xrEnumerateInstanceExtensionProperties(nullptr, count, &count, offered.data());
    const auto offers = [&](const std::string& name) {
        return std::any_of(offered.begin(), offered.end(),
                           [&](const auto& p) { return name == p.extensionName; });
    };
    if (!offers(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME)) {
        LOG_ERROR(Frontend, "xr: the runtime has no Vulkan binding (XR_KHR_vulkan_enable2)");
        return false;
    }
    enabled_extensions = {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    for (const auto& name : options.optional_extensions) {
        if (offers(name) && !HasExtension(name.c_str())) {
            enabled_extensions.push_back(name);
        }
    }
    std::vector<const char*> names;
    std::string list;
    for (const auto& n : enabled_extensions) {
        names.push_back(n.c_str());
        list += " " + n;
    }

    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strncpy(ci.applicationInfo.applicationName, "Azahar for Frame",
                 XR_MAX_APPLICATION_NAME_SIZE - 1);
    ci.applicationInfo.applicationVersion = 1;
    std::strncpy(ci.applicationInfo.engineName, "Azahar", XR_MAX_ENGINE_NAME_SIZE - 1);
    ci.applicationInfo.engineVersion = 1;
    ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    ci.enabledExtensionCount = static_cast<uint32_t>(names.size());
    ci.enabledExtensionNames = names.data();
    r = xr.xrCreateInstance(&ci, &instance);
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: xrCreateInstance: {}", ResultName(r));
        instance = XR_NULL_HANDLE;
        return false;
    }
    if (!LoadInstanceFunctions(instance)) {
        Destroy();
        return false;
    }
    XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
    xr.xrGetInstanceProperties(instance, &props);
    LOG_INFO(Frontend, "xr: runtime {} {}.{}.{}, extensions:{}", props.runtimeName,
             XR_VERSION_MAJOR(props.runtimeVersion), XR_VERSION_MINOR(props.runtimeVersion),
             XR_VERSION_PATCH(props.runtimeVersion), list);

    XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    r = xr.xrGetSystem(instance, &sgi, &system);
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: no headset ({})", ResultName(r));
        Destroy();
        return false;
    }
    XrSystemProperties sys{XR_TYPE_SYSTEM_PROPERTIES};
    xr.xrGetSystemProperties(instance, system, &sys);
    LOG_INFO(Frontend, "xr: system {} (max {} layers, {}x{} max swapchain)", sys.systemName,
             sys.graphicsProperties.maxLayerCount, sys.graphicsProperties.maxSwapchainImageWidth,
             sys.graphicsProperties.maxSwapchainImageHeight);

    // Vulkan: the hooks must be live before Azahar's Instance runs (bootstrap
    // now, then every game boot adopts the same handles).
    vulkan = std::make_unique<VulkanBridge>(instance, system);
    if (!vulkan->Init()) {
        Destroy();
        return false;
    }
    VideoCore::SetVulkanHooks(vulkan.get());
    if (!vulkan->Bootstrap(options.window_type)) {
        Destroy();
        return false;
    }

    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    binding.instance = vulkan->Instance();
    binding.physicalDevice = vulkan->PhysicalDevice();
    binding.device = vulkan->Device();
    binding.queueFamilyIndex = vulkan->QueueFamily();
    binding.queueIndex = 0; // Azahar uses queue 0 of its family for everything
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &binding;
    sci.systemId = system;
    r = xr.xrCreateSession(instance, &sci, &session);
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: xrCreateSession: {}", ResultName(r));
        session = XR_NULL_HANDLE;
        Destroy();
        return false;
    }

    XrReferenceSpaceCreateInfo rs{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rs.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};
    rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    r = xr.xrCreateReferenceSpace(session, &rs, &local_space);
    if (XR_SUCCEEDED(r)) {
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        r = xr.xrCreateReferenceSpace(session, &rs, &view_space);
    }
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: xrCreateReferenceSpace: {}", ResultName(r));
        Destroy();
        return false;
    }

    uint32_t format_count = 0;
    xr.xrEnumerateSwapchainFormats(session, 0, &format_count, nullptr);
    std::vector<int64_t> formats(format_count);
    xr.xrEnumerateSwapchainFormats(session, format_count, &format_count, formats.data());
    for (const VkFormat f : kFormatPreference) {
        if (std::find(formats.begin(), formats.end(), static_cast<int64_t>(f)) != formats.end()) {
            swapchain_format = f;
            break;
        }
    }
    if (swapchain_format == VK_FORMAT_UNDEFINED) {
        LOG_ERROR(Frontend, "xr: the runtime offers no 8-bit RGBA swapchain format");
        Destroy();
        return false;
    }
    LOG_INFO(Frontend, "xr: session made; screen swapchains in VkFormat {}",
             static_cast<int>(swapchain_format));

    ui = std::make_unique<VrUi>();
    ui->Create(*this);
    return true;
}

void Runtime::Destroy() {
    auto& xr = Xr();
    if (ui) {
        ui->Destroy();
        ui.reset();
    }
    for (auto& chain : screens) {
        if (chain.handle && xr.xrDestroySwapchain) {
            xr.xrDestroySwapchain(chain.handle);
        }
        chain = {};
    }
    if (view_space && xr.xrDestroySpace) {
        xr.xrDestroySpace(view_space);
    }
    if (local_space && xr.xrDestroySpace) {
        xr.xrDestroySpace(local_space);
    }
    view_space = local_space = XR_NULL_HANDLE;
    if (session && xr.xrDestroySession) {
        xr.xrDestroySession(session);
    }
    session = XR_NULL_HANDLE;
    if (vulkan) {
        if (VideoCore::GetVulkanHooks() == vulkan.get()) {
            VideoCore::SetVulkanHooks(nullptr);
        }
        vulkan->Destroy();
        vulkan.reset();
    }
    if (instance && xr.xrDestroyInstance) {
        xr.xrDestroyInstance(instance);
    }
    instance = XR_NULL_HANDLE;
    UnloadInstanceFunctions();
}

// ---- VideoCore::XrScreenSink ---------------------------------------------------

bool Runtime::IsActive() const {
    return session != XR_NULL_HANDLE && running.load() && !lost.load();
}

VkFormat Runtime::SwapchainFormat() const {
    return swapchain_format;
}

bool Runtime::EnsureSwapchain(VideoCore::XrScreen screen, uint32_t width, uint32_t height) {
    const auto i = static_cast<std::size_t>(screen);
    std::scoped_lock lock{screens_mutex};
    ScreenChain& chain = screens[i];
    if (chain.handle && chain.width == width && chain.height == height) {
        return true;
    }
    auto& xr = Xr();
    if (chain.handle) {
        xr.xrDestroySwapchain(chain.handle);
        chain = {};
    }
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                    XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT;
    ci.format = swapchain_format;
    ci.sampleCount = 1;
    ci.width = width;
    ci.height = height;
    ci.faceCount = 1;
    ci.arraySize = 1;
    ci.mipCount = 1; // SteamVR on the Frame allocates level 0 only
    const XrResult r = xr.xrCreateSwapchain(session, &ci, &chain.handle);
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: xrCreateSwapchain {} {}x{}: {}", ScreenName(i), width, height,
                  ResultName(r));
        chain = {};
        return false;
    }
    uint32_t count = 0;
    xr.xrEnumerateSwapchainImages(chain.handle, 0, &count, nullptr);
    std::vector<XrSwapchainImageVulkan2KHR> images(count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    xr.xrEnumerateSwapchainImages(chain.handle, count, &count,
                                  reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));
    for (const auto& image : images) {
        chain.images.push_back(image.image);
    }
    chain.width = width;
    chain.height = height;
    LOG_INFO(Frontend, "xr: swapchain {} {}x{}, {} images", ScreenName(i), width, height, count);
    return true;
}

uint32_t Runtime::SwapchainImages(VideoCore::XrScreen screen, VkImage* images, uint32_t capacity) {
    std::scoped_lock lock{screens_mutex};
    const ScreenChain& chain = screens[static_cast<std::size_t>(screen)];
    const uint32_t n = std::min<uint32_t>(capacity, static_cast<uint32_t>(chain.images.size()));
    std::copy_n(chain.images.begin(), n, images);
    return static_cast<uint32_t>(chain.images.size());
}

int32_t Runtime::Acquire(VideoCore::XrScreen screen) {
    const auto i = static_cast<std::size_t>(screen);
    // Only the renderer acquires, releases and recreates, from the emu thread and
    // its release thread (which the renderer orders): no screens_mutex here.
    ScreenChain& chain = screens[i];
    if (!chain.handle || chain.waited) {
        return -1;
    }
    auto& xr = Xr();
    if (!chain.acquired) {
        XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        const XrResult r = queue_lock.With(
            [&] { return xr.xrAcquireSwapchainImage(chain.handle, &ai, &chain.index); });
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrAcquireSwapchainImage {}: {}", ScreenName(i),
                      ResultName(r));
            return -1;
        }
        chain.acquired = true;
    }
    XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    wi.timeout = 100'000'000; // 100 ms; the compositor holds an image for a refresh or two
    const XrResult r = xr.xrWaitSwapchainImage(chain.handle, &wi);
    if (r == XR_TIMEOUT_EXPIRED) {
        // Kept acquired: the next frame waits for the same image again.
        stats.skipped_acquires++;
        return -1;
    }
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: xrWaitSwapchainImage {}: {}", ScreenName(i), ResultName(r));
        return -1;
    }
    chain.acquired = false;
    chain.waited = true;
    return static_cast<int32_t>(chain.index);
}

void Runtime::Release(VideoCore::XrScreen screen, bool gpu_finished) {
    const auto i = static_cast<std::size_t>(screen);
    ScreenChain& chain = screens[i];
    if (!chain.handle || !chain.waited) {
        return;
    }
    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    const XrResult r =
        queue_lock.With([&] { return Xr().xrReleaseSwapchainImage(chain.handle, &ri); });
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: xrReleaseSwapchainImage {}: {}", ScreenName(i), ResultName(r));
    }
    chain.waited = false;
    {
        std::scoped_lock lock{screens_mutex};
        chain.has_image = true;
    }
    stats.releases[i]++;
    if (!gpu_finished) {
        stats.unfinished[i]++;
    }
}

void Runtime::SetQueueMutex(std::mutex* mutex) {
    queue_lock.Set(mutex);
}

void Runtime::SetScreensLive(bool live) {
    screens_live = live;
    LOG_INFO(Frontend, "xr: game screens {}", live ? "live" : "gone");
}

void Runtime::NoteLateRelease(VideoCore::XrScreen screen) {
    stats.late[static_cast<std::size_t>(screen)]++;
}

} // namespace FrameXr
