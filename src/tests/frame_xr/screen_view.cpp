// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/input/screen_view.h"

// AzaharFrame screenswap
using Catch::Approx;
using namespace FrameXr::Input;

namespace {

// The normal placement: the top screen (5:3) is the main slot, 2 m ahead and 1 m high; the
// bottom screen (4:3) is the secondary slot, lower, nearer, tilted back, 0.5625 m high.
const PanelQuad kMain{Posef{Quatf{}, Vec3f{0.0f, 0.0f, -2.0f}}, 5.0f / 3.0f, 1.0f, true};
const PanelQuad kSecondary{Posef{Quatf{0.38268f, 0.0f, 0.0f, 0.92388f}, Vec3f{0.0f, -0.75f, -1.5f}},
                           0.75f, 0.5625f, true};

} // namespace

TEST_CASE("FrameXr screen view: the top screen starts in focus, nothing hidden", "[frame_xr]") {
    const ScreenView view;
    REQUIRE_FALSE(view.bottom_focus());
    REQUIRE_FALSE(view.top_hidden());
    REQUIRE(view.BottomShown(false)); // a normal game
    REQUIRE_FALSE(view.BottomShown(true)); // a game that hides it: Immersive starts hidden
    const ScreenQuads q = view.Layout(kMain, kSecondary, false);
    REQUIRE(q.top.pose.position.z == Approx(-2.0f));
    REQUIRE(q.top.width_m == Approx(5.0f / 3.0f));
    REQUIRE(q.bottom.pose.position.y == Approx(-0.75f));
    REQUIRE(q.bottom.width_m == Approx(0.75f));
    REQUIRE(q.top.visible);
    REQUIRE(q.bottom.visible);
}

TEST_CASE("FrameXr screen view: a swap trades the slots and swaps back", "[frame_xr]") {
    ScreenView view;
    auto effect = view.Swap(false);
    REQUIRE_FALSE(effect.toggle_immersive_bottom);
    REQUIRE(view.bottom_focus());

    const ScreenQuads q = view.Layout(kMain, kSecondary, false);
    // The bottom screen has the main slot's place and height, at its own 4:3.
    REQUIRE(q.bottom.pose.position.z == Approx(-2.0f));
    REQUIRE(q.bottom.pose.position.y == Approx(0.0f));
    REQUIRE(q.bottom.height_m == Approx(1.0f));
    REQUIRE(q.bottom.width_m == Approx(4.0f / 3.0f));
    // The top screen has the secondary slot's place, tilt and height, at its own 5:3.
    REQUIRE(q.top.pose.position.y == Approx(-0.75f));
    REQUIRE(q.top.pose.position.z == Approx(-1.5f));
    REQUIRE(q.top.pose.orientation.x == Approx(0.38268f));
    REQUIRE(q.top.height_m == Approx(0.5625f));
    REQUIRE(q.top.width_m == Approx(0.5625f * 5.0f / 3.0f));
    REQUIRE(q.top.visible);
    REQUIRE(q.bottom.visible);

    effect = view.Swap(false);
    REQUIRE_FALSE(effect.toggle_immersive_bottom);
    REQUIRE_FALSE(view.bottom_focus());
    const ScreenQuads back = view.Layout(kMain, kSecondary, false);
    REQUIRE(back.top.width_m == Approx(5.0f / 3.0f));
    REQUIRE(back.bottom.width_m == Approx(0.75f));
}

TEST_CASE("FrameXr screen view: grips move the slots, so both focuses follow", "[frame_xr]") {
    ScreenView view;
    view.Swap(false);
    PanelQuad main = kMain;
    main.pose.position = Vec3f{0.0f, 0.3f, -3.0f}; // dragged up and back
    const ScreenQuads q = view.Layout(main, kSecondary, false);
    REQUIRE(q.bottom.pose.position.z == Approx(-3.0f));
    REQUIRE(q.bottom.pose.position.y == Approx(0.3f));
}

