// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: the soft click of Edit Controls. It is Steam's own UI sound
// (steamui/sounds/deck_ui_navigation.wav: about 40 ms of sound, the shortest and softest of
// Steam's clicks), read at run time from the Steam install on the machine; nothing of Valve's is
// bundled. With no Steam install it plays nothing. Played quietly through SDL's audio, on a
// thread of its own: the XR thread only signals it (no file reading, decoding or audio calls
// there).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace FrameXr::UiSound {

/// Loads the click and opens the audio device ahead of the first PlayClick. Any thread; returns
/// at once.
void Prepare();
/// Plays the click (a click still playing is cut). Any thread; returns at once.
void PlayClick();
/// Stops the thread and closes the device (at shutdown).
void Shutdown();

/// The click, ready to play: interleaved stereo signed 16-bit samples at `rate`.
struct Clip {
    int rate = 0;
    std::vector<int16_t> samples;
};

/// A RIFF/WAVE file of 16-bit PCM (mono or stereo) as a Clip: cut to `max_ms` with a linear fade
/// over its last `fade_ms`, every sample times `gain`. Nothing for any other file.
std::optional<Clip> DecodeWav(const std::vector<uint8_t>& bytes, float gain, int max_ms,
                              int fade_ms);

/// Where the click may be, in order: Steam's data folder under XDG_DATA_HOME (or
/// ~/.local/share), then ~/.steam/steam.
std::vector<std::string> ClickPaths(const std::string& home, const std::string& xdg_data_home);

} // namespace FrameXr::UiSound
