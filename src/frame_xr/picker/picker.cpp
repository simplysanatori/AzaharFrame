// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version

#include "frame_xr/picker/picker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace frame_xr::picker {

namespace layout {

Rect TileRect(int slot) {
    const int col = slot % kColumns, row = slot / kColumns;
    const float x = static_cast<float>(kMargin + col * (kTileW + kGap));
    const float y = static_cast<float>(kGridTop + row * (kTileH + kGap));
    return {x, y, x + kTileW, y + kTileH};
}

Rect RescanButton() {
    return {kWidth - kMargin - 208.0f, 24.0f, kWidth - static_cast<float>(kMargin), 88.0f};
}

Rect PreviousButton() {
    return {static_cast<float>(kMargin), 896.0f, kMargin + 240.0f, 960.0f};
}

Rect NextButton() {
    return {kWidth - kMargin - 240.0f, 896.0f, kWidth - static_cast<float>(kMargin), 960.0f};
}

} // namespace layout

int PageCount(int entry_count) {
    return std::max(1, (entry_count + layout::kTilesPerPage - 1) / layout::kTilesPerPage);
}

Hit HitTest(float x, float y, int page, int entry_count) {
    using namespace layout;
    if (RescanButton().Contains(x, y)) {
        return {HitKind::Rescan, -1};
    }
    if (page > 0 && PreviousButton().Contains(x, y)) {
        return {HitKind::Previous, -1};
    }
    if (page < PageCount(entry_count) - 1 && NextButton().Contains(x, y)) {
        return {HitKind::Next, -1};
    }
    for (int slot = 0; slot < kTilesPerPage; slot++) {
        const int index = page * kTilesPerPage + slot;
        if (index >= entry_count) {
            break;
        }
        if (TileRect(slot).Contains(x, y)) {
            return {HitKind::Tile, index};
        }
    }
    return {};
}

namespace {

// AzaharFrame: the logo is scaled to the size it is drawn at (area average, in premultiplied
// alpha so transparent edges do not turn dark), so it is blitted 1:1 and stays crisp.
std::vector<uint8_t> BoxScaleRGBA(const std::vector<uint8_t>& src, int sw, int sh, int dw, int dh) {
    std::vector<uint8_t> out(static_cast<size_t>(dw) * dh * 4);
    const float fx = static_cast<float>(sw) / dw, fy = static_cast<float>(sh) / dh;
    for (int y = 0; y < dh; y++) {
        const int sy0 = std::min(sh - 1, static_cast<int>(std::floor(y * fy)));
        const int sy1 = std::clamp(static_cast<int>(std::ceil((y + 1) * fy)), sy0 + 1, sh);
        for (int x = 0; x < dw; x++) {
            const int sx0 = std::min(sw - 1, static_cast<int>(std::floor(x * fx)));
            const int sx1 = std::clamp(static_cast<int>(std::ceil((x + 1) * fx)), sx0 + 1, sw);
            float r = 0, g = 0, b = 0, a = 0;
            for (int sy = sy0; sy < sy1; sy++) {
                for (int sx = sx0; sx < sx1; sx++) {
                    const uint8_t* p = &src[(static_cast<size_t>(sy) * sw + sx) * 4];
                    const float pa = p[3] / 255.0f;
                    r += p[0] * pa;
                    g += p[1] * pa;
                    b += p[2] * pa;
                    a += pa;
                }
            }
            const float n = static_cast<float>((sy1 - sy0) * (sx1 - sx0));
            uint8_t* o = &out[(static_cast<size_t>(y) * dw + x) * 4];
            if (a > 0.0f) {
                o[0] = static_cast<uint8_t>(std::lround(std::min(255.0f, r / a)));
                o[1] = static_cast<uint8_t>(std::lround(std::min(255.0f, g / a)));
                o[2] = static_cast<uint8_t>(std::lround(std::min(255.0f, b / a)));
            } else {
                o[0] = o[1] = o[2] = 0;
            }
            o[3] = static_cast<uint8_t>(std::lround(std::min(255.0f, a / n * 255.0f)));
        }
    }
    return out;
}

// Colours (sRGB values).
const ui::Color kBackground = ui::rgb(16, 18, 26);
const ui::Color kTile = ui::rgb(34, 38, 52);
const ui::Color kTileHover = ui::rgb(56, 64, 94);
const ui::Color kAccent = ui::rgb(120, 168, 255);
const ui::Color kText = ui::rgb(240, 242, 248);
const ui::Color kTextDim = ui::rgb(150, 158, 178);
const ui::Color kButton = ui::rgb(46, 52, 74);
const ui::Color kButtonHover = ui::rgb(78, 96, 150);
const ui::Color kPlaceholder = ui::rgb(58, 64, 86);

// Stick paging: push past the high mark to flip a page, release below the low mark to
// re-arm; holding repeats after a delay.
constexpr float kScrollOn = 0.6f;
constexpr float kScrollOff = 0.3f;
constexpr float kScrollRepeatDelay = 0.5f;
constexpr float kScrollRepeatEvery = 0.35f;

std::string Plural(int n) {
    return std::to_string(n) + (n == 1 ? " Game" : " Games");
}

} // namespace

