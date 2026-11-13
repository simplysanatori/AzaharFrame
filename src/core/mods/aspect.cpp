// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <array>
#include <utility>
#include "core/custom_tex/download.h"
#include "core/custom_tex/packs.h"
#include "core/mods/aspect.h"
#include "core/mods/library.h"

namespace Mods {

namespace {
constexpr std::array<std::pair<int, int>, 5> kShapes{{{16, 9}, {21, 9}, {16, 10}, {4, 3}, {5, 3}}};

std::string_view Trim(std::string_view s) {
    while (!s.empty() && s.front() == ' ') {
        s.remove_prefix(1);
    }
    while (!s.empty() && s.back() == ' ') {
        s.remove_suffix(1);
    }
    return s;
}
} // namespace

std::string NormalizeAspect(std::string_view text) {
    const auto colon = text.find(':');
    if (colon == std::string_view::npos) {
        return {};
    }
    const std::string w(Trim(text.substr(0, colon)));
    const std::string h(Trim(text.substr(colon + 1)));
    for (const auto& [sw, sh] : kShapes) {
        if (w == std::to_string(sw) && h == std::to_string(sh)) {
            return w + ":" + h;
        }
    }
    return {};
}

std::optional<float> AspectRatio(std::string_view text) {
    const std::string norm = NormalizeAspect(text);
    if (norm.empty()) {
        return std::nullopt;
    }
    const auto colon = norm.find(':');
    return std::stof(norm.substr(0, colon)) / std::stof(norm.substr(colon + 1));
}

std::optional<float> FirstAspect(const std::vector<std::string>& aspects) {
    for (const std::string& a : aspects) {
        if (const auto r = AspectRatio(a)) {
            return r;
        }
    }
    return std::nullopt;
}

std::optional<float> EnabledModsAspectHint(u64 title_id) {
    std::vector<std::string> aspects;
    for (const ModInfo& m : Default().ListLoaded(title_id)) {
        if (m.managed && m.state == ModState::Active && !m.aspect.empty()) {
            aspects.push_back(m.aspect);
        }
    }
    if (const auto r = FirstAspect(aspects)) {
        return r;
    }
    // Then the downloaded texture packs that are on: the marker's id in the texture catalog
    namespace D = CustomTex::Download;
    const D::Catalog catalog = D::LatestCatalog();
    for (const CustomTex::Pack& p : CustomTex::ListPacks(CustomTex::DefaultPaths(), title_id)) {
        if (!p.active || !p.managed) {
            continue;
        }
        const auto id = D::MarkerId(p.path);
        if (!id) {
            continue;
        }
        const std::string marked = D::MarkerAspect(p.path);
        if (!marked.empty()) {
            aspects.push_back(marked);
        } else if (const D::Entry* e = catalog.Find(*id); e != nullptr && !e->aspect.empty()) {
            aspects.push_back(e->aspect);
        }
    }
    return FirstAspect(aspects);
}

} // namespace Mods
