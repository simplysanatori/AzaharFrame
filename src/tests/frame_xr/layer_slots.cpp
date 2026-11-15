// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash: fixed layer positions for the quads after the screens
// (frame_xr/layer_slots.h): a quad that comes and goes never moves another quad to another
// position (SteamVR's overlay of that index), absent quads are collapsed in their own slot, new
// ones go where the producers' order puts them, and the runtime's limit drops the oldest absent.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/layer_slots.h"
#include "frame_xr/retired_swapchains.h" // AzaharFrame projdef

namespace {

namespace LS = FrameXr::LayerSlots;
using G = LS::Group;

struct Q {
    std::string name;
    float size = 1.0f;
};

std::string Names(const std::vector<LS::Map<Q>::Out>& out) {
    std::string s;
    for (const auto& o : out) {
        s += (s.empty() ? "" : " ") + o.quad.name + (o.present ? "" : "_");
    }
    return s;
}

std::vector<LS::Map<Q>::Out> Frame(LS::Map<Q>& m, uint32_t context,
                                   const std::vector<std::pair<G, std::string>>& quads,
                                   std::size_t max = 32) {
    m.Begin(context);
    for (const auto& [g, name] : quads) {
        // The swapchain is the name's first letter, the rect's size its length: "t1" and "t2"
        // share a swapchain and a size (two of a kind), "beam" and "dot" do not.
        m.Add(g, static_cast<uint64_t>(name[0]), static_cast<int32_t>(name.size()), 1, Q{name});
    }
    std::vector<LS::Map<Q>::Out> out;
    m.End(max, out, [](Q& q) { q.size *= LS::kCollapse; });
    return out;
}

} // namespace

TEST_CASE("Layer slots: a quad that goes keeps its place, collapsed", "[frame_xr][flash]") {
    LS::Map<Q> m;
    CHECK(Names(Frame(m, 1, {{G::Ui, "stats"}, {G::Toast, "toast"}, {G::Top, "beam"}})) ==
          "stats toast beam");
    // The toast goes: the beam stays at index 2, the toast's slot is a collapsed toast.
    const auto out = Frame(m, 1, {{G::Ui, "stats"}, {G::Top, "beam"}});
    CHECK(Names(out) == "stats toast_ beam");
    CHECK(out[1].quad.size == LS::kCollapse);
    CHECK(out[2].present);
    // It comes back in the same slot.
    CHECK(Names(Frame(m, 1, {{G::Ui, "stats"}, {G::Toast, "toast"}, {G::Top, "beam"}})) ==
          "stats toast beam");
    CHECK(m.Size() == 3);
}

TEST_CASE("Layer slots: a new quad goes where the producers' order puts it", "[frame_xr][flash]") {
    LS::Map<Q> m;
    Frame(m, 1, {{G::Ui, "stats"}, {G::Top, "beam"}});
    // A hint chip first seen: between the Ui group and the Top group.
    CHECK(Names(Frame(m, 1, {{G::Ui, "stats"}, {G::Hint, "hint"}, {G::Top, "beam"}})) ==
          "stats hint beam");
    // A cursor first seen after the stats this frame: right after them, in the Ui group.
    CHECK(Names(Frame(m, 1, {{G::Ui, "stats"}, {G::Ui, "cursor"}, {G::Top, "beam"}})) ==
          "stats cursor hint_ beam");
    // A dot first seen with no beam this frame: at the start of the Top group.
    CHECK(Names(Frame(m, 1, {{G::Ui, "stats"}, {G::Top, "dot"}})) ==
          "stats cursor_ hint_ dot beam_");
}

TEST_CASE("Layer slots: two of a kind keep their own slots", "[frame_xr][flash]") {
    LS::Map<Q> m;
    Frame(m, 1, {{G::Top, "d1"}, {G::Top, "d2"}});
    // Both cursors are of one swapchain and size: the first of them this frame takes slot 0.
    CHECK(Names(Frame(m, 1, {{G::Top, "d1"}})) == "d1 d2_");
    // A wider rect of the same swapchain is another kind (an atlas's other region).
    CHECK(Names(Frame(m, 1, {{G::Top, "d1"}, {G::Top, "dot"}})) == "d1 dot d2_");
}

