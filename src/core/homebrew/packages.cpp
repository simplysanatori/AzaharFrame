// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emerald: see packages.h.

#include "core/homebrew/packages.h"

#include <array>

namespace Homebrew {

namespace {
constexpr std::array<PackageInfo, 1> kPackages{{
    {"emerald3ds", "Pokémon Emerald 3DS", "ZallaxDev", "emerald-3ds.com"},
}};
} // namespace

std::span<const PackageInfo> Packages() {
    return kPackages;
}

const PackageInfo* FindPackage(std::string_view id) {
    for (const PackageInfo& p : kPackages) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

std::string AddLabel(const PackageInfo& package) {
    return "Add " + std::string(package.title);
}

std::string UpdateLabel(const PackageInfo& package) {
    return "Update " + std::string(package.title);
}

} // namespace Homebrew
