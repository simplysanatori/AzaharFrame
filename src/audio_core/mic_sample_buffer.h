// Copyright 2026 Azahar for Frame
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <span>
#include <vector>
#include "common/common_types.h"

namespace AudioCore {

/// AzaharFrame fixes11: the bytes a real microphone has captured and the core has not read yet.
/// It holds at most `capacity` bytes and drops the oldest when it is full, so a mic that is open
/// while nothing reads it (emulation paused) cannot grow without limit. Thread-safe: the audio
/// thread calls Push, the emulation thread calls Take.
class MicSampleBuffer {
public:
    explicit MicSampleBuffer(std::size_t capacity_bytes = 0) : capacity(capacity_bytes) {}

    void SetCapacity(std::size_t capacity_bytes) {
        std::scoped_lock lock{mutex};
        capacity = capacity_bytes;
        if (data.size() > capacity) {
            data.erase(data.begin(), data.end() - capacity);
        }
    }

    void Push(std::span<const u8> bytes) {
        std::scoped_lock lock{mutex};
        if (capacity == 0) {
            return;
        }
        if (bytes.size() >= capacity) {
            data.assign(bytes.end() - capacity, bytes.end());
            return;
        }
        if (data.size() + bytes.size() > capacity) {
            const std::size_t drop = data.size() + bytes.size() - capacity;
            data.erase(data.begin(), data.begin() + drop);
        }
        data.insert(data.end(), bytes.begin(), bytes.end());
    }

    /// Returns everything buffered, oldest first, and empties the buffer.
    std::vector<u8> Take() {
        std::scoped_lock lock{mutex};
        std::vector<u8> out;
        out.swap(data);
        return out;
    }

    std::size_t Size() const {
        std::scoped_lock lock{mutex};
        return data.size();
    }

    void Clear() {
        std::scoped_lock lock{mutex};
        data.clear();
    }

private:
    mutable std::mutex mutex;
    std::vector<u8> data;
    std::size_t capacity;
};

} // namespace AudioCore
