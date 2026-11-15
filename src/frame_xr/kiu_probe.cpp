// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
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
    // 1.2.0 RC2: what the trace line shows of the Turn Zone's push reaching the game: the HID
    // gyroscope polls Override answered since the last line, the last gyro it gave, the scale.
    uint32_t gyro_polls = 0;
    Common::Vec3f last_gyro{};
    float last_scale = 1.0f;
    bool direct_turn = true; ///< the Turn Zone writes past the box (not the mod's gyro)
    // 1.2.0 RC2: the mod's stick counter (state + kModStickCounter) and flags (+kModFlags) as
    // last read, and how often the counter was cleared since the last trace line.
    uint32_t mod_counter = 0;
    uint8_t mod_flags = 0;
    uint32_t counter_resets = 0;
    // 1.2.0 RC2 (the owner: after the C-Stick the Turn Zone stopped for good; the trace showed the
    // game no longer pulling a pixel written past the box back to its edge): turning polls in a row
    // that read the reticle still past the box; at kStretchedPolls the box counts as stretched
    // (the mod's whole screen) and the pixel goes past the screen's edge instead.
    bool stretched = false;
    bool was_turning = false;
    int64_t turn_start_ns = 0;
    bool clamp_seen = false;   ///< the game's clamp flag seen since this turn started
    uint32_t clamp_word = 0;   ///< the reticle's +kReticleClampFlags as last read
    // 1.2.0 RC2 (the owner's 701 run: after the C-Stick the Turn Zone never turned again until
    // the battle was left): the mod's reticle easing block (kModEaseFromState) as last read, and
    // how often a live test switch cleared it or the reticle's bit 2 since the last trace line.
    uint32_t ease_on_a = 0;    ///< block +0x24
    uint32_t ease_on_b = 0;    ///< block +0x28
    uint32_t ease_timer = 0;   ///< block +0x4C (float bits)
    uint32_t mod_hold = 0;     ///< the mod's state +0x2C
    uint32_t unstick_writes = 0;
    // The mod's box hook (0xF5A118) stretches the box to the whole screen while its stick counter
    // (+kModStickCounter) is set and any of these is non-zero: the state's +0x40, +0x44, +0x64.
    // The stick it reads is +0x0C, +0x10 (floats).
    uint32_t mod_40 = 0, mod_44 = 0, mod_64 = 0, mod_0c = 0, mod_10 = 0;
    std::array<uint32_t, 32> last_dump{}; ///< the reticle's +0x80 words last logged (trace on)
    // The mod's held-stylus latches (state +kModTouchHeld, +kModStretchOn) as last read, when the
    // C-Stick last steered, and how often they were released since the last trace line.
    uint32_t latch_touch = 0;
    uint32_t latch_stretch = 0;
    int64_t stick_seen_ns = 0;
    uint32_t latch_releases = 0;
};

/// The reticle object's flags; kClampBit (value 8, MAPPING.md's "bit 8") is set by the game's clamp (0x6D2C08) when it pulls
/// the reticle back into the box and turns by the rest (MAPPING.md, the Frame mock's C-Stick run).
constexpr uint32_t kReticleClampFlags = 0x84;
constexpr uint32_t kClampBit = 0x08; // the owner's 698 trace: 0x41 at rest, 0x49 while the game turns

