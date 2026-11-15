// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame fovaxis: Render Margin per axis, "Render Margin: Horizontal" and "Render Margin:
// Vertical". Three keys hold it (common/settings.h):
//   - vr_immersive_wider_view_level, Render Margin as every build before reads it: both axes.
//     This build never writes it from the rows; the grip gesture steps it while both axes
//     follow it, exactly as before.
//   - vr_immersive_render_margin_x, _y: an axis's own level, or -1 (the default) to follow the
//     key above.
// Each is global and per game. An axis's level is decided scope first: the game's own value of
// that axis, else the game's own Render Margin, else the global value of that axis, else the
// global Render Margin. So a game whose Render Margin an older build set keeps it on both axes,
// and an older build reading the file sees the key it knows, unchanged.
// The VR menu, Azahar's window and the grip gesture all go through here.

#pragma once

#include <cstdint>

namespace FrameXr::Immersive::Margin {

enum class Axis { Horizontal, Vertical };

/// The keys' values in one scope order (what the pure rule needs); `*_game_set` when the game
/// has a value of its own for that key (the setting not on the global value).
struct Keys {
    uint32_t old_global = 4;
    bool old_game_set = false;
    uint32_t old_game = 4;
    int32_t axis_global = -1;
    bool axis_game_set = false;
    int32_t axis_game = -1;
};
/// The rule above (levels clamped to 0..4).
uint32_t Resolve(const Keys& k);
/// The same with no game values: what the global scope shows.
uint32_t ResolveGlobal(const Keys& k);
/// A game value decides it (the game's own axis value, or its own Render Margin).
bool GameDecides(const Keys& k);

/// The keys of `axis` from Settings::values.
Keys Read(Axis axis);
/// The level in effect on `axis` (the running game's, or the global one).
uint32_t Level(Axis axis);
/// The level the global scope has on `axis`.
uint32_t GlobalLevel(Axis axis);
/// The running game has a value of its own for `axis` (the menu's Per-Game count, the global
/// row locked).
bool GameHasOwn(Axis axis);
/// Sets `axis` to `level`: the game's own value when `per_game`, else the global one (left as
/// it is when the game decides it: the menu locks that row).
void Set(Axis axis, uint32_t level, bool per_game);
/// Use Global Value for `axis`: its own game value goes; when the game's own Render Margin
/// decided it, that goes too, and the other axis, if it followed it, keeps that level as its
/// own. True when the game had a value of its own.
bool UseGlobal(Axis axis);

/// Grip + D-pad up or down: both axes one level up (`dir` > 0) or down, for the game on view.
/// While both follow Render Margin it steps that key (as every build before); otherwise each
/// axis steps its own level. `changed`: something moved (false at the ends).
struct Stepped {
    uint32_t x_before = 0, y_before = 0;
    uint32_t x = 0, y = 0;
    bool changed = false;
};
Stepped StepBoth(int dir);

} // namespace FrameXr::Immersive::Margin
