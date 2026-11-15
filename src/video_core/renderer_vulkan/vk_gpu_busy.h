// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame projdef: how long the GPU really works on the renderer's command buffers.
//
// The XR thread's per-second line had only "GPU behind": the time from a game frame's submit to
// its fence, which is latency (queueing behind the compositor and the XR pass included), not how
// much of the GPU the frame used. So each of the scheduler's command buffers is bracketed by two
// timestamps (top of the pipe as it starts, bottom as it ends), read back once its tick has
// signalled (never waited for), and the busy time added up (Common::StallTelemetry::gpu_busy_ns).
// The XR thread divides it by the refreshes of each second, next to the XR pass's own time
// (vis/controller_models.cpp), for the headroom per refresh (handoff report 76's fix plan, item 5).
// AZAHARFRAME_GPU_BUSY=0 turns it off. All on the scheduler's worker thread.

#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;
class MasterSemaphore;

class GpuBusyTimer {
public:
    explicit GpuBusyTimer(const Instance& instance);
    ~GpuBusyTimer();

    GpuBusyTimer(const GpuBusyTimer&) = delete;
    GpuBusyTimer& operator=(const GpuBusyTimer&) = delete;

    /// Right after `cmdbuf` began (outside a render pass): its first timestamp.
    void Begin(vk::CommandBuffer cmdbuf);
    /// Before `cmdbuf` ends, with the tick its submit signals: its last timestamp.
    void End(vk::CommandBuffer cmdbuf, u64 tick);
    /// The pairs whose tick has signalled, added up (no wait).
    void Collect(const MasterSemaphore& master);

private:
    static constexpr u32 kSlots = 64; ///< command buffers in flight that can be timed

    vk::Device device;
    vk::QueryPool pool;
    double ns_per_tick = 1.0;
    u64 mask = ~u64{0};
    u32 next = 0;
    int open = -1; ///< the slot of the command buffer being recorded, -1: not timed
    struct Pending {
        u32 slot;
        u64 tick;
    };
    std::deque<Pending> pending;
};

} // namespace Vulkan
