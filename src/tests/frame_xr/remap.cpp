// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame remap: the state machine of Edit Controls (frame_xr/menu/remap.h).

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/remap.h"
#include "frame_xr/menu/tab_menu.h"

using namespace FrameXr::Menu;
using FrameXr::Input::Control;

namespace {

constexpr int C(Control c) {
    return static_cast<int>(c);
}

// Buttons by index in the 3DS's order for the test (A, B, X, Y, Up, Down, Left, Right, L, R, ...).
enum : int { kA, kB, kX, kY, kUp, kDown, kLeft, kRight, kL, kR, kStart, kSelect, kZL = 14, kZR };

// The default profile: each button on the control of the same name.
RemapTable Defaults() {
    RemapTable t;
    t.fill(-1);
    t[kA] = C(Control::A);
    t[kB] = C(Control::B);
    t[kX] = C(Control::X);
    t[kY] = C(Control::Y);
    t[kUp] = C(Control::DpadUp);
    t[kDown] = C(Control::DpadDown);
    t[kLeft] = C(Control::DpadLeft);
    t[kRight] = C(Control::DpadRight);
    t[kL] = C(Control::BumperL);
    t[kR] = C(Control::BumperR);
    t[kStart] = C(Control::Menu);
    t[kSelect] = C(Control::View);
    t[kZL] = C(Control::TriggerL);
    t[kZR] = C(Control::TriggerR);
    return t;
}

Remap Started() {
    Remap r;
    r.Begin(Defaults(), Defaults());
    return r;
}

} // namespace

TEST_CASE("Remap: edit mode starts idle and ends with Done", "[frame_xr]") {
    Remap r;
    REQUIRE_FALSE(r.Editing());
    REQUIRE(r.Choose(C(Control::A)) == Remap::Event::None); // not editing
    r.Begin(Defaults(), Defaults());
    REQUIRE(r.Editing());
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.Done() == Remap::Event::Done);
    REQUIRE_FALSE(r.Editing());
}

TEST_CASE("Remap: a chip listens, a press moves its function", "[frame_xr]") {
    Remap r = Started();
    // The Y button's chip: 3DS Y goes to the Frame's right bumper (which had R): they swap.
    REQUIRE(r.Choose(C(Control::Y)) == Remap::Event::Listening);
    REQUIRE(r.Listening() == C(Control::Y));
    REQUIRE(r.Press(C(Control::BumperR)) == Remap::Event::Swapped);
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.Table()[kY] == C(Control::BumperR));
    REQUIRE(r.Table()[kR] == C(Control::Y));
    REQUIRE(r.Table()[kA] == C(Control::A)); // the rest stays
}

TEST_CASE("Remap: a function moves to a control that had none", "[frame_xr]") {
    RemapTable t = Defaults();
    t[kStart] = -1; // nothing on Menu
    Remap r;
    r.Begin(t, Defaults());
    r.Choose(C(Control::View)); // View has Select
    REQUIRE(r.Press(C(Control::Menu)) == Remap::Event::Moved);
    REQUIRE(r.Table()[kSelect] == C(Control::Menu));
    REQUIRE(RemapButtonsOn(r.Table(), C(Control::View)) == 0);
}

TEST_CASE("Remap: a control with two functions swaps both", "[frame_xr]") {
    RemapTable t = Defaults();
    t[kStart] = C(Control::A); // A drives A and Start
    Remap r;
    r.Begin(t, Defaults());
    r.Choose(C(Control::A));
    REQUIRE(r.Press(C(Control::X)) == Remap::Event::Swapped);
    REQUIRE(r.Table()[kA] == C(Control::X));
    REQUIRE(r.Table()[kStart] == C(Control::X));
    REQUIRE(r.Table()[kX] == C(Control::A));
}

TEST_CASE("Remap: pressing the listening chip's own button, or a gesture, does not move", "[frame_xr]") {
    Remap r = Started();
    r.Choose(C(Control::A));
    SECTION("its own button cancels") {
        REQUIRE(r.Press(C(Control::A)) == Remap::Event::Cancelled);
        REQUIRE(r.Listening() == -1);
        REQUIRE(r.Table() == Defaults());
    }
    SECTION("a grip or stick click is no target: it keeps listening") {
        REQUIRE(r.Press(C(Control::GripL)) == Remap::Event::None);
        REQUIRE(r.Press(C(Control::StickClickR)) == Remap::Event::None);
        REQUIRE(r.Listening() == C(Control::A));
        REQUIRE(r.Table() == Defaults());
    }
    SECTION("a trigger is no target either (it clicks the beam)") {
        REQUIRE(r.Press(C(Control::TriggerL)) == Remap::Event::None);
        REQUIRE(r.Listening() == C(Control::A));
    }
}

