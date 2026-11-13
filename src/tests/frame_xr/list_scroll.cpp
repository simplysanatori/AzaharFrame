// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame scroll: the in-game menu's list scrolling (frame_xr/menu/list_scroll.h): the
// stick's speed and build-up, the scrollbar's drag and paging, dragging the list and its fling,
// and the lists drawing the bar.

#include <cmath>
#include <string>
#include <vector>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "frame_xr/menu/list_scroll.h"
#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/tab_menu.h"
#include "frame_xr/ui/ui_canvas.h"

using namespace FrameXr::Menu;
using namespace FrameXr::Menu::Scrolling;
using Catch::Approx;

namespace {

constexpr float kDt = 1.0f / 90.0f; // the Frame's menu runs at 90 Hz or more
constexpr float kViewH = 360.0f;    // about the Cheats list's height
constexpr float kMax = 6000.0f;     // 125 rows of 48 dp

/// Holds the stick at `y` for `seconds`; returns the scroll after.
float Hold(StickScroll& s, float y, float seconds, float scroll, float max = kMax,
           float step = 48.0f) {
    const int frames = static_cast<int>(std::lround(seconds / kDt));
    for (int i = 0; i < frames; i++) {
        scroll = s.Update(y, kDt, scroll, max, kViewH, step);
    }
    return scroll;
}

} // namespace

TEST_CASE("List scroll: the stick's speed follows the push", "[frame_xr][scroll]") {
    CHECK(StickSpeed(0.0f, kStartSpeed) == 0.0f);
    CHECK(StickSpeed(kStickDead, kStartSpeed) == 0.0f);
    CHECK(StickSpeed(-0.1f, kStartSpeed) == 0.0f);
    float last = 0.0f;
    for (float y = 0.15f; y <= 1.0f; y += 0.05f) {
        const float v = StickSpeed(y, kStartSpeed);
        CHECK(v > last); // the further, the faster
        CHECK(v == StickSpeed(-y, kStartSpeed));
        last = v;
    }
    CHECK(StickSpeed(1.0f, kStartSpeed) == Approx(kStartSpeed));
    CHECK(StickSpeed(0.95f, kStartSpeed) == Approx(kStartSpeed)); // a full push on the Frame
    // Half a push is slow enough to read along: about a third of the speed.
    const float half = StickSpeed(0.5f, kStartSpeed) / kStartSpeed;
    CHECK(half > 0.2f);
    CHECK(half < 0.4f);
}

TEST_CASE("List scroll: held over, the stick speeds up smoothly to a screen in 0.25 s",
          "[frame_xr][scroll]") {
    // The build-up alone.
    CHECK(HeldSpeed(0.0f, kViewH) == Approx(kStartSpeed));
    CHECK(HeldSpeed(kAccelDelay, kViewH) == Approx(kStartSpeed));
    CHECK(HeldSpeed(kAccelFull, kViewH) == Approx(kViewH / 0.25f));
    CHECK(HeldSpeed(5.0f, kViewH) == Approx(kViewH / 0.25f));
    // About today's speed to start with: a row every 0.2 s is 240 (48 dp) to 360 (72 dp) dp/s.
    CHECK(kStartSpeed >= 240.0f);
    CHECK(kStartSpeed <= 360.0f);
    // A short list still never goes slower than the start.
    CHECK(HeldSpeed(5.0f, 40.0f) == Approx(kStartSpeed));

    // Pushed all the way up from the top of the list's end: frame by frame.
    StickScroll s;
    float scroll = kMax;
    float last_v = 0.0f, biggest_jump = 0.0f;
    float at_100ms = 0.0f, at_1s = 0.0f, t90 = -1.0f;
    for (int i = 1; i <= static_cast<int>(1.5f / kDt); i++) {
        scroll = s.Update(1.0f, kDt, scroll, kMax, kViewH, 48.0f);
        const float v = -s.Velocity(); // stick up: up the list
        CHECK(v >= last_v - 0.001f);   // never slows while held
        biggest_jump = std::max(biggest_jump, v - last_v);
        last_v = v;
        const float t = static_cast<float>(i) * kDt;
        if (std::fabs(t - 0.1f) < kDt * 0.5f) {
            at_100ms = v;
        }
        if (std::fabs(t - 1.0f) < kDt * 0.5f) {
            at_1s = v;
        }
        if (t90 < 0.0f && v >= 0.9f * kViewH / 0.25f) {
            t90 = t;
        }
    }
    CHECK(at_100ms > 0.8f * kStartSpeed); // eased in within a tenth of a second...
    CHECK(at_100ms < 1.1f * kStartSpeed); // ...at about today's speed
    CHECK(at_1s == Approx(kViewH / 0.25f).epsilon(0.02));
    CHECK(t90 > 0.7f);
    CHECK(t90 < 1.0f);
    CHECK(biggest_jump < 75.0f); // dp/s a frame: no jolt (the largest is the ease-in's first)
    CHECK(scroll < kMax - kViewH * 3.0f);
}

