// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// See art.h. The shapes (anti-aliased by their signed distance, composited source-over in sRGB
// space into premultiplied pixels), the text (stb_truetype at the exact size, a quarter pixel
// phase, kerning, coverage through a 0.8 gamma) and every size and colour are Moonlight XR's.

#include "frame_xr/keyboard/art.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>

#include "common/logging/log.h"
#include "frame_xr/keyboard/layout.h"
#include "frame_xr/keyboard/stb_truetype.h"
#include "frame_xr/picker/logo_loader.h"
#include "frame_xr/ui/ui_canvas.h"

namespace FrameXr::Keyboard {

namespace {

float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ---- The canvas (Moonlight XR ui/canvas.c) -------------------------------------------------

struct Canvas {
    uint8_t* px = nullptr; // w * h * 4 bytes, R G B A, premultiplied
    int w = 0, h = 0;
    int cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0; // clip, [x0, x1) [y0, y1)

    void ResetClip() {
        cx0 = 0;
        cy0 = 0;
        cx1 = w;
        cy1 = h;
    }
    void SetClip(float x0, float y0, float x1, float y1) {
        cx0 = std::max(0, static_cast<int>(std::floor(x0)));
        cy0 = std::max(0, static_cast<int>(std::floor(y0)));
        cx1 = std::min(w, static_cast<int>(std::ceil(x1)));
        cy1 = std::min(h, static_cast<int>(std::ceil(y1)));
    }
    // One pixel covered by cov (0..1) in a 0xAARRGGBB colour of straight alpha: source-over.
    void Over(int x, int y, uint32_t argb, float cov) {
        if (x < cx0 || x >= cx1 || y < cy0 || y >= cy1 || cov <= 0.0f) {
            return;
        }
        const float a = (static_cast<float>(argb >> 24) * (1.0f / 255.0f)) * (cov > 1.0f ? 1.0f : cov);
        if (a <= 0.0f) {
            return;
        }
        uint8_t* d = px + (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                           static_cast<std::size_t>(x)) *
                              4;
        const float inv = 1.0f - a;
        const float r = static_cast<float>((argb >> 16) & 0xFF) * a;
        const float g = static_cast<float>((argb >> 8) & 0xFF) * a;
        const float b = static_cast<float>(argb & 0xFF) * a;
        const float na = a * 255.0f;
        const int ro = static_cast<int>(r + static_cast<float>(d[0]) * inv + 0.5f);
        const int go = static_cast<int>(g + static_cast<float>(d[1]) * inv + 0.5f);
        const int bo = static_cast<int>(b + static_cast<float>(d[2]) * inv + 0.5f);
        const int ao = static_cast<int>(na + static_cast<float>(d[3]) * inv + 0.5f);
        d[0] = static_cast<uint8_t>(std::min(ro, 255));
        d[1] = static_cast<uint8_t>(std::min(go, 255));
        d[2] = static_cast<uint8_t>(std::min(bo, 255));
        d[3] = static_cast<uint8_t>(std::min(ao, 255));
    }
};

// Visits every pixel of a box, padded for the anti-aliased fringe, inside the clip.
template <class F>
void Box(const Canvas& cv, float x0, float y0, float x1, float y1, float pad, F&& f) {
    const int bx0 = std::max(cv.cx0, static_cast<int>(std::floor(x0 - pad)));
    const int by0 = std::max(cv.cy0, static_cast<int>(std::floor(y0 - pad)));
    const int bx1 = std::min(cv.cx1, static_cast<int>(std::ceil(x1 + pad)));
    const int by1 = std::min(cv.cy1, static_cast<int>(std::ceil(y1 + pad)));
    for (int py = by0; py < by1; py++) {
        for (int px = bx0; px < bx1; px++) {
            f(px, py);
        }
    }
}

// Signed distance to a rounded rectangle (negative inside).
float SdRRect(float px, float py, float x0, float y0, float x1, float y1, float r) {
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    const float hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f;
    r = std::max(0.0f, std::min({r, hx, hy}));
    const float qx = std::fabs(px - cx) - (hx - r);
    const float qy = std::fabs(py - cy) - (hy - r);
    const float ox = qx > 0.0f ? qx : 0.0f, oy = qy > 0.0f ? qy : 0.0f;
    float inside = qx > qy ? qx : qy;
    if (inside > 0.0f) {
        inside = 0.0f;
    }
    return std::sqrt(ox * ox + oy * oy) + inside - r;
}

void FillRRect(Canvas& cv, float x0, float y0, float x1, float y1, float r, uint32_t argb) {
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    Box(cv, x0, y0, x1, y1, 1.0f, [&](int px, int py) {
        const float d = SdRRect(px + 0.5f, py + 0.5f, x0, y0, x1, y1, r);
        cv.Over(px, py, argb, Clamp(0.5f - d, 0.0f, 1.0f));
    });
}

void StrokeRRect(Canvas& cv, float x0, float y0, float x1, float y1, float r, float width,
                 uint32_t argb) {
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    const float hw = width * 0.5f;
    Box(cv, x0, y0, x1, y1, hw + 1.0f, [&](int px, int py) {
        const float d = SdRRect(px + 0.5f, py + 0.5f, x0, y0, x1, y1, r);
        cv.Over(px, py, argb, Clamp(0.5f - (std::fabs(d) - hw), 0.0f, 1.0f));
    });
}

void FillCircle(Canvas& cv, float cx, float cy, float r, uint32_t argb) {
    Box(cv, cx - r, cy - r, cx + r, cy + r, 1.0f, [&](int px, int py) {
        const float d = std::hypot(px + 0.5f - cx, py + 0.5f - cy) - r;
        cv.Over(px, py, argb, Clamp(0.5f - d, 0.0f, 1.0f));
    });
}

float DistSeg(float px, float py, float ax, float ay, float bx, float by) {
    const float dx = bx - ax, dy = by - ay;
    const float l2 = dx * dx + dy * dy;
    float t = l2 > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0.0f;
    t = Clamp(t, 0.0f, 1.0f);
    return std::hypot(px - (ax + dx * t), py - (ay + dy * t));
}

// Open polyline of n points (x, y pairs), round caps and joins, one composite.
void Polyline(Canvas& cv, const float* xy, int n, float width, uint32_t argb) {
    if (n < 2) {
        return;
    }
    const float hw = width * 0.5f;
    float minx = xy[0], maxx = xy[0], miny = xy[1], maxy = xy[1];
    for (int i = 1; i < n; i++) {
        minx = std::min(minx, xy[2 * i]);
        maxx = std::max(maxx, xy[2 * i]);
        miny = std::min(miny, xy[2 * i + 1]);
        maxy = std::max(maxy, xy[2 * i + 1]);
    }
    Box(cv, minx - hw, miny - hw, maxx + hw, maxy + hw, 1.0f, [&](int px, int py) {
        const float fx = px + 0.5f, fy = py + 0.5f;
        float best = 1e30f;
        for (int i = 0; i + 1 < n; i++) {
            best = std::min(best, DistSeg(fx, fy, xy[2 * i], xy[2 * i + 1], xy[2 * i + 2],
                                          xy[2 * i + 3]));
        }
        cv.Over(px, py, argb, Clamp(0.5f - (best - hw), 0.0f, 1.0f));
    });
}

void Line(Canvas& cv, float x0, float y0, float x1, float y1, float width, uint32_t argb) {
    const float p[4] = {x0, y0, x1, y1};
    Polyline(cv, p, 2, width, argb);
}

// ---- Text (Moonlight XR ui/text.c, one face, ASCII) ------------------------------------------

constexpr int kSubpixel = 4;
constexpr float kTextGamma = 0.80f;

class Font {
public:
    bool Load(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            return false;
        }
        data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        if (data.size() < 12) {
            data.clear();
            return false;
        }
        const int offset = stbtt_GetFontOffsetForIndex(data.data(), 0);
        if (offset < 0 || !stbtt_InitFont(&info, data.data(), offset)) {
            data.clear();
            return false;
        }
        for (int i = 0; i < 256; i++) {
            lut[i] = static_cast<uint8_t>(255.0f * std::pow(i / 255.0f, kTextGamma) + 0.5f);
        }
        ok = true;
        return true;
    }
    bool Ok() const {
        return ok;
    }