TEST_CASE("Remap: only 3DS buttons are editable", "[frame_xr]") {
    Remap r = Started();
    for (const Control c : {Control::GripL, Control::GripR, Control::StickClickL, Control::StickClickR}) {
        REQUIRE_FALSE(Remap::Editable(C(c)));
        REQUIRE(r.Choose(C(c)) == Remap::Event::None);
        REQUIRE(r.Listening() == -1);
    }
    for (const Control c : {Control::A, Control::DpadUp, Control::BumperL, Control::TriggerR,
                            Control::Menu, Control::View}) {
        REQUIRE(Remap::Editable(C(c)));
    }
    // A press while nothing listens does nothing.
    REQUIRE(r.Press(C(Control::A)) == Remap::Event::None);
    REQUIRE(r.Table() == Defaults());
}

TEST_CASE("Remap: Cancel, another chip, and the timeout", "[frame_xr]") {
    Remap r = Started();
    r.Choose(C(Control::X));
    REQUIRE(r.Cancel() == Remap::Event::Cancelled);
    REQUIRE(r.Cancel() == Remap::Event::None);
    REQUIRE(r.Table() == Defaults());

    r.Choose(C(Control::X));
    REQUIRE(r.Choose(C(Control::Y)) == Remap::Event::Listening); // switches chips
    REQUIRE(r.Listening() == C(Control::Y));
    REQUIRE(r.Choose(C(Control::Y)) == Remap::Event::Cancelled); // the same chip again

    r.Choose(C(Control::X));
    REQUIRE(r.Tick(Remap::kListenSeconds * 0.5f) == Remap::Event::None);
    REQUIRE(r.Listening() == C(Control::X));
    REQUIRE(r.Tick(Remap::kListenSeconds * 0.6f) == Remap::Event::Cancelled);
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.Tick(100.0f) == Remap::Event::None);
}

TEST_CASE("Remap: Reset to Default restores the profile and stops listening", "[frame_xr]") {
    Remap r = Started();
    r.Choose(C(Control::A));
    r.Press(C(Control::X));
    REQUIRE(r.Table() != Defaults());
    r.Choose(C(Control::B));
    REQUIRE(r.Reset() == Remap::Event::Reset);
    REQUIRE(r.Table() == Defaults());
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.Editing());
}

TEST_CASE("Remap: every change bumps the revision, so the menu redraws", "[frame_xr]") {
    Remap r = Started();
    uint32_t rev = r.View().rev;
    r.Choose(C(Control::A));
    REQUIRE(r.View().rev != rev);
    rev = r.View().rev;
    r.Press(C(Control::X));
    REQUIRE(r.View().rev != rev);
    rev = r.View().rev;
    r.Done();
    REQUIRE(r.View().rev != rev);
    REQUIRE_FALSE(r.View().editing);
}

TEST_CASE("Remap: the Controls tab in edit mode has chips, Cancel only while listening", "[frame_xr]") {
    TabMenu menu;
    Kit kit{1.0f};
    frame_xr::ui::Canvas canvas;
    // A diagram that draws two chips, as the real one draws one per control.
    menu.SetDiagram([](Kit&, const Rect& area, int, const DiagramEdit& edit,
                       std::vector<DiagramChipHit>& chips) {
        if (edit.on) {
            chips.push_back({{area.x0, area.y0, area.x0 + 100, area.y0 + 40}, 0});
            chips.push_back({{area.x0 + 120, area.y0, area.x0 + 220, area.y0 + 40}, 13});
        }
    });
    const auto has = [&menu](Id id, int index) {
        for (const auto& h : menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                return true;
            }
        }
        return false;
    };
    MenuModel m;
    MenuView v;
    v.tab = Tab::Controls;
    v.remap.editing = true;
    canvas.Init(static_cast<int>(kPanelW), static_cast<int>(kPanelH));
    kit.Begin(canvas);
    menu.Draw(kit, m, v);
    CHECK(has(Id::RemapChip, 0));
    CHECK(has(Id::RemapChip, 13));
    CHECK(has(Id::RemapDone, -1));
    CHECK(has(Id::RemapReset, -1));
    CHECK_FALSE(has(Id::RemapCancel, -1));
    CHECK_FALSE(has(Id::EditControls, -1)); // not in edit mode's own screen
    CHECK_FALSE(has(Id::ControlsPage, 0));
    v.remap.listening = 13;
    canvas.Init(static_cast<int>(kPanelW), static_cast<int>(kPanelH));
    kit.Begin(canvas);
    menu.Draw(kit, m, v);
    CHECK(has(Id::RemapCancel, -1));
}
