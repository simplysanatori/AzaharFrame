// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cheats: reading and merging a Sharkive cheat file. No network and no Qt here, so it can
// be tested. Sharkive (FlagBrew/Sharkive, GPL-3.0) keeps one file per title in Azahar's own gateway
// format.

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Cheats::Sharkive {

struct Entry {
    std::string name;
    std::string comments; // lines without the leading '*', each ending in '\n'
    std::string code;     // "XXXXXXXX YYYYYYYY" lines joined by '\n', no trailing newline
};

/// Path of the title's file on raw.githubusercontent.com ("/FlagBrew/Sharkive/master/3ds/<ID>.txt").
std::string UrlPath(u64 title_id);

/// Reads a Sharkive file. Drops the "---" line, "*citra_enabled" (downloaded cheats arrive disabled),
/// blocks with no code, blocks with a line that is not "8 hex digits, space, 8 hex digits", and later
/// blocks whose name was already seen.
std::vector<Entry> Parse(std::string_view text);

/// The entries of `downloaded` whose name is not in `existing_names` (names compared after trimming).
std::vector<Entry> NewEntries(const std::vector<Entry>& downloaded,
                              const std::vector<std::string>& existing_names);

} // namespace Cheats::Sharkive
