// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kbd: draws the game's keyboard (swkbd_model.h, swkbd_view.h) with the real code into
// PNG files, in a few states, for design reviews without a headset.
//
//   frame_xr_swkbd_dump OUT_DIR [--px-per-dp N]

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <lodepng.h>

#include "frame_xr/menu/md3.h"
#include "frame_xr/menu/swkbd_view.h"

using namespace FrameXr;
using namespace FrameXr::Swkbd;

namespace {

constexpr float kMargin = 24.0f; // dp of backdrop round the panel
constexpr uint32_t kBackdrop = 0x2A2D31;

bool WritePng(const std::string& path, const frame_xr::ui::Canvas& cv) {
    std::vector<unsigned char> rgba(static_cast<std::size_t>(cv.w) * cv.h * 4);
    for (std::size_t i = 0; i < cv.px.size(); i++) {
        const uint32_t p = cv.px[i];
        rgba[i * 4 + 0] = static_cast<unsigned char>(p & 255);
        rgba[i * 4 + 1] = static_cast<unsigned char>((p >> 8) & 255);
        rgba[i * 4 + 2] = static_cast<unsigned char>((p >> 16) & 255);
        rgba[i * 4 + 3] = 255;
    }
    const unsigned err =
        lodepng::encode(path, rgba, static_cast<unsigned>(cv.w), static_cast<unsigned>(cv.h));
    if (err) {
        std::fprintf(stderr, "%s: %s\n", path.c_str(), lodepng_error_text(err));
        return false;
    }
    std::printf("%s\n", path.c_str());
    return true;
}

Spec NameSpec() {
    Spec s;
    s.hint = "Enter your name.";
    s.max_length = 8;
    s.buttons = {{"Cancel", 0, false}, {"OK", 1, true}};
    s.validate = [](const std::string& t) {
        return t.find_first_not_of(' ') == std::string::npos
                   ? std::string{"Blank input is not allowed"}
                   : std::string{};
    };
    s.filter = [](const std::string& t) {
        return t.find('@') != std::string::npos ? std::string{"@ is not allowed"} : std::string{};
    };
    return s;
}

int KeyIndex(const Model& m, char32_t ch) {
    const auto& keys = m.GetLayout().keys;
    for (int i = 0; i < static_cast<int>(keys.size()); i++) {
        if (keys[static_cast<std::size_t>(i)].kind == KeyKind::Char &&
            keys[static_cast<std::size_t>(i)].ch == ch) {
            return i;
        }
    }
    return -1;
}

int KindIndex(const Model& m, KeyKind kind, int nth = 0) {
    const auto& keys = m.GetLayout().keys;
    for (int i = 0; i < static_cast<int>(keys.size()); i++) {
        if (keys[static_cast<std::size_t>(i)].kind == kind && nth-- == 0) {
            return i;
        }
    }
    return -1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s OUT_DIR [--px-per-dp N]\n", argv[0]);
        return 2;
    }
    const std::string out = argv[1];
    float px_per_dp = 1.75f; // the swapchain's own scale (menu_panel.cpp)
    for (int i = 2; i + 1 < argc; i++) {
        if (std::string{argv[i]} == "--px-per-dp") {
            px_per_dp = static_cast<float>(std::atof(argv[i + 1]));
        }
    }
    Menu::Kit kit(px_per_dp);
    const int w = static_cast<int>((kPanelW + kMargin * 2.0f) * px_per_dp + 0.5f);
    const int h = static_cast<int>((kPanelH + kMargin * 2.0f) * px_per_dp + 0.5f);
    const auto shot = [&](const std::string& name, const Model& m, int hover, int pressed) {
        frame_xr::ui::Canvas cv;
        cv.Init(w, h);
        cv.Clear(Menu::Rgb(kBackdrop));
        frame_xr::ui::Canvas panel;
        panel.Init(static_cast<int>(kPanelW * px_per_dp + 0.5f),
                   static_cast<int>(kPanelH * px_per_dp + 0.5f));
        kit.Begin(panel);
        Draw(kit, m, hover, pressed);
        const int ox = static_cast<int>(kMargin * px_per_dp + 0.5f);
        for (int y = 0; y < panel.h; y++) {
            for (int x = 0; x < panel.w; x++) {
                const uint32_t p = panel.px[static_cast<std::size_t>(y) * panel.w + x];
                const float a = static_cast<float>(p >> 24) / 255.0f;
                if (a <= 0.0f) {
                    continue;
                }
                cv.Blend(x + ox, y + ox,
                         {static_cast<float>(p & 255) / a, static_cast<float>((p >> 8) & 255) / a,
                          static_cast<float>((p >> 16) & 255) / a, 1.0f},
                         a);
            }
        }
        return WritePng(out + "/" + name + ".png", cv);
    };

    bool ok = true;
    {
        Model m(NameSpec()); // just opened: Shift once for the first capital
        ok &= shot("1-name-empty", m, KeyIndex(m, U'L'), -1);
    }
    {
        Model m(NameSpec());
        m.InsertText("Lin");
        const int k = KeyIndex(m, U'k');
        ok &= shot("2-name-typing", m, k, k);
    }
    {
        Model m(NameSpec());
        m.InsertText("LINK");
        m.ToggleShift();
        m.ToggleShift(); // Off -> Once -> Locked
        ok &= shot("3-shift-locked", m, KindIndex(m, KeyKind::Shift), -1);
    }
    {
        Model m(NameSpec());
        m.InsertText("Link");
        m.SetPage(Page::Symbols);
        ok &= shot("4-symbols", m, KeyIndex(m, U'#'), -1);
    }
    {
        Model m(NameSpec());
        m.InsertText("   ");
        m.PressButton(1); // OK: blank text is refused
        ok &= shot("5-validation-error", m, KindIndex(m, KeyKind::Button, 1), -1);
    }
    {
        Model m(NameSpec());
        m.InsertText("Link");
        m.InsertText("@"); // a filter refuses the key
        ok &= shot("5b-filter-error", m, -1, -1);
    }
    {
        Model m(NameSpec());
        m.InsertText("Kafei123"); // at the limit
        ok &= shot("5c-full", m, KindIndex(m, KeyKind::Button, 1), -1);
    }
    {
        Spec s;
        s.max_length = 16;
        s.buttons = {{"Cancel", 0, false}, {"I Forgot", 1, false}, {"Ok", 2, true}};
        Model m(s); // no hint, three buttons with Azahar's labels
        m.InsertText("hunter2");
        ok &= shot("6-three-buttons-no-hint", m, KindIndex(m, KeyKind::Button, 2), -1);
    }
    {
        Spec s;
        s.hint = "Write a message to your friend.";
        s.max_length = 120;
        s.multiline = true;
        s.buttons = {{"Cancel", 0, false}, {"Send", 1, true}};
        Model m(s);
        m.InsertText(
            "Meet me at the Clock Town south gate at dawn of the first day.\nBring the mask.");
        ok &= shot("7-multiline", m, KindIndex(m, KeyKind::Enter), -1);
    }
    {
        Spec s;
        s.hint = "Enter the code.";
        s.max_length = 4;
        s.numpad = true;
        s.buttons = {{"OK", 0, true}};
        Model m(s);
        m.InsertText("2026");
        ok &= shot("8-numpad", m, KeyIndex(m, U'5'), -1);
    }
    {
        Spec s = NameSpec();
        s.error = "That name cannot be used.";
        Model m(s); // the game's own message from its check, at the start
        ok &= shot("9-game-message", m, -1, -1);
    }
    return ok ? 0 : 1;
}
