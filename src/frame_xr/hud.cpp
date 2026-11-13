// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <string>

#include "common/logging/log.h"
#include "common/settings.h"
#include "frame_xr/hud.h"
#include "frame_xr/immersive_layout.h" // AzaharFrame shaderbadge: BothEyes
#include "frame_xr/menu/md3.h"         // AzaharFrame shaderbadge: the menu's colours
#include "frame_xr/menu/md3_fonts.h"   // AzaharFrame shaderbadge: the badge's own font
#include "frame_xr/menu/tab_menu.h" // AzaharFrame autosave: DrawAutosavedLabel
#include "frame_xr/session.h"
#include "frame_xr/toast_fit.h" // AzaharFrame menuux
#include "frame_xr/ui/ui_canvas.h"
#include "frame_xr/vis/autosave_label.h" // AzaharFrame autosave
#include "frame_xr/vis/shader_badge.h"   // AzaharFrame shaderbadge

namespace FrameXr {

namespace {

// AzaharFrame menuux: rows as wide as the longest toast may need (toast_fit.h); each toast's box
// is as wide as its text. The hint chip keeps its 512.
constexpr uint32_t kToastWidth = ToastFit::kAtlasW;
constexpr uint32_t kHintWidth = 512;
constexpr uint32_t kToastHeight = ToastFit::kRowPx;
constexpr int kToastKinds = 6; // AzaharFrame autosave: Autosaved
// AzaharFrame vrsize: two rows for ShowToastText, used in turn (the one on view stays as it is
// while the other is copied).
constexpr int kTextRows = 2;
// AzaharFrame controls: one more row for the grip hint chip (ShowHint), two small lines.
constexpr int kHintRow = kToastKinds + kTextRows;
// AzaharFrame autosave: two rows for the "Autosaved" label at Vis::kLabelLevels opacities, two to
// a row (kLabelPx wide each), drawn once at the start.
constexpr uint32_t kLabelPx = 480;
constexpr int kLabelRow = kHintRow + 1;
constexpr int kLabelRows = (Vis::kLabelLevels + 1) / 2;
constexpr int kAtlasRows = kToastKinds + kTextRows + 1 + kLabelRows;
constexpr XrTime kHintNs = 3'000'000'000;
// AzaharFrame shaderbadge: below the rows, the badge at Vis::Badge::kLevels opacities, two to a
// band (kBadgePxW x kBadgePxH each), drawn again when its line changes, at most every
// kBadgeRedrawNs (each change uploads the whole atlas).
constexpr int kBadgePxW = 512;
constexpr int kBadgePxH = 64;
constexpr int kBadgeY = static_cast<int>(kToastHeight) * kAtlasRows;
constexpr int kBadgeBands = (Vis::Badge::kLevels + 1) / 2;
constexpr XrTime kBadgeRedrawNs = 250'000'000;
// The atlas holds the toasts and, below them, a 16x16 transparent square: the passthrough layer's
// image (SteamVR allows 16 swapchains in a session, so they share one).
constexpr uint32_t kPassthroughSize = 16;
constexpr uint32_t kAtlasHeight =
    kToastHeight * kAtlasRows + kBadgePxH * kBadgeBands + kPassthroughSize;
constexpr float kToastDistance = 0.9f;  // metres ahead of the head
constexpr float kToastDrop = 0.25f;     // metres below eye level

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
    case Toast::Autosaved: // AzaharFrame autosave
        return "Autosaved";
    }
    return "";
}

