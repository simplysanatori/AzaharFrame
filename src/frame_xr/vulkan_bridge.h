// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// VideoCore::VulkanHooks through XR_KHR_vulkan_enable2: the runtime makes the
// VkInstance and VkDevice from Azahar's own create infos, once per process.
// Bootstrap() runs Azahar's Vulkan::Instance once at startup so the device
// exists before the XR session (which needs it) and before any game boots.

#pragma once

#include <string>
#include <vector>

#include "frame_xr/xr_includes.h"
#include "video_core/vulkan_hooks.h"

namespace Frontend {
enum class WindowSystemType : u8;
}

namespace FrameXr {

class VulkanBridge final : public VideoCore::VulkanHooks {
public:
    VulkanBridge(XrInstance instance, XrSystemId system);
    ~VulkanBridge() override;

    /// Checks the runtime's Vulkan version range and pins libvulkan. False: no Vulkan.
    bool Init();

    /// Makes the instance and device through a throwaway Vulkan::Instance.
    /// The hooks must be installed (VideoCore::SetVulkanHooks(this)) before.
    bool Bootstrap(Frontend::WindowSystemType window_type);

    /// Destroys the device and instance (after the XR session is gone).
    void Destroy();

    VkInstance Instance() const {
        return instance;
    }
    VkPhysicalDevice PhysicalDevice() const {
        return physical_device;
    }
    VkDevice Device() const {
        return device;
    }
    uint32_t QueueFamily() const {
        return queue_family;
    }

    VkInstance CreateInstance(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                              const VkInstanceCreateInfo& create_info) override;
    VkPhysicalDevice PickPhysicalDevice(VkInstance instance) override;
    VkDevice CreateDevice(VkPhysicalDevice physical_device, const VkDeviceCreateInfo& create_info,
                          uint32_t queue_family_index) override;
    bool OwnsInstance(VkInstance instance) const override;
    bool OwnsDevice(VkDevice device) const override;

private:
    XrInstance xr_instance;
    XrSystemId xr_system;
    void* libvulkan = nullptr;
    PFN_vkGetInstanceProcAddr gipa = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t queue_family = 0;
    uint32_t instance_api_version = 0;
    std::vector<std::string> instance_extensions;
    std::vector<std::string> device_extensions;
    uint32_t adoptions = 0;
};

} // namespace FrameXr
