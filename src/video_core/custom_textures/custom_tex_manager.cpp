// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <array>   // AzaharFrame memguard
#include <charconv>   // AzaharFrame texlayers
#include <chrono>  // AzaharFrame
#include <filesystem> // AzaharFrame texlayers
#include <cstdlib> // AzaharFrame memguard
#include <fstream> // AzaharFrame memwarn
#include <thread>  // AzaharFrame memguard
#ifdef __GLIBC__
#include <malloc.h> // AzaharFrame memwarn: malloc_trim
#endif
#include <json.hpp>
#include "common/test_hooks.h"
#include "common/file_util.h"
#include "common/literals.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"      // AzaharFrame
#include "common/stall_telemetry.h" // AzaharFrame
#include "common/settings.h"
#include "common/string_util.h"
#include "common/texture.h"
#include "core/core.h"
#include "core/custom_tex/layers.h" // AzaharFrame texlayers
#include "core/custom_tex/parts.h"  // AzaharFrame texparts
#include "core/frontend/image_interface.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "video_core/custom_textures/custom_tex_manager.h"
#include "video_core/custom_textures/pack_config.h" // AzaharFrame jsoncrash
#include "video_core/custom_textures/preload_budget.h" // AzaharFrame memguard
#include "video_core/rasterizer_cache/surface_params.h"
#include "video_core/rasterizer_cache/utils.h"

namespace VideoCore {

namespace {

MICROPROFILE_DEFINE(CustomTexManager_TickFrame, "CustomTexManager", "TickFrame",
                    MP_RGB(54, 16, 32));

constexpr std::size_t MAX_UPLOADS_PER_TICK = 8;

using namespace Common::Literals;

// AzaharFrame: a tick stops starting uploads past this many bytes (a 4096x4096 PNG is 64 MiB),
// so a burst of large textures is spread over frames instead of stalling one
constexpr u64 MAX_UPLOAD_BYTES_PER_TICK = 96_MiB;

bool IsPow2(u32 value) {
    return value != 0 && (value & (value - 1)) == 0;
}

CustomFileFormat MakeFileFormat(std::string_view ext) {
    if (ext == "png") {
        return CustomFileFormat::PNG;
    } else if (ext == "dds") {
        return CustomFileFormat::DDS;
    } else if (ext == "ktx") {
        return CustomFileFormat::KTX;
    }
    return CustomFileFormat::None;
}

MapType MakeMapType(std::string_view ext) {
    if (ext == "norm") {
        return MapType::Normal;
    }
    LOG_ERROR(Render, "Unknown material extension {}", ext);
    return MapType::Color;
}

} // Anonymous namespace

CustomTexManager::CustomTexManager(Core::System& system_)
    : system{system_}, image_interface{*system.GetImageInterface()},
      async_custom_loading{Settings::values.async_custom_loading.GetValue()} {}

CustomTexManager::~CustomTexManager() = default;

void CustomTexManager::TickFrame() {
    MICROPROFILE_SCOPE(CustomTexManager_TickFrame);
    if (!textures_loaded) {
        return;
    }
    // AzaharFrame savemem: what the memory dialog reads (HeldBytesHint), every 2 s at most
    if (const auto now = std::chrono::steady_clock::now();
        now - held_hint_at >= std::chrono::seconds(2)) {
        held_hint_at = now;
        held_hint.store(HeldBytes(), std::memory_order_relaxed);
    }
    std::size_t num_uploads = 0;
    u64 upload_bytes = 0;
    // AzaharFrame: how long the uploads of this tick took, for the stall line (perf_stats.cpp)
    // and a line of its own when a tick is long
    auto& telemetry = Common::StallTelemetry::Get();
    const auto tick_start = std::chrono::steady_clock::now();
    const u64 image_us_before = telemetry.custom_image_us.load(std::memory_order_relaxed);
    const u64 staging_us_before = telemetry.staging_wait_us.load(std::memory_order_relaxed);
    SCOPE_EXIT({
        if (num_uploads == 0) {
            return;
        }
        const u64 us = static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(
                                            std::chrono::steady_clock::now() - tick_start)
                                            .count());
        telemetry.custom_upload_us.fetch_add(us, std::memory_order_relaxed);
        telemetry.custom_upload_count.fetch_add(num_uploads, std::memory_order_relaxed);
        telemetry.custom_upload_bytes.fetch_add(upload_bytes, std::memory_order_relaxed);
        if (us >= 20'000) {
            LOG_WARNING(Render,
                        "custom textures: {} uploads, {:.1f} MiB in {:.1f} ms (images {:.1f} ms, "
                        "stream buffer waits {:.1f} ms), {} queued",
                        num_uploads, upload_bytes / 1048576.0, us / 1000.0,
                        (telemetry.custom_image_us.load(std::memory_order_relaxed) -
                         image_us_before) /
                            1000.0,
                        (telemetry.staging_wait_us.load(std::memory_order_relaxed) -
                         staging_us_before) /
                            1000.0,
                        async_uploads.size());
        }
    });
    for (auto it = async_uploads.begin(); it != async_uploads.end();) {
        if (num_uploads >= MAX_UPLOADS_PER_TICK || upload_bytes >= MAX_UPLOAD_BYTES_PER_TICK) {
            return;
        }
        Material* const material = it->material;
        switch (material->state) {
        case DecodeState::Decoded:
            if (!material->IsResident()) {
                QueueDecode(material);
                it++;
                continue;
            }
            it->func();
            num_uploads++;
            upload_bytes += material->size;
            it = async_uploads.erase(it);
            ReleaseUploaded(material);
            continue;
        case DecodeState::Failed:
            it = async_uploads.erase(it);
            continue;
        case DecodeState::None:
            // AzaharFrame: released after an earlier upload; read it again
            QueueDecode(material);
            it++;
            continue;
        default:
            it++;
            break;
        }
    }
}

