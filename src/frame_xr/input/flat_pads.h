// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "input_common/main.h"

// AzaharFrame flat
namespace FrameXr::Input {

/// Which game controller drives Flat (citra_qt/frame_mode.cpp, ReadGamepad). Every controller
/// input_common has open is read; the one that changed last drives, and keeps driving until
/// another one changes after it did. So a pad that stops reporting while something is held (a
/// Steam Input virtual pad whose controller Steam gave to something else, or that Steam replaced
/// with a new one) is dropped, with what it held, as soon as another pad moves; and when the pad
/// in use goes away, the one that changed last (else the first) takes over. Not thread-safe.
class FlatPadPicker {
public:
    /// `states`: every connected controller now. `now_ns`: any steady clock. Returns the SDL
    /// instance id of the pad that drives, or -1 with none.
    int Update(const std::vector<InputCommon::SDLPadState>& states, int64_t now_ns) {
        seen_.erase(std::remove_if(seen_.begin(), seen_.end(),
                                   [&states](const Seen& s) { return !Has(states, s.id); }),
                    seen_.end());
        for (const auto& st : states) {
            Seen* s = Find(st.id);
            if (!s) {
                // New: it has not changed yet, so it takes over from no pad in use.
                seen_.push_back({st.id, st, 0, false});
            } else if (!Same(s->last, st)) {
                s->last = st;
                s->changed = now_ns;
                s->moved = true;
            }
        }
        const Seen* newest = nullptr;
        for (const Seen& s : seen_) {
            if (s.moved && (!newest || s.changed > newest->changed)) {
                newest = &s;
            }
        }
        const Seen* active = Find(active_);
        if (!active) {
            active_ = newest ? newest->id : (seen_.empty() ? -1 : seen_.front().id);
        } else if (newest && newest->id != active_ &&
                   (!active->moved || newest->changed > active->changed)) {
            active_ = newest->id;
        }
        return active_;
    }

    int Active() const {
        return active_;
    }

    /// More than noise apart (an axis by more than 0.02, or any button).
    static bool Same(const InputCommon::SDLPadState& a, const InputCommon::SDLPadState& b) {
        if (a.buttons != b.buttons) {
            return false;
        }
        for (std::size_t i = 0; i < a.axes.size(); i++) {
            if (std::abs(a.axes[i] - b.axes[i]) > 0.02f) {
                return false;
            }
        }
        return true;
    }

private:
    struct Seen {
        int id = -1;
        InputCommon::SDLPadState last;
        int64_t changed = 0;
        bool moved = false; ///< changed at least once since it came
    };

    static bool Has(const std::vector<InputCommon::SDLPadState>& states, int id) {
        return std::any_of(states.begin(), states.end(),
                           [id](const auto& st) { return st.id == id; });
    }
    Seen* Find(int id) {
        for (Seen& s : seen_) {
            if (s.id == id) {
                return &s;
            }
        }
        return nullptr;
    }
    const Seen* Find(int id) const {
        for (const Seen& s : seen_) {
            if (s.id == id) {
                return &s;
            }
        }
        return nullptr;
    }

    std::vector<Seen> seen_;
    int active_ = -1;
};

} // namespace FrameXr::Input