Picker::Picker(std::shared_ptr<Library> lib) : library(std::move(lib)) {
    ui::InitFonts();
    canvas.Init(layout::kWidth, layout::kHeight);
}

void Picker::SetLogo(const std::vector<uint8_t>& rgba, int width, int height) {
    logo_header = {};
    logo_large = {};
    if (width > 0 && height > 0 && rgba.size() >= static_cast<size_t>(width) * height * 4) {
        // Header: 108 px tall, between the top edge and the first row of tiles.
        // Empty screens: 200 px tall, above the message.
        const auto scaled = [&](int dh) {
            const int dw = std::max(1, static_cast<int>(std::lround(static_cast<float>(dh) * width / height)));
            return LogoImage{BoxScaleRGBA(rgba, width, height, dw, dh), dw, dh};
        };
        logo_header = scaled(108);
        logo_large = scaled(200);
    }
    needs_draw = true;
}

bool Picker::DrawLogo(const LogoImage& img, float x, float y) {
    if (img.rgba.empty()) {
        return false;
    }
    // Whole pixels, 1:1: BlitRGBA is then an exact copy (blended over the background).
    const float x0 = std::floor(x), y0 = std::floor(y);
    canvas.BlitRGBA(img.rgba.data(), img.w, img.h, x0, y0, x0 + img.w, y0 + img.h);
    return true;
}

void Picker::SetLibraryState(std::vector<GameEntry> new_entries, bool dirs, bool is_scanning) {
    entries = std::move(new_entries);
    has_dirs = dirs;
    scanning = is_scanning;
    current_page = std::min(current_page, page_count() - 1);
    hover_hit = {};
    needs_draw = true;
}

void Picker::PullLibrary() {
    if (!library) {
        return;
    }
    const uint64_t generation = library->Generation();
    if (generation == seen_generation) {
        return;
    }
    seen_generation = generation;
    const auto snapshot = library->Snapshot();
    SetLibraryState(snapshot ? *snapshot : std::vector<GameEntry>{}, library->HasDirs(),
                    library->IsScanning());
}

void Picker::SetPage(int page) {
    page = std::clamp(page, 0, page_count() - 1);
    if (page != current_page) {
        current_page = page;
        hover_hit = {};
        needs_draw = true;
    }
}

PickerResult Picker::Update(const PickerInput& in) {
    PickerResult result;
    PullLibrary();

    // Hover.
    Hit hit;
    if (in.hover) {
        hit = HitTest(in.hover->u * layout::kWidth, in.hover->v * layout::kHeight, current_page,
                      static_cast<int>(entries.size()));
    }
    if (hit != hover_hit) {
        hover_hit = hit;
        needs_draw = true;
    }

    // Stick paging.
    const float axis = in.scroll;
    if (std::fabs(axis) < kScrollOff) {
        scroll_latched = false;
        scroll_hold = 0.0f;
    } else if (std::fabs(axis) >= kScrollOn) {
        const int dir = axis > 0 ? 1 : -1;
        if (!scroll_latched) {
            scroll_latched = true;
            scroll_hold = 0.0f;
            SetPage(current_page + dir);
        } else {
            scroll_hold += in.dt;
            if (scroll_hold >= kScrollRepeatDelay) {
                scroll_hold -= kScrollRepeatEvery;
                SetPage(current_page + dir);
            }
        }
    }

    // Buttons.
    if (in.back) {
        if (current_page > 0) {
            SetPage(current_page - 1);
        } else {
            result.back_at_root = true;
        }
    }
    if (in.select) {
        // The hit of this frame (a page flip above may have changed what is under it).
        const Hit h = in.hover ? HitTest(in.hover->u * layout::kWidth, in.hover->v * layout::kHeight,
                                         current_page, static_cast<int>(entries.size()))
                               : Hit{};
        switch (h.kind) {
        case HitKind::Tile:
            result.launch_path = entries[h.index].path;
            break;
        case HitKind::Previous:
            SetPage(current_page - 1);
            break;
        case HitKind::Next:
            SetPage(current_page + 1);
            break;
        case HitKind::Rescan:
            if (library) {
                library->Rescan();
            }
            break;
        case HitKind::None:
            break;
        }
    }
    // Pick up a Rescan() that started in this call.
    PullLibrary();

    if (needs_draw) {
        Draw();
        needs_draw = false;
        result.dirty = true;
    }
    return result;
}

void Picker::DrawButton(const layout::Rect& r, const char* label, bool enabled, bool hovered) {
    const ui::Color fill = !enabled ? ui::rgb(30, 34, 46) : hovered ? kButtonHover : kButton;
    canvas.RoundRect(r.x0, r.y0, r.x1, r.y1, 16.0f, fill);
    if (enabled && hovered) {
        canvas.StrokeRoundRect(r.x0, r.y0, r.x1, r.y1, 16.0f, 4.0f, kAccent);
    }
    canvas.Text((r.x0 + r.x1) * 0.5f, r.y0 + 44.0f, ui::kFontLarge, label,
                enabled ? kText : ui::rgb(90, 98, 118), 1);
}