TEST_CASE("List scroll: a gentle push stays gentle; letting go stops at once",
          "[frame_xr][scroll]") {
    StickScroll s;
    float scroll = Hold(s, -0.5f, 3.0f, 0.0f); // half down for 3 s
    const float gentle = StickSpeed(0.5f, kStartSpeed);
    CHECK(s.Velocity() == Approx(gentle));
    CHECK(scroll == Approx(gentle * 3.0f).epsilon(0.05));

    // Full speed, then let go: not a dp further.
    scroll = Hold(s, -1.0f, 2.0f, scroll);
    CHECK(s.Velocity() > 1000.0f);
    const float before = scroll;
    scroll = s.Update(0.0f, kDt, scroll, kMax, kViewH, 48.0f);
    CHECK(scroll == before);
    CHECK_FALSE(s.Active());
    CHECK(s.Velocity() == 0.0f);
    CHECK(Hold(s, 0.0f, 1.0f, scroll) == before);

    // The stick springing back through the middle slows it on the way, never speeds it up.
    s.Reset();
    scroll = Hold(s, -1.0f, 1.0f, 0.0f);
    float v = s.Velocity();
    for (float y : {-0.8f, -0.5f, -0.25f, -0.1f}) {
        scroll = s.Update(y, kDt, scroll, kMax, kViewH, 48.0f);
        CHECK(s.Velocity() <= v);
        v = s.Velocity();
    }
    CHECK(v == 0.0f);

    // At an end it stops there.
    s.Reset();
    CHECK(Hold(s, 1.0f, 1.0f, 30.0f) == 0.0f);
    CHECK(Hold(s, -1.0f, 3.0f, kMax - 30.0f) == kMax);
}

TEST_CASE("List scroll: a tap of the stick moves exactly one row", "[frame_xr][scroll]") {
    StickScroll s;
    float scroll = Hold(s, -1.0f, 0.1f, 480.0f);
    scroll = s.Update(0.0f, kDt, scroll, kMax, kViewH, 48.0f);
    CHECK(scroll == Approx(528.0f));
    scroll = Hold(s, 1.0f, 0.1f, scroll);
    scroll = s.Update(0.0f, kDt, scroll, kMax, kViewH, 48.0f);
    CHECK(scroll == Approx(480.0f));
    // A Settings row.
    scroll = Hold(s, -1.0f, 0.05f, 0.0f, kMax, 72.0f);
    CHECK(s.Update(0.0f, kDt, scroll, kMax, kViewH, 72.0f) == Approx(72.0f));
    // A wheel's notches (50 ms pressed, 40 ms not): a row each.
    scroll = 0.0f;
    for (int n = 0; n < 3; n++) {
        scroll = Hold(s, -1.0f, 0.05f, scroll);
        scroll = Hold(s, 0.0f, 0.04f, scroll);
    }
    CHECK(scroll == Approx(144.0f));
    // A light touch is no tap: it moves what it moved.
    scroll = Hold(s, -0.3f, 0.1f, 0.0f);
    CHECK(s.Update(0.0f, kDt, scroll, kMax, kViewH, 48.0f) < 5.0f);
    // Held longer it is a scroll, not a tap: it stops where it is.
    scroll = Hold(s, -1.0f, 0.3f, 0.0f);
    const float held = scroll;
    CHECK(s.Update(0.0f, kDt, scroll, kMax, kViewH, 48.0f) == held);
    // A tap at the end stays at the end.
    scroll = Hold(s, -1.0f, 0.1f, kMax - 10.0f);
    CHECK(s.Update(0.0f, kDt, scroll, kMax, kViewH, 48.0f) == kMax);
}

