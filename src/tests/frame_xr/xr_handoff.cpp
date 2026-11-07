// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <deque>
#include <optional>
#include <random>
#include <catch2/catch_test_macros.hpp>
#include "video_core/renderer_vulkan/vk_xr_handoff.h"

// AzaharFrame: the XR screen handoff between the emu thread and the handoff thread.
using Vulkan::XrHandoff;
using Path = XrHandoff::Path;
using JobKind = XrHandoff::JobKind;

namespace {

/// Runs a handoff job to its end at once (what the handoff thread does after NextJob).
void Finish(XrHandoff& h, const XrHandoff::Job& job, bool arm_ok = true) {
    switch (job.kind) {
    case JobKind::ReleaseDirect:
        h.ReleaseDone();
        break;
    case JobKind::Arm:
        h.ArmDone(arm_ok);
        break;
    case JobKind::CopySpill:
        h.CopyDone(job.slot);
        break;
    case JobKind::None:
        break;
    }
}

/// Runs jobs until there is none.
void Settle(XrHandoff& h) {
    for (XrHandoff::Job job = h.NextJob(); job.kind != JobKind::None; job = h.NextJob()) {
        Finish(h, job);
    }
}

} // namespace

TEST_CASE("XrHandoff: armed images take the frame directly, released at its tick", "[frame_xr]") {
    XrHandoff h;
    // Nothing armed at first: the first frame is spilled, never waited for.
    const auto first = h.BeginFrame(true);
    REQUIRE(first.path == Path::Spill);
    REQUIRE_FALSE(first.replaced);
    h.EndFrame(first, 10);
    REQUIRE(h.NextJob().kind == JobKind::Arm);
    h.ArmDone(true);
    const auto copy = h.NextJob();
    REQUIRE(copy.kind == JobKind::CopySpill);
    REQUIRE(copy.tick == 10);
    REQUIRE(copy.slot == first.slot);
    h.CopyDone(copy.slot);
    REQUIRE(h.NextJob().kind == JobKind::Arm);
    h.ArmDone(true);
    REQUIRE(h.NextJob().kind == JobKind::None);

    const auto direct = h.BeginFrame(true);
    REQUIRE(direct.path == Path::Direct);
    REQUIRE_FALSE(h.Armed());                   // the emu thread has the images now
    REQUIRE(h.NextJob().kind == JobKind::None); // nothing to release before EndFrame
    h.EndFrame(direct, 11);
    const auto release = h.NextJob();
    REQUIRE(release.kind == JobKind::ReleaseDirect);
    REQUIRE(release.tick == 11);
    h.ReleaseDone();
    REQUIRE(h.NextJob().kind == JobKind::Arm);
}

TEST_CASE("XrHandoff: a frame while the last is in flight is spilled, the newest wins",
          "[frame_xr]") {
    XrHandoff h;
    h.EndFrame(h.BeginFrame(true), 1); // spilled; settle to the steady state
    Settle(h);
    REQUIRE(h.Armed());

    // Frame 2 direct; the GPU is still on it when frame 3 comes.
    const auto f2 = h.BeginFrame(true);
    REQUIRE(f2.path == Path::Direct);
    h.EndFrame(f2, 2);
    const auto rel2 = h.NextJob(); // the handoff thread waits for tick 2
    REQUIRE(rel2.kind == JobKind::ReleaseDirect);

    const auto f3 = h.BeginFrame(true);
    REQUIRE(f3.path == Path::Spill); // not a wait
    h.EndFrame(f3, 3);
    // Frame 4 comes before the handoff thread got to frame 3: it takes frame 3's slot.
    const auto f4 = h.BeginFrame(true);
    REQUIRE(f4.path == Path::Spill);
    REQUIRE(f4.slot == f3.slot);
    REQUIRE(f4.replaced);
    h.EndFrame(f4, 4);

    h.ReleaseDone(); // frame 2 shown
    REQUIRE(h.NextJob().kind == JobKind::Arm);
    h.ArmDone(true);
    const auto copy = h.NextJob();
    REQUIRE(copy.kind == JobKind::CopySpill);
    REQUIRE(copy.tick == 4); // the newest frame, never frame 3

    // While frame 4 is copied, frame 5 goes to the other slot, not over the one being read.
    const auto f5 = h.BeginFrame(true);
    REQUIRE(f5.path == Path::Spill);
    REQUIRE(f5.slot != copy.slot);
    REQUIRE_FALSE(f5.replaced);
    h.EndFrame(f5, 5);
    // And no direct frame while a spilled one waits (it would be shown before an older one).
    h.CopyDone(copy.slot);
    REQUIRE(h.NextJob().kind == JobKind::Arm);
    h.ArmDone(true);
    const auto f6 = h.BeginFrame(true);
    REQUIRE(f6.path == Path::Spill);
    REQUIRE(f6.replaced); // frame 5 was still queued
    h.EndFrame(f6, 6);
    const auto copy6 = h.NextJob();
    REQUIRE(copy6.kind == JobKind::CopySpill);
    REQUIRE(copy6.tick == 6);
}

