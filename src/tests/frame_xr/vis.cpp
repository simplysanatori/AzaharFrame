// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/vis/beam.h"
#include "frame_xr/vis/help_content.h"
#include "frame_xr/vis/help_layout.h"
#include "frame_xr/vis/model_file.h"

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
        REQUIRE(ActionFor(page.game, "Left Stick Click") == "Hide or Show Other Screen");
        REQUIRE(ActionFor(page.game, "Hold Left Stick Click (1 s)") == "3DS Home");
        REQUIRE(ActionFor(page.game, "Right Stick Click") == "Swap Screens");
        REQUIRE(ActionFor(page.game, "Hold Right Stick Click (1 s)") == "Reset Screens");
        REQUIRE(ActionFor(page.game, "Hold View + Left Bumper") == "Quick Load");
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
