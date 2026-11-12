// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame ui2: cheats the player adds in the Cheats segment: typed in (a name, then the code
// in Gateway / Action Replay form, on the game's keyboard in VR) or from a file (Citra's and
// Azahar's cheat file, a plain list of code lines, or a zip holding one). The text side only: what
// is a valid code, the file's cheats, which file of a zip to take. Plain C++ (no Qt or core), so
// the tests reach all of it; citra_qt/frame_cheats.cpp and frame_mods.cpp add them to the game.
//
// A code is lines of two 8-digit hex words, "XXXXXXXX YYYYYYYY". Typed text may have them in
// lower case, with more spaces, as one 16-digit word, or several pairs on one line; the code kept
// is one pair a line in upper case.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace FrameXr::Menu {

/// "" if the name can be a cheat's name, else why not (short, for under the field).
std::string CheckCheatName(const std::string& name);

/// Reads a typed code. "" and `code` set (one "XXXXXXXX YYYYYYYY" a line) if it is one, else why
/// not ("Line 2: Not a Code"), and `code` is left alone.
std::string NormalizeCheatCode(const std::string& text, std::string& code);

struct CheatText {
    std::string name;
    std::string code;     ///< as NormalizeCheatCode gives it
    std::string comments; ///< the file's "*" lines (without the "*"), one a line
    bool enabled = false; ///< "*citra_enabled"
};

struct CheatFileText {
    std::vector<CheatText> cheats; ///< the valid ones, in file order
    int skipped = 0;               ///< cheats with a line that is not a code
};

/// A cheat file's cheats: "[Name]" starts one, "*citra_enabled" turns it on, other "*" lines are
/// notes, the rest is code. Code lines before any "[Name]" make one cheat called `fallback_name`
/// (a plain list of code lines). A cheat with a line that is not a code is skipped (counted).
CheatFileText ParseCheatFile(const std::string& text, const std::string& fallback_name);

/// Of an archive's files (paths inside it), the cheat file to read: "<TITLEID>.txt" for the
/// title first, else the only .txt, else the first .txt whose name is a title ID; "" when none.
std::string PickCheatFile(const std::vector<std::string>& files, uint64_t title_id);

/// A file name (without .txt) that is a title ID ("0004000000033500").
bool IsTitleIdName(const std::string& stem);

/// "3 Cheats Added", "1 Cheat Added", "No New Cheats" (all were there already), for the toast.
std::string CheatsAddedText(int added);

} // namespace FrameXr::Menu
