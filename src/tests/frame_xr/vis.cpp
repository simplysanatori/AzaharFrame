// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/vis/beam.h"
#include "frame_xr/vis/controller_diagram.h"
#include "frame_xr/vis/help_content.h"
#include "frame_xr/vis/help_layout.h"
#include "frame_xr/vis/model_file.h"
#include "frame_xr/vis/stats_place.h"

// AzaharFrame vrvis
using Catch::Approx;
using namespace FrameXr;
using namespace FrameXr::Vis;

namespace {

std::string ActionFor(const std::vector<HelpRow>& rows, const std::string& control) {
    for (const auto& r : rows) {
        if (r.kind == HelpRow::Kind::Row && r.control == control) {
            return r.action;
        }
    }
    return "(missing)";
}

} // namespace

TEST_CASE("FrameXr vis: the help follows the profile and Face Buttons", "[frame_xr]") {
    HelpContext ctx;
    SECTION("default profile, by position: the Frame's A is the 3DS's B") {
        const HelpPage page = BuildHelp(DefaultHelpBindings(), ctx);
        REQUIRE(ActionFor(page.game, "A") == "3DS B");
        REQUIRE(ActionFor(page.game, "B") == "3DS A");
        REQUIRE(ActionFor(page.game, "X") == "3DS Y");
        REQUIRE(ActionFor(page.game, "Y") == "3DS X");
        REQUIRE(ActionFor(page.game, "D-Pad") == "3DS D-Pad");
        REQUIRE(ActionFor(page.game, "Left Stick") == "Circle Pad");
        REQUIRE(ActionFor(page.game, "Right Stick") == "C-Stick");
        REQUIRE(ActionFor(page.game, "Left Bumper") == "3DS L");
        REQUIRE(ActionFor(page.game, "Left Trigger") == "3DS ZL, Touch on Bottom Screen");
        REQUIRE(ActionFor(page.game, "Menu") == "3DS Start");
        REQUIRE(ActionFor(page.game, "View") == "3DS Select");
        REQUIRE(ActionFor(page.game, "Left Stick Click") == "Hide or Show Secondary Screen");
        REQUIRE(ActionFor(page.game, "Hold Left Stick Click (1 s)") == "Screen Layout");
        REQUIRE(ActionFor(page.game, "Right Stick Click") == "Swap Screens");
        REQUIRE(ActionFor(page.game, "Hold Right Stick Click (1 s)") == "Reset Screens and View");
        REQUIRE(ActionFor(page.game, "Hold View + Left Bumper (0.5 s)") == "Quick Load");
        REQUIRE(ActionFor(page.game, "Hold View + Right Bumper") == "Quick Save");
        REQUIRE(ActionFor(page.game, "Hold View + Right Trigger") == "Fast Forward On / Off");
        REQUIRE(ActionFor(page.window, "Both Bumpers + View + Menu") == "Switch VR / Flat");
        REQUIRE(page.footer.find("By Position (Nintendo)") != std::string::npos);
    }
    SECTION("by label: A is A") {
        ctx.face_by_position = false;
        const HelpPage page = BuildHelp(DefaultHelpBindings(), ctx);
        REQUIRE(ActionFor(page.game, "A") == "3DS A");
        REQUIRE(ActionFor(page.game, "Y") == "3DS Y");
    }
    SECTION("a changed profile shows what it binds") {
        HelpBindings b = DefaultHelpBindings();
        // The 3DS's L on the Frame's left grip as well as the bumper; nothing on D-pad up.
        b.buttons[Settings::NativeButton::L] = "engine:frame_xr,button:8"; // GripL
        b.buttons[Settings::NativeButton::Up] = "engine:sdl,button:3";
        b.profile_name = "Mine";
        const HelpPage page = BuildHelp(b, ctx);
        REQUIRE(ActionFor(page.game, "Left Bumper") == "Nothing");
        REQUIRE(ActionFor(page.game, "Grip on a Screen") == "3DS L, Move Screens");
        REQUIRE(ActionFor(page.game, "Grip (Immersive)") == "Move Immersive Screen");
        REQUIRE(ActionFor(page.game, "Hold Grip + D-Pad Up or Down") ==
                "Screen Curve or Wider View");
        REQUIRE(ActionFor(page.game, "D-Pad Up") == "Nothing");
        REQUIRE(ActionFor(page.game, "D-Pad Down") == "3DS D-Pad Down");
        REQUIRE(page.footer.find("Profile: Mine") != std::string::npos);
    }
    SECTION("the Now mark") {
        ctx.game_running = true;
        ctx.in_menu = true;
        const HelpPage page = BuildHelp(DefaultHelpBindings(), ctx);
        REQUIRE(page.game.front().kind == HelpRow::Kind::Heading);
        REQUIRE_FALSE(page.game.front().now);
        bool menu_now = false;
        for (const auto& r : page.window) {
            if (r.kind == HelpRow::Kind::Heading && r.control == "In-Game Menu") {
                menu_now = r.now;
            }
        }
        REQUIRE(menu_now);
    }
}

