// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <fmt/format.h>

#include "common/logging/log.h"
#include "common/frame_flat.h"
#include "common/frame_test_switches.h" // AzaharFrame integ9
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

// AzaharFrame handoff: the renderer's handoff thread waits for an image of each screen ahead of
// time, so a swapchain may hold an acquired or waited image when it is destroyed. It goes back
// first (OpenXR runtimes may refuse to destroy a swapchain with acquired images). The caller
// holds screens_mutex, so no xrEndFrame comes in between.
void ReturnHeldImage(ScreenChain& chain, QueueLock& queue_lock) {
    auto& xr = Xr();
    if (chain.acquired && !chain.waited) {
        XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wi.timeout = 100'000'000;
        if (xr.xrWaitSwapchainImage(chain.handle, &wi) == XR_SUCCESS) {
            chain.acquired = false;
            chain.waited = true;
        }
    }
    if (chain.waited) {
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        queue_lock.With([&] { return xr.xrReleaseSwapchainImage(chain.handle, &ri); });
        chain.waited = false;
    }
}

} // namespace

Runtime::Runtime() {
    placement = DefaultScreenPlacement();
    // AzaharFrame doubling: A/B switches, fixed for the run (the renderer asks PackedTop once per
    // session, the swapchains are made to match).
    const auto env_on = [](const char* name, bool fallback) {
        const char* v = std::getenv(name);
        if (!v || !*v) {
            return fallback;
        }
        return v[0] == '1';
    };
    set_release = env_on("AZAHARFRAME_XR_SET_RELEASE", true);
    packed_top = env_on("AZAHARFRAME_XR_PACKED", true);
    LOG_INFO(Frontend, "xr: eye images: {}, {} (AZAHARFRAME_XR_PACKED, AZAHARFRAME_XR_SET_RELEASE)",
             packed_top ? "both in one swapchain" : "a swapchain each",
             set_release ? "released together" : "released one by one");
    pacing.Configure(set_release); // AzaharFrame pacing
}

Runtime::~Runtime() {
    StopThread(); // no-op after Shutdown; never destroy under a running frame loop
    Destroy();
}

bool Runtime::HasExtension(const char* name) const {
    return std::find(enabled_extensions.begin(), enabled_extensions.end(), name) !=
           enabled_extensions.end();
}

void Runtime::RequestRefreshRate() {
    auto& xr = Xr();
    if (!session || !xr.xrRequestDisplayRefreshRateFB) {
        return;
    }
    const std::optional<std::string> want =
        Common::FrameTestSwitches::Value("AZAHARFRAME_XR_REFRESH_RATE");
    const float hz = want && !want->empty() ? static_cast<float>(std::atof(want->c_str()))
                                            : start_refresh_hz;
    if (!(hz > 0.0f)) {
        return;
    }
    const XrResult rr = xr.xrRequestDisplayRefreshRateFB(session, hz);
    LOG_INFO(Frontend, "xr: AZAHARFRAME_XR_REFRESH_RATE: asked for {:.1f} Hz{}: {}", hz,
             want && !want->empty() ? "" : " (the start's)", ResultName(rr));
}

bool Runtime::CreateInstance(const StartOptions& options) {
    // AzaharFrame fdm: Valve's FDM injection layer (an implicit OpenXR layer) registers every
    // swapchain over about 1000 px high and gives the app's own Vulkan render passes of the same
    // height a gaze-based fragment density map: at 6x our screens' swapchains and Azahar's own
    // render targets are both 1440 high. Main (citra_qt.cpp) and azaharframe.sh turn it off with
    // the variable its manifest reads; AZAHARFRAME_FDM=1 leaves it on.
    const char* fdm_off = std::getenv("DISABLE_VULKAN_FDM_INJECTION_LAYER");
    LOG_INFO(Frontend, "xr: Valve's FDM injection layer: {}",
             fdm_off && *fdm_off ? "disabled (DISABLE_VULKAN_FDM_INJECTION_LAYER)"
                                 : "left on (AZAHARFRAME_FDM=1)");
    if (!LoadLoader()) {
        return false;
    }
    start_options = options; // AzaharFrame flat2
    instance_sessions = InstanceSessions{InstanceModeFrom(std::getenv("AZAHARFRAME_XR_INSTANCE"))};
    LOG_INFO(Frontend, "xr: XR instance: {}",
             instance_sessions.Mode() == InstanceMode::PerSession
                 ? "a new one for each VR session"
                 : "one for the whole run (AZAHARFRAME_XR_INSTANCE=keep)");
    if (!CreateXrInstance(false)) {
        return false;
    }

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
    want_vr = options.vr;
    ui = std::make_unique<VrUi>();
    ui->Init();
    return true;
}

