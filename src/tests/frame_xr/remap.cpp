// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame remap: Edit Controls (frame_xr/menu/remap.h, edit_devices.h, ds_drawing.h): the
// state machine (a 3DS button listens, a device's button press gives it that control), the
// devices' button names, and the Controls tab's edit screen (AzaharFrame padsui).

#include <algorithm>
#include <initializer_list>
#include <set>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/ds_drawing.h"
#include "frame_xr/menu/edit_devices.h"
#include "frame_xr/menu/remap.h"
#include "frame_xr/menu/tab_menu.h"

using namespace FrameXr::Menu;
using FrameXr::Input::Control;

namespace {

constexpr int C(Control c) {
    return static_cast<int>(c);
}

namespace B = DsButton;

// The default profile: each button on the control of the same name.
RemapTable Defaults() {
    RemapTable t;
    t.fill(-1);
    t[B::A] = C(Control::A);
    t[B::B] = C(Control::B);
    t[B::X] = C(Control::X);
    t[B::Y] = C(Control::Y);
    t[B::Up] = C(Control::DpadUp);
    t[B::Down] = C(Control::DpadDown);
    t[B::Left] = C(Control::DpadLeft);
    t[B::Right] = C(Control::DpadRight);
    t[B::L] = C(Control::BumperL);
    t[B::R] = C(Control::BumperR);
    t[B::Start] = C(Control::Menu);
    t[B::Select] = C(Control::View);
    t[B::ZL] = C(Control::TriggerL);
    t[B::ZR] = C(Control::TriggerR);
    return t;
}

Remap Started(RemapSource source = RemapSource::Frame) {
    Remap r;
    r.Begin(Defaults(), Defaults(), source);
    return r;
}

} // namespace

TEST_CASE("Remap: edit mode starts idle and ends with Done", "[frame_xr]") {
    Remap r;
    REQUIRE_FALSE(r.Editing());
    REQUIRE(r.Choose(B::A) == Remap::Event::None); // not editing
    r.Begin(Defaults(), Defaults());
    REQUIRE(r.Editing());
    REQUIRE(r.Source() == RemapSource::Frame);
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.Done() == Remap::Event::Done);
    REQUIRE_FALSE(r.Editing());
}

TEST_CASE("Remap: a 3DS button listens, a press gives it that control, the other swaps", "[frame_xr]") {
    Remap r = Started();
    // 3DS Y takes the Frame's right bumper, which R had: R takes Y's old control.
    REQUIRE(r.Choose(B::Y) == Remap::Event::Listening);
    REQUIRE(r.Listening() == B::Y);
    REQUIRE(r.Press(C(Control::BumperR)) == Remap::Event::Swapped);
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.Table()[B::Y] == C(Control::BumperR));
    REQUIRE(r.Table()[B::R] == C(Control::Y));
    REQUIRE(r.Table()[B::A] == C(Control::A)); // the rest stays
}

TEST_CASE("Remap: a control no 3DS button had just moves", "[frame_xr]") {
    RemapTable t = Defaults();
    t[B::Start] = -1; // nothing on Menu
    Remap r;
    r.Begin(t, Defaults());
    r.Choose(B::Select);
    REQUIRE(r.Press(C(Control::Menu)) == Remap::Event::Moved);
    REQUIRE(r.Table()[B::Select] == C(Control::Menu));
    REQUIRE(RemapButtonsOn(r.Table(), C(Control::View)) == 0); // View drives nothing now
    // Home had nothing: taking another's control leaves that one with nothing (Moved).
    r.Choose(B::Home);
    REQUIRE(r.Press(C(Control::A)) == Remap::Event::Moved);
    REQUIRE(r.Table()[B::Home] == C(Control::A));
    REQUIRE(r.Table()[B::A] == -1);
}

TEST_CASE("Remap: a control on two 3DS buttons swaps on both", "[frame_xr]") {
    RemapTable t = Defaults();
    t[B::Start] = C(Control::X); // X drives X and Start
    Remap r;
    r.Begin(t, Defaults());
    r.Choose(B::A);
    REQUIRE(r.Press(C(Control::X)) == Remap::Event::Swapped);
    REQUIRE(r.Table()[B::A] == C(Control::X));
    REQUIRE(r.Table()[B::X] == C(Control::A));
    REQUIRE(r.Table()[B::Start] == C(Control::A));
}

