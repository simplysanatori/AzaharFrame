// Copyright Azahar for Frame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame cpphelpers: see frame_controller_models.h. The arithmetic follows the Python original
// operation by operation (doubles, no fused multiply-add), so the files match it.

#pragma clang fp contract(off)

#include "core/frame_controller_models.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <fmt/format.h>
#include <json.hpp>

namespace fs = std::filesystem;

namespace FrameControllerModels {

namespace {

using json = nlohmann::json;
using Vec = std::array<double, 3>;

constexpr u32 kVersion = 2;
constexpr u32 kFlags = 1; // the vertices are in the OpenXR aim space
constexpr int kVertexFloats = 12;
constexpr int kNameBytes = 32;
constexpr int kMotionFloats = 32;
constexpr int kMotionOrigin = 0, kMotionAxis = 3, kMotionRange = 6, kMotionPressed = 8,
              kMotionFrame = 9, kMotionRotX = 18, kMotionRotY = 20, kMotionPress = 22;

struct ConvertError {
    std::string text;
};

Vec Sub(const Vec& a, const Vec& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec Add(const Vec& a, const Vec& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
double Dot(const Vec& a, const Vec& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec Cross(const Vec& a, const Vec& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec Scale3(const Vec& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
Vec Norm(const Vec& a) {
    const double n = std::sqrt(Dot(a, a));
    return n != 0.0 ? Vec{a[0] / n, a[1] / n, a[2] / n} : Vec{0.0, 0.0, 0.0};
}

using Mat = std::array<std::array<double, 3>, 3>;

Mat Mul(const Mat& a, const Mat& b) {
    Mat m{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double sum = 0; // Python's sum() starts from the integer 0
            for (int k = 0; k < 3; ++k) {
                sum += a[i][k] * b[k][j];
            }
            m[i][j] = sum;
        }
    }
    return m;
}

/// SteamVR's rotate_xyz (degrees): X then Y then Z.
Mat EulerXyz(const Vec& degrees) {
    constexpr double kRad = 3.141592653589793 / 180.0; // math.radians
    const double x = degrees[0] * kRad, y = degrees[1] * kRad, z = degrees[2] * kRad;
    const double cx = std::cos(x), sx = std::sin(x), cy = std::cos(y), sy = std::sin(y),
                 cz = std::cos(z), sz = std::sin(z);
    const Mat rx{{{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}}};
    const Mat ry{{{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}}};
    const Mat rz{{{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}}};
    return Mul(rz, Mul(ry, rx));
}

struct Corner {
    Vec pos;
    std::array<double, 2> uv;
    Vec normal;
};
using Tri = std::array<Corner, 3>;

std::vector<Tri> ReadObj(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw ConvertError{path + ": cannot be read"};
    }
    std::vector<Vec> vs, vns;
    std::vector<std::array<double, 2>> vts;
    std::vector<Tri> tris;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::vector<std::string> parts;
        for (std::string p; ss >> p;) {
            parts.push_back(p);
        }
        if (parts.empty()) {
            continue;
        }
        const auto num = [&](std::size_t i) {
            if (i >= parts.size()) {
                throw ConvertError{path + ": a line is too short"};
            }
            return std::strtod(parts[i].c_str(), nullptr);
        };
        if (parts[0] == "v") {
            vs.push_back({num(1), num(2), num(3)});
        } else if (parts[0] == "vt") {
            vts.push_back({num(1), num(2)});
        } else if (parts[0] == "vn") {
            vns.push_back({num(1), num(2), num(3)});
        } else if (parts[0] == "f") {
            std::vector<Corner> corners;
            for (std::size_t r = 1; r < parts.size(); ++r) {
                std::string ref[3];
                std::size_t k = 0;
                for (const char c : parts[r]) {
                    if (c == '/') {
                        ++k;
                    } else if (k < 3) {
                        ref[k].push_back(c);
                    }
                }
                const auto idx = [&](const std::string& s, std::size_t size) {
                    const long i = std::strtol(s.c_str(), nullptr, 10);
                    // Python's negative indices are not used by these models; refuse them
                    if (i < 1 || static_cast<std::size_t>(i) > size) {
                        throw ConvertError{path + ": a bad index"};
                    }
                    return static_cast<std::size_t>(i - 1);
                };
                Corner c;
                c.pos = vs[idx(ref[0], vs.size())];
                c.uv = ref[1].empty() ? std::array<double, 2>{0.0, 0.0} : vts[idx(ref[1], vts.size())];
                c.normal = ref[2].empty() ? Vec{0.0, 1.0, 0.0} : vns[idx(ref[2], vns.size())];
                corners.push_back(c);
            }
            for (std::size_t k = 1; k + 1 < corners.size(); ++k) { // fan
                tris.push_back({corners[0], corners[k], corners[k + 1]});
            }
        }
    }
    return tris;
}

/// round(x, digits) as a key: the correctly rounded decimal text (no negative zero).
std::string Key(double x, int digits) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", digits, x);
    std::string s = buf;
    if (s.find_first_not_of("-0.") == std::string::npos) {
        s.erase(std::remove(s.begin(), s.end(), '-'), s.end());
    }
    return s;
}

struct Vertex {
    float v[kVertexFloats];
};

void BuildVertices(const std::vector<Tri>& tris, u32 first_vertex, std::vector<Vertex>* verts,
                   std::vector<u32>* indices) {
    std::map<std::string, std::size_t> index;
    std::vector<Vec> pos, nrm;
    std::vector<std::array<double, 2>> uvs;
    std::vector<std::size_t> idx;
    for (const Tri& tri : tris) {
        for (const Corner& c : tri) {
            const std::string key = Key(c.pos[0], 7) + "|" + Key(c.pos[1], 7) + "|" + Key(c.pos[2], 7) + "|" +
                                    Key(c.uv[0], 6) + "|" + Key(c.uv[1], 6) + "|" + Key(c.normal[0], 5) +
                                    "|" + Key(c.normal[1], 5) + "|" + Key(c.normal[2], 5);
            auto it = index.find(key);
            if (it == index.end()) {
                it = index.emplace(key, pos.size()).first;
                pos.push_back(c.pos);
                nrm.push_back(c.normal);
                uvs.push_back(c.uv);
            }
            idx.push_back(it->second);
        }
    }
    std::vector<Vec> tan(pos.size(), Vec{0, 0, 0}), bit(pos.size(), Vec{0, 0, 0});
    for (std::size_t t = 0; t < idx.size(); t += 3) {
        const std::size_t i0 = idx[t], i1 = idx[t + 1], i2 = idx[t + 2];
        const Vec e1 = Sub(pos[i1], pos[i0]), e2 = Sub(pos[i2], pos[i0]);
        const double du1 = uvs[i1][0] - uvs[i0][0], dv1 = uvs[i1][1] - uvs[i0][1];
        const double du2 = uvs[i2][0] - uvs[i0][0], dv2 = uvs[i2][1] - uvs[i0][1];
        const double r = du1 * dv2 - du2 * dv1;
        if (std::fabs(r) < 1e-12) {
            continue;
        }
        const Vec sdir = Scale3(Sub(Scale3(e1, dv2), Scale3(e2, dv1)), 1.0 / r);
        const Vec tdir = Scale3(Sub(Scale3(e2, du1), Scale3(e1, du2)), 1.0 / r);
        for (const std::size_t i : {i0, i1, i2}) {
            tan[i] = Add(tan[i], sdir);
            bit[i] = Add(bit[i], tdir);
        }
    }
    for (std::size_t k = 0; k < pos.size(); ++k) {
        const Vec& n = nrm[k];
        Vec t = Norm(Sub(tan[k], Scale3(n, Dot(n, tan[k]))));
        if (Dot(t, t) < 0.5) { // no UV gradient here: any direction across the normal
            t = Norm(Cross(n, std::fabs(n[1]) < 0.9 ? Vec{0.0, 1.0, 0.0} : Vec{1.0, 0.0, 0.0}));
        }
        const double w = Dot(Cross(n, t), bit[k]) < 0.0 ? -1.0 : 1.0;
        Vertex out;
        const double values[kVertexFloats] = {pos[k][0], pos[k][1], pos[k][2], n[0], n[1], n[2],
                                              t[0],      t[1],      t[2],      w,    uvs[k][0],
                                              1.0 - uvs[k][1]}; // v from the top row down
        for (int i = 0; i < kVertexFloats; ++i) {
            out.v[i] = static_cast<float>(values[i]);
        }
        verts->push_back(out);
    }
    for (const std::size_t i : idx) {
        indices->push_back(first_vertex + static_cast<u32>(i));
    }
}

struct Component {
    std::string name, path;
    u32 first = 0, count = 0, type = 0;
    std::array<float, kMotionFloats> motion{};
};

Vec ToVec(const json& j) {
    if (!j.is_array() || j.size() < 3) {
        throw ConvertError{"a model component has a bad vector"};
    }
    return {j[0].get<double>(), j[1].get<double>(), j[2].get<double>()};
}

} // namespace

Image Scale(const Image& src, int w, int h) {
    if (src.width == w && src.height == h) {
        return src;
    }
    Image out;
    out.width = w;
    out.height = h;
    out.rgb.resize(static_cast<std::size_t>(w) * h * 3);
    // Separable area averaging: each output pixel is the mean of the source span it covers.
    std::vector<double> tmp(static_cast<std::size_t>(w) * src.height * 3);
    const double sx = static_cast<double>(src.width) / w, sy = static_cast<double>(src.height) / h;
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < w; ++x) {
            const double a = x * sx, b = (x + 1) * sx;
            double acc[3] = {0, 0, 0}, wsum = 0;
            for (int i = static_cast<int>(a); i < src.width && i < b; ++i) {
                const double lo = std::max<double>(a, i), hi = std::min<double>(b, i + 1);
                const double wt = hi - lo;
                if (wt <= 0) {
                    continue;
                }
                const u8* p = &src.rgb[(static_cast<std::size_t>(y) * src.width + i) * 3];
                for (int c = 0; c < 3; ++c) {
                    acc[c] += wt * p[c];
                }
                wsum += wt;
            }
            for (int c = 0; c < 3; ++c) {
                tmp[(static_cast<std::size_t>(y) * w + x) * 3 + c] = acc[c] / wsum;
            }
        }
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double a = y * sy, b = (y + 1) * sy;
            double acc[3] = {0, 0, 0}, wsum = 0;
            for (int j = static_cast<int>(a); j < src.height && j < b; ++j) {
                const double lo = std::max<double>(a, j), hi = std::min<double>(b, j + 1);
                const double wt = hi - lo;
                if (wt <= 0) {
                    continue;
                }
                for (int c = 0; c < 3; ++c) {
                    acc[c] += wt * tmp[(static_cast<std::size_t>(j) * w + x) * 3 + c];
                }
                wsum += wt;
            }
            for (int c = 0; c < 3; ++c) {
                out.rgb[(static_cast<std::size_t>(y) * w + x) * 3 + c] =
                    static_cast<u8>(std::clamp(std::lround(acc[c] / wsum), 0l, 255l));
            }
        }
    }
    return out;
}

