// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace frame_xr::picker {

/// Where the Azahar for Frame art file `name` (e.g. "logo.png") is: in $AZAHARFRAME_ART when
/// that is set, else in art/ next to the program (the installed layout, see
/// steam-frame/install.sh), else in steam-frame/art/ of the checkout the program was built
/// in (<build>/bin/Release/azahar). Empty when it is nowhere.
std::string FindArtFile(const std::string& name);

/// Decodes a PNG file to straight RGBA8, rows top down. False (and `rgba` empty) when the
/// file is missing or is not a PNG.
bool LoadPngFile(const std::string& path, std::vector<uint8_t>& rgba, int& width, int& height);

} // namespace frame_xr::picker