TEST_CASE("FrameXr vis: the help text is sized so no label is cut", "[frame_xr]") {
    // A stand-in for the font: every character is 0.58 em wide, bold ones 0.64.
    const TextMeasure measure = [](float size, bool bold, const std::string& text) {
        return static_cast<float>(text.size()) * size * (bold ? 0.64f : 0.58f);
    };
    const HelpColumnSpan game{44.0f, 640.0f}, window{684.0f, 1196.0f}; // as panels.cpp
    for (const bool in_menu : {false, true}) {
        HelpContext ctx;
        ctx.game_running = true;
        ctx.in_menu = in_menu;
        const HelpPage page = BuildHelp(DefaultHelpBindings(), ctx);
        const HelpTextLayout layout = LayoutHelpText(page, game, window, 20.0f, measure);
        REQUIRE(layout.fits);
        REQUIRE(layout.size >= kHelpMinTextSize);
        REQUIRE(layout.size <= 20.0f);
        // Every row fits between its column's edges, whatever the widest label is.
        const auto check = [&](const std::vector<HelpRow>& rows, const HelpColumnSpan& span,
                               float action_x) {
            for (const auto& r : rows) {
                if (r.kind != HelpRow::Kind::Row) {
                    continue;
                }
                REQUIRE(span.x0 + measure(layout.size, true, r.control) + kHelpColumnGap <=
                        action_x + 0.001f);
                REQUIRE(action_x + measure(layout.size, false, r.action) <= span.x1 + 0.001f);
            }
        };
        check(page.game, game, layout.game.action_x);
        check(page.window, window, layout.window.action_x);
    }
    SECTION("a narrow column shrinks the text until it fits") {
        HelpContext ctx;
        const HelpPage page = BuildHelp(DefaultHelpBindings(), ctx);
        const HelpTextLayout wide = LayoutHelpText(page, game, window, 20.0f, measure);
        const HelpTextLayout narrow =
            LayoutHelpText(page, {44.0f, 500.0f}, window, 20.0f, measure);
        REQUIRE(narrow.fits);
        REQUIRE(narrow.size < wide.size);
    }
    SECTION("a column that cannot fit says so, at the smallest size") {
        HelpContext ctx;
        const HelpPage page = BuildHelp(DefaultHelpBindings(), ctx);
        const HelpTextLayout none = LayoutHelpText(page, {0.0f, 50.0f}, window, 20.0f, measure);
        REQUIRE_FALSE(none.fits);
        REQUIRE(none.size == Approx(kHelpMinTextSize));
    }
}

TEST_CASE("FrameXr vis: beams", "[frame_xr]") {
    const Input::Posef head{}; // at the origin, looking down -Z
    SECTION("a beam ahead lies between its ends, along +Y, facing the head") {
        const auto b = PlaceBeam({0.2f, -0.3f, -0.4f}, {0.2f, -0.3f, -2.4f}, head, 0.01f);
        REQUIRE(b.has_value());
        REQUIRE(b->length == Approx(2.0f));
        REQUIRE(b->pose.position.z == Approx(-1.4f));
        const Input::Vec3f y = Input::Rotate(b->pose.orientation, {0, 1, 0});
        REQUIRE(std::abs(y.z) == Approx(1.0f).margin(1e-4));
        const Input::Vec3f z = Input::Rotate(b->pose.orientation, {0, 0, 1});
        const Input::Vec3f to_head = Input::Normalized(head.position - b->pose.position);
        REQUIRE(Input::Dot(z, to_head) > 0.0f);
    }
    SECTION("the part behind the head is cut, a beam all behind is none") {
        const auto cut = PlaceBeam({0, 0, 0.5f}, {0, 0, -1.0f}, head, 0.01f);
        REQUIRE(cut.has_value());
        REQUIRE(cut->length == Approx(1.0f - kBeamClipAhead).margin(1e-4));
        REQUIRE_FALSE(PlaceBeam({0, 0, 0.5f}, {0, 0, 0.2f}, head, 0.01f).has_value());
    }
    SECTION("levels") {
        REQUIRE(BeamLevel(1.0f) == 0);
        REQUIRE_FALSE(BeamLevel(0.0f).has_value());
        REQUIRE(*BeamLevel(0.1f) == kBeamLevels - 1);
        const auto art = BeamArt();
        REQUIRE(art.size() == std::size_t{kBeamLevels} * kBeamTexWidth * kBeamTexHeight * 4);
    }
}

