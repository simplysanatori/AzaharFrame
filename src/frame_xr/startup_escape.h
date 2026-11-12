// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame memguard: a way out while a game starts. From the moment a game is booted until its
// screens are live (texture preload, shader cache, the first frame) the game's controls do not
// run (VrUi::GameFrame needs live screens), so neither the View stop hold nor the Menu hold did
// anything: a long or stuck start left the player with nothing that responded. Now Menu or View
// held 3 s stops the game (back to Azahar's window), with "Stopping..." from 1.5 s; if the game
// is still running 10 s after that (the emulator is stuck), Azahar quits. Pure (unit-tested in
// src/tests/frame_xr/startup_escape.cpp).

#pragma once

#include <cstdint>

namespace FrameXr {

class StartupEscape {
public:
    static constexpr std::int64_t kShowNs = 1'500'000'000;
    static constexpr std::int64_t kStopNs = 3'000'000'000;
    static constexpr std::int64_t kQuitAfterStopNs = 10'000'000'000;

    enum class Action {
        None,
        ShowStopping, ///< the "Stopping..." toast
        HideStopping, ///< let go before the stop: take the toast back
        Stop,         ///< stop the game
        Quit,         ///< the stop did not happen: end the process
    };

    /// Once a frame. `starting`: a game is running and its screens are not live yet.
    /// `running`: a game is running. `held`: Menu or View is down (no chord with a bumper).
    Action Update(bool starting, bool running, bool held, std::int64_t now_ns) {
        if (!running) {
            const bool was_shown = shown;
            *this = {};
            return was_shown ? Action::HideStopping : Action::None;
        }
        if (stop_at != 0) {
            if (!quit_sent && now_ns - stop_at >= kQuitAfterStopNs) {
                quit_sent = true;
                return Action::Quit;
            }
            return Action::None;
        }
        if (!starting) {
            // Live: the game's own controls take over. A hold under way is dropped.
            const bool was_shown = shown;
            armed = false;
            down = false;
            shown = false;
            return was_shown ? Action::HideStopping : Action::None;
        }
        if (!held) {
            armed = true;
            const bool was_shown = shown;
            down = false;
            shown = false;
            return was_shown ? Action::HideStopping : Action::None;
        }
        if (!armed) {
            return Action::None; // held from before the start: let go first
        }
        if (!down) {
            down = true;
            since = now_ns;
        }
        const std::int64_t held_ns = now_ns - since;
        if (held_ns >= kStopNs) {
            stop_at = now_ns;
            shown = false;
            return Action::Stop;
        }
        if (held_ns >= kShowNs && !shown) {
            shown = true;
            return Action::ShowStopping;
        }
        return Action::None;
    }

    [[nodiscard]] bool StopRequested() const {
        return stop_at != 0;
    }

private:
    bool armed = false;
    bool down = false;
    bool shown = false;
    bool quit_sent = false;
    std::int64_t since = 0;
    std::int64_t stop_at = 0;
};

} // namespace FrameXr
