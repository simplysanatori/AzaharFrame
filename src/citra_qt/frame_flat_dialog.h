// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flatdialog: the windows Azahar opens in Flat (dialogs, message boxes, file
// dialogs, Configure, a game's Properties) sized for SteamVR's theater screen. gamescope shows
// the newest window as the whole picture, scaled to fit the 16:9 screen (frame_xr/flat_fit.h has
// the rule); FrameMirror calls this on every such window's Show event in Flat, before it maps,
// and once more right after (a message box sets its size in its own showEvent). VR does not use
// it: there the mirror fits dialogs to its panel (FrameMirror::FitDialog).

#pragma once

class QDialog;
class QWidget;

namespace FrameFlatDialog {

/// Resizes `window` to the Flat size for `main` and centres it over `main`. `first` (the Show
/// event): a dialog whose minimum size is larger than the area gets its content in a scroll
/// area first, OK and Cancel staying under it; the size is logged. GUI thread.
void Fit(QWidget* window, const QWidget* main, bool first);

/// The dialog's content (its layout but the last QDialogButtonBox) into a scroll area. False,
/// changing nothing, for Qt's own dialogs, a fixed-size layout, no layout, or when done already.
bool AddScrollArea(QDialog* dialog);

} // namespace FrameFlatDialog