TEST_CASE("Remap: the control it has, or one that cannot be used, does not move", "[frame_xr]") {
    Remap r = Started();
    r.Choose(B::A);
    SECTION("its own control cancels") {
        REQUIRE(r.Press(C(Control::A)) == Remap::Event::Cancelled);
        REQUIRE(r.Listening() == -1);
        REQUIRE(r.Table() == Defaults());
    }
    SECTION("a Frame grip or stick click keeps listening") {
        REQUIRE(r.Press(C(Control::GripL)) == Remap::Event::None);
        REQUIRE(r.Press(C(Control::StickClickR)) == Remap::Event::None);
        REQUIRE(r.Listening() == B::A);
        REQUIRE(r.Table() == Defaults());
    }
    SECTION("a Frame trigger is no source either (it clicks the beam)") {
        REQUIRE(r.Press(C(Control::TriggerL)) == Remap::Event::None);
        REQUIRE(r.Listening() == B::A);
    }
}

TEST_CASE("Remap: a gamepad's triggers, paddles and stick clicks can be used", "[frame_xr]") {
    Remap r = Started(RemapSource::Pad);
    REQUIRE(r.Source() == RemapSource::Pad);
    for (const Control c : {Control::TriggerL, Control::GripR, Control::StickClickL}) {
        REQUIRE(Remap::Listenable(C(c), RemapSource::Pad));
        REQUIRE_FALSE(Remap::Listenable(C(c), RemapSource::Frame));
    }
    REQUIRE_FALSE(Remap::Listenable(-1, RemapSource::Pad));
    REQUIRE_FALSE(Remap::Listenable(FrameXr::Input::kNumControls, RemapSource::Pad));
    r.Choose(B::Home);
    REQUIRE(r.Press(C(Control::StickClickL)) == Remap::Event::Moved);
    REQUIRE(r.Table()[B::Home] == C(Control::StickClickL));
    r.Choose(B::A);
    REQUIRE(r.Press(C(Control::TriggerR)) == Remap::Event::Swapped);
    REQUIRE(r.Table()[B::A] == C(Control::TriggerR));
    REQUIRE(r.Table()[B::ZR] == C(Control::A));
}

TEST_CASE("Remap: only the 3DS's own buttons can listen", "[frame_xr]") {
    Remap r = Started();
    for (const int b : std::initializer_list<int>{12, 13, 17, -1, 18, 31, 34}) { // Debug, GPIO14, Power
        REQUIRE_FALSE(Remap::Editable(b));
        REQUIRE(r.Choose(b) == Remap::Event::None);
        REQUIRE(r.Listening() == -1);
    }
    for (const int b : {B::A, B::Up, B::L, B::ZR, B::Start, B::Select, B::Home}) {
        REQUIRE(Remap::Editable(b));
    }
    // A press while nothing listens does nothing.
    REQUIRE(r.Press(C(Control::A)) == Remap::Event::None);
    REQUIRE(r.Table() == Defaults());
}

TEST_CASE("Remap: Cancel, another 3DS button, and the timeout", "[frame_xr]") {
    Remap r = Started();
    r.Choose(B::X);
    REQUIRE(r.Cancel() == Remap::Event::Cancelled);
    REQUIRE(r.Cancel() == Remap::Event::None);
    REQUIRE(r.Table() == Defaults());

    r.Choose(B::X);
    REQUIRE(r.Choose(B::Y) == Remap::Event::Listening); // switches
    REQUIRE(r.Listening() == B::Y);
    REQUIRE(r.Choose(B::Y) == Remap::Event::Cancelled); // the same again

    r.Choose(B::X);
    REQUIRE(r.Tick(Remap::kListenSeconds * 0.5f) == Remap::Event::None);
    REQUIRE(r.Listening() == B::X);
    REQUIRE(r.Tick(Remap::kListenSeconds * 0.6f) == Remap::Event::Cancelled);
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.Tick(100.0f) == Remap::Event::None);
}