    float Width(float size, const char* s) const {
        float x = 0.0f;
        for (const char* p = s; *p; p++) {
            x += Advance(size, static_cast<uint8_t>(p[0]), static_cast<uint8_t>(p[1]));
        }
        return x;
    }

    float CapHeight(float size) const {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBox(&info, stbtt_FindGlyphIndex(&info, 'H'), &x0, &y0, &x1, &y1);
        return static_cast<float>(y1) * stbtt_ScaleForMappingEmToPixels(&info, size);
    }

    // align: 0 left of x, 1 centred on it.
    void Draw(Canvas& cv, float size, float x, float baseline, int align, uint32_t argb,
              const char* s) {
        const float total = Width(size, s);
        float pen = align == 1 ? x - total * 0.5f : x;
        const int size_q = std::max(8, static_cast<int>(size * 8.0f + 0.5f));
        const int by = static_cast<int>(std::floor(baseline + 0.5f));
        for (const char* p = s; *p; p++) {
            const uint8_t c = static_cast<uint8_t>(p[0]);
            const float adv = Advance(size, c, static_cast<uint8_t>(p[1]));
            if (c > ' ') {
                int ix = static_cast<int>(std::floor(pen));
                int phase = static_cast<int>((pen - static_cast<float>(ix)) * kSubpixel + 0.5f);
                if (phase >= kSubpixel) {
                    phase = 0;
                    ix++;
                }
                const Glyph& g = GlyphFor(stbtt_FindGlyphIndex(&info, c), size_q, phase);
                for (int yy = 0; yy < g.h; yy++) {
                    for (int xx = 0; xx < g.w; xx++) {
                        const uint8_t a = g.coverage[static_cast<std::size_t>(yy * g.w + xx)];
                        if (a) {
                            cv.Over(ix + g.bx + xx, by + g.by + yy, argb, a * (1.0f / 255.0f));
                        }
                    }
                }
            }
            pen += adv;
        }
    }

private:
    struct Glyph {
        int w = 0, h = 0, bx = 0, by = 0;
        std::vector<uint8_t> coverage;
    };

