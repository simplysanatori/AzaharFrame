// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame fixes7: the picture shape a mod or texture pack is made for. Both catalogs may give an
// entry an optional "aspect" ("16:9", "21:9", "16:10", "4:3", "5:3"; builds before this one ignore
// it), e.g. a code patch that makes the game widescreen. The screen shape code asks
// EnabledModsAspectHint for the turned-on ones of a game.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Mods {

/// The catalog's "aspect" as stored: "16:9", "21:9", "16:10", "4:3" or "5:3" (spaces around the
/// colon allowed); "" for anything else.
std::string NormalizeAspect(std::string_view text);
/// Width / height of such a value (16:9 -> 1.777...), nullopt when it is not one.
std::optional<float> AspectRatio(std::string_view text);
/// The first of `aspects` (in order) that is one, as width / height.
std::optional<float> FirstAspect(const std::vector<std::string>& aspects);

/// The shape the game's turned-on catalog mods (Mods::Default(), active ones, in the loaded list's
/// order) and then its turned-on downloaded texture packs are made for: the first that has an
/// "aspect", as width / height (16:9 -> 1.777...). nullopt when none has one. Reads the mod library's
/// manifests and the texture packs folder (a few small files); any thread.
std::optional<float> EnabledModsAspectHint(u64 title_id);

} // namespace Mods
