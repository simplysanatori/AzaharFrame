// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame crashreport: the crash report's "What Were You Doing?" box on the VR keyboard
// (swkbd_model.h): multi-line, up to 500 characters, Cancel, Clear and Done. The Qt side
// (citra_qt/frame_crash_dialog.cpp) opens it when the box is tapped in VR; the dumps and the
// tests use the same spec.

#pragma once

#include <string>

#include "frame_xr/menu/swkbd_model.h"

namespace FrameXr::Menu {

inline constexpr int kCrashNoteMax = 500;
inline constexpr const char* kCrashNoteLabel = "What Were You Doing?";

inline Swkbd::Spec CrashNoteSpec(const std::string& initial) {
    Swkbd::Spec s;
    s.hint = kCrashNoteLabel;
    s.max_length = kCrashNoteMax;
    s.multiline = true;
    s.initial = initial;
    Swkbd::Button cancel{"Cancel", 0, false};
    Swkbd::Button clear{"Clear", 1, false};
    clear.clears = true;
    Swkbd::Button done{"Done", 2, true};
    s.buttons = {cancel, clear, done};
    return s;
}

} // namespace FrameXr::Menu