TEST_CASE("List scroll: the scrollbar's thumb follows the beam 1:1", "[frame_xr][scroll]") {
    const Bar b{{120.0f, 160.0f, 828.0f, 540.0f}, 828.0f, 6000.0f, 5664.0f};
    REQUIRE(b.Valid());
    const float track = TrackY1(b) - TrackY0(b);
    CHECK(ThumbH(b) == Approx(std::max(kThumbMin, track * b.list.H() / b.content_h)));
    CHECK(ThumbY(b, 0.0f) == Approx(TrackY0(b)));
    CHECK(ThumbY(b, b.max_scroll) + ThumbH(b) == Approx(TrackY1(b)));
    // The beam's strip: wide enough for a beam, wider than the bar, on the bar.
    const Rect hit = HitRect(b);
    CHECK(hit.W() >= 28.0f);
    CHECK(hit.W() > kBarHoverW * 3.0f);
    CHECK(hit.x1 <= kPanelW);
    const Rect slim = ThumbRect(b, 0.0f, false), wide = ThumbRect(b, 0.0f, true);
    CHECK(slim.W() == Approx(kBarW));
    CHECK(wide.W() == Approx(kBarHoverW));
    CHECK(hit.x0 < wide.x0);
    CHECK(hit.x1 > wide.x1);
    CHECK(hit.y0 == b.list.y0);
    CHECK(hit.y1 == b.list.y1);

    // Held at its middle, then dragged: the thumb moves just as far as the beam.
    BarPress press;
    float scroll = 1000.0f;
    const float grab_y = ThumbY(b, scroll) + ThumbH(b) * 0.5f;
    CHECK(press.Begin(b, scroll, grab_y) == scroll); // taking it moves nothing
    CHECK(press.Dragging());
    for (float d : {-20.0f, 5.0f, 37.5f, 80.0f}) {
        const float s2 = press.Update(b, scroll, grab_y + d, kDt);
        CHECK(ThumbY(b, s2) == Approx(ThumbY(b, scroll) + d).margin(0.01));
    }
    // Past the track's ends it stops at the list's ends.
    CHECK(press.Update(b, scroll, grab_y - 1000.0f, kDt) == 0.0f);
    CHECK(press.Update(b, scroll, grab_y + 1000.0f, kDt) == b.max_scroll);
    // The whole track is the whole list.
    CHECK(ScrollForThumb(b, TrackY0(b)) == 0.0f);
    CHECK(ScrollForThumb(b, TrackY1(b) - ThumbH(b)) == Approx(b.max_scroll));
    press.End();
    CHECK_FALSE(press.Active());
}

