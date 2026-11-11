// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cheats: one HTTPS GET of a title's Sharkive file, only when the Cheats dialog's
// Download button is clicked. Blocking; call it off the GUI thread.

#pragma once

#include <string>
#include "common/common_types.h"

namespace SharkiveClient {

enum class Status { Ok, NotFound, Error };

struct Result {
    Status status = Status::Error;
    std::string body;  // Ok: the file
    std::string error; // Error: a short reason for the status line
};

/// GET https://raw.githubusercontent.com/FlagBrew/Sharkive/master/3ds/<title id>.txt
Result Fetch(u64 title_id);

} // namespace SharkiveClient
