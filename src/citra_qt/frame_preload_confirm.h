// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame memguard: "Preload Custom Textures", asked before the setting is turned on (the VR
// menu's Textures segment, citra_qt/frame_mods.cpp; Flat's Enhancements checkbox,
// configuration/configure_enhancements.cpp). The numbers are the preload's own, worked out now
// (video_core/custom_textures/preload_budget.h): the loaded pack's estimate, the memory in use,
// Azahar's limit (the launcher's scope), and whether the pack fits.

#pragma once

#include "video_core/custom_textures/preload_budget.h"

class QDialog;
class QWidget;

/// The question's text with the numbers of this moment. GUI thread (the running game's pack is
/// read from the emulator, which only the GUI thread stops).
VideoCore::PreloadBudget::ConfirmText FramePreloadConfirmText();

/// The Qt dialog for `text` (not shown; the caller owns it). Accepted is Turn On.
QDialog* FrameMakePreloadDialog(QWidget* parent, const VideoCore::PreloadBudget::ConfirmText& text);

/// Asks it in a Qt dialog (Flat, and Azahar's window in VR). True for Turn On.
bool FrameAskPreload(QWidget* parent);
