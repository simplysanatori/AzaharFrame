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

char32_t NextUtf8(const char*& s, const char* end) {
    const uint8_t c = static_cast<uint8_t>(*s++);
    if (c < 0x80) {
        return c;
    }
    // A malformed sequence is read as Latin-1 (one byte, U+0080..U+00FF) rather than "?": text
    // from an old Latin-1 source still shows its accents.
    const int extra = c >= 0xF8 ? -1 : c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 ? 1 : -1;
    if (extra < 0 || end - s < extra) {
        return c;
    }
    char32_t cp = c & (0x3Fu >> extra);
    for (int k = 0; k < extra; k++) {
        if ((static_cast<uint8_t>(s[k]) & 0xC0) != 0x80) {
            return c;
        }
        cp = (cp << 6) | (static_cast<uint8_t>(s[k]) & 0x3Fu);
    }
    s += extra;
    return cp;
}

char32_t NextUtf8(const std::string& s, std::size_t& i) {
    const char* p = s.data() + i;
    const char32_t cp = NextUtf8(p, s.data() + s.size());
    i = static_cast<std::size_t>(p - s.data());
    return cp;
}

std::string FoldedText(char32_t cp) {
    switch (cp) {
    case 0x2018:
    case 0x2019:
    case 0x201A:
    case 0x2032:
        return "'";
    case 0x201C:
    case 0x201D:
    case 0x201E:
    case 0x2033:
        return "\"";
    case 0x2010:
    case 0x2011:
    case 0x2012:
    case 0x2013:
    case 0x2014:
    case 0x2015:
        return "-";
    case 0x2026:
        return "...";
    case 0x00A0:
    case 0x2002:
    case 0x2003:
    case 0x2009:
    case 0x202F:
        return " ";
    case 0x2122:
        return "TM";
    case 0x00D7:
        return "x";
    case 0x00C6:
        return "AE";
    case 0x00E6:
        return "ae";
    case 0x0152:
        return "OE";
    case 0x0153:
        return "oe";
    case 0x00DF:
        return "ss";
    default:
        break;
    }
    // Latin-1 letters with an accent, by U+00C0..U+00FF: the base letter.
    static const char kBase[] = "AAAAAAACEEEEIIII"  // C0
                                "DNOOOOO*OUUUUYTs"   // D0
                                "aaaaaaaceeeeiiii"   // E0
                                "dnooooo/ouuuuyty";  // F0
    if (cp >= 0xC0 && cp <= 0xFF && kBase[cp - 0xC0] != '*' && kBase[cp - 0xC0] != '/') {
        return std::string(1, kBase[cp - 0xC0]);
    }
    return "?";
}

// The UTF-8 of one code point.
std::string Utf8Of(char32_t cp) {
    std::string s;
    if (cp < 0x80) {
        s += static_cast<char>(cp);
    } else if (cp < 0x800) {
        s += static_cast<char>(0xC0 | (cp >> 6));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        s += static_cast<char>(0xE0 | (cp >> 12));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        s += static_cast<char>(0xF0 | (cp >> 18));
        s += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return s;
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

    // `s` with each character this face lacks replaced by FoldedText, so a missing glyph never
    // draws as the empty box. ASCII goes through untouched.
    std::string Usable(const std::string& s) const {
        std::size_t i = 0;
        while (i < s.size() && static_cast<uint8_t>(s[i]) < 0x80) {
            i++;
        }
        if (i == s.size()) {
            return s;
        }
        std::string out = s.substr(0, i);
        while (i < s.size()) {
            const char32_t cp = NextUtf8(s, i);
            out += cp < 0x80 || stbtt_FindGlyphIndex(&info, static_cast<int>(cp)) != 0
                       ? Utf8Of(cp)
                       : FoldedText(cp);
        }
        return out;
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

bool TtfFont::HasGlyph(char32_t cp) const { // AzaharFrame emerald
    return impl->ok && stbtt_FindGlyphIndex(&impl->info, static_cast<int>(cp)) != 0;
}

float TtfFont::Width(float px, const std::string& text) const {
    if (!impl->ok) {
        return 0.0f;
    }
    const std::string s = impl->Usable(text);
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
                    const std::string& text, int align) {
    if (!impl->ok || text.empty()) {
        return 0.0f;
    }
    const std::string s = impl->Usable(text);
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
