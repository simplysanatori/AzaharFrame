// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <unordered_map>

#include "common/logging/log.h"
#include "frame_xr/keyboard/stb_truetype.h" // the implementation is in keyboard/stb_truetype.cpp
#include "frame_xr/picker/logo_loader.h"    // FindArtFile
#include "frame_xr/vis/panel_art.h"

namespace FrameXr::Vis {

namespace {

frame_xr::ui::Color ToColor(uint32_t argb) {
    return {static_cast<float>((argb >> 16) & 0xFF), static_cast<float>((argb >> 8) & 0xFF),
            static_cast<float>(argb & 0xFF), static_cast<float>(argb >> 24) / 255.0f};
}

// The next code point of a UTF-8 string (invalid bytes come out as '?').
uint32_t NextCodePoint(const std::string& s, std::size_t& i) {
    const auto c = static_cast<uint8_t>(s[i++]);
    if (c < 0x80) {
        return c;
    }
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : -1;
    if (extra < 0) {
        return '?';
    }
    uint32_t cp = c & (0x3Fu >> extra);
    while (extra-- > 0) {
        if (i >= s.size() || (static_cast<uint8_t>(s[i]) & 0xC0) != 0x80) {
            return '?';
        }
        cp = (cp << 6) | (static_cast<uint8_t>(s[i++]) & 0x3Fu);
    }
    return cp;
}

// Text coverage a little heavier, as the keyboard's (art.cpp kTextGamma).
constexpr float kTextGamma = 0.80f;

class Font {
public:
    bool Load(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            return false;
        }
        data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        const int offset = data.size() > 12 ? stbtt_GetFontOffsetForIndex(data.data(), 0) : -1;
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

    float Width(float size, const std::string& s) const {
        const float scale = stbtt_ScaleForMappingEmToPixels(&info, size);
        float x = 0.0f;
        int prev = 0;
        for (std::size_t i = 0; i < s.size();) {
            const int g = stbtt_FindGlyphIndex(&info, static_cast<int>(NextCodePoint(s, i)));
            int adv = 0, lsb = 0;
            stbtt_GetGlyphHMetrics(&info, g, &adv, &lsb);
            if (prev) {
                x += static_cast<float>(stbtt_GetGlyphKernAdvance(&info, prev, g)) * scale;
            }
            x += static_cast<float>(adv) * scale;
            prev = g;
        }
        return x;
    }

    float CapHeight(float size) const {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBox(&info, stbtt_FindGlyphIndex(&info, 'H'), &x0, &y0, &x1, &y1);
        return static_cast<float>(y1) * stbtt_ScaleForMappingEmToPixels(&info, size);
    }

    template <typename Plot>
    void Draw(float size, float x, float baseline, const std::string& s, Plot&& plot) {
        const float scale = stbtt_ScaleForMappingEmToPixels(&info, size);
        const int by = static_cast<int>(std::floor(baseline + 0.5f));
        float pen = x;
        int prev = 0;
        for (std::size_t i = 0; i < s.size();) {
            const uint32_t cp = NextCodePoint(s, i);
            const int g = stbtt_FindGlyphIndex(&info, static_cast<int>(cp));
            if (prev) {
                pen += static_cast<float>(stbtt_GetGlyphKernAdvance(&info, prev, g)) * scale;
            }
            int adv = 0, lsb = 0;
            stbtt_GetGlyphHMetrics(&info, g, &adv, &lsb);
            if (cp > ' ') {
                int ix = static_cast<int>(std::floor(pen));
                int phase = static_cast<int>((pen - static_cast<float>(ix)) * 4.0f + 0.5f);
                if (phase >= 4) {
                    phase = 0;
                    ix++;
                }
                const Glyph& gl = GlyphFor(g, size, phase);
                for (int yy = 0; yy < gl.h; yy++) {
                    for (int xx = 0; xx < gl.w; xx++) {
                        const uint8_t a = gl.coverage[static_cast<std::size_t>(yy * gl.w + xx)];
                        if (a) {
                            plot(ix + gl.bx + xx, by + gl.by + yy, a * (1.0f / 255.0f));
                        }
                    }
                }
            }
            pen += static_cast<float>(adv) * scale;
            prev = g;
        }
    }

private:
    struct Glyph {
        int w = 0, h = 0, bx = 0, by = 0;
        std::vector<uint8_t> coverage;
    };

