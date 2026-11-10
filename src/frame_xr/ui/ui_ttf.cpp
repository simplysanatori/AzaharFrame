// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "frame_xr/ui/ui_ttf.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <unordered_map>

#include "frame_xr/keyboard/stb_truetype.h" // the implementation: keyboard/stb_truetype.cpp

namespace frame_xr::ui {

namespace {

// Coverage a little heavier, as the keyboard's and the vis panels' (kTextGamma there).
constexpr float kGamma = 0.80f;
// Glyphs larger than this (pixels) are not drawn.
constexpr int kMaxGlyph = 400;

int Base64Value(char c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '/')
        return 63;
    return -1;
}

} // namespace

char32_t NextUtf8(const std::string& s, std::size_t& i) {
    const uint8_t c = static_cast<uint8_t>(s[i++]);
    if (c < 0x80) {
        return c;
    }
    const int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
    char32_t cp = c & (0x3Fu >> extra);
    for (int k = 0; k < extra && i < s.size() && (static_cast<uint8_t>(s[i]) & 0xC0) == 0x80; k++) {
        cp = (cp << 6) | (static_cast<uint8_t>(s[i++]) & 0x3Fu);
    }
    return cp;
}

struct TtfFont::Impl {
    struct Glyph {
        int w = 0, h = 0, bx = 0, by = 0;
        std::vector<uint8_t> coverage;
    };

    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    bool ok = false;
    uint8_t lut[256]{};
    std::unordered_map<uint64_t, Glyph> cache;

    bool Init() {
        const int offset = data.size() > 12 ? stbtt_GetFontOffsetForIndex(data.data(), 0) : -1;
        if (offset < 0 || !stbtt_InitFont(&info, data.data(), offset)) {
            data.clear();
            return false;
        }
        for (int i = 0; i < 256; i++) {
            lut[i] = static_cast<uint8_t>(255.0f * std::pow(i / 255.0f, kGamma) + 0.5f);
        }
        cache.clear();
        ok = true;
        return true;
    }

    float Scale(float px) const {
        return stbtt_ScaleForMappingEmToPixels(&info, px);
    }

    const Glyph& GlyphFor(int glyph, float px, int phase) {
        const int size_q = std::max(8, static_cast<int>(px * 8.0f + 0.5f));
        const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(glyph)) << 24) |
                             (static_cast<uint64_t>(size_q) << 4) | static_cast<uint64_t>(phase);
        if (const auto found = cache.find(key); found != cache.end()) {
            return found->second;
        }
        Glyph& out = cache[key];
        const float scale = Scale(static_cast<float>(size_q) / 8.0f);
        const float shift = static_cast<float>(phase) / 4.0f;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetGlyphBitmapBoxSubpixel(&info, glyph, scale, scale, shift, 0.0f, &x0, &y0, &x1, &y1);
        const int bw = x1 - x0, bh = y1 - y0;
        if (bw <= 0 || bh <= 0 || bw > kMaxGlyph || bh > kMaxGlyph) {
            return out;
        }
        out.w = bw;
        out.h = bh;
        out.bx = x0;
        out.by = y0;
        out.coverage.assign(static_cast<std::size_t>(bw) * static_cast<std::size_t>(bh), 0);
        stbtt_MakeGlyphBitmapSubpixel(&info, out.coverage.data(), bw, bh, bw, scale, scale, shift,
                                      0.0f, glyph);
        for (uint8_t& a : out.coverage) {
            a = lut[a];
        }
        return out;
    }

    void Put(Canvas& cv, int glyph, float px, float pen, int by, const Color& c) {
        int ix = static_cast<int>(std::floor(pen));
        int phase = static_cast<int>((pen - static_cast<float>(ix)) * 4.0f + 0.5f);
        if (phase >= 4) {
            phase = 0;
            ix++;
        }
        const Glyph& g = GlyphFor(glyph, px, phase);
        if (g.w > 0) {
            cv.BlendMask(ix + g.bx, by + g.by, g.coverage.data(), g.w, g.h, g.w, c);
        }
    }
};

TtfFont::TtfFont() : impl(std::make_unique<Impl>()) {}
TtfFont::~TtfFont() = default;

