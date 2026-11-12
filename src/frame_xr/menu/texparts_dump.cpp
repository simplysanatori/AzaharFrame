// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame texparts: the Customize screens for the menu dump (texparts_dump.h). The models are
// what citra_qt/frame_mods.cpp makes from core/custom_tex/parts.h, written out by hand from real
// packs: Henriko Magnifico's Ocarina of Time 3D 4K (ASTC build, installed), Stella Glow HD,
// DeathWrench's MM3DHD 0.0.42b and HardDanX's Touchless Layout UI (Vanilla) (archive listings;
// the Touchless sizes are estimates).

#include "frame_xr/menu/texparts_dump.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <vector>

#include <lodepng.h>

#include "frame_xr/menu/mods_segments.h"
#include "frame_xr/menu/mods_tab.h"

namespace FrameXr::Menu::TexPartsDump {

namespace {

constexpr int kPreviewPx = 96; ///< as the Qt side's previews

std::string Count(uint64_t n, const char* one, const char* many) {
    std::string digits = std::to_string(n), out;
    for (std::size_t i = 0; i < digits.size(); i++) {
        if (i > 0 && (digits.size() - i) % 3 == 0) {
            out += ',';
        }
        out += digits[i];
    }
    return out + " " + (n == 1 ? one : many);
}

std::string Detail(uint64_t textures, uint64_t bytes, const std::string& conflicts = "") {
    return JoinDetail({Count(textures, "Texture", "Textures"), SizeText(bytes),
                       conflicts.empty() ? std::string() : "Conflicts with " + conflicts});
}

/// The part previews: the PNG's middle square, shrunk, corners rounded (as frame_mods.cpp).
struct Thumbs {
    std::string dir;
    std::map<std::string, std::shared_ptr<const std::vector<uint8_t>>> cache;

    std::shared_ptr<const std::vector<uint8_t>> Get(const std::string& key) {
        if (key.empty() || dir.empty()) {
            return {};
        }
        auto it = cache.find(key);
        if (it == cache.end()) {
            it = cache.emplace(key, Load(dir + "/" + key + ".png")).first;
        }
        return it->second;
    }

    static std::shared_ptr<const std::vector<uint8_t>> Load(const std::string& path) {
        std::vector<unsigned char> img;
        unsigned w = 0, h = 0;
        if (lodepng::decode(img, w, h, path) != 0 || w == 0 || h == 0) {
            return {};
        }
        const int n = kPreviewPx;
        const float radius = n * 8.0f / 40.0f;
        const double side = std::min(w, h);
        const double ox = (w - side) * 0.5, oy = (h - side) * 0.5;
        auto out = std::make_shared<std::vector<uint8_t>>(static_cast<std::size_t>(n) * n * 4);
        for (int y = 0; y < n; y++) {
            for (int x = 0; x < n; x++) {
                const int sx0 = static_cast<int>(ox + side * x / n);
                const int sx1 = std::max(sx0 + 1, static_cast<int>(ox + side * (x + 1) / n));
                const int sy0 = static_cast<int>(oy + side * y / n);
                const int sy1 = std::max(sy0 + 1, static_cast<int>(oy + side * (y + 1) / n));
                double acc[4] = {0, 0, 0, 0};
                int cnt = 0;
                for (int sy = sy0; sy < sy1 && sy < static_cast<int>(h); sy++) {
                    for (int sx = sx0; sx < sx1 && sx < static_cast<int>(w); sx++) {
                        const unsigned char* p = &img[(static_cast<std::size_t>(sy) * w + sx) * 4];
                        const double a = p[3] / 255.0;
                        acc[0] += p[0] * a;
                        acc[1] += p[1] * a;
                        acc[2] += p[2] * a;
                        acc[3] += a;
                        cnt++;
                    }
                }
                uint8_t* q = &(*out)[(static_cast<std::size_t>(y) * n + x) * 4];
                const double a = cnt ? acc[3] / cnt : 0.0;
                for (int i = 0; i < 3; i++) {
                    q[i] = static_cast<uint8_t>(acc[3] > 0 ? std::clamp(acc[i] / acc[3], 0.0, 255.0)
                                                           : 0);
                }
                const float px = x + 0.5f, py = y + 0.5f;
                const float cx = std::clamp(px, radius, n - radius);
                const float cy = std::clamp(py, radius, n - radius);
                const float cover =
                    std::clamp(radius - std::hypot(px - cx, py - cy) + 0.5f, 0.0f, 1.0f);
                q[3] = static_cast<uint8_t>(std::clamp(a * cover * 255.0, 0.0, 255.0));
            }
        }
        return out;
    }
};

struct Rows {
    Thumbs* thumbs;
    std::vector<TexturePartRow> rows;

