// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame autosave: where the "Autosaved" label goes and whether it can be seen, with no
// OpenXR calls (unit tested in src/tests/frame_xr/autosave_label.cpp).
//
// The label is kLabelW x kLabelH metres in the top screen's plane, its bottom right corner
// kLabelGap over the screen's top right corner, so it moves with the screen and never covers the
// picture. It shows for kLabelNs with a kFadeNs fade in and out. It is used only when all its
// corners are in front of both eyes and at least kMarginDeg inside each eye's field of view, as
// decided when it is asked for (it never switches while it shows); otherwise the normal toast
// says "Autosaved".

#pragma once

#include <cstdint>

#include "frame_xr/input/types.h"

namespace FrameXr::Vis {

constexpr float kLabelW = 0.5f;
constexpr float kLabelH = 0.1f;
constexpr float kLabelGap = 0.03f;
constexpr float kMarginDeg = 10.0f;
constexpr int64_t kLabelNs = 1'500'000'000;
constexpr int64_t kFadeNs = 200'000'000;
/// The opacities drawn into the atlas (the fade steps through them).
constexpr int kLabelLevels = 4;

/// One eye: its pose and its field of view's half angles (radians, left and down negative), as
/// XrFovf.
struct EyeFov {
    Input::Posef pose;
    float left = 0.0f, right = 0.0f, up = 0.0f, down = 0.0f;
};

/// The label's centre pose for a top screen at `top` (its centre), `top_w` x `top_h` metres.
Input::Posef LabelPose(const Input::Posef& top, float top_w, float top_h);
/// Whether `point` is in front of `eye` and at least `margin_deg` inside its field of view.
bool PointInView(const EyeFov& eye, const Input::Vec3f& point, float margin_deg = kMarginDeg);
/// Whether the label at `label` (LabelPose) can be seen whole by both eyes.
bool LabelInView(const Input::Posef& label, const EyeFov& left, const EyeFov& right,
                 float margin_deg = kMarginDeg);
/// Its opacity `t_ns` after it began (0 before and after).
float LabelOpacity(int64_t t_ns);
/// The pre-drawn level for an opacity (0 none, else 1 to kLabelLevels).
int LabelLevel(float opacity);

} // namespace FrameXr::Vis
