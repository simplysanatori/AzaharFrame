// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame kiuguns: Kid Icarus: Uprising's weapons as models Azahar for Frame draws in the
// aiming hand (frame_xr/vis/controller_models.h). They are made on the player's own machine from
// the player's own copy of the game, never shipped, never uploaded: the game's RomFS through
// Azahar's loader (a cartridge or an installed title, with LayeredFS mods), each weapon's
// `player/m<class><index>.zrc` (LZ11, darc), its `fmdl/00.bcres` (CGFX: frame_cgfx.h) and
// `json/00.json` (PSLB: the muzzle and fire parameters).
//
// Each file holds Pit as well as the weapon; only the weapon's meshes are taken (not the
// materials Body and Face, not the meshes the game hides by default), frozen in the bind pose and
// put in the space of the class's anchor bone ("weapon space", game units; frame_xr fits that
// space to the hand). One MXFM v2 file per material (flags kModelFlagWeaponSpace, with the
// texture's wrap modes), its components the sides ("C" whole, "R" and "L" for the claws' and
// orbitars' two halves, each in its own hand's or orbitar's space).
//
//   <user dir>/frame-models/games/<title id>/kiu-weapons/w<id>.model, w<id>-<n>.model,
//   weapons.txt (the manifest, plain lines: per weapon its class, anchor, muzzle, fire parameters, parts and
//   the hash of the .zrc it was made from)
//
// A weapon is made again when its .zrc changes (a mod) or the converter's version does. The run
// for a booted game is on a thread of its own at the lowest priority and never waits on the game.

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace FileSys {
class RomFSReader;
}

