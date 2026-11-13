// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ipd: the headset's IPD, measured from the eye poses SteamVR locates each frame, for
// the game's eye separation in immersive mode 3 and the head-locked screens. The value follows
// the IPD dial during play and nothing else:
// - a frame whose eye positions are not valid and tracked, or whose distance is outside
//   kMinM..kMaxM, is ignored;
// - each frame's value is the mean of the middle kKeep of the last kWindow good frames (one-off
//   outliers drop out, and jitter that alternates frame by frame averages out);
// - the mean replaces the value in use only after it has stayed more than kDeadbandM away for
//   kConfirmS (frame-to-frame jitter never does); for kFollowS after such a change (and after
//   the first value) the deadband is kFineM instead, so the end of a slow turn is reached
//   exactly, then the value is held again;
// - the value handed out eases to the new one with a time constant of kEaseS, so a dial turn
//   shows as a quick glide, not a snap.
// A dial step settles in about 0.4 s (half a window, the confirm time, three time
// constants). kFallbackM (CitraVR's VR_IPD) is used until the first good window.
// Header only, no XR types: XR thread only; frame_xr hands the value to video_core.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace FrameXr::Ipd {

inline constexpr float kFallbackM = 0.065f;   ///< CitraVR's VR_IPD, until the headset's is known
inline constexpr float kMinM = 0.045f;        ///< below or above: not an IPD, ignored
inline constexpr float kMaxM = 0.085f;
inline constexpr int kWindow = 10;            ///< 83 ms at 120 Hz
inline constexpr int kKeep = 6;               ///< the middle 6 of 10: two dropped each side
inline constexpr float kDeadbandM = 0.0003f;  ///< a mean this close to the value is jitter
inline constexpr double kConfirmS = 0.15;     ///< how long a new mean must hold
inline constexpr float kFineM = 0.00005f;     ///< the deadband just after a change
inline constexpr double kFollowS = 1.0;       ///< how long kFineM is used after a change
inline constexpr double kEaseS = 0.05;        ///< the ease's time constant
inline constexpr float kLogStepM = 0.0005f;   ///< a change logged when it is more than this
inline constexpr double kMaxStepS = 0.1;      ///< a longer gap between frames counts as this

class Smoother {
public:
    /// One XR frame: its display time (ns), the distance between the located eyes (m) and
    /// whether both eye positions were valid and tracked. Returns Value().
    float Add(int64_t time_ns, float ipd_m, bool valid) {
        double dt = 0.0;
        if (has_time_ && time_ns > last_ns_) {
            dt = std::min((time_ns - last_ns_) / 1e9, kMaxStepS);
        }
        if (!has_time_ || time_ns > last_ns_) {
            last_ns_ = time_ns;
            has_time_ = true;
        }
        if (valid && std::isfinite(ipd_m) && ipd_m >= kMinM && ipd_m <= kMaxM) {
            ring_[next_] = ipd_m;
            next_ = (next_ + 1) % kWindow;
            count_ = std::min(count_ + 1, kWindow);
            if (count_ == kWindow) {
                Settle(TrimmedMean(), time_ns);
            }
        }
        // The ease, by time, so it takes as long at 90 Hz as at 120 Hz.
        if (known_ && value_ != target_) {
            value_ += static_cast<float>((target_ - value_) * (1.0 - std::exp(-dt / kEaseS)));
            if (std::abs(target_ - value_) < 1e-6f) {
                value_ = target_;
            }
        }
        return value_;
    }

    /// The IPD to use now (m): the eased value, kFallbackM until known.
    float Value() const {
        return value_;
    }
    /// The settled value the ease is heading for (kFallbackM until known).
    float Target() const {
        return target_;
    }
    bool Known() const {
        return known_;
    }

    /// True once when the first value is known, then on each settled change of more than
    /// kLogStepM from the last value logged; `m` the value to log (metres).
    bool TakeLog(float& m) {
        if (!known_ || (logged_ && std::abs(target_ - logged_m_) <= kLogStepM)) {
            return false;
        }
        logged_ = true;
        logged_m_ = target_;
        m = target_;
        return true;
    }

private:
    float TrimmedMean() const {
        std::array<float, kWindow> s = ring_;
        std::sort(s.begin(), s.end());
        constexpr int drop = (kWindow - kKeep) / 2;
        double sum = 0.0;
        for (int i = drop; i < drop + kKeep; i++) {
            sum += s[i];
        }
        return static_cast<float>(sum / kKeep);
    }

    void Settle(float mean, int64_t time_ns) {
        if (!known_) { // the first good window: used at once (nothing shown with it yet)
            known_ = true;
            target_ = value_ = mean;
            follow_until_ = time_ns + static_cast<int64_t>(kFollowS * 1e9);
            return;
        }
        const bool following = time_ns < follow_until_;
        if (std::abs(mean - target_) <= (following ? kFineM : kDeadbandM)) {
            pending_ = false; // back within the deadband: it was jitter
            return;
        }
        if (!pending_) {
            pending_ = true;
            pending_since_ = time_ns;
        }
        if ((time_ns - pending_since_) / 1e9 >= kConfirmS) {
            // Only a change past the wide deadband starts (or extends) the close following, so
            // jitter cannot keep it going.
            if (std::abs(mean - target_) > kDeadbandM) {
                follow_until_ = time_ns + static_cast<int64_t>(kFollowS * 1e9);
            }
            target_ = mean;
            pending_ = false;
        }
    }

    std::array<float, kWindow> ring_{};
    int next_ = 0;
    int count_ = 0;
    bool known_ = false;
    float target_ = kFallbackM;
    float value_ = kFallbackM;
    bool pending_ = false;
    int64_t pending_since_ = 0;
    int64_t follow_until_ = 0;
    bool has_time_ = false;
    int64_t last_ns_ = 0;
    bool logged_ = false;
    float logged_m_ = 0.0f;
};

} // namespace FrameXr::Ipd