void Picker::DrawTile(int slot, int index, bool hovered) {
    const layout::Rect r = layout::TileRect(slot);
    const GameEntry& e = entries[index];
    canvas.RoundRect(r.x0, r.y0, r.x1, r.y1, 24.0f, hovered ? kTileHover : kTile);
    if (hovered) {
        canvas.StrokeRoundRect(r.x0, r.y0, r.x1, r.y1, 24.0f, 5.0f, kAccent);
    }

    const float cx = (r.x0 + r.x1) * 0.5f;
    const float ix0 = cx - layout::kIconDraw * 0.5f, iy0 = r.y0 + 28.0f;
    const float ix1 = ix0 + layout::kIconDraw, iy1 = iy0 + layout::kIconDraw;
    if (!e.icon_rgba.empty() && e.icon_w > 0 && e.icon_h > 0) {
        // Nearest keeps the 48 px art crisp at an integer 4x.
        canvas.BlitRGBA(e.icon_rgba.data(), e.icon_w, e.icon_h, ix0, iy0, ix1, iy1,
                        e.icon_w * 4 != layout::kIconDraw, 16.0f);
    } else {
        canvas.RoundRect(ix0, iy0, ix1, iy1, 16.0f, kPlaceholder);
        canvas.Text(cx, iy0 + layout::kIconDraw * 0.5f + 14.0f, ui::kFontLarge, "?", kTextDim, 1);
    }

    const auto lines = ui::WrapText(ui::kFontSmall, e.title, layout::kTileW - 32.0f, 3);
    float baseline = iy1 + 48.0f;
    for (const std::string& line : lines) {
        canvas.Text(cx, baseline, ui::kFontSmall, line.c_str(), kText, 1);
        baseline += 34.0f;
    }
}

void Picker::Draw() {
    using namespace layout;
    canvas.Clear(kBackground);

    // Header: the "Azahar for Frame" lockup, or its name as text when the logo is missing.
    if (!DrawLogo(logo_header, kMargin, 10.0f)) {
        canvas.Text(kMargin, 76.0f, ui::kFontLarge, "Azahar for Frame", kText);
    }
    const std::string count = scanning ? std::string("Scanning") : Plural(static_cast<int>(entries.size()));
    if (scanning || !entries.empty()) {
        canvas.Text(RescanButton().x0 - 28.0f, 66.0f, ui::kFontSmall, count.c_str(), kTextDim, 2);
    }
    DrawButton(RescanButton(), "Rescan", true, hover_hit.kind == HitKind::Rescan);

    const int total = static_cast<int>(entries.size());
    if (entries.empty()) {
        const float cx = kWidth * 0.5f;
        // The empty screens are what someone sees first: the big lockup above the message.
        DrawLogo(logo_large, cx - logo_large.w * 0.5f, 170.0f);
        if (!has_dirs) {
            canvas.Text(cx, 460.0f, ui::kFontLarge, "No Game Folders Set", kText, 1);
            canvas.Text(cx, 520.0f, ui::kFontSmall,
                        "Add one in the Azahar for Frame window on the dashboard", kTextDim, 1);
            canvas.Text(cx, 560.0f, ui::kFontSmall, "Then select Rescan", kTextDim, 1);
        } else if (scanning) {
            canvas.Text(cx, 480.0f, ui::kFontLarge, "Scanning Games", kText, 1);
        } else {
            canvas.Text(cx, 460.0f, ui::kFontLarge, "No Games Found", kText, 1);
            canvas.Text(cx, 520.0f, ui::kFontSmall,
                        "Check the folders in the Azahar for Frame window, then select Rescan", kTextDim, 1);
        }
    } else {
        for (int slot = 0; slot < kTilesPerPage; slot++) {
            const int index = current_page * kTilesPerPage + slot;
            if (index >= total) {
                break;
            }
            DrawTile(slot, index, hover_hit.kind == HitKind::Tile && hover_hit.index == index);
        }
    }

    // Footer: paging, then where the settings are.
    const int pages = page_count();
    if (pages > 1) {
        DrawButton(PreviousButton(), "Previous", current_page > 0,
                   hover_hit.kind == HitKind::Previous);
        DrawButton(NextButton(), "Next", current_page < pages - 1, hover_hit.kind == HitKind::Next);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Page %d of %d", current_page + 1, pages);
        canvas.Text(kWidth * 0.5f, 938.0f, ui::kFontSmall, buf, kTextDim, 1);
    }
    canvas.Text(kWidth * 0.5f, 1000.0f, ui::kFontSmall,
                "Settings are in the Azahar for Frame window on the dashboard", kTextDim, 1);
}

} // namespace frame_xr::picker