// AzaharFrame flat2: the XR half of CreateInstance, also for every VR session after the first
// (RenewInstance). `renew`: the extension list chosen the first time is asked for again, and
// stays as it is (other threads read it through HasExtension).
bool Runtime::CreateXrInstance(bool renew) {
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
    if (!renew) {
        enabled_extensions = {XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
        for (const auto& name : start_options.optional_extensions) {
            if (offers(name) && !HasExtension(name.c_str())) {
                enabled_extensions.push_back(name);
            }
        }
    } else {
        for (const auto& name : enabled_extensions) {
            if (!offers(name)) {
                LOG_ERROR(Frontend, "xr: the runtime no longer offers {}", name);
                return false;
            }
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
        if (renew) {
            DestroyXrInstance();
        } else {
            Destroy();
        }
        return false;
    }
    XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
    xr.xrGetInstanceProperties(instance, &props);
    LOG_INFO(Frontend, "xr: runtime {} {}.{}.{}, extensions:{}", props.runtimeName,
             XR_VERSION_MAJOR(props.runtimeVersion), XR_VERSION_MINOR(props.runtimeVersion),
             XR_VERSION_PATCH(props.runtimeVersion), list);
    runtime_name = fmt::format("{} {}.{}.{}", props.runtimeName,
                               XR_VERSION_MAJOR(props.runtimeVersion),
                               XR_VERSION_MINOR(props.runtimeVersion),
                               XR_VERSION_PATCH(props.runtimeVersion)); // AzaharFrame imglag

    XrSystemGetInfo sgi{XR_TYPE_SYSTEM_GET_INFO};
    sgi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    r = xr.xrGetSystem(instance, &sgi, &system);
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: no headset ({})", ResultName(r));
        if (renew) {
            DestroyXrInstance();
        } else {
            Destroy();
        }
        return false;
    }
    // AzaharFrame vrqol: the room behind the screens needs the alpha blend mode.
    {
        uint32_t modes = 0;
        std::vector<XrEnvironmentBlendMode> blend;
        if (XR_SUCCEEDED(xr.xrEnumerateEnvironmentBlendModes(
                instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &modes, nullptr)) &&
            modes > 0) {
            blend.resize(modes);
            xr.xrEnumerateEnvironmentBlendModes(instance, system,
                                                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, modes,
                                                &modes, blend.data());
        }
        std::string text;
        bool alpha = false; // AzaharFrame flat2: set once known (the GUI reads it any time)
        for (const auto mode : blend) {
            text += mode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE        ? " opaque"
                    : mode == XR_ENVIRONMENT_BLEND_MODE_ADDITIVE    ? " additive"
                    : mode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND ? " alpha_blend"
                                                                    : " other";
            alpha |= mode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
        }
        alpha_blend_supported = alpha;
        LOG_INFO(Frontend, "xr: environment blend modes:{} (passthrough background {})", text,
                 alpha ? "available" : "not available");
    }

    XrSystemProperties sys{XR_TYPE_SYSTEM_PROPERTIES};
    xr.xrGetSystemProperties(instance, system, &sys);
    LOG_INFO(Frontend, "xr: system {} (max {} layers, {}x{} max swapchain)", sys.systemName,
             sys.graphicsProperties.maxLayerCount, sys.graphicsProperties.maxSwapchainImageWidth,
             sys.graphicsProperties.maxSwapchainImageHeight);
    instance_sessions.InstanceMade();
    return true;
}

bool Runtime::RenewInstance() {
    if (!instance_sessions.NeedsNewInstance()) {
        if (instance_sessions.NextSessionReused()) {
            LOG_WARNING(Frontend, "xr: a session again on the same XR instance "
                                  "(AZAHARFRAME_XR_INSTANCE=keep): SteamVR may give it no "
                                  "controllers");
        }
        return true;
    }
    if (instance) {
        DestroyXrInstance();
    }
    if (!vulkan || !vulkan->Device()) {
        return false;
    }
    LOG_INFO(Frontend, "xr: a new XR instance for this session (SteamVR gives the controllers to "
                       "the first session of an instance)");
    if (!CreateXrInstance(true)) {
        DestroyXrInstance();
        return false;
    }
    auto& xr = Xr();
    // Before xrCreateSession, once per instance (XR_KHR_vulkan_enable2), then the GPU this
    // instance wants must be the one the device was made on.
    XrGraphicsRequirementsVulkan2KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    XrResult r = xr.xrGetVulkanGraphicsRequirements2KHR
                     ? xr.xrGetVulkanGraphicsRequirements2KHR(instance, system, &req)
                     : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: xrGetVulkanGraphicsRequirements2KHR: {}", ResultName(r));
        DestroyXrInstance();
        return false;
    }
    XrVulkanGraphicsDeviceGetInfoKHR gi{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    gi.systemId = system;
    gi.vulkanInstance = vulkan->Instance();
    VkPhysicalDevice wanted = VK_NULL_HANDLE;
    r = xr.xrGetVulkanGraphicsDevice2KHR ? xr.xrGetVulkanGraphicsDevice2KHR(instance, &gi, &wanted)
                                         : XR_ERROR_FUNCTION_UNSUPPORTED;
    if (XR_FAILED(r) || wanted != vulkan->PhysicalDevice()) {
        LOG_ERROR(Frontend, "xr: the new XR instance wants another GPU ({}); no VR",
                  XR_FAILED(r) ? ResultName(r) : "a different device");
        DestroyXrInstance();
        return false;
    }
    vulkan->SetXr(instance, system);
    return true;
}

void Runtime::DestroyXrInstance() {
    auto& xr = Xr();
    if (session) {
        DestroySession(); // never an instance destroyed under its session
    }
    ForgetInstance(); // ResultName must not ask the instance being destroyed
    if (instance && xr.xrDestroyInstance) {
        const XrResult r = xr.xrDestroyInstance(instance);
        LOG_INFO(Frontend, "xr: XR instance destroyed ({})", ResultName(r));
    }
    instance = XR_NULL_HANDLE;
    instance_sessions.InstanceGone();
    lost = false; // a lost instance is gone with it: VR may ask for a new one
}

bool Runtime::CreateSession() {
    if (session) {
        return true;
    }
    auto& xr = Xr();
    XrResult r = XR_SUCCESS;
    session_ended = false;
    detaching = false;
    state = XR_SESSION_STATE_UNKNOWN;
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
        return false;
    }
    // AzaharFrame dbl: the display refresh rates the runtime offers (XR_FB_display_refresh_rate),
    // logged; AZAHARFRAME_XR_REFRESH_RATE=<Hz> (a test switch) asks for one of them.
    if (xr.xrEnumerateDisplayRefreshRatesFB && xr.xrGetDisplayRefreshRateFB) {
        uint32_t n = 0;
        std::vector<float> rates;
        if (XR_SUCCEEDED(xr.xrEnumerateDisplayRefreshRatesFB(session, 0, &n, nullptr)) && n > 0) {
            rates.resize(n);
            if (XR_FAILED(xr.xrEnumerateDisplayRefreshRatesFB(session, n, &n, rates.data()))) {
                rates.clear();
            }
        }
        float now = 0.0f;
        xr.xrGetDisplayRefreshRateFB(session, &now);
        std::string list;
        for (const float hz : rates) {
            list += fmt::format("{}{:.1f}", list.empty() ? "" : " ", hz);
        }
        LOG_INFO(Frontend, "xr: display refresh rates offered: {} Hz (now {:.1f} Hz)",
                 list.empty() ? "none listed" : list, now);
        start_refresh_hz = now;
        if (Common::FrameTestSwitches::Value("AZAHARFRAME_XR_REFRESH_RATE")) {
            RequestRefreshRate();
        }
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
        DestroySession();
        return false;
    }

    uint32_t format_count = 0;
    swapchain_format = VK_FORMAT_UNDEFINED;
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
        DestroySession();
        return false;
    }
    LOG_INFO(Frontend, "xr: session made (session {} of this XR instance); screen swapchains in "
                       "VkFormat {}",
             instance_sessions.SessionMade(), static_cast<int>(swapchain_format));

    if (ui) {
        ui->Create(*this);
    }
    hud.Create(*this); // AzaharFrame vrqol
    sent_serial = 0;                  // AzaharFrame imglag: nothing in the compositor yet
    last_display_time = 0;
    UpdateImageLag("session started"); // AzaharFrame imglag
    has_session = true;
    Settings::frame_flat_mono = false; // stereo again for the headset
    return true;
}