// 1.2.0 RC2 (the owner: touching the C-Stick stopped the Turn Zone for good): the mod keeps a
// counter of its stick (MAPPING.md: 0xF35D88 with the state at 0xF35D40) and, while it is set,
// stretches the reticle's box to the whole screen (0xF45450, 0xF5B118), so a pixel past the
// game's box is no longer clamped and nothing turns. Its flags (0xF35D74) are logged with it.
constexpr uint32_t kModStickCounter = 0x48;
constexpr uint32_t kModFlags = 0x34;
// 1.2.0 RC2: before the game's clamp turns the camera, the mod (0xF3BA64 -> 0xF3ABF8 ->
// 0xF3AA44) answers "do not turn" while it eases the reticle itself: its block at 0xF3E638 (the
// state's address + kModEaseFromState) holds the reticle (+0x1C), two on flags (+0x24, +0x28) and
// a timer (+0x4C, under 0.2 s while it eases); it also answers so while the state's +0x2C is set.
// The owner's 701 trace: after the C-Stick the reticle's +0x84 kept bit 2 (0x43, was 0x41) and
// the game's clamp flag never showed again. Which of these sticks is what the trace shows.
constexpr uint32_t kModEaseFromState = 0xF3E638 - 0xF35D40;
constexpr uint32_t kModHold = 0x2C;
constexpr uint32_t kReticleStickBit = 0x02;
// 1.2.0 RC2 (the owner: after the C-Stick, edge turning stops until the battle is left): when
// its stick steers, the mod sets two words of its state to 1 (0xF45230, 0xF45234) and clears
// them only when the battle (its pad object) changes (0xF45048). The first holds a stylus touch
// down for good (0xF4523C: the game's touch, which sets the reticle's bit 2 every frame,
// 0x4D201C); with the second, the mod's box wrapper (0xF453E4 -> 0xF5A038) stretches the game's
// aim box to take in the reticle, so a pixel past the box's edge is inside it and nothing turns.
// Before the first C-Stick both are 0, which is the state the Turn Zone works in.
constexpr uint32_t kModTouchHeld = 0x04;
constexpr uint32_t kModStretchOn = 0x24;
/// The C-Stick let go for this long before the latches are released (no flick on the lift).
constexpr int64_t kLatchReleaseAfterNs = 150'000'000;

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
    // 1.2.0 RC2 (the owner: Direct Aim cannot aim at the ground as the C-Stick can): live test
    // switch AZAHARFRAME_KIU_GROUND_AIM=<factor> (0.3 to 1.0, e.g. 0.9; "1" means 0.9): on the
    // ground, the aim box's bottom (+kReticleBoxBottom, of the centre's y, which the game writes
    // for its state, 0.25 to 0.8) is written at least this low, so pointing low on the screen aims
    // there and only past the screen's bottom edge turns the camera down. Whether the game keeps
    // the value between its frames is what the trace's box shows.
    if (reading.ground) {
        if (const auto v = Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_GROUND_AIM")) {
            char* end = nullptr;
            float f = std::strtof(v->c_str(), &end);
            if (*v == "1") {
                f = 0.9f;
            }
            if (end != v->c_str() && f >= 0.3f && f <= 1.0f && reading.box_bottom < f) {
                Core::System& system = Core::System::GetInstance();
                for (const std::shared_ptr<Kernel::Process>& process :
                     system.Kernel().GetProcessList()) {
                    if (process.get() == p.process) {
                        system.Memory().Write32(*process, p.object + kReticleBoxBottom,
                                                FloatBits(f));
                        reading.box_bottom = f;
                        break;
                    }
                }
            }
        }
    }
    const int64_t now = static_cast<int64_t>(static_cast<double>(ticks) * 1'000'000'000.0 /
                                             static_cast<double>(BASE_CLOCK_RATE_ARM11));
    const Input::DirectAim::Out out =
        p.aim.Update(now, reading, target,
                     Input::DirectAim::Turn{frame.zone_fraction, frame.zone_from, frame.turn_speed,
                                            frame.turn_speed_vertical}, // AzaharFrame fixa
                     frame.clutch || frame.stick);
    // 1.2.0 RC2 (the owner): the Turn Zone turns the game's camera itself, not through the mod's
    // gyro. The game's clamp (0x6D2C08) turns the camera by what it clamps off the reticle past
    // the aim box, so the pixel is written past the box's edge by one game frame of the push
    // (kTurnPxPerDeg x the push's degrees a second / 60: one screen width a second at full depth
    // and Turn Speed 1.0x, as the mod's drag was); the mod's gyro gets zero. Live test switch
    // AZAHARFRAME_KIU_TURN=mod: the push goes to the mod's gyro as before.
    const bool direct_turn =
        Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_TURN").value_or("direct") != "mod";
    Input::DirectAim::Out write = out;
    {
        Memory::MemorySystem& memory = Core::System::GetInstance().Memory();
        for (const std::shared_ptr<Kernel::Process>& process :
             Core::System::GetInstance().Kernel().GetProcessList()) {
            if (process.get() != p.process) {
                continue;
            }
            const uint32_t counter_at = p.addr.state + kModStickCounter;
            if (memory.IsValidVirtualAddress(*process, counter_at) &&
                memory.IsValidVirtualAddress(*process, counter_at + 3) &&
                memory.IsValidVirtualAddress(*process, p.addr.state + kModFlags)) {
                p.mod_counter = memory.Read32(*process, counter_at);
                if (memory.IsValidVirtualAddress(*process, p.object + kReticleClampFlags + 3)) {
                    p.clamp_word = memory.Read32(*process, p.object + kReticleClampFlags);
                }
                p.mod_flags = memory.Read8(*process, p.addr.state + kModFlags);
                // The Turn Zone wants a turn, the stick is let go and the counter is still set:
                // clear it, so the game's box (and its turn) is back. Live test switch
                // AZAHARFRAME_KIU_STICK_RESET=0 leaves it alone.
                const bool reset = Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_STICK_RESET")
                                       .value_or("1") != "0";
                if (reset && direct_turn && out.turning && !frame.stick && !frame.clutch &&
                    p.mod_counter != 0) {
                    memory.Write32(*process, counter_at, 0);
                    p.counter_resets++;
                }
                const uint32_t ease = p.addr.state + kModEaseFromState;
                const bool ease_ok = memory.IsValidVirtualAddress(*process, ease + 0x24) &&
                                     memory.IsValidVirtualAddress(*process, ease + 0x4C + 3);
                if (ease_ok) {
                    p.ease_on_a = memory.Read32(*process, ease + 0x24);
                    p.ease_on_b = memory.Read32(*process, ease + 0x28);
                    p.ease_timer = memory.Read32(*process, ease + 0x4C);
                }
                p.mod_hold = memory.Read32(*process, p.addr.state + kModHold);
                // Release the mod's latches once its stick has been let go for
                // kLatchReleaseAfterNs (its stick, +0x0C and +0x10, back at zero and the right
                // stick not steering). Live test switch AZAHARFRAME_KIU_STICK_LATCH=0 leaves them.
                if (memory.IsValidVirtualAddress(*process, p.addr.state + kModStretchOn + 3)) {
                    p.latch_touch = memory.Read32(*process, p.addr.state + kModTouchHeld);
                    p.latch_stretch = memory.Read32(*process, p.addr.state + kModStretchOn);
                    const uint32_t sx = memory.Read32(*process, p.addr.state + 0x0C);
                    const uint32_t sy = memory.Read32(*process, p.addr.state + 0x10);
                    const bool steering = frame.stick || (sx & 0x7FFFFFFFu) != 0 ||
                                          (sy & 0x7FFFFFFFu) != 0;
                    if (steering || p.stick_seen_ns == 0) {
                        p.stick_seen_ns = host_now;
                    }
                    const bool release =
                        Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_STICK_LATCH")
                            .value_or("1") != "0";
                    if (release && !steering &&
                        host_now - p.stick_seen_ns > kLatchReleaseAfterNs &&
                        (p.latch_touch != 0 || p.latch_stretch != 0)) {
                        memory.Write32(*process, p.addr.state + kModTouchHeld, 0);
                        memory.Write32(*process, p.addr.state + kModStretchOn, 0);
                        p.latch_touch = p.latch_stretch = 0;
                        p.latch_releases++;
                    }
                }
                const bool wide_ok =
                    memory.IsValidVirtualAddress(*process, p.addr.state + 0x64 + 3);
                if (wide_ok) {
                    p.mod_0c = memory.Read32(*process, p.addr.state + 0x0C);
                    p.mod_10 = memory.Read32(*process, p.addr.state + 0x10);
                    p.mod_40 = memory.Read32(*process, p.addr.state + 0x40);
                    p.mod_44 = memory.Read32(*process, p.addr.state + 0x44);
                    p.mod_64 = memory.Read32(*process, p.addr.state + 0x64);
                }
                // Live test switches, each only while the Turn Zone turns with the C-Stick let go
                // (the mod's flag 1 off): AZAHARFRAME_KIU_UNSTICK_BIT=1 clears the reticle's bit 2;
                // AZAHARFRAME_KIU_UNSTICK_EASE=1 clears the easing block's on flags.
                const bool unstick_time = direct_turn && out.turning && !frame.stick &&
                                          !frame.clutch && (p.mod_flags & 1) == 0;
                if (unstick_time &&
                    Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_UNSTICK_BIT").value_or("") ==
                        "1" &&
                    (p.clamp_word & kReticleStickBit) != 0) {
                    memory.Write32(*process, p.object + kReticleClampFlags,
                                   p.clamp_word & ~kReticleStickBit);
                    p.unstick_writes++;
                }
                // AZAHARFRAME_KIU_UNSTICK_MOD=1: zero the box hook's three values and the counter.
                if (unstick_time && wide_ok &&
                    Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_UNSTICK_MOD").value_or("") ==
                        "1" &&
                    (p.mod_40 != 0 || p.mod_44 != 0 || p.mod_64 != 0)) {
                    memory.Write32(*process, p.addr.state + 0x40, 0);
                    memory.Write32(*process, p.addr.state + 0x44, 0);
                    memory.Write32(*process, p.addr.state + 0x64, 0);
                    memory.Write32(*process, p.addr.state + kModStickCounter, 0);
                    p.unstick_writes++;
                }
                if (unstick_time && ease_ok &&
                    Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_UNSTICK_EASE")
                            .value_or("") == "1" &&
                    (p.ease_on_a != 0 || p.ease_on_b != 0)) {
                    memory.Write32(*process, ease + 0x24, 0);
                    memory.Write32(*process, ease + 0x28, 0);
                    p.unstick_writes++;
                }
            }
            break;
        }
    }
    if (direct_turn && out.write && out.turning) {
        constexpr float kTurnPxPerDeg = 12.0f;
        constexpr float kGameFrameS = 1.0f / 60.0f;
        constexpr float kMaxOvershootPx = 240.0f;
        // The owner's first run: a pixel past the side edges turns the camera far less than one
        // past the top or bottom (sideways barely turned at 4.0x, up and down felt right at
        // 1.0x), so each axis has its own strength. Live test switches, for tuning in the
        // headset: AZAHARFRAME_KIU_TURN_GAIN_X (sideways, default kTurnGainX), _Y (default 1).
        const auto gain = [](const char* name, float fallback) {
            const auto v = Common::FrameTestSwitches::Value(name);
            if (!v) {
                return fallback;
            }
            char* end = nullptr;
            const float f = std::strtof(v->c_str(), &end);
            return end != v->c_str() && std::isfinite(f) && f > 0.0f && f <= 50.0f ? f : fallback;
        };
        constexpr float kTurnGainX = 4.0f;
        const float gx = gain("AZAHARFRAME_KIU_TURN_GAIN_X", kTurnGainX);
        const float gy = gain("AZAHARFRAME_KIU_TURN_GAIN_Y", 1.0f);
        const float ox = std::clamp(out.right_dps * kTurnPxPerDeg * kGameFrameS * gx,
                                    -kMaxOvershootPx, kMaxOvershootPx);
        const float oy = std::clamp(out.down_dps * kTurnPxPerDeg * kGameFrameS * gy,
                                    -kMaxOvershootPx, kMaxOvershootPx);
        // The game's clamp flag is still read for the trace (clamp_seen).
        if (!p.was_turning) {
            p.was_turning = true;
            p.turn_start_ns = host_now;
            p.clamp_seen = false;
            p.stretched = false;
        }
        if ((p.clamp_word & kClampBit) != 0) {
            p.clamp_seen = true;
        }
        // The owner's 704 run (the reticle's +0x80 to +0x100 logged before and after): the only
        // lasting change the C-Stick leaves is the reticle's bit 2 (+0x84: 0x41 -> 0x43, back to
        // 0x41 when the battle is left; the game sets it again every frame). With it set the game
        // no longer turns at its box's edge, only past the screen's edge (699, 701: 0x4B there).
        // So with bit 2 set the pixel goes past the screen's edge by the overshoot x live test
        // switch AZAHARFRAME_KIU_STRETCH_GAIN (sideways, default 0.25) and _GAIN_Y (up and down,
        // default 0.25). The owner's 706 run: past the screen's edge the game turns by the whole
        // way from its box's edge, a spin at any strength, so this is off unless
        // AZAHARFRAME_KIU_STRETCH_EDGE=1; the mod's latches are released instead (above).
        p.stretched = (p.clamp_word & kReticleStickBit) != 0;
        const bool screen_edge =
            p.stretched && Common::FrameTestSwitches::Value("AZAHARFRAME_KIU_STRETCH_EDGE")
                                   .value_or("0") == "1";
        if (screen_edge) {
            const float sg = gain("AZAHARFRAME_KIU_STRETCH_GAIN", 0.25f);
            const float sgy = gain("AZAHARFRAME_KIU_STRETCH_GAIN_Y", 0.25f);
            if (ox > 0.0f) {
                write.x = reading.w + ox * sg;
            } else if (ox < 0.0f) {
                write.x = ox * sg;
            } else {
                write.x += ox;
            }
            if (oy > 0.0f) {
                write.y = reading.h + oy * sgy;
            } else if (oy < 0.0f) {
                write.y = oy * sgy;
            } else {
                write.y += oy;
            }
        } else {
            write.x += ox;
            write.y += oy;
        }
    } else {
        p.was_turning = false;
        p.stretched = false;
    }
    p.direct_turn = direct_turn;
    if (write.write && (write.x != reading.x || write.y != reading.y)) {
        // Only these two floats of the object the guards just checked; the game reads them at
        // its next frame and adds its own moves.
        Core::System& system = Core::System::GetInstance();
        for (const std::shared_ptr<Kernel::Process>& process : system.Kernel().GetProcessList()) {
            if (process.get() == p.process) {
                system.Memory().Write32(*process, p.object + kReticleX, FloatBits(write.x));
                system.Memory().Write32(*process, p.object + kReticleY, FloatBits(write.y));
                p.writes++;
                break;
            }
        }
    }
    p.push_right = direct_turn ? 0.0f : out.right_dps;
    p.push_down = direct_turn ? 0.0f : out.down_dps;
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
                 "{:.1f}, {:.1f} to {:.1f}), ray {:.1f}, {:.1f}{}{}{}{}",
                 reading.x, reading.y, reading.w, reading.h, reading.ground ? "ground" : "air",
                 box.x0, box.x1, box.y0, box.y1, want.x, want.y,
                 // AzaharFrame kiuaim3: Keep Reticle on Screen's edge point, and how far past
                 target.edge ? fmt::format(", edge {:+d} {:+d} past {:.0f} deg{}", target.past_u,
                                           target.past_v, target.past_deg,
                                           target.resting ? " resting" : "")
                             : std::string{},
                 !target.valid ? ", off the top screen"
                 : out.write   ? fmt::format(", written {:.1f}, {:.1f}", write.x, write.y)
                               : std::string{},
                 frame.stick           ? ", C-Stick steers"
                 : frame.clutch        ? ", paused"
                 : reading.game_places ? ", the game places it"
                 : out.easing          ? ", easing back in"
                 : out.turning         ? ", turning"
                                       : "",
                 // 1.2.0 RC2: the push and what the game's gyroscope was given
                 fmt::format("; {} push {:.1f} right {:.1f} down dps, gyro {:.1f} {:.1f} {:.1f} "
                             "(scale {:.2f}, {} polls); mod flags {:02X} stick {:08X}, {} cleared, clamp {:08X}{}; "
                             "ease {:X} {:X} {:.3f}, hold {:X}, {} unstuck; mod stick {:.3f} {:.3f}, "
                             "40 {:08X} 44 {:08X} 64 {:08X}; latches {:X} {:X}, {} released",
                             p.direct_turn ? "direct" : "mod", out.right_dps, out.down_dps,
                             p.last_gyro.x, p.last_gyro.y,
                             p.last_gyro.z, p.last_scale, p.gyro_polls, p.mod_flags,
                             p.mod_counter, p.counter_resets, p.clamp_word,
                             p.stretched ? ", stretched" : "", p.ease_on_a, p.ease_on_b,
                             std::bit_cast<float>(p.ease_timer), p.mod_hold, p.unstick_writes,
                             std::bit_cast<float>(p.mod_0c), std::bit_cast<float>(p.mod_10),
                             p.mod_40, p.mod_44, p.mod_64, p.latch_touch, p.latch_stretch,
                             p.latch_releases));
        p.latch_releases = 0;
        p.unstick_writes = 0;
        p.gyro_polls = 0;
        // 1.2.0 RC2 (the owner's 703 run: none of the mod's values the box hook reads were set
        // after the C-Stick): the reticle object's +0x80 to +0x100, the words the game's turn path
        // reads, as hex, on its own line whenever a word other than the reticle's place changed.
        Core::System& system = Core::System::GetInstance();
        for (const std::shared_ptr<Kernel::Process>& process : system.Kernel().GetProcessList()) {
            if (process.get() != p.process) {
                continue;
            }
            Memory::MemorySystem& memory = system.Memory();
            if (memory.IsValidVirtualAddress(*process, p.object + 0x80) &&
                memory.IsValidVirtualAddress(*process, p.object + 0xFF)) {
                std::array<uint32_t, 32> words{};
                for (size_t i = 0; i < words.size(); ++i) {
                    words[i] = memory.Read32(*process, p.object + 0x80 + static_cast<uint32_t>(i) * 4);
                }
                // +0xA4, +0xA8 are the reticle's place: left out of the compare.
                std::array<uint32_t, 32> cmp = words;
                cmp[(0xA4 - 0x80) / 4] = cmp[(0xA8 - 0x80) / 4] = 0;
                if (cmp != p.last_dump) {
                    p.last_dump = cmp;
                    std::string hex;
                    for (size_t i = 0; i < words.size(); ++i) {
                        hex += fmt::format("{}{:08X}", i % 8 == 0 ? (i ? " | " : "") : " ", words[i]);
                    }
                    LOG_INFO(Frontend, "xr: Light Gun: reticle +80: {}", hex);
                }
            }
            break;
        }
        p.counter_resets = 0;
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
        p.last_scale = static_cast<float>(scale);
    }
    gyro = Common::Vec3f{g.x, g.y, g.z};
    p.gyro_polls++;
    p.last_gyro = gyro;
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
