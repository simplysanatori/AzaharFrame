// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// The controller models' shaders: Moonlight XR Native's shaders/model_tex.vert and .frag (same
// author, GPLv3), with the strength of the model's fade (params.z, premultiplied out) added and
// OUT_LINEAR as a #define. Compiled at run time with Azahar's glslang (vk_shader_util), on the
// thread that reads the model files.
//
// The Frame's plastic is near black and fairly glossy, so, as under SteamVR, its shape shows in
// its highlights: a key light from above and behind the viewer, a sky and ground fill dimmed by
// the ambient occlusion, GGX highlights and a faint Fresnel sheen, all in linear light.

#pragma once

namespace FrameXr::Vis::Shaders {

inline constexpr const char* kModelVert = R"(
layout(push_constant) uniform PC {
    mat4 mvp;     // proj * view * model * part (OpenGL clip space, turned into Vulkan's below)
    vec4 eye;     // camera position, model space
    vec4 key;     // xyz: toward the key light, model space
    vec4 up;      // xyz: the room's up, model space
    vec4 params;  // x: debug view, y: base colour gain, z: fade (0..1)
} pc;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_tangent;
layout(location = 3) in vec2 a_uv;
layout(location = 0) out vec3 v_position;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec4 v_tangent;
layout(location = 3) out vec2 v_uv;

void main() {
    v_position = a_position;
    v_normal = a_normal;
    v_tangent = a_tangent;
    v_uv = a_uv;
    vec4 p = pc.mvp * vec4(a_position, 1.0);
    gl_Position = vec4(p.x, -p.y, 0.5 * (p.z + p.w), p.w);
}
)";

inline constexpr const char* kModelFrag = R"(
layout(push_constant) uniform PC {
    mat4 mvp;
    vec4 eye;
    vec4 key;
    vec4 up;
    vec4 params;
} pc;

layout(set = 0, binding = 0) uniform sampler2D u_colour;   // sRGB base colour, AO in alpha
layout(set = 0, binding = 1) uniform sampler2D u_surface;  // normal (OpenGL green), roughness in alpha

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_tangent;
layout(location = 3) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

const vec3 KEY = vec3(1.0, 0.97, 0.92) * 2.6;
const vec3 SKY = vec3(0.60, 0.64, 0.70);
const vec3 GROUND = vec3(0.10, 0.095, 0.09);
const float F0 = 0.04;
const float ROUGH_FLOOR = 0.5;
const float SPEC_GAIN = 0.7;
const float BASE_FLOOR = 0.014;
const float ENV_GAIN = 0.25;
const float PI = 3.14159265;

vec3 linearToSrgb(vec3 c) {
    c = clamp(c, 0.0, 1.0);
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}

// Linear light out (an _SRGB target encodes it), premultiplied by the fade
vec4 writeLinear(vec3 c) {
    float a = clamp(pc.params.z, 0.0, 1.0);
    vec3 v = OUT_LINEAR ? clamp(c, 0.0, 1.0) : linearToSrgb(c);
    return vec4(v * a, a);
}

vec3 sky(vec3 dir, vec3 up, float spread) {
    float t = clamp(0.5 + 0.5 * dot(dir, up) / max(spread + 0.15, 0.15), 0.0, 1.0);
    return mix(GROUND, SKY, t);
}

void main() {
    int view = int(pc.params.x + 0.5);
    vec4 base = texture(u_colour, v_uv);
    vec4 surf = texture(u_surface, v_uv);
    vec3 albedo = max(base.rgb, vec3(BASE_FLOOR)) * (pc.params.y > 0.0 ? pc.params.y : 1.0);
    float ao = base.a;

    vec3 ng = normalize(v_normal);
    if (!gl_FrontFacing) ng = -ng;
    vec3 t = v_tangent.xyz - ng * dot(ng, v_tangent.xyz);
    t = dot(t, t) > 1e-10 ? normalize(t) : vec3(1.0, 0.0, 0.0);
    vec3 b = cross(ng, t) * (v_tangent.w < 0.0 ? -1.0 : 1.0);
    vec3 nt = surf.xyz * 2.0 - 1.0;
    float ntLen = max(length(nt), 1e-3);
    vec3 n = view == 2 ? ng : normalize(t * nt.x + b * nt.y + ng * nt.z);

    if (view == 1) {
        fragColor = vec4(n * 0.5 + 0.5, 1.0);
        return;
    }
    if (view == 3) {
        fragColor = writeLinear(albedo * ao);
        return;
    }

    vec3 V = normalize(pc.eye.xyz - v_position);
    vec3 L = normalize(pc.key.xyz);
    vec3 U = normalize(pc.up.xyz);
    float NdotV = max(dot(n, V), 1e-4);
    float NdotL = max(dot(n, L), 0.0);

    float rough = mix(ROUGH_FLOOR, 1.0, clamp(surf.a, 0.0, 1.0) * 0.5);
    float a2 = rough * rough * rough * rough;
    a2 += (1.0 - ntLen) / ntLen;
    vec3 dnx = dFdx(ng), dny = dFdy(ng);
    a2 += min(2.0 * (dot(dnx, dnx) + dot(dny, dny)), 0.18);
    a2 = clamp(a2, 2e-4, 1.0);

    vec3 diffuse = albedo * (KEY * NdotL / PI + sky(n, U, 1.0) * ao);

    vec3 H = normalize(L + V);
    float NdotH = max(dot(n, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * d * d);
    float k = sqrt(a2) * 0.5;
    float G = (NdotV / (NdotV * (1.0 - k) + k)) * (NdotL / (NdotL * (1.0 - k) + k));
    float F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
    vec3 spec = KEY * SPEC_GAIN * (D * G * F / (4.0 * NdotV));

    vec3 R = reflect(-V, n);
    float fr = F0 + (max(1.0 - sqrt(sqrt(a2)), F0) - F0) * pow(1.0 - NdotV, 5.0);
    float specAo = clamp(ao * ao * 1.2, 0.0, 1.0);
    vec3 env = sky(R, U, sqrt(sqrt(a2))) * fr * specAo * ENV_GAIN;

    fragColor = writeLinear(diffuse + spec + env);
}
)";

} // namespace FrameXr::Vis::Shaders
