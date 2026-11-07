// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame: seams between the Vulkan renderer and an OpenXR frontend
// (src/frame_xr), which installs them before the first renderer is made.
// video_core only sees these interfaces, never OpenXR.
//
// VulkanHooks: the OpenXR runtime has to make the VkInstance and VkDevice
// (XR_KHR_vulkan_enable2). The frontend makes them once per process, from the
// create infos Azahar builds itself, and every later Vulkan::Instance (one per
// game boot) adopts the same handles without owning them.
//
// XrScreenSink: the three swapchains (top-left, top-right, bottom) the renderer
// draws into after its window (renderer_vulkan_xr.cpp, vk_xr_target.cpp).

#pragma once

#include <cstdint>
#include <mutex>

// vk_common.h first: it sets the Vulkan defines (no prototypes) for the whole project.
#include "video_core/renderer_vulkan/vk_common.h"

namespace VideoCore {

class VulkanHooks {
public:
    virtual ~VulkanHooks() = default;

    /// Makes (first call) or returns (later calls) the process's VkInstance.
    /// Returns VK_NULL_HANDLE when the instance cannot be made this way.
    virtual VkInstance CreateInstance(PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                                      const VkInstanceCreateInfo& create_info) = 0;

    /// The physical device the runtime wants (VK_NULL_HANDLE: no preference).
    virtual VkPhysicalDevice PickPhysicalDevice(VkInstance instance) = 0;

    /// Makes (first call) or returns (later calls) the process's VkDevice.
    virtual VkDevice CreateDevice(VkPhysicalDevice physical_device,
                                  const VkDeviceCreateInfo& create_info,
                                  uint32_t queue_family_index) = 0;

    /// True for handles made by the hooks: a Vulkan::Instance must not destroy them.
    virtual bool OwnsInstance(VkInstance instance) const = 0;
    virtual bool OwnsDevice(VkDevice device) const = 0;
};

/// The three game screens as the XR output sees them.
enum class XrScreen : uint32_t { TopLeft = 0, TopRight = 1, Bottom = 2, Count = 3 };

class XrScreenSink {
public:
    virtual ~XrScreenSink() = default;

    /// True while the XR session runs and wants game images. While true the desktop window is
    /// not drawn (the headset shows the screens; see RendererVulkan::SwapBuffers).
    virtual bool IsActive() const = 0;

    /// Changes each time the swapchains were destroyed (the session ended, VR to flat): the
    /// renderer then drops the views and framebuffers it made of their images.
    virtual uint64_t SwapchainGeneration() const = 0;

    /// The VkFormat of the swapchains (sRGB when the runtime offers it).
    virtual VkFormat SwapchainFormat() const = 0;

    /// (Re)creates the swapchain of a screen at this size if it differs. No image
    /// of that swapchain may be acquired. Returns false on failure.
    virtual bool EnsureSwapchain(XrScreen screen, uint32_t width, uint32_t height) = 0;

    /// The images of a screen's swapchain (valid until the next EnsureSwapchain).
    virtual uint32_t SwapchainImages(XrScreen screen, VkImage* images, uint32_t capacity) = 0;

    /// Acquires and waits for the next image of a screen. Returns the index, or -1
    /// (the image last released has not been, or the runtime timed out). Takes the
    /// queue lock itself: the caller must not hold the renderer's submit mutex.
    /// Only the renderer's handoff thread calls it (and Release), never the emu thread.
    virtual int32_t Acquire(XrScreen screen) = 0;

    /// Releases the image last acquired, once the GPU finished writing it
    /// (gpu_finished: the caller checked that it had). Takes the queue lock itself.
    virtual void Release(XrScreen screen, bool gpu_finished) = 0;

    /// The mutex that guards the renderer's VkQueue (Scheduler::submit_mutex), or
    /// nullptr when no renderer exists. The XR thread holds it around the OpenXR
    /// calls that may use the queue (XR_KHR_vulkan_enable2: xrBeginFrame,
    /// xrEndFrame, swapchain acquire / release).
    virtual void SetQueueMutex(std::mutex* mutex) = 0;

    /// The renderer stopped (true) or started (false) producing game images.
    virtual void SetScreensLive(bool live) = 0;

    /// Counts for the once-a-second log line.
    /// A frame found the screen's swapchain image still in use (the GPU had not finished the
    /// frame before) and was drawn into a spill slot instead of waiting.
    virtual void NoteLateRelease(XrScreen screen) = 0;
    /// Once per emulated frame given to the XR output (emu thread): its period.
    virtual void NoteEmuFrame() = 0;
    /// A spilled frame was copied into the swapchains and released (handoff thread).
    virtual void NoteSpillShown() = 0;
    /// A spilled frame was dropped before it was shown (a newer one replaced it).
    virtual void NoteSpillReplaced() = 0;
    /// The emu thread waited `us` for the XR side (only when swapchains are made again).
    virtual void NoteEmuWait(uint32_t us) = 0;
    /// The desktop window was drawn and presented, taking `cpu_us` on the emu thread.
    virtual void NoteWindowPresent(uint32_t cpu_us) = 0;
};

namespace Detail {
inline VulkanHooks*& VulkanHooksSlot() {
    static VulkanHooks* hooks = nullptr;
    return hooks;
}
inline XrScreenSink*& XrScreenSinkSlot() {
    static XrScreenSink* sink = nullptr;
    return sink;
}
} // namespace Detail

inline VulkanHooks* GetVulkanHooks() {
    return Detail::VulkanHooksSlot();
}
inline void SetVulkanHooks(VulkanHooks* hooks) {
    Detail::VulkanHooksSlot() = hooks;
}
inline XrScreenSink* GetXrScreenSink() {
    return Detail::XrScreenSinkSlot();
}
inline void SetXrScreenSink(XrScreenSink* sink) {
    Detail::XrScreenSinkSlot() = sink;
}

} // namespace VideoCore