TEST_CASE("List scroll: a click on the track pages by one screen", "[frame_xr][scroll]") {
    const Bar b{{120.0f, 160.0f, 828.0f, 540.0f}, 828.0f, 6000.0f, 5664.0f};
    const float screen = b.list.H();
    float scroll = 2000.0f;
    CHECK(TrackSide(b, scroll, ThumbY(b, scroll) - 1.0f) == -1);
    CHECK(TrackSide(b, scroll, ThumbY(b, scroll) + 1.0f) == 0);
    CHECK(TrackSide(b, scroll, ThumbY(b, scroll) + ThumbH(b) + 1.0f) == 1);
    BarPress press;
    const float below = TrackY1(b) - 2.0f;
    CHECK(press.Begin(b, scroll, below) == Approx(scroll + screen));
    CHECK_FALSE(press.Dragging());
    press.End();
    CHECK(press.Begin(b, scroll, TrackY0(b) + 1.0f) == Approx(scroll - screen));
    press.End();
    CHECK(PageFrom(b, 100.0f, -1) == 0.0f);
    CHECK(PageFrom(b, b.max_scroll - 100.0f, 1) == b.max_scroll);

    // Held on the track: after a pause it pages on, and stops once the thumb is under the beam.
    scroll = press.Begin(b, 0.0f, below);
    int pages = 1;
    for (int i = 0; i < static_cast<int>(10.0f / kDt); i++) {
        const float next = press.Update(b, scroll, below, kDt);
        if (next != scroll) {
            pages++;
            if (pages == 2) {
                CHECK(static_cast<float>(i + 1) * kDt >= kPageRepeatDelay - kDt);
            }
        }
        scroll = next;
    }
    CHECK(TrackSide(b, scroll, below) != 1); // the thumb came to the beam
    CHECK(pages > 3);
    CHECK(pages < 20);
}

TEST_CASE("List scroll: a press drags the list only past the slop; a fast one flings",
          "[frame_xr][scroll]") {
    DragScroll d;
    float scroll = 500.0f;
    d.Press(300.0f, scroll, 0.0f);
    // A tap's wobble (a trigger pull tips the beam): not a drag, the row still clicks.
    float s = scroll;
    CHECK_FALSE(d.Move(300.0f + kDragSlop * 0.9f, 0.05f, s, kMax));
    CHECK_FALSE(d.Move(300.0f - kDragSlop * 0.9f, 0.10f, s, kMax));
    CHECK(s == scroll);
    CHECK_FALSE(d.Dragging());
    d.Release(0.12f);
    CHECK_FALSE(d.Flinging());

    // Past the slop it drags, from where it is (no jump), then 1:1 with the beam.
    d.Press(300.0f, scroll, 0.0f);
    CHECK(d.Move(300.0f + kDragSlop + 1.0f, 0.02f, s, kMax));
    CHECK(s == Approx(scroll));
    CHECK(d.Dragging());
    CHECK(d.Move(300.0f + kDragSlop + 51.0f, 0.04f, s, kMax));
    CHECK(s == Approx(scroll - 50.0f)); // the beam down the panel pulls the list down with it
    // Held still before letting go: no fling.
    for (int i = 0; i < 20; i++) {
        d.Move(300.0f + kDragSlop + 51.0f, 0.04f + static_cast<float>(i + 1) * kDt, s, kMax);
    }
    d.Release(0.3f);
    CHECK_FALSE(d.Flinging());

    // Swept up quickly (1500 dp/s) and let go: it coasts on, slows and stops, no bounce.
    scroll = 2000.0f;
    d.Press(400.0f, scroll, 0.0f);
    float y = 400.0f, t = 0.0f;
    s = scroll;
    for (int i = 0; i < 12; i++) {
        t += kDt;
        y -= 1500.0f * kDt;
        d.Move(y, t, s, kMax);
    }
    const float at_release = s;
    d.Release(t);
    REQUIRE(d.Flinging());
    CHECK(d.FlingVelocity() == Approx(1500.0f).epsilon(0.05)); // scroll's way: down the list
    float coast = at_release, last_step = 1e9f;
    int frames = 0;
    while (d.Flinging() && frames < 1000) {
        const float next = d.Coast(coast, kMax, kDt);
        CHECK(next - coast <= last_step + 0.001f); // always slowing
        last_step = next - coast;
        coast = next;
        frames++;
    }
    CHECK_FALSE(d.Flinging());
    CHECK(frames * kDt < 2.0f); // gentle: done within two seconds
    const float distance = coast - at_release;
    CHECK(distance == Approx(1500.0f * kFlingTau).epsilon(0.1)); // v * tau
    // A new press catches it.
    d.Press(400.0f, 2000.0f, 0.0f);
    for (int i = 0; i < 12; i++) {
        d.Move(400.0f - 20.0f * static_cast<float>(i + 1), kDt * static_cast<float>(i + 1), s, kMax);
    }
    d.Release(12.0f * kDt);
    REQUIRE(d.Flinging());
    d.Press(0.0f, s, 0.0f);
    CHECK_FALSE(d.Flinging());
    d.Cancel();

    // A slow drag let go: it stays put.
    d.Press(400.0f, 1000.0f, 0.0f);
    s = 1000.0f;
    y = 400.0f;
    t = 0.0f;
    for (int i = 0; i < 60; i++) {
        t += kDt;
        y -= 100.0f * kDt;
        d.Move(y - kDragSlop, t, s, kMax);
    }
    d.Release(t);
    CHECK_FALSE(d.Flinging());

    // A fling into the end stops there.
    d.Press(400.0f, kMax - 50.0f, 0.0f);
    s = kMax - 50.0f;
    y = 400.0f;
    t = 0.0f;
    for (int i = 0; i < 4; i++) {
        t += kDt;
        y -= 1200.0f * kDt;
        d.Move(y - kDragSlop - 1.0f, t, s, kMax);
    }
    d.Release(t);
    float c = s;
    for (int i = 0; i < 200; i++) {
        c = d.Coast(c, kMax, kDt);
    }
    CHECK(c == kMax);
    CHECK_FALSE(d.Flinging());
}

