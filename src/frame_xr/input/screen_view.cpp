// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/input/screen_view.h"

// AzaharFrame screenswap
namespace FrameXr::Input {

ScreenView::Effect ScreenView::SetBottomShown(bool managed, bool show) {
    Effect effect;
    if (managed) {
        if (immersive_bottom_shown_ != show) {
            immersive_bottom_shown_ = show;
            effect.toggle_immersive_bottom = true;
        }
    } else {
        bottom_hidden_ = !show;
    }
    return effect;
}

ScreenView::Effect ScreenView::Swap(bool managed) {
    Effect effect;
    if (!bottom_focus_) {
        peek_ = !BottomShown(managed);
        if (peek_) {
            effect = SetBottomShown(managed, true);
        }
        bottom_focus_ = true;
        top_hidden_ = false;
    } else {
        bottom_focus_ = false;
        top_hidden_ = false;
        if (peek_) {
            effect = SetBottomShown(managed, false);
        }
        peek_ = false;
    }
    return effect;
}

ScreenView::Effect ScreenView::ToggleBottom(bool managed) {
    if (bottom_focus_) {
        // The focused screen cannot be hidden: hiding it hands the focus back to the top screen.
        bottom_focus_ = false;
        top_hidden_ = false;
        peek_ = false;
        return SetBottomShown(managed, false);
    }
    return SetBottomShown(managed, !BottomShown(managed));
}

ScreenView::Effect ScreenView::ToggleOther(bool managed) {
    if (bottom_focus_) {
        top_hidden_ = !top_hidden_;
        return {};
    }
    return ToggleBottom(managed);
}

ScreenQuads ScreenView::Layout(const PanelQuad& main_slot, const PanelQuad& secondary_slot,
                               bool managed) const {
    ScreenQuads q;
    if (!bottom_focus_) {
        q.top = main_slot;
        q.bottom = secondary_slot;
        // Managed games: immersive.cpp decides (and its flags are applied by the caller).
        q.bottom.visible = secondary_slot.visible && (managed || !bottom_hidden_);
        return q;
    }
    const float bottom_aspect =
        secondary_slot.height_m > 0.0f ? secondary_slot.width_m / secondary_slot.height_m : 4.0f / 3.0f;
    const float top_aspect = main_slot.height_m > 0.0f ? main_slot.width_m / main_slot.height_m : 5.0f / 3.0f;
    q.bottom = {main_slot.pose, main_slot.height_m * bottom_aspect, main_slot.height_m,
                main_slot.visible};
    q.top = {secondary_slot.pose, secondary_slot.height_m * top_aspect, secondary_slot.height_m,
             secondary_slot.visible && !top_hidden_};
    return q;
}

Vec3f ScreenView::MainDragTarget(const Vec3f& mapper_target, const PanelQuad& mapper_top,
                                 const PanelQuad& main_slot) const {
    if (!bottom_focus_) {
        return mapper_target;
    }
    return main_slot.pose.position + (mapper_target - mapper_top.pose.position);
}

} // namespace FrameXr::Input
