// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#pragma once

#include <atomic> // AzaharFrame savemem
#include <chrono> // AzaharFrame savemem
#include <list>
#include <mutex>    // AzaharFrame memguard
#include <optional> // AzaharFrame memguard
#include <span>
#include <unordered_map>
#include <unordered_set>
#include "common/thread_worker.h"
#include "video_core/custom_textures/material.h"
#include "video_core/rasterizer_interface.h"

namespace Core {
class System;
}

namespace FileUtil {
struct FSTEntry;
}

namespace VideoCore {

class SurfaceParams;

/// AzaharFrame memguard: how a preload ended (EmuThread tells the player when it did not happen)
enum class PreloadResult {
    Done,      ///< every texture is in memory
    Declined,  ///< the pack would not fit in the memory there is: loaded as needed instead
    Stopped,   ///< memory ran low part way: what was read is dropped, loaded as needed instead
    Cancelled, ///< the game was stopped meanwhile
};

struct AsyncUpload {
    Material* material;
    std::function<bool()> func;
};

class CustomTexManager {
public:
    explicit CustomTexManager(Core::System& system);
    ~CustomTexManager();

    /// AzaharFrame kiuguns: the hash a pack with use_new_hash names a texture level by (the raw
    /// tiled bytes the game uploads); RasterizerCache::ComputeHash uses it.
    static u64 NewHash(std::span<const u8> upload_data);

    /// AzaharFrame kiuguns: a game's active pack as FindCustomTextures sees it (the packs and
    /// parts turned on, pack.json's file map, the first file of a hash winning), its colour
    /// textures only: hash to file. Reads the folders only (no texture is decoded); any thread.
    struct PackIndex {
        bool config = false; ///< a pack.json with options
        bool use_new_hash = false;
        bool flip_png_files = true;
        std::unordered_map<u64, std::string> colour;
    };
    static PackIndex IndexPack(Core::System& system, u64 title_id);

    /// Processes queued texture uploads
    void TickFrame();

    /// Searches the load directory assigned to program_id for any custom textures and loads them
    void FindCustomTextures();

    /// Reads the pack configuration file
    bool ReadConfig(u64 title_id, bool options_only = false);

    /// Saves the pack configuration file template to the dump directory if it doesn't exist.
    void PrepareDumping(u64 title_id);

    /// Preloads all registered custom textures. AzaharFrame memguard: only when they fit in the
    /// memory there is (video_core/custom_textures/preload_budget.h)
    PreloadResult PreloadTextures(const std::atomic_bool& stop_run,
                                  const VideoCore::DiskResourceLoadCallback& callback);

    /// AzaharFrame memguard: the memory a preload of the loaded pack would hold, in bytes (0 when
    /// no pack is loaded). Worked out once; any thread once the game has booted
    u64 PreloadEstimate();

    /// AzaharFrame memguard: the custom texture pixel data held in memory now (decoded PNGs,
    /// read DDS/KTX files), for the log when a game stops
    /// AzaharFrame savemem: `skip_busy` leaves out a texture whose decode holds its lock now
    /// (never waits; for HeldBytesHint).
    u64 HeldBytes(bool skip_busy = false) const;
    /// AzaharFrame savemem: HeldBytes as of the last few seconds (worked out by TickFrame at most
    /// every 2 s, and by Unload), for the memory dialog's advice. Any thread.
    u64 HeldBytesHint() const {
        return held_hint.load(std::memory_order_relaxed);
    }

    /// AzaharFrame memwarn: custom textures were turned off while the game runs (the memory
    /// dialog): every texture's pixel data and the queued uploads go, and the heap is given back.
    /// The materials stay (surfaces point at them); turned on again, they load as needed. Returns
    /// the pixel data freed, in bytes. Emulation thread (RasterizerCache::TickFrame).
    u64 Unload();

    /// Saves the provided pixel data described by params to disk as png
    void DumpTexture(const SurfaceParams& params, u32 level, std::span<u8> data, u64 data_hash);

    /// Returns the material assigned to the provided data hash
    Material* GetMaterial(u64 data_hash);

    /// Decodes the textures in material to a consumable format and uploads it.
    bool Decode(Material* material, std::function<bool()>&& upload);

    /// True when mipmap uploads should be skipped (legacy packs only)
    bool SkipMipmaps() const noexcept {
        return skip_mipmap;
    }

    /// Returns true if the pack uses the new hashing method.
    bool UseNewHash() const noexcept {
        return use_new_hash;
    }

private:
    /// AzaharFrame kiuguns: for IndexPack (no image interface needed: nothing is decoded)
    CustomTexManager(Core::System& system, Frontend::ImageInterface& images);

    /// Parses the custom texture filename (hash, material type, etc).
    bool ParseFilename(const FileUtil::FSTEntry& file, CustomTexture* texture);

    /// Returns a vector of all custom texture files.
    std::vector<FileUtil::FSTEntry> GetTextures(u64 title_id);

    /// AzaharFrame texlayers: a lower pack's pack.json "textures" (file to hash) map
    void ReadTextureMap(const std::string& config_path);

    /// Creates the thread workers.
    void CreateWorkers();

    /// AzaharFrame: reads the material on a worker thread
    void QueueDecode(Material* material);

    /// AzaharFrame: drops the CPU copy of an uploaded DDS/KTX material no queued upload needs
    void ReleaseUploaded(Material* material);

    /// AzaharFrame memguard: the memory a preload of every material would hold, in bytes
    u64 EstimatePreload() const;

    /// AzaharFrame memguard: every material back to unloaded (a preload that stopped)
    void DropPreloaded();

private:
    Core::System& system;
    Frontend::ImageInterface& image_interface;
    std::unordered_set<u64> dumped_textures;
    // AzaharFrame stall: surfaces the pack has no replacement for, logged as a count at most every
    // 10 s (a line per surface was 147k lines in a 20-minute session, most of them each frame).
    u64 missing_count = 0;
    s64 missing_logged_at = 0; ///< steady clock, ms
    std::unordered_map<u64, std::unique_ptr<Material>> material_map;
    std::unordered_map<std::string, std::vector<u64>> path_to_hash_map;
    std::vector<std::unique_ptr<CustomTexture>> custom_textures;
    std::list<AsyncUpload> async_uploads;
    std::unique_ptr<Common::ThreadWorker> workers;
    // AzaharFrame memguard: atomic, PreloadEstimate reads it on the GUI thread
    std::atomic_bool textures_loaded{false};
    bool async_custom_loading{true};
    /// AzaharFrame memguard: a preload finished; uploads drop the DDS/KTX copies
    bool preloaded{false};
    std::mutex estimate_mutex;   ///< AzaharFrame memguard
    std::optional<u64> estimate; ///< AzaharFrame memguard: PreloadEstimate's
    bool skip_mipmap{false};
    bool flip_png_files{true};
    bool use_new_hash{true};
    std::atomic<u64> held_hint{0}; ///< AzaharFrame savemem: HeldBytesHint
    std::chrono::steady_clock::time_point held_hint_at{}; ///< emulation thread
};

} // namespace VideoCore