void CustomTexManager::QueueDecode(Material* material) {
    material->state = DecodeState::Pending;
    workers->QueueWork([material, this] { material->LoadFromDisk(flip_png_files); });
}

void CustomTexManager::ReleaseUploaded(Material* material) {
    // Keeping every uploaded texture in memory grows up to the whole pack over a long session.
    // AzaharFrame memguard: PNGs too (decoded again on a worker if their surface is made again)
    if (!async_custom_loading || !material->IsDecoded()) {
        return;
    }
    const bool still_queued =
        std::any_of(async_uploads.begin(), async_uploads.end(),
                    [material](const AsyncUpload& upload) { return upload.material == material; });
    if (!still_queued) {
        material->Release();
    }
}

void CustomTexManager::FindCustomTextures() {
    if (textures_loaded) {
        return;
    }
    if (!workers) {
        CreateWorkers();
    }

    const u64 title_id = system.Kernel().GetCurrentProcess()->codeset->program_id;
    const auto textures = GetTextures(title_id);
    if (!ReadConfig(title_id)) {
        use_new_hash = false;
        skip_mipmap = true;
    }

    custom_textures.reserve(textures.size());
    std::size_t replaced = 0; // AzaharFrame texlayers
    for (const FileUtil::FSTEntry& file : textures) {
        if (file.isDirectory) {
            continue;
        }
        custom_textures.push_back(std::make_unique<CustomTexture>(image_interface));
        CustomTexture* const texture{custom_textures.back().get()};
        if (!ParseFilename(file, texture)) {
            continue;
        }
        for (const u64 hash : texture->hashes) {
            auto& material = material_map[hash];
            if (!material) {
                material = std::make_unique<Material>();
            }
            // AzaharFrame texlayers: a hash a pack higher in the list already has (the first file
            // read wins); counted, not an error line each
            if (material->textures[static_cast<std::size_t>(texture->type)]) {
                replaced++;
                continue;
            }
            material->hash = hash;
            material->per_level_files = !skip_mipmap;
            material->AddMapTexture(texture);
        }
    }
    if (replaced > 0) {
        LOG_INFO(Render, "custom textures: {} textures replaced by a part or pack higher in the list",
                 replaced);
    }
    textures_loaded = true;
}