// A line of a toast centred on `cx`. The font has no degree sign, so a "°" in the text is
// drawn as a small ring.
void DrawToastLine(frame_xr::ui::Canvas& canvas, int font, float cx, float baseline,
                   const std::string& text, const frame_xr::ui::Color& color) {
    namespace ui = frame_xr::ui;
    const std::size_t deg = text.find("\xC2\xB0");
    if (deg == std::string::npos) {
        canvas.Text(cx, baseline, font, text.c_str(), color, 1);
        return;
    }
    const float k = font == ui::kFontLarge ? 1.0f : 0.7f; // 40 and 28 px fonts
    const std::string before = text.substr(0, deg);
    const std::string after = text.substr(deg + 2);
    const float ring_r = 5.5f * k, ring_w = 15.0f * k;
    const float w_before = ui::TextWidth(font, before.c_str());
    const float w_after = ui::TextWidth(font, after.c_str());
    float x = cx - (w_before + ring_w + w_after) / 2.0f;
    canvas.Text(x, baseline, font, before.c_str(), color);
    x += w_before;
    const float rx = x + ring_w / 2.0f, ry = baseline - 24.0f * k;
    canvas.StrokeRoundRect(rx - ring_r, ry - ring_r, rx + ring_r, ry + ring_r, ring_r, 3.0f * k,
                           color);
    canvas.Text(x + ring_w, baseline, font, after.c_str(), color);
}

// The width of a toast line ("°" as the ring).
float ToastLineWidth(int font, const std::string& text) {
    namespace ui = frame_xr::ui;
    const std::size_t deg = text.find("\xC2\xB0");
    if (deg == std::string::npos) {
        return ui::TextWidth(font, text.c_str());
    }
    const float k = font == ui::kFontLarge ? 1.0f : 0.7f;
    return ui::TextWidth(font, text.substr(0, deg).c_str()) + 15.0f * k +
           ui::TextWidth(font, text.substr(deg + 2).c_str());
}

// One toast row: the rounded box, as wide as the text needs (AzaharFrame menuux: toast_fit.h),
// and the text centred in it; one line in the large font, else two in the small one.
ToastFit::Box DrawRow(frame_xr::ui::Canvas& canvas, int row, const std::string& text) {
    namespace ui = frame_xr::ui;
    std::vector<std::string> lines;
    const ToastFit::Box box =
        ToastFit::Fit(ToastLineWidth(ui::kFontLarge, text), [&] {
            lines = ui::WrapText(ui::kFontSmall, text,
                                 static_cast<float>(kToastWidth) - 2.0f * ToastFit::kPadPx, 2);
            float w = 0.0f;
            for (const std::string& l : lines) {
                w = std::max(w, ToastLineWidth(ui::kFontSmall, l));
            }
            return w;
        });
    const int top_px = row * static_cast<int>(kToastHeight);
    std::fill(canvas.px.begin() + top_px * canvas.w,
              canvas.px.begin() + (top_px + static_cast<int>(kToastHeight)) * canvas.w, 0u);
    const float top = static_cast<float>(top_px);
    const float w = static_cast<float>(box.box_px);
    canvas.RoundRect(2, top + 2, w - 2, top + kToastHeight - 2, 36, ui::rgb(28, 28, 34, 0.92f));
    const ui::Color white = ui::rgb(240, 240, 245);
    if (!box.two_lines) {
        DrawToastLine(canvas, ui::kFontLarge, w / 2.0f, top + 62, text, white);
    } else if (lines.size() == 1) {
        DrawToastLine(canvas, ui::kFontSmall, w / 2.0f, top + 58, lines[0], white);
    } else if (lines.size() >= 2) {
        DrawToastLine(canvas, ui::kFontSmall, w / 2.0f, top + 40, lines[0], white);
        DrawToastLine(canvas, ui::kFontSmall, w / 2.0f, top + 78, lines[1], white);
    }
    return box;
}

