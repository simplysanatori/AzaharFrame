// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// The reader and the posing are a C++ port of Moonlight XR Native's rooms/room_gpu.c
// (mlxr_model_file_parse, openTextured, mlxr_model_pose; GPLv3, same author).

#include <cmath>
#include <cstring>
#include <fstream>

#include "frame_xr/vis/model_file.h"

namespace FrameXr::Vis {

namespace {

constexpr uint32_t kHeaderBytes = 32;
constexpr uint32_t kComponentBytes = 208;
constexpr uint32_t kNameBytes = 32;
constexpr uint32_t kTextureMax = 8192;
constexpr uint32_t kComponentsMax = 64;

enum Motion : uint32_t { kNone, kTranslate, kRotate, kJoystick, kDpad };
// The motion floats (the converter's MOTION_*).
constexpr int kOrigin = 0;
constexpr int kAxis = 3;
constexpr int kRange = 6;
constexpr int kPressed = 8;
constexpr int kFrame = 9;
constexpr int kRotX = 18;
constexpr int kRotY = 20;
constexpr int kPress = 22;

int MotionInput(const std::string& path) {
    static const struct {
        const char* path;
        int input;
    } table[] = {
        {"/input/trigger", kInTrigger},         {"/input/trigger/value", kInTrigger},
        {"/input/grip", kInGrip},               {"/input/squeeze", kInGrip},
        {"/input/squeeze/value", kInGrip},      {"/input/thumbstick", kInStickX},
        {"/input/a/click", kInA},               {"/input/b/click", kInB},
        {"/input/x/click", kInX},               {"/input/y/click", kInY},
        {"/input/menu/click", kInMenu},         {"/input/view/click", kInView},
        {"/input/system/click", kInSystem},     {"/input/bumper/click", kInBumper},
        {"/input/dpad_up/click", kInDpadUp},
    };
    for (const auto& e : table) {
        if (path == e.path) {
            return e.input;
        }
    }
    return -1;
}

uint32_t U32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4); // little endian, as the converter writes and the Frame runs
    return v;
}

using Mat = std::array<float, 16>;

Mat Identity() {
    Mat m{};
    m[0] = m[5] = m[10] = m[15] = 1.0f;
    return m;
}

// Rotation by deg about the unit axis a through the point p (column major).
Mat About(const float* a, float deg, const float* p) {
    const float r = deg * (3.14159265358979f / 180.0f);
    const float c = std::cos(r), s = std::sin(r), k = 1.0f - c;
    const float x = a[0], y = a[1], z = a[2];
    const float R[9] = {c + x * x * k,     y * x * k + z * s, z * x * k - y * s,
                        x * y * k - z * s, c + y * y * k,     z * y * k + x * s,
                        x * z * k + y * s, y * z * k - x * s, c + z * z * k};
    Mat m = Identity();
    for (int col = 0; col < 3; col++) {
        for (int row = 0; row < 3; row++) {
            m[col * 4 + row] = R[col * 3 + row];
        }
    }
    for (int row = 0; row < 3; row++) {
        m[12 + row] = p[row] - (R[row] * p[0] + R[3 + row] * p[1] + R[6 + row] * p[2]);
    }
    return m;
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float Clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

// A joystick's or d-pad's tilt about its centre in its own frame, dipped by the click.
Mat Stick(const float* mo, float x, float y, float click) {
    const float len = std::sqrt(x * x + y * y);
    if (len > 1.0f) {
        x /= len;
        y /= len;
    }
    const float ax = Lerp(mo[kRotX], mo[kRotX + 1], 0.5f + 0.5f * x);
    const float ay = Lerp(mo[kRotY], mo[kRotY + 1], 0.5f + 0.5f * y);
    const Mat rz = About(&mo[kFrame + 6], ax, mo + kOrigin);
    const Mat rx = About(&mo[kFrame], ay, mo + kOrigin);
    Mat m{};
    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float v = 0.0f;
            for (int k = 0; k < 4; k++) {
                v += rx[k * 4 + row] * rz[col * 4 + k];
            }
            m[col * 4 + row] = v;
        }
    }
    for (int row = 0; row < 3; row++) {
        m[12 + row] += mo[kPress + row] * click;
    }
    return m;
}

} // namespace

