// Copyright 2026 CitraFrame contributors
// Licensed under GPLv3 or any later version

// State, layout and drawing of the in-VR application picker: a grid of icon tiles,
// painted on the CPU into one RGBA8 texture that is shown on a flat quad (1.6 m wide,
// 2 m ahead). No XR, Qt or Vulkan dependencies: the caller feeds it a ray hit in quad
// UV coordinates plus button edges, and uploads Pixels() when Update() says it changed.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "frame_xr/picker/library.h"
#include "frame_xr/ui/ui_canvas.h"

namespace frame_xr::picker {

namespace layout {

// The texture. Aspect 3:2, so on the 1.6 m wide quad it is 1.0667 m tall and one
// texel is about 1.04 mm: the small font (28 px) is about 2.9 cm tall at 2 m.
inline constexpr int kWidth = 1536;
inline constexpr int kHeight = 1024;
inline constexpr int kMargin = 48;
inline constexpr int kColumns = 4;
inline constexpr int kRows = 2;
inline constexpr int kTilesPerPage = kColumns * kRows;
inline constexpr int kGap = 24;
inline constexpr int kGridTop = 128;
inline constexpr int kTileW = (kWidth - 2 * kMargin - (kColumns - 1) * kGap) / kColumns; // 342
inline constexpr int kTileH = 360;
inline constexpr int kIconDraw = 192; // the 48 px icon, 4x

struct Rect {
    float x0, y0, x1, y1;
    bool Contains(float x, float y) const {
        return x >= x0 && x < x1 && y >= y0 && y < y1;
    }
};

/// Tile `slot` (0..kTilesPerPage-1) of a page, row by row.
Rect TileRect(int slot);
Rect RescanButton();
Rect PreviousButton();
Rect NextButton();

} // namespace layout

enum class HitKind { None, Tile, Rescan, Previous, Next };

struct Hit {
    HitKind kind = HitKind::None;
    int index = -1; // Tile: index into the entry list
    bool operator==(const Hit& o) const {
        return kind == o.kind && index == o.index;
    }
    bool operator!=(const Hit& o) const {
        return !(*this == o);
    }
};

/// Number of pages for `entry_count` entries; always at least 1.
int PageCount(int entry_count);

/// What is under the point (`x`, `y`) in texture pixels on page `page`. Tile slots
/// without an entry and disabled paging buttons hit nothing.
Hit HitTest(float x, float y, int page, int entry_count);

struct Vec2 {
    float u = 0, v = 0;
};

struct PickerInput {
    /// Where the pointer ray meets the picker quad: u to the right and v downwards,
    /// both 0..1 across the whole quad ((0,0) = top-left). nullopt = not pointing at it.
    std::optional<Vec2> hover;
    /// Trigger / A went down this frame (an edge, not a level).
    bool select = false;
    /// B went down this frame.
    bool back = false;
    /// Stick Y, -1..1 with the caller's dead zone applied; positive = next page.
    float scroll = 0.0f;
    /// Seconds since the previous Update().
    float dt = 0.0f;
};

struct PickerResult {
    /// Set when an application tile was selected.
    std::optional<std::string> launch_path;
    /// Pixels() changed in this call; upload it.
    bool dirty = false;
    /// Back was pressed on the first page: nothing left to go back from.
    bool back_at_root = false;
};

class Picker {
public:
    /// `library` may be null (tests); then use SetLibraryState().
    explicit Picker(std::shared_ptr<Library> library = nullptr);

    /// Per-frame update; redraws only when something visible changed.
    PickerResult Update(const PickerInput& input);

    /// Feeds the state the picker would otherwise take from its Library.
    void SetLibraryState(std::vector<GameEntry> entries, bool has_dirs, bool scanning);

    /// Forces a redraw and a dirty flag on the next Update (after the picker is shown
    /// again, or after the quad swapchain was recreated).
    void Invalidate() {
        needs_draw = true;
    }

    /// RGBA8 (R first in memory), rows top down, premultiplied (fully opaque).
    const uint8_t* Pixels() const {
        return reinterpret_cast<const uint8_t*>(canvas.px.data());
    }
    size_t PixelBytes() const {
        return canvas.px.size() * sizeof(uint32_t);
    }
    int Width() const {
        return layout::kWidth;
    }
    int Height() const {
        return layout::kHeight;
    }

    int page() const {
        return current_page;
    }
    int page_count() const {
        return PageCount(static_cast<int>(entries.size()));
    }
    const std::vector<GameEntry>& Entries() const {
        return entries;
    }
    Hit hovered() const {
        return hover_hit;
    }

private:
    void PullLibrary();
    void Draw();
    void DrawTile(int slot, int index, bool hovered);
    void DrawButton(const layout::Rect& r, const char* label, bool enabled, bool hovered);
    void SetPage(int page);

    std::shared_ptr<Library> library;
    uint64_t seen_generation = ~0ull;

    std::vector<GameEntry> entries;
    bool has_dirs = false;
    bool scanning = false;

    int current_page = 0;
    Hit hover_hit;
    bool needs_draw = true;

    // Stick paging: one page per push, repeating while held.
    bool scroll_latched = false;
    float scroll_hold = 0.0f;

    ui::Canvas canvas;
};

} // namespace frame_xr::picker