TEST_CASE("Remap: Reset to Default restores the device's default and stops listening", "[frame_xr]") {
    Remap r = Started();
    r.Choose(B::A);
    r.Press(C(Control::X));
    REQUIRE(r.Table() != Defaults());
    r.Choose(B::B);
    REQUIRE(r.Reset() == Remap::Event::Reset);
    REQUIRE(r.Table() == Defaults());
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.Editing());
    // Another device: Begin again with its own table.
    r.Choose(B::A);
    r.Begin(PositionalPadTable(), PositionalPadTable(), RemapSource::Pad);
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.Table() == PositionalPadTable());
    REQUIRE(r.Source() == RemapSource::Pad);
}

TEST_CASE("Remap: every change bumps the revision, so the menu redraws", "[frame_xr]") {
    Remap r = Started();
    uint32_t rev = r.View().rev;
    r.Choose(B::A);
    REQUIRE(r.View().rev != rev);
    rev = r.View().rev;
    r.Press(C(Control::X));
    REQUIRE(r.View().rev != rev);
    rev = r.View().rev;
    r.Done();
    REQUIRE(r.View().rev != rev);
    REQUIRE_FALSE(r.View().editing);
}

TEST_CASE("Edit Controls: each device's buttons by its own names", "[frame_xr]") {
    // The Frame by position: the Frame's B drives Control::A, so the 3DS's A shows "B".
    CHECK(SourceName(ButtonNames::Frame, C(Control::A), true) == "B");
    CHECK(SourceName(ButtonNames::Frame, C(Control::A), false) == "A");
    CHECK(SourceName(ButtonNames::Frame, C(Control::BumperL)) == "Left Bumper");
    CHECK(SourceName(ButtonNames::Frame, -1).empty());
    const ButtonNames ds4 = NamesFor(4, false);
    CHECK(ds4 == ButtonNames::DualShock);
    CHECK(SourceName(ds4, C(Control::A)) == "Cross");
    CHECK(SourceName(ds4, C(Control::B)) == "Circle");
    CHECK(SourceName(ds4, C(Control::X)) == "Square");
    CHECK(SourceName(ds4, C(Control::Y)) == "Triangle");
    CHECK(SourceName(ds4, C(Control::TriggerL)) == "L2");
    CHECK(SourceName(ds4, C(Control::View)) == "Share");
    CHECK(SourceName(NamesFor(7, false), C(Control::View)) == "Create");
    CHECK(SourceName(NamesFor(2, false), C(Control::BumperR)) == "RB");
    CHECK(SourceName(NamesFor(2, false), C(Control::View)) == "View");
    // A Switch pad: by label as SDL reports it, by position behind Steam Input.
    CHECK(SourceName(NamesFor(5, false), C(Control::A)) == "A");
    CHECK(SourceName(NamesFor(5, true), C(Control::A)) == "B");
    CHECK(SourceName(NamesFor(5, false), C(Control::Menu)) == "Plus");
    CHECK(SourceName(NamesFor(100, false), C(Control::GripL)) == "Left Grip");
    CHECK(SourceName(NamesFor(0, false), C(Control::GripL)) == "Left Paddle");
    CHECK(StickRouteName(0) == "Left Stick");
    CHECK(StickRouteName(1) == "Right Stick");
    CHECK(StickRouteName(FrameXr::Input::EncodeStickRoute({1, false, false, true})) ==
          "Right Stick Flipped");
    CHECK(StickRouteName(-1).empty());
    // The positional default: the right button is the 3DS's A.
    const RemapTable p = PositionalPadTable();
    CHECK(SourceName(ds4, p[B::A]) == "Circle");
    CHECK(SourceName(ds4, p[B::B]) == "Cross");
    CHECK(p[B::Home] == -1);
}

TEST_CASE("Edit Controls: the 3DS drawing keeps every input inside it", "[frame_xr]") {
    const DsDrawing d = PlaceDs({100.0f, 50.0f, 700.0f, 400.0f});
    CHECK(d.box.H() == 350.0f); // height bound
    CHECK(d.box.Cx() == 400.0f);
    for (const int t : std::initializer_list<int>{B::A, B::B, B::X, B::Y, B::Up, B::Down, B::Left,
                                                  B::Right, B::L, B::R, B::Start, B::Select, B::ZL,
                                                  B::ZR, B::Home, kDsCirclePad, kDsCStick}) {
        const DsPoint p = d.Anchor(t);
        CHECK(d.box.Contains(p.x, p.y));
    }
    const Rect bs = d.BottomScreen();
    CHECK(bs.W() / bs.H() > 1.25f); // 4:3
    CHECK(bs.W() / bs.H() < 1.42f);
    CHECK(d.box.Contains(bs.x0, bs.y0));
}