TEST_CASE("FrameXr vis: a still controller fades, a lost one goes", "[frame_xr]") {
    HandActivity a;
    Input::HandInput hand;
    hand.active = true;
    int64_t t = 0;
    const int64_t step = 8'333'333;
    for (int i = 0; i < 60; i++, t += step) {
        a.Update(t, hand, false);
    }
    REQUIRE(a.Tracked());
    REQUIRE(a.Beam() == Approx(1.0f));
    for (; t < HandActivity::kBeamIdleNs + 1'000'000'000; t += step) {
        a.Update(t, hand, false);
    }
    REQUIRE(a.Beam() == Approx(0.0f));
    REQUIRE(a.Model() == Approx(1.0f));
    a.Update(t, hand, true); // used: back
    for (const int64_t end = t + 200'000'000; t < end;) {
        t += step; // frame by frame: Update clamps one step to 0.1 s
        a.Update(t, hand, false);
    }
    REQUIRE(a.Beam() == Approx(1.0f));
    hand.active = false;
    t += 100'000'000;
    a.Update(t, hand, false);
    REQUIRE(a.Tracked()); // a short loss keeps the pose
    t += HandActivity::kHoldNs;
    a.Update(t, hand, false);
    REQUIRE_FALSE(a.Tracked());
    REQUIRE(a.Model() == 0.0f);
}

TEST_CASE("FrameXr vis: model files", "[frame_xr]") {
    // A one-triangle model with a translating button, written as the converter writes it.
    const std::string path = "frame_xr_vis_test.model";
    {
        std::ofstream f(path, std::ios::binary);
        const uint32_t head[8] = {0, 2, 3, 3, 1, 1, 1, kModelFlagAimSpace};
        f.write("MXFM", 4);
        f.write(reinterpret_cast<const char*>(head + 1), 28);
        char name[32] = "button_a", input[32] = "/input/a/click";
        f.write(name, 32);
        f.write(input, 32);
        const uint32_t range[4] = {0, 3, 1, 0};
        f.write(reinterpret_cast<const char*>(range), 16);
        float motion[32] = {};
        motion[3] = 0.0f;
        motion[4] = -1.0f; // axis
        motion[5] = 0.0f;
        motion[6] = 0.0f;
        motion[7] = 0.002f; // 2 mm when pressed
        motion[8] = NAN;
        f.write(reinterpret_cast<const char*>(motion), sizeof(motion));
        const float v[36] = {};
        f.write(reinterpret_cast<const char*>(v), sizeof(v));
        const uint32_t idx[3] = {0, 1, 2};
        f.write(reinterpret_cast<const char*>(idx), sizeof(idx));
        const uint8_t tex[8] = {};
        f.write(reinterpret_cast<const char*>(tex), sizeof(tex));
    }
    ModelFile m;
    std::string error;
    REQUIRE(LoadModelFile(path, m, error));
    REQUIRE(m.components.size() == 1);
    REQUIRE(m.components[0].input == kInA);
    ModelInputs in{};
    in[kInA] = 1.0f;
    const auto parts = PoseModel(m, in);
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0].transform[13] == Approx(-0.002f));
    std::remove(path.c_str());
}

// ---- AzaharFrame ctrldiag: the controls diagram ----

