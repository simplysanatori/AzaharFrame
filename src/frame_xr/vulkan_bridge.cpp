// Copyright 2026 CitraFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <exception>
#include <dlfcn.h>

#include "common/logging/log.h"
#include "common/settings.h"
#include "core/frontend/emu_window.h"
#include "frame_xr/vulkan_bridge.h"
#include "frame_xr/xr_loader.h"
#include "video_core/renderer_vulkan/vk_instance.h"

namespace FrameXr {

namespace {

// The window Azahar's Vulkan::Instance is made for at startup: only its window
// system type matters (it picks the surface extension); no surface is made.
class BootstrapWindow final : public Frontend::EmuWindow {
public:
    explicit BootstrapWindow(Frontend::WindowSystemType type) {
        window_info.type = type;
    }
    void PollEvents() override {}
};

std::vector<std::string> Names(const char* const* names, uint32_t count) {
    std::vector<std::string> out;
    for (uint32_t i = 0; i < count; i++) {
        out.emplace_back(names[i]);
    }
    return out;
}

// Names of `wanted` not in `have`.
std::vector<std::string> Missing(const std::vector<std::string>& have,
                                 const std::vector<std::string>& wanted) {
    std::vector<std::string> out;
    for (const auto& w : wanted) {
        if (std::find(have.begin(), have.end(), w) == have.end()) {
            out.push_back(w);
        }
    }
    return out;
}

std::string Join(const std::vector<std::string>& names) {
    std::string out;
    for (const auto& n : names) {
        out += out.empty() ? n : " " + n;
    }
    return out;
}

} // namespace

VulkanBridge::VulkanBridge(XrInstance instance_, XrSystemId system_)
    : xr_instance{instance_}, xr_system{system_} {}

VulkanBridge::~VulkanBridge() {
    Destroy();
    // libvulkan stays loaded: Azahar's dispatcher may still point into it.
}

bool VulkanBridge::Init() {
    auto& xr = Xr();
    if (!xr.xrGetVulkanGraphicsRequirements2KHR || !xr.xrCreateVulkanInstanceKHR ||
        !xr.xrGetVulkanGraphicsDevice2KHR || !xr.xrCreateVulkanDeviceKHR) {
        LOG_ERROR(Frontend, "xr: the runtime has no XR_KHR_vulkan_enable2 entry points");
        return false;
    }
    XrGraphicsRequirementsVulkan2KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    const XrResult r = xr.xrGetVulkanGraphicsRequirements2KHR(xr_instance, xr_system, &req);
    if (XR_FAILED(r)) {
        LOG_ERROR(Frontend, "xr: xrGetVulkanGraphicsRequirements2KHR: {}", ResultName(r));
        return false;
    }
    LOG_INFO(Frontend, "xr: the runtime takes Vulkan {}.{} to {}.{}",
             XR_VERSION_MAJOR(req.minApiVersionSupported),
             XR_VERSION_MINOR(req.minApiVersionSupported),
             XR_VERSION_MAJOR(req.maxApiVersionSupported),
             XR_VERSION_MINOR(req.maxApiVersionSupported));
    // Pinned for the whole process: the instance and device made through it
    // outlive every Vulkan::Instance (each loads and unloads its own reference).
    libvulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!libvulkan) {
        libvulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!libvulkan) {
        LOG_ERROR(Frontend, "xr: no Vulkan loader (libvulkan.so.1)");
        return false;
    }
    gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(libvulkan, "vkGetInstanceProcAddr"));
    return gipa != nullptr;
}

bool VulkanBridge::Bootstrap(Frontend::WindowSystemType window_type) {
    try {
        BootstrapWindow window{window_type};
        Vulkan::Instance boot{window, Settings::values.physical_device.GetValue()};
        if (!device) {
            LOG_ERROR(Frontend, "xr: the Vulkan device was not made through the runtime");
            return false;
        }
        LOG_INFO(Frontend, "xr: Vulkan instance and device made through the runtime ({})",
                 boot.GetModelName());
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "xr: Vulkan bootstrap failed: {}", e.what());
        return false;
    }
    return device != VK_NULL_HANDLE;
}