    // The advance of c followed by next (0 for none), kerned, in pixels.
    float Advance(float size, uint8_t c, uint8_t next) const {
        if (c < ' ') {
            return 0.0f;
        }
        const int g = stbtt_FindGlyphIndex(&info, c);
        int adv = 0, lsb = 0;
        stbtt_GetGlyphHMetrics(&info, g, &adv, &lsb);
        float a = static_cast<float>(adv);
        if (next >= ' ') {
            a += static_cast<float>(stbtt_GetGlyphKernAdvance(&info, g, stbtt_FindGlyphIndex(&info, next)));
        }
        return a * stbtt_ScaleForMappingEmToPixels(&info, size);
    }

    const Glyph& GlyphFor(int glyph, int size_q, int phase) {
        const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(glyph)) << 24) |
                             (static_cast<uint64_t>(size_q) << 4) | static_cast<uint64_t>(phase);
        const auto found = cache.find(key);
        if (found != cache.end()) {
            return found->second;
        }
        Glyph& out = cache[key];
        const float scale = stbtt_ScaleForMappingEmToPixels(&info, size_q / 8.0f);
        const float shift = static_cast<float>(phase) / kSubpixel;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBoxSubpixel(&info, glyph, scale, scale, shift, 0.0f, &x0, &y0, &x1, &y1);
        const int bw = x1 - x0, bh = y1 - y0;
        if (bw <= 0 || bh <= 0 || bw > 250 || bh > 250) {
            return out; // nothing to draw (a space) or absurd
        }
        out.w = bw;
        out.h = bh;
        out.bx = x0;
        out.by = y0;
        out.coverage.assign(static_cast<std::size_t>(bw * bh), 0);
        stbtt_MakeGlyphBitmapSubpixel(&info, out.coverage.data(), bw, bh, bw, scale, scale, shift,
                                      0.0f, glyph);
        for (uint8_t& a : out.coverage) {
            a = lut[a];
        }
        return out;
    }

    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    bool ok = false;
    uint8_t lut[256]{};
    std::unordered_map<uint64_t, Glyph> cache;
};

