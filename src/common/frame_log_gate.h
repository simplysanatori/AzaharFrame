// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame integ8: when a status line that is worked out every second is written to the log.
// The Full modes' stereo and world-pose lines were written every second, always (3 to 4 MB an
// hour, into every crash report). Now each is written the first time, at once when what it says
// changes meaningfully (the caller decides what is meaningful), and otherwise as a summary at
// most every kSummaryMs. Header only: video_core and frame_xr both use it.

#pragma once

#include <cmath>
#include <cstdint>

namespace Common::FrameLog {

class ChangeGate {
public:
    static constexpr int64_t kSummaryMs = 10000;

    enum class Say {
        Nothing, ///< not now
        First,   ///< the first line (or the first after Reset)
        Changed, ///< what it says changed meaningfully
        Summary, ///< no change, kSummaryMs since the last line
    };

    /// Once per second's values, at `now_ms` (any monotonic clock). `changed`: they differ
    /// meaningfully from the ones last written.
    Say Update(int64_t now_ms, bool changed) {
        if (!have_) {
            have_ = true;
            last_ms_ = now_ms;
            return Say::First;
        }
        if (changed) {
            last_ms_ = now_ms;
            return Say::Changed;
        }
        if (now_ms - last_ms_ >= kSummaryMs) {
            last_ms_ = now_ms;
            return Say::Summary;
        }
        return Say::Nothing;
    }
    void Reset() {
        *this = ChangeGate{};
    }

private:
    bool have_ = false;
    int64_t last_ms_ = 0;
};

/// A value below 0 is "not known" (not seen, not measured). Whether `now` is meaningfully
/// different from `then`: known on one side only, or apart by more than `rel` of `then` and
/// more than `abs_min`.
inline bool Moved(float then, float now, float rel, float abs_min) {
    const bool had = then >= 0.0f, has = now >= 0.0f;
    if (had != has) {
        return true;
    }
    if (!had) {
        return false;
    }
    const float d = std::abs(now - then);
    return d > abs_min && d > rel * std::abs(then);
}

/// The words for a line's reason, at its end: "first", "changed" or "no change".
inline const char* Why(ChangeGate::Say say) {
    switch (say) {
    case ChangeGate::Say::Changed:
        return "changed";
    case ChangeGate::Say::Summary:
        return "no change";
    default:
        return "first";
    }
}

} // namespace Common::FrameLog
