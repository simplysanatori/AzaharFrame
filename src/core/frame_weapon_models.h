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
#include <string_view>
#include <vector>
#include "common/common_types.h"
#include "core/frame_cgfx.h" // AzaharFrame kiuweap2: the poses (SkeletalPose, Mat34)

namespace FileSys {
class RomFSReader;
}

namespace FrameWeapons {

constexpr int kClasses = 9;
constexpr int kPerClass = 12;
constexpr int kWeapons = kClasses * kPerClass; // ids 0..107 (the game's WPNID order)
/// The manifest's and files' version: a change makes every weapon again.
/// 4 (AzaharFrame kiumodels): the parts' texture animations (`uvanim` lines).
constexpr int kConverterVersion = 4;

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

/// AzaharFrame kiumodels: how a part's texture moves (drawn on its glow pass): its texture
/// coordinates slide by (du, dv) over each loop of `frames` game frames (kGameFps a second), in
/// the model file's UV space (v = 1 - v_game). From the game's own material animation for the
/// part's material and coordinator (the class's Pit file, player/a<class>.zrc, "Wpn...UV"), or,
/// for a glow card the game does not animate, a flow along the card's length (one repeat of its
/// texture per loop, away from the anchor bone) when its texture repeats that way.
enum class UvSource : u8 { None = 0, Game = 1, Flow = 2 };
/// The game's frames a second: a battle draws 30 new pictures a second (each shown for two of
/// the 3DS's 60 refreshes: the Frame mock's pacing log, "game pictures 30 and 30 repeats"), and
/// its animations step once a picture (Pit's palm run cycle, 26 frames, is 0.87 s).
constexpr float kGameFps = 30.0f;
constexpr float kFlowFrames = 75.0f; ///< a flow's loop: 2.5 s

struct UvMotion {
    UvSource source = UvSource::None;
    float frames = 0.0f;
    float du = 0.0f, dv = 0.0f;
    float turns = 0.0f; ///< the coordinator's turn over a loop (whole turns; not drawn yet)
};

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
    UvMotion uv; ///< AzaharFrame kiumodels
};

// ---- AzaharFrame kiuflash: the muzzle flashes ----
//
// Each weapon's `character/effect.bcres` holds its own flashes as particle models
// (frame_cgfx.h ReadParticleModels): one for rapid fire, one for the charged shot. They are made
// into flash cards (sprites at the muzzle, drawn added to what is behind for a moment after each
// real shot: frame_xr/vis/muzzle_flash.h), in files that join the weapon's own:
// w<id>-f<n>.model, one per texture, its components the cards. No particle is simulated: a
// flash is at most kFlashCardsMax cards from its model's sets (smoke left out), each with its
// set's texture, colour mode and colour, and its size in the game's units.

enum class FlashKind : u8 { Rapid, Charged };
constexpr int kFlashCardsMax = 4;

/// The effect models a weapon's flashes are, from their names only (pure): rapid fire's
/// ("Ptc<Class><NN>RapidFlash...", claws' "RapidShot", the first orbitars' "Bullet01_Flash",
/// the first blade's "ShotA", clubs' "Muzzle") and the charged shot's ("ShotFlash...", the
/// orbitars' "Bullet02_Flash", "ShotB"). A kind the file lacks takes the other's; "" for none.
struct FlashModels {
    std::string rapid, charged;
};
FlashModels PickFlashModels(int id, const std::vector<std::string>& names);

/// A card's values, in its MXFM component's motion floats (the component's name is its kind,
/// "rapid" or "charged", its path its role, "face" or "fan"; its quad in the card's plane, game
/// units: a face is centred on the muzzle and turned to the eye, a fan starts at the muzzle and
/// lies along the ray, turned about it to the eye).
enum FlashValue : int {
    kFlashMode,   ///< FlashMode
    kFlashRed,    ///< the set's colour, 0 to 1 (as the game's: not linear)
    kFlashGreen,
    kFlashBlue,
    kFlashCore,   ///< a two-tone mask's white core (the colour's top byte), 0 to 1
    kFlashOffset, ///< how far ahead of the muzzle along the ray (game units)
    kFlashGrow,   ///< the size at the end of the flash against its start (1 to 2)
    kFlashValues,
};
/// How a card's texture is coloured (the set's Type, low byte): the texture's own colour times
/// the set's colour; its red channel as light times the colour; a two-tone mask (RGB565 sprites
/// with masks in their channels: green the shape in the colour, red a white core).
enum FlashMode : int { kFlashTexture, kFlashLuminance, kFlashTwoTone };

struct FlashPart {
    std::string file;    ///< in the weapon folder
    std::string texture; ///< the game's texture name
    u64 hash = 0;        ///< its custom texture packs' new hash
    u32 cards = 0;
    std::string replaced; ///< the texture pack's PNG, as Part::replaced
    bool replaced_flip = true;
};

