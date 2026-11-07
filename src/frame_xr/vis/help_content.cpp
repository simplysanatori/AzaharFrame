// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

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
    // The Frame's face buttons, and the pad control each drives (mapper.cpp: by position, the
    // Frame's A is the pad's B, and so on).
    const bool pos = ctx.face_by_position;
    const struct {
        const char* frame;
        Control pad;
    } faces[] = {
        {"A", pos ? Control::B : Control::A},
        {"B", pos ? Control::A : Control::B},
        {"X", pos ? Control::Y : Control::X},
        {"Y", pos ? Control::X : Control::Y},
    };
    for (const auto& f : faces) {
        Row(g, f.frame, OrNothing(ThreeDs(b, f.pad)));
    }
    // The D-pad as one row when it is the 3DS's D-pad, else one row per direction.
    const bool dpad_plain = BoundTo(b, Control::DpadUp) == std::vector<int>{NB::Up} &&
                            BoundTo(b, Control::DpadDown) == std::vector<int>{NB::Down} &&
                            BoundTo(b, Control::DpadLeft) == std::vector<int>{NB::Left} &&
                            BoundTo(b, Control::DpadRight) == std::vector<int>{NB::Right};
    if (dpad_plain) {
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
    Row(g, "Left Stick Click", OrNothing(ThreeDs(b, Control::StickClickL)));
    Row(g, "Right Stick Click", With("Reset Screens", ThreeDs(b, Control::StickClickR)));
    std::string grips = Names(BoundTo(b, Control::GripL));
    if (const std::string r = Names(BoundTo(b, Control::GripR)); !r.empty()) {
        grips += (grips.empty() ? "" : " + ") + r;
    }
    Row(g, "Grip on a Screen", With("Move Screens", grips.empty() ? grips : "3DS " + grips));
    Row(g, "Stick While Moving", "Nearer or Farther");
    Row(g, "Hold Menu (1 s)", "In-Game Menu");
    Row(g, "Hold View (1.5 s)", "Stop Game");
    Row(g, "Hold View + Left Bumper", "Quick Load");
    Row(g, "Hold View + Right Bumper", "Quick Save");
    Row(g, "Both Bumpers + View", "Show or Hide Bottom Screen");

    // ---- Azahar's window (the home panel), the in-game menu, everywhere ----
    auto& w = page.window;
    Heading(w, "Azahar's Window", !ctx.game_running || ctx.in_menu);
    Row(w, "Trigger or A", "Click");
    Row(w, "Right Stick Click", "Right-Click");
    Row(w, "Hold Right Stick Click (1 s)", "Bring Window Back");
    Row(w, "B", "Back");
    Row(w, "Grip", "Move Window");
    Row(w, "Stick Up or Down", "Scroll");
    Row(w, "Y", "Keyboard");
    Heading(w, "In-Game Menu", ctx.in_menu);
    Row(w, "Hold Menu (1 s)", "Back to Game");
    Row(w, "B (Nothing Open)", "Back to Game");
    Row(w, "Hold View (1.5 s)", "Stop Game");
    Heading(w, "Everywhere", false);
    Row(w, "Both Bumpers + View + Menu", "Switch VR / Flat");

    page.footer = std::string{"Face Buttons: "} +
                  (pos ? "By Position (Nintendo)" : "By Label (Xbox)");
    if (!given.valid) {
        page.footer += "  \xC2\xB7  Default Controls";
    } else if (!given.profile_name.empty()) {
        page.footer += "  \xC2\xB7  Profile: " + given.profile_name;
    }
    return page;
}

} // namespace FrameXr::Vis