// AzaharFrame controls: a line of the hint chip in the small font, centred at `y` (baseline).
// "↕" and "↔" (UTF-8) are drawn as small double arrows, which the font does not have.
void DrawHintLine(frame_xr::ui::Canvas& canvas, float baseline, const std::string& text,
                  const frame_xr::ui::Color& color) {
    namespace ui = frame_xr::ui;
    constexpr float kArrowWidth = 26.0f;
    struct Piece {
        std::string text;
        int arrow = 0; ///< 0 text, 1 up-down, 2 left-right
    };
    std::vector<Piece> pieces;
    std::string run;
    for (std::size_t i = 0; i < text.size();) {
        if (text.compare(i, 3, "\xE2\x86\x95") == 0 || text.compare(i, 3, "\xE2\x86\x94") == 0) {
            if (!run.empty()) {
                pieces.push_back({run, 0});
                run.clear();
            }
            pieces.push_back({"", text[i + 2] == '\x95' ? 1 : 2});
            i += 3;
        } else {
            run += text[i++];
        }
    }
    if (!run.empty()) {
        pieces.push_back({run, 0});
    }
    float width = 0.0f;
    for (const Piece& p : pieces) {
        width += p.arrow ? kArrowWidth : ui::TextWidth(ui::kFontSmall, p.text.c_str());
    }
    float x = kHintWidth / 2.0f - width / 2.0f;
    for (const Piece& p : pieces) {
        if (!p.arrow) {
            canvas.Text(x, baseline, ui::kFontSmall, p.text.c_str(), color);
            x += ui::TextWidth(ui::kFontSmall, p.text.c_str());
            continue;
        }
        // A stem and a head at each end, from rectangles (2 px steps).
        const float cx = x + kArrowWidth / 2.0f, cy = baseline - 10.0f;
        constexpr float kHalf = 10.0f, kStem = 1.5f, kHead = 6.0f;
        if (p.arrow == 1) {
            canvas.FillRect(cx - kStem, cy - kHalf, cx + kStem, cy + kHalf, color);
            for (float k = 0; k < kHead; k += 2.0f) {
                canvas.FillRect(cx - k - 1, cy - kHalf + k, cx + k + 1, cy - kHalf + k + 2, color);
                canvas.FillRect(cx - k - 1, cy + kHalf - k - 2, cx + k + 1, cy + kHalf - k, color);
            }
        } else {
            canvas.FillRect(cx - kHalf, cy - kStem, cx + kHalf, cy + kStem, color);
            for (float k = 0; k < kHead; k += 2.0f) {
                canvas.FillRect(cx - kHalf + k, cy - k - 1, cx - kHalf + k + 2, cy + k + 1, color);
                canvas.FillRect(cx + kHalf - k - 2, cy - k - 1, cx + kHalf - k, cy + k + 1, color);
            }
        }
        x += kArrowWidth;
    }
}

// The hint chip: up to two lines ('\n'), small, in a box like the toasts.
void DrawHintRow(frame_xr::ui::Canvas& canvas, const std::string& text) {
    namespace ui = frame_xr::ui;
    const int top_px = kHintRow * static_cast<int>(kToastHeight);
    std::fill(canvas.px.begin() + top_px * canvas.w,
              canvas.px.begin() + (top_px + static_cast<int>(kToastHeight)) * canvas.w, 0u);
    const float top = static_cast<float>(top_px);
    canvas.RoundRect(2, top + 2, kHintWidth - 2, top + kToastHeight - 2, 30,
                     ui::rgb(28, 28, 34, 0.88f));
    const ui::Color white = ui::rgb(240, 240, 245);
    const std::size_t nl = text.find('\n');
    if (nl == std::string::npos) {
        DrawHintLine(canvas, top + 58, text, white);
    } else {
        DrawHintLine(canvas, top + 40, text.substr(0, nl), white);
        DrawHintLine(canvas, top + 78, text.substr(nl + 1), white);
    }
}

// AzaharFrame autosave: the label once, then at each level of opacity into its rows (the canvas is
// premultiplied, so every channel scales).
void DrawLabels(frame_xr::ui::Canvas& atlas) {
    frame_xr::ui::Canvas one;
    one.Init(static_cast<int>(kLabelPx), static_cast<int>(kToastHeight));
    one.Clear(frame_xr::ui::rgb(0, 0, 0, 0.0f));
    Menu::Kit kit(1.0f);
    kit.Begin(one);
    Menu::DrawAutosavedLabel(kit, {2.0f, 2.0f, kLabelPx - 2.0f, kToastHeight - 2.0f});
    for (int level = 1; level <= Vis::kLabelLevels; level++) {
        const float a = static_cast<float>(level) / Vis::kLabelLevels;
        const int x0 = ((level - 1) % 2) * 512;
        const int y0 = (kLabelRow + (level - 1) / 2) * static_cast<int>(kToastHeight);
        for (int y = 0; y < static_cast<int>(kToastHeight); y++) {
            for (int x = 0; x < static_cast<int>(kLabelPx); x++) {
                const uint32_t p = one.px[static_cast<std::size_t>(y * one.w + x)];
                uint32_t out = 0;
                for (int c = 0; c < 4; c++) {
                    const float v = static_cast<float>((p >> (c * 8)) & 0xFF) * a;
                    out |= static_cast<uint32_t>(std::lround(v)) << (c * 8);
                }
                atlas.px[static_cast<std::size_t>((y0 + y) * atlas.w + x0 + x)] = out;
            }
        }
    }
}

