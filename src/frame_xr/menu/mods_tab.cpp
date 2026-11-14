// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cheatstab: the Mods tab's shell (mods_tab.h): the segmented button, then the segment.

#include "frame_xr/menu/mods_tab.h"

#include "frame_xr/menu/tab_menu.h"

namespace FrameXr::Menu {

void TabMenu::DrawMods(Kit& kit, const MenuModel& model, const MenuView& view, const Rect& pane) {
    constexpr float kPad = 24.0f, kSegH = 48.0f, kGap = 12.0f;
    const Rect c{pane.x0 + kPad, pane.y0 + kPad, pane.x1 - kPad, pane.y1 - kPad};
    // AzaharFrame ctgp7: CTGP-7 as a fourth segment while Mario Kart 7 is open
    const bool ctgp7 = model.mods.ctgp7.shown;
    const std::vector<std::string> opts = ModsSegmentLabels(ctgp7);
    const Rect seg{c.x0, c.y0, c.x0 + kit.SegmentedWidth(opts), c.y0 + kSegH};
    const int hover = view.hover.id == Id::ModsSegment ? view.hover.index : -1;
    const int pressed = view.pressed.id == Id::ModsSegment ? view.pressed.index : -1;
    const ModsSegment segment =
        view.mods_segment == ModsSegment::Ctgp7 && !ctgp7 ? ModsSegment::Cheats : view.mods_segment;
    const int selected = static_cast<int>(segment);
    kit.Segmented(seg, opts, selected, hover, pressed);
    const auto e = kit.SegmentEdges(seg, opts);
    for (int i = 0; i < static_cast<int>(opts.size()); i++) {
        AddHit({e[static_cast<std::size_t>(i)], seg.y0, e[static_cast<std::size_t>(i + 1)], seg.y1},
               {Id::ModsSegment, i}, pane);
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
    }
}

} // namespace FrameXr::Menu
