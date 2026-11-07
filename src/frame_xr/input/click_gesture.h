// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>

// AzaharFrame screenswap
// A stick click that is either a tap (acts when it is let go) or a hold (acts once, when the hold
// time is up). A hold never also taps. A click that starts while the stick is pushed further than
// `max_deflection` is ignored from press to release, so a free camera cannot trigger it.
namespace FrameXr::Input {

class ClickGesture {
public:
    enum class Result { None, Tap, Hold };

    /// Call once per frame. Tap comes on the frame the click is let go (if it was shorter than
    /// `hold_ns` and not ignored), Hold on the first frame at or after `hold_ns` of holding.
    Result Update(bool down, float deflection, int64_t now_ns, int64_t hold_ns,
                  float max_deflection) {
        Result result = Result::None;
        if (down && !prev_) {
            since_ns_ = now_ns;
            fired_ = false;
            ignored_ = deflection > max_deflection;
        } else if (down) {
            if (!ignored_ && !fired_ && now_ns - since_ns_ >= hold_ns) {
                fired_ = true;
                result = Result::Hold;
            }
        } else if (prev_ && !ignored_ && !fired_) {
            result = Result::Tap;
        }
        prev_ = down;
        return result;
    }

    void Reset() {
        *this = ClickGesture{};
    }

private:
    bool prev_ = false;
    bool ignored_ = false;
    bool fired_ = false;
    int64_t since_ns_ = 0;
};

} // namespace FrameXr::Input
