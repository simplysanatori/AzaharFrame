// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>

#include "common/logging/log.h"
#include "frame_xr/controls.h"
#include "frame_xr/immersive.h"
#include "frame_xr/input/geometry.h"
#include "frame_xr/picker/library.h"
#include "frame_xr/picker/library_loader.h"
#include "frame_xr/picker/logo_loader.h"
#include "frame_xr/picker/picker.h"
#include "frame_xr/session.h"

namespace FrameXr {

namespace {

namespace picker = frame_xr::picker;

// The picker panel: 1.6 m wide (1536 x 1024 texels), 2 m ahead at eye height.
constexpr float kPickerWidth = 1.6f;
constexpr float kPickerHeight = kPickerWidth * 1024.0f / 1536.0f;
// The bottom screen relative to the top one (CitraVR GameSurfaceLayer.cpp:125-139):
// 0.75 m lower, 0.5 m nearer, tilted 45 degrees back. It follows the top screen.
constexpr Input::Vec3f kBottomOffset{0.0f, -0.75f, 0.5f};
constexpr float kBottomPitch = -0.785398163f;
// The top screen stays at least this far ahead (the bottom one is 0.5 m nearer).
constexpr float kNearestTopZ = -1.0f;
// The pointer dot texture.
constexpr uint32_t kDotSize = 64;
constexpr float kStickPage = 0.6f;

Input::Posef ToInput(const XrPosef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef ToXr(const Input::Posef& p) {
    return {{p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w},
            {p.position.x, p.position.y, p.position.z}};
}

XrPosef BottomFromTop(const XrPosef& top) {
    const Input::Posef t = ToInput(top);
    const Input::Quatf pitch{std::sin(kBottomPitch / 2), 0.0f, 0.0f, std::cos(kBottomPitch / 2)};
    Input::Posef b;
    b.position = t.position + Input::Rotate(t.orientation, kBottomOffset);
    b.orientation = Input::Normalized(Input::Multiply(t.orientation, pitch));
    return ToXr(b);
}

// A white dot with a dark rim, premultiplied alpha, sRGB values.
std::vector<uint8_t> MakeDot() {
    std::vector<uint8_t> px(kDotSize * kDotSize * 4);
    const float c = (kDotSize - 1) / 2.0f;
    const float r = kDotSize / 2.0f - 1.0f;
    for (uint32_t y = 0; y < kDotSize; y++) {
        for (uint32_t x = 0; x < kDotSize; x++) {
            const float d = std::hypot(x - c, y - c);
            const float alpha = std::clamp(r - d, 0.0f, 1.0f);
            const float white = std::clamp(r * 0.72f - d, 0.0f, 1.0f);
            const float v = white * 255.0f + (1.0f - white) * 40.0f;
            uint8_t* p = &px[(y * kDotSize + x) * 4];
            p[0] = p[1] = p[2] = static_cast<uint8_t>(v * alpha);
            p[3] = static_cast<uint8_t>(alpha * 255.0f);
        }
    }
    return px;
}

} // namespace

VrUi::VrUi() = default;
VrUi::~VrUi() = default;

bool VrUi::Create(Runtime& rt) {
    actions_ok = actions.Create(rt.instance, rt.HasExtension(Input::Actions::kFrameControllerExtension));
    if (actions_ok) {
        actions_ok = actions.Attach(rt.session);
    }
    if (!actions_ok) {
        LOG_ERROR(Frontend, "xr: controller actions unavailable; no VR input");
    }

    uploader.Init(rt.vulkan->Device(), rt.vulkan->PhysicalDevice(), rt.vulkan->QueueFamily(),
                  rt.queue_lock);
    if (uploader.CreateChain(rt.session, rt.swapchain_format, kDotSize, kDotSize, cursor_chain)) {
        const auto dot = MakeDot();
        uploader.Upload(cursor_chain, dot.data());
    }

    library = std::make_shared<picker::Library>(picker::ReadMetadataWithLoader,
                                                picker::DefaultIconCacheDir());
    picker = std::make_unique<picker::Picker>(library);
    { // AzaharFrame: the "Azahar for Frame" lockup for the picker's header (text when missing).
        std::vector<uint8_t> logo;
        int logo_w = 0, logo_h = 0;
        const std::string logo_path = picker::FindArtFile("logo.png");
        if (!logo_path.empty() && picker::LoadPngFile(logo_path, logo, logo_w, logo_h)) {
            picker->SetLogo(logo, logo_w, logo_h);
        } else {
            LOG_WARNING(Frontend, "xr: art/logo.png not found, the picker shows the name as text");
        }
    }
    return true;
}

void VrUi::Destroy() {
    if (bridge_live) {
        bridge.Release();
        bridge_live = false;
    }
    mirror.Destroy(uploader);
    uploader.DestroyChain(picker_chain);
    uploader.DestroyChain(cursor_chain);
    uploader.Shutdown();
    actions.Destroy();
    picker.reset();
    library.reset();
}

void VrUi::SetFrontendHooks(FrontendHooks new_hooks) {
    std::scoped_lock lock{hooks_mutex};
    hooks = std::move(new_hooks);
}

void VrUi::SetGameRunning(bool running) {
    game_running = running;
    launch_pending = false;
}

void VrUi::SetHomePanel(std::function<void(const HomePanelEvent&)> on_event) {
    mirror.SetHandler(std::move(on_event));
}

void VrUi::SetHomePanelImage(std::shared_ptr<const HomePanelImage> image) {
    mirror.SetImage(std::move(image));
}

void VrUi::SetLibraryDirs(const std::vector<LibraryDir>& dirs) {
    if (!library || mirror.Enabled()) {
        return; // the mirrored Qt window has its own game list
    }
    std::vector<picker::GameDir> out;
    for (const auto& d : dirs) {
        out.push_back({d.path, d.deep_scan, d.max_depth});
    }
    library->SetDirs(std::move(out));
    library->Rescan();
}

void VrUi::ResetPlacement(Runtime& rt) {
    SetScreenPlacement(DefaultScreenPlacement());
    static_cast<void>(rt);
}

void VrUi::AddCursor(const XrPosef& pose, float size_m, std::vector<Quad>& layers) {
    if (!cursor_chain.has_image) {
        return;
    }
    Quad q;
    q.swapchain = cursor_chain.handle;
    q.image_rect = {{0, 0}, {static_cast<int32_t>(kDotSize), static_cast<int32_t>(kDotSize)}};
    q.pose = pose;
    // A hair in front of the panel so it is not hidden by it.
    const Input::Vec3f forward = Input::Rotate(ToInput(pose).orientation, {0, 0, 0.005f});
    q.pose.position.x += forward.x;
    q.pose.position.y += forward.y;
    q.pose.position.z += forward.z;
    q.size = {size_m, size_m};
    q.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    layers.push_back(q);
}

void VrUi::Frame(Runtime& rt, const FrameInfo& info, std::vector<Quad>& layers) {
    const bool focused = info.state == XR_SESSION_STATE_FOCUSED;
    Input::InputFrame in;
    in.time_ns = info.display_time;
    const bool have_input = focused && actions_ok &&
                            actions.Sync(info.session, info.local_space, info.display_time, in);

    if (launch_pending && info.display_time - launch_time > 10'000'000'000LL) {
        launch_pending = false; // the boot failed or was refused; allow another pick
    }

    if (game_running) {
        if (picker_shown) {
            picker_shown = false;
        }
        mirror.Hide();
        GameFrame(rt, info, in, have_input, layers);
    } else {
        if (bridge_live) {
            bridge.Release();
            bridge_live = false;
        }
        if (mirror.Enabled()) {
            std::optional<XrPosef> cursor;
            mirror.Frame(rt.session, rt.swapchain_format, uploader, info, in, have_input, layers,
                         cursor);
            if (cursor) {
                AddCursor(*cursor, 0.02f, layers);
            }
        } else {
            PickerFrame(rt, info, in, have_input, layers);
        }
    }
    last_time = info.display_time;
}

void VrUi::GameFrame(Runtime& rt, const FrameInfo& info, const Input::InputFrame& in,
                     bool have_input, std::vector<Quad>& layers) {
    if (!have_input || !info.screens_live) {
        if (bridge_live) {
            bridge.Release();
            bridge_live = false;
        }
        return;
    }
    ScreenPlacement place = GetScreenPlacement();
    Input::PanelGeometry panels;
    panels.top = {ToInput(place.top_pose), place.top_size.width, place.top_size.height, true};
    panels.bottom = {ToInput(place.bottom_pose), place.bottom_size.width,
                     place.bottom_size.height, true};
    Immersive::OnGameInput(in, panels.top.visible, panels.bottom.visible); // immersive.cpp
    const Input::MapperOutput& out = bridge.Process(in, panels);
    bridge_live = true;

    const auto& cmd = out.commands;
    if (cmd.reset_panels) {
        ResetPlacement(rt);
    } else if (cmd.top_drag_active || cmd.top_depth_delta_z != 0.0f) {
        Input::Posef top = ToInput(place.top_pose);
        if (cmd.top_drag_active) {
            Input::DragLimits limits;
            limits.max_z = kNearestTopZ;
            // The bottom screen follows the top one: no distance check between them.
            if (auto moved = Input::SolveSphereDrag(top, cmd.top_drag_target, {0, 0, 0}, nullptr,
                                                    limits)) {
                top = *moved;
            }
        }
        if (cmd.top_depth_delta_z != 0.0f) {
            top.position.z = Input::ApplyDepth(top.position.z, cmd.top_depth_delta_z, kNearestTopZ);
        }
        place.top_pose = ToXr(top);
        place.bottom_pose = BottomFromTop(place.top_pose);
        SetScreenPlacement(place);
    }
    // From 20 % on, so a short View press (Select) logs nothing.
    if (cmd.stop_hold_progress > 0.2f && !stop_hold_logged) {
        stop_hold_logged = true;
        LOG_INFO(Frontend, "xr: View hold started (the game stops when it completes)");
    } else if (cmd.stop_hold_progress <= 0.0f && stop_hold_logged && !cmd.stop_game) {
        stop_hold_logged = false;
        LOG_INFO(Frontend, "xr: View hold let go before the stop");
    }
    if (cmd.stop_game) {
        stop_hold_logged = false;
        std::scoped_lock lock{hooks_mutex};
        if (hooks.stop_game) {
            LOG_INFO(Frontend, "xr: View held, stopping the game");
            hooks.stop_game();
        }
    }
    if (out.cursor.visible) {
        const XrPosef pose{{out.cursor.orientation.x, out.cursor.orientation.y,
                            out.cursor.orientation.z, out.cursor.orientation.w},
                           {out.cursor.position.x, out.cursor.position.y, out.cursor.position.z}};
        AddCursor(pose, out.cursor.scale_m, layers);
    }
}

void VrUi::PickerFrame(Runtime& rt, const FrameInfo& info, const Input::InputFrame& in,
                       bool have_input, std::vector<Quad>& layers) {
    if (!picker || !info.should_render) {
        return;
    }
    if (!picker_shown) {
        picker_shown = true;
        picker->Invalidate();
        prev_select = prev_back = true; // no select from a button already held
    }

    picker::PickerInput pin;
    pin.dt = last_time ? static_cast<float>(info.display_time - last_time) * 1e-9f : 0.0f;
    std::optional<XrPosef> cursor;
    if (have_input) {
        const Input::PanelQuad quad{ToInput(picker_pose), kPickerWidth, kPickerHeight, true};
        // The right hand first, then the left.
        for (int h : {1, 0}) {
            const Input::HandInput& hand = in.hand[h];
            if (!hand.active) {
                continue;
            }
            const Input::QuadHit hit = Input::IntersectQuad(Input::RayFromPose(hand.aim), quad);
            if (hit.in_bounds) {
                pin.hover = picker::Vec2{hit.u, hit.v};
                cursor = XrPosef{picker_pose.orientation,
                                 {hit.point_world.x, hit.point_world.y, hit.point_world.z}};
                break;
            }
        }
        const bool select = in.a || in.hand[0].trigger > 0.55f || in.hand[1].trigger > 0.55f;
        const bool back = in.b;
        pin.select = select && !prev_select;
        pin.back = back && !prev_back;
        prev_select = select;
        prev_back = back;
        const Input::Vec2f stick = std::abs(in.hand[1].stick.x) + std::abs(in.hand[1].stick.y) >
                                           std::abs(in.hand[0].stick.x) + std::abs(in.hand[0].stick.y)
                                       ? in.hand[1].stick
                                       : in.hand[0].stick;
        const float s = std::abs(stick.x) > std::abs(stick.y) ? stick.x : -stick.y;
        pin.scroll = std::abs(s) > kStickPage ? s : 0.0f;
    }

    // One log line per finished scan.
    if (library && !library->IsScanning() && library->Generation() != logged_generation) {
        logged_generation = library->Generation();
        const auto snapshot = library->Snapshot();
        LOG_INFO(Frontend, "xr: picker: {} games", snapshot ? snapshot->size() : 0);
    }

    const picker::PickerResult r = picker->Update(pin);
    if (!picker_chain.handle) {
        uploader.CreateChain(rt.session, rt.swapchain_format, picker->Width(), picker->Height(),
                             picker_chain);
        picker->Invalidate();
    }
    if (r.dirty || !picker_chain.has_image) {
        uploader.Upload(picker_chain, picker->Pixels());
    }
    if (r.launch_path && !launch_pending) {
        std::scoped_lock lock{hooks_mutex};
        if (hooks.launch_game) {
            LOG_INFO(Frontend, "xr: picker launches {}", *r.launch_path);
            launch_pending = true;
            launch_time = info.display_time;
            hooks.launch_game(*r.launch_path);
        }
    }

    if (picker_chain.has_image) {
        Quad q;
        q.swapchain = picker_chain.handle;
        q.image_rect = {{0, 0},
                        {static_cast<int32_t>(picker_chain.width),
                         static_cast<int32_t>(picker_chain.height)}};
        q.pose = picker_pose;
        q.size = {kPickerWidth, kPickerHeight};
        layers.push_back(q);
    }
    if (cursor) {
        AddCursor(*cursor, 0.02f, layers);
    }
}

} // namespace FrameXr