namespace {

struct EditScreen {
    TabMenu menu;
    Kit kit{1.0f};
    frame_xr::ui::Canvas canvas;
    MenuModel m;
    MenuView v;

    EditScreen() {
        EditDevice frame;
        frame.name = "Frame Controllers";
        frame.mapping = Defaults();
        EditDevice pad;
        pad.name = "DualShock 4";
        pad.kind = DeviceKind::Gamepad;
        pad.names = ButtonNames::DualShock;
        pad.has_touchpad = true;
        pad.mapping = PositionalPadTable();
        EditDevice mouse;
        mouse.name = "Mouse";
        mouse.kind = DeviceKind::Mouse;
        mouse.names = ButtonNames::Mouse; // AzaharFrame padsui: as the mouse's default
        mouse.sticks = {-1, -1};
        mouse.mapping[B::A] = 1;
        mouse.mapping[B::L] = 3;
        mouse.mapping[B::R] = 4;
        mouse.extras = {0, 2};
        m.devices.list = {frame, pad, mouse};
        v.tab = Tab::Controls;
        v.remap.editing = true;
    }
    void Draw() {
        canvas.Init(static_cast<int>(kPanelW), static_cast<int>(kPanelH));
        kit.Begin(canvas);
        menu.Draw(kit, m, v);
    }
    bool Has(Id id, int index) const {
        return std::any_of(menu.Hits().begin(), menu.Hits().end(),
                           [&](const auto& h) { return h.t.id == id && h.t.index == index; });
    }
};

} // namespace

TEST_CASE("Edit Controls: a chip on every 3DS button, the devices, Cancel only while listening",
          "[frame_xr]") {
    EditScreen s;
    s.Draw();
    for (const int b : {B::A, B::B, B::X, B::Y, B::Up, B::Down, B::Left, B::Right, B::L, B::R, B::Start,
                        B::Select, B::ZL, B::ZR, B::Home}) {
        CHECK(s.Has(Id::RemapChip, b));
    }
    CHECK(s.Has(Id::RemapChip, kDsCirclePad));
    CHECK(s.Has(Id::RemapChip, kDsCStick));
    CHECK(s.Has(Id::EditDevice, 0));
    CHECK(s.Has(Id::EditDevice, 1));
    CHECK(s.Has(Id::EditDevice, 2));
    CHECK(s.Has(Id::RemapDone, -1));
    CHECK(s.Has(Id::RemapReset, -1));
    CHECK_FALSE(s.Has(Id::RemapCancel, -1));
    CHECK_FALSE(s.Has(Id::TouchpadTouch, 0)); // the Frame controllers: no pointer here
    CHECK_FALSE(s.Has(Id::EditControls, -1)); // not in edit mode's own screen
    CHECK_FALSE(s.Has(Id::ControlsPage, 0));

    // Every target is at least 48 dp tall, none overlap, all inside the panel.
    const auto& hits = s.menu.Hits();
    for (std::size_t i = 0; i < hits.size(); i++) {
        const Rect& a = hits[i].r;
        if (hits[i].t.id == Id::RemapChip || hits[i].t.id == Id::EditDevice) {
            CHECK(a.H() >= 48.0f - 0.01f);
            CHECK(a.x0 >= 104.0f);
            CHECK(a.x1 <= kPanelW - 12.0f);
            for (std::size_t j = i + 1; j < hits.size(); j++) {
                const Rect& b = hits[j].r;
                if (hits[j].t.id != Id::RemapChip && hits[j].t.id != Id::EditDevice) {
                    continue;
                }
                const bool apart = a.x1 <= b.x0 || b.x1 <= a.x0 || a.y1 <= b.y0 + 0.01f ||
                                   b.y1 <= a.y0 + 0.01f;
                CHECK(apart);
            }
        }
    }

    s.v.remap.listening = B::X;
    s.Draw();
    CHECK(s.Has(Id::RemapCancel, -1));
    CHECK_FALSE(s.Has(Id::EditDevice, 0)); // the snackbar is where the devices were
}

