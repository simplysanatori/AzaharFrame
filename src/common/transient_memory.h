// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame savemem: memory held for a moment, and known to go: a save state's serialized system
// while it is made, compressed and written (core/savestate.cpp, the autosave writer), or its
// decompressed copy while it is loaded. The memory watch (frame_xr/mem_watch.h) leaves these bytes
// out of its pause rules, so a save does not pause the game for its own short, bounded spike,
// while everything else (the game, the GPU, custom textures, a leak) still counts as before, and
// it samples again soon after they are let go. Any thread.

#pragma once

#include <atomic>
#include <cstdint>
#include <utility>

namespace Common::TransientMemory {

namespace detail {
inline std::atomic<std::uint64_t> bytes{0};
inline std::atomic<std::uint32_t> holds{0};
} // namespace detail

/// What all holds have now, in bytes.
inline std::uint64_t Bytes() {
    return detail::bytes.load(std::memory_order_relaxed);
}
/// How many holds there are (a save or load is under way while it is above 0).
inline std::uint32_t Holds() {
    return detail::holds.load(std::memory_order_relaxed);
}

/// Counts `n` bytes while it lives (Set changes the count as a buffer grows). Movable.
class Hold {
public:
    Hold() = default;
    explicit Hold(std::uint64_t n) {
        detail::holds.fetch_add(1, std::memory_order_relaxed);
        active = true;
        Set(n);
    }
    ~Hold() {
        Release();
    }
    Hold(Hold&& other) noexcept
        : count(std::exchange(other.count, 0)), active(std::exchange(other.active, false)) {}
    Hold& operator=(Hold&& other) noexcept {
        if (this != &other) {
            Release();
            count = std::exchange(other.count, 0);
            active = std::exchange(other.active, false);
        }
        return *this;
    }
    Hold(const Hold&) = delete;
    Hold& operator=(const Hold&) = delete;

    void Set(std::uint64_t n) {
        if (!active) {
            return;
        }
        if (n > count) {
            detail::bytes.fetch_add(n - count, std::memory_order_relaxed);
        } else {
            detail::bytes.fetch_sub(count - n, std::memory_order_relaxed);
        }
        count = n;
    }
    void Release() {
        if (!active) {
            return;
        }
        Set(0);
        active = false;
        detail::holds.fetch_sub(1, std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t Count() const {
        return count;
    }

private:
    std::uint64_t count = 0;
    bool active = false;
};

} // namespace Common::TransientMemory
