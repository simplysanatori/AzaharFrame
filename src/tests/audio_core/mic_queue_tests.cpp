// AzaharFrame fixes11
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "audio_core/mic_sample_buffer.h"
#include "common/threadsafe_queue.h"

TEST_CASE("MicSampleBuffer: stays at its cap and drops the oldest", "[audio_core][mic]") {
    AudioCore::MicSampleBuffer buf(32760); // 1 s of 16360 Hz 16-bit
    std::vector<u8> chunk(1024);
    u8 counter = 0;
    for (int i = 0; i < 200000; i++) { // ~200 MB offered
        for (auto& b : chunk) {
            b = counter++;
        }
        buf.Push(chunk);
        REQUIRE(buf.Size() <= 32760);
    }
    REQUIRE(buf.Size() == 32760);
    auto out = buf.Take();
    REQUIRE(out.size() == 32760);
    // the newest bytes were kept, in order
    for (std::size_t i = 1; i < out.size(); i++) {
        REQUIRE(static_cast<u8>(out[i - 1] + 1) == out[i]);
    }
    REQUIRE(static_cast<u8>(out.back() + 1) == counter);
    REQUIRE(buf.Size() == 0);
}

TEST_CASE("MicSampleBuffer: a push bigger than the cap keeps the tail", "[audio_core][mic]") {
    AudioCore::MicSampleBuffer buf(4);
    const std::vector<u8> in{1, 2, 3, 4, 5, 6};
    buf.Push(in);
    REQUIRE(buf.Take() == std::vector<u8>{3, 4, 5, 6});
}

TEST_CASE("SPSCQueue: a huge queue is destroyed without recursion", "[common][mic]") {
    {
        Common::SPSCQueue<std::vector<u8>> q;
        for (int i = 0; i < 3000000; i++) {
            q.Push(std::vector<u8>{1});
        }
        REQUIRE(q.Size() == 3000000);
    } // destructor: would overflow the stack if it recursed
    Common::SPSCQueue<int> q2;
    for (int i = 0; i < 3000000; i++) {
        q2.Push(i);
    }
    q2.Clear();
    REQUIRE(q2.Empty());
}
