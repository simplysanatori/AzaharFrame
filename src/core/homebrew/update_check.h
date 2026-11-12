// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ui3: Check for Updates on a homebrew package (the VR menu's Homebrew tab and the Flat
// Homebrew menu). A press asks GitHub for the latest release at once, past the once-a-day check
// (Emerald3ds::CheckDue), but at most once in kManualCheckGap: a press sooner gets the last
// result again, so GitHub's limit for clients with no token (60 an hour) is never reached.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include "common/common_types.h"

namespace Homebrew {

enum class CheckResult : u8 {
    NoUpdate,        ///< the latest release is the one installed (or older)
    UpdateAvailable, ///< a newer release: Update shows
    Failed,          ///< no answer, or not one we read: nothing changes
};

inline constexpr s64 kManualCheckGap = 30; ///< seconds between two requests a press makes

struct ManualCheck {
    s64 last_request = 0; ///< unix seconds a press last asked GitHub (0 never)
    std::optional<CheckResult> last_result;

    /// Whether a press now asks GitHub; else it reuses last_result (also when the clock went back
    /// it asks).
    bool Due(s64 now) const;
    /// A press asked GitHub at `now`.
    void Asked(s64 now) {
        last_request = now;
    }
    void Answered(CheckResult result) {
        last_result = result;
    }
};

/// What a check found: the latest release's tag ("" when it failed) against the installed one.
CheckResult ResultOf(std::string_view latest_tag, std::string_view installed_tag);

/// The toast (VR) or message (Flat): "No Updates Available", "Update Available", "Couldn't Check
/// for Updates".
std::string_view ResultText(CheckResult result);

/// The row's note after a check: "Checked Just Now", "Checked 5 Minutes Ago", "Checked 1 Hour
/// Ago", "Checked Yesterday", "Checked 3 Days Ago"; "" when never checked (0) or in the future.
std::string CheckedText(s64 checked, s64 now);

} // namespace Homebrew