namespace {

const DiagramLine* LineFor(const DiagramLabel* label, const std::string& key) {
    if (!label) {
        return nullptr;
    }
    for (const auto& l : label->lines) {
        if (l.key == key) {
            return &l;
        }
    }
    return nullptr;
}

std::string DiagramAction(const DiagramPage& page, int hand, CtrlPart part, const std::string& key) {
    const DiagramLine* l = LineFor(FindDiagramLabel(page, hand, part), key);
    return l ? l->action : "(missing)";
}

// Where the Steam Frame controllers' drawings put each part (panel pixels, measured from the
// on-device models): hand, part, the anchor, the bounds.
const struct {
    int hand;
    CtrlPart part;
    float x, y, x0, y0, x1, y1;
} kFrameAnchors[] = {
    {0, CtrlPart::Stick, 547, 431, 523, 406, 572, 455},
    {0, CtrlPart::Dpad, 473, 404, 441, 372, 506, 436},
    {0, CtrlPart::View, 518, 375, 505, 368, 532, 381},
    {0, CtrlPart::System, 497, 462, 485, 449, 509, 474},
    {0, CtrlPart::Bumper, 516, 190, 477, 167, 561, 218},
    {0, CtrlPart::Trigger, 523, 227, 482, 200, 560, 254},
    {0, CtrlPart::Grip, 532, 509, 522, 497, 542, 519},
    {1, CtrlPart::Stick, 693, 431, 668, 406, 717, 455},
    {1, CtrlPart::Menu, 722, 375, 708, 368, 735, 381},
    {1, CtrlPart::System, 743, 462, 731, 449, 755, 474},
    {1, CtrlPart::A, 769, 428, 756, 416, 781, 440},
    {1, CtrlPart::B, 790, 402, 777, 390, 802, 415},
    {1, CtrlPart::X, 743, 407, 731, 394, 755, 419},
    {1, CtrlPart::Y, 764, 381, 752, 369, 776, 393},
    {1, CtrlPart::Bumper, 724, 190, 679, 167, 763, 218},
    {1, CtrlPart::Trigger, 717, 227, 680, 200, 758, 254},
    {1, CtrlPart::Grip, 708, 509, 698, 497, 718, 519},
};

DiagramAnchors FrameAnchors() {
    DiagramAnchors an;
    for (const auto& a : kFrameAnchors) {
        const auto p = static_cast<std::size_t>(a.part);
        an.at[a.hand][p] = DiagramPoint{a.x, a.y};
        an.bounds[a.hand][p] = {a.x0, a.y0, a.x1, a.y1};
    }
    return an;
}

// Does the segment p-q pass through the rectangle (x0, y0, x1, y1)?
bool Crosses(const DiagramPoint& p, const DiagramPoint& q, float x0, float y0, float x1, float y1) {
    for (int i = 0; i <= 200; i++) {
        const float t = static_cast<float>(i) / 200.0f;
        const float x = p.x + (q.x - p.x) * t, y = p.y + (q.y - p.y) * t;
        if (x > x0 && x < x1 && y > y0 && y < y1) {
            return true;
        }
    }
    return false;
}

// One part of the box model below: a box with its own face normals.
void AddBox(ModelFile& m, const char* name, const char* path, float x0, float y0, float z0,
            float x1, float y1, float z1) {
    ModelComponent c;
    c.name = name;
    c.path = path;
    c.first_index = static_cast<uint32_t>(m.indices.size());
    const float n[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const auto& f : n) {
        const auto base = static_cast<uint32_t>(m.vertices.size() / kModelVertexFloats);
        // The face's four corners: the box corners on that side.
        std::vector<std::array<float, 3>> corners;
        for (const float x : {x0, x1}) {
            for (const float y : {y0, y1}) {
                for (const float z : {z0, z1}) {
                    const float c3[3] = {x, y, z};
                    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
                    bool on = false;
                    for (int k = 0; k < 3; k++) {
                        on = on || (f[k] > 0 && c3[k] == hi[k]) || (f[k] < 0 && c3[k] == lo[k]);
                    }
                    if (on) {
                        corners.push_back({x, y, z});
                    }
                }
            }
        }
        for (const auto& p : corners) {
            const float v[kModelVertexFloats] = {p[0], p[1], p[2], f[0], f[1], f[2], 1, 0, 0, 1, 0, 0};
            m.vertices.insert(m.vertices.end(), v, v + kModelVertexFloats);
        }
        // Corners 0..3 in the order above: 0-1-3 and 0-3-2 cover the face.
        for (const uint32_t i : {0u, 1u, 3u, 0u, 3u, 2u}) {
            m.indices.push_back(base + i);
        }
    }
    c.index_count = static_cast<uint32_t>(m.indices.size()) - c.first_index;
    c.input = -1;
    m.components.push_back(c);
}

} // namespace