namespace {

std::string Interleave(const std::vector<u8>& rgb, const std::vector<u8>& a, std::size_t texels) {
    std::string out(texels * 4, '\0');
    for (std::size_t i = 0; i < texels; ++i) {
        out[4 * i] = static_cast<char>(rgb[3 * i]);
        out[4 * i + 1] = static_cast<char>(rgb[3 * i + 1]);
        out[4 * i + 2] = static_cast<char>(rgb[3 * i + 2]);
        out[4 * i + 3] = static_cast<char>(a[3 * i]);
    }
    return out;
}

void PackName(std::string& out, const std::string& s) {
    std::string b = s.substr(0, kNameBytes - 1);
    b.resize(kNameBytes, '\0');
    out += b;
}

void Put32(std::string& s, u32 v) {
    for (int i = 0; i < 4; ++i) {
        s.push_back(static_cast<char>(v >> (8 * i)));
    }
}

void PutFloat(std::string& s, float f) {
    u32 v;
    std::memcpy(&v, &f, 4);
    Put32(s, v);
}

} // namespace

bool Convert(const std::string& hand, const std::string& rendermodels, int texture,
             const ImageLoader& load, Model* result, std::string* error) {
    try {
        const std::string name = "frame_controller_" + hand;
        const std::string directory = rendermodels + "/" + name;
        std::ifstream jf(directory + "/" + name + ".json");
        if (!jf) {
            throw ConvertError{directory + "/" + name + ".json: cannot be read"};
        }
        const json doc = json::parse(jf, nullptr, false);
        if (doc.is_discarded() || !doc.contains("components") || !doc["components"].is_object()) {
            throw ConvertError{name + ": the model description is damaged"};
        }
        const json& components = doc["components"];
        const json* grip = nullptr;
        if (components.contains("openxr_aim")) {
            grip = &components["openxr_aim"];
        } else if (components.contains("tip")) {
            grip = &components["tip"];
        }
        if (grip == nullptr || !grip->contains("component_local")) {
            throw ConvertError{name + ": no aim component"};
        }
        const json& local = (*grip)["component_local"];
        const Vec grip_origin = ToVec(local["origin"]);
        const Mat grip_rot = EulerXyz(local.contains("rotate_xyz") ? ToVec(local["rotate_xyz"]) : Vec{0, 0, 0});
        const auto to_grip = [&](const Vec& p) {
            Vec r{};
            for (int i = 0; i < 3; ++i) {
                double sum = 0;
                for (int j = 0; j < 3; ++j) {
                    sum += grip_rot[j][i] * (p[j] - grip_origin[j]);
                }
                r[i] = sum;
            }
            return r;
        };
        const auto dir_to_grip = [&](const Vec& d) {
            Vec r{};
            for (int i = 0; i < 3; ++i) {
                double sum = 0;
                for (int j = 0; j < 3; ++j) {
                    sum += grip_rot[j][i] * d[j];
                }
                r[i] = sum;
            }
            return r;
        };

        std::map<std::string, std::vector<Tri>> meshes;
        for (const auto& [key, c] : components.items()) {
            if (c.is_object() && c.contains("filename") &&
                (!c.contains("visibility") || c["visibility"].value("default", true))) {
                meshes[key] = ReadObj(directory + "/" + c["filename"].get<std::string>());
            }
        }
        if (meshes.empty()) {
            throw ConvertError{name + ": no visible component meshes"};
        }
        std::size_t full = 0;
        for (const auto& [k, t] : meshes) {
            full += t.size();
        }
        std::vector<std::string> order;
        for (const auto& [k, t] : meshes) {
            order.push_back(k);
        }
        std::sort(order.begin(), order.end(), [](const std::string& a, const std::string& b) {
            return std::make_pair(a != "body", a) < std::make_pair(b != "body", b);
        });
        std::vector<Vertex> verts;
        std::vector<u32> indices;
        std::vector<Component> comps;
        for (const std::string& key : order) {
            std::vector<Tri> tris;
            for (const Tri& tri : meshes[key]) {
                Tri t;
                for (int k = 0; k < 3; ++k) {
                    t[k] = {to_grip(tri[k].pos), tri[k].uv, Norm(dir_to_grip(tri[k].normal))};
                }
                tris.push_back(t);
            }
            std::vector<u32> i;
            const std::size_t before = verts.size();
            BuildVertices(tris, static_cast<u32>(before), &verts, &i);
            Component comp;
            comp.name = key;
            comp.first = static_cast<u32>(indices.size());
            comp.count = static_cast<u32>(i.size());
            comp.motion[kMotionPressed] = std::nanf("");
            const json& m = components[key].contains("motion") ? components[key]["motion"] : json();
            static const std::map<std::string, u32> kTypes{{"translate", 1}, {"rotate", 2}, {"joystick", 3}, {"dpad", 4}};
            if (m.is_object() && m.contains("type") && m["type"].is_string() && kTypes.count(m["type"].get<std::string>())) {
                const std::string kind = m["type"].get<std::string>();
                comp.type = kTypes.at(kind);
                std::array<double, kMotionFloats> f{};
                std::string path;
                for (const char* k : {"component_path", "joystick_path", "trigger_path", "pressed_path", "dpad_up_path"}) {
                    if (m.contains(k) && m[k].is_string() && !m[k].get<std::string>().empty()) {
                        path = m[k].get<std::string>();
                        break;
                    }
                }
                comp.path = path;
                double pressed = std::nan("");
                double rng0 = 0, rng1 = 0;
                if (m.contains("value_mapping")) {
                    rng0 = m["value_mapping"][0].get<double>();
                    rng1 = m["value_mapping"][1].get<double>();
                }
                f[kMotionRange] = rng0;
                f[kMotionRange + 1] = rng1;
                if (m.contains("pressed_value_override")) {
                    pressed = m["pressed_value_override"].get<double>();
                }
                bool has_pressed = m.contains("pressed_value_override");
                if (kind == "translate" || kind == "rotate") {
                    const Vec a = Norm(dir_to_grip(ToVec(m["axis"])));
                    for (int c = 0; c < 3; ++c) {
                        f[kMotionAxis + c] = a[c];
                    }
                }
                if (kind == "rotate") {
                    const Vec p = to_grip(ToVec(m["pivot"]));
                    for (int c = 0; c < 3; ++c) {
                        f[kMotionOrigin + c] = p[c];
                    }
                }
                if (kind == "joystick" || kind == "dpad") {
                    const Vec center = to_grip(ToVec(m["center"]));
                    for (int c = 0; c < 3; ++c) {
                        f[kMotionOrigin + c] = center[c];
                    }
                    const Mat frame = EulerXyz(m.contains("rotate_xyz") ? ToVec(m["rotate_xyz"]) : Vec{0, 0, 0});
                    for (int c = 0; c < 3; ++c) {
                        const Vec axis = dir_to_grip({frame[0][c], frame[1][c], frame[2][c]});
                        for (int r = 0; r < 3; ++r) {
                            f[kMotionFrame + 3 * c + r] = axis[r];
                        }
                    }
                    const auto pair = [&](const char* k, int at) {
                        double a = 0, b = 0;
                        if (m.contains(k)) {
                            a = m[k][0].get<double>();
                            b = m[k][1].get<double>();
                        }
                        f[at] = a;
                        f[at + 1] = b;
                    };
                    pair("joystick_rotation_x", kMotionRotX);
                    pair("joystick_rotation_y", kMotionRotY);
                    const Vec press = m.contains("press_translate") ? ToVec(m["press_translate"]) : Vec{0, 0, 0};
                    Vec v{};
                    for (int r = 0; r < 3; ++r) {
                        double sum = 0;
                        for (int c = 0; c < 3; ++c) {
                            sum += frame[r][c] * press[c];
                        }
                        v[r] = sum;
                    }
                    const Vec g = dir_to_grip(v);
                    for (int c = 0; c < 3; ++c) {
                        f[kMotionPress + c] = g[c];
                    }
                }
                for (int k = 0; k < kMotionFloats; ++k) {
                    comp.motion[k] = static_cast<float>(f[k]);
                }
                comp.motion[kMotionPressed] = has_pressed ? static_cast<float>(pressed) : std::nanf("");
            }
            indices.insert(indices.end(), i.begin(), i.end());
            comps.push_back(std::move(comp));
        }
        if (verts.size() >= (1u << 22)) {
            throw ConvertError{fmt::format("{}: {} vertices, more than the client reads", name, verts.size())};
        }

        // Textures
        Image colour;
        if (!load(directory + "/" + name + "_color.png", &colour)) {
            throw ConvertError{directory + "/" + name + "_color.png: cannot be read"};
        }
        if (texture > 0) {
            colour = Scale(colour, texture, texture);
        }
        const int w = colour.width, h = colour.height;
        const std::size_t texels = static_cast<std::size_t>(w) * h;
        const auto optional = [&](const char* kind, std::array<u8, 3> flat) {
            Image img;
            if (load(directory + "/" + name + "_" + kind + ".png", &img)) {
                img = Scale(img, w, h);
                return img.rgb;
            }
            std::vector<u8> rgb(texels * 3);
            for (std::size_t i = 0; i < texels; ++i) {
                std::memcpy(&rgb[3 * i], flat.data(), 3);
            }
            return rgb;
        };
        const std::vector<u8> ao = optional("ao", {255, 255, 255});
        std::vector<u8> normal = optional("normal", {128, 128, 255});
        const std::vector<u8> rough = optional("rough", {128, 128, 128});
        // Valve's normal maps have DirectX's green; the client's shader reads OpenGL's.
        for (std::size_t i = 0; i < texels; ++i) {
            normal[3 * i + 1] = static_cast<u8>(255 - normal[3 * i + 1]);
        }
        const std::string colour_ao = Interleave(colour.rgb, ao, texels);
        const std::string surface = Interleave(normal, rough, texels);

        std::string out = "MXFM";
        for (const u32 v : {kVersion, static_cast<u32>(verts.size()), static_cast<u32>(indices.size()),
                            static_cast<u32>(comps.size()), static_cast<u32>(w), static_cast<u32>(h), kFlags}) {
            Put32(out, v);
        }
        for (const Component& c : comps) {
            PackName(out, c.name);
            PackName(out, c.path);
            Put32(out, c.first);
            Put32(out, c.count);
            Put32(out, c.type);
            Put32(out, 0);
            for (const float f : c.motion) {
                PutFloat(out, f);
            }
        }
        for (const Vertex& v : verts) {
            for (const float f : v.v) {
                PutFloat(out, f);
            }
        }
        for (const u32 i : indices) {
            Put32(out, i);
        }
        out += colour_ao;
        out += surface;
        result->bytes = std::move(out);
        result->triangles = static_cast<int>(indices.size() / 3);
        result->vertices = static_cast<int>(verts.size());
        result->components = static_cast<int>(comps.size());
        result->tex_w = w;
        result->tex_h = h;
        std::printf("%s: %zu triangles of %zu, %zu vertices, %zu components, textures %dx%d\n", hand.c_str(),
                    indices.size() / 3, full, verts.size(), comps.size(), w, h);
        return true;
    } catch (const ConvertError& e) {
        *error = e.text;
    } catch (const json::exception& e) {
        *error = std::string("the model description is damaged: ") + e.what();
    } catch (const std::bad_alloc&) {
        *error = "out of memory";
    }
    return false;
}

