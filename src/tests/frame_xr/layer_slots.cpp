// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flash: fixed layer positions for the quads after the screens
// (frame_xr/layer_slots.h): a quad that comes and goes never moves another quad to another
// position (SteamVR's overlay of that index), absent quads are collapsed in their own slot, new
// ones go where the producers' order puts them, and the runtime's limit drops the oldest absent.

#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/layer_slots.h"

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