TEST_CASE("XrHandoff: pausing, draining, a failed arm and no spill slots", "[frame_xr]") {
    XrHandoff h;
    // No spill slots: a frame that cannot go directly is dropped (no output, no wait).
    REQUIRE(h.BeginFrame(false).path == Path::None);

    // A failed arm (the runtime timed out) is tried again on the next frame only.
    REQUIRE(h.NextJob().kind == JobKind::Arm);
    h.ArmDone(false);
    REQUIRE(h.NextJob().kind == JobKind::None);
    const auto f = h.BeginFrame(true);
    REQUIRE(f.path == Path::Spill);
    REQUIRE(h.NextJob().kind == JobKind::Arm);
    h.ArmDone(true);
    REQUIRE(h.NextJob().kind == JobKind::None); // the spill is still being drawn
    h.EndFrame(f, 7);

    // Paused (swapchains made again): no copy starts, the queued frame can be dropped.
    h.SetPaused(true);
    REQUIRE(h.NextJob().kind == JobKind::None);
    REQUIRE_FALSE(h.Idle());
    REQUIRE(h.DropQueued());
    REQUIRE(h.Idle());
    h.Unarm();
    REQUIRE(h.NextJob().kind == JobKind::None); // no arming while paused
    h.SetPaused(false);
    REQUIRE(h.NextJob().kind == JobKind::Arm);

    // A pending release still runs while paused, so draining ends.
    h.ArmDone(true);
    const auto d = h.BeginFrame(true);
    REQUIRE(d.path == Path::Direct);
    h.EndFrame(d, 8);
    h.SetPaused(true);
    REQUIRE(h.NextJob().kind == JobKind::ReleaseDirect);
    h.ReleaseDone();
    REQUIRE(h.Idle());
}

TEST_CASE("XrHandoff: random interleavings keep frames in order and slots safe", "[frame_xr]") {
    // A model of both threads: the emu thread presents frames, the handoff thread starts and
    // ends jobs at random moments. Checks: every frame gets a target at once, frames reach the
    // screen in increasing order, a slot being copied is never drawn into, the swapchain images
    // are never drawn into while a copy or a release holds them.
    std::mt19937 rng{1234};
    for (int run = 0; run < 200; run++) {
        XrHandoff h;
        u64 next_frame = 1;
        u64 last_shown = 0;
        std::optional<XrHandoff::Frame> drawing;
        u64 drawing_tick = 0;
        std::optional<XrHandoff::Job> job;
        std::array<bool, XrHandoff::kSlots> copying{};
        for (int step = 0; step < 400; step++) {
            const int what = static_cast<int>(rng() % 4);
            if (what == 0 && !drawing) {
                const auto f = h.BeginFrame(true);
                REQUIRE(f.path != Path::None);
                if (f.path == Path::Spill) {
                    REQUIRE_FALSE(copying[f.slot]);
                }
                if (f.path == Path::Direct) {
                    REQUIRE((!job || job->kind == JobKind::None));
                }
                drawing = f;
                drawing_tick = next_frame++;
            } else if (what == 1 && drawing) {
                h.EndFrame(*drawing, drawing_tick);
                drawing.reset();
            } else if (what == 2 && !job) {
                const auto j = h.NextJob();
                if (j.kind != JobKind::None) {
                    // No swapchain call while the emu thread draws into the waited images.
                    REQUIRE_FALSE((drawing && drawing->path == Path::Direct));
                    job = j;
                    if (j.kind == JobKind::CopySpill) {
                        copying[j.slot] = true;
                    }
                }
            } else if (what == 3 && job) {
                if (job->kind == JobKind::ReleaseDirect || job->kind == JobKind::CopySpill) {
                    REQUIRE(job->tick > last_shown);
                    last_shown = job->tick;
                }
                if (job->kind == JobKind::CopySpill) {
                    copying[job->slot] = false;
                }
                Finish(h, *job, rng() % 8 != 0);
                job.reset();
            }
        }
    }
}
