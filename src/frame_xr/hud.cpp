// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstring>

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
constexpr int kAtlasRows = kToastKinds;
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

} // namespace

void Hud::Create(Runtime& rt) {
    uploader.Init(rt.vulkan->Device(), rt.vulkan->PhysicalDevice(), rt.vulkan->QueueFamily(),
                  rt.queue_lock);
    logged_unsupported = false;
    shown_kind = -1;
    toast_kind = -1;

    if (uploader.CreateChain(rt.session, rt.swapchain_format, kToastWidth, kAtlasHeight,
                             toast_chain)) {
        frame_xr::ui::InitFonts();
        frame_xr::ui::Canvas canvas;
        canvas.Init(kToastWidth, kAtlasHeight); // all transparent black, then the toasts
        canvas.Clear(frame_xr::ui::rgb(0, 0, 0, 0.0f));
        for (int kind = 0; kind < kAtlasRows; ++kind) {
            const float top = static_cast<float>(kind * kToastHeight);
            canvas.RoundRect(2, top + 2, kToastWidth - 2, top + kToastHeight - 2, 36,
                             frame_xr::ui::rgb(28, 28, 34, 0.92f));
            canvas.Text(kToastWidth / 2.0f, top + 62, frame_xr::ui::kFontLarge, ToastText(kind),
                        frame_xr::ui::rgb(240, 240, 245), 1);
        }
        uploader.Upload(toast_chain, reinterpret_cast<const uint8_t*>(canvas.px.data()));
    }
}

void Hud::Destroy() {
    uploader.DestroyChain(toast_chain);
    toast_chain = {};
    uploader.Shutdown();
    shown_kind = -1;
}

bool Hud::PassthroughActive(const Runtime& rt) {
    if (Settings::values.vr_background.GetValue() != 1) {
        return false;
    }
    if (!rt.alpha_blend_supported || !toast_chain.has_image) {
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
        views[eye].subImage.swapchain = toast_chain.handle;
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

bool Hud::ToastQuad(const ViewPose& head, XrTime time, Quad& quad) {
    if (!toast_chain.has_image) {
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
    }
    if (shown_kind >= 0 && time - shown_since > kToastNs) {
        shown_kind = -1;
    }
    const int row = shown_kind;
    if (row < 0) {
        return false;
    }
    quad.swapchain = toast_chain.handle;
    quad.image_rect = {{0, static_cast<int32_t>(row * static_cast<int>(kToastHeight))},
                       {static_cast<int32_t>(kToastWidth), static_cast<int32_t>(kToastHeight)}};
    quad.pose = shown_pose;
    quad.size = {kToastWidthM, kToastWidthM * kToastHeight / kToastWidth};
    quad.eye = XR_EYE_VISIBILITY_BOTH;
    quad.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    return true;
}

} // namespace FrameXr