bool LoadModelFile(const std::string& path, ModelFile& out, std::string& error) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        error = "cannot be read";
        return false;
    }
    const auto size = static_cast<uint64_t>(f.tellg());
    f.seekg(0);
    uint8_t head[kHeaderBytes];
    if (size < kHeaderBytes || !f.read(reinterpret_cast<char*>(head), kHeaderBytes) ||
        std::memcmp(head, "MXFM", 4) != 0) {
        error = "not an MXFM model";
        return false;
    }
    ModelFile m;
    m.version = U32(head + 4);
    const uint32_t vertex_count = U32(head + 8);
    const uint32_t index_count = U32(head + 12);
    const uint32_t component_count = U32(head + 16);
    m.tex_width = U32(head + 20);
    m.tex_height = U32(head + 24);
    m.flags = U32(head + 28);
    if (m.version != 2 || m.flags != kModelFlagAimSpace) {
        error = "version " + std::to_string(m.version) + " flags " + std::to_string(m.flags) +
                ": not an aim space model (frame-controller-models.py --space aim)";
        return false;
    }
    if (vertex_count == 0 || vertex_count >= (1u << 22) || index_count == 0 ||
        index_count % 3 != 0 || index_count > (1u << 24) || component_count == 0 ||
        component_count > kComponentsMax || m.tex_width == 0 || m.tex_height == 0 ||
        m.tex_width > kTextureMax || m.tex_height > kTextureMax) {
        error = "sizes out of range";
        return false;
    }
    const uint64_t tex_bytes = uint64_t{m.tex_width} * m.tex_height * 4;
    const uint64_t expected = kHeaderBytes + uint64_t{component_count} * kComponentBytes +
                              uint64_t{vertex_count} * kModelVertexFloats * 4 +
                              uint64_t{index_count} * 4 + 2 * tex_bytes;
    if (expected != size) {
        error = "the sizes do not add up";
        return false;
    }
    std::vector<uint8_t> comps(std::size_t{component_count} * kComponentBytes);
    m.vertices.resize(std::size_t{vertex_count} * kModelVertexFloats);
    m.indices.resize(index_count);
    m.colour.resize(static_cast<std::size_t>(tex_bytes));
    m.surface.resize(static_cast<std::size_t>(tex_bytes));
    if (!f.read(reinterpret_cast<char*>(comps.data()), static_cast<std::streamsize>(comps.size())) ||
        !f.read(reinterpret_cast<char*>(m.vertices.data()),
                static_cast<std::streamsize>(m.vertices.size() * 4)) ||
        !f.read(reinterpret_cast<char*>(m.indices.data()),
                static_cast<std::streamsize>(m.indices.size() * 4)) ||
        !f.read(reinterpret_cast<char*>(m.colour.data()),
                static_cast<std::streamsize>(m.colour.size())) ||
        !f.read(reinterpret_cast<char*>(m.surface.data()),
                static_cast<std::streamsize>(m.surface.size()))) {
        error = "short read";
        return false;
    }
    for (const uint32_t i : m.indices) {
        if (i >= vertex_count) {
            error = "an index out of range";
            return false;
        }
    }
    for (uint32_t c = 0; c < component_count; c++) {
        const uint8_t* rec = comps.data() + std::size_t{c} * kComponentBytes;
        ModelComponent mc;
        mc.name.assign(reinterpret_cast<const char*>(rec),
                       strnlen(reinterpret_cast<const char*>(rec), kNameBytes - 1));
        mc.path.assign(reinterpret_cast<const char*>(rec + kNameBytes),
                       strnlen(reinterpret_cast<const char*>(rec + kNameBytes), kNameBytes - 1));
        mc.first_index = U32(rec + 2 * kNameBytes);
        mc.index_count = U32(rec + 2 * kNameBytes + 4);
        mc.motion_type = U32(rec + 2 * kNameBytes + 8);
        std::memcpy(mc.motion.data(), rec + 2 * kNameBytes + 16, sizeof(float) * 32);
        mc.input = MotionInput(mc.path);
        if (uint64_t{mc.first_index} + mc.index_count > index_count || mc.first_index % 3 != 0 ||
            mc.index_count % 3 != 0 || mc.motion_type > kDpad) {
            error = "component " + mc.name + " out of range";
            return false;
        }
        m.components.push_back(std::move(mc));
    }
    out = std::move(m);
    return true;
}

std::vector<ModelPart> PoseModel(const ModelFile& model, const ModelInputs& in) {
    std::vector<ModelPart> parts;
    for (const ModelComponent& mc : model.components) {
        if (mc.index_count == 0) {
            continue;
        }
        ModelPart part{mc.first_index, mc.index_count, Identity()};
        const float* mo = mc.motion.data();
        if (mc.input >= 0 && mc.input < kModelInputs) {
            float v = in[static_cast<std::size_t>(mc.input)];
            switch (mc.motion_type) {
            case kTranslate: {
                const float off = Lerp(mo[kRange], mo[kRange + 1], Clamp01(v));
                for (int k = 0; k < 3; k++) {
                    part.transform[12 + k] = mo[kAxis + k] * off;
                }
                break;
            }
            case kRotate: {
                v = Clamp01(v);
                float deg = Lerp(mo[kRange], mo[kRange + 1], v);
                if (!std::isnan(mo[kPressed])) {
                    // The trigger's and grip's last bit of travel (pressed_value_override).
                    deg = Lerp(deg, mo[kPressed], Clamp01((v - 0.9f) * 10.0f));
                }
                part.transform = About(&mo[kAxis], deg, &mo[kOrigin]);
                break;
            }
            case kJoystick:
                part.transform = Stick(mo, in[kInStickX], in[kInStickY], in[kInStickClick]);
                break;
            case kDpad: {
                const float x = in[kInDpadRight] - in[kInDpadLeft];
                const float y = in[kInDpadUp] - in[kInDpadDown];
                const float any = (in[kInDpadUp] > 0.0f || in[kInDpadDown] > 0.0f ||
                                   in[kInDpadLeft] > 0.0f || in[kInDpadRight] > 0.0f)
                                      ? 1.0f
                                      : 0.0f;
                part.transform = Stick(mo, x, y, any);
                break;
            }
            default:
                break;
            }
        }
        parts.push_back(part);
    }
    // Parts at rest whose ranges run on are one draw.
    const Mat rest = Identity();
    std::vector<ModelPart> out;
    bool prev_rest = false;
    for (const ModelPart& p : parts) {
        const bool at_rest = p.transform == rest;
        if (!out.empty() && at_rest && prev_rest &&
            out.back().first_index + out.back().index_count == p.first_index) {
            out.back().index_count += p.index_count;
            continue;
        }
        out.push_back(p);
        prev_rest = at_rest;
    }
    return out;
}

} // namespace FrameXr::Vis