TEST_CASE("FrameXr vis: the diagram follows the profile and Face Buttons", "[frame_xr]") {
    HelpContext ctx;
    ctx.game_running = true;
    SECTION("default profile, by position") {
        const HelpDiagram d = BuildHelpDiagram(DefaultHelpBindings(), ctx);
        REQUIRE(d.now == DiagramPageId::Game);
        const DiagramPage& g = d.pages[0];
        REQUIRE(g.name == "In Game");
        REQUIRE(d.pages[1].name == "Azahar's Window");
        REQUIRE(DiagramAction(g, 1, CtrlPart::A, "A") == "3DS B");
        REQUIRE(DiagramAction(g, 1, CtrlPart::Y, "Y") == "3DS X");
        REQUIRE(DiagramAction(g, 0, CtrlPart::Dpad, "D-Pad") == "3DS D-Pad");
        REQUIRE(DiagramAction(g, 0, CtrlPart::Stick, "Left Stick") == "Circle Pad");
        REQUIRE(DiagramAction(g, 0, CtrlPart::Stick, "Click") == "Hide or Show Secondary Screen");
        REQUIRE(DiagramAction(g, 1, CtrlPart::Stick, "Right Stick") == "C-Stick");
        REQUIRE(DiagramAction(g, 1, CtrlPart::Stick, "Click") == "Swap Screens");
        REQUIRE(DiagramAction(g, 1, CtrlPart::Stick, "Hold Click (1 s)") == "Reset Screens and View");
        REQUIRE(DiagramAction(g, 0, CtrlPart::Bumper, "Left Bumper") == "3DS L");
        REQUIRE(DiagramAction(g, 1, CtrlPart::Trigger, "Right Trigger") == "3DS ZR");
        REQUIRE(DiagramAction(g, 1, CtrlPart::Trigger, "") == "Touch on Bottom Screen");
        REQUIRE(DiagramAction(g, 1, CtrlPart::Menu, "Menu") == "3DS Start");
        REQUIRE(DiagramAction(g, 1, CtrlPart::Menu, "Hold (1 s)") == "In-Game Menu");
        // The holds and the chords, under View.
        REQUIRE(DiagramAction(g, 0, CtrlPart::View, "View") == "3DS Select");
        REQUIRE(DiagramAction(g, 0, CtrlPart::View, "Hold (1.5 s)") == "Stop Game");
        const DiagramLabel* view = FindDiagramLabel(g, 0, CtrlPart::View);
        REQUIRE(LineFor(view, "Hold View +"));
        REQUIRE(LineFor(view, "Hold View +")->heading);
        REQUIRE(DiagramAction(g, 0, CtrlPart::View, "Hold Left Bumper (0.5 s)") == "Quick Load");
        REQUIRE(DiagramAction(g, 0, CtrlPart::View, "Right Bumper") == "Quick Save");
        REQUIRE(DiagramAction(g, 0, CtrlPart::View, "Right Trigger") == "Fast Forward On / Off");
        REQUIRE(LineFor(view, "Both Bumpers +"));
        REQUIRE(std::any_of(view->lines.begin(), view->lines.end(), [](const DiagramLine& l) {
            return l.sub && l.key == "View" && l.action == "Show or Hide Bottom Screen";
        }));
        REQUIRE(DiagramAction(g, 0, CtrlPart::View, "View + Menu") == "Switch VR / Flat");
        // One label for both grips.
        REQUIRE(FindDiagramLabel(g, 0, CtrlPart::Grip) == FindDiagramLabel(g, 1, CtrlPart::Grip));
        REQUIRE(DiagramAction(g, 0, CtrlPart::Grip, "Grip on a Screen") == "Move Screens");
        REQUIRE(DiagramAction(g, 0, CtrlPart::Grip, "D-Pad Up or Down") == "Curve or Wider View");
        // The window's page.
        const DiagramPage& w = d.pages[1];
        REQUIRE(DiagramAction(w, 1, CtrlPart::A, "A") == "Click");
        REQUIRE(DiagramAction(w, 1, CtrlPart::B, "B") == "Back");
        REQUIRE(DiagramAction(w, 1, CtrlPart::Y, "Y") == "Keyboard");
        REQUIRE(DiagramAction(w, 1, CtrlPart::Stick, "Hold Click (1 s)") == "Bring Window Back");
        REQUIRE(DiagramAction(w, 1, CtrlPart::Menu, "Hold Menu (1 s)") == "Back to Game");
        REQUIRE(DiagramAction(w, 0, CtrlPart::View, "View + Menu") == "Switch VR / Flat");
        REQUIRE(DiagramAction(w, 0, CtrlPart::Grip, "Grip") == "Move Window");
        // AzaharFrame gamelist
        REQUIRE(DiagramAction(w, 0, CtrlPart::Dpad, "D-Pad Up or Down") == "Choose Game");
        REQUIRE(DiagramAction(w, 1, CtrlPart::A, "Twice") == "Start Game");
    }
    SECTION("by label, and the window's page with no game") {
        ctx.face_by_position = false;
        ctx.game_running = false;
        const HelpDiagram d = BuildHelpDiagram(DefaultHelpBindings(), ctx);
        REQUIRE(d.now == DiagramPageId::Window);
        REQUIRE(DiagramAction(d.pages[0], 1, CtrlPart::A, "A") == "3DS A");
        REQUIRE(DiagramAction(d.pages[0], 1, CtrlPart::X, "X") == "3DS X");
        REQUIRE(d.footer.find("By Label (Xbox)") != std::string::npos);
    }
    SECTION("a changed profile shows what it binds, as the list does") {
        HelpBindings b = DefaultHelpBindings();
        b.buttons[Settings::NativeButton::L] = "engine:frame_xr,button:8"; // GripL
        b.buttons[Settings::NativeButton::Up] = "engine:sdl,button:3";
        b.buttons[Settings::NativeButton::Start] = "";
        b.profile_name = "Mine";
        const HelpDiagram d = BuildHelpDiagram(b, ctx);
        const HelpPage list = BuildHelp(b, ctx);
        const DiagramPage& g = d.pages[0];
        REQUIRE(DiagramAction(g, 0, CtrlPart::Bumper, "Left Bumper") == "Nothing");
        REQUIRE(DiagramAction(g, 1, CtrlPart::Menu, "Menu") == "Nothing");
        REQUIRE(DiagramAction(g, 0, CtrlPart::Grip, "Grip on a Screen") ==
                ActionFor(list.game, "Grip on a Screen"));
        // A direction changed: the 3DS's D-pad, and that direction.
        REQUIRE(DiagramAction(g, 0, CtrlPart::Dpad, "D-Pad") == "3DS D-Pad");
        REQUIRE(DiagramAction(g, 0, CtrlPart::Dpad, "Up") == "Nothing");
        REQUIRE(DiagramAction(g, 0, CtrlPart::Dpad, "Down") == "(missing)");
        REQUIRE(FindDiagramLabel(g, 0, CtrlPart::Dpad)->lines.size() == 2);
        REQUIRE(d.footer == list.footer);
        REQUIRE(d.footer.find("Profile: Mine") != std::string::npos);
    }
}