VkInstance VulkanBridge::CreateInstance(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                                        const VkInstanceCreateInfo& create_info) {
    const auto wanted = Names(create_info.ppEnabledExtensionNames, create_info.enabledExtensionCount);
    if (instance != VK_NULL_HANDLE) {
        const auto missing = Missing(instance_extensions, wanted);
        if (!missing.empty()) {
            LOG_WARNING(Frontend,
                        "xr: Vulkan instance reused without the extensions asked for later: {}",
                        Join(missing));
        }
        return instance;
    }
    if (!gipa) {
        return VK_NULL_HANDLE;
    }
    XrVulkanInstanceCreateInfoKHR info{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    info.systemId = xr_system;
    // The process's own loader entry (pinned), not the Vulkan::Instance's copy.
    info.pfnGetInstanceProcAddr = gipa;
    info.vulkanCreateInfo = &create_info;
    VkResult vk_result = VK_SUCCESS;
    VkInstance made = VK_NULL_HANDLE;
    const XrResult r = Xr().xrCreateVulkanInstanceKHR(xr_instance, &info, &made, &vk_result);
    if (XR_FAILED(r) || vk_result != VK_SUCCESS) {
        LOG_ERROR(Frontend, "xr: xrCreateVulkanInstanceKHR: {} (VkResult {})", ResultName(r),
                  static_cast<int>(vk_result));
        return VK_NULL_HANDLE;
    }
    static_cast<void>(get_instance_proc_addr);
    instance = made;
    instance_extensions = wanted;
    instance_api_version =
        create_info.pApplicationInfo ? create_info.pApplicationInfo->apiVersion : 0;
    LOG_INFO(Frontend, "xr: Vulkan instance made (extensions: {})", Join(wanted));
    return instance;
}

VkPhysicalDevice VulkanBridge::PickPhysicalDevice(VkInstance instance_) {
    if (instance_ != instance) {
        return VK_NULL_HANDLE;
    }
    if (physical_device == VK_NULL_HANDLE) {
        XrVulkanGraphicsDeviceGetInfoKHR info{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        info.systemId = xr_system;
        info.vulkanInstance = instance;
        const XrResult r = Xr().xrGetVulkanGraphicsDevice2KHR(xr_instance, &info, &physical_device);
        if (XR_FAILED(r)) {
            LOG_ERROR(Frontend, "xr: xrGetVulkanGraphicsDevice2KHR: {}", ResultName(r));
            physical_device = VK_NULL_HANDLE;
        }
    }
    return physical_device;
}

VkDevice VulkanBridge::CreateDevice(VkPhysicalDevice physical_device_,
                                    const VkDeviceCreateInfo& create_info,
                                    uint32_t queue_family_index) {
    const auto wanted = Names(create_info.ppEnabledExtensionNames, create_info.enabledExtensionCount);
    if (device != VK_NULL_HANDLE) {
        if (physical_device_ != physical_device || queue_family_index != queue_family) {
            LOG_ERROR(Frontend, "xr: a later Vulkan::Instance asked for another GPU or queue "
                                "family; it gets its own device (no XR output)");
            return VK_NULL_HANDLE;
        }
        const auto missing = Missing(device_extensions, wanted);
        if (!missing.empty()) {
            LOG_WARNING(Frontend, "xr: Vulkan device reused without the extensions asked for "
                                  "later: {}",
                        Join(missing));
        }
        adoptions++;
        LOG_INFO(Frontend, "xr: Vulkan device adopted (boot {})", adoptions);
        return device;
    }
    if (physical_device_ != physical_device) {
        LOG_ERROR(Frontend, "xr: Vulkan device asked for a GPU the runtime did not pick");
        return VK_NULL_HANDLE;
    }
    XrVulkanDeviceCreateInfoKHR info{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    info.systemId = xr_system;
    info.pfnGetInstanceProcAddr = gipa;
    info.vulkanPhysicalDevice = physical_device;
    info.vulkanCreateInfo = &create_info;
    VkResult vk_result = VK_SUCCESS;
    VkDevice made = VK_NULL_HANDLE;
    const XrResult r = Xr().xrCreateVulkanDeviceKHR(xr_instance, &info, &made, &vk_result);
    if (XR_FAILED(r) || vk_result != VK_SUCCESS) {
        LOG_ERROR(Frontend, "xr: xrCreateVulkanDeviceKHR: {} (VkResult {})", ResultName(r),
                  static_cast<int>(vk_result));
        return VK_NULL_HANDLE;
    }
    device = made;
    queue_family = queue_family_index;
    device_extensions = wanted;
    LOG_INFO(Frontend, "xr: Vulkan device made (queue family {}, extensions: {})", queue_family,
             Join(wanted));
    return device;
}

bool VulkanBridge::OwnsInstance(VkInstance instance_) const {
    return instance_ != VK_NULL_HANDLE && instance_ == instance;
}

bool VulkanBridge::OwnsDevice(VkDevice device_) const {
    return device_ != VK_NULL_HANDLE && device_ == device;
}

void VulkanBridge::Destroy() {
    if (!gipa) {
        return;
    }
    if (device != VK_NULL_HANDLE) {
        auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>(gipa(instance, "vkGetDeviceProcAddr"));
        auto wait_idle = reinterpret_cast<PFN_vkDeviceWaitIdle>(gdpa(device, "vkDeviceWaitIdle"));
        auto destroy = reinterpret_cast<PFN_vkDestroyDevice>(gdpa(device, "vkDestroyDevice"));
        wait_idle(device);
        destroy(device, nullptr);
        device = VK_NULL_HANDLE;
    }
    if (instance != VK_NULL_HANDLE) {
        auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(gipa(instance, "vkDestroyInstance"));
        destroy(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
    physical_device = VK_NULL_HANDLE;
}

} // namespace FrameXr