TEST_CASE("Edit Controls: a pad with a touchpad shows Touchpad Touch, the mouse its Mouse Mode",
          "[frame_xr]") {
    EditScreen s;
    s.m.devices.selected = 1;
    s.Draw();
    CHECK(s.Has(Id::RemapChip, B::A));
    CHECK(s.Has(Id::TouchpadTouch, 0));
    CHECK(s.Has(Id::TouchpadTouch, 2));
    CHECK(s.Has(Id::RemapReset, -1));
    CHECK(s.Has(Id::RecentreAtEdge, -1));
    CHECK_FALSE(s.Has(Id::RemapChip, B::Touch));
    CHECK_FALSE(s.Has(Id::MouseMode, 0));
    CHECK(s.Has(Id::UseDevice, -1)); // a gamepad can be switched off
    CHECK_FALSE(s.Has(Id::CursorSpeed, -1));
    // AzaharFrame padsui: the mouse: its buttons, Touch Screen and Open Menu, no sticks; Mouse
    // Mode in place of Touchpad Touch; Recentre at Edge in Bottom Screen mode only.
    s.m.devices.selected = 2;
    s.Draw();
    for (const int b : std::initializer_list<int>{B::A, B::B, B::X, B::Y, B::Up, B::L, B::R, B::ZL,
                                                  B::ZR, B::Start, B::Select, B::Home, B::Touch,
                                                  B::OpenMenu}) {
        CHECK(s.Has(Id::RemapChip, b));
    }
    CHECK_FALSE(s.Has(Id::RemapChip, kDsCirclePad));
    CHECK_FALSE(s.Has(Id::RemapChip, kDsCStick));
    CHECK(s.Has(Id::RemapReset, -1));
    CHECK(s.Has(Id::MouseMode, 0));
    CHECK(s.Has(Id::MouseMode, 1));
    CHECK_FALSE(s.Has(Id::TouchpadTouch, 1));
    CHECK(s.Has(Id::RecentreAtEdge, -1));
    CHECK(s.Has(Id::EditDevice, 2));
    CHECK(s.Has(Id::UseDevice, -1));
    CHECK(s.Has(Id::CursorSpeed, -1));
    CHECK(s.menu.Layout().SliderAt(Id::CursorSpeed, 10000.0f) == 1.0f);
    const auto& hits = s.menu.Hits();
    for (std::size_t i = 0; i < hits.size(); i++) {
        const auto counted = [](Id id) {
            return id == Id::RemapChip || id == Id::RecentreAtEdge || id == Id::CursorSpeed ||
                   id == Id::UseDevice || id == Id::EditDevice || id == Id::RemapReset ||
                   id == Id::MouseMode || id == Id::RemapDone;
        };
        const Id ai = hits[i].t.id;
        if (!counted(ai)) {
            continue;
        }
        CHECK(hits[i].r.H() >= 48.0f - 0.01f);
        for (std::size_t j = i + 1; j < hits.size(); j++) {
            const Id bj = hits[j].t.id;
            if (!counted(bj)) {
                continue;
            }
            const Rect& a = hits[i].r;
            const Rect& b = hits[j].r;
            CHECK((a.x1 <= b.x0 || b.x1 <= a.x0 || a.y1 <= b.y0 + 0.01f || b.y1 <= a.y0 + 0.01f));
        }
    }
    CHECK(SourceName(ButtonNames::Mouse, 1) == "Right Click");
    // The Frame Controllers are always used: no switch.
    s.m.devices.selected = 0;
    s.Draw();
    CHECK_FALSE(s.Has(Id::UseDevice, -1));
    CHECK_FALSE(s.Has(Id::CursorSpeed, -1));
    s.m.devices.selected = 2;
    s.m.devices.list[2].mouse_mode = 1;
    s.Draw();
    CHECK_FALSE(s.Has(Id::RecentreAtEdge, -1));
    // No devices from the host: the chips still show, each with nothing on it.
    EditScreen none;
    none.m.devices = {};
    none.Draw();
    CHECK(none.Has(Id::RemapChip, B::A));
}