TEST_CASE("FrameXr vis: the diagram's labels fit and their lines are clear", "[frame_xr]") {
    // A stand-in for Noto Sans, a little wide: every character 0.48 em, bold ones 0.51.
    const TextMeasure measure = [](float size, bool bold, const std::string& text) {
        return static_cast<float>(text.size()) * size * (bold ? 0.51f : 0.48f);
    };
    const DiagramAnchors an = FrameAnchors();
    HelpBindings one = DefaultHelpBindings();
    one.buttons[Settings::NativeButton::Up] = "engine:sdl,button:3";
    HelpBindings all = one; // a five-line D-pad
    all.buttons[Settings::NativeButton::Down] = "";
    all.buttons[Settings::NativeButton::Left] = "engine:frame_xr,button:1";
    all.buttons[Settings::NativeButton::Right] = "engine:frame_xr,button:0";
    for (const HelpBindings& b : {DefaultHelpBindings(), one, all}) {
        HelpContext ctx;
        ctx.game_running = true;
        const HelpDiagram d = BuildHelpDiagram(b, ctx);
        for (const DiagramPage& page : d.pages) {
            const auto placed = LayoutHelpDiagram(page, an, measure);
            INFO(page.name);
            REQUIRE(placed.size() == page.labels.size());
            for (std::size_t i = 0; i < placed.size(); i++) {
                const DiagramPlaced& p = placed[i];
                INFO(p.label->lines.front().key);
                // Inside the panel, under the title row, over the footer.
                REQUIRE(p.x0 >= 30.0f);
                REQUIRE(p.x1 <= kDiagramPanelW - 30.0f);
                REQUIRE(p.y0 >= kDiagramTop - 0.01f);
                REQUIRE(p.y1 <= kDiagramBottom + 0.01f);
                REQUIRE_FALSE(p.leaders.empty());
                for (std::size_t j = 0; j < placed.size(); j++) {
                    if (i == j) {
                        continue;
                    }
                    const DiagramPlaced& q = placed[j];
                    // No two labels overlap, and no line runs through another label.
                    REQUIRE((p.x1 <= q.x0 || q.x1 <= p.x0 || p.y1 <= q.y0 || q.y1 <= p.y0));
                    for (const DiagramLeader& l : p.leaders) {
                        REQUIRE_FALSE(Crosses(l.from, l.to, q.x0, q.y0, q.x1, q.y1));
                    }
                }
                // No line runs over a control it does not point at (a little inside its edge).
                for (const DiagramLeader& l : p.leaders) {
                    for (const auto& a : kFrameAnchors) {
                        const auto& parts = p.label->parts;
                        if (std::find(parts.begin(), parts.end(), a.part) != parts.end() ||
                            DiagramPartInFront(a.part)) {
                            continue;
                        }
                        INFO("crosses part " << static_cast<int>(a.part) << " of hand " << a.hand);
                        REQUIRE_FALSE(Crosses(l.from, l.to, a.x0 + 3, a.y0 + 3, a.x1 - 3, a.y1 - 3));
                    }
                }
            }
        }
    }
    SECTION("a label whose parts do not show is left out") {
        DiagramAnchors none;
        const HelpDiagram d = BuildHelpDiagram(DefaultHelpBindings(), {});
        REQUIRE(LayoutHelpDiagram(d.pages[0], none, measure).empty());
    }
}

TEST_CASE("FrameXr vis: controller drawings", "[frame_xr]") {
    // A box for a body, a button on top, a trigger underneath at the front.
    ModelFile m;
    AddBox(m, "body", "", -0.03f, -0.02f, 0.0f, 0.03f, 0.0f, 0.1f);
    AddBox(m, "button_a", "/input/a/click", 0.01f, 0.0f, 0.02f, 0.02f, 0.004f, 0.03f);
    AddBox(m, "trigger", "/input/trigger", -0.01f, -0.035f, 0.0f, 0.01f, -0.02f, 0.02f);
    REQUIRE(PartOfComponent(m.components[1]) == CtrlPart::A);
    REQUIRE(PartOfComponent(m.components[2]) == CtrlPart::Trigger);
    REQUIRE_FALSE(PartOfComponent(m.components[0]));

    const DiagramAngle top{0.0f, 90.0f};
    const float scale = DiagramScale({&m}, top, 120, 200);
    REQUIRE(scale > 0.0f);
    const DiagramImage img = DrawDiagram(m, top, scale, 120, 200);
    REQUIRE(img.rgba.size() == 120u * 200u * 4u);
    // Seen from above: the button shows right of the middle, nearer the front (the top); the
    // trigger is under the body.
    const auto& a = img.anchor[static_cast<std::size_t>(CtrlPart::A)];
    REQUIRE(a);
    REQUIRE(a->x > 60.0f);
    REQUIRE(a->y < 100.0f);
    REQUIRE_FALSE(img.anchor[static_cast<std::size_t>(CtrlPart::Trigger)]);
    // Outlines are drawn: some pixels are the line colour, the middle of the body is fill.
    int line = 0;
    for (std::size_t i = 0; i < img.rgba.size(); i += 4) {
        line += img.rgba[i] > 0xD0 && img.rgba[i + 3] > 0xF0;
    }
    REQUIRE(line > 100);
    const uint8_t* mid = &img.rgba[(150u * 120u + 30u) * 4u];
    REQUIRE(mid[3] == 0xFF);
    REQUIRE(mid[0] < 0x60);

    // From the front, the trigger shows; cut to its rows, its anchor moves with them.
    const DiagramImage front = DrawDiagram(m, {180.0f, 165.0f}, scale, 120, 200);
    const auto& t = front.anchor[static_cast<std::size_t>(CtrlPart::Trigger)];
    REQUIRE(t);
    const DiagramImage cut = CropDiagram(front, 20, 180, 10);
    REQUIRE(cut.height == 160);
    REQUIRE(cut.anchor[static_cast<std::size_t>(CtrlPart::Trigger)]);
    REQUIRE(cut.anchor[static_cast<std::size_t>(CtrlPart::Trigger)]->y == Approx(t->y - 20.0f));
}

