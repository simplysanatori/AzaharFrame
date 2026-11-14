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
LightGunFrame g_frame; ///< under g_mutex
std::atomic<bool> g_installed{false};
std::atomic<bool> g_locked{false};

int64_t NowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

/// The emulated time (the game's own clock: its frames and the HID polls are evenly spaced in
/// it, however the host runs them).
int64_t EmulatedNs(Core::System& system) {
    return static_cast<int64_t>(system.CoreTiming().GetTicks() * 1'000'000'000.0 /
                                static_cast<double>(BASE_CLOCK_RATE_ARM11));
}

/// Emulation thread only.
struct Probe {
    const Kernel::Process* process = nullptr;
    uint64_t title = 0;
    ModAddresses addr;
    bool scanned = false;
    int64_t last_scan_ns = 0;
    Input::LightGunLoop loop;
    std::string reason;
    int64_t last_stats_ns = 0;
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
        p.loop.Reset();
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
    uint32_t options = 0, scheme_word = 0;
    if (!read32(p.addr.options_ptr, options) || options == 0 ||
        !read32(options + kOptionsScheme, scheme_word)) {
        why = "the game's options are not readable";
        return false;
    }
    if (SchemeOf(scheme_word) != kSchemeGyro) {
        why = fmt::format("the game's controls are not set to gyro (scheme {})",
                          SchemeOf(scheme_word));
        return false;
    }
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
    uint32_t bits[6]{};
    const uint32_t offsets[6]{kReticleX, kReticleY, kReticleW, kReticleH, kReticleCx, kReticleCy};
    for (int i = 0; i < 6; i++) {
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
    r.valid = true;
    return true;
}

bool Override(Common::Vec3f& gyro) {
    LightGunFrame frame;
    {
        std::scoped_lock lock{g_mutex};
        frame = g_frame;
    }
    Probe& p = TheProbe();
    if (!frame.enabled) {
        p.loop.Reset();
        g_locked = false;
        return false;
    }
    Input::ReticleReading reading;
    std::string why;
    if (!ReadReticle(p, reading, why)) {
        Note(p, why);
        p.loop.Reset();
        g_locked = false;
        return false;
    }
    Note(p, "locked");
    Core::System& system = Core::System::GetInstance();
    const int64_t now = EmulatedNs(system);
    const Input::LightGunLoop::Out out = p.loop.Update(
        now, reading, frame.target, frame.zone_fraction, frame.turn_speed, frame.clutch);
    Input::Vec3f g =
        Input::ModGyro(Input::ModRateFor(out.right_dps), Input::ModRateFor(out.down_dps));
    // The HID service multiplies the gyroscope by the last frame's time scale (to keep a real
    // rotation right while the emulator runs slow); these rates are in the game's own time.
    if (system.perf_stats) {
        const double scale = system.perf_stats->GetLastFrameTimeScale();
        if (scale > 0.1 && scale < 10.0) {
            g = g * static_cast<float>(1.0 / scale);
        }
    }
    gyro = Common::Vec3f{g.x, g.y, g.z};
    g_locked = true;
    const int64_t host_now = NowNs();
    // AzaharFrame kiufix: AZAHARFRAME_KIU_LIGHTGUN_TRACE=1 (tests, live): 4 lines a second.
    const int64_t every =
        Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_LIGHTGUN_TRACE").value_or("") == "1"
            ? 250'000'000
            : 5'000'000'000;
    if (host_now - p.last_stats_ns > every) {
        p.last_stats_ns = host_now;
        LOG_INFO(Frontend,
                 "xr: Light Gun: reticle {:.0f}, {:.0f} of {:.0f}x{:.0f} ({}), target {:.3f}, "
                 "{:.3f}, gain {:.2f}, {:.2f} px/deg, game frame {:.1f} ms",
                 reading.x, reading.y, reading.w, reading.h, reading.ground ? "ground" : "air",
                 frame.target.u, frame.target.v, p.loop.gain_x(), p.loop.gain_y(),
                 p.loop.frame_s() * 1000.0f);
    }
    return true;
}

} // namespace

void Publish(const LightGunFrame& frame) {
    {
        std::scoped_lock lock{g_mutex};
        g_frame = frame;
    }
    if (frame.enabled && !g_installed.exchange(true)) {
        InputCommon::FrameXr::GetMotion().SetGyroOverride(Override);
    }
}

void Stop() {
    std::scoped_lock lock{g_mutex};
    g_frame.enabled = false;
}

bool Locked() {
    return g_locked;
}

} // namespace FrameXr::Kiu