TEST_CASE("Layer slots: a new context starts a new list", "[frame_xr][flash]") {
    LS::Map<Q> m;
    Frame(m, 1, {{G::Ui, "menu"}, {G::Ui, "dim"}, {G::Top, "dot"}});
    CHECK(Names(Frame(m, 2, {{G::Top, "beam"}})) == "beam");
    CHECK(m.Resets() == 2);
}

TEST_CASE("Layer slots: over the limit the slot absent longest goes", "[frame_xr][flash]") {
    LS::Map<Q> m;
    Frame(m, 1, {{G::Ui, "a"}, {G::Ui, "bb"}, {G::Ui, "ccc"}});
    Frame(m, 1, {{G::Ui, "bb"}, {G::Ui, "ccc"}}); // "a" absent since this frame
    Frame(m, 1, {{G::Ui, "ccc"}});               // "bb" absent since this one
    CHECK(Names(Frame(m, 1, {{G::Ui, "ccc"}}, 2)) == "bb_ ccc");
    CHECK(m.Dropped() == 1);
    // Present quads are never dropped here (the caller's limit cuts them).
    CHECK(Names(Frame(m, 1, {{G::Ui, "bb"}, {G::Ui, "ccc"}}, 1)) == "bb ccc");
}

// ---- integ13: the HUD's slots reserved from the start of the list ----

namespace {

/// A frame as app.cpp makes it: Begin, the HUD's four slots reserved (toast, hint, label, badge:
/// one swapchain 'h', keyed by their group alone), then the quads present. A HUD quad is keyed as
/// app.cpp keys it (the HUD's swapchain, size 0); others as Frame does.
std::vector<LS::Map<Q>::Out> HudFrame(LS::Map<Q>& m, uint32_t context,
                                      const std::vector<std::pair<G, std::string>>& quads,
                                      std::size_t max = 32) {
    m.Begin(context);
    for (const G g : {G::Toast, G::Hint, G::Label, G::Badge}) {
        m.Reserve(g, 'h', 0, 0, Q{"res"});
    }
    for (const auto& [g, name] : quads) {
        const bool hud = g == G::Toast || g == G::Hint || g == G::Label || g == G::Badge;
        m.Add(g, hud ? 'h' : static_cast<uint64_t>(name[0]),
              hud ? 0 : static_cast<int32_t>(name.size()), hud ? 0 : 1, Q{name});
    }
    std::vector<LS::Map<Q>::Out> out;
    m.End(max, out, [](Q& q) { q.size *= LS::kCollapse; });
    return out;
}

/// The positions of the quads named `name` (present or not) in a frame's list.
std::vector<std::size_t> Where(const std::vector<LS::Map<Q>::Out>& out, const std::string& name) {
    std::vector<std::size_t> at;
    for (std::size_t i = 0; i < out.size(); i++) {
        if (out[i].quad.name == name) {
            at.push_back(i);
        }
    }
    return at;
}

} // namespace

TEST_CASE("Layer slots: a toast, then a label, mid-game never move the controllers",
          "[frame_xr][flash][integ13]") {
    LS::Map<Q> m;
    // The controllers' two eye quads (one swapchain, one rect size: ordinals 0 and 1), then a
    // beam, all in the Top group; the stats panel in the Ui group.
    const std::vector<std::pair<G, std::string>> play{
        {G::Ui, "stats"}, {G::Top, "eye"}, {G::Top, "eye"}, {G::Top, "beam"}};
    const auto first = HudFrame(m, 3, play);
    CHECK(Names(first) == "stats res_ res_ res_ res_ eye eye beam");
    const std::vector<std::size_t> eyes = Where(first, "eye");
    REQUIRE(eyes.size() == 2);
    // The reserved slots are collapsed stand-ins.
    CHECK(first[1].quad.size == LS::kCollapse);
    auto with = [&](std::vector<std::pair<G, std::string>> extra) {
        auto q = play;
        q.insert(q.begin() + 1, extra.begin(), extra.end());
        return q;
    };
    for (int f = 0; f < 3; f++) {
        CHECK(Where(HudFrame(m, 3, play), "eye") == eyes);
    }
    // A Quick Save toast shows, then goes; then the Autosaved label shows with a second toast.
    auto out = HudFrame(m, 3, with({{G::Toast, "toast"}}));
    CHECK(Names(out) == "stats toast res_ res_ res_ eye eye beam");
    CHECK(Where(out, "eye") == eyes);
    out = HudFrame(m, 3, play);
    CHECK(Names(out) == "stats toast_ res_ res_ res_ eye eye beam");
    CHECK(Where(out, "eye") == eyes);
    out = HudFrame(m, 3, with({{G::Label, "label"}}));
    CHECK(Names(out) == "stats toast_ res_ label res_ eye eye beam");
    CHECK(Where(out, "eye") == eyes);
    out = HudFrame(m, 3, with({{G::Toast, "toast"}, {G::Label, "label"}, {G::Badge, "badge"}}));
    CHECK(Names(out) == "stats toast res_ label badge eye eye beam");
    CHECK(Where(out, "eye") == eyes);
    // Only the first frame made slots: the list never changed length.
    CHECK(m.Size() == first.size());
    CHECK(m.Added() == first.size());
    CHECK(m.Resets() == 1);
}