void Runtime::DestroySession() {
    auto& xr = Xr();
    detaching = true; // the renderer starts no new acquire from here on (IsActive)
    running = false;
    if (has_session.exchange(false)) {
        Settings::frame_flat_mono = true; // the window shows one eye: games draw one
    }
    hud.Destroy(); // AzaharFrame vrqol
    if (ui) {
        ui->DestroyXr();
    }
    {
        // Waits for a renderer swapchain call in progress (an acquire waits 100 ms at most).
        std::unique_lock chains_lock{chains_mutex};
        std::scoped_lock lock{screens_mutex};
        for (auto& chain : screens) {
            if (chain.handle && xr.xrDestroySwapchain) {
                ReturnHeldImage(chain, queue_lock);
                const XrResult r =
                    queue_lock.With([&] { return xr.xrDestroySwapchain(chain.handle); });
                if (XR_FAILED(r)) {
                    LOG_ERROR(Frontend, "xr: xrDestroySwapchain: {}", ResultName(r));
                }
            }
            chain = {};
        }
        generation++;
    }
    if (view_space && xr.xrDestroySpace) {
        xr.xrDestroySpace(view_space);
    }
    if (local_space && xr.xrDestroySpace) {
        xr.xrDestroySpace(local_space);
    }
    view_space = local_space = XR_NULL_HANDLE;
    if (session && xr.xrDestroySession) {
        const XrResult r = xr.xrDestroySession(session);
        LOG_INFO(Frontend, "xr: session destroyed ({})", ResultName(r));
    }
    session = XR_NULL_HANDLE;
    state = XR_SESSION_STATE_UNKNOWN;
    session_ended = false;
}