struct FlashInfo {
    /// Whether the weapon was made with its flashes (a weapon made by a build before them has
    /// none and is made again when asked for).
    bool known = false;
    std::string rapid, charged; ///< the effect models used ("" for none)
    /// The weapon whose effects they are: this one, or another of its class when this one has
    /// none (then the colours are made neutral); -1 for none.
    int source = -1;
    u64 pack_key = 0; ///< PackKey of the flash textures' hashes
    std::vector<FlashPart> parts;
};

// ---- AzaharFrame kiuflash: the projectiles ----
//
// Each weapon's effect file also holds its shots' bullets: mesh models ("Eff<Class><NN>Bullet1",
// the charged shot's "...Bullet3"), most of them added light with combiner-coloured textures.
// Each is made into files that join the weapon's own (w<id>-b<n>.model, one per bullet material:
// its meshes in the bind pose, centred, scaled so the largest side is 1, the head to +z (its
// trail reaches farther along z from its mass); components "rapid" and "charged" for the kinds it is), the material's look fitted
// to its texture (BulletValue: a colour mode and a colour, so a texture pack's PNG can stand in).
// A kind with no mesh bullet (or one over kBulletTrianglesMax) uses its bullet sprites instead
// ("Ptc<Class><NN>Bullet1"/"Bullet3": face cards as the flashes'). Bullet animations are not
// read: the projectile turns about the way it flies (frame_xr/vis/projectile.h).

constexpr u32 kBulletTrianglesMax = 600;

/// The bullet models (mesh) a weapon's shots are, by name only: rapid fire's "Bullet1",
/// "Bullet01" or "BulletA01", the charged shot's "Bullet3" or "Bullet03" (the other kind's when
/// missing; "" for none).
FlashModels PickBulletModels(int id, const std::vector<std::string>& names);
/// The bullet sprites (particle models) the same way: "Ptc<Class><NN>Bullet1"/"Bullet01",
/// "Bullet3"/"Bullet03".
FlashModels PickBulletSprites(int id, const std::vector<std::string>& names);

/// A bullet part's values in its components' motion floats (a mesh bullet's; a sprite one's are
/// FlashValue, its file flagged a flash).
enum BulletValue : int {
    kBulletMode,  ///< FlashMode
    kBulletRed,   ///< the colour fitted to the material's look, 0 to 1 (as the game's: not linear)
    kBulletGreen,
    kBulletBlue,
    kBulletAdded, ///< 1: added light (the glow pipeline); 0: drawn lit and solid
    /// 1: its look is times the vertex colour (the combiner's first stage multiplies by it, as
    /// most bullets'); 0: the vertex colour is in the look already (its mean, run through the
    /// combiner as the game does)
    kBulletVertexColour,
    kBulletValues,
};

struct BulletInfo {
    bool known = false; ///< made by a build with the projectiles (else made again when asked for)
    std::string rapid, charged; ///< the models used ("" for none)
    bool rapid_cards = false, charged_cards = false; ///< sprites, not a mesh
    u64 pack_key = 0;
    std::vector<FlashPart> parts; ///< file, texture, hash, components, pack PNG (as the flashes')
};