int Run(const std::vector<std::string>& args, const ImageLoader& load) {
    if (args.empty()) {
        std::fprintf(stderr, "usage: --frame-controller-models OUTDIR [SteamVR rendermodels dir]\n");
        return 2;
    }
    const std::string out = args[0];
    const std::string src = args.size() > 1 ? args[1] : kRenderModels;
    std::error_code ec;
    if (!fs::is_directory(src, ec)) {
        std::fprintf(stderr, "%s: no Steam Frame controller models here (is SteamVR installed?)\n", src.c_str());
        return 1;
    }
    int texture = 1024;
    if (const char* env = std::getenv("AZAHARFRAME_MODEL_TEXTURE")) {
        texture = std::atoi(env);
    }
    Model models[2];
    const char* hands[2] = {"left", "right"};
    for (int i = 0; i < 2; ++i) {
        std::string error;
        if (!Convert(hands[i], src, texture, load, &models[i], &error)) {
            std::fprintf(stderr, "could not convert the Steam Frame controller models from %s: %s\n",
                         src.c_str(), error.c_str());
            return 1;
        }
    }
    fs::create_directories(out, ec); // both hands or neither
    for (int i = 0; i < 2; ++i) {
        const std::string path = fmt::format("{}/frame_controller_{}.model", out, hands[i]);
        const std::string tmp = path + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary);
            f.write(models[i].bytes.data(), static_cast<std::streamsize>(models[i].bytes.size()));
            if (!f) {
                std::fprintf(stderr, "could not write %s\n", path.c_str());
                return 1;
            }
        }
        fs::rename(tmp, path, ec);
        if (ec) {
            return 1;
        }
    }
    return 0;
}

} // namespace FrameControllerModels