TEST_CASE("Remap: the mouse's Touch Screen and Open Menu swap with its buttons", "[frame_xr]") {
    // Left Click 0, Right Click 1, Middle Click 2, Back 3, Forward 4, Wheel Up 5, Wheel Down 6.
    RemapTable t = kNoMapping;
    t[B::A] = 1;
    t[B::L] = 3;
    Remap r;
    r.Begin(t, t, RemapSource::Mouse, {-1, -1}, {-1, -1}, {0, 2}, {0, 2});
    CHECK(Remap::Listenable(6, RemapSource::Mouse));
    CHECK_FALSE(Remap::Listenable(7, RemapSource::Mouse));
    // A takes Left Click: Touch Screen gets Right Click.
    REQUIRE(r.Choose(B::A) == Remap::Event::Listening);
    CHECK(r.Press(0) == Remap::Event::Swapped);
    CHECK(r.Table()[B::A] == 0);
    CHECK(r.Extras()[0] == 1);
    // Open Menu takes Back: L has Middle Click.
    REQUIRE(r.Choose(B::OpenMenu) == Remap::Event::Listening);
    CHECK(r.Press(3) == Remap::Event::Swapped);
    CHECK(r.Extras()[1] == 3);
    CHECK(r.Table()[B::L] == 2);
    // Touch Screen takes Wheel Down, which nothing had.
    REQUIRE(r.Choose(B::Touch) == Remap::Event::Listening);
    CHECK(r.Press(6) == Remap::Event::Moved);
    CHECK(r.Extras()[0] == 6);
    CHECK(r.Reset() == Remap::Event::Reset);
    CHECK(r.Extras() == RemapExtras{0, 2});
    // A gamepad has no Touch Screen to choose.
    Remap pad;
    pad.Begin(t, t, RemapSource::Pad);
    pad.Choose(B::Touch);
    CHECK(pad.Listening() < 0);
}

TEST_CASE("Edit Controls: many devices still fit the row", "[frame_xr]") {
    EditScreen s;
    for (int i = 0; i < 5; i++) {
        EditDevice d;
        d.name = "8BitDo Ultimate 2C Wireless Controller";
        d.kind = DeviceKind::Gamepad;
        s.m.devices.list.push_back(d);
    }
    s.Draw();
    float right = 0.0f;
    for (const auto& h : s.menu.Hits()) {
        if (h.t.id == Id::EditDevice) {
            right = std::max(right, h.r.x1);
        }
    }
    CHECK(right > 0.0f);
    CHECK(right <= kPanelW - 12.0f);
}

