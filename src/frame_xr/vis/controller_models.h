// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame
// The Steam Frame controllers drawn in VR ("Show Controllers"): SteamVR's own models of them
// (model_file.h), lit as in Moonlight XR Native and with their buttons, sticks and triggers
// following the controls. AzaharFrame ctrlfront: one image, submitted as two eye quads over the
// screens and panels, cut where a screen or panel is nearer or the choice keeps a screen in
// front ("Controllers in Front of Screen": Primary, Secondary, Both), or as a projection
// layer of its own, which SteamVR puts under every quad (Off); model_plane.h explains why.
// AzaharFrame ctrlglitch: the eye quads are a canvas that stays put in the room, the image drawn
// onto it from where the eyes are, so SteamVR taking up an image and a place from different
// refreshes no longer moves the controllers; the canvas moves rarely, through a handover, and the
// models are drawn where the controller will be on the refresh their image is on view
// (model_plane.h, input/pose_guard.h).
//
// This is the one part of frame_xr that renders on the XR thread while a game runs, so it keeps
// to the rules the screens keep: it never waits for the GPU on the XR thread, it holds the queue
// lock only around vkQueueSubmit and the swapchain calls, and an image is released only on a
// later frame once its fence has signalled, the way the renderer releases the screens. Until then
// the layer shows the image released before, with the eye poses it was drawn for, so SteamVR
// moves it with the head correctly; only the controllers can lag a frame behind when the GPU is
// busy. At most one image is in flight. The pass is small: two models at about half the eye
// resolution (never more than 1000 pixels high, so Valve's foveation layer leaves it alone),
// 4x MSAA where the GPU has it.
//
// The files are read, and the shaders compiled, on a thread of their own the first time models
// are wanted; the meshes and textures go to the GPU with a fence that is polled, too.

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>
#include <string>
#include <thread>

#include "frame_xr/frame_xr.h"
#include "frame_xr/input/types.h"
#include "frame_xr/vis/model_file.h"
#include "frame_xr/vis/model_plane.h"  // AzaharFrame ctrlfront
#include "frame_xr/vis/muzzle_flash.h" // AzaharFrame kiuflash
#include "frame_xr/vis/weapon_fit.h"   // AzaharFrame kiuguns

namespace FrameXr {
class Runtime;
}

namespace FrameXr::Vis {

/// The folder of the model files: AZAHARFRAME_MODELS, else <user dir>/frame-models (the
/// launcher and the installer write them there).
std::string ControllerModelDir();
/// Reads both hands' files from it (any thread). False with the file and the reason in `error`.
bool ReadControllerModels(std::array<ModelFile, 2>& files, std::string& error);

class ControllerModels {
public:
    ControllerModels();
    ~ControllerModels();
    ControllerModels(const ControllerModels&) = delete;
    ControllerModels& operator=(const ControllerModels&) = delete;

    struct Hand {
        bool shown = false;
        Input::Posef aim; ///< LOCAL space, the display time of the frame
        float fade = 1.0f;
        ModelInputs input{};
        /// AzaharFrame kiuguns: Kid Icarus: Uprising's weapon to draw in place of this hand's
        /// controller (core/frame_weapon_models.h, fitted by weapon_fit.h), or -1. The
        /// controller is drawn until the weapon's files are read and on the GPU.
        int weapon = -1;
        /// AzaharFrame kiuflash: the weapon's muzzle flash this frame (muzzle_flash.h), drawn at
        /// its muzzle with its own flash cards when the weapon is.
        FlashFrame flash;
    };

    /// XR thread, each frame while models are wanted (anything shown or not): reads the files
    /// the first time, sets up the GPU side, releases a finished image and draws the next one.
    /// AzaharFrame ctrlfront: `choice`, how the image goes out; `occluders` (not Off): the
    /// screens and panels whose depth the next image is drawn against.
    void Frame(Runtime& rt, const ViewPose& views, const std::array<Hand, 2>& hands,
               ControllersInFront choice = ControllersInFront::Off,
               const std::vector<Occluder>& occluders = {});
    /// AzaharFrame kiuguns: the weapon's muzzle in `hand`'s aim space, while the weapon is drawn
    /// there (Laser Sight's beam starts at it); the fit of the last Frame.
    std::optional<std::array<float, 3>> WeaponMuzzle(int hand) const {
        return hand == muzzle_hand ? muzzle : std::nullopt;
    }
    /// The layer for this frame's xrEndFrame, or null (also when the image goes out as the eye
    /// quads). Valid until the next Frame.
    const XrCompositionLayerBaseHeader* Layer() const;
    /// AzaharFrame ctrlfront: the image as two quads, the left eye's and the right eye's
    /// (any choice but Off); false when there is none this frame.
    bool EyeQuads(std::array<Quad, 2>& out) const;
    /// XR thread, before the session ends: everything on the GPU (the files stay read).
    void DestroyXr();

private:
    void Setup(Runtime& rt);
    void Record(const ViewPose& views, const std::array<Hand, 2>& hands,
                const std::vector<Occluder>& occluders);
    /// AzaharFrame ctrlglitch: the shown hands' bounding spheres (LOCAL), and how many.
    std::array<ModelSphere, 2> Spheres(const std::array<Hand, 2>& hands) const;
    static std::size_t SphereCount(const std::array<Hand, 2>& hands);
    /// AzaharFrame ctrlfront: the pass's GPU time (timestamps), read once the image's fence has
    /// signalled; logged every kGpuLogSeconds while the models show.
    void NoteGpuTime(int64_t now_ns);
    /// AzaharFrame kiuguns: reads, uploads and swaps the weapon in hand (XR thread, no waits).
    void UpdateWeapon(int id);
    void UploadWeapon();
    void FitWeapon(const ViewPose& views, const std::array<Hand, 2>& hands);
    std::vector<FitPart> weapon_fits; ///< this frame's, for the weapon's hand
    float weapon_scale = 0.0f;        ///< AzaharFrame kiuflash: the fit's metres per game unit
    float weapon_clip = 0.0f;         ///< FitClipX: a palm's forearm cut (weapon space)
    bool palm_elbow = false;          ///< the palm's forearm drawn apart, turned to the elbow
    FitMat palm_turn{};               ///< that turn about the wrist (LOCAL space)
    std::array<float, 3> palm_dir{};  ///< the smoothed forearm direction (LOCAL)
    int64_t palm_time_ns = 0;
    int muzzle_hand = -1;
    std::optional<std::array<float, 3>> muzzle;

    struct Files;
    struct Gpu;
    struct WeaponLoad;
    std::unique_ptr<WeaponLoad> weapon_load; ///< the read under way or done (loader thread)
    std::thread weapon_loader;
    std::atomic<int> weapon_load_state{0}; ///< 0 none, 1 reading, 2 read
    std::unique_ptr<Files> files;
    std::unique_ptr<Gpu> gpu;
    std::thread loader;
    std::atomic<int> load_state{0}; ///< 0 not started, 1 reading, 2 read, 3 failed
    bool layer_ready = false;
    ControllersInFront over_ = ControllersInFront::Off; ///< AzaharFrame ctrlfront: this frame's
};

} // namespace FrameXr::Vis