TEST_CASE("FrameXr screen view: left stick click hides the screen that is not in focus",
          "[frame_xr]") {
    ScreenView view;
    SECTION("top in focus: the bottom screen, in a normal game") {
        REQUIRE_FALSE(view.ToggleOther(false).toggle_immersive_bottom);
        REQUIRE_FALSE(view.BottomShown(false));
        REQUIRE_FALSE(view.Layout(kMain, kSecondary, false).bottom.visible);
        REQUIRE(view.Layout(kMain, kSecondary, false).top.visible);
        view.ToggleOther(false);
        REQUIRE(view.BottomShown(false));
        REQUIRE(view.Layout(kMain, kSecondary, false).bottom.visible);
    }
    SECTION("bottom in focus: the top screen, and it comes back with the focus") {
        view.Swap(false);
        view.ToggleOther(false);
        REQUIRE(view.top_hidden());
        ScreenQuads q = view.Layout(kMain, kSecondary, false);
        REQUIRE_FALSE(q.top.visible);
        REQUIRE(q.bottom.visible); // the screen in focus is always shown
        view.ToggleOther(false);
        REQUIRE_FALSE(view.top_hidden());
        REQUIRE(view.Layout(kMain, kSecondary, false).top.visible);
        // Hidden, then swapped back: the top screen is shown again as the main one.
        view.ToggleOther(false);
        view.Swap(false);
        REQUIRE_FALSE(view.top_hidden());
        REQUIRE(view.Layout(kMain, kSecondary, false).top.visible);
    }
    SECTION("the top screen stays shown when the bottom screen takes the focus again") {
        view.Swap(false);
        view.ToggleOther(false);
        view.Swap(false);
        view.Swap(false);
        REQUIRE_FALSE(view.top_hidden());
    }
}

TEST_CASE("FrameXr screen view: a hidden bottom screen is shown by taking focus and hidden "
          "again when it gives focus back",
          "[frame_xr]") {
    ScreenView view;
    SECTION("a normal game, bottom hidden by the left stick click") {
        view.ToggleOther(false);
        REQUIRE_FALSE(view.BottomShown(false));
        view.Swap(false);
        REQUIRE(view.BottomShown(false));
        REQUIRE(view.Layout(kMain, kSecondary, false).bottom.visible);
        view.Swap(false);
        REQUIRE_FALSE(view.BottomShown(false));
        REQUIRE_FALSE(view.Layout(kMain, kSecondary, false).bottom.visible);
    }
    SECTION("a game that hides it (Hide Bottom Screen), through Immersive") {
        // Hidden at the start; taking focus shows it through Immersive::ToggleBottom...
        REQUIRE_FALSE(view.BottomShown(true));
        auto effect = view.Swap(true);
        REQUIRE(effect.toggle_immersive_bottom);
        REQUIRE(view.BottomShown(true));
        // ...and handing the focus back hides it again.
        effect = view.Swap(true);
        REQUIRE(effect.toggle_immersive_bottom);
        REQUIRE_FALSE(view.BottomShown(true));
    }
    SECTION("a game that hides it, bottom shown with the chord first: swapping leaves it shown") {
        REQUIRE(view.ToggleBottom(true).toggle_immersive_bottom);
        REQUIRE(view.BottomShown(true));
        REQUIRE_FALSE(view.Swap(true).toggle_immersive_bottom);
        REQUIRE_FALSE(view.Swap(true).toggle_immersive_bottom);
        REQUIRE(view.BottomShown(true));
    }
}

TEST_CASE("FrameXr screen view: in a managed game the left stick click and the chord are one "
          "toggle",
          "[frame_xr]") {
    ScreenView view;
    // Hidden at the start (Hide Bottom Screen or an immersive mode).
    REQUIRE(view.ToggleOther(true).toggle_immersive_bottom); // shows
    REQUIRE(view.BottomShown(true));
    REQUIRE(view.ToggleBottom(true).toggle_immersive_bottom); // the chord hides
    REQUIRE_FALSE(view.BottomShown(true));
    REQUIRE(view.ToggleBottom(true).toggle_immersive_bottom); // the chord shows
    REQUIRE(view.ToggleOther(true).toggle_immersive_bottom);  // the click hides
    REQUIRE_FALSE(view.BottomShown(true));
    // No hidden flag of the unmanaged kind is touched: the layout leaves it to Immersive.
    REQUIRE(view.Layout(kMain, kSecondary, true).bottom.visible);
}

