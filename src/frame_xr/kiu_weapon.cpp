// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <string>
#include <vector>
#include "common/frame_emu_poll.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frame_weapon_models.h"
#include "core/hle/kernel/process.h"
#include "core/memory.h"
#include "frame_xr/kiu_scan.h"
#include "frame_xr/kiu_weapon.h"

// AzaharFrame kiuguns (kiu_weapon.h)
namespace FrameXr::Kiu {

namespace {

std::atomic<bool> g_want_weapon{false};
std::atomic<bool> g_want_shots{false};
std::atomic<bool> g_installed{false};
std::atomic<int> g_weapon{-1};
std::atomic<bool> g_shots_ok{false};
std::atomic<uint64_t> g_shot_total{0};

constexpr int64_t kWeaponEveryNs = 250'000'000;
constexpr int64_t kShotsEveryNs = 16'000'000; // about a game frame

/// Emulation thread only.
struct Probe {
    const Kernel::Process* process = nullptr;
    uint64_t title = 0;
    bool scanned = false;
    WeaponAddresses weapon;
    ShotAddresses shots;
    WeaponSettle settle;
    ShotCounter counter;
    int64_t next_weapon_ns = 0;
    int64_t next_shots_ns = 0;
    std::string weapon_reason, shots_reason;
    int logged_weapon = -3;
};

Probe& TheProbe() {
    static Probe p;
    return p;
}

void Note(std::string& last, const std::string& reason, const char* what) {
    if (reason != last) {
        last = reason;
        LOG_INFO(Frontend, "xr: Kid Icarus: {}: {}", what, reason);
    }
}

void Scan(Probe& p, Kernel::Process& process, Memory::MemorySystem& memory) {
    p.scanned = true;
    const auto& seg = process.codeset->CodeSegment();
    const std::vector<u8>& image = process.codeset->memory;
    std::vector<u8> copy;
    const u8* code = nullptr;
    if (seg.size > 0 && seg.offset + seg.size <= image.size()) {
        code = image.data() + seg.offset; // the code set's own image: nothing read through the guest
    } else if (seg.size > 0 && seg.size <= 64u * 1024u * 1024u &&
               memory.IsValidVirtualAddress(process, seg.addr) &&
               memory.IsValidVirtualAddress(process, seg.addr + seg.size - 1)) {
        copy.resize(seg.size);
        memory.ReadBlock(process, seg.addr, copy.data(), copy.size());
        code = copy.data();
    }
    if (!code) {
        return;
    }
    p.weapon = ScanForWeapon(code, seg.size, seg.addr);
    p.shots = ScanForShots(code, seg.size, seg.addr);
    LOG_INFO(Frontend,
             "xr: Kid Icarus: code scanned: weapon record {}, shots record {}",
             p.weapon.found ? fmt::format("at [0x{:08X}] + slot * 0x370 + {}", p.weapon.ptr,
                                          p.weapon.field)
                            : std::string{"not found"},
             p.shots.found ? fmt::format("at [0x{:08X}] + 0x{:X}", p.shots.cache, p.shots.offset)
                           : std::string{"not found"});
}

void Poll(int64_t ticks) {
    const bool want_weapon = g_want_weapon.load(std::memory_order_relaxed);
    const bool want_shots = g_want_shots.load(std::memory_order_relaxed);
    Probe& p = TheProbe();
    if (!want_weapon && !want_shots) {
        if (g_weapon.load() != -1 || g_shots_ok.load()) {
            g_weapon = -1;
            g_shots_ok = false;
            p.settle.Reset();
            p.counter.Reset();
        }
        return;
    }
    const int64_t now =
        static_cast<int64_t>(static_cast<double>(ticks) * 1'000'000'000.0 /
                             static_cast<double>(BASE_CLOCK_RATE_ARM11));
    // The game's clock goes back when a state is loaded: no wait longer than a few periods.
    if (p.next_weapon_ns - now > 4 * kWeaponEveryNs) {
        p.next_weapon_ns = now;
    }
    if (p.next_shots_ns - now > 4 * kShotsEveryNs) {
        p.next_shots_ns = now;
    }
    const bool weapon_due = want_weapon && now >= p.next_weapon_ns;
    const bool shots_due = want_shots && now >= p.next_shots_ns;
    if (!weapon_due && !shots_due) {
        return;
    }
    if (weapon_due) {
        p.next_weapon_ns = now + kWeaponEveryNs;
    }
    if (shots_due) {
        p.next_shots_ns = now + kShotsEveryNs;
    }
    Core::System& system = Core::System::GetInstance();
    std::shared_ptr<Kernel::Process> process;
    for (const auto& candidate : system.Kernel().GetProcessList()) {
        if (candidate && candidate->codeset && IsKidIcarus(candidate->codeset->program_id)) {
            process = candidate;
            break;
        }
    }
    if (!process) {
        g_weapon = -1;
        g_shots_ok = false;
        return;
    }
    Memory::MemorySystem& memory = system.Memory();
    if (process.get() != p.process || process->codeset->program_id != p.title) {
        p.process = process.get();
        p.title = process->codeset->program_id;
        p.scanned = false;
        p.weapon = {};
        p.shots = {};
        p.settle.Reset();
        p.counter.Reset();
        p.logged_weapon = -3;
    }
    if (!p.scanned) {
        Scan(p, *process, memory);
    }
    const auto valid = [&](uint32_t a, uint32_t bytes) {
        return a != 0 && memory.IsValidVirtualAddress(*process, a) &&
               memory.IsValidVirtualAddress(*process, a + bytes - 1);
    };
    const auto read32 = [&](uint32_t a, uint32_t& v) {
        if ((a & 3u) != 0 || !valid(a, 4)) {
            return false;
        }
        v = memory.Read32(*process, a);
        return true;
    };

    if (weapon_due) {
        int raw = -1;
        std::string why;
        uint32_t base = 0;
        if (!p.weapon.found) {
            why = "the weapon record's code was not found";
        } else if (!read32(p.weapon.ptr, base) || base == 0 || (base & 3u) != 0 ||
                   !valid(base, kFighterRecordBytes * kFighterSlots)) {
            why = "no fighter records yet";
        } else {
            FighterRecords r;
            for (int s = 0; s < kFighterSlots; s++) {
                const uint32_t rec = base + static_cast<uint32_t>(s) * kFighterRecordBytes;
                r.kind[static_cast<std::size_t>(s)] =
                    static_cast<int16_t>(memory.Read16(*process, rec));
                r.weapon[static_cast<std::size_t>(s)] =
                    static_cast<int8_t>(memory.Read8(*process, rec + p.weapon.field));
            }
            raw = WeaponFromRecords(r);
            if (raw == -2) {
                why = "the fighter records' values are out of range";
                raw = -1;
            } else if (raw < 0 && (r.kind[0] == 1 || r.kind[0] == 2)) {
                why = r.weapon[0] < 0 ? "no weapon" : "more than one fighter (not single player)";
            } else if (raw < 0) {
                why = "no fighter";
            } else {
                why = "reading";
            }
        }
        Note(p.weapon_reason, why, "weapon");
        const int settled = p.settle.Update(now, raw);
        g_weapon = settled;
        if (settled != p.logged_weapon) {
            p.logged_weapon = settled;
            if (settled >= 0) {
                LOG_INFO(Frontend, "xr: Kid Icarus: weapon {} (a {})", settled,
                         FrameWeapons::ClassName(FrameWeapons::ClassOf(settled)));
            }
        }
    }

    if (shots_due) {
        std::string why;
        uint32_t object = 0, value = 0;
        if (!p.shots.found) {
            why = "the shots record's code was not found";
        } else if (!read32(p.shots.cache, object) || object == 0 ||
                   !read32(object + p.shots.offset, value) || value > kShotsCap) {
            why = "the shots record is not readable yet";
        }
        if (why.empty()) {
            Note(p.shots_reason, "reading", "shots");
            g_shot_total.fetch_add(p.counter.Update(value), std::memory_order_relaxed);
            g_shots_ok = true;
        } else {
            Note(p.shots_reason, why, "shots");
            p.counter.Reset();
            g_shots_ok = false;
        }
    }
}

} // namespace

void SetWanted(bool weapon, bool shots) {
    g_want_weapon.store(weapon, std::memory_order_relaxed);
    g_want_shots.store(shots, std::memory_order_relaxed);
    if ((weapon || shots) && !g_installed.exchange(true)) {
        Common::FrameEmuPoll::Set(Poll);
    }
}

int EquippedWeapon() {
    return g_want_weapon.load(std::memory_order_relaxed) ? g_weapon.load() : -1;
}

bool ShotsReadable() {
    return g_want_shots.load(std::memory_order_relaxed) && g_shots_ok.load();
}

uint64_t ShotTotal() {
    return g_shot_total.load(std::memory_order_relaxed);
}

} // namespace FrameXr::Kiu