    const Glyph& GlyphFor(int glyph, float size, int phase) {
        const int size_q = std::max(8, static_cast<int>(size * 8.0f + 0.5f));
        const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(glyph)) << 24) |
                             (static_cast<uint64_t>(size_q) << 4) | static_cast<uint64_t>(phase);
        if (const auto found = cache.find(key); found != cache.end()) {
            return found->second;
        }
        Glyph& out = cache[key];
        const float scale = stbtt_ScaleForMappingEmToPixels(&info, size_q / 8.0f);
        const float shift = static_cast<float>(phase) / 4.0f;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBoxSubpixel(&info, glyph, scale, scale, shift, 0.0f, &x0, &y0, &x1, &y1);
        const int bw = x1 - x0, bh = y1 - y0;
        if (bw <= 0 || bh <= 0 || bw > 300 || bh > 300) {
            return out;
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

// Noto Sans as the keyboard finds it (keyboard/art.cpp FindFont): the art folder, then the
// distributions' places. `name` is "NotoSans-Regular.ttf" or "NotoSans-Bold.ttf".
std::string FindFont(const char* name) {
    if (std::string art = frame_xr::picker::FindArtFile(name); !art.empty()) {
        return art;
    }
    for (const char* dir : {"/usr/share/fonts/noto/", "/usr/share/fonts/TTF/",
                            "/usr/share/fonts/truetype/noto/", "/usr/share/fonts/google-noto/"}) {
        const std::string path = std::string{dir} + name;
        if (std::ifstream(path).good()) {
            return path;
        }
    }
    return {};
}

} // namespace

struct PanelArt::Fonts {
    Font regular;
    Font bold;
};

PanelArt::PanelArt() : fonts(std::make_unique<Fonts>()) {
    static bool warned = false;
    if (!fonts->regular.Load(FindFont("NotoSans-Regular.ttf")) && !warned) {
        warned = true;
        LOG_WARNING(Frontend, "xr: panels: Noto Sans not found, the picker's font is used");
    }
    if (!fonts->bold.Load(FindFont("NotoSans-Bold.ttf"))) {
        fonts->bold.Load(FindFont("NotoSans-Regular.ttf"));
    }
    frame_xr::ui::InitFonts();
}

PanelArt::~PanelArt() = default;

void PanelArt::Resize(int width, int height) {
    w = width;
    h = height;
    store.Init(width, height);
}

void PanelArt::Clear(uint32_t argb) {
    store.Clear(ToColor(argb));
}

void PanelArt::ClearRect(int x, int y, int width, int height) {
    for (int yy = std::max(0, y); yy < std::min(h, y + height); yy++) {
        for (int xx = std::max(0, x); xx < std::min(w, x + width); xx++) {
            store.px[static_cast<std::size_t>(yy) * w + xx] = 0;
        }
    }
}

void PanelArt::Put(int x, int y, const uint8_t* rgba, int width, int height) {
    for (int yy = 0; yy < height; yy++) {
        if (y + yy < 0 || y + yy >= h) {
            continue;
        }
        for (int xx = 0; xx < width; xx++) {
            if (x + xx < 0 || x + xx >= w) {
                continue;
            }
            const uint8_t* p = rgba + (static_cast<std::size_t>(yy) * width + xx) * 4;
            store.px[static_cast<std::size_t>(y + yy) * w + (x + xx)] =
                uint32_t{p[0]} | uint32_t{p[1]} << 8 | uint32_t{p[2]} << 16 | uint32_t{p[3]} << 24;
        }
    }
}

void PanelArt::PutOver(int x, int y, const uint8_t* rgba, int width, int height) {
    for (int yy = std::max(0, -y); yy < height && y + yy < h; yy++) {
        for (int xx = std::max(0, -x); xx < width && x + xx < w; xx++) {
            const uint8_t* p = rgba + (static_cast<std::size_t>(yy) * width + xx) * 4;
            if (p[3] == 0) {
                continue;
            }
            uint32_t& d = store.px[static_cast<std::size_t>(y + yy) * w + (x + xx)];
            const uint32_t keep = 255u - p[3];
            uint32_t out = 0;
            for (int k = 0; k < 4; k++) {
                const uint32_t dst = (d >> (8 * k)) & 0xFFu;
                out |= std::min(255u, p[k] + (dst * keep + 127u) / 255u) << (8 * k);
            }
            d = out;
        }
    }
}

void PanelArt::FillRRect(float x0, float y0, float x1, float y1, float radius, uint32_t argb) {
    store.RoundRect(x0, y0, x1, y1, radius, ToColor(argb));
}

void PanelArt::StrokeRRect(float x0, float y0, float x1, float y1, float radius, float width,
                           uint32_t argb) {
    store.StrokeRoundRect(x0, y0, x1, y1, radius, width, ToColor(argb));
}

void PanelArt::Line(float x0, float y0, float x1, float y1, float width, uint32_t argb) {
    // Only straight lines are needed (rules and the close cross): a thin rounded box along it.
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::hypot(dx, dy);
    if (len < 1e-3f) {
        return;
    }
    const float hw = width * 0.5f;
    const int minx = static_cast<int>(std::floor(std::min(x0, x1) - hw - 1));
    const int maxx = static_cast<int>(std::ceil(std::max(x0, x1) + hw + 1));
    const int miny = static_cast<int>(std::floor(std::min(y0, y1) - hw - 1));
    const int maxy = static_cast<int>(std::ceil(std::max(y0, y1) + hw + 1));
    for (int y = std::max(0, miny); y < std::min(h, maxy); y++) {
        for (int x = std::max(0, minx); x < std::min(w, maxx); x++) {
            const float px = x + 0.5f - x0, py = y + 0.5f - y0;
            const float t = std::clamp((px * dx + py * dy) / (len * len), 0.0f, 1.0f);
            const float d = std::hypot(px - t * dx, py - t * dy);
            Over(x, y, argb, std::clamp(hw - d + 0.5f, 0.0f, 1.0f));
        }
    }
}

void PanelArt::Over(int x, int y, uint32_t argb, float coverage) {
    if (x < 0 || y < 0 || x >= w || y >= h || coverage <= 0.0f) {
        return;
    }
    store.Blend(x, y, ToColor(argb), coverage);
}

float PanelArt::TextWidth(float size, bool bold, const std::string& s) const {
    const Font& f = bold ? fonts->bold : fonts->regular;
    if (f.Ok()) {
        return f.Width(size, s);
    }
    return frame_xr::ui::TextWidth(frame_xr::ui::kFontSmall, s.c_str());
}

float PanelArt::CapHeight(float size) const {
    return fonts->regular.Ok() ? fonts->regular.CapHeight(size) : size * 0.7f;
}

std::string PanelArt::Fit(float size, bool bold, const std::string& s, float width) const {
    if (TextWidth(size, bold, s) <= width) {
        return s;
    }
    std::string cut = s;
    while (!cut.empty() && TextWidth(size, bold, cut + "...") > width) {
        cut.pop_back();
        while (!cut.empty() && (static_cast<uint8_t>(cut.back()) & 0xC0) == 0x80) {
            cut.pop_back(); // not in the middle of a UTF-8 sequence
        }
        if (!cut.empty() && static_cast<uint8_t>(cut.back()) >= 0xC0) {
            cut.pop_back();
        }
    }
    return cut + "...";
}

void PanelArt::Text(float x, float baseline, float size, bool bold, uint32_t argb,
                    const std::string& s, int align) {
    Font& f = bold ? fonts->bold : fonts->regular;
    const float width = TextWidth(size, bold, s);
    const float left = align == 1 ? x - width * 0.5f : align == 2 ? x - width : x;
    if (!f.Ok()) {
        store.Text(left, baseline, frame_xr::ui::kFontSmall, s.c_str(), ToColor(argb), 0);
        return;
    }
    f.Draw(size, left, baseline, s, [&](int px, int py, float cov) { Over(px, py, argb, cov); });
}

} // namespace FrameXr::Vis
