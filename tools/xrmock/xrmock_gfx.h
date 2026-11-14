// The graphics side of the mock OpenXR runtime (xrmock.c): everything that
// depends on the graphics API the session is bound to.  The core never
// includes a platform or graphics header; it asks the table that claims the
// session's graphics binding.
//
// Only Vulkan is implemented (xrmock_vk.c, XR_KHR_vulkan_enable2): the app's
// instance and device are made through the mock with the app's loader,
// swapchain images are VkImages on the app's device.  GalaxyFrame's OpenGL ES
// side was left out; the seam stays so another API can be added.
#pragma once

#include <openxr/openxr.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { MOCK_FMT_EYE, MOCK_FMT_UI, MOCK_FMT_MOTION, MOCK_FMT_DEPTH } MockFormatKind;

typedef struct MockGfx {
    const char* name;
    // The session's graphics binding (first struct of XrSessionCreateInfo::next
    // that this table claims).  Writes a description for the trace.
    int (*owns_binding)(XrStructureType type);
    int (*describe_binding)(const XrBaseInStructure* binding, char* out, size_t cap);
    // Entry points of the graphics extension (xrGetVulkanGraphicsRequirements2KHR ...).
    PFN_xrVoidFunction (*get_proc)(const char* name);
    // The swapchain formats of the API, and the one for each kind.
    int (*formats)(const int64_t** list);
    int64_t (*format_of)(MockFormatKind kind);
    // Images of a swapchain: `count` of them, `mips` levels.
    void* (*create_images)(int64_t format, uint64_t usage, int width, int height, int mips, int count);
    // Fills `count` XrSwapchainImage* structures (type-checked) with the images.
    XrResult (*fill_images)(void* images, int count, XrSwapchainImageBaseHeader* out);
    // At a release: how long the app's GPU work took to finish (ms; 0 when it
    // already had).  Null: not checked.
    double (*release_wait_ms)(void);
    // Reads image `index` back as tightly packed RGBA8 (w*h*4 bytes) for a
    // PNG dump.  The image is in its released layout (color attachment).
    // 0 on success, -1 when the format or the state does not allow it.
    int (*read_image)(void* images, int index, int w, int h, int64_t format, uint8_t* rgba);
    // AzaharFrame projscreen (MOCK_XR_PROJ_SCAN): copies `n` rectangles (x, y, w, h each) of
    // image `index` into a host buffer on the app's queue, without waiting: in queue order, so the
    // app may go on using the image. NULL when the format does not allow it.
    void* (*copy_regions)(void* images, int index, int64_t format, const int* rects, int n);
    // Waits for that copy and writes its pixels as RGBA8 (the rectangles one after the other) to
    // `rgba`, which holds sum(w * h * 4) bytes. 0 on success.
    int (*read_copy)(void* copy, uint8_t* rgba);
    void (*free_copy)(void* copy);
} MockGfx;

extern const MockGfx kMockGfxVk;

// AzaharFrame flat2: a new instance; the graphics requirements must be asked for again.
void mock_vk_instance_made(void);

// A violation found by a graphics table: an ERROR line in the trace, counted
// in the summary.
void mock_error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