bool TtfFont::LoadBase64(const char* base64) {
    impl->data.clear();
    uint32_t bits = 0;
    int nbits = 0;
    for (const char* p = base64; *p; p++) {
        const int v = Base64Value(*p);
        if (v < 0) {
            continue;
        }
        bits = (bits << 6) | static_cast<uint32_t>(v);
        nbits += 6;
        if (nbits >= 8) {
            nbits -= 8;
            impl->data.push_back(static_cast<unsigned char>(bits >> nbits));
        }
    }
    return impl->Init();
}

bool TtfFont::LoadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    impl->data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return impl->Init();
}

bool TtfFont::Ok() const {
    return impl->ok;
}

float TtfFont::Width(float px, const std::string& s) const {
    if (!impl->ok) {
        return 0.0f;
    }
    const float scale = impl->Scale(px);
    float x = 0.0f;
    int prev = 0;
    for (std::size_t i = 0; i < s.size();) {
        const int g = stbtt_FindGlyphIndex(&impl->info, static_cast<int>(NextUtf8(s, i)));
        int adv = 0, lsb = 0;
        stbtt_GetGlyphHMetrics(&impl->info, g, &adv, &lsb);
        if (prev) {
            x += static_cast<float>(stbtt_GetGlyphKernAdvance(&impl->info, prev, g)) * scale;
        }
        x += static_cast<float>(adv) * scale;
        prev = g;
    }
    return x;
}

float TtfFont::CapHeight(float px) const {
    if (!impl->ok) {
        return px * 0.7f;
    }
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBox(&impl->info, stbtt_FindGlyphIndex(&impl->info, 'H'), &x0, &y0, &x1, &y1);
    return static_cast<float>(y1) * impl->Scale(px);
}

std::string TtfFont::Fit(float px, const std::string& s, float width) const {
    if (Width(px, s) <= width) {
        return s;
    }
    std::string t = s;
    while (!t.empty()) {
        std::size_t n = t.size() - 1;
        while (n > 0 && (static_cast<uint8_t>(t[n]) & 0xC0) == 0x80) {
            n--;
        }
        t.resize(n);
        while (!t.empty() && t.back() == ' ') {
            t.pop_back();
        }
        if (Width(px, t + "…") <= width) {
            return t + "…";
        }
    }
    return {};
}

float TtfFont::Draw(Canvas& cv, float x, float baseline, float px, const Color& c,
                    const std::string& s, int align) {
    if (!impl->ok || s.empty()) {
        return 0.0f;
    }
    const float width = Width(px, s);
    float pen = x - (align == 1 ? width * 0.5f : align == 2 ? width : 0.0f);
    const float scale = impl->Scale(px);
    const int by = static_cast<int>(std::floor(baseline + 0.5f));
    int prev = 0;
    for (std::size_t i = 0; i < s.size();) {
        const char32_t cp = NextUtf8(s, i);
        const int g = stbtt_FindGlyphIndex(&impl->info, static_cast<int>(cp));
        if (prev) {
            pen += static_cast<float>(stbtt_GetGlyphKernAdvance(&impl->info, prev, g)) * scale;
        }
        int adv = 0, lsb = 0;
        stbtt_GetGlyphHMetrics(&impl->info, g, &adv, &lsb);
        if (cp > U' ') {
            impl->Put(cv, g, px, pen, by, c);
        }
        pen += static_cast<float>(adv) * scale;
        prev = g;
    }
    return width;
}

void TtfFont::DrawCentred(Canvas& cv, float cx, float cy, float px, char32_t code, const Color& c) {
    if (!impl->ok) {
        return;
    }
    const int g = stbtt_FindGlyphIndex(&impl->info, static_cast<int>(code));
    if (g == 0) {
        return;
    }
    // The em square centred on the point: Material Symbols draw each icon in it.
    int ascent = 0, descent = 0, gap = 0;
    stbtt_GetFontVMetrics(&impl->info, &ascent, &descent, &gap);
    const float scale = impl->Scale(px);
    int adv = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&impl->info, g, &adv, &lsb);
    const float baseline = cy + static_cast<float>(ascent + descent) * 0.5f * scale;
    const float pen = cx - static_cast<float>(adv) * 0.5f * scale;
    impl->Put(cv, g, px, pen, static_cast<int>(std::floor(baseline + 0.5f)), c);
}

} // namespace frame_xr::ui
