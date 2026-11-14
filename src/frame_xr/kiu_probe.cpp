// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include "common/frame_emu_poll.h" // AzaharFrame kiuaim2
#include "common/frame_test_switches.h" // AzaharFrame kiufix: AZAHARFRAME_KIU_LIGHTGUN_TRACE
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/perf_stats.h"
#include "core/hle/kernel/process.h"
#include "core/memory.h"
#include "frame_xr/kiu_probe.h"
#include "frame_xr/kiu_scan.h"
#include "input_common/frame_xr.h"

// AzaharFrame kiugyro (kiu_probe.h)
namespace FrameXr::Kiu {

namespace {

std::mutex g_mutex;
LightGunFrame g_frame;       ///< under g_mutex
Input::AimSampler g_sampler; ///< under g_mutex (AzaharFrame kiuaim2)
Input::ReticleReading g_last_reading; ///< under g_mutex (AzaharFrame kiuaim2)
std::atomic<bool> g_enabled{false}; ///< AzaharFrame kiuaim2: g_frame.enabled, without the lock
std::atomic<bool> g_installed{false};
std::atomic<bool> g_locked{false};

int64_t NowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/// Emulation thread only.
struct Probe {
    const Kernel::Process* process = nullptr;
    uint64_t title = 0;
    ModAddresses addr;
    bool scanned = false;
    int64_t last_scan_ns = 0;
    uint32_t object = 0; ///< AzaharFrame kiuaim2: the reticle object of the last good read
    Input::DirectAim aim; ///< AzaharFrame kiuaim2 (was the relative loop)
    std::string reason;
    int64_t last_stats_ns = 0;
    bool stick_logged = false; // AzaharFrame kiufix2
    // AzaharFrame kiuaim2: what the last write did, for the gyroscope poll and the trace.
    bool locked = false;
    float push_right = 0.0f, push_down = 0.0f;
    Input::DirectAim::Out last_out;
    Input::LightGunTarget last_target;
    uint64_t writes = 0;
};

Probe& TheProbe() {
    static Probe probe;
    return probe;
}

float AsFloat(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

void Note(Probe& p, const std::string& reason) {
    if (reason == p.reason) {
        return;
    }
    p.reason = reason;
    if (reason == "locked") {
        LOG_INFO(Frontend, "xr: Light Gun: reticle found (mod state 0x{:08X}, options 0x{:08X})",
                 p.addr.state, p.addr.options_ptr);
    } else {
        LOG_INFO(Frontend, "xr: Light Gun: Gyro instead: {}", reason);
    }
}

bool ReadReticle(Probe& p, Input::ReticleReading& r, std::string& why) {
    Core::System& system = Core::System::GetInstance();
    if (!system.IsPoweredOn()) {
        why = "no game running";
        return false;
    }
    // The game's own process (an applet or a system module may be the current one).
    std::shared_ptr<Kernel::Process> process;
    for (const std::shared_ptr<Kernel::Process>& candidate : system.Kernel().GetProcessList()) {
        if (candidate && candidate->codeset && IsKidIcarus(candidate->codeset->program_id)) {
            process = candidate;
            break;
        }
    }
    if (!process) {
        why = "not Kid Icarus: Uprising";
        return false;
    }
    const uint64_t title = process->codeset->program_id;
    if (process.get() != p.process || title != p.title) {
        p.process = process.get();
        p.title = title;
        p.addr = {};
        p.scanned = false;
        p.aim.Reset();
        std::scoped_lock lock{g_mutex}; // AzaharFrame kiuaim2: another game's reading goes
        g_last_reading = {};
    }
    Memory::MemorySystem& memory = system.Memory();
    const auto& data = process->codeset->DataSegment();
    const auto& code = process->codeset->CodeSegment();
    const int64_t now = NowNs(); // the scan's retry is in host time
    if (!p.addr.found && (!p.scanned || now - p.last_scan_ns > 10'000'000'000)) {
        p.scanned = true;
        p.last_scan_ns = now;
        if (data.size > 0 && data.size <= 64u * 1024u * 1024u &&
            memory.IsValidVirtualAddress(*process, data.addr) &&
            memory.IsValidVirtualAddress(*process, data.addr + data.size - 1)) {
            std::vector<uint8_t> bytes(data.size);
            memory.ReadBlock(*process, data.addr, bytes.data(), bytes.size());
            p.addr = ScanForMod(bytes.data(), bytes.size(), data.addr);
        }
    }
    if (!p.addr.found) {
        why = "the C-Stick & Gyro mod's code was not found";
        return false;
    }
    const auto read32 = [&](uint32_t a, uint32_t& v) {
        if ((a & 3u) != 0 || !memory.IsValidVirtualAddress(*process, a) ||
            !memory.IsValidVirtualAddress(*process, a + 3)) {
            return false;
        }
        v = memory.Read32(*process, a);
        return true;
    };
    // AzaharFrame kiufix2: any control scheme. The mod's gyro aims in the C-Stick scheme too
    // (the owner's headset check of 2026-11-14), so the scheme is only logged (kiu_weapon.h).
    uint32_t object = 0;
    if (!read32(p.addr.state + kStateReticle, object) || object == 0) {
        why = "no reticle";
        return false;
    }
    uint32_t vtable = 0;
    if (!read32(object, vtable) || !memory.IsValidVirtualAddress(*process, object + kReticleSpan) ||
        vtable < code.addr || vtable >= data.addr + data.size) {
        why = "the reticle's address is not one";
        return false;
    }
    uint32_t bits[8]{};
    const uint32_t offsets[8]{kReticleX,  kReticleY,  kReticleW,         kReticleH,
                              kReticleCx, kReticleCy, kReticleBoxBottom, kReticleBoxTop};
    for (int i = 0; i < 8; i++) {
        if (!read32(object + offsets[i], bits[i])) {
            why = "the reticle is not readable";
            return false;
        }
    }
    r.x = AsFloat(bits[0]);
    r.y = AsFloat(bits[1]);
    r.w = AsFloat(bits[2]);
    r.h = AsFloat(bits[3]);
    r.cx = AsFloat(bits[4]);
    r.cy = AsFloat(bits[5]);
    const auto finite = [](float v) { return std::isfinite(v); };
    if (!finite(r.x) || !finite(r.y) || !finite(r.w) || !finite(r.h) || !finite(r.cx) ||
        !finite(r.cy) || r.w < 2.0f || r.w > 4096.0f || r.h < 2.0f || r.h > 4096.0f ||
        r.cx <= 0.0f || r.cx > r.w || r.cy <= 0.0f || r.cy > r.h || r.x < -r.w ||
        r.x > 2.0f * r.w || r.y < -r.h || r.y > 2.0f * r.h) {
        why = "the reticle's values are out of range";
        return false;
    }
    r.ground = memory.Read8(*process, object + kReticleGround) == 1;
    // AzaharFrame kiuaim2: the box's top and bottom as the game wrote them (0x6D163C: -0.6, and
    // 0.25 to 0.8); out of that, the defaults (the smallest box: never a write past it).
    const float bottom = AsFloat(bits[6]), top = AsFloat(bits[7]);
    r.box_bottom = std::isfinite(bottom) && bottom >= 0.2f && bottom <= 1.0f
                       ? bottom
                       : Input::ReticleReading::kBoxBottomDefault;
    r.box_top = std::isfinite(top) && top >= -0.7f && top <= -0.3f
                    ? top
                    : Input::ReticleReading::kBoxTopDefault;
    r.game_places = memory.Read8(*process, object + kReticleGamePlaces) != 0;
    r.valid = true;
    p.object = object;
    return true;
}

uint32_t FloatBits(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    return bits;
}

/// AzaharFrame kiuaim2: Direct Aim, on the emulation thread at each pad update and VBlank: reads
/// the reticle and writes the ray's pixel into it (Input::DirectAim).
void AimPoll(int64_t ticks) {
    Probe& p = TheProbe();
    if (!g_enabled.load(std::memory_order_relaxed)) {
        if (p.locked) {
            p.aim.Reset();
            p.locked = false;
            p.push_right = p.push_down = 0.0f;
            g_locked = false;
        }
        return;
    }
    LightGunFrame frame;
    Input::LightGunTarget target;
    const int64_t host_now = NowNs();
    {
        std::scoped_lock lock{g_mutex};
        frame = g_frame;
        target = g_sampler.At(host_now); // the freshest XR sample, carried forward
    }
    if (!frame.enabled) {
        return;
    }
    Input::ReticleReading reading;
    std::string why;
    if (!ReadReticle(p, reading, why)) {
        Note(p, why);
        p.aim.Reset();
        p.locked = false;
        p.push_right = p.push_down = 0.0f;
        g_locked = false;
        return;
    }
    Note(p, "locked");
    {
        std::scoped_lock lock{g_mutex};
        g_last_reading = reading;
    }
    // AzaharFrame kiufix2: the C-Stick pushed pauses Direct Aim as its grip does.
    if (frame.stick != p.stick_logged) {
        p.stick_logged = frame.stick;
        LOG_DEBUG(Frontend, "xr: Light Gun: {}",
                  frame.stick ? "the C-Stick steers" : "the C-Stick let go, easing back in");
    }
    const int64_t now = static_cast<int64_t>(static_cast<double>(ticks) * 1'000'000'000.0 /
                                             static_cast<double>(BASE_CLOCK_RATE_ARM11));
    const Input::DirectAim::Out out =
        p.aim.Update(now, reading, target, frame.zone_fraction, frame.turn_speed,
                     frame.clutch || frame.stick);
    if (out.write && (out.x != reading.x || out.y != reading.y)) {
        // Only these two floats of the object the guards just checked; the game reads them at
        // its next frame and adds its own moves.
        Core::System& system = Core::System::GetInstance();
        for (const std::shared_ptr<Kernel::Process>& process : system.Kernel().GetProcessList()) {
            if (process.get() == p.process) {
                system.Memory().Write32(*process, p.object + kReticleX, FloatBits(out.x));
                system.Memory().Write32(*process, p.object + kReticleY, FloatBits(out.y));
                p.writes++;
                break;
            }
        }
    }
    p.push_right = out.right_dps;
    p.push_down = out.down_dps;
    p.last_out = out;
    p.last_target = target;
    p.locked = true;
    g_locked = true;
    // AzaharFrame kiufix: AZAHARFRAME_KIU_LIGHTGUN_TRACE=1 (tests, live): 4 lines a second;
    // AzaharFrame kiuaim2: =2, 20 a second (the Frame mock's check).
    const std::string trace =
        Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_LIGHTGUN_TRACE").value_or("");
    const int64_t every = trace == "2" ? 50'000'000 : trace == "1" ? 250'000'000 : 5'000'000'000;
    if (host_now - p.last_stats_ns > every) {
        p.last_stats_ns = host_now;
        const Input::Vec2f want = Input::ReticlePixel(reading, target.u, target.v);
        const Input::ReticleBox box = Input::ReachableBox(reading);
        LOG_INFO(Frontend,
                 "xr: Light Gun: reticle {:.1f}, {:.1f} of {:.0f}x{:.0f} ({}, box {:.1f} to "
                 "{:.1f}, {:.1f} to {:.1f}), ray {:.1f}, {:.1f}{}{}",
                 reading.x, reading.y, reading.w, reading.h, reading.ground ? "ground" : "air",
                 box.x0, box.x1, box.y0, box.y1, want.x, want.y,
                 !target.valid ? ", off the top screen"
                 : out.write   ? fmt::format(", written {:.1f}, {:.1f}", out.x, out.y)
                               : std::string{},
                 frame.stick           ? ", C-Stick steers"
                 : frame.clutch        ? ", paused"
                 : reading.game_places ? ", the game places it"
                 : out.easing          ? ", easing back in"
                 : out.turning         ? ", turning"
                                       : "");
    }
}

/// The HID gyroscope poll (~101 Hz): while Direct Aim has the reticle, the mod's gyro is the Turn
/// Zone's push, or zero; otherwise the XR thread's gyro (Gyro, the fallback) goes through.
bool Override(Common::Vec3f& gyro) {
    Probe& p = TheProbe();
    if (!g_enabled.load(std::memory_order_relaxed) || !p.locked) {
        return false;
    }
    Input::Vec3f g =
        Input::ModGyro(Input::ModRateFor(p.push_right), Input::ModRateFor(p.push_down));
    // The HID service multiplies the gyroscope by the last frame's time scale (to keep a real
    // rotation right while the emulator runs slow); these rates are in the game's own time.
    Core::System& system = Core::System::GetInstance();
    if (system.perf_stats) {
        const double scale = system.perf_stats->GetLastFrameTimeScale();
        if (scale > 0.1 && scale < 10.0) {
            g = g * static_cast<float>(1.0 / scale);
        }
    }
    gyro = Common::Vec3f{g.x, g.y, g.z};
    return true;
}

} // namespace

void Publish(const LightGunFrame& frame) {
    {
        std::scoped_lock lock{g_mutex};
        g_frame = frame;
        g_sampler.Push(NowNs(), frame.target); // AzaharFrame kiuaim2
    }
    g_enabled.store(frame.enabled, std::memory_order_relaxed);
    if (frame.enabled && !g_installed.exchange(true)) {
        InputCommon::FrameXr::GetMotion().SetGyroOverride(Override);
        Common::FrameEmuPoll::SetAim(AimPoll); // AzaharFrame kiuaim2
    }
}

void Stop() {
    std::scoped_lock lock{g_mutex};
    g_frame.enabled = false;
    g_enabled.store(false, std::memory_order_relaxed);
    g_sampler.Reset();
}

bool Locked() {
    return g_locked;
}

Input::ReticleReading LastReading() {
    std::scoped_lock lock{g_mutex};
    return g_last_reading;
}

} // namespace FrameXr::Kiu
