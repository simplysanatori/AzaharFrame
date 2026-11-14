// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame imglag: which game picture the compositor shows on a display refresh.
//
// SteamVR shows an image one refresh after the pose sent with it: the image an xrEndFrame carries
// is on view at the NEXT xrEndFrame's display time, with that frame's layer pose (the image lag,
// 1 refresh). The anchored immersive screen must be placed for the picture that will be on view,
// not for the one the frame carries, or each new picture is shown once with the pose of the next
// one: a double image on every head turn, whatever the hold (38/dbl/advisor.md, cause 1a).
//
// So the XR thread records the release serial each xrEndFrame actually carried in its top screen
// layer (SentAfterEnd), and the picture on view is found by that serial (OnViewSerial,
// FindPicture): no counting of a picture's refreshes, so a picture never on view, a gap in the
// serials, a picture without a pose, a frame without the top layer and the 250 ms limit are all
// judged on the right picture. Pure (unit-tested).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include "video_core/xr_content_pose.h"

namespace FrameXr {

/// AzaharFrame heldquad: a picture released, its release serial (ScreenChain::frame_serial) and
/// the head pose it was drawn with.
struct ShownPicture {
    uint64_t serial = 0; ///< 0: none
    VideoCore::XrContentPose content{};
};
/// AzaharFrame imglag: 8 (was 4): the picture on view is found by its serial, one or more
/// releases behind the newest (two releases inside one refresh, a frame loop that fell behind).
inline constexpr std::size_t kShownPictures = 8;

namespace ImageLag {

/// The runtime's image lag in refreshes: 0 (the image an xrEndFrame carries is on view at that
/// frame's display time) or 1 (at the next one's: SteamVR 2.x on the Steam Frame).
inline constexpr uint32_t kMaxLag = 1;
inline constexpr uint32_t kDefaultLag = 1;

/// The live test-switch override (AZAHARFRAME_XR_IMAGE_LAG): "0" or "1"; anything else, or unset:
/// none (the setting holds).
inline std::optional<uint32_t> OverrideFrom(const std::optional<std::string>& v) {
    if (v && (*v == "0" || *v == "1")) {
        return static_cast<uint32_t>((*v)[0] - '0');
    }
    return std::nullopt;
}

/// The lag in effect: the override, else the setting (vr_xr_image_lag) clamped to 0..kMaxLag.
inline uint32_t InEffect(uint32_t setting, std::optional<uint32_t> override_lag) {
    if (override_lag) {
        return *override_lag;
    }
    return setting > kMaxLag ? kMaxLag : setting;
}

/// What an xrEndFrame leaves in the compositor's pipeline for the next refresh (Runtime::
/// sent_serial, set after each xrEndFrame): the release serial of the top screen's image when the
/// frame's layers carried the top screen (`carried`: its layer was added, not hidden by
/// SINGLE_FLASH, not another screen in focus), the runtime asked for the frame to be rendered and
/// the call succeeded; else 0, nothing (the compositor has no older image of the layer then).
inline uint64_t SentAfterEnd(bool carried, bool should_render, bool succeeded, uint64_t serial) {
    return carried && should_render && succeeded ? serial : 0;
}

/// Why a picture is the one on view (for the per-second log line).
enum class Source : uint8_t {
    Newest,  ///< lag 0: the newest released, the one this frame carries
    Sent,    ///< lag 1: the one the last xrEndFrame carried
    NoSent,  ///< lag 1, but the last xrEndFrame had no top layer (or none yet): the newest
    Gone,    ///< lag 1, the one sent is no longer among the pictures kept: the newest
    None,    ///< no picture released
};

struct OnView {
    ShownPicture picture; ///< serial 0: none
    Source source = Source::None;
};

/// The picture with `serial` among `pics` (newest first), nullptr when it is not there.
template <std::size_t N>
const ShownPicture* FindPicture(const std::array<ShownPicture, N>& pics, uint64_t serial) {
    if (serial == 0) {
        return nullptr;
    }
    for (const ShownPicture& p : pics) {
        if (p.serial == serial) {
            return &p;
        }
    }
    return nullptr;
}

/// The picture on view on this display refresh: `pics` the last pictures released (newest
/// first), `sent` the serial the last xrEndFrame carried (SentAfterEnd), `lag` InEffect. With lag 1 the one the last xrEndFrame
/// carried (by serial, whatever came between: a picture never on view, a serial gap); when that
/// frame had no top layer the compositor has no older image of it, so the newest is shown at once.
template <std::size_t N>
OnView PictureOnView(const std::array<ShownPicture, N>& pics, uint64_t sent, uint32_t lag) {
    const ShownPicture& newest = pics[0];
    if (newest.serial == 0) {
        return {};
    }
    if (lag == 0) {
        return {newest, Source::Newest};
    }
    if (sent == 0) {
        return {newest, Source::NoSent};
    }
    if (const ShownPicture* p = FindPicture(pics, sent)) {
        return {*p, Source::Sent};
    }
    return {newest, Source::Gone};
}

/// The serial on view for SingleFlash and the log: as PictureOnView, from the newest serial alone.
inline uint64_t SerialOnView(uint64_t newest, uint64_t sent, uint32_t lag) {
    return lag == 0 || sent == 0 ? newest : sent;
}

/// Whether the picture on view anchors the screen at `display` (Immersive's AnchorPose): it has a
/// pose, drawn no earlier than the last recentre, at most `max_ms` old (and not more than 100 ms
/// ahead). Judged on the picture on view, not the newest one released.
inline bool AnchorUsable(const VideoCore::XrContentPose& c, int64_t display, int64_t recentre_time,
                         double max_ms) {
    if (!c.valid || c.time < recentre_time) {
        return false;
    }
    const double age_ms = static_cast<double>(display - c.time) / 1e6;
    return age_ms <= max_ms && age_ms >= -100.0;
}

} // namespace ImageLag
} // namespace FrameXr