namespace {

struct Drawn {
    Kit kit{1.0f};
    frame_xr::ui::Canvas canvas;
    TabMenu menu;
    void Draw(const MenuModel& m, const MenuView& v) {
        canvas.Init(static_cast<int>(kPanelW), static_cast<int>(kPanelH));
        kit.Begin(canvas);
        menu.Draw(kit, m, v);
    }
    const MenuLayout::Hit* Find(Id id) const {
        for (const auto& h : menu.Hits()) {
            if (h.t.id == id) {
                return &h;
            }
        }
        return nullptr;
    }
};

MenuModel LongCheats(int n) {
    MenuModel m;
    m.game_title = "Pokémon X";
    m.mods.cheats.download = DownloadState::Done;
    for (int i = 0; i < n; i++) {
        m.mods.cheats.cheats.push_back(
            {"Cheat " + std::to_string(i), CheatSource::Sharkive, false,
             i == 3 ? "for v1.5, installed v1.0" : ""});
    }
    return m;
}

} // namespace

TEST_CASE("List scroll: a long list draws a scrollbar a beam can take; a short one none",
          "[frame_xr][scroll]") {
    Drawn d;
    MenuView v;
    v.tab = Tab::Mods;
    d.Draw(LongCheats(125), v);
    const MenuLayout& layout = d.menu.Layout();
    REQUIRE(layout.bar.Valid());
    CHECK(layout.bar.max_scroll == layout.max_scroll);
    CHECK(layout.bar.content_h == Approx(125.0f * 48.0f));
    REQUIRE(d.Find(Id::Scrollbar) != nullptr);
    const Rect bar = d.Find(Id::Scrollbar)->r;
    CHECK(bar.W() >= 28.0f);
    CHECK(bar.H() == Approx(layout.bar.list.H()));
    // It wins over the row beside it, at the pane's right edge.
    CHECK(d.menu.HitTest(bar.Cx(), bar.Cy()).id == Id::Scrollbar);
    CHECK(d.menu.HitTest(bar.x0 - 30.0f, bar.Cy()).id == Id::Cheat);

    // Scrolled part of a row: the rows move by that much (smooth), cut at the list's top.
    v.scroll[static_cast<int>(Tab::Mods)] = 0.0f;
    d.Draw(LongCheats(125), v);
    float y0 = -1.0f;
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id == Id::Cheat && h.t.index == 5) {
            y0 = h.r.y0;
        }
    }
    v.scroll[static_cast<int>(Tab::Mods)] = 130.0f;
    d.Draw(LongCheats(125), v);
    float y1 = -1.0f;
    for (const auto& h : d.menu.Hits()) {
        if (h.t.id == Id::Cheat && h.t.index == 5) {
            y1 = h.r.y0;
        }
    }
    CHECK(y0 - y1 == Approx(130.0f));

    // A dialog over the list holds it still where it was (it used to jump to the top).
    v.confirm = {Id::Cheat, 3, "Cheat 3"};
    d.Draw(LongCheats(125), v);
    CHECK(d.menu.Layout().scroll_locked);
    CHECK(d.menu.MaxScroll() > 130.0f);
    CHECK(d.menu.HitTest(bar.Cx(), bar.Cy()).id != Id::Scrollbar);
    v.confirm = {};

    // Five cheats fit: no bar, nothing to take.
    d.Draw(LongCheats(5), v);
    CHECK_FALSE(d.menu.Layout().bar.Valid());
    CHECK(d.Find(Id::Scrollbar) == nullptr);

    // The other long lists draw the same bar: Settings.
    v = {};
    v.tab = Tab::Settings;
    v.section = SettingsSection::Graphics;
    d.Draw(LongCheats(5), v);
    if (d.menu.MaxScroll() > 0.0f) {
        CHECK(d.menu.Layout().bar.Valid());
        CHECK(d.Find(Id::Scrollbar) != nullptr);
    }
    // Homebrew.
    v = {};
    v.tab = Tab::Homebrew;
    MenuModel hm;
    for (int i = 0; i < 12; i++) {
        HomebrewItem it;
        it.title = "App " + std::to_string(i);
        hm.homebrew.items.push_back(it);
    }
    d.Draw(hm, v);
    REQUIRE(d.menu.MaxScroll() > 0.0f);
    CHECK(d.menu.Layout().bar.Valid());
    CHECK(d.Find(Id::Scrollbar) != nullptr);
}