// Noto Sans Regular, Moonlight XR's: a file named by AZAHARFRAME_KEYBOARD_FONT, one in the art
// folder, else SteamOS's own (the noto-fonts package) or another distribution's.
std::string FindFont() {
    if (const char* env = std::getenv("AZAHARFRAME_KEYBOARD_FONT"); env && *env) {
        return env;
    }
    if (std::string art = frame_xr::picker::FindArtFile("NotoSans-Regular.ttf"); !art.empty()) {
        return art;
    }
    for (const char* path : {"/usr/share/fonts/noto/NotoSans-Regular.ttf",
                             "/usr/share/fonts/TTF/NotoSans-Regular.ttf",
                             "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
                             "/usr/share/fonts/google-noto/NotoSans-Regular.ttf"}) {
        if (std::ifstream(path).good()) {
            return path;
        }
    }
    return {};
}

// What a "@" token says (the arrows stay tokens: they are drawn).
const char* Resolve(const char* label) {
    if (!label || label[0] != '@') {
        return label;
    }
    static const struct {
        const char* token;
        const char* text;
    } kWords[] = {{"@shift", "Shift"}, {"@space", "space"}, {"@enter", "Enter"}, {"@hide", "Hide"},
                  {"@ctrl", "Ctrl"},   {"@alt", "Alt"},     {"@win", "Win"}};
    for (const auto& w : kWords) {
        if (std::strcmp(label, w.token) == 0) {
            return w.text;
        }
    }
    return label;
}

void Arrow(Canvas& cv, const char* token, float cx, float cy, uint32_t ink) {
    float dx = 0.0f, dy = 0.0f;
    if (!std::strcmp(token, "@up")) {
        dy = -1.0f;
    } else if (!std::strcmp(token, "@down")) {
        dy = 1.0f;
    } else if (!std::strcmp(token, "@left")) {
        dx = -1.0f;
    } else {
        dx = 1.0f;
    }
    const float L = 13.0f, H = 9.0f; // half length, half head
    const float x0 = cx - dx * L, y0 = cy - dy * L, x1 = cx + dx * L, y1 = cy + dy * L;
    Line(cv, x0, y0, x1, y1, 3.5f, ink);
    const float px = -dy, py = dx; // perpendicular
    const float head[6] = {x1 - dx * H + px * H, y1 - dy * H + py * H, x1, y1,
                           x1 - dx * H - px * H, y1 - dy * H - py * H};
    Polyline(cv, head, 3, 3.5f, ink);
}

// Soft edged coverage for a distance from a shape, in pixels (Moonlight XR xr_assets.c).
float EdgeAlpha(float distance, float half_stroke) {
    return Clamp((half_stroke - distance) / 1.5f + 0.5f, 0.0f, 1.0f);
}

} // namespace

struct Art::Impl {
    frame_xr::ui::Canvas store; // the pixels (premultiplied RGBA8, R in the low byte)
    std::vector<uint32_t> sheet; // the dark sheet alone, for drawing keys again
    Font font;
    View last;
    bool drawn = false;

    Canvas Cv() {
        Canvas cv;
        cv.px = reinterpret_cast<uint8_t*>(store.px.data());
        cv.w = store.w;
        cv.h = store.h;
        cv.ResetClip();
        return cv;
    }

    float TextWidth(float size, const char* s) {
        return font.Ok() ? font.Width(size, s) : frame_xr::ui::TextWidth(frame_xr::ui::kFontSmall, s);
    }

    // A line centred on cy by its cap height, centred on cx (align 1) or from it (align 0).
    void Text(Canvas& cv, float size, float x, float cy, int align, uint32_t argb, const char* s) {
        if (font.Ok()) {
            font.Draw(cv, size, x, cy + font.CapHeight(size) * 0.5f, align, argb, s);
            return;
        }
        // The picker's baked font (Inter Bold, 28 px), when Noto Sans is nowhere.
        const frame_xr::ui::Color c{static_cast<float>((argb >> 16) & 0xFF),
                                    static_cast<float>((argb >> 8) & 0xFF),
                                    static_cast<float>(argb & 0xFF), (argb >> 24) / 255.0f};
        store.Text(x, cy + 10.0f, frame_xr::ui::kFontSmall, s, c, align);
    }

