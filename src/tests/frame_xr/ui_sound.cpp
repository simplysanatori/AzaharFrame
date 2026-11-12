// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame menuux: the click's file read and shaped (frame_xr/ui_sound.h). Made-up files only:
// nothing of Steam's is in the tree.

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/ui_sound.h"

using namespace FrameXr::UiSound;

namespace {

void Put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        b.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
}
void Put16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v));
    b.push_back(static_cast<uint8_t>(v >> 8));
}

// A WAVE file of `frames` frames, every sample `value`, with a chunk before "fmt " to skip.
std::vector<uint8_t> Wav(int channels, int rate, int frames, int16_t value, int bits = 16) {
    std::vector<uint8_t> b{'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
    b.insert(b.end(), {'L', 'I', 'S', 'T'});
    Put32(b, 3);
    b.insert(b.end(), {1, 2, 3, 0}); // odd size: padded
    b.insert(b.end(), {'f', 'm', 't', ' '});
    Put32(b, 16);
    Put16(b, 1);
    Put16(b, static_cast<uint16_t>(channels));
    Put32(b, static_cast<uint32_t>(rate));
    Put32(b, static_cast<uint32_t>(rate * channels * bits / 8));
    Put16(b, static_cast<uint16_t>(channels * bits / 8));
    Put16(b, static_cast<uint16_t>(bits));
    b.insert(b.end(), {'d', 'a', 't', 'a'});
    Put32(b, static_cast<uint32_t>(frames * channels * 2));
    for (int i = 0; i < frames * channels; i++) {
        Put16(b, static_cast<uint16_t>(value));
    }
    return b;
}

} // namespace

TEST_CASE("UI sound: a 16-bit WAVE file, cut, faded and quieter", "[frame_xr]") {
    const auto clip = DecodeWav(Wav(2, 44100, 44100, 10000), 0.5f, 150, 40);
    REQUIRE(clip);
    CHECK(clip->rate == 44100);
    const std::size_t frames = 44100 * 150 / 1000;
    REQUIRE(clip->samples.size() == frames * 2);
    CHECK(clip->samples[0] == 5000); // the gain
    CHECK(clip->samples[1] == 5000);
    // The fade: down to near nothing at the end, never louder than the gain.
    CHECK(clip->samples[(frames - 1) * 2] < 50);
    for (const int16_t s : clip->samples) {
        CHECK(s <= 5000);
        CHECK(s >= 0);
    }
}

TEST_CASE("UI sound: mono becomes stereo, a short file stays short", "[frame_xr]") {
    const auto clip = DecodeWav(Wav(1, 48000, 480, -8000), 1.0f, 150, 0);
    REQUIRE(clip);
    REQUIRE(clip->samples.size() == 960);
    CHECK(clip->samples[0] == -8000);
    CHECK(clip->samples[1] == -8000);
}

TEST_CASE("UI sound: anything else plays nothing", "[frame_xr]") {
    CHECK_FALSE(DecodeWav({}, 0.5f, 150, 40));
    CHECK_FALSE(DecodeWav(Wav(2, 44100, 100, 1, 8), 0.5f, 150, 40)); // 8-bit
    auto riff = Wav(2, 44100, 100, 1);
    riff[8] = 'X'; // not WAVE
    CHECK_FALSE(DecodeWav(riff, 0.5f, 150, 40));
    auto cut = Wav(2, 44100, 100, 1);
    cut.resize(40); // ends inside "fmt "
    CHECK_FALSE(DecodeWav(cut, 0.5f, 150, 40));
}

TEST_CASE("UI sound: Steam's sound is looked for in Steam's folders only", "[frame_xr]") {
    const auto paths = ClickPaths("/home/u", "");
    REQUIRE(paths.size() == 2);
    CHECK(paths[0] == "/home/u/.local/share/Steam/steamui/sounds/deck_ui_navigation.wav");
    CHECK(paths[1] == "/home/u/.steam/steam/steamui/sounds/deck_ui_navigation.wav");
    const auto xdg = ClickPaths("/home/u", "/data");
    REQUIRE(xdg.size() == 3);
    CHECK(xdg[0] == "/data/Steam/steamui/sounds/deck_ui_navigation.wav");
    CHECK(ClickPaths("", "relative").empty());
}