TEST_CASE("Stick routes: two pushes give the stick, its axes and their directions", "[frame_xr]") {
    using namespace FrameXr::Input;
    // Plain: up is +Y, right is +X.
    StickRoute r = RouteFromPushes({0, 1, 1}, {0, 0, 1});
    CHECK(r == StickRoute{0, false, false, false});
    CHECK(EncodeStickRoute(r) == 0);
    // Up read as down (an inverted Y), right as right.
    r = RouteFromPushes({1, 1, -1}, {1, 0, 1});
    CHECK(r == StickRoute{1, false, false, true});
    Vec2f v = ApplyStickRoute({0.0f, -1.0f}, r);
    CHECK(v.y == 1.0f);
    // A pad turned a quarter: up moves X, right moves -Y.
    r = RouteFromPushes({0, 0, 1}, {0, 1, -1});
    CHECK(r.swap_xy);
    CHECK(r.invert_x);
    v = ApplyStickRoute({1.0f, 0.0f}, r); // pushed "up"
    CHECK(v.x == 0.0f);
    CHECK(v.y == 1.0f);
    v = ApplyStickRoute({0.0f, -1.0f}, r); // pushed "right"
    CHECK(v.x == 1.0f);
    // Not an answer: the same axis twice, or two sticks.
    CHECK(RouteFromPushes({0, 1, 1}, {0, 1, -1}).stick == -1);
    CHECK(RouteFromPushes({0, 1, 1}, {1, 0, 1}).stick == -1);
    // Round trip of every route.
    for (int st = 0; st < 2; st++) {
        for (int bits = 0; bits < 8; bits++) {
            const StickRoute q{st, (bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0};
            CHECK(DecodeStickRoute(EncodeStickRoute(q)) == q);
        }
    }
    CHECK(DecodeStickRoute(-1).stick == -1);
    // A push is clear: past 0.7 along one axis, the other under half of it.
    CHECK(DetectStickPush(0, {0.1f, 0.9f}) == StickPush{0, 1, 1});
    CHECK(DetectStickPush(1, {-0.8f, 0.2f}) == StickPush{1, 0, -1});
    CHECK(DetectStickPush(0, {0.6f, 0.6f}).stick == -1);
    CHECK(DetectStickPush(0, {0.5f, 0.0f}).stick == -1);
}

TEST_CASE("Remap: a 3DS stick learns from two pushes, the other swaps", "[frame_xr]") {
    using FrameXr::Input::DecodeStickRoute;
    Remap r = Started();
    REQUIRE(r.Choose(B::CirclePad) == Remap::Event::Listening);
    REQUIRE(r.ListeningStick());
    REQUIRE(r.View().step == 0);
    REQUIRE(r.Press(C(Control::A)) == Remap::Event::None); // buttons do nothing now
    REQUIRE(r.Stick(1, {0.2f, 0.3f}) == Remap::Event::None); // not a clear push
    REQUIRE(r.Stick(1, {0.0f, 0.95f}) == Remap::Event::Next); // the right stick, up
    REQUIRE(r.View().step == 1);
    REQUIRE(r.Stick(1, {0.0f, 0.95f}) == Remap::Event::None); // still up: waits for right
    REQUIRE(r.Stick(0, {0.95f, 0.0f}) == Remap::Event::None); // the other stick: no
    REQUIRE(r.Stick(1, {0.9f, 0.1f}) == Remap::Event::Swapped);
    REQUIRE(r.Listening() == -1);
    REQUIRE(r.View().step == 0);
    REQUIRE(r.Sticks()[0] == 1); // the Circle Pad on the right stick
    REQUIRE(r.Sticks()[1] == 0); // the C-Stick took the left
    // The same route again: no change.
    r.Choose(B::CirclePad);
    r.Stick(1, {0.0f, 1.0f});
    REQUIRE(r.Stick(1, {1.0f, 0.0f}) == Remap::Event::Cancelled);
    // Inverted Y on the C-Stick: up pushed as down.
    r.Choose(B::CStick);
    r.Stick(0, {0.0f, -1.0f});
    REQUIRE(r.Stick(0, {1.0f, 0.0f}) == Remap::Event::Moved);
    REQUIRE(DecodeStickRoute(r.Sticks()[1]).invert_y);
    REQUIRE(DecodeStickRoute(r.Sticks()[1]).stick == 0);
    // Cancel and Reset.
    r.Choose(B::CStick);
    r.Stick(0, {0.0f, 1.0f});
    REQUIRE(r.Cancel() == Remap::Event::Cancelled);
    REQUIRE(r.View().step == 0);
    REQUIRE(r.Reset() == Remap::Event::Reset);
    REQUIRE(r.Sticks() == kDefaultSticks);
    // Timeout restarts with each step.
    r.Choose(B::CirclePad);
    r.Tick(Remap::kListenSeconds * 0.9f);
    r.Stick(0, {0.0f, 1.0f});
    REQUIRE(r.Tick(Remap::kListenSeconds * 0.5f) == Remap::Event::None);
    REQUIRE(r.Tick(Remap::kListenSeconds * 0.6f) == Remap::Event::Cancelled);
}

TEST_CASE("Remap: a gamepad's sticks swap the same way", "[frame_xr]") {
    Remap r;
    r.Begin(PositionalPadTable(), PositionalPadTable(), RemapSource::Pad, kDefaultSticks,
            kDefaultSticks);
    r.Choose(B::CStick);
    r.Stick(0, {0.0f, 1.0f});
    REQUIRE(r.Stick(0, {1.0f, 0.0f}) == Remap::Event::Swapped);
    REQUIRE(r.Sticks() == RemapSticks{1, 0});
    REQUIRE(r.Table() == PositionalPadTable()); // buttons untouched
}

TEST_CASE("Edit Controls: a listening stick asks for up, then right", "[frame_xr]") {
    EditScreen s;
    s.v.remap.listening = B::CirclePad;
    s.Draw();
    CHECK(s.Has(Id::RemapCancel, -1));
    s.v.remap.step = 1;
    s.Draw();
    CHECK(s.Has(Id::RemapCancel, -1));
}