/// AzaharFrame kiuweap2: how Pit holds the weapon, from the game's own animations of his class
/// (player/g<class>.zrc, character/mdl.arc!mdl/00.bcres): in a pose, the primary anchor's space
/// in the holding hand's (`to_hand`, row major 3x4) and the fist's centre in that hand's space
/// (the grip bone's origin for a weapon with one, else the fist marker of Pit's hand; game
/// units).
struct HoldPose {
    FrameCgfx::Mat34 to_hand{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    std::array<float, 3> fist{};
};
struct HoldInfo {
    bool known = false; ///< made by a build that makes it (else the weapon is made again once)
    bool valid = false; ///< read from the game's files (else the class's own fit is used)
    /// The holding hand: RHandN; a bow's LHandN (Pit holds it in his left hand); a cannon's
    /// RArmN (it has no right hand: the forearm is the cannon).
    std::string hand;
    HoldPose idle; ///< his idle (ChrPit00Wait<Class>), its first frame
    HoldPose shot; ///< his aiming pose (ChrPit00ShotWait<Class>; clubs have none: the idle)
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
    FlashInfo flash; ///< AzaharFrame kiuflash
    BulletInfo bullets; ///< AzaharFrame kiuflash
    HoldInfo hold;      ///< AzaharFrame kiuweap2
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

/// AzaharFrame kiuweap2: Pit's idle and aiming poses of a class (skeletal animations of
/// player/g<class>.zrc), read once per class.
struct ClassPoses {
    FrameCgfx::SkeletalPose idle, shot;
};
/// The file: "g<class>.zrc".
std::string PoseZrcName(int cls);
/// The animations' names: "ChrPit00Wait<Name>", "ChrPit00ShotWait<Name>" (Name: the game's
/// class name, Rifle, Snipe, Nail, Bow, Palm, Pillar, Launch, Bit, Arm).
std::string IdleAnimation(int cls);
std::string ShotAnimation(int cls);
/// Reads both (an aiming pose that is missing is the idle). False with the reason.
bool ReadClassPoses(std::span<const u8> pose_zrc, int cls, ClassPoses& out, std::string& error);
/// A weapon's hold (HoldInfo) from its skeleton (`m`: the weapon's fmdl model, Pit and the
/// weapon; `anchor`: its primary anchor bone) posed by the class's poses. Not valid when the
/// skeleton lacks the bones.
HoldInfo MakeHold(const FrameCgfx::Model& m, int cls, s32 anchor, const ClassPoses& poses);

/// One weapon's .zrc made into its files (in memory). Pure but for the pack's files: tested.
struct Converted {
    Weapon info;
    std::vector<std::pair<std::string, std::vector<u8>>> files;
};
/// `class_anims` (AzaharFrame kiumodels): the material animations of the class's Pit file
/// (ReadClassUvAnims), for the parts' UvMotion; none: glow cards flow.
/// `poses` (AzaharFrame kiuweap2): the class's Pit poses (ReadClassPoses), for the hold; none:
/// the hold is not valid (the class's own fit).
bool ConvertWeapon(int id, std::span<const u8> zrc, Converted& out, std::string& error,
                   const TexturePack* pack = nullptr,
                   const std::vector<FrameCgfx::UvAnim>* class_anims = nullptr,
                   const ClassPoses* poses = nullptr);

/// AzaharFrame kiumodels: the class's Pit file (player/a<class>.zrc: LZ11, darc,
/// character/mdl.arc!mdl/00.bcres) read for its texture coordinator animations (the weapons'
/// "Wpn...UV": claws, palms, clubs, cannons, orbitars have one).
bool ReadClassUvAnims(std::span<const u8> class_zrc, std::vector<FrameCgfx::UvAnim>& out,
                      std::string& error);
/// The class's Pit file name: "a<class>.zrc".
std::string ClassZrcName(int cls);

/// A part's UvMotion from the animations (the coordinator `coord` of `material`; a scale on it
/// turns the slide: the game samples at scale * (uv - translate)). None when nothing targets it.
UvMotion GameUvMotion(const std::vector<FrameCgfx::UvAnim>& anims, const std::string& material,
                      s32 coord, float scale_u, float scale_v);

/// AzaharFrame kiuflash: weapon `id`'s flashes from the effects of `source_zrc` (its own, or
/// another weapon's of its class, weapon `source`, with `neutral` colours) into `out`
/// (out.info.flash and its files). ConvertWeapon calls it with the weapon's own file. False
/// when that file has no flash (out unchanged but for flash.known).
bool ConvertFlash(int id, std::span<const u8> source_zrc, int source, bool neutral,
                  Converted& out, const TexturePack* pack = nullptr);

/// AzaharFrame kiuflash: weapon `id`'s bullets from its effect file into `out` (out.info.bullets
/// and its files). ConvertWeapon calls it. False when the file has none.
bool ConvertBullets(int id, std::span<const u8> zrc, Converted& out,
                    const TexturePack* pack = nullptr);

/// A texture pack's PNG (Part::replaced) for a model's colour image: RGBA8, rows in the game's
/// memory order, alpha 255 (AzaharFrame kiuflash: or the PNG's own, `keep_alpha`, for a flash
/// card). False when it cannot be read (the model's own texture stays).
/// AzaharFrame fixc: or a KTX or DDS file in ASTC (4x4, 6x6, 8x6) or RGBA8, as a pack converted
/// for the Frame holds ("Original-astc4x4"): its largest mip level of kPackDecodeMaxSide or less
/// a side, decoded (astcenc), rows as for a PNG. `why`: the reason when it is false.
inline constexpr unsigned kPackDecodeMaxSide = 1024;
bool LoadPackTexture(const std::string& path, bool flip_png, std::vector<u8>& rgba, u32& width,
                     u32& height, bool keep_alpha = false, std::string* why = nullptr);
/// AzaharFrame fixc: a pack file's format as the hand reads it ("PNG", "KTX ASTC 4x4", "DDS
/// RGBA8"), from its header; empty when it is not read (BC formats, another file), with why.
std::string PackFileFormat(const std::string& path, std::string* why = nullptr);
/// AzaharFrame fixc: whether a pack's file can replace a model's texture (PackFileFormat); a
/// file that cannot is logged with its format and why (`what`: the models it is for, "weapon
/// models"), and the game's texture is used. Then LoadPackTexture reads it, whatever its format.
bool PackReplacementUsable(const std::string& path, std::string_view what);

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