// AzaharFrame shaderbadge: the badge once into `one` (kBadgePxW x kBadgePxH): a dark pill as the
// "Autosaved" label's, a determinate progress ring in the menu's primary colour on its track, and
// the line in the menu's font (Medium). Returns the pill's width.
int DrawBadge(frame_xr::ui::Canvas& one, frame_xr::ui::TtfFont& font, const std::string& text,
              float progress) {
    namespace ui = frame_xr::ui;
    namespace Tok = Menu::Tok;
    const auto color = [](uint32_t rgb, float a = 1.0f) {
        return ui::rgb(static_cast<int>(rgb >> 16 & 0xFF), static_cast<int>(rgb >> 8 & 0xFF),
                       static_cast<int>(rgb & 0xFF), a);
    };
    constexpr float kH = static_cast<float>(kBadgePxH);
    constexpr float kPadL = 18.0f, kPadR = 24.0f, kRing = 30.0f, kRingW = 4.5f, kGap = 12.0f;
    constexpr float kTextPx = 27.0f;
    const float text_x = kPadL + kRing + kGap;
    const float max_text = static_cast<float>(kBadgePxW) - 2.0f - text_x - kPadR;
    const std::string line = font.Ok() ? font.Fit(kTextPx, text, max_text) : text;
    const float tw = font.Ok() ? font.Width(kTextPx, line) : 0.0f;
    const float w = std::min(static_cast<float>(kBadgePxW) - 2.0f, std::ceil(text_x + tw + kPadR));
    one.Clear(ui::rgb(0, 0, 0, 0.0f));
    one.RoundRect(1.0f, 1.0f, w, kH - 1.0f, (kH - 2.0f) * 0.5f, color(0x1C1C22, 0.88f));
    // The ring: its track, and the arc clockwise from the top for `progress`.
    const float cx = kPadL + kRing * 0.5f, cy = kH * 0.5f, r = (kRing - kRingW) * 0.5f;
    const float sweep = std::clamp(progress, 0.0f, 1.0f) * 6.2831853f;
    const ui::Color track = color(Tok::kSecondaryContainer), arc = color(Tok::kPrimary);
    for (int y = static_cast<int>(cy - kRing); y <= static_cast<int>(cy + kRing); y++) {
        for (int x = static_cast<int>(cx - kRing); x <= static_cast<int>(cx + kRing); x++) {
            const float dx = static_cast<float>(x) + 0.5f - cx,
                        dy = static_cast<float>(y) + 0.5f - cy;
            const float cover =
                std::clamp(kRingW * 0.5f + 0.5f - std::abs(std::hypot(dx, dy) - r), 0.0f, 1.0f);
            if (cover <= 0.0f) {
                continue;
            }
            float angle = std::atan2(dx, -dy); // 0 at the top, clockwise
            if (angle < 0.0f) {
                angle += 6.2831853f;
            }
            one.Blend(x, y, angle <= sweep ? arc : track, cover);
        }
    }
    if (font.Ok()) {
        font.Draw(one, text_x, cy + font.CapHeight(kTextPx) * 0.5f, kTextPx, color(0xF0F0F5), line);
    }
    return static_cast<int>(w) + 1;
}

// Copies `one` into the atlas at each level of opacity (premultiplied: every channel scales).
void CopyBadgeLevels(const frame_xr::ui::Canvas& one, frame_xr::ui::Canvas& atlas) {
    for (int level = 1; level <= Vis::Badge::kLevels; level++) {
        const float a = static_cast<float>(level) / Vis::Badge::kLevels;
        const int x0 = ((level - 1) % 2) * kBadgePxW;
        const int y0 = kBadgeY + ((level - 1) / 2) * kBadgePxH;
        for (int y = 0; y < kBadgePxH; y++) {
            for (int x = 0; x < kBadgePxW; x++) {
                const uint32_t p = one.px[static_cast<std::size_t>(y * one.w + x)];
                uint32_t out = 0;
                for (int c = 0; c < 4; c++) {
                    const float v = static_cast<float>((p >> (c * 8)) & 0xFF) * a;
                    out |= static_cast<uint32_t>(std::lround(v)) << (c * 8);
                }
                atlas.px[static_cast<std::size_t>((y0 + y) * atlas.w + x0 + x)] = out;
            }
        }
    }
}

