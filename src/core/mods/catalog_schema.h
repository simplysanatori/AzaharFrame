// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame harden: the "schema" of catalog/mods.json and catalog/textures.json, checked. The live
// catalogs are read from the repository's main branch, so a later format change must not be half-parsed
// by a program that does not know it: a catalog whose schema is newer than the program supports is
// refused whole, and the compiled-in copy stays in use.
#pragma once

#include <string>
#include <fmt/format.h>

namespace Mods {

/// The newest catalog "schema" this program understands (both catalog files).
constexpr int kSupportedSchema = 1;

/// Start of the error ParseCatalog gives for a catalog that is too new (callers tell it from a
/// broken file by this prefix, see SchemaTooNew).
constexpr const char* kSchemaTooNewPrefix = "Catalog schema ";

inline bool SchemaTooNew(const std::string& error) {
    return error.rfind(kSchemaTooNewPrefix, 0) == 0;
}

/// Reads "schema" from the parsed root (a missing one counts as 1). False, with `error` set, when it
/// is not a whole number or is newer than kSupportedSchema.
template <class Json>
bool ReadSchema(const Json& root, int& schema, std::string* error) {
    schema = 1;
    const auto it = root.find("schema");
    if (it == root.end()) {
        return true;
    }
    if (!it->is_number_integer()) {
        if (error) {
            *error = "The catalog's schema is not a number";
        }
        return false;
    }
    const long long value = it->template get<long long>();
    if (value > kSupportedSchema) {
        if (error) {
            *error = fmt::format("{}{} is newer than this version supports ({})", kSchemaTooNewPrefix, value,
                                 kSupportedSchema);
        }
        return false;
    }
    schema = value < 1 ? 1 : static_cast<int>(value);
    return true;
}

} // namespace Mods