bool CustomTexManager::ParseFilename(const FileUtil::FSTEntry& file, CustomTexture* texture) {
    auto parts = Common::SplitString(file.virtualName, '.');
    if (parts.size() > 3) {
        LOG_ERROR(Render, "Invalid filename {}, ignoring", file.virtualName);
        return false;
    }
    // The last string should always be the file extension.
    const CustomFileFormat file_format = MakeFileFormat(parts.back());
    if (file_format == CustomFileFormat::None) {
        return false;
    }
    // AzaharFrame: DDS/KTX files bring their own mip chain (or clamp the image to the levels
    // they have), so they work in packs that skip mipmaps too
    texture->file_format = file_format;
    parts.pop_back();

    // This means the texture is a material type other than color.
    texture->type = MapType::Color;
    if (parts.size() > 1) {
        texture->type = MakeMapType(parts.back());
        parts.pop_back();
    }

    // First look if this file is mapped to any number of hashes.
    std::vector<u64>& hashes = texture->hashes;
    const auto it = path_to_hash_map.find(file.virtualName);
    if (it != path_to_hash_map.end()) {
        hashes = it->second;
    }

    // It's also possible for pack creators to retain the default texture name
    // still map the texture to another hash. Support that as well.
    u32 width;
    u32 height;
    u32 format;
    unsigned long long hash{};
    const bool is_parsed = std::sscanf(parts.back().c_str(), "tex1_%ux%u_%llX_%u", &width, &height,
                                       &hash, &format) == 4;
    const bool is_mapped =
        !hashes.empty() && std::find(hashes.begin(), hashes.end(), hash) != hashes.end();
    if (is_parsed && !is_mapped) {
        hashes.push_back(hash);
    }

    texture->path = file.physicalName;
    texture->file_size = file.size; // AzaharFrame memguard
    return true;
}

void CustomTexManager::PrepareDumping(u64 title_id) {
    // If a pack exists in the load folder that uses the old hash, dump textures using the old hash.
    // This occurs either if a configuration file doesn't exist or that file sets the old hash.
    const std::string load_path =
        fmt::format("{}textures/{:016X}/", GetUserPath(FileUtil::UserPath::LoadDir), title_id);
    if (FileUtil::Exists(load_path) && !ReadConfig(title_id, true)) {
        use_new_hash = false;
    }

    // Write template config file
    const std::string dump_path =
        fmt::format("{}textures/{:016X}/", GetUserPath(FileUtil::UserPath::DumpDir), title_id);
    const std::string pack_config = dump_path + "pack.json";
    if (FileUtil::Exists(pack_config)) {
        return;
    }

    nlohmann::ordered_json json;
    json["author"] = "citra";
    json["version"] = "1.0.0";
    json["description"] = "A graphics pack";

    auto& options = json["options"];
    options["skip_mipmap"] = false;
    options["flip_png_files"] = true;
    options["use_new_hash"] = true;

    FileUtil::IOFile file{pack_config, "w"};
    const std::string output = json.dump(4);
    file.WriteString(output);
}

// AzaharFrame memguard: every texture the materials use, counted once (a file mapped to more than
// one hash belongs to more than one material). DDS/KTX: the file as it is. PNG: decoded to RGBA8,
// from the size in its header.
u64 CustomTexManager::EstimatePreload() const {
    std::unordered_set<const CustomTexture*> seen;
    u64 bytes = 0;
    for (const auto& [hash, material] : material_map) {
        for (const CustomTexture* texture : material->textures) {
            if (!texture || !seen.insert(texture).second) {
                continue;
            }
            const bool png = texture->file_format == CustomFileFormat::PNG;
            u32 width = 0;
            u32 height = 0;
            if (png) {
                std::array<u8, 24> head{};
                FileUtil::IOFile file{texture->path, "rb"};
                if (file.IsOpen() && file.ReadBytes(head.data(), head.size()) == head.size()) {
                    PreloadBudget::ParsePngSize(head, width, height);
                }
            }
            bytes += PreloadBudget::FileCost(png, texture->file_size, width, height);
        }
    }
    return bytes;
}

u64 CustomTexManager::PreloadEstimate() {
    if (!textures_loaded || material_map.empty()) {
        return 0;
    }
    std::scoped_lock lock{estimate_mutex};
    if (!estimate) {
        estimate = EstimatePreload();
    }
    return *estimate;
}

u64 CustomTexManager::HeldBytes() const {
    u64 bytes = 0;
    for (const auto& texture : custom_textures) {
        std::scoped_lock lock{texture->decode_mutex};
        bytes += texture->data.capacity(); // AzaharFrame memwarn: what is held, not used
    }
    return bytes;
}

