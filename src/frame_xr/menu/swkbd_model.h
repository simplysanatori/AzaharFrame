// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: the game's keyboard in VR (the 3DS software keyboard applet, SWKBD). In flat
// Azahar shows its Qt dialog (citra_qt/applets/swkbd.cpp); in VR that window is not on view, so
// the game would wait for it forever. This is the keyboard's content: the text, the caret, Shift,
// the pages and the game's buttons, and where every key is on the panel (dp, the in-game menu's
// 840 x 560 panel). Plain C++ (no Qt, OpenXR or emulator), so the tests reach all of it; the
// drawing is swkbd_view.h, the XR side frame_xr/swkbd_panel.h, the applet's rules
// frame_xr/swkbd_config.h.

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace FrameXr::Swkbd {

/// The panel, in dp: the in-game menu's (Menu::kPanelW x kPanelH), so both use one swapchain.
constexpr float kPanelW = 840.0f;
constexpr float kPanelH = 560.0f;
constexpr float kPanelRadius = 32.0f;

/// One of the game's buttons (1 to 3, left to right as on the 3DS).
struct Button {
    std::string label;
    uint8_t id = 0;       ///< what the game gets back (Frontend::KeyboardData::button)
    bool confirm = false; ///< the OK button: the text is checked first
};

/// What the game asked for.
struct Spec {
    std::string hint;   ///< the game's hint text (UTF-8), the field's label
    int max_length = 0; ///< UTF-16 units, as the 3DS counts
    bool multiline = false;
    bool numpad = false; ///< digits only (KeyboardType::NumPad)
    std::vector<Button> buttons;
    /// While typing: "" if the text may be typed, else why not (a key that breaks the game's
    /// filters is refused). Empty function: anything.
    std::function<std::string(const std::string& utf8)> filter;
    /// On the confirm button: "" if the game takes the text, else why not.
    std::function<std::string(const std::string& utf8)> validate;
    /// Shown under the field at the start (the game's own message when it refused the last text).
    std::string error;
};

/// What closes the keyboard: the text and the button.
struct Result {
    std::string text; ///< UTF-8
    uint8_t button = 0;
};

enum class Page : uint8_t { Letters, Symbols, Digits };
enum class Shift : uint8_t { Off, Once, Locked };

enum class KeyKind : uint8_t {
    Char, ///< `ch`
    Shift,
    Backspace,
    Space,
    Enter,      ///< a new line (multi-line text only)
    PageSwitch, ///< "?123" or "ABC"
    Button,     ///< the game's button `button` (an index into Spec::buttons)
};

struct Rect {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    float W() const {
        return x1 - x0;
    }
    float H() const {
        return y1 - y0;
    }
    float Cx() const {
        return (x0 + x1) * 0.5f;
    }
    float Cy() const {
        return (y0 + y1) * 0.5f;
    }
    bool Contains(float x, float y) const {
        return x >= x0 && x < x1 && y >= y0 && y < y1;
    }
};

struct Key {
    KeyKind kind = KeyKind::Char;
    char32_t ch = 0; ///< Char: what it types (Shift applied)
    int button = -1; ///< Button: the index into Spec::buttons
    Rect r;
    std::string label;
};

/// Where everything is (dp from the panel's top left).
struct Layout {
    Rect field;            ///< the text field
    Rect supporting;       ///< the line under it (an error, the count)
    std::vector<Key> keys; ///< the keys, then the game's buttons
};

// Geometry (dp).
constexpr float kPad = 24.0f;
constexpr float kFieldH = 72.0f;       ///< one line
constexpr float kFieldMultiH = 112.0f; ///< multi-line
constexpr float kSupportingH = 28.0f;
constexpr float kKeyGap = 8.0f;
constexpr float kButtonH = 56.0f;
constexpr float kButtonW = 200.0f;
constexpr float kButtonGap = 12.0f;

/// The keyboard's state and what each press does.
class Model {
public:
    explicit Model(Spec spec);

    const Spec& GetSpec() const {
        return spec;
    }
    const std::u32string& Text() const {
        return text;
    }
    std::string TextUtf8() const;
    int Caret() const {
        return caret;
    }
    Page GetPage() const {
        return page;
    }
    Shift GetShift() const {
        return shift;
    }
    const std::string& Error() const {
        return error;
    }
    /// The text's length as the game counts it (UTF-16 units).
    int Length() const;
    /// "3 / 10" (the character counter under the field).
    std::string Counter() const;
    /// Bumped on every change (the panel redraws when it moves).
    uint64_t Revision() const {
        return revision;
    }
    const Layout& GetLayout() const {
        return layout;
    }

    /// The key at (x, y) dp, or -1.
    int KeyAt(float x, float y) const;
    /// A press on key `index` of the layout: a game's button may close the keyboard.
    std::optional<Result> Press(int index);

    // Edits (also from a physical keyboard). False: nothing changed (too long, a filter).
    bool Insert(char32_t c);
    bool InsertText(const std::string& utf8);
    bool Backspace();
    bool DeleteForward();
    void MoveCaret(int delta);
    void CaretHome();
    void CaretEnd();
    void ToggleShift();
    void SetPage(Page p);
    /// The game's button `index` (into Spec::buttons): the confirm button checks the text first
    /// (and shows why it was refused); the others close at once.
    std::optional<Result> PressButton(int index);
    /// Enter on a physical keyboard: a new line in multi-line text, else the confirm button.
    std::optional<Result> Enter();
    /// The button that cancels (the game's id 0 when it has two or three buttons), for closing
    /// the keyboard from outside (the game stops). Otherwise the confirm button's id.
    uint8_t CancelId() const;

private:
    void Changed();
    void Relayout();
    bool Accept(const std::u32string& next);

    Spec spec;
    std::u32string text;
    int caret = 0;
    Page page = Page::Letters;
    Shift shift = Shift::Once;
    std::string error;
    uint64_t revision = 1;
    Layout layout;
};

/// The layout for a page (exposed for the tests and the mockups).
Layout MakeLayout(const Spec& spec, Page page, Shift shift);

/// UTF-8 helpers (no dependencies).
std::u32string FromUtf8(const std::string& s);
std::string ToUtf8(const std::u32string& s);
/// UTF-16 units of a string.
int Utf16Length(const std::u32string& s);

} // namespace FrameXr::Swkbd