Input::Posef ToInput(const XrPosef& p) {
    Input::Posef q;
    q.orientation = {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w};
    q.position = {p.position.x, p.position.y, p.position.z};
    return q;
}

XrPosef ToXrPose(const Input::Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
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
        badge_requested = true; // AzaharFrame shaderbadge: the last request again, in this atlas
    }
    badge_drawn.clear();
    badge_drawn_progress = -1.0f;
    badge_waiting = false;
    badge_ready = false;
    badge_opacity = 0.0f;
    badge_time = 0;

    if (toast_chain.Create(rt.session, rt.vulkan->Device(), rt.vulkan->PhysicalDevice(),
                           rt.vulkan->QueueFamily(), rt.queue_lock, rt.swapchain_format,
                           kToastWidth, kAtlasHeight)) {
        frame_xr::ui::InitFonts();
        atlas.Init(kToastWidth, kAtlasHeight); // all transparent black, then the toasts
        atlas.Clear(frame_xr::ui::rgb(0, 0, 0, 0.0f));
        row_boxes.assign(kAtlasRows, ToastFit::Box{});
        for (int kind = 0; kind < kToastKinds; ++kind) {
            row_boxes[static_cast<std::size_t>(kind)] = DrawRow(atlas, kind, ToastText(kind));
        }
        DrawLabels(atlas); // AzaharFrame autosave
        if (!badge_font.Ok()) { // AzaharFrame shaderbadge: decoded once, used on the XR thread only
            badge_font.LoadBase64(Menu::Fonts::kSans500);
        }
        toast_chain.Upload(reinterpret_cast<const uint8_t*>(atlas.px.data()));
        toast_chain.Poll(); // starts the copy; released on a later frame
    }
}

