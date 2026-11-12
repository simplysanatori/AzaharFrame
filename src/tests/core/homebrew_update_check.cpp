// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ui3: Check for Updates' states with no network: what a check found, the gap between
// two requests, and the texts.

#include <catch2/catch_test_macros.hpp>
#include "core/homebrew/update_check.h"

using Homebrew::CheckResult;

TEST_CASE("Homebrew: Check for Updates' result", "[homebrew][update_check]") {
    CHECK(Homebrew::ResultOf("v0.6.0", "v0.5.0") == CheckResult::UpdateAvailable);
    CHECK(Homebrew::ResultOf("v0.5.0", "v0.5.0") == CheckResult::NoUpdate);
    CHECK(Homebrew::ResultOf("v0.4.9", "v0.5.0") == CheckResult::NoUpdate); // older: no update
    CHECK(Homebrew::ResultOf("v0.10.0", "v0.9.0") == CheckResult::UpdateAvailable);
    CHECK(Homebrew::ResultOf("", "v0.5.0") == CheckResult::Failed); // no answer
    CHECK(Homebrew::ResultText(CheckResult::NoUpdate) == "No Updates Available");
    CHECK(Homebrew::ResultText(CheckResult::UpdateAvailable) == "Update Available");
    CHECK(Homebrew::ResultText(CheckResult::Failed) == "Couldn't Check for Updates");
}

TEST_CASE("Homebrew: Check for Updates asks at most once in 30 seconds", "[homebrew][update_check]") {
    Homebrew::ManualCheck m;
    const s64 t = 1'800'000'000;
    CHECK(m.Due(t)); // never asked
    m.Asked(t);
    CHECK(m.Due(t + 1)); // asked, no answer yet: the next press is held by the running check
    m.Answered(CheckResult::NoUpdate);
    CHECK_FALSE(m.Due(t + 1));
    CHECK_FALSE(m.Due(t + Homebrew::kManualCheckGap - 1)); // the last result again
    CHECK(*m.last_result == CheckResult::NoUpdate);
    CHECK(m.Due(t + Homebrew::kManualCheckGap));
    CHECK(m.Due(t - 5)); // the clock went back
    // A failure is a result too: pressed again at once, it is not asked again.
    m.Asked(t + 100);
    m.Answered(CheckResult::Failed);
    CHECK_FALSE(m.Due(t + 110));
    CHECK(m.Due(t + 130));
}

TEST_CASE("Homebrew: when it was checked", "[homebrew][update_check]") {
    const s64 now = 1'800'000'000;
    CHECK(Homebrew::CheckedText(0, now).empty());
    CHECK(Homebrew::CheckedText(now + 10, now).empty());
    CHECK(Homebrew::CheckedText(now, now) == "Checked Just Now");
    CHECK(Homebrew::CheckedText(now - 59, now) == "Checked Just Now");
    CHECK(Homebrew::CheckedText(now - 60, now) == "Checked 1 Minute Ago");
    CHECK(Homebrew::CheckedText(now - 5 * 60, now) == "Checked 5 Minutes Ago");
    CHECK(Homebrew::CheckedText(now - 3600, now) == "Checked 1 Hour Ago");
    CHECK(Homebrew::CheckedText(now - 7 * 3600, now) == "Checked 7 Hours Ago");
    CHECK(Homebrew::CheckedText(now - 30 * 3600, now) == "Checked Yesterday");
    CHECK(Homebrew::CheckedText(now - 3 * 86400, now) == "Checked 3 Days Ago");
}