// ---- AzaharFrame statspos -----------------------------------------------------------------------

namespace {

Input::PanelQuad ScreenAt(float z, float w, float h, float y = 0.0f) {
    return {Input::Posef{{}, {0.0f, y, z}}, w, h, true};
}

} // namespace

TEST_CASE("FrameXr vis: the stats in front never go behind a screen", "[frame_xr]") {
    const Input::Vec3f eye{0.0f, 1.6f, 0.0f};
    const Input::Vec3f ahead{0.0f, 0.0f, -1.0f};
    const float w = kStatsFrontWidth, h = kStatsFrontWidth * 320.0f / 512.0f;
    SECTION("nothing near: 1 m") {
        REQUIRE(StatsClearDistance(eye, ahead, kStatsFrontDistance, w, h, {}) ==
                Approx(kStatsFrontDistance));
        // A screen 2 m away is farther than the stats: no change.
        REQUIRE(StatsClearDistance(eye, ahead, kStatsFrontDistance, w, h,
                                   {ScreenAt(-2.0f, 1.6f, 1.0f, 1.6f)}) ==
                Approx(kStatsFrontDistance));
    }
    SECTION("a screen nearer than 1 m: just in front of it") {
        const float d = StatsClearDistance(eye, ahead, kStatsFrontDistance, w, h,
                                           {ScreenAt(-0.8f, 1.6f, 1.0f, 1.6f)});
        REQUIRE(d == Approx(0.8f - kStatsScreenGap).margin(1e-4));
    }
    SECTION("the nearest of two, a hidden one does not count") {
        Input::PanelQuad hidden = ScreenAt(-0.5f, 1.6f, 1.0f, 1.6f);
        hidden.visible = false;
        const float d = StatsClearDistance(
            eye, ahead, kStatsFrontDistance, w, h,
            {ScreenAt(-0.9f, 1.6f, 1.0f, 1.6f), ScreenAt(-0.7f, 1.0f, 1.0f, 1.6f), hidden});
        REQUIRE(d == Approx(0.7f - kStatsScreenGap).margin(1e-4));
    }
    SECTION("a screen only under an edge of the panel counts") {
        // Its left edge 1 cm right of the panel's middle: rays through the right side hit it.
        Input::PanelQuad side = ScreenAt(-0.8f, 1.0f, 1.0f, 1.6f);
        side.pose.position.x = 0.51f;
        REQUIRE(StatsClearDistance(eye, ahead, kStatsFrontDistance, w, h, {side}) <
                kStatsFrontDistance);
        side.pose.position.x = 0.8f; // well clear of it
        REQUIRE(StatsClearDistance(eye, ahead, kStatsFrontDistance, w, h, {side}) ==
                Approx(kStatsFrontDistance));
    }
    SECTION("never nearer than 0.4 m, never farther than 3 m") {
        REQUIRE(StatsClearDistance(eye, ahead, kStatsFrontDistance, w, h,
                                   {ScreenAt(-0.3f, 1.6f, 1.0f, 1.6f)}) ==
                Approx(kStatsMinDistance));
        REQUIRE(StatsClearDistance(eye, ahead, 5.0f, w, h, {}) == Approx(kStatsMaxDistance));
    }
}

