// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame cpphelpers: the parts of emerald3ds_build.h that hold JSON (core and its tests only).

#pragma once

#include <json.hpp>
#include "core/homebrew/emerald3ds_build.h"

namespace Homebrew::Emerald3ds::Build {

/// A recipe (container: "EM3DRCP1", u32 length, the metadata JSON in .xz, the literal pool in .xz).
struct Recipe {
    u32 engine_abi = 0;
    std::string rom_sha1;
    std::string release;
    nlohmann::json entries = nlohmann::json::array();
    nlohmann::json bitmaps = nlohmann::json::array();
    nlohmann::json generated = nlohmann::json::array();
    nlohmann::json inputs = nlohmann::json::array();
    std::string literals;
};
/// The metadata only (no literal pool): enough for the ROM's SHA-1.
std::optional<Recipe> ParseRecipe(std::string_view data, bool with_literals, std::string* error);
/// The SHA-1 a recipe is made from, reading only its header and metadata ("" on failure).
std::string RecipeRomSha1(std::string_view data);

/// The program interpreter of the release's precompiled voxel files (precompiled.py).
using ProgramSources = std::function<const std::string*(const std::string& path)>;
std::optional<std::string> RunProgram(std::string_view program, const ProgramSources& read,
                                      std::size_t limit, std::string* error);

/// The bytes one recipe entry writes, checked against its size and CRC-32. `sources` is what
/// "V" programs read (nullptr when none).
std::optional<std::string> BuildEntry(const nlohmann::json& entry, std::string_view rom,
                                      const Recipe& recipe, const ProgramSources* sources,
                                      std::string* error);

// Followers (followers.py)
struct FollowerFile {
    std::string source, path, sha256, language, key;
    u64 size = 0;
    int follower_index = 0;
};
/// followers_sources.py's SOURCES literal (a Python dict) as JSON.
std::optional<nlohmann::json> ParsePythonLiteral(std::string_view text, std::string* error);
} // namespace Homebrew::Emerald3ds::Build
