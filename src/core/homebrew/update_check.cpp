// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ui3: see update_check.h.

#include "core/homebrew/emerald3ds.h"
#include "core/homebrew/update_check.h"

namespace Homebrew {

bool ManualCheck::Due(s64 now) const {
    return !last_result || last_request <= 0 || now < last_request ||
           now - last_request >= kManualCheckGap;
}

CheckResult ResultOf(std::string_view latest_tag, std::string_view installed_tag) {
    if (latest_tag.empty()) {
        return CheckResult::Failed;
    }
    return Emerald3ds::IsNewer(latest_tag, installed_tag) ? CheckResult::UpdateAvailable
                                                          : CheckResult::NoUpdate;
}

std::string_view ResultText(CheckResult result) {
    switch (result) {
    case CheckResult::NoUpdate:
        return "No Updates Available";
    case CheckResult::UpdateAvailable:
        return "Update Available";
    case CheckResult::Failed:
        break;
    }
    return "Couldn't Check for Updates";
}

std::string CheckedText(s64 checked, s64 now) {
    if (checked <= 0 || now < checked) {
        return {};
    }
    const s64 ago = now - checked;
    const auto n = [](s64 v, const char* one, const char* many) {
        return "Checked " + std::to_string(v) + " " + (v == 1 ? one : many) + " Ago";
    };
    if (ago < 60) {
        return "Checked Just Now";
    }
    if (ago < 3600) {
        return n(ago / 60, "Minute", "Minutes");
    }
    if (ago < 86400) {
        return n(ago / 3600, "Hour", "Hours");
    }
    if (ago < 2 * 86400) {
        return "Checked Yesterday";
    }
    return n(ago / 86400, "Day", "Days");
}

} // namespace Homebrew