// AzaharFrame scroll: the Cheats list's "For Another Version" section.
TEST_CASE("Cheats: cheats for another version go last, in a section that starts closed",
          "[frame_xr][scroll][cheats]") {
    CHECK(OtherVersionHeader(119, 0) == "For Another Version (119)");
    CHECK(OtherVersionHeader(119, 2) == "For Another Version (119 · 2 On)");
    CHECK(OtherVersionHeader(1, 1) == "For Another Version (1 · 1 On)");
    CHECK(std::string(kAllOtherVersionNote) == "All Cheats Are for Another Version");

    CheatsModel m;
    m.cheats = {{"A"}, {"B", CheatSource::Sharkive, true, "for v1.5, this game is v1.0"},
                {"C"}, {"D", CheatSource::Sharkive, false, "for Rev0, this cartridge is Rev1"},
                {"E"}};
    CHECK_FALSE(CheatForOtherVersion(m.cheats[0]));
    CHECK(CheatForOtherVersion(m.cheats[1]));
    CHECK(CheatForOtherVersion(m.cheats[3]));
    using K = CheatRow::Kind;
    CHECK(CheatRows(m, false) ==
          std::vector<CheatRow>{{K::Cheat, 0}, {K::Cheat, 2}, {K::Cheat, 4}, {K::Header}});
    CHECK(CheatRows(m, true) == std::vector<CheatRow>{{K::Cheat, 0}, {K::Cheat, 2},
                                                      {K::Cheat, 4}, {K::Header},
                                                      {K::Cheat, 1}, {K::Cheat, 3}});
    // None for another version: no section.
    CheatsModel plain;
    plain.cheats = {{"A"}, {"B"}};
    CHECK(CheatRows(plain, false) == std::vector<CheatRow>{{K::Cheat, 0}, {K::Cheat, 1}});
    // All of them: the note, the section (still closed).
    CheatsModel all;
    all.cheats = {m.cheats[1], m.cheats[3]};
    CHECK(CheatRows(all, false) == std::vector<CheatRow>{{K::Note}, {K::Header}});
    CHECK(CheatRows(all, true) ==
          std::vector<CheatRow>{{K::Note}, {K::Header}, {K::Cheat, 0}, {K::Cheat, 1}});
    CHECK(CheatRows({}, false).empty());
}