    // The lit pill at the left end of a key, for the modifiers held.
    void KeyPill(Canvas& cv, float x0, float y0, float y1, const char* text) {
        const float tw = TextWidth(20.0f, text) + 20.0f;
        const float th = (y1 - y0) - 16.0f;
        FillRRect(cv, x0 + 8, y0 + 8, x0 + 8 + tw, y0 + 8 + th, 8.0f, 0xEEFFFFFFu);
        Text(cv, 20.0f, x0 + 18, y0 + 8 + th * 0.5f, 0, 0xFF141416u, text);
    }

    void DrawKey(Canvas& cv, const View& v, int i, const std::string& held) {
        const char* label = Resolve(KeyLabel(v.sheet, i));
        if (!label) {
            return; // a blank is no key
        }
        const Rect r = KeyRect(i);
        const float x0 = r.x0 * kTexWidth, y0 = r.y0 * kTexHeight;
        const float x1 = r.x1 * kTexWidth, y1 = r.y1 * kTexHeight;
        const int code = KeyCode(v.sheet, i);
        const int bit = ModifierBit(code);
        // A modifier held down to lock is drawn in blue with an underline and a dot.
        const bool locked = (bit && (v.locked & bit)) || (code == kCodeShift && (v.locked & kModShift));
        const bool lit = locked || (v.mods & bit) != 0;
        const bool hot = v.hover == i;
        FillRRect(cv, x0, y0, x1, y1, 10.0f,
                  locked ? 0xEE9CC9FFu : lit ? 0xEEFFFFFFu : hot ? 0x44FFFFFFu : 0x28FFFFFFu);
        StrokeRRect(cv, x0, y0, x1, y1, 10.0f, 2.0f,
                    locked ? 0xFFCFE4FFu : lit ? 0xFFFFFFFFu : hot ? 0x90FFFFFFu : 0x50FFFFFFu);
        const uint32_t ink = lit ? 0xFF141416u : 0xFFFFFFFFu;
        const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
        if (label[0] == '@') {
            Arrow(cv, label, cx, cy, ink);
        } else {
            // A single character is what the key types, so it gets the room.
            Text(cv, std::strlen(label) == 1 ? 34.0f : 22.0f, cx, cy, 1, ink, label);
        }
        if (locked) {
            Line(cv, x0 + 12.0f, y1 - 9.0f, x1 - 12.0f, y1 - 9.0f, 4.0f, ink);
            FillCircle(cv, x1 - 10.0f, y0 + 10.0f, 4.0f, ink);
        }
        if (code == 32 && !held.empty()) {
            KeyPill(cv, x0, y0, y1, held.c_str()); // the held modifiers, on the space bar
        }
    }

    // The modifiers lit, said on the space bar on the sheets without their keys.
    static std::string Held(const View& v) {
        for (int i = 0; i < kKeyCount; i++) {
            if (ModifierBit(KeyCode(v.sheet, i)) != 0) {
                return {}; // the keys show it themselves
            }
        }
        std::string held;
        static const struct {
            int bit;
            const char* name;
        } kNames[] = {{kModCtrl, "Ctrl"}, {kModAlt, "Alt"}, {kModWin, "Win"}};
        for (const auto& n : kNames) {
            if (v.mods & n.bit) {
                if (!held.empty()) {
                    held += ' ';
                }
                held += n.name;
            }
        }
        return held;
    }

    void DrawAll(const View& v) {
        std::copy(sheet.begin(), sheet.end(), store.px.begin());
        Canvas cv = Cv();
        const std::string held = Held(v);
        for (int i = 0; i < kKeyCount; i++) {
            DrawKey(cv, v, i, held);
        }
    }

    // One key again, over the sheet, inside its own box (the gaps keep the neighbours clear).
    void Redraw(const View& v, int i) {
        if (i < 0 || i >= kKeyCount) {
            return;
        }
        const Rect r = KeyRect(i);
        const int x0 = std::max(0, static_cast<int>(std::floor(r.x0 * kTexWidth - 2.5f)));
        const int y0 = std::max(0, static_cast<int>(std::floor(r.y0 * kTexHeight - 2.5f)));
        const int x1 = std::min(kTexWidth, static_cast<int>(std::ceil(r.x1 * kTexWidth + 2.5f)));
        const int y1 = std::min(kTexHeight, static_cast<int>(std::ceil(r.y1 * kTexHeight + 2.5f)));
        for (int y = y0; y < y1; y++) {
            const std::size_t row = static_cast<std::size_t>(y) * kTexWidth;
            std::copy(sheet.begin() + static_cast<std::ptrdiff_t>(row + x0),
                      sheet.begin() + static_cast<std::ptrdiff_t>(row + x1),
                      store.px.begin() + static_cast<std::ptrdiff_t>(row + x0));
        }
        Canvas cv = Cv();
        cv.SetClip(static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1),
                   static_cast<float>(y1));
        DrawKey(cv, v, i, Held(v));
    }
};