void Hud::Destroy() {
    toast_chain.Destroy();
    shown_kind = -1;
    text_row_waiting = -1;
    badge_ready = false; // AzaharFrame shaderbadge
    badge_waiting = false;
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

void Hud::ShowToastText(const std::string& text, ToastTime time) {
    std::scoped_lock lock{text_mutex};
    text_request = text;
    text_request_time = time;
    text_requested = true;
}

void Hud::HideToastText(const std::string& text) {
    std::scoped_lock lock{text_mutex};
    if (text_requested && text_request == text) {
        text_requested = false; // not shown yet: never
    }
    hide_request = text;
    hide_requested = true;
}

void Hud::ShowHint(const std::string& text, const XrPosef& pose, float width_m) {
    std::scoped_lock lock{text_mutex};
    hint_request = text;
    hint_request_pose = pose;
    hint_request_width = width_m;
    hint_requested = true;
}

bool Hud::HintQuad(XrTime time, Quad& quad) {
    if (!toast_chain.HasImage()) {
        return false;
    }
    {
        std::scoped_lock lock{text_mutex};
        if (hint_requested) {
            hint_requested = false;
            DrawHintRow(atlas, hint_request);
            toast_chain.Upload(reinterpret_cast<const uint8_t*>(atlas.px.data()));
            hint_waiting = true;
            hint_pose = hint_request_pose;
            hint_width = hint_request_width;
            hint_shown = false;
        }
    }
    if (hint_waiting && !toast_chain.Busy()) {
        hint_waiting = false;
        hint_shown = true;
        hint_since = time;
    }
    if (!hint_shown || time - hint_since > kHintNs) {
        hint_shown = false;
        return false;
    }
    quad.swapchain = toast_chain.Handle();
    quad.image_rect = {{0, static_cast<int32_t>(kHintRow * static_cast<int>(kToastHeight))},
                       {static_cast<int32_t>(kHintWidth), static_cast<int32_t>(kToastHeight)}};
    quad.pose = hint_pose;
    quad.size = {hint_width, hint_width * kToastHeight / kHintWidth};
    quad.eye = XR_EYE_VISIBILITY_BOTH;
    quad.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    return true;
}

// AzaharFrame autosave
void Hud::ShowAutosaved() {
    label_requested = true;
}

bool Hud::LabelQuad(const ViewPose& head, bool top_ok, const XrPosef& top_pose,
                    XrExtent2Df top_size, XrTime time, Quad& quad) {
    if (!toast_chain.HasImage()) {
        label_requested = false;
        return false;
    }
    const auto to_input = [](const XrPosef& p) {
        Input::Posef q;
        q.orientation = {p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w};
        q.position = {p.position.x, p.position.y, p.position.z};
        return q;
    };
    const Input::Posef label = Vis::LabelPose(to_input(top_pose), top_size.width, top_size.height);
    if (label_requested.exchange(false)) {
        bool in_view = false;
        if (top_ok && head.valid) {
            Vis::EyeFov eyes[2];
            for (int e = 0; e < 2; e++) {
                eyes[e].pose = to_input(head.eye[e]);
                eyes[e].left = head.fov[e].angleLeft;
                eyes[e].right = head.fov[e].angleRight;
                eyes[e].up = head.fov[e].angleUp;
                eyes[e].down = head.fov[e].angleDown;
            }
            in_view = Vis::LabelInView(label, eyes[0], eyes[1]);
        }
        if (in_view) {
            label_shown = true;
            label_since = time;
        } else {
            label_shown = false;
            toast_kind = static_cast<int>(Toast::Autosaved);
        }
        LOG_INFO(Frontend, "xr: autosave: {}",
                 in_view ? "the label over the top screen"
                 : top_ok
                     ? "the toast (the corner is not in view)"
                     : "the toast (immersive mode, or the top screen is not placed in the room)");
    }
    if (!label_shown) {
        return false;
    }
    const int level = Vis::LabelLevel(Vis::LabelOpacity(time - label_since));
    if (level == 0 || !top_ok) {
        label_shown = level != 0 && top_ok;
        return false;
    }
    quad.swapchain = toast_chain.Handle();
    quad.image_rect = {
        {((level - 1) % 2) * 512,
         static_cast<int32_t>((kLabelRow + (level - 1) / 2) * static_cast<int>(kToastHeight))},
        {static_cast<int32_t>(kLabelPx), static_cast<int32_t>(kToastHeight)}};
    quad.pose.orientation = {label.orientation.x, label.orientation.y, label.orientation.z,
                             label.orientation.w};
    quad.pose.position = {label.position.x, label.position.y, label.position.z};
    quad.size = {Vis::kLabelW, Vis::kLabelW * kToastHeight / kLabelPx};
    quad.eye = XR_EYE_VISIBILITY_BOTH;
    quad.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    return true;
}

// AzaharFrame shaderbadge
void Hud::SetShaderBadge(const std::string& text, float progress) {
    std::scoped_lock lock{text_mutex};
    badge_request = text;
    badge_request_progress = progress;
    badge_requested = true;
}

bool Hud::BadgeQuad(const ViewPose& head, const BadgeAnchor& anchor, XrTime time, Quad& quad,
                    bool& in_view_space) {
    namespace B = Vis::Badge;
    in_view_space = false;
    {
        std::scoped_lock lock{text_mutex};
        if (badge_requested) {
            badge_requested = false;
            badge_want = badge_request;
            badge_want_progress = badge_request_progress;
        }
    }
    const XrTime dt = badge_time == 0 ? 0 : std::min<XrTime>(time - badge_time, 100'000'000);
    badge_time = time;
    if (!toast_chain.HasImage()) {
        return false;
    }
    // A new line (or ring) is drawn and copied; the one on view stays until the copy is done.
    const bool changed =
        badge_want != badge_drawn || std::abs(badge_want_progress - badge_drawn_progress) > 0.005f;
    if (!badge_want.empty() && changed && !badge_waiting &&
        (badge_opacity == 0.0f || time - badge_drawn_at >= kBadgeRedrawNs)) {
        frame_xr::ui::Canvas one;
        one.Init(kBadgePxW, kBadgePxH);
        badge_waiting_px = DrawBadge(one, badge_font, badge_want, badge_want_progress);
        CopyBadgeLevels(one, atlas);
        toast_chain.Upload(reinterpret_cast<const uint8_t*>(atlas.px.data()));
        badge_waiting = true;
        badge_drawn = badge_want;
        badge_drawn_progress = badge_want_progress;
        badge_drawn_at = time;
    }
    if (badge_waiting && !toast_chain.Busy()) {
        badge_waiting = false;
        badge_ready = true;
        badge_box_px = badge_waiting_px;
    }
    // Not faded in with the last showing's line while the new one is still being copied.
    const bool show =
        !badge_want.empty() && badge_ready && !(badge_opacity == 0.0f && badge_waiting);
    const float aspect = static_cast<float>(badge_box_px) / static_cast<float>(kBadgePxH);
    const auto on_screen = [&] {
        return B::OnScreen(ToInput(anchor.screen_pose), anchor.screen_size.width,
                           anchor.screen_size.height, anchor.curve, aspect);
    };
    if (show && badge_opacity == 0.0f) {
        // It appears: on the normal screen's corner when that is well in view, else head-locked.
        bool corner_in_view = false;
        if (anchor.screen && head.valid) {
            const B::Placed p = on_screen();
            Vis::EyeFov eyes[2];
            for (int e = 0; e < 2; e++) {
                eyes[e].pose = ToInput(head.eye[e]);
                eyes[e].left = head.fov[e].angleLeft;
                eyes[e].right = head.fov[e].angleRight;
                eyes[e].up = head.fov[e].angleUp;
                eyes[e].down = head.fov[e].angleDown;
            }
            // The badge is not the label's size: its own corners, at a smaller margin.
            corner_in_view = true;
            for (const float sx : {-0.5f, 0.5f}) {
                for (const float sy : {-0.5f, 0.5f}) {
                    const Input::Vec3f c =
                        Input::Transform(p.pose, {sx * p.width, sy * p.height, 0});
                    corner_in_view = corner_in_view && Vis::PointInView(eyes[0], c, 5.0f) &&
                                     Vis::PointInView(eyes[1], c, 5.0f);
                }
            }
        }
        badge_head = !corner_in_view;
        LOG_INFO(Frontend, "xr: shader badge: {} ({})",
                 corner_in_view     ? "in the main screen's corner"
                 : anchor.screen    ? "head-locked (the screen's corner is not in view)"
                 : anchor.immersive ? "head-locked on the immersive screen"
                                    : "head-locked (no game screen)",
                 badge_want);
    }
    badge_opacity = B::Fade(badge_opacity, show, dt);
    const int level = B::Level(badge_opacity);
    if (level == 0 || !badge_ready || !head.valid) {
        return false;
    }
    B::Placed placed;
    if (!badge_head && anchor.screen) {
        placed = on_screen();
    } else {
        // Head-locked: what both eyes see, and the part of the immersive screen in it.
        Immersive::Layout::ViewFov view;
        if (head.views_valid) {
            const Input::Quatf head_inv = Input::Conjugate(ToInput(head.head).orientation);
            Immersive::Layout::EyeFov eyes[2];
            for (int e = 0; e < 2; e++) {
                const Input::Vec3f f = Input::Rotate(
                    head_inv, Input::Rotate(ToInput(head.eye[e]).orientation, {0.0f, 0.0f, -1.0f}));
                eyes[e] = {head.fov[e].angleLeft, head.fov[e].angleRight, head.fov[e].angleUp,
                           head.fov[e].angleDown};
                eyes[e].yaw = std::atan2(-f.x, -f.z);
                eyes[e].pitch = std::atan2(f.y, std::hypot(f.x, f.z));
            }
            view = Immersive::Layout::BothEyes(eyes[0], eyes[1]);
        }
        std::optional<B::Angles> screen;
        if (anchor.immersive) {
            const Input::Posef head_from_local = Input::Inverse(ToInput(head.head));
            for (int e = 0; e < 2; e++) {
                Input::Posef p = ToInput(anchor.immersive_pose[e]);
                if (!anchor.immersive_in_view) {
                    p.orientation = Input::Multiply(head_from_local.orientation, p.orientation);
                    p.position = Input::Transform(head_from_local, p.position);
                }
                const auto a =
                    B::ScreenAngles(p, anchor.immersive_size.width, anchor.immersive_size.height);
                if (!a) {
                    screen.reset();
                    break;
                }
                if (!screen) {
                    screen = a;
                } else { // the part both eyes' screens cover
                    screen->left = std::max(screen->left, a->left);
                    screen->right = std::min(screen->right, a->right);
                    screen->up = std::min(screen->up, a->up);
                    screen->down = std::max(screen->down, a->down);
                }
            }
        }
        placed = B::HeadLocked({view.left, view.right, view.up, view.down}, screen, aspect);
        in_view_space = true;
    }
    quad.swapchain = toast_chain.Handle();
    quad.image_rect = {{((level - 1) % 2) * kBadgePxW, kBadgeY + ((level - 1) / 2) * kBadgePxH},
                       {badge_box_px, kBadgePxH}};
    quad.pose = ToXrPose(placed.pose);
    quad.size = {placed.width, placed.height};
    quad.eye = XR_EYE_VISIBILITY_BOTH;
    quad.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    return true;
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
        shown_ns = ToastFit::DurationNs(ToastTimeOf(static_cast<Toast>(requested))); // ui3
        text_row_waiting = -1;
    }
    // AzaharFrame vrsize: a text toast goes into the text row not on view; it shows once copied.
    {
        std::string text;
        ToastTime text_time = ToastTime::Short;
        bool has_text = false;
        {
            std::scoped_lock lock{text_mutex};
            // AzaharFrame controls: a text toast taken back (HideToastText), if it is the one.
            if (hide_requested) {
                hide_requested = false;
                if (shown_kind >= kToastKinds && shown_text == hide_request) {
                    shown_kind = -1;
                }
                if (text_row_waiting >= 0 && waiting_text == hide_request) {
                    text_row_waiting = -1;
                }
            }
            if (text_requested) {
                text = std::move(text_request);
                text_time = text_request_time;
                text_requested = false;
                has_text = true;
            }
        }
        if (has_text && head.valid) {
            const int row = text_row_waiting >= 0      ? text_row_waiting
                            : shown_kind == kToastKinds ? kToastKinds + 1
                                                        : kToastKinds;
            row_boxes[static_cast<std::size_t>(row)] = DrawRow(atlas, row, text);
            toast_chain.Upload(reinterpret_cast<const uint8_t*>(atlas.px.data()));
            text_row_waiting = row;
            waiting_text = text;
            waiting_time = text_time;
        }
    }
    if (text_row_waiting >= 0 && !toast_chain.Busy()) {
        // Nudged again while the last one shows: it stays where it is, with the new text.
        const bool showing_text = shown_kind >= kToastKinds && time - shown_since <= shown_ns;
        if (!showing_text) {
            shown_pose = ahead();
        }
        shown_kind = text_row_waiting;
        shown_since = time;
        shown_ns = ToastFit::DurationNs(waiting_time); // AzaharFrame ui3
        shown_text = waiting_text;
        text_row_waiting = -1;
    }
    if (shown_kind >= 0 && time - shown_since > shown_ns) {
        shown_kind = -1;
    }
    const int row = shown_kind;
    if (row < 0) {
        return false;
    }
    // AzaharFrame menuux: the row's box only, at its own size.
    const ToastFit::Box box = static_cast<std::size_t>(row) < row_boxes.size()
                                  ? row_boxes[static_cast<std::size_t>(row)]
                                  : ToastFit::Box{};
    quad.swapchain = toast_chain.Handle();
    quad.image_rect = {{0, static_cast<int32_t>(row * static_cast<int>(kToastHeight))},
                       {static_cast<int32_t>(box.box_px), static_cast<int32_t>(kToastHeight)}};
    quad.pose = shown_pose;
    quad.size = {box.width_m, box.height_m};
    quad.eye = XR_EYE_VISIBILITY_BOTH;
    quad.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    return true;
}

} // namespace FrameXr
