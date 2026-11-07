// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/hud.h"
#include "frame_xr/session.h"
#include "frame_xr/ui/ui_canvas.h"

namespace FrameXr {

namespace {

constexpr uint32_t kToastWidth = 512;
constexpr uint32_t kToastHeight = 96;
constexpr int kToastKinds = 5;
// AzaharFrame vrsize: two rows for ShowToastText, used in turn (the one on view stays as it is
// while the other is copied).
constexpr int kTextRows = 2;
constexpr int kAtlasRows = kToastKinds + kTextRows;
// The atlas holds the toasts and, below them, a 16x16 transparent square: the passthrough layer's
// image (SteamVR allows 16 swapchains in a session, so they share one).
constexpr uint32_t kPassthroughSize = 16;
constexpr uint32_t kAtlasHeight = kToastHeight * kAtlasRows + kPassthroughSize;
constexpr XrTime kToastNs = 2'000'000'000;
constexpr float kToastDistance = 0.9f;  // metres ahead of the head
constexpr float kToastDrop = 0.25f;     // metres below eye level
constexpr float kToastWidthM = 0.45f;

const char* ToastText(int kind) {
    switch (static_cast<Toast>(kind)) {
    case Toast::QuickSaved:
        return "Quick Saved";
    case Toast::QuickLoaded:
        return "Quick Loaded";
    case Toast::NoQuickSave:
        return "No Quick Save";
    case Toast::FastForward:
        return "Fast Forward";
    case Toast::NormalSpeed:
        return "Normal Speed";
    }
    return "";
}

// One toast row: the rounded box and the text centred in it. The font has no degree sign, so a
// "°" in the text is drawn as a small ring.
void DrawRow(frame_xr::ui::Canvas& canvas, int row, const std::string& text) {
    namespace ui = frame_xr::ui;
    const int top_px = row * static_cast<int>(kToastHeight);
    std::fill(canvas.px.begin() + top_px * canvas.w,
              canvas.px.begin() + (top_px + static_cast<int>(kToastHeight)) * canvas.w, 0u);
    const float top = static_cast<float>(top_px);
    canvas.RoundRect(2, top + 2, kToastWidth - 2, top + kToastHeight - 2, 36,
                     ui::rgb(28, 28, 34, 0.92f));
    const ui::Color white = ui::rgb(240, 240, 245);
    const float baseline = top + 62;
    const std::size_t deg = text.find("\xC2\xB0");
    if (deg == std::string::npos) {
        canvas.Text(kToastWidth / 2.0f, baseline, ui::kFontLarge, text.c_str(), white, 1);
        return;
    }
    const std::string before = text.substr(0, deg);
    const std::string after = text.substr(deg + 2);
    constexpr float kRingR = 5.5f, kRingWidth = 15.0f;
    const float w_before = ui::TextWidth(ui::kFontLarge, before.c_str());
    const float w_after = ui::TextWidth(ui::kFontLarge, after.c_str());
    float x = kToastWidth / 2.0f - (w_before + kRingWidth + w_after) / 2.0f;
    canvas.Text(x, baseline, ui::kFontLarge, before.c_str(), white);
    x += w_before;
    const float cx = x + kRingWidth / 2.0f, cy = baseline - 24.0f;
    canvas.StrokeRoundRect(cx - kRingR, cy - kRingR, cx + kRingR, cy + kRingR, kRingR, 3.0f, white);
    canvas.Text(x + kRingWidth, baseline, ui::kFontLarge, after.c_str(), white);
}

} // namespace

void Hud::Create(Runtime& rt) {
    logged_unsupported = false;
    shown_kind = -1;
    toast_kind = -1;
    text_row_waiting = -1;
    {
        std::scoped_lock lock{text_mutex};
        text_requested = false;
    }

    if (toast_chain.Create(rt.session, rt.vulkan->Device(), rt.vulkan->PhysicalDevice(),
                           rt.vulkan->QueueFamily(), rt.queue_lock, rt.swapchain_format,
                           kToastWidth, kAtlasHeight)) {
        frame_xr::ui::InitFonts();
        atlas.Init(kToastWidth, kAtlasHeight); // all transparent black, then the toasts
        atlas.Clear(frame_xr::ui::rgb(0, 0, 0, 0.0f));
        for (int kind = 0; kind < kToastKinds; ++kind) {
            DrawRow(atlas, kind, ToastText(kind));
        }
        toast_chain.Upload(reinterpret_cast<const uint8_t*>(atlas.px.data()));
        toast_chain.Poll(); // starts the copy; released on a later frame
    }
}

void Hud::Destroy() {
    toast_chain.Destroy();
    shown_kind = -1;
    text_row_waiting = -1;
}

bool Hud::PassthroughActive(const Runtime& rt) {
    if (Settings::values.vr_background.GetValue() != 1) {
        return false;
    }
    if (!rt.alpha_blend_supported || !toast_chain.HasImage()) {
        if (!logged_unsupported) {
            logged_unsupported = true;
            LOG_WARNING(Frontend,
                        "xr: Background is Passthrough, but the runtime does not offer alpha "
                        "blending; the background stays black");
        }
        return false;
    }
    return true;
}

bool Hud::PassthroughLayer(const ViewPose& pose, XrSpace space,
                           XrCompositionLayerProjection& layer,
                           XrCompositionLayerProjectionView (&views)[2]) const {
    for (int eye = 0; eye < 2; ++eye) {
        if (pose.fov[eye].angleLeft == pose.fov[eye].angleRight) {
            return false; // not located yet
        }
        views[eye] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        views[eye].pose = pose.eye[eye];
        views[eye].fov = pose.fov[eye];
        views[eye].subImage.swapchain = toast_chain.Handle();
        views[eye].subImage.imageRect = {{0, static_cast<int32_t>(kAtlasHeight - kPassthroughSize)},
                                         {static_cast<int32_t>(kPassthroughSize),
                                          static_cast<int32_t>(kPassthroughSize)}};
        views[eye].subImage.imageArrayIndex = 0;
    }
    layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layer.space = space;
    layer.viewCount = 2;
    layer.views = views;
    return true;
}

void Hud::ShowToast(Toast kind) {
    toast_kind = static_cast<int>(kind);
}

void Hud::ShowToastText(const std::string& text) {
    std::scoped_lock lock{text_mutex};
    text_request = text;
    text_requested = true;
}

bool Hud::ToastQuad(const ViewPose& head, XrTime time, Quad& quad) {
    toast_chain.Poll();
    if (!toast_chain.HasImage()) {
        toast_kind = -1;
        return false;
    }
    // In front of where you look now, level, and it stays there.
    const auto ahead = [&head] {
        const XrQuaternionf q = head.head.orientation;
        const float fx = -2.0f * (q.x * q.z + q.w * q.y);
        const float fz = -(1.0f - 2.0f * (q.x * q.x + q.y * q.y));
        const float yaw = std::atan2(-fx, -fz);
        const float len = std::max(std::hypot(fx, fz), 1e-4f);
        XrPosef pose;
        pose.orientation = {0.0f, std::sin(yaw / 2), 0.0f, std::cos(yaw / 2)};
        pose.position = {head.head.position.x + fx / len * kToastDistance,
                         head.head.position.y - kToastDrop,
                         head.head.position.z + fz / len * kToastDistance};
        return pose;
    };
    const int requested = toast_kind.exchange(-1);
    if (requested >= 0 && requested < kToastKinds && head.valid) {
        shown_pose = ahead();
        shown_kind = requested;
        shown_since = time;
        text_row_waiting = -1;
    }
    // AzaharFrame vrsize: a text toast goes into the text row not on view; it shows once copied.
    {
        std::string text;
        bool has_text = false;
        {
            std::scoped_lock lock{text_mutex};
            if (text_requested) {
                text = std::move(text_request);
                text_requested = false;
                has_text = true;
            }
        }
        if (has_text && head.valid) {
            const int row = text_row_waiting >= 0      ? text_row_waiting
                            : shown_kind == kToastKinds ? kToastKinds + 1
                                                        : kToastKinds;
            DrawRow(atlas, row, text);
            toast_chain.Upload(reinterpret_cast<const uint8_t*>(atlas.px.data()));
            text_row_waiting = row;
        }
    }
    if (text_row_waiting >= 0 && !toast_chain.Busy()) {
        // Nudged again while the last one shows: it stays where it is, with the new text.
        const bool showing_text = shown_kind >= kToastKinds && time - shown_since <= kToastNs;
        if (!showing_text) {
            shown_pose = ahead();
        }
        shown_kind = text_row_waiting;
        shown_since = time;
        text_row_waiting = -1;
    }
    if (shown_kind >= 0 && time - shown_since > kToastNs) {
        shown_kind = -1;
    }
    const int row = shown_kind;
    if (row < 0) {
        return false;
    }
    quad.swapchain = toast_chain.Handle();
    quad.image_rect = {{0, static_cast<int32_t>(row * static_cast<int>(kToastHeight))},
                       {static_cast<int32_t>(kToastWidth), static_cast<int32_t>(kToastHeight)}};
    quad.pose = shown_pose;
    quad.size = {kToastWidthM, kToastWidthM * kToastHeight / kToastWidth};
    quad.eye = XR_EYE_VISIBILITY_BOTH;
    quad.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    return true;
}

} // namespace FrameXr
