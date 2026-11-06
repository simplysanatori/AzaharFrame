// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// Adapted from GalaxyFrame's ui_canvas (same author).

#include "frame_xr/ui/ui_canvas.h"

#include <algorithm>
#include <cmath>

#include "frame_xr/ui/ui_font.h"

namespace frame_xr::ui {

namespace {

struct Font {
    const font::Face* face = nullptr;
    std::vector<uint8_t> coverage; // face->width x face->height
    const font::Glyph* glyph(uint32_t code) const {
        if (code >= 32 && code < 127) {
            return &face->glyphs[code - 32];
        }
        for (int i = 95; i < face->count; i++) {
            if (face->glyphs[i].code == code) {
                return &face->glyphs[i];
            }
        }
        return &face->glyphs['?' - 32];
    }
};
Font g_fonts[2];

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

// Next code point of UTF-8 text.
uint32_t NextCode(const char*& s) {
    const uint8_t c = static_cast<uint8_t>(*s++);
    if (c < 0x80)
        return c;
    const int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
    uint32_t code = c & (0x3F >> extra);
    for (int i = 0; i < extra && (*s & 0xC0) == 0x80; i++) {
        code = (code << 6) | (static_cast<uint8_t>(*s++) & 0x3F);
    }
    return code;
}

// Signed distance shapes, anti-aliased over a pixel.
template <class Dist, class Paint>
void Fill(Canvas& cv, float x0, float y0, float x1, float y1, Dist dist, Paint paint) {
    const int ix0 = static_cast<int>(std::fmax(0.0f, std::floor(x0 - 1.0f)));
    const int iy0 = static_cast<int>(std::fmax(0.0f, std::floor(y0 - 1.0f)));
    const int ix1 = static_cast<int>(std::fmin(static_cast<float>(cv.w), std::ceil(x1 + 1.0f)));
    const int iy1 = static_cast<int>(std::fmin(static_cast<float>(cv.h), std::ceil(y1 + 1.0f)));
    for (int y = iy0; y < iy1; y++) {
        for (int x = ix0; x < ix1; x++) {
            const float d = dist(x + 0.5f, y + 0.5f);
            const float cov = std::fmin(1.0f, std::fmax(0.0f, 0.5f - d));
            if (cov > 0.0f) {
                paint(x, y, cov);
            }
        }
    }
}

float RoundRectDist(float x, float y, float cx, float cy, float hx, float hy, float radius) {
    const float qx = std::fabs(x - cx) - hx + radius;
    const float qy = std::fabs(y - cy) - hy + radius;
    const float ox = std::fmax(qx, 0.0f), oy = std::fmax(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::fmin(std::fmax(qx, qy), 0.0f) - radius;
}

} // namespace

void InitFonts() {
    if (g_fonts[0].face) {
        return;
    }
    for (int f = 0; f < 2; f++) {
        const font::Face& face = font::kFaces[f];
        std::vector<uint8_t> packed;
        uint32_t bits = 0;
        int nbits = 0;
        for (const char* p = face.atlas; *p; p++) {
            const int v = Base64Value(*p);
            if (v < 0)
                continue;
            bits = (bits << 6) | static_cast<uint32_t>(v);
            nbits += 6;
            if (nbits >= 8) {
                nbits -= 8;
                packed.push_back(static_cast<uint8_t>(bits >> nbits));
            }
        }
        Font& fnt = g_fonts[f];
        fnt.face = &face;
        fnt.coverage.assign(static_cast<size_t>(face.width) * face.height, 0);
        for (size_t i = 0; i < fnt.coverage.size(); i++) {
            const uint8_t b = i / 2 < packed.size() ? packed[i / 2] : 0;
            fnt.coverage[i] = static_cast<uint8_t>(((i & 1) ? (b & 15) : (b >> 4)) * 17);
        }
    }
}

float TextWidth(int font, const char* s) {
    float w = 0.0f;
    while (*s) {
        w += g_fonts[font].glyph(NextCode(s))->advance * 0.25f;
    }
    return w;
}

std::vector<std::string> WrapText(int font, const std::string& s, float width, int max_lines) {
    std::vector<std::string> lines;
    if (s.empty() || max_lines <= 0) {
        return lines;
    }
    const auto fits = [&](const std::string& t) { return TextWidth(font, t.c_str()) <= width; };

    // Shortens `t` until `t + "..."` fits.
    const auto ellipsize = [&](std::string t) {
        while (!t.empty() && !fits(t + "...")) {
            // Drop one whole UTF-8 sequence.
            size_t n = t.size() - 1;
            while (n > 0 && (static_cast<uint8_t>(t[n]) & 0xC0) == 0x80)
                n--;
            t.resize(n);
        }
        while (!t.empty() && t.back() == ' ')
            t.pop_back();
        return t + "...";
    };

    std::string line;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = i;
        while (j < s.size() && s[j] == ' ')
            j++;
        size_t k = j;
        while (k < s.size() && s[k] != ' ')
            k++;
        std::string word = s.substr(j, k - j);
        const std::string sep = line.empty() ? std::string() : " ";
        i = k;
        if (word.empty()) {
            continue; // trailing spaces
        }

        if (fits(line + sep + word)) {
            line += sep + word;
            continue;
        }
        if (!line.empty()) {
            lines.push_back(line);
            line.clear();
            if (static_cast<int>(lines.size()) == max_lines) {
                lines.back() = ellipsize(lines.back() + " " + word);
                return lines;
            }
        }
        // `word` starts a fresh line; split it if it is wider than a line.
        while (!word.empty() && !fits(word)) {
            size_t n = word.size();
            do {
                n--;
                while (n > 0 && (static_cast<uint8_t>(word[n]) & 0xC0) == 0x80)
                    n--;
            } while (n > 1 && !fits(word.substr(0, n)));
            if (n == 0)
                n = 1;
            if (static_cast<int>(lines.size()) + 1 == max_lines) {
                lines.push_back(ellipsize(word + (i < s.size() ? s.substr(i) : "")));
                return lines;
            }
            lines.push_back(word.substr(0, n));
            word.erase(0, n);
        }
        line = word;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

void Canvas::Init(int width, int height) {
    w = width;
    h = height;
    px.assign(static_cast<size_t>(w) * h, 0);
}

void Canvas::Clear(const Color& c) {
    const auto to8 = [](float v) {
        return static_cast<uint32_t>(std::clamp(v, 0.0f, 255.0f) + 0.5f);
    };
    const float a = std::clamp(c.a, 0.0f, 1.0f);
    const uint32_t v = to8(c.r * a) | to8(c.g * a) << 8 | to8(c.b * a) << 16 | to8(255.0f * a) << 24;
    std::fill(px.begin(), px.end(), v);
}

void Canvas::Blend(int x, int y, const Color& c, float coverage) {
    const float a = c.a * coverage;
    if (a <= 0.0f)
        return;
    uint32_t& d = px[static_cast<size_t>(y) * w + x];
    const float k = 1.0f - a;
    const uint32_t r = static_cast<uint32_t>(c.r * a + static_cast<float>(d & 255) * k + 0.5f);
    const uint32_t g = static_cast<uint32_t>(c.g * a + static_cast<float>((d >> 8) & 255) * k + 0.5f);
    const uint32_t b = static_cast<uint32_t>(c.b * a + static_cast<float>((d >> 16) & 255) * k + 0.5f);
    const uint32_t al = static_cast<uint32_t>(255.0f * a + static_cast<float>(d >> 24) * k + 0.5f);
    d = (r > 255 ? 255 : r) | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b) << 16 |
        (al > 255 ? 255 : al) << 24;
}

void Canvas::FillRect(float x0, float y0, float x1, float y1, const Color& c) {
    const int ix0 = std::max(0, static_cast<int>(std::lround(x0)));
    const int iy0 = std::max(0, static_cast<int>(std::lround(y0)));
    const int ix1 = std::min(w, static_cast<int>(std::lround(x1)));
    const int iy1 = std::min(h, static_cast<int>(std::lround(y1)));
    for (int y = iy0; y < iy1; y++) {
        for (int x = ix0; x < ix1; x++) {
            Blend(x, y, c, 1.0f);
        }
    }
}

void Canvas::RoundRect(float x0, float y0, float x1, float y1, float radius, const Color& c) {
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    const float hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f;
    Fill(
        *this, x0, y0, x1, y1,
        [=](float x, float y) { return RoundRectDist(x, y, cx, cy, hx, hy, radius); },
        [&](int x, int y, float cov) { Blend(x, y, c, cov); });
}

void Canvas::StrokeRoundRect(float x0, float y0, float x1, float y1, float radius, float thickness,
                             const Color& c) {
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    const float hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f;
    const float half = thickness * 0.5f;
    Fill(
        *this, x0 - half, y0 - half, x1 + half, y1 + half,
        [=](float x, float y) {
            return std::fabs(RoundRectDist(x, y, cx, cy, hx, hy, radius)) - half;
        },
        [&](int x, int y, float cov) { Blend(x, y, c, cov); });
}

void Canvas::Circle(float cx, float cy, float r, const Color& c) {
    Fill(
        *this, cx - r, cy - r, cx + r, cy + r,
        [=](float x, float y) { return std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r; },
        [&](int x, int y, float cov) { Blend(x, y, c, cov); });
}

void Canvas::BlitRGBA(const uint8_t* src, int sw, int sh, float x0, float y0, float x1, float y1,
                      bool smooth, float radius, float alpha) {
    if (!src || sw <= 0 || sh <= 0 || x1 <= x0 || y1 <= y0) {
        return;
    }
    const float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    const float hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f;
    const float su = static_cast<float>(sw) / (x1 - x0);
    const float sv = static_cast<float>(sh) / (y1 - y0);

    const auto texel = [&](int tx, int ty, float out[4]) {
        tx = std::clamp(tx, 0, sw - 1);
        ty = std::clamp(ty, 0, sh - 1);
        const uint8_t* p = src + (static_cast<size_t>(ty) * sw + tx) * 4;
        for (int i = 0; i < 4; i++)
            out[i] = p[i];
    };

    Fill(
        *this, x0, y0, x1, y1,
        [=](float x, float y) { return RoundRectDist(x, y, cx, cy, hx, hy, radius); },
        [&](int x, int y, float cov) {
            // Source position of the pixel centre.
            const float u = (x + 0.5f - x0) * su, v = (y + 0.5f - y0) * sv;
            float t[4];
            if (smooth) {
                const float fu = u - 0.5f, fv = v - 0.5f;
                const int ix = static_cast<int>(std::floor(fu)), iy = static_cast<int>(std::floor(fv));
                const float ax = fu - ix, ay = fv - iy;
                float a[4], b[4], c[4], d[4];
                texel(ix, iy, a);
                texel(ix + 1, iy, b);
                texel(ix, iy + 1, c);
                texel(ix + 1, iy + 1, d);
                for (int i = 0; i < 4; i++) {
                    t[i] = (a[i] * (1 - ax) + b[i] * ax) * (1 - ay) +
                           (c[i] * (1 - ax) + d[i] * ax) * ay;
                }
            } else {
                texel(static_cast<int>(std::floor(u)), static_cast<int>(std::floor(v)), t);
            }
            Blend(x, y, {t[0], t[1], t[2], t[3] / 255.0f * alpha}, cov);
        });
}

void Canvas::Text(float x, float baseline, int font, const char* s, const Color& c, int align) {
    const Font& f = g_fonts[font];
    float pen = x - (align == 1   ? TextWidth(font, s) * 0.5f
                     : align == 2 ? TextWidth(font, s)
                                  : 0.0f);
    const int by = static_cast<int>(std::lround(baseline));
    while (*s) {
        const font::Glyph* g = f.glyph(NextCode(s));
        const int gx0 = static_cast<int>(std::lround(pen)) + g->left, gy0 = by - g->top;
        for (int gy = 0; gy < g->h; gy++) {
            const int y = gy0 + gy;
            if (y < 0 || y >= h)
                continue;
            const uint8_t* row = &f.coverage[static_cast<size_t>(g->y + gy) * f.face->width + g->x];
            for (int gx = 0; gx < g->w; gx++) {
                const int xx = gx0 + gx;
                if (xx >= 0 && xx < w && row[gx])
                    Blend(xx, y, c, row[gx] / 255.0f);
            }
        }
        pen += g->advance * 0.25f;
    }
}

float Canvas::TextWrapped(float x, float baseline, float width, float line_height, int font,
                          const char* s, const Color& c) {
    std::string line;
    const char* p = s;
    while (*p) {
        // The next word, with the spaces before it.
        const char* start = p;
        while (*p == ' ')
            p++;
        while (*p && *p != ' ' && *p != '\n')
            p++;
        std::string word(start, p);
        std::string trial = line + word;
        if (!line.empty() && TextWidth(font, trial.c_str()) > width) {
            Text(x, baseline, font, line.c_str(), c);
            baseline += line_height;
            const size_t first = word.find_first_not_of(' ');
            line = first == std::string::npos ? std::string() : word.substr(first);
        } else {
            line = trial;
        }
        if (*p == '\n') {
            Text(x, baseline, font, line.c_str(), c);
            baseline += line_height;
            line.clear();
            p++;
        }
    }
    if (!line.empty()) {
        Text(x, baseline, font, line.c_str(), c);
        baseline += line_height;
    }
    return baseline;
}

} // namespace frame_xr::ui
