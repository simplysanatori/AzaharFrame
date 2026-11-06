// Copyright 2026 CitraFrame contributors
// Licensed under GPLv3 or any later version
#pragma once

#include <optional>
#include <string>

#include "frame_xr/picker/library.h"

namespace frame_xr::picker {

/// Reads a file's title and large SMDH icon through Loader::GetLoader(), applying the
/// same filters as Azahar's game list: not executable (and not merely encrypted) files
/// are skipped, as are system titles without the SMDH "visible" flag. Needs the
/// emulator core; the rest of the picker does not.
std::optional<GameMeta> ReadMetadataWithLoader(const std::string& path);

} // namespace frame_xr::picker