namespace {
// AzaharFrame memwarn: RssAnon of /proc/self/status in MiB, for Unload's log line
u64 RssAnonMiB() {
    std::ifstream status{"/proc/self/status"};
    std::string line;
    while (std::getline(status, line)) {
        if (line.starts_with("RssAnon:")) {
            return std::strtoull(line.c_str() + 8, nullptr, 10) / 1024;
        }
    }
    return 0;
}
} // namespace

u64 CustomTexManager::Unload() { // AzaharFrame memwarn
    if (!textures_loaded) {
        return 0;
    }
    const u64 anon_before = RssAnonMiB();
    if (workers) {
        workers->WaitForRequests(); // no worker is reading into a texture now
    }
    const u64 held = HeldBytes();
    async_uploads.clear();
    DropPreloaded();
    preloaded = false; // turned on again: uploads drop their copies, as without a preload
#ifdef __GLIBC__
    malloc_trim(0); // large frees stay in the heap otherwise (the mmap threshold grows)
#endif
    LOG_INFO(Render,
             "custom textures: turned off, {} MiB of pixel data freed ({} left); anonymous memory "
             "{} -> {} MiB",
             held >> 20, HeldBytes() >> 20, anon_before, RssAnonMiB());
    held_hint.store(HeldBytes(), std::memory_order_relaxed); // AzaharFrame savemem
    return held;
}

void CustomTexManager::DropPreloaded() {
    for (const auto& texture : custom_textures) {
        texture->Drop();
    }
    for (auto& [hash, material] : material_map) {
        if (material->state != DecodeState::Failed) {
            material->size = 0;
            material->state = DecodeState::None;
        }
    }
}

PreloadResult CustomTexManager::PreloadTextures(
    const std::atomic_bool& stop_run, const VideoCore::DiskResourceLoadCallback& callback) {
    // AzaharFrame memguard: the pack is preloaded only when it fits in the memory the system and
    // this process's cgroup can still give (preload_budget.h); else, or when memory runs low part
    // way, it loads as needed (async custom loading) and the player is told.
    // AZAHARFRAME_PRELOAD_RESERVE_MB: the system reserve (default the larger of 4 GiB and a
    // quarter of RAM). AZAHARFRAME_PRELOAD_FORCE=1 (tests): skip the check before, keep the
    // check while it runs.
    using namespace PreloadBudget;
    const u64 reserve = ReserveFromEnv();
    const char* force_env = std::getenv("AZAHARFRAME_PRELOAD_FORCE");
    const bool force = force_env && std::string_view{force_env} == "1";
    // Tests: AZAHARFRAME_TEST_PRELOAD_DELAY_MS waits that long after each material, a slow disk
    // or a busy headset, so the XR loop and the stop hold can be watched during a long preload
    const char* delay_env = Common::TestHookEnv("AZAHARFRAME_TEST_PRELOAD_DELAY_MS");
    const int delay_ms = delay_env ? std::atoi(delay_env) : 0;

    const auto mib = [](s64 bytes) { return bytes < 0 ? -1.0 : bytes / 1048576.0; };
    const auto describe = [&](const Room& room) {
        const u64 sys_reserve =
            reserve ? reserve : SystemReserve(static_cast<u64>(std::max<s64>(room.total, 0)));
        std::string text = fmt::format("MemAvailable {:.0f} MiB of {:.0f} (reserve {:.0f})",
                                       mib(room.available), mib(room.total), mib(sys_reserve));
        if (room.cgroup_limited) {
            text += fmt::format(", cgroup {} {:.0f} of {:.0f} MiB used (reserve {:.0f})",
                                room.cgroup, mib(room.cgroup_used), mib(room.cgroup_limit),
                                mib(CgroupReserve(room.cgroup_limit)));
        }
        return text;
    };

    const u64 need = PreloadEstimate();
    const Room before = ReadRoom();
    const s64 budget = Budget(before, reserve);
    const bool fits = ShouldPreload(need, before, reserve);
    LOG_INFO(Render,
             "custom textures: preload of {} materials needs about {:.0f} MiB; budget {:.0f} MiB "
             "({}): {}",
             material_map.size(), mib(static_cast<s64>(need)), mib(budget), describe(before),
             fits ? "preloading"
                  : (force ? "too large, preloading anyway (AZAHARFRAME_PRELOAD_FORCE)"
                           : "too large, loading as needed"));
    if (!fits && !force) {
        return PreloadResult::Declined;
    }

    const std::size_t total = material_map.size();
    if (callback) {
        callback(VideoCore::LoadCallbackStage::Preload, 0, total, "");
    }
    u64 size_sum = 0;
    std::size_t preloaded_count = 0;
    bool low_memory = false;
    Room at_stop;
    const auto started = std::chrono::steady_clock::now();
    workers->QueueWork([&]() {
        std::size_t since_check = 0;
        u64 bytes_since_check = 0;
        for (auto& [hash, material] : material_map) {
            if (stop_run) {
                return;
            }
            if (since_check >= kRecheckEveryMaterials || bytes_since_check >= kRecheckEveryBytes) {
                since_check = 0;
                bytes_since_check = 0;
                const Room now = ReadRoom();
                if (ShouldStop(now, reserve)) {
                    low_memory = true;
                    at_stop = now;
                    return;
                }
            }
            material->LoadFromDisk(flip_png_files, true);
            size_sum += material->size;
            bytes_since_check += material->size;
            since_check++;
            preloaded_count++;
            if (delay_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            }
            if (callback) {
                callback(VideoCore::LoadCallbackStage::Preload, preloaded_count, total, "");
            }
        }
    });
    workers->WaitForRequests();
    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    if (low_memory) {
        LOG_WARNING(Render,
                    "custom textures: preload stopped at {} of {} materials ({:.0f} MiB in {:.1f} "
                    "s): memory low ({}); dropped, loading as needed",
                    preloaded_count, total, mib(static_cast<s64>(size_sum)), secs,
                    describe(at_stop));
        DropPreloaded();
        return PreloadResult::Stopped;
    }
    if (stop_run) {
        LOG_INFO(Render, "custom textures: preload cancelled at {} of {} materials",
                 preloaded_count, total);
        DropPreloaded();
        return PreloadResult::Cancelled;
    }
    LOG_INFO(Render, "custom textures: preloaded {} materials, {:.0f} MiB in {:.1f} s ({})",
             preloaded_count, mib(static_cast<s64>(size_sum)), secs, describe(ReadRoom()));
    async_custom_loading = false;
    preloaded = true;
    return PreloadResult::Done;
}