void Runtime::Destroy() {
    auto& xr = Xr();
    DestroySession();
    if (ui) {
        ui->Destroy();
        ui.reset();
    }
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
    return has_session.load() && running.load() && !lost.load() && !detaching.load();
}

uint64_t Runtime::SwapchainGeneration() const {
    return generation.load();
}

void Runtime::NoteWindowPresent(uint32_t cpu_us) {
    stats.window_presents++;
    stats.window_us_sum += cpu_us;
    uint32_t prev = stats.window_us_max.load();
    while (cpu_us > prev && !stats.window_us_max.compare_exchange_weak(prev, cpu_us)) {
    }
}

VkFormat Runtime::SwapchainFormat() const {
    return swapchain_format;
}

bool Runtime::EnsureSwapchain(VideoCore::XrScreen screen, uint32_t width, uint32_t height) {
    const auto i = static_cast<std::size_t>(screen);
    std::shared_lock chains_lock{chains_mutex};
    std::scoped_lock lock{screens_mutex};
    ScreenChain& chain = screens[i];
    if (chain.handle && chain.width == width && chain.height == height) {
        return true;
    }
    if (!session || detaching) {
        return false; // flat, or the session is ending
    }
    auto& xr = Xr();
    // Runtimes may use the app's queue here (layout transitions), so under the queue lock
    // (order: screens_mutex, then the queue lock, as in EndFrame).
    if (chain.handle) {
        ReturnHeldImage(chain, queue_lock);
        queue_lock.With([&] { return xr.xrDestroySwapchain(chain.handle); });
        chain = {};
    }
    XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    // Transfer dst: a frame drawn while the images were in use is copied in (vk_xr_target.cpp).
    ci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                    XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
    ci.format = swapchain_format;
    ci.sampleCount = 1;
    ci.width = width;
    ci.height = height;
    ci.faceCount = 1;
    ci.arraySize = 1;
    ci.mipCount = 1; // SteamVR on the Frame allocates level 0 only
    const XrResult r =
        queue_lock.With([&] { return xr.xrCreateSwapchain(session, &ci, &chain.handle); });
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
    std::shared_lock chains_lock{chains_mutex};
    std::scoped_lock lock{screens_mutex};
    const ScreenChain& chain = screens[static_cast<std::size_t>(screen)];
    const uint32_t n = std::min<uint32_t>(capacity, static_cast<uint32_t>(chain.images.size()));
    std::copy_n(chain.images.begin(), n, images);
    return static_cast<uint32_t>(chain.images.size());
}