namespace FrameWeapons {

constexpr int kClasses = 9;
constexpr int kPerClass = 12;
constexpr int kWeapons = kClasses * kPerClass; // ids 0..107 (the game's WPNID order)
/// The manifest's and files' version: a change makes every weapon again.
constexpr int kConverterVersion = 3;

enum Class : int { kBlade, kStaff, kClaws, kBow, kPalm, kClub, kCannon, kOrbitars, kArm };

constexpr int ClassOf(int id) {
    return id / kPerClass;
}
/// "blade", "staff", ... (the English names; the game's own are Rifle, Snipe, Nail, ...).
const char* ClassName(int cls);
/// The game's file for a weapon id: "m%03d.zrc" with class * 100 + index.
std::string ZrcName(int id);

enum class Side : u8 { Centre, Right, Left };
const char* SideName(Side side); ///< "C", "R", "L" (the component names)

struct Part {
    std::string file;     ///< in the weapon folder
    std::string material; ///< the game's material name
    std::vector<Side> sides;
    u32 triangles = 0;
    u32 wrap_u = 2, wrap_v = 2; ///< PICA wrap modes
    bool glow = false;          ///< an effect material (drawn plain in this version)
    /// The game's face culling for it (0 both sides, else one side). Every part is drawn from
    /// both sides here, each triangle wound to agree with its normals so its back is lit right.
    u32 cull = 0;
    /// The custom texture pack's PNG drawn instead of the game's texture ("" for the game's),
    /// read from the pack when the model is loaded (never copied: the pack stays the one copy),
    /// and whether the pack flips its PNGs (pack.json's flip_png_files).
    std::string replaced;
    bool replaced_flip = true;
};

struct Weapon {
    int id = -1;
    int cls = -1;
    std::string wpnid;  ///< the game's WPNID_WPN_..., from its json
    std::string anchor; ///< the anchor bone (the right side's for a pair)
    std::array<float, 3> muzzle{};      ///< weapon space (game units)
    bool muzzle_valid = false;
    std::array<float, 3> bounds_min{}, bounds_max{}; ///< the centre or right side, weapon space
    std::array<float, 2> charge_time{}, rapid_fire_time{}; ///< as the json gives them
    std::array<float, 3> rapid_normal{}; ///< fireTime_, interval_, bulletNum_ (RAPID_NORMAL)
    u64 source_hash = 0;
    /// The colour textures' hashes (custom texture packs' new hash) and what the pack had for
    /// them when this was made (PackKey): a changed pack makes the weapon again.
    std::vector<u64> texture_hashes;
    u64 pack_key = 0;
    std::vector<Part> parts;
};

struct Manifest {
    int version = 0;
    u64 title_id = 0;
    std::string region; ///< the RomFS folder the files came from ("eu", "us", "jp")
    std::vector<Weapon> weapons;
    const Weapon* Find(int id) const;
};

bool ReadManifest(const std::string& path, Manifest& out, std::string& error);
bool WriteManifest(const std::string& path, const Manifest& m, std::string& error);

/// The game's custom texture pack (Use Custom Textures on): the colour textures by hash, as
/// Azahar's own manager finds them (video_core CustomTexManager::IndexPack). A weapon texture the
/// pack replaces is taken from it at its own size, so the HD pack that upgrades the weapon in the
/// game upgrades it in the hand too. PNG files only (others keep the game's texture).
struct TexturePack {
    bool on = false;
    bool flip_png = true; ///< pack.json's flip_png_files
    std::map<u64, std::string> files;
};
TexturePack ReadTexturePack(u64 title_id);
/// What the pack holds for these hashes (the files' names, sizes and times; 0 for nothing).
u64 PackKey(const TexturePack* pack, const std::vector<u64>& hashes);

/// One weapon's .zrc made into its files (in memory). Pure but for the pack's files: tested.
struct Converted {
    Weapon info;
    std::vector<std::pair<std::string, std::vector<u8>>> files;
};
bool ConvertWeapon(int id, std::span<const u8> zrc, Converted& out, std::string& error,
                   const TexturePack* pack = nullptr);

/// A texture pack's PNG (Part::replaced) for a model's colour image: RGBA8, rows in the game's
/// memory order, alpha 255. False when it cannot be read (the model's own texture stays).
bool LoadPackTexture(const std::string& path, bool flip_png, std::vector<u8>& rgba, u32& width,
                     u32& height);

/// The folder of a game's files: AZAHARFRAME_MODELS or <user dir>/frame-models/, then
/// games/<title id>/kiu-weapons/ (with the trailing '/').
std::string WeaponDir(u64 title_id);

struct RunResult {
    bool ok = false;
    int made = 0, kept = 0, failed = 0;
    int replaced = 0; ///< textures taken from the texture pack in what was made
    std::string region;
    std::string error;
};
/// Makes every weapon missing or stale in `dir` from a game's RomFS (the command line's run; the
/// booted game makes only the weapons asked for: Request(id)). `cancel` is polled between
/// weapons. Logs a line per failed weapon and none per good one.
RunResult ConvertGame(std::shared_ptr<FileSys::RomFSReader> romfs, u64 title_id,
                      const std::string& dir, const std::atomic<bool>* cancel = nullptr,
                      const TexturePack* pack = nullptr);

/// `azahar --frame-weapon-models GAME [OUTDIR] [--textures]`: the same for a game file (OUTDIR:
/// the user folder's; --textures: with the game's custom texture pack). Prints a summary.
int RunCli(const std::vector<std::string>& args);

// ---- The booted game (the Qt thread calls these; the XR thread polls State) ----

/// At the boot of a Kid Icarus: Uprising title: keeps its RomFS reader. `start`: open the RomFS's
/// tables now on the worker thread (else at the first Request). Weapons are made one at a time,
/// only when asked for (Request(id)): the equipped one.
/// `custom_textures`: Use Custom Textures is on for the game (its pack is read for the run).
void OnGameBoot(u64 title_id, std::shared_ptr<FileSys::RomFSReader> romfs, bool start,
                bool custom_textures);
/// At the game's stop: stops a run between weapons and lets the reader go.
void OnGameStop();
/// Any thread: starts the worker for the booted game if it has not started in this boot.
void Request();
/// Any thread: asks for one weapon (made, or found made and current, on the worker thread).
void Request(int id);

enum class State : int { None, Working, Ready, Failed };
/// The worker: Working while it opens the game's files, Ready when it can make weapons.
State CurrentState();
/// One weapon: None (not asked for this boot), Working, Ready (its files are current), Failed.
State WeaponState(int id);
/// The booted game's title id (0 for none).
u64 CurrentTitle();
/// Bumped each time a run finishes, so a reader of the files reads them again.
u32 Generation();

} // namespace FrameWeapons