Art::Art() : impl(std::make_unique<Impl>()) {
    impl->store.Init(kTexWidth, kTexHeight);
    std::fill(impl->store.px.begin(), impl->store.px.end(), 0u);
    Canvas cv = impl->Cv();
    // The dark sheet every panel of Moonlight XR sits on: inset a pixel, corners of 32.
    FillRRect(cv, 1.0f, 1.0f, kTexWidth - 1.0f, kTexHeight - 1.0f, 32.0f, 0xF0141416u);
    impl->sheet = impl->store.px;

    const std::string path = FindFont();
    if (!path.empty() && impl->font.Load(path)) {
        LOG_INFO(Frontend, "xr: keyboard font {}", path);
    } else {
        LOG_WARNING(Frontend, "xr: keyboard: Noto Sans not found, the keys use the picker's font");
        frame_xr::ui::InitFonts();
    }
}

Art::~Art() = default;

bool Art::Draw(const View& view) {
    View v = view;
    if (v.sheet < 0 || v.sheet >= kSheetCount) {
        v.sheet = kSheetLower;
    }
    if (impl->drawn && v == impl->last) {
        return false;
    }
    const View& was = impl->last;
    if (impl->drawn && v.sheet == was.sheet && v.mods == was.mods && v.locked == was.locked) {
        impl->Redraw(v, was.hover);
        impl->Redraw(v, v.hover);
    } else {
        impl->DrawAll(v);
    }
    impl->last = v;
    impl->drawn = true;
    return true;
}

const uint8_t* Art::Pixels() const {
    return reinterpret_cast<const uint8_t*>(impl->store.px.data());
}

std::vector<uint8_t> Art::RingArt() {
    // A rounded rectangle's border (Moonlight XR's outline art): marks the key under the ray.
    std::vector<uint8_t> px(static_cast<std::size_t>(kRingSize * kRingSize * 4));
    const float radius = 16.0f;
    const float border = 2.5f;
    const float half = kRingSize * 0.5f;
    for (int y = 0; y < kRingSize; y++) {
        for (int x = 0; x < kRingSize; x++) {
            const float qx = std::fabs(x + 0.5f - half) - (half - radius);
            const float qy = std::fabs(y + 0.5f - half) - (half - radius);
            const float mx = qx > 0.0f ? qx : 0.0f;
            const float my = qy > 0.0f ? qy : 0.0f;
            const float outside = std::sqrt(mx * mx + my * my);
            const float inside = std::min(std::max(qx, qy), 0.0f);
            const float dist = std::fabs(outside + inside);
            const auto a = static_cast<uint8_t>(EdgeAlpha(dist, border) * 255.0f);
            uint8_t* p = &px[static_cast<std::size_t>((y * kRingSize + x) * 4)];
            p[0] = p[1] = p[2] = p[3] = a;
        }
    }
    return px;
}

std::vector<uint8_t> Art::PillArt() {
    // A rounded bar (Moonlight XR's move pill).
    std::vector<uint8_t> px(static_cast<std::size_t>(kPillWidth * kPillHeight * 4));
    const float bar_r = kPillHeight * 0.5f;
    for (int y = 0; y < kPillHeight; y++) {
        for (int x = 0; x < kPillWidth; x++) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            const float cx = Clamp(fx, bar_r, kPillWidth - bar_r);
            const float d = std::hypot(fx - cx, fy - bar_r);
            const auto a = static_cast<uint8_t>(EdgeAlpha(d, bar_r - 1.0f) * 235.0f);
            uint8_t* p = &px[static_cast<std::size_t>((y * kPillWidth + x) * 4)];
            p[0] = p[1] = p[2] = p[3] = a;
        }
    }
    return px;
}

} // namespace FrameXr::Keyboard
