// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the Mods tab's shell (mods_tab.h): the segmented button, then the segment.

#include <algorithm>

#include "frame_xr/menu/mods_tab.h"

#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu {

std::vector<ModsSegment> ModsSegmentsShown(const ModsTabModel& m) {
    std::vector<ModsSegment> out{ModsSegment::Cheats, ModsSegment::Mods, ModsSegment::Textures};
    if (m.ctgp7.shown) {
        out.push_back(ModsSegment::Ctgp7);
    }
    if (m.angels_aim.shown) {
        out.push_back(ModsSegment::AngelsAim);
    }
    return out;
}

ModsSegment ShownSegment(const ModsTabModel& m, ModsSegment chosen) {
    const auto shown = ModsSegmentsShown(m);
    return std::find(shown.begin(), shown.end(), chosen) != shown.end() ? chosen
                                                                       : ModsSegment::Cheats;
}

void TabMenu::DrawMods(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    constexpr float kPad = 24.0f, kSegH = 48.0f, kGap = 12.0f;
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    // AzaharFrame ctgp7: CTGP-7 as a fourth segment while Mario Kart 7 is open. AzaharFrame
    // kiufix: Angel's Aim the same while Kid Icarus: Uprising is; a target's index is the
    // segment's own value (ModsSegment), so a hidden one before it moves nothing.
    const std::vector<ModsSegment> segments = ModsSegmentsShown(model.mods);
    std::vector<std::string> opts = ModsSegmentLabels(model.mods.ctgp7.shown);
    if (model.mods.angels_aim.shown) {
        opts.emplace_back(kAngelsAimSegment);
    }
    const auto place = [&](int value) {
        for (std::size_t i = 0; i < segments.size(); i++) {
            if (static_cast<int>(segments[i]) == value) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };
    const Rect seg{c.x0, c.y0, c.x0 + kit.SegmentedWidth(opts), c.y0 + kSegH};
    const int hover = view.hover.id == Id::ModsSegment ? place(view.hover.index) : -1;
    const int pressed = view.pressed.id == Id::ModsSegment ? place(view.pressed.index) : -1;
    const ModsSegment segment = ShownSegment(model.mods, view.mods_segment);
    const int selected = place(static_cast<int>(segment));
    kit.Segmented(seg, opts, selected, hover, pressed);
    const auto e = kit.SegmentEdges(seg, opts);
    for (int i = 0; i < static_cast<int>(opts.size()); i++) {
        AddHit({e[static_cast<std::size_t>(i)], seg.y0, e[static_cast<std::size_t>(i + 1)], seg.y1},
               {Id::ModsSegment, static_cast<int>(segments[static_cast<std::size_t>(i)])}, pane);
    }
    // The segment's area: the pane's width (its own padding), under the segmented button.
    const Rect area{pane.x0, seg.y1 + kGap, pane.x1, pane.y1};
    switch (segment) {
    case ModsSegment::Cheats:
        DrawCheats(kit, model, view, area);
        break;
    case ModsSegment::Mods:
        DrawModsList(kit, model, view, area);
        break;
    case ModsSegment::Textures:
        DrawTextures(kit, model, view, area);
        break;
    case ModsSegment::Ctgp7: // AzaharFrame ctgp7
        DrawCtgp7(kit, model, view, area);
        break;
    case ModsSegment::AngelsAim: // AzaharFrame kiufix
        DrawAngelsAim(kit, model, view, area);
        break;
    }
}

} // namespace FrameXr::Menu