TEST_CASE("Layer slots: over the limit, reserved slots go last and stay gone",
          "[frame_xr][flash][integ13]") {
    LS::Map<Q> m;
    HudFrame(m, 3, {{G::Ui, "a"}, {G::Ui, "bb"}, {G::Top, "eye"}, {G::Top, "eye"}});
    HudFrame(m, 3, {{G::Ui, "bb"}, {G::Top, "eye"}, {G::Top, "eye"}}); // "a" absent from now
    // Two slots too many: "a" goes first (absent, not reserved), then the reserved slot absent
    // longest (all four since the first frame: the first of them, the toast's).
    auto out = HudFrame(m, 3, {{G::Ui, "bb"}, {G::Top, "eye"}, {G::Top, "eye"}}, 6);
    CHECK(Names(out) == "bb res_ res_ res_ eye eye");
    CHECK(m.Dropped() == 2);
    // The dropped reserved slot is not made again in this list, so the eyes stay put.
    const auto eyes = Where(out, "eye");
    out = HudFrame(m, 3, {{G::Ui, "bb"}, {G::Top, "eye"}, {G::Top, "eye"}}, 6);
    CHECK(Names(out) == "bb res_ res_ res_ eye eye");
    CHECK(Where(out, "eye") == eyes);
    // A new list reserves all four again.
    CHECK(Names(HudFrame(m, 4, {{G::Top, "eye"}})) == "res_ res_ res_ res_ eye");
}

