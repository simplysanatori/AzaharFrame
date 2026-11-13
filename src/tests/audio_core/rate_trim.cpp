// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame pacing: the audio read at the game's speed while the VR frame pacing lock runs it
// 0.28 % fast (audio_core/rate_trim.h).

#include <cmath>
#include <cstdlib>
#include <deque>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "audio_core/rate_trim.h"
#include "common/xr_pacer.h"

using AudioCore::RateTrim;

namespace {

struct Source {
    std::deque<RateTrim::Frame> fifo;
    std::size_t Pop(RateTrim::Frame* dst, std::size_t max) {
        const std::size_t n = std::min(max, fifo.size());
        for (std::size_t i = 0; i < n; i++) {
            dst[i] = fifo.front();
            fifo.pop_front();
        }
        return n;
    }
};

} // namespace

TEST_CASE("RateTrim: a ramp read faster stays smooth across callbacks", "[audio_core][pacing]") {
    const double ratio = Common::XrPacer::LockSpeed(8'333'333);
    Source src;
    for (int i = 0; i < 20000; i++) {
        src.fifo.push_back({static_cast<s16>(i % 30000), static_cast<s16>(-(i % 30000))});
    }
    RateTrim trim;
    std::vector<s16> out;
    std::vector<s16> buf(2 * 512);
    for (int cb = 0; cb < 30; cb++) {
        const std::size_t n = trim.Process(ratio, buf.data(), 512,
                                           [&](auto* d, std::size_t m) { return src.Pop(d, m); });
        REQUIRE(n == 512);
        out.insert(out.end(), buf.begin(), buf.end());
    }
    // Every step is the ratio (give or take the rounding), including at the callbacks' edges.
    for (std::size_t k = 1; k < out.size() / 2; k++) {
        const double step = out[2 * k] - out[2 * (k - 1)];
        REQUIRE(step == Catch::Approx(ratio).margin(1.0));
        REQUIRE(out[2 * k + 1] == -out[2 * k]);
    }
    // 30 x 512 output frames read 30 x 512 x ratio input frames.
    const double read = 20000.0 - static_cast<double>(src.fifo.size());
    CHECK(read == Catch::Approx(30 * 512 * ratio).margin(4));
}

TEST_CASE("RateTrim: a game 0.28 % fast keeps a steady backlog", "[audio_core][pacing]") {
    // 32728 Hz in 512-frame callbacks; the game adds ratio x as much each time (fractionally).
    const double ratio = Common::XrPacer::LockSpeed(8'333'333);
    Source src;
    RateTrim trim;
    std::vector<s16> buf(2 * 512);
    double owed = 0;
    s16 v = 0;
    const auto produce = [&](double frames) {
        owed += frames;
        while (owed >= 1) {
            src.fifo.push_back({v, v});
            v = static_cast<s16>(v + 1);
            owed -= 1;
        }
    };
    produce(1024); // a starting backlog
    std::size_t lo = SIZE_MAX, hi = 0, short_reads = 0;
    for (int cb = 0; cb < 64 * 60; cb++) { // a minute
        produce(512 * ratio);
        const std::size_t n = trim.Process(ratio, buf.data(), 512,
                                           [&](auto* d, std::size_t m) { return src.Pop(d, m); });
        short_reads += n < 512;
        if (cb > 64) {
            lo = std::min(lo, src.fifo.size());
            hi = std::max(hi, src.fifo.size());
        }
    }
    CHECK(short_reads == 0);
    CHECK(hi - lo < 8); // without the trim it would grow 2.8 ms (92 frames) a second
}

TEST_CASE("RateTrim: dry input, then a flush back to the plain path", "[audio_core][pacing]") {
    Source src;
    for (int i = 0; i < 100; i++) {
        src.fifo.push_back({static_cast<s16>(i), static_cast<s16>(i)});
    }
    RateTrim trim;
    CHECK(trim.Idle());
    std::vector<s16> buf(2 * 256);
    const std::size_t n =
        trim.Process(1.0028, buf.data(), 256, [&](auto* d, std::size_t m) { return src.Pop(d, m); });
    CHECK(n < 100);
    CHECK(n > 95);
    CHECK(src.fifo.empty());
    // The rest goes out as it is, then nothing is held back.
    for (int i = 100; i < 110; i++) {
        src.fifo.push_back({static_cast<s16>(i), static_cast<s16>(i)});
    }
    const std::size_t m =
        trim.Process(1.0028, buf.data(), 4, [&](auto* d, std::size_t k) { return src.Pop(d, k); });
    CHECK(m == 4);
    CHECK_FALSE(trim.Idle());
    const std::size_t f = trim.Flush(buf.data(), 256);
    CHECK(f >= 1);
    CHECK(trim.Idle());
    CHECK(std::abs(static_cast<long>(buf[0]) - static_cast<long>(n + 4)) <= 1); // carries on
}
