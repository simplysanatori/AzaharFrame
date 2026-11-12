// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: the click's file, read and shaped (no audio here; unit-tested).

#include <algorithm>
#include <cmath>
#include <cstring>

#include "frame_xr/ui_sound.h"

namespace FrameXr::UiSound {

namespace {

uint32_t Le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
           static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

uint16_t Le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | p[1] << 8);
}

} // namespace

std::optional<Clip> DecodeWav(const std::vector<uint8_t>& bytes, float gain, int max_ms,
                              int fade_ms) {
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
        std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return std::nullopt;
    }
    int channels = 0, rate = 0, bits = 0, format = 0;
    const uint8_t* data = nullptr;
    std::size_t data_size = 0;
    std::size_t at = 12;
    while (at + 8 <= bytes.size()) {
        const uint8_t* chunk = bytes.data() + at;
        const std::size_t size = Le32(chunk + 4);
        const std::size_t body = at + 8;
        const std::size_t have = std::min(size, bytes.size() - body);
        if (std::memcmp(chunk, "fmt ", 4) == 0 && have >= 16) {
            format = Le16(chunk + 8);
            channels = Le16(chunk + 10);
            rate = static_cast<int>(Le32(chunk + 12));
            bits = Le16(chunk + 22);
            if (format == 0xFFFE && have >= 40) { // WAVE_FORMAT_EXTENSIBLE: the sub-format
                format = Le16(chunk + 32);
            }
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            data = bytes.data() + body;
            data_size = have;
        }
        at = body + size + (size & 1);
    }
    if (format != 1 || bits != 16 || (channels != 1 && channels != 2) || rate < 8000 ||
        rate > 192000 || !data) {
        return std::nullopt;
    }
    const std::size_t frames_in = data_size / (2 * static_cast<std::size_t>(channels));
    const std::size_t max_frames =
        std::max<std::size_t>(1, static_cast<std::size_t>(rate) * std::max(1, max_ms) / 1000);
    const std::size_t frames = std::min(frames_in, max_frames);
    if (frames == 0) {
        return std::nullopt;
    }
    const std::size_t fade = std::min<std::size_t>(
        frames, static_cast<std::size_t>(rate) * std::max(0, fade_ms) / 1000);
    Clip clip;
    clip.rate = rate;
    clip.samples.resize(frames * 2);
    for (std::size_t f = 0; f < frames; f++) {
        float g = gain;
        if (fade > 0 && f >= frames - fade) {
            g *= static_cast<float>(frames - f) / static_cast<float>(fade + 1);
        }
        for (int c = 0; c < 2; c++) {
            const int from = channels == 2 ? c : 0;
            const auto s = static_cast<int16_t>(
                Le16(data + (f * static_cast<std::size_t>(channels) + from) * 2));
            const float v = std::round(static_cast<float>(s) * g);
            clip.samples[f * 2 + static_cast<std::size_t>(c)] =
                static_cast<int16_t>(std::clamp(v, -32768.0f, 32767.0f));
        }
    }
    return clip;
}

std::vector<std::string> ClickPaths(const std::string& home, const std::string& xdg_data_home) {
    constexpr const char* kFile = "/Steam/steamui/sounds/deck_ui_navigation.wav";
    std::vector<std::string> paths;
    if (!xdg_data_home.empty() && xdg_data_home[0] == '/') {
        paths.push_back(xdg_data_home + kFile);
    }
    if (!home.empty()) {
        const std::string local = home + "/.local/share" + kFile;
        if (std::find(paths.begin(), paths.end(), local) == paths.end()) {
            paths.push_back(local);
        }
        paths.push_back(home + "/.steam/steam/steamui/sounds/deck_ui_navigation.wav");
    }
    return paths;
}

} // namespace FrameXr::UiSound