TEST_CASE("Layer slots: the VR menu's list with Motion Clarity's backing fits 16 layers",
          "[frame_xr][flash][flash2]") {
    // flash2: Motion Clarity stays in effect in the menu, so the screens keep their backing: 4
    // screen layers (backing and screen per eye). The menu's list as app.cpp makes it: the HUD's
    // Toast, Label and Badge reserved (not the hint chip, which shows only in play), the dim and
    // the menu panel (Ui), then the Top group as Visuals::TopLayers fills it from the room left
    // (pointer dots, the controllers' projection layer, their two eye quads, the beams).
    constexpr std::size_t kLimit = 16, kScreens = 4;
    LS::Map<Q> m;
    const auto frame = [&](int dots_wanted, int beams_wanted, bool budget_from_all) {
        m.Begin(1);
        for (const G g : {G::Toast, G::Label, G::Badge}) {
            m.Reserve(g, 'h', 0, 0, Q{"res"});
        }
        m.Add(G::Ui, 'd', 8, 8, Q{"dim"});
        m.Add(G::Ui, 'm', 1470, 980, Q{"menu"});
        // The Top group's room: before flash2 from Size(), which counts the Top group's own slots
        // of earlier frames too.
        const std::size_t used = kScreens + (budget_from_all ? m.Size() : m.SizeBefore(G::Top));
        std::size_t room = kLimit > used ? kLimit - used : 0;
        const std::size_t dots = std::min<std::size_t>(room, static_cast<std::size_t>(dots_wanted));
        room -= dots;
        const bool models = room > 0;
        room -= models ? 1 : 0;
        const bool eyes = room >= 2;
        room -= eyes ? 2 : 0;
        const std::size_t beams = std::min<std::size_t>(room, static_cast<std::size_t>(beams_wanted));
        if (eyes) {
            m.Add(G::Top, 'e', 2, 1, Q{"eye"});
            m.Add(G::Top, 'e', 2, 1, Q{"eye"});
        }
        for (std::size_t i = 0; i < beams; i++) {
            m.Add(G::Top, 'b', 3, 1, Q{"beam"});
        }
        for (std::size_t i = 0; i < dots; i++) {
            m.Add(G::Top, 'c', 1, 1, Q{"dot"});
        }
        std::vector<LS::Map<Q>::Out> out;
        m.End(kLimit - kScreens - (models ? 1 : 0), out, [](Q& q) { q.size *= LS::kCollapse; });
        struct Result {
            std::size_t layers;
            bool models, eyes;
            std::size_t beams;
            std::string names;
        };
        return Result{kScreens + (models ? 1 : 0) + out.size(), models, eyes, beams, Names(out)};
    };
    // Both controllers pointing at the menu, for 2 s, one of them looking away now and then: all
    // of it shows, in 16 layers, and the list keeps its length and order.
    std::string first;
    for (int f = 0; f < 240; f++) {
        const auto r = frame(f % 60 < 50 ? 2 : 1, f % 60 < 50 ? 2 : 1, false);
        CHECK(r.layers <= kLimit);
        CHECK(r.models);
        CHECK(r.eyes);
        CHECK(r.beams == (f % 60 < 50 ? 2u : 1u));
        if (f == 0) {
            first = r.names;
            CHECK(r.layers == kLimit);
        }
    }
    CHECK(m.Dropped() == 0);
    CHECK(m.Added() == 11); // made on the first frame only
    CHECK(first == "dim menu res_ res_ res_ eye eye beam beam dot dot");
    // The budget before flash2, from Size(): on the next frame the Top group's own slots took its
    // room, and the eye quads and the beams went (the "vis: 16 layers allowed" warnings of the
    // owner's menus, with 2 screen layers then; with the backing's 4 even the models would go).
    LS::Map<Q> fresh;
    std::swap(m, fresh);
    frame(2, 2, true);
    const auto old = frame(2, 2, true);
    CHECK_FALSE(old.eyes);
    CHECK(old.beams == 0u);
}

// ---- projdef: a slot never keeps a destroyed swapchain ----

namespace {

/// What app.cpp does before each frame's list: the swapchains destroyed since, forgotten.
std::size_t ForgetRetired(LS::Map<Q>& m) {
    std::size_t n = 0;
    for (const uint64_t h : FrameXr::RetiredSwapchains::Take()) {
        n += m.Forget(h);
    }
    return n;
}

bool Carries(const std::vector<LS::Map<Q>::Out>& out, char swapchain) {
    return std::any_of(out.begin(), out.end(),
                       [&](const auto& o) { return !o.quad.name.empty() && o.quad.name[0] == swapchain; });
}

} // namespace

TEST_CASE("Layer slots: a live switch to the projection layer never sends the models' old "
          "swapchain",
          "[frame_xr][projdef]") {
    // 1.1.1-590, Kid Icarus in the normal view: the controllers as two eye quads of the models'
    // swapchain 'e' (Top group) with a beam. The switch makes the models' pass again: 'e' is
    // destroyed, and while the new pass has no image the top screen still goes out as quads, so
    // the screens' layers keep their number and the list (its context) lives on.
    FrameXr::RetiredSwapchains::Take(); // nothing left from another test
    LS::Map<Q> m;
    const std::vector<std::pair<G, std::string>> quads{
        {G::Ui, "stats"}, {G::Top, "eye"}, {G::Top, "eye"}, {G::Top, "beam"}};
    for (int f = 0; f < 3; f++) {
        CHECK(Carries(HudFrame(m, 3, quads), 'e'));
    }
    // The switch: the models' swapchain destroyed (ControllerModels::DestroyXr notes it).
    FrameXr::RetiredSwapchains::Note('e');
    // Before the fix: the next frames carried the eye quads' slots, collapsed but with the dead
    // handle, and SteamVR refused them (XR_ERROR_HANDLE_INVALID).
    {
        LS::Map<Q> old = m;
        const auto stale = HudFrame(old, 3, {{G::Ui, "stats"}, {G::Top, "beam"}});
        CHECK(Names(stale) == "stats res_ res_ res_ res_ eye_ eye_ beam");
    }
    CHECK(ForgetRetired(m) == 2);
    CHECK(m.Forgotten() == 2);
    for (int f = 0; f < 3; f++) {
        const auto out = HudFrame(m, 3, {{G::Ui, "stats"}, {G::Top, "beam"}});
        CHECK_FALSE(Carries(out, 'e'));
        CHECK(Names(out) == "stats res_ res_ res_ res_ beam");
    }
    CHECK(m.Resets() == 1); // the same list: only the dead slots went
    // And back (projection to quads): the new pass's swapchain 'f' as eye quads, then destroyed
    // at the next switch; its slots go too.
    for (int f = 0; f < 2; f++) {
        HudFrame(m, 3, {{G::Ui, "stats"}, {G::Top, "f1"}, {G::Top, "f1"}, {G::Top, "beam"}});
    }
    FrameXr::RetiredSwapchains::Note('f');
    CHECK(ForgetRetired(m) == 2);
    CHECK_FALSE(Carries(HudFrame(m, 3, {{G::Ui, "stats"}, {G::Top, "beam"}}), 'f'));
    // Taken once: nothing left for the next frame.
    CHECK(FrameXr::RetiredSwapchains::Take().empty());
    // A handle not in the list changes nothing.
    FrameXr::RetiredSwapchains::Note('z');
    CHECK(ForgetRetired(m) == 0);
    FrameXr::RetiredSwapchains::Note(0); // ignored
    CHECK(FrameXr::RetiredSwapchains::Take().empty());
}