    TexturePartRow& Add(PartRowKind kind, const std::string& name, const std::string& detail = {},
                        const std::string& thumb = {}, int value = 0) {
        TexturePartRow r;
        r.kind = kind;
        r.name = name;
        r.detail = detail;
        r.value = value;
        if (auto px = thumbs->Get(thumb)) {
            r.preview = std::move(px);
            r.preview_px = kPreviewPx;
        }
        rows.push_back(std::move(r));
        return rows.back();
    }
};

void Arrows(TexturePartRow& r, bool up, bool down) {
    r.arrows = true;
    r.up = up;
    r.down = down;
}

/// Henriko Magnifico's OoT3D 4K, ASTC build (its counts and sizes); UI off. EFFECTS and
/// ENVIRONMENT share a texture hash.
TexturePartsModel OotPage(Thumbs& t) {
    TexturePartsModel m;
    m.pack_id = "oot-4k";
    m.title = "4K";
    m.author = "Henriko Magnifico";
    m.rest = "6.0.0 · 2,034 Textures On · 5.0 GB";
    m.all_buttons = true;
    m.note = true;
    Rows r{&t};
    r.Add(PartRowKind::Check, "Characters", Detail(291, 262608424), "oot-CHARACTERS", 2);
    r.Add(PartRowKind::Check, "Creatures", Detail(81, 72280948), "oot-CREATURES", 2);
    Arrows(
        r.Add(PartRowKind::Check, "Effects", Detail(52, 73494804, "Environment"), "oot-EFFECTS", 2),
        true, true);
    Arrows(r.Add(PartRowKind::Check, "Environment", Detail(1434, 4496484836ull, "Effects"),
                 "oot-ENVIRONMENT", 2),
           true, true);
    r.Add(PartRowKind::Check, "Link", Detail(96, 79833336), "oot-LINK", 2);
    r.Add(PartRowKind::Check, "Maps", Detail(80, 42565292), "oot-MAPS", 2);
    r.Add(PartRowKind::Check, "UI", Detail(274, 1007027932), "oot-UI", 0);
    m.rows = std::move(r.rows);
    return m;
}

/// Stella Glow HD: no conflicts, so no order buttons; Characters and UI are folders of parts.
TexturePartsModel StellaPage(Thumbs& t) {
    TexturePartsModel m;
    m.pack_id = "sg-hd";
    m.title = "Stella Glow HD";
    m.author = "simplysanatori";
    m.rest = "1.0.0 · 5,013 Textures On · 3.2 GB";
    m.all_buttons = true;
    Rows r{&t};
    const auto part = [&r](const char* name, uint64_t n, uint64_t bytes, const char* thumb, int v,
                           bool child = false) {
        r.Add(PartRowKind::Check, name, Detail(n, bytes), thumb, v).child = child;
    };
    part("Backgrounds", 806, 1323126008, "sg-backgrounds", 2);
    r.Add(PartRowKind::Folder, "Characters", Detail(10914, 2311374131ull), "", 1);
    part("3D Models", 279, 178555557, "sg-3d", 2, true);
    part("Battle Portraits", 136, 113749005, "sg-battle-portraits", 2, true);
    part("Portraits", 795, 204743216, "sg-portraits", 2, true);
    part("Sprites", 9704, 1814326353, "sg-sprites", 0, true);
    part("Effects", 535, 110989425, "sg-effects", 2);
    part("Event CGs", 178, 551156940, "sg-cgs", 2);
    part("Maps", 662, 349513655, "sg-maps", 2);
    part("Misc", 3, 15407, "sg-misc", 2);
    r.Add(PartRowKind::Folder, "UI", Detail(1451, 333022144), "", 2);
    part("Battle", 624, 59914387, "sg-ui-battle", 2, true);
    part("Common", 451, 181668333, "sg-ui-common", 2, true);
    part("Dialogue Boxes", 30, 920238, "sg-ui-dialogue", 2, true);
    part("Menus", 181, 22980112, "sg-ui-menus", 2, true);
    part("Staff Roll", 54, 5406107, "sg-ui-staff", 2, true);
    part("System", 43, 2155531, "sg-ui-system", 2, true);
    part("Titles & Chapter Cards", 68, 59977436, "sg-ui-titles", 2, true);
    part("Weapons", 168, 33656722, "sg-weapons", 2);
    m.rows = std::move(r.rows);
    return m;
}

/// DeathWrench's MM3DHD 0.0.42b as one pack: the "AI Upscaled or WIP" folder, two choices
/// ([Fonts], [Link's Tunic]), [Link] Nerrel Style, and [Main] last (the base). [Link] Nerrel
/// Style and AI Upscaled or WIP/Other Link Textures share 10 hashes.
TexturePartsModel MmPage(Thumbs& t) {
    TexturePartsModel m;
    m.pack_id = "mm3dhd";
    m.title = "MM3D HD";
    m.author = "DeathWrench";
    m.rest = "0.0.42b · 209 Textures On · 73 MB";
    m.all_buttons = true;
    m.note = true;
    Rows r{&t};
    Arrows(r.Add(PartRowKind::Folder, "AI Upscaled or WIP",
                 Detail(119, 30973689, "Link: Nerrel Style"), "", 1),
           false, true);
    r.Add(PartRowKind::Check, "Map", Detail(37, 3355982), "mm-ai-map", 2).child = true;
    r.Add(PartRowKind::Check, "Other Link Textures", Detail(78, 23172816, "Link: Nerrel Style"),
          "mm-ai-link", 2)
        .child = true;
    r.Add(PartRowKind::Check, "wip", Detail(4, 4444891), "mm-ai-wip", 0).child = true;
    r.Add(PartRowKind::ChoiceTitle, "Fonts");
    r.Add(PartRowKind::ChoiceOff, "Off");
    r.Add(PartRowKind::Choice, "Original N64", Detail(1, 177980), "mm-font-n64", 2);
    r.Add(PartRowKind::Choice, "Original Pizza", Detail(1, 117365), "mm-font-pizza", 0);
    r.Add(PartRowKind::Choice, "Original WW (resembles 3DS)", Detail(1, 123279), "mm-font-ww", 0);
    r.Add(PartRowKind::ChoiceTitle, "Link's Tunic");
    r.Add(PartRowKind::ChoiceOff, "Off");
    r.Add(PartRowKind::Choice, "N64 Inspired", Detail(3, 770125), "mm-tunic-n64", 0);
    r.Add(PartRowKind::Choice, "Nerrel Style", Detail(3, 809766), "mm-tunic-nerrel", 2);
    Arrows(r.Add(PartRowKind::Check, "Link: Nerrel Style",
                 Detail(10, 252138, "Other Link Textures"), "mm-link-nerrel", 2),
           true, true);
    r.Add(PartRowKind::Check, "Main", Detail(80, 45393424), "mm-main", 2);
    m.rows = std::move(r.rows);
    return m;
}

/// HardDanX's Touchless Layout UI (Vanilla): one controller choice (the catalog's hints), the
/// names without the words they share; Unedited textures starts off.
TexturePartsModel TouchlessPage(Thumbs& t) {
    TexturePartsModel m;
    m.pack_id = "touchless";
    m.title = "Touchless Layout UI";
    m.author = "HardDanX";
    m.rest = "116 Textures On · 10 MB";
    Rows r{&t};
    r.Add(PartRowKind::ChoiceTitle, "Controller");
    r.Add(PartRowKind::ChoiceOff, "Off");
    r.Add(PartRowKind::Choice, "Dualshock N64 UI", Detail(116, 10 * 1000000ull), "", 2);
    r.Add(PartRowKind::Choice, "Dualshock UI", Detail(116, 10 * 1000000ull), "", 0);
    r.Add(PartRowKind::Choice, "Nintendo Colored UI", Detail(116, 10 * 1000000ull), "", 0);
    r.Add(PartRowKind::Choice, "Nintendo UI", Detail(105, 9 * 1000000ull), "", 0);
    r.Add(PartRowKind::Choice, "Unedited textures (Vanilla)", Detail(116, 8 * 1000000ull), "", 0);
    r.Add(PartRowKind::Choice, "Xbox N64 UI", Detail(116, 10 * 1000000ull), "", 0);
    r.Add(PartRowKind::Choice, "Xbox UI", Detail(116, 10 * 1000000ull), "", 0);
    m.rows = std::move(r.rows);
    return m;
}

} // namespace

bool Run(const std::string& thumbs_dir, const MenuModel& base, const Shot& shot) {
    Thumbs thumbs;
    thumbs.dir = thumbs_dir;
    MenuView v;
    v.tab = Tab::Mods;
    v.mods_segment = ModsSegment::Textures;
    MenuModel oot = base;
    oot.game_title = "The Legend of Zelda: Ocarina of Time 3D";
    TexturesModel& tx = oot.mods.textures;
    tx = {};
    tx.custom_textures = true;
    tx.packs = {
        {"oot-1080p", "1080p", "Henriko Magnifico · 6.0.0 · ASTC/KTX · 2.3 GB", false, false, true},
        {"oot-4k", "4K", "Henriko Magnifico · 6.0.0 · ASTC/KTX · 6.0 GB · 6 of 7 Parts", true,
         false, true},
        {"oot-4k-png", "4K (PNG)", "Henriko Magnifico · 6.0.0 · PNG · 4.7 GB", false, false, true}};
    bool ok = true;

    // a. The Textures segment: Customize on each pack of two or more parts.
    v.hover = {Id::PackCustomize, 1};
    ok &= shot("a1-textures-customize", oot, v);

    // b. A pack's Customize page.
    v.textures_page = TexturesPage::Parts;
    tx.parts = OotPage(thumbs);
    v.hover = {Id::PartDown, 2};
    v.tooltip = Id::PartDown;
    v.tooltip_index = 2;
    ok &= shot("b1-customize-oot3d-4k", oot, v);
    v.tooltip = Id::None;
    v.tooltip_index = -1;
    v.hover = {Id::TexturePart, 6};
    v.scroll[static_cast<std::size_t>(Tab::Mods)] = -1.0f;
    tx.needs_restart = true;
    ok &= shot("b2-customize-oot3d-4k-restart", oot, v);
    tx.needs_restart = false;
    MenuModel sg = oot;
    sg.game_title = "Stella Glow";
    sg.mods.textures.parts = StellaPage(thumbs);
    v.hover = {Id::TexturePart, 5};
    v.scroll[static_cast<std::size_t>(Tab::Mods)] = 0.0f;
    ok &= shot("b3-customize-stella-glow", sg, v);
    v.hover = {};
    v.scroll[static_cast<std::size_t>(Tab::Mods)] = -1.0f;
    ok &= shot("b4-customize-stella-glow-end", sg, v);

    // c. Variants: choices in a pack; a pack that is only a choice.
    MenuModel mm = oot;
    mm.game_title = "The Legend of Zelda: Majora's Mask 3D";
    mm.mods.textures.parts = MmPage(thumbs);
    v.hover = {Id::PartDown, 0};
    v.tooltip = Id::PartDown;
    v.tooltip_index = 0;
    v.scroll[static_cast<std::size_t>(Tab::Mods)] = 0.0f;
    ok &= shot("c1-variants-mm3dhd", mm, v);
    v.hover = {Id::PartUp, 13};
    v.tooltip = Id::PartUp;
    v.tooltip_index = 13;
    v.scroll[static_cast<std::size_t>(Tab::Mods)] = -1.0f;
    ok &= shot("c2-variants-mm3dhd-end", mm, v);
    v.tooltip = Id::None;
    v.tooltip_index = -1;
    MenuModel tl = oot;
    tl.mods.textures.parts = TouchlessPage(thumbs);
    v.hover = {Id::TexturePart, 3};
    v.scroll[static_cast<std::size_t>(Tab::Mods)] = 0.0f;
    ok &= shot("c3-variants-touchless-layout", tl, v);
    return ok;
}

} // namespace FrameXr::Menu::TexPartsDump