TEST_CASE("FrameXr screen view: the chord with the bottom screen in focus hides it and returns "
          "the focus to the top screen",
          "[frame_xr]") {
    ScreenView view;
    view.Swap(true); // peeked: Immersive shows it
    REQUIRE(view.BottomShown(true));
    const auto effect = view.ToggleBottom(true);
    REQUIRE(effect.toggle_immersive_bottom);
    REQUIRE_FALSE(view.bottom_focus());
    REQUIRE_FALSE(view.BottomShown(true));
    // The peek is over: a later swap peeks again, and ends hidden again.
    REQUIRE(view.Swap(true).toggle_immersive_bottom);
    REQUIRE(view.Swap(true).toggle_immersive_bottom);
    REQUIRE_FALSE(view.BottomShown(true));
}

TEST_CASE("FrameXr screen view: a new game starts with the top screen in focus", "[frame_xr]") {
    ScreenView view;
    view.Swap(false);
    view.ToggleOther(false);
    view.Reset();
    REQUIRE_FALSE(view.bottom_focus());
    REQUIRE_FALSE(view.top_hidden());
    REQUIRE(view.BottomShown(false));
    REQUIRE_FALSE(view.BottomShown(true));
}

TEST_CASE("FrameXr screen view: immersive's hiding of a slot carries over", "[frame_xr]") {
    ScreenView view;
    PanelQuad hidden_secondary = kSecondary;
    hidden_secondary.visible = false;
    REQUIRE_FALSE(view.Layout(kMain, hidden_secondary, true).bottom.visible);
    view.Swap(false);
    // In focus the secondary slot holds the top screen.
    REQUIRE_FALSE(view.Layout(kMain, hidden_secondary, false).top.visible);
    PanelQuad hidden_main = kMain;
    hidden_main.visible = false;
    REQUIRE_FALSE(view.Layout(hidden_main, kSecondary, false).bottom.visible);
}

TEST_CASE("FrameXr screen view: a drag moves the main slot whichever screen was grabbed",
          "[frame_xr]") {
    ScreenView view;
    SECTION("top in focus: the mapper's target is the main slot's") {
        const ScreenQuads q = view.Layout(kMain, kSecondary, false);
        const Vec3f target{0.0f, 0.5f, -2.5f};
        const Vec3f out = view.MainDragTarget(target, q.top, kMain);
        REQUIRE(out.x == Approx(target.x));
        REQUIRE(out.y == Approx(target.y));
        REQUIRE(out.z == Approx(target.z));
    }
    SECTION("bottom in focus: the mapper's top is in the small slot, the move is the same") {
        view.Swap(false);
        const ScreenQuads q = view.Layout(kMain, kSecondary, false);
        // The mapper wants "its" top screen (the small slot) 0.2 m higher and 0.3 m further.
        const Vec3f wanted = q.top.pose.position + Vec3f{0.0f, 0.2f, -0.3f};
        const Vec3f out = view.MainDragTarget(wanted, q.top, kMain);
        REQUIRE(out.x == Approx(kMain.pose.position.x));
        REQUIRE(out.y == Approx(kMain.pose.position.y + 0.2f));
        REQUIRE(out.z == Approx(kMain.pose.position.z - 0.3f));
        // No movement asked: the main slot stays.
        const Vec3f still = view.MainDragTarget(q.top.pose.position, q.top, kMain);
        REQUIRE(still.y == Approx(kMain.pose.position.y));
        REQUIRE(still.z == Approx(kMain.pose.position.z));
    }
}

TEST_CASE("FrameXr screen view: which screen is in the main slot (AzaharFrame move)",
          "[frame_xr]") {
    ScreenView view;
    REQUIRE(view.InMainSlot(true));
    REQUIRE_FALSE(view.InMainSlot(false));
    view.Swap(false);
    REQUIRE_FALSE(view.InMainSlot(true));
    REQUIRE(view.InMainSlot(false));
}