TEST_CASE("Layer slots: a forgotten reserved key may be reserved again", "[frame_xr][projdef]") {
    LS::Map<Q> m;
    HudFrame(m, 3, {{G::Top, "eye"}});
    CHECK(m.Forget('h') == 4); // the HUD's chain destroyed: its four reserved slots go
    CHECK(Names(HudFrame(m, 3, {{G::Top, "eye"}})) == "res_ res_ res_ res_ eye");
}

TEST_CASE("Layer slots: every path switch starts a new list", "[frame_xr][projdef]") {
    // 590b: Mario Kart 7 paused, Motion Clarity off: 2 screen layers before and after the flip to
    // the projection layer (its fallback quads), so the count alone kept the list, and a slot kept
    // the models' destroyed swapchain (XR_ERROR_LAYER_INVALID). Kid Icarus: Uprising's normal view
    // in 590 likewise (3 -> 3). From Motion Clarity on (4: a backing and a screen per eye) to 2
    // the count changed and nothing failed. Every case now gives a new key, and so a new list.
    for (const std::size_t screens : {std::size_t{2}, std::size_t{3}}) {
        CHECK(LS::ContextKey(3, true, false, screens) != LS::ContextKey(3, true, true, screens));
    }
    CHECK(LS::ContextKey(3, true, false, 4) != LS::ContextKey(3, true, true, 2));
    // The other parts still count, and no two inputs share a key.
    CHECK(LS::ContextKey(3, true, true, 2) != LS::ContextKey(3, false, true, 2));
    CHECK(LS::ContextKey(3, true, true, 2) != LS::ContextKey(4, true, true, 2));
    CHECK(LS::ContextKey(3, true, true, 20) == LS::ContextKey(3, true, true, 15)); // capped
    // The sequence: the same screens' count over the flip, the eye quads of 'e' gone with it.
    LS::Map<Q> m;
    const std::vector<std::pair<G, std::string>> quads{
        {G::Ui, "stats"}, {G::Top, "eye"}, {G::Top, "eye"}, {G::Top, "beam"}};
    for (int f = 0; f < 3; f++) {
        CHECK(Carries(HudFrame(m, LS::ContextKey(3, true, false, 2), quads), 'e'));
    }
    // No Forget here: the new list alone drops the dead slots.
    for (int f = 0; f < 3; f++) {
        const auto out =
            HudFrame(m, LS::ContextKey(3, true, true, 2), {{G::Ui, "stats"}, {G::Top, "beam"}});
        CHECK_FALSE(Carries(out, 'e'));
    }
    CHECK(m.Resets() == 2);
    // And from Motion Clarity on (4 screen layers) to the projection layer (2).
    LS::Map<Q> mc;
    HudFrame(mc, LS::ContextKey(3, true, false, 4), quads);
    CHECK_FALSE(Carries(
        HudFrame(mc, LS::ContextKey(3, true, true, 2), {{G::Ui, "stats"}, {G::Top, "beam"}}), 'e'));
}
