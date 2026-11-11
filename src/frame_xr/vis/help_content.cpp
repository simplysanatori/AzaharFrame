// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/param_package.h"
#include "frame_xr/input/controls.h"
#include "frame_xr/input/default_bindings.h"
#include "frame_xr/vis/help_content.h"

namespace FrameXr::Vis {

namespace {

using Input::AnalogControl;
using Input::Control;
namespace NB = Settings::NativeButton;
namespace NA = Settings::NativeAnalog;

const char* ButtonName(int button) {
    switch (button) {
    case NB::A:
        return "A";
    case NB::B:
        return "B";
    case NB::X:
        return "X";
    case NB::Y:
        return "Y";
    case NB::Up:
        return "D-Pad Up";
    case NB::Down:
        return "D-Pad Down";
    case NB::Left:
        return "D-Pad Left";
    case NB::Right:
        return "D-Pad Right";
    case NB::L:
        return "L";
    case NB::R:
        return "R";
    case NB::Start:
        return "Start";
    case NB::Select:
        return "Select";
    case NB::Debug:
        return "Debug";
    case NB::Gpio14:
        return "GPIO14";
    case NB::ZL:
        return "ZL";
    case NB::ZR:
        return "ZR";
    case NB::Home:
        return "Home";
    case NB::Power:
        return "Power";
    default:
        return "?";
    }
}

// The 3DS buttons a frame_xr control drives in the profile, in the 3DS's order.
std::vector<int> BoundTo(const HelpBindings& b, Control control) {
    std::vector<int> out;
    for (int i = 0; i < NB::NumButtons; i++) {
        const Common::ParamPackage p(b.buttons[static_cast<std::size_t>(i)]);
        if (p.Get("engine", "") == "frame_xr" && p.Get("button", -1) == static_cast<int>(control)) {
            out.push_back(i);
        }
    }
    return out;
}

std::string Names(const std::vector<int>& buttons) {
    std::string s;
    for (const int b : buttons) {
        if (!s.empty()) {
            s += " + ";
        }
        s += ButtonName(b);
    }
    return s;
}

// "3DS B", "3DS L + R", or empty for none.
std::string ThreeDs(const HelpBindings& b, Control control) {
    const std::vector<int> bound = BoundTo(b, control);
    return bound.empty() ? std::string{} : "3DS " + Names(bound);
}

// The 3DS stick a frame_xr stick drives ("Circle Pad", "C-Stick"), or empty.
std::string StickTo(const HelpBindings& b, AnalogControl control) {
    std::string s;
    for (int i = 0; i < NA::NumAnalogs; i++) {
        const Common::ParamPackage p(b.analogs[static_cast<std::size_t>(i)]);
        if (p.Get("engine", "") == "frame_xr" && p.Get("analog", -1) == static_cast<int>(control)) {
            if (!s.empty()) {
                s += " + ";
            }
            s += i == NA::CirclePad ? "Circle Pad" : "C-Stick";
        }
    }
    return s;
}

// A fixed action, and the 3DS buttons the profile also put on the control.
std::string With(const std::string& fixed, const std::string& bound) {
    if (bound.empty()) {
        return fixed;
    }
    return bound + ", " + fixed;
}

std::string OrNothing(const std::string& s) {
    return s.empty() ? std::string{"Nothing"} : s;
}

void Row(std::vector<HelpRow>& rows, std::string control, std::string action) {
    rows.push_back({HelpRow::Kind::Row, std::move(control), std::move(action), false});
}

void Heading(std::vector<HelpRow>& rows, std::string text, bool now) {
    rows.push_back({HelpRow::Kind::Heading, std::move(text), {}, now});
}

// The Frame's face buttons, and the pad control each drives (mapper.cpp: by position, the
// Frame's A is the pad's B, and so on).
struct Face {
    const char* frame;
    Control pad;
};
std::array<Face, 4> Faces(bool by_position) {
    return {{
        {"A", by_position ? Control::B : Control::A},
        {"B", by_position ? Control::A : Control::B},
        {"X", by_position ? Control::Y : Control::X},
        {"Y", by_position ? Control::X : Control::Y},
    }};
}

// The D-pad drives the 3DS's D-pad, one direction each.
bool DpadPlain(const HelpBindings& b) {
    return BoundTo(b, Control::DpadUp) == std::vector<int>{NB::Up} &&
           BoundTo(b, Control::DpadDown) == std::vector<int>{NB::Down} &&
           BoundTo(b, Control::DpadLeft) == std::vector<int>{NB::Left} &&
           BoundTo(b, Control::DpadRight) == std::vector<int>{NB::Right};
}

// The 3DS buttons on the grips ("3DS L + R"), or empty.
std::string Grips(const HelpBindings& b) {
    std::string grips = Names(BoundTo(b, Control::GripL));
    if (const std::string r = Names(BoundTo(b, Control::GripR)); !r.empty()) {
        grips += (grips.empty() ? "" : " + ") + r;
    }
    return grips.empty() ? grips : "3DS " + grips;
}

std::string Footer(const HelpBindings& given, bool by_position) {
    std::string footer = std::string{"Face Buttons: "} +
                         (by_position ? "By Position (Nintendo)" : "By Label (Xbox)");
    if (!given.valid) {
        footer += "  \xC2\xB7  Default Controls";
    } else if (!given.profile_name.empty()) {
        footer += "  \xC2\xB7  Profile: " + given.profile_name;
    }
    return footer;
}

// ---- The diagram's labels ----

constexpr int kLeft = 0;
constexpr int kRight = 1;
constexpr int kBoth = 2;

DiagramLine Title(std::string key, std::string action) {
    return {std::move(key), std::move(action), true, false, false};
}
DiagramLine Line(std::string key, std::string action) {
    return {std::move(key), std::move(action), false, false, false};
}
DiagramLine Sub(std::string key, std::string action) {
    return {std::move(key), std::move(action), false, false, true};
}
DiagramLine SubHeading(std::string text) {
    return {std::move(text), {}, false, true, false};
}

void Label(DiagramPage& page, int hand, std::vector<CtrlPart> parts, std::vector<DiagramLine> lines) {
    page.labels.push_back({hand, std::move(parts), std::move(lines)});
}

// A trigger: the 3DS button it is, and the touch on the bottom screen, which it always is.
std::vector<DiagramLine> TriggerLines(const char* name, const std::string& bound) {
    if (bound.empty()) {
        return {Title(name, "Touch on Bottom Screen")};
    }
    return {Title(name, bound), Line("", "Touch on Bottom Screen")};
}

} // namespace

HelpBindings DefaultHelpBindings() {
    HelpBindings b;
    for (const auto& binding : Input::kDefaultButtonBindings) {
        b.buttons[binding.button] =
            "engine:frame_xr,button:" + std::to_string(static_cast<int>(binding.control));
    }
    for (const auto& binding : Input::kDefaultAnalogBindings) {
        b.analogs[binding.analog] =
            "engine:frame_xr,analog:" + std::to_string(static_cast<int>(binding.control));
    }
    b.valid = true;
    return b;
}

HelpPage BuildHelp(const HelpBindings& given, const HelpContext& ctx) {
    const HelpBindings b = given.valid ? given : DefaultHelpBindings();
    HelpPage page;

    // ---- In Game ----
    auto& g = page.game;
    Heading(g, "In Game", ctx.game_running && !ctx.in_menu);
    const bool pos = ctx.face_by_position;
    for (const Face& f : Faces(pos)) {
        Row(g, f.frame, OrNothing(ThreeDs(b, f.pad)));
    }
    // The D-pad as one row when it is the 3DS's D-pad, else one row per direction.
    if (DpadPlain(b)) {
        Row(g, "D-Pad", "3DS D-Pad");
    } else {
        Row(g, "D-Pad Up", OrNothing(ThreeDs(b, Control::DpadUp)));
        Row(g, "D-Pad Down", OrNothing(ThreeDs(b, Control::DpadDown)));
        Row(g, "D-Pad Left", OrNothing(ThreeDs(b, Control::DpadLeft)));
        Row(g, "D-Pad Right", OrNothing(ThreeDs(b, Control::DpadRight)));
    }
    Row(g, "Left Stick", OrNothing(StickTo(b, AnalogControl::StickL)));
    Row(g, "Right Stick", OrNothing(StickTo(b, AnalogControl::StickR)));
    Row(g, "Left Bumper", OrNothing(ThreeDs(b, Control::BumperL)));
    Row(g, "Right Bumper", OrNothing(ThreeDs(b, Control::BumperR)));
    // The triggers touch when aimed at the shown bottom screen, whatever the profile says.
    Row(g, "Left Trigger", With("Touch on Bottom Screen", ThreeDs(b, Control::TriggerL)));
    Row(g, "Right Trigger", With("Touch on Bottom Screen", ThreeDs(b, Control::TriggerR)));
    Row(g, "Menu", OrNothing(ThreeDs(b, Control::Menu)));
    Row(g, "View", OrNothing(ThreeDs(b, Control::View)));
    // AzaharFrame screenswap: the stick clicks are taps (on release) and 1 s holds (mapper.cpp).
    Row(g, "Left Stick Click", "Hide or Show Secondary Screen");
    Row(g, "Hold Left Stick Click (1 s)", "Screen Layout"); // AzaharFrame layout
    Row(g, "Right Stick Click", With("Swap Screens", ThreeDs(b, Control::StickClickR)));
    Row(g, "Hold Right Stick Click (1 s)", "Reset Screens and View");
    Row(g, "Grip on a Screen", With("Move Screens", Grips(b)));
    Row(g, "Grip (Immersive)", "Move Immersive Screen");
    Row(g, "Hold Grip + Stick Up or Down", "Nearer or Farther");
    Row(g, "Hold Grip + Stick Sideways", "Screen Size");
    Row(g, "Hold Grip + D-Pad Left or Right", "Nudge Screen"); // AzaharFrame vrsize
    Row(g, "Hold Grip + D-Pad Up or Down", "Screen Curve or Wider View"); // curve
    Row(g, "Hold Menu (1 s)", "In-Game Menu");
    Row(g, "Hold View (1.5 s)", "Stop Game");
    Row(g, "Hold View + Left Bumper (0.5 s)", "Quick Load");
    Row(g, "Hold View + Right Bumper", "Quick Save");
    Row(g, "Hold View + Right Trigger", "Fast Forward On / Off");
    Row(g, "Both Bumpers + View", "Show or Hide Bottom Screen");

    // ---- Azahar's window (the home panel), the in-game menu, everywhere ----
    auto& w = page.window;
    Heading(w, "Azahar's Window", !ctx.game_running || ctx.in_menu);
    Row(w, "Trigger or A", "Click");
    Row(w, "D-Pad or Left Stick Up / Down", "Choose Game"); // AzaharFrame gamelist
    Row(w, "A Twice", "Start Game");
    Row(w, "Right Stick Click", "Right Click");
    Row(w, "Hold Right Stick Click (1 s)", "Bring Window Back");
    Row(w, "B", "Back");
    Row(w, "Grip", "Move Window");
    Row(w, "Right Stick Up or Down", "Scroll");
    Row(w, "Y", "Keyboard");
    Heading(w, "In-Game Menu", ctx.in_menu);
    Row(w, "Hold Menu (1 s)", "Back to Game");
    Row(w, "B (Nothing Open)", "Back to Game");
    Row(w, "Hold View (1.5 s)", "Stop Game");
    Heading(w, "Everywhere", false);
    Row(w, "Both Bumpers + View + Menu", "Switch VR / Flat");

    page.footer = Footer(given, pos);
    return page;
}

HelpDiagram BuildHelpDiagram(const HelpBindings& given, const HelpContext& ctx) {
    const HelpBindings b = given.valid ? given : DefaultHelpBindings();
    const bool pos = ctx.face_by_position;
    HelpDiagram d;
    d.now = HelpDiagramPageFor(ctx);
    d.footer = Footer(given, pos);

    // ---- In Game ----
    DiagramPage& g = d.pages[static_cast<std::size_t>(DiagramPageId::Game)];
    g.name = "In Game";
    Label(g, kLeft, {CtrlPart::Bumper}, {Title("Left Bumper", OrNothing(ThreeDs(b, Control::BumperL)))});
    Label(g, kLeft, {CtrlPart::Trigger}, TriggerLines("Left Trigger", ThreeDs(b, Control::TriggerL)));
    {
        // The 3DS's D-pad, and a line for each direction the profile changed (all four when
        // none is left).
        const struct {
            const char* name;
            Control control;
            int button;
        } dirs[] = {{"Up", Control::DpadUp, NB::Up},
                    {"Down", Control::DpadDown, NB::Down},
                    {"Left", Control::DpadLeft, NB::Left},
                    {"Right", Control::DpadRight, NB::Right}};
        std::vector<DiagramLine> lines;
        bool any_plain = false;
        for (const auto& d : dirs) {
            if (BoundTo(b, d.control) == std::vector<int>{d.button}) {
                any_plain = true;
            } else {
                lines.push_back(Sub(d.name, OrNothing(ThreeDs(b, d.control))));
            }
        }
        lines.insert(lines.begin(), any_plain ? Title("D-Pad", "3DS D-Pad") : SubHeading("D-Pad"));
        Label(g, kLeft, {CtrlPart::Dpad}, std::move(lines));
    }
    Label(g, kLeft, {CtrlPart::View},
          {Title("View", OrNothing(ThreeDs(b, Control::View))), Line("Hold (1.5 s)", "Stop Game"),
           SubHeading("Hold View +"), Sub("Hold Left Bumper (0.5 s)", "Quick Load"),
           Sub("Right Bumper", "Quick Save"), Sub("Right Trigger", "Fast Forward On / Off"),
           SubHeading("Both Bumpers +"), Sub("View", "Show or Hide Bottom Screen"),
           Sub("View + Menu", "Switch VR / Flat")});
    // AzaharFrame screenswap: the stick clicks are taps (on release) and 1 s holds (mapper.cpp).
    Label(g, kLeft, {CtrlPart::Stick},
          {Title("Left Stick", OrNothing(StickTo(b, AnalogControl::StickL))),
           Line("Click", "Hide or Show Secondary Screen"),
           Line("Hold Click (1 s)", "Screen Layout")}); // AzaharFrame layout
    std::vector<DiagramLine> faces;
    for (const Face& f : Faces(pos)) {
        faces.push_back(Title(f.frame, OrNothing(ThreeDs(b, f.pad))));
    }
    Label(g, kRight, {CtrlPart::A, CtrlPart::B, CtrlPart::X, CtrlPart::Y}, std::move(faces));
    Label(g, kRight, {CtrlPart::Bumper}, {Title("Right Bumper", OrNothing(ThreeDs(b, Control::BumperR)))});
    Label(g, kRight, {CtrlPart::Trigger}, TriggerLines("Right Trigger", ThreeDs(b, Control::TriggerR)));
    Label(g, kRight, {CtrlPart::Menu},
          {Title("Menu", OrNothing(ThreeDs(b, Control::Menu))), Line("Hold (1 s)", "In-Game Menu")});
    Label(g, kRight, {CtrlPart::Stick},
          {Title("Right Stick", OrNothing(StickTo(b, AnalogControl::StickR))),
           Line("Click", With("Swap Screens", ThreeDs(b, Control::StickClickR))),
           Line("Hold Click (1 s)", "Reset Screens and View")});
    Label(g, kBoth, {CtrlPart::Grip},
          {Title("Grip on a Screen", With("Move Screens", Grips(b))), SubHeading("Hold Grip +"),
           Sub("Stick Up or Down", "Nearer or Farther"), Sub("Stick Sideways", "Screen Size"),
           Sub("D-Pad Left or Right", "Nudge"), // AzaharFrame vrsize
           Sub("D-Pad Up or Down", "Curve or Wider View")}); // AzaharFrame curve

    // ---- Azahar's window (the home panel), the in-game menu, everywhere ----
    DiagramPage& w = d.pages[static_cast<std::size_t>(DiagramPageId::Window)];
    w.name = "Azahar's Window";
    Label(w, kLeft, {CtrlPart::Trigger}, {Title("Left Trigger", "Click")});
    Label(w, kLeft, {CtrlPart::View},
          {SubHeading("In-Game Menu"), Sub("Hold View (1.5 s)", "Stop Game"),
           SubHeading("Both Bumpers +"), Sub("View + Menu", "Switch VR / Flat")});
    // AzaharFrame gamelist: the game list (the left stick scrolls in dialogs and menus).
    Label(w, kLeft, {CtrlPart::Stick}, {Title("Left Stick", "Choose Game")});
    Label(w, kLeft, {CtrlPart::Dpad}, {Title("D-Pad Up or Down", "Choose Game")});
    Label(w, kRight, {CtrlPart::A, CtrlPart::B, CtrlPart::X, CtrlPart::Y},
          {Title("A", "Click"), Line("Twice", "Start Game"), Title("B", "Back"),
           Title("Y", "Keyboard")});
    Label(w, kRight, {CtrlPart::Trigger}, {Title("Right Trigger", "Click")});
    Label(w, kRight, {CtrlPart::Menu},
          {SubHeading("In-Game Menu"), Sub("Hold Menu (1 s)", "Back to Game"),
           Sub("B (Nothing Open)", "Back to Game")});
    Label(w, kRight, {CtrlPart::Stick},
          {Title("Right Stick", "Scroll"), Line("Click", "Right-Click"),
           Line("Hold Click (1 s)", "Bring Window Back")});
    Label(w, kBoth, {CtrlPart::Grip}, {Title("Grip", "Move Window")});
    return d;
}

// AzaharFrame remap: Edit Controls. Every control is a chip; the 3DS buttons on it show as text and
// a glyph (the first one). The ones that are gestures are disabled chips.
DiagramPage BuildEditPage(const HelpBindings& given, bool by_position) {
    const HelpBindings b = given.valid ? given : DefaultHelpBindings();
    DiagramPage page;
    page.name = "Edit Controls";
    const auto chip = [&](std::string key, Control control) {
        const std::vector<int> bound = BoundTo(b, control);
        std::string action = Names(bound);
        if (action.empty()) {
            action = "Nothing";
        }
        DiagramLine l{std::move(key), std::move(action), true, false, false};
        l.chip = true;
        l.control = static_cast<int>(control);
        l.glyph = bound.empty() ? -1 : bound.front();
        if (l.key == l.action) {
            l.action.clear(); // "D-Pad Up", not "D-Pad Up: D-Pad Up"
        }
        return l;
    };
    const auto locked = [](std::string key, std::string action = {}) {
        DiagramLine l{std::move(key), std::move(action), true, false, false};
        l.chip = true;
        return l;
    };
    Label(page, kLeft, {CtrlPart::Bumper}, {chip("Left Bumper", Control::BumperL)});
    Label(page, kLeft, {CtrlPart::Trigger}, {chip("Left Trigger", Control::TriggerL)});
    Label(page, kLeft, {CtrlPart::Dpad},
          {chip("D-Pad Up", Control::DpadUp), chip("D-Pad Down", Control::DpadDown),
           chip("D-Pad Left", Control::DpadLeft), chip("D-Pad Right", Control::DpadRight)});
    Label(page, kLeft, {CtrlPart::View},
          {chip("View", Control::View), locked("View Holds and Chords")});
    Label(page, kLeft, {CtrlPart::Stick},
          {locked("Left Stick", OrNothing(StickTo(b, AnalogControl::StickL))), locked("Left Stick Click")});
    std::vector<DiagramLine> faces;
    for (const Face& f : Faces(by_position)) {
        faces.push_back(chip(f.frame, f.pad));
    }
    Label(page, kRight, {CtrlPart::A, CtrlPart::B, CtrlPart::X, CtrlPart::Y}, std::move(faces));
    Label(page, kRight, {CtrlPart::Bumper}, {chip("Right Bumper", Control::BumperR)});
    Label(page, kRight, {CtrlPart::Trigger}, {chip("Right Trigger", Control::TriggerR)});
    Label(page, kRight, {CtrlPart::Menu}, {chip("Menu", Control::Menu), locked("Menu Hold")});
    Label(page, kRight, {CtrlPart::Stick},
          {locked("Right Stick", OrNothing(StickTo(b, AnalogControl::StickR))), locked("Right Stick Click")});
    Label(page, kBoth, {CtrlPart::Grip}, {locked("Grips", "Move Screens")});
    return page;
}

DiagramPageId HelpDiagramPageFor(const HelpContext& ctx) {
    return ctx.game_running ? DiagramPageId::Game : DiagramPageId::Window;
}

const DiagramLabel* FindDiagramLabel(const DiagramPage& page, int hand, CtrlPart part) {
    for (const DiagramLabel& l : page.labels) {
        if ((l.hand == hand || l.hand == kBoth) &&
            std::find(l.parts.begin(), l.parts.end(), part) != l.parts.end()) {
            return &l;
        }
    }
    return nullptr;
}

} // namespace FrameXr::Vis