void CustomTexManager::DumpTexture(const SurfaceParams& params, u32 level, std::span<u8> data,
                                   u64 data_hash) {
    const u64 program_id = system.Kernel().GetCurrentProcess()->codeset->program_id;
    const u32 data_size = static_cast<u32>(data.size());
    const u32 width = params.width;
    const u32 height = params.height;
    const PixelFormat format = params.pixel_format;

    std::string dump_path = fmt::format(
        "{}textures/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::DumpDir), program_id);
    if (!FileUtil::CreateFullPath(dump_path)) {
        LOG_ERROR(Render, "Unable to create {}", dump_path);
        return;
    }

    dump_path +=
        fmt::format("tex1_{}x{}_{:016X}_{}_mip{}.png", width, height, data_hash, format, level);
    if (dumped_textures.contains(data_hash) || FileUtil::Exists(dump_path)) {
        return;
    }

    // Make sure the texture size is a power of 2.
    // If not, the surface is probably a framebuffer
    if (!IsPow2(width) || !IsPow2(height)) {
        LOG_WARNING(Render, "Not dumping {:016X} because size isn't a power of 2 ({}x{})",
                    data_hash, width, height);
        return;
    }

    const u32 decoded_size = width * height * 4;
    std::vector<u8> pixels(data_size + decoded_size);
    std::memcpy(pixels.data(), data.data(), data_size);

    auto dump = [this, width, height, params, data_size, decoded_size, pixels = std::move(pixels),
                 dump_path = std::move(dump_path)]() mutable {
        const std::span encoded = std::span{pixels}.first(data_size);
        const std::span decoded = std::span{pixels}.last(decoded_size);
        DecodeTexture(params, params.addr, params.end, encoded, decoded,
                      params.type == SurfaceType::Color);
        Common::FlipRGBA8Texture(decoded, width, height);
        image_interface.EncodePNG(dump_path, width, height, decoded);
    };
    if (!workers) {
        CreateWorkers();
    }
    workers->QueueWork(std::move(dump));
    dumped_textures.insert(data_hash);
}

Material* CustomTexManager::GetMaterial(u64 data_hash) {
    const auto it = material_map.find(data_hash);
    if (it == material_map.end()) {
        // AzaharFrame stall: a count every 10 s at most, not a line per surface.
        missing_count++;
        const s64 now = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
        if (missing_logged_at == 0 || now - missing_logged_at >= 10'000) {
            LOG_INFO(Render, "No replacement for {} surface(s) so far (latest hash {:016X})",
                     missing_count, data_hash);
            missing_logged_at = now;
        }
        return nullptr;
    }
    return it->second.get();
}

bool CustomTexManager::Decode(Material* material, std::function<bool()>&& upload) {
    // AzaharFrame: a material whose files were released after an earlier upload reads them again
    const bool released = material->IsDecoded() && !material->IsResident();
    if (!async_custom_loading) {
        if (released) {
            material->state = DecodeState::None;
        }
        material->LoadFromDisk(flip_png_files);
        const bool uploaded = upload();
        // AzaharFrame memguard: the GPU holds the uploaded copy, and on the Frame GPU memory is
        // the same RAM, so the CPU copy goes. After a preload a PNG keeps its decoded copy
        // (not decoding while playing is what the preload is for, and its budget counted it)
        const bool png =
            material->textures[0] && material->textures[0]->file_format == CustomFileFormat::PNG;
        if (!preloaded || !png) {
            material->Release();
        }
        return uploaded;
    }
    if (material->IsUnloaded() || released) {
        QueueDecode(material);
    }
    async_uploads.push_back({
        .material = material,
        .func = std::move(upload),
    });
    return false;
}

bool CustomTexManager::ReadConfig(u64 title_id, bool options_only) {
    const std::string load_path =
        fmt::format("{}textures/{:016X}/", GetUserPath(FileUtil::UserPath::LoadDir), title_id);
    if (!FileUtil::Exists(load_path)) {
        FileUtil::CreateFullPath(load_path);
    }

    const std::string config_path = load_path + "pack.json";
    FileUtil::IOFile config_file{config_path, "r"};
    if (!config_file.IsOpen()) {
        LOG_INFO(Render, "Unable to find pack config file, using legacy defaults");
        return false;
    }
    std::string config(config_file.GetSize(), '\0');
    const std::size_t read_size = config_file.ReadBytes(config.data(), config.size());
    if (!read_size) {
        return false;
    }

    // AzaharFrame jsoncrash: read without throwing (video_core/custom_textures/pack_config.h); a
    // pack.json without options is a legacy pack.
    const PackConfig::Config cfg = PackConfig::Parse(config);
    if (!cfg.valid) {
        LOG_INFO(Render, "pack.json of {:016X} has no options, using legacy defaults", title_id);
        return false;
    }
    skip_mipmap = cfg.skip_mipmap;
    flip_png_files = cfg.flip_png_files;
    use_new_hash = cfg.use_new_hash;

    if (options_only) {
        return true;
    }

    for (const auto& [key, files] : cfg.textures) {
        std::size_t idx{};
        u64 hash = 0;
        try {
            hash = std::stoull(key, &idx, 16);
        } catch (const std::exception&) {
            idx = 0;
        }
        if (!idx) {
            LOG_ERROR(Render, "Key {} is invalid, skipping", key);
            continue;
        }
        if (files.empty()) {
            LOG_ERROR(Render, "Material with key {} is invalid", key);
            continue;
        }
        for (const std::string& file : files) {
            const std::string filename{FileUtil::GetFilename(file)};
            auto [it, new_hash] = path_to_hash_map.try_emplace(filename);
            it->second.push_back(hash);
        }
    }
    return true;
}

std::vector<FileUtil::FSTEntry> CustomTexManager::GetTextures(u64 title_id) {
    const std::string load_path =
        fmt::format("{}textures/{:016X}/", GetUserPath(FileUtil::UserPath::LoadDir), title_id);
    if (!FileUtil::Exists(load_path)) {
        FileUtil::CreateFullPath(load_path);
    }

    // AzaharFrame texlayers: two or more packs on (core/custom_tex/layers.h): every pack's files
    // where they are, the top entry's first, so it keeps a hash two packs share
    if (const auto layered = CustomTex::Layers::LoaderFiles(
            GetUserPath(FileUtil::UserPath::LoadDir), title_id)) {
        std::vector<FileUtil::FSTEntry> loaded;
        loaded.reserve(layered->files.size());
        for (const auto& [path, size] : layered->files) {
            FileUtil::FSTEntry e{};
            e.isDirectory = false;
            e.size = size;
            e.physicalName = path;
            e.virtualName = std::string{FileUtil::GetFilename(path)};
            loaded.push_back(std::move(e));
        }
        // ReadConfig reads the top pack's pack.json (load/textures/<TID> points at it); the
        // others' file-to-hash maps count too
        std::error_code ec;
        const std::string linked =
            std::filesystem::weakly_canonical(load_path, ec).generic_string();
        for (const std::string& pack : layered->packs) {
            if (pack != linked && pack + "/" != linked) {
                ReadTextureMap(pack + "/pack.json");
            }
        }
        return loaded;
    }

    FileUtil::FSTEntry texture_dir;
    std::vector<FileUtil::FSTEntry> textures;
    FileUtil::ScanDirectoryTree(load_path, texture_dir, 64);
    FileUtil::GetAllFilesFromNestedEntries(texture_dir, textures);
    // AzaharFrame texparts: the parts of the pack turned off in Customize are left out, and the
    // parts higher in its list are read first, so they keep a hash two parts share
    // (Material::AddMapTexture keeps the first file of a hash). core/custom_tex/parts.h.
    std::vector<CustomTex::Parts::File> files;
    files.reserve(textures.size());
    for (const FileUtil::FSTEntry& e : textures) {
        std::string rel = e.physicalName.size() > load_path.size()
                              ? e.physicalName.substr(load_path.size())
                              : e.physicalName;
        for (std::size_t at = rel.find("//"); at != std::string::npos; at = rel.find("//", at)) {
            rel.erase(at, 1);
        }
        rel.erase(0, std::min(rel.size(), rel.find_first_not_of('/')));
        files.push_back({std::move(rel), e.size});
    }
    const CustomTex::Parts::State state = CustomTex::Parts::ReadState(load_path);
    const CustomTex::Parts::Layout layout = CustomTex::Parts::Build(files, state.hints);
    const std::vector<std::size_t> order = CustomTex::Parts::LoadOrder(layout, state, files);
    std::vector<FileUtil::FSTEntry> loaded;
    loaded.reserve(order.size());
    for (const std::size_t i : order) {
        loaded.push_back(std::move(textures[i]));
    }
    if (loaded.size() != textures.size()) {
        LOG_INFO(Render, "custom textures: {} of {} files are in parts turned off",
                 textures.size() - loaded.size(), textures.size());
    }
    return loaded;
}

void CustomTexManager::ReadTextureMap(const std::string& config_path) { // AzaharFrame texlayers
    FileUtil::IOFile file{config_path, "r"};
    if (!file.IsOpen()) {
        return;
    }
    std::string text(file.GetSize(), '\0');
    if (file.ReadBytes(text.data(), text.size()) != text.size()) {
        return;
    }
    const nlohmann::json json = nlohmann::json::parse(text, nullptr, false, true);
    const auto textures = json.is_object() ? json.find("textures") : json.end();
    if (!json.is_object() || textures == json.end() || !textures->is_object()) {
        return;
    }
    for (const auto& [key, value] : textures->items()) {
        u64 hash = 0;
        const auto [end, err] = std::from_chars(key.data(), key.data() + key.size(), hash, 16);
        if (err != std::errc{} || end == key.data()) {
            continue;
        }
        const auto add = [&](const nlohmann::json& file_name) {
            if (file_name.is_string()) {
                path_to_hash_map[std::string{FileUtil::GetFilename(file_name.get<std::string>())}]
                    .push_back(hash);
            }
        };
        if (value.is_array()) {
            for (const auto& v : value) {
                add(v);
            }
        } else {
            add(value);
        }
    }
}

void CustomTexManager::CreateWorkers() {
    const std::size_t num_workers = std::max(std::thread::hardware_concurrency(), 2U) >> 1;
    workers = std::make_unique<Common::ThreadWorker>(num_workers, "Custom textures");
}

} // namespace VideoCore