int32_t Runtime::Acquire(VideoCore::XrScreen screen) {
    const auto i = static_cast<std::size_t>(screen);
    // Only the renderer acquires and releases (its handoff thread) and recreates (the emu
    // thread, with the handoff thread drained and paused): no screens_mutex here.
    ScreenChain& chain = screens[i];
    std::shared_lock chains_lock{chains_mutex}; // the swapchain stays until this returns
    if (!chain.handle || chain.waited || detaching) {
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
    std::shared_lock chains_lock{chains_mutex};
    ScreenChain& chain = screens[i];
    if (!chain.handle || !chain.waited) {
        return; // also when the session ended in between (the chain was reset)
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

void Runtime::WaitPresentSlot(uint64_t game_swaps) {
    pacing.WaitSlot(release_serial, game_swaps); // AzaharFrame pacing (this thread's serial)
}

void Runtime::ReleaseFrame(bool gpu_finished, const VideoCore::XrContentPose& content) {
    std::shared_lock chains_lock{chains_mutex};
    // Set releases: under screens_mutex (taken before the queue lock, as in the frame loop), so
    // the frame loop's layers and xrEndFrame see every screen of this game frame or none.
    std::unique_lock set_lock{screens_mutex, std::defer_lock};
    if (set_release) {
        set_lock.lock();
    } else {
        releasing++; // AzaharFrame imglag: an xrEndFrame meanwhile may carry it before its serial
    }
    const uint64_t serial = ++release_serial; // only the handoff thread releases
    for (std::size_t i = 0; i < kScreens; i++) {
        ScreenChain& chain = screens[i];
        if (!chain.handle || !chain.waited) {
            continue; // no image of this screen (the packed right eye, a session that ended)
        }
        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        const XrResult r =
            queue_lock.With([&] { return Xr().xrReleaseSwapchainImage(chain.handle, &ri); });
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrReleaseSwapchainImage {}: {}", ScreenName(i),
                      ResultName(r));
        }
        chain.waited = false;
        if (set_release) {
            chain.has_image = true;
            chain.frame_serial = serial;
        } else {
            std::scoped_lock lock{screens_mutex};
            chain.has_image = true;
            chain.frame_serial = serial;
        }
        // A packed top screen counts for both eyes (the log line keeps its meaning).
        const bool both = packed_top && i == static_cast<std::size_t>(VideoCore::XrScreen::TopLeft);
        for (std::size_t k = i; k <= (both ? i + 1 : i); k++) {
            stats.releases[k]++;
            if (!gpu_finished) {
                stats.unfinished[k]++;
            }
        }
    }
    {
        std::scoped_lock lock{content_mutex};
        shown_content = content;
        shown_pictures[shown_next] = {serial, content}; // AzaharFrame heldquad
        shown_next = (shown_next + 1) % shown_pictures.size();
    }
    if (!set_release) {
        releasing--;
    }
}

// AzaharFrame imglag: the image lag from the internal setting (vr_xr_image_lag, config.ini [VR],
// default 1: SteamVR shows an image one refresh after the pose sent with it) and the live switch
// AZAHARFRAME_XR_IMAGE_LAG=0|1 (for re-testing after a SteamVR update). Logged with the runtime
// at each session's start and when it changes.
void Runtime::UpdateImageLag(const char* why) {
    const uint32_t setting = Settings::values.vr_xr_image_lag.GetValue();
    const std::optional<uint32_t> over =
        ImageLag::OverrideFrom(Common::FrameTestSwitches::Value("AZAHARFRAME_XR_IMAGE_LAG"));
    const uint32_t lag = ImageLag::InEffect(setting, over);
    image_lag = lag;
    LOG_INFO(Frontend,
             "xr: image lag {} refresh ({}; setting vr_xr_image_lag {}{}), runtime {}: the "
             "anchored screen is placed for the picture {} (lag 1 measured on {})",
             lag, why, setting,
             over ? fmt::format(", AZAHARFRAME_XR_IMAGE_LAG={} wins", *over) : std::string{},
             runtime_name.empty() ? std::string{"unknown"} : runtime_name,
             lag ? "the last xrEndFrame carried" : "each xrEndFrame carries",
             ImageLag::kMeasuredRuntime);
    // Any SteamVR version keeps lag 1 without a word (SteamVR updates almost daily); only
    // another runtime, or none known, is warned about.
    if (lag == 1 && !ImageLag::SteamVrRuntime(runtime_name)) {
        LOG_WARNING(Frontend,
                    "xr: image lag 1 is assumed for runtime {}, which is not SteamVR (the lag was "
                    "measured on {}): if head turns double the world, check once with "
                    "AZAHARFRAME_XR_IMAGE_LAG=0 (the doubling must come back with 0 if 1 is right)",
                    runtime_name.empty() ? std::string{"unknown"} : runtime_name,
                    ImageLag::kMeasuredRuntime);
    }
    if (!set_release) {
        LOG_WARNING(Frontend,
                    "xr: AZAHARFRAME_XR_SET_RELEASE=0: releases happen outside the screens lock, "
                    "so a frame can carry an image before its serial is written; the image lag "
                    "pairing may be a picture behind then (counted as serials maybe behind)");
    }
}

bool Runtime::PackedTop() const {
    return packed_top;
}

void Runtime::SetQueueMutex(std::mutex* mutex) {
    queue_lock.Set(mutex);
}

void Runtime::SetScreensLive(bool live) {
    {
        // Nothing of the previous game is shown until this one releases an image.
        std::scoped_lock lock{screens_mutex};
        for (ScreenChain& chain : screens) {
            chain.has_image = false;
        }
    }
    screens_live = live;
    LOG_INFO(Frontend, "xr: game screens {}", live ? "live" : "gone");
}

void Runtime::NoteLateRelease(VideoCore::XrScreen screen) {
    stats.late[static_cast<std::size_t>(screen)]++;
}

void Runtime::NoteEmuFrame() {
    // Once per emulated frame: its period for the per-second summary.
    const auto now = std::chrono::steady_clock::now();
    const auto us =
        std::chrono::duration_cast<std::chrono::microseconds>(now - last_emu_frame).count();
    if (last_emu_frame.time_since_epoch().count() != 0 && us > 0 && us < 1'000'000) {
        stats.emu_frames++;
        stats.emu_frame_us_sum += static_cast<uint64_t>(us);
        uint32_t prev = stats.emu_frame_us_max.load();
        while (static_cast<uint32_t>(us) > prev &&
               !stats.emu_frame_us_max.compare_exchange_weak(prev, static_cast<uint32_t>(us))) {
        }
    }
    last_emu_frame = now;
}

void Runtime::NoteSpillShown() {
    stats.spill_shown++;
}

void Runtime::NoteSpillReplaced() {
    stats.spill_replaced++;
}

void Runtime::NoteEmuWait(uint32_t us) {
    stats.emu_waits++;
    uint32_t prev = stats.emu_wait_us_max.load();
    while (us > prev && !stats.emu_wait_us_max.compare_exchange_weak(prev, us)) {
    }
}

} // namespace FrameXr
