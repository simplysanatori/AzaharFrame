// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cstdlib>
#include <cstring>
#include "common/logging/log.h"
#include "common/stall_telemetry.h"
#include "video_core/renderer_vulkan/vk_gpu_busy.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"

namespace Vulkan {

GpuBusyTimer::GpuBusyTimer(const Instance& instance) {
    // Off by default (owner decision for 1.2.0): AZAHARFRAME_GPU_BUSY=1 turns it on. Off, no
    // query pool is made, so Begin and End write no timestamps.
    if (const char* env = std::getenv("AZAHARFRAME_GPU_BUSY"); !env || std::strcmp(env, "1") != 0) {
        return;
    }
    const vk::PhysicalDevice physical = instance.GetPhysicalDevice();
    const auto families = physical.getQueueFamilyProperties();
    const u32 family = instance.GetGraphicsQueueFamilyIndex();
    const u32 bits = family < families.size() ? families[family].timestampValidBits : 0;
    const float period = physical.getProperties().limits.timestampPeriod;
    if (bits == 0 || period <= 0.0f) {
        LOG_INFO(Render_Vulkan, "GPU busy time: the queue has no timestamps, not measured");
        return;
    }
    device = instance.GetDevice();
    try {
        pool = device.createQueryPool({
            .queryType = vk::QueryType::eTimestamp,
            .queryCount = kSlots * 2,
        });
    } catch (const vk::SystemError&) {
        pool = vk::QueryPool{};
        return;
    }
    ns_per_tick = period;
    mask = bits >= 64 ? ~u64{0} : (u64{1} << bits) - 1;
}

GpuBusyTimer::~GpuBusyTimer() {
    if (pool) {
        device.destroyQueryPool(pool);
    }
}

void GpuBusyTimer::Begin(vk::CommandBuffer cmdbuf) {
    open = -1;
    if (!pool || pending.size() >= kSlots) {
        return; // none, or every slot still on the GPU: this one goes untimed
    }
    const u32 slot = next;
    next = (next + 1) % kSlots;
    cmdbuf.resetQueryPool(pool, slot * 2, 2);
    cmdbuf.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, pool, slot * 2);
    open = static_cast<int>(slot);
}

void GpuBusyTimer::End(vk::CommandBuffer cmdbuf, u64 tick) {
    if (open < 0) {
        return;
    }
    const u32 slot = static_cast<u32>(open);
    cmdbuf.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, pool, slot * 2 + 1);
    pending.push_back({slot, tick});
    open = -1;
}

void GpuBusyTimer::Collect(const MasterSemaphore& master) {
    auto& telemetry = Common::StallTelemetry::Get();
    while (!pending.empty() && master.IsFree(pending.front().tick)) {
        const Pending p = pending.front();
        std::array<u64, 2> t{};
        if (device.getQueryPoolResults(pool, p.slot * 2, 2, sizeof(t), t.data(), sizeof(u64),
                                       vk::QueryResultFlagBits::e64) != vk::Result::eSuccess) {
            break; // not written yet: next time
        }
        pending.pop_front();
        const u64 ticks = ((t[1] & mask) - (t[0] & mask)) & mask;
        const double ns = static_cast<double>(ticks) * ns_per_tick;
        if (ns >= 0.0 && ns < 1e9) {
            telemetry.gpu_busy_ns.fetch_add(static_cast<u64>(ns), std::memory_order_relaxed);
            telemetry.gpu_busy_buffers.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

} // namespace Vulkan