TEST_CASE("FrameXr vis: the stats are placed ahead and a little below, facing you",
          "[frame_xr]") {
    Input::Posef head{{}, {0.0f, 1.6f, 0.0f}};
    SECTION("looking ahead") {
        const Input::Vec3f dir = StatsFrontDirection(head);
        REQUIRE(YawOf(dir) == Approx(0.0f).margin(1e-5));
        REQUIRE(PitchOf(dir) * 180.0f / 3.14159265f == Approx(-kStatsFrontBelowDeg).margin(1e-3));
        const Input::Posef p = StatsPoseAt(head.position, dir, 1.0f);
        REQUIRE(p.position.z == Approx(-std::cos(kStatsFrontBelowDeg * 3.14159265f / 180.0f)));
        // Facing the eyes about the vertical only: its front is +Z, level.
        const Input::Vec3f front = Input::Rotate(p.orientation, {0, 0, 1});
        REQUIRE(front.z == Approx(1.0f).margin(1e-5));
        REQUIRE(front.y == Approx(0.0f).margin(1e-5));
    }
    SECTION("turned 90 degrees to the left and looking down a lot") {
        const float a = 3.14159265f * 0.25f; // half of 90 degrees
        head.orientation = {0.0f, std::sin(a), 0.0f, std::cos(a)};
        const float down = -60.0f * 3.14159265f / 180.0f;
        head.orientation = Input::Multiply(head.orientation,
                                           {std::sin(down * 0.5f), 0.0f, 0.0f, std::cos(down * 0.5f)});
        const Input::Vec3f dir = StatsFrontDirection(head);
        REQUIRE(dir.x < -0.5f); // to the left
        REQUIRE(PitchOf(dir) * 180.0f / 3.14159265f ==
                Approx(-kStatsFrontMaxPitchDeg - kStatsFrontBelowDeg).margin(1e-2));
        const Input::Posef p = StatsPoseAt(head.position, dir, 1.0f);
        const Input::Vec3f front = Input::Rotate(p.orientation, {0, 0, 1});
        REQUIRE(front.x == Approx(1.0f).margin(1e-4)); // back toward the head
    }
}

TEST_CASE("FrameXr vis: a grip moves the stats round you at their distance", "[frame_xr]") {
    const Input::Vec3f eye{0.0f, 1.6f, 0.0f};
    const Input::Posef panel = StatsPoseAt(eye, {0.0f, 0.0f, -1.0f}, 1.0f);
    StatsGrab grab;
    // The beam from a hand below the eyes onto the panel's right part (on the sphere round the
    // eyes it is on).
    const Input::Vec3f hand{0.2f, 1.2f, -0.2f};
    const Input::Vec3f hit{0.0995037f, 1.6f, -0.9950372f};
    grab.Begin(eye, panel, hit);
    const Input::Ray still{hand, Input::Normalized(hit - hand)};
    SECTION("held still, it stays") {
        const Input::Vec3f d = grab.Direction(eye, still, 1.0f);
        REQUIRE(d.x == Approx(0.0f).margin(1e-3));
        REQUIRE(d.y == Approx(0.0f).margin(1e-3));
    }
    SECTION("the beam to the left moves it left, at the same distance") {
        const Input::Vec3f left_hit{-0.4f, 1.6f, -0.9165f};
        const Input::Ray ray{hand, Input::Normalized(left_hit - hand)};
        const Input::Vec3f d = grab.Direction(eye, ray, 1.0f);
        REQUIRE(d.x < -0.3f);
        REQUIRE(Input::Length(d) == Approx(1.0f));
    }
}

TEST_CASE("FrameXr vis: the wrist stats face the eyes only when turned to them", "[frame_xr]") {
    const Input::Posef local = WristLocalPose();
    // Past the handle's end, inward of it (the aim space: +Z back, +Y up, +X the inner side).
    REQUIRE(local.position.z > 0.117f);
    REQUIRE(local.position.x > 0.015f);
    const Input::Vec3f front = Input::Rotate(local.orientation, {0, 0, 1});
    REQUIRE(front.x > 0.95f); // facing inward, a little up
    REQUIRE(front.y > 0.0f);
    const Input::Vec3f along = Input::Rotate(local.orientation, {1, 0, 0});
    REQUIRE(along.z < -0.7f); // the text reads toward the hand
    // Held to play (pointing ahead, 0.4 m under the eyes, 0.25 m left), it faces away.
    const Input::Posef play{{}, {-0.25f, -0.4f, -0.35f}};
    REQUIRE_FALSE(WristShown(WristFacingDeg(WristStatsPose(play), {0, 0, 0}), false));
    // A panel and an eye straight in front of it, then off to the side.
    const Input::Posef p{{}, {0.0f, 1.2f, -0.4f}};
    REQUIRE(WristFacingDeg(p, {0.0f, 1.2f, 0.0f}) == Approx(0.0f).margin(1e-3));
    REQUIRE(WristFacingDeg(p, {0.4f, 1.2f, -0.4f + 0.4f}) == Approx(45.0f).margin(1e-2));
    // Shown under 35 degrees, kept until past 40.
    REQUIRE(WristShown(34.0f, false));
    REQUIRE_FALSE(WristShown(37.0f, false));
    REQUIRE(WristShown(37.0f, true));
    REQUIRE_FALSE(WristShown(41.0f, true));
}