TEST_CASE("Cheats: Pokémon X 1.0's 125 cheats, 119 for v1.5", "[frame_xr][scroll][cheats]") {
    MenuModel m = LongCheats(125);
    for (int i = 0; i < 125; i++) {
        CheatItem& c = m.mods.cheats.cheats[static_cast<std::size_t>(i)];
        c.version_note = i < 6 ? "" : "for v1.5, this game is v1.0";
        c.enabled = i == 2 || i == 40 || i == 77;
    }
    Kit kit{1.0f};
    frame_xr::ui::Canvas canvas;
    TabMenu menu;
    std::vector<DrawnText> texts;
    const auto draw = [&](const MenuView& v) {
        canvas.Init(static_cast<int>(kPanelW), static_cast<int>(kPanelH));
        kit.Begin(canvas);
        texts.clear();
        kit.RecordText(&texts);
        menu.Draw(kit, m, v);
        kit.RecordText(nullptr);
    };
    const auto drew = [&](const std::string& s) {
        for (const auto& t : texts) {
            if (t.text == s) {
                return true;
            }
        }
        return false;
    };
    const auto hit = [&](Id id, int index) -> const MenuLayout::Hit* {
        for (const auto& h : menu.Hits()) {
            if (h.t.id == id && h.t.index == index) {
                return &h;
            }
        }
        return nullptr;
    };
    MenuView v;
    v.tab = Tab::Mods;
    draw(v);
    // Closed: the six for 1.0 on top, then the section; it fits, nothing scrolls.
    CHECK(drew("For Another Version (119 · 2 On)")); // cheat 2 is for 1.0: not counted
    REQUIRE(hit(Id::CheatsOtherVersion, -1) != nullptr);
    CHECK(hit(Id::CheatsOtherVersion, -1)->r.H() >= 48.0f);
    for (int i = 0; i < 6; i++) {
        REQUIRE(hit(Id::Cheat, i) != nullptr);
        CHECK(hit(Id::Cheat, i)->r.y1 <= hit(Id::CheatsOtherVersion, -1)->r.y0 + 0.01f);
    }
    CHECK(hit(Id::Cheat, 6) == nullptr);
    CHECK(menu.MaxScroll() == 0.0f);
    CHECK_FALSE(menu.Layout().bar.Valid());

    // Open: the 119 under the header, in file order, each with its own (file) index.
    v.cheats_other_open = true;
    draw(v);
    REQUIRE(hit(Id::Cheat, 6) != nullptr);
    CHECK(hit(Id::Cheat, 6)->r.y0 >= hit(Id::CheatsOtherVersion, -1)->r.y1 - 0.01f);
    CHECK(menu.MaxScroll() > 100.0f * 48.0f);
    CHECK(menu.Layout().bar.Valid());
    v.scroll[static_cast<int>(Tab::Mods)] = 10000.0f;
    draw(v);
    CHECK(hit(Id::Cheat, 124) != nullptr); // the last one at the end
    CHECK(hit(Id::Cheat, 5) == nullptr);

    // Every cheat for another version: the note over the section, still closed.
    for (CheatItem& c : m.mods.cheats.cheats) {
        c.version_note = "for v1.5, this game is v1.0";
    }
    v = {};
    v.tab = Tab::Mods;
    draw(v);
    CHECK(drew(kAllOtherVersionNote));
    CHECK(drew("For Another Version (125 · 3 On)"));
    CHECK(hit(Id::Cheat, 0) == nullptr);
    CHECK(LabelFor(Id::CheatsOtherVersion) == "For Another Version");
}
