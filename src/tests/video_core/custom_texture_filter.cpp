// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texfilt: with a texture filter on (any but None), every game texture is made at the
// resolution factor so the filter can upscale it (GetTextureSurface). A pack texture that replaced
// one kept that scale, and the backends make a second, scaled image for a scaled surface and
// sample it; UploadCustom writes the base image only. So with a filter on, every replaced texture
// sampled an image nothing wrote: black, or garbage from uninitialised memory (Kid Icarus's
// Burning Palm on the Frame, Bicubic). The cache is driven here with a fake runtime whose
// surfaces keep their images' contents. The fake makes the scaled image whenever a surface has a
// scale, as the backends did before the fix, so these tests hold whatever a backend does.

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <fmt/format.h>
#include "common/settings.h"
#include "common/vector_math.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/image_interface.h"
#include "core/memory.h"
#include "video_core/custom_textures/custom_tex_manager.h"
#include "video_core/custom_textures/material.h"
#include "video_core/pica/regs_internal.h"
#include "video_core/rasterizer_cache/framebuffer_base.h"
#include "video_core/rasterizer_cache/rasterizer_cache.h"
#include "video_core/rasterizer_cache/sampler_params.h"
#include "video_core/rasterizer_cache/surface_base.h"
#include "video_core/rasterizer_cache/utils.h"
#include "video_core/renderer_base.h"
#include "video_core/texture/texture_decode.h"

namespace VideoCore {
/// Gives the manager a material without a pack on disk (a friend of CustomTexManager)
struct CustomTexManagerTestAccess {
    static void Setup(CustomTexManager& manager, bool async, bool skip_mipmap) {
        manager.textures_loaded = true;
        manager.async_custom_loading = async;
        manager.skip_mipmap = skip_mipmap;
        manager.use_new_hash = true;
    }
    static Material& Add(CustomTexManager& manager, u64 hash) {
        auto& material = manager.material_map[hash];
        material = std::make_unique<Material>();
        material->hash = hash;
        material->per_level_files = !manager.skip_mipmap;
        return *material;
    }
};
} // namespace VideoCore

namespace {

using namespace VideoCore;

struct FakeRuntime;

/// One image of a surface: what each mip level holds, empty if nothing ever wrote it.
struct FakeImage {
    u32 width;
    u32 height;
    std::vector<std::vector<u8>> levels;
    std::vector<bool> generated; ///< written by GenerateMipmaps

    FakeImage(u32 width_, u32 height_, u32 num_levels)
        : width{width_}, height{height_}, levels(num_levels), generated(num_levels) {}
};

struct FakeSurface : SurfaceBase {
    enum Index : u32 { Base = 0, Scaled = 1 };
    std::array<std::optional<FakeImage>, 2> images;
    u32 current = Base;
    u32 filter_runs = 0;

    FakeSurface(FakeRuntime&, const SurfaceParams& params, const SurfaceFlagBits& flags = {})
        : SurfaceBase(params, flags) {
        images[Base].emplace(width, height, levels);
        if (res_scale != 1) {
            images[Scaled].emplace(GetScaledWidth(), GetScaledHeight(), levels);
            current = Scaled;
        }
    }
    /// A replacement: an image at the material's size and, for a surface with a scale, the
    /// scaled image that is sampled, as the backends made before the fix
    FakeSurface(FakeRuntime&, const SurfaceBase& surface, const Material* mat)
        : SurfaceBase(surface) {
        const u32 image_levels = mat->ImageLevels(levels);
        images[Base].emplace(mat->width, mat->height, image_levels);
        if (res_scale != 1) {
            images[Scaled].emplace(mat->width, mat->height, image_levels);
            current = Scaled;
        }
        custom_format = mat->format;
        material = mat;
    }

    u32 GetWidth() const noexcept {
        return width;
    }
    u32 GetHeight() const noexcept {
        return height;
    }
    u32 GetResScale() const noexcept {
        return res_scale;
    }
    u32 GetInternalBytesPerPixel() const {
        return 4;
    }

    /// The image a draw samples
    const FakeImage& Sampled() const {
        return *images[current];
    }

    /// Guest data goes to the base image; a scaled surface runs the texture filter into the
    /// scaled one (the backends' Upload)
    void Upload(const BufferTextureCopy& upload, const StagingData& staging) {
        const std::vector<u8> data(staging.mapped.begin(), staging.mapped.end());
        images[Base]->levels.at(upload.texture_level) = data;
        if (res_scale != 1) {
            images[Scaled]->levels.at(upload.texture_level) = data;
            filter_runs++;
        }
    }
    /// The material goes to the base image only (the backends' UploadCustom)
    void UploadCustom(const Material* mat, u32 level) {
        FakeImage& image = *images[Base];
        if (level >= image.levels.size()) {
            return;
        }
        const auto data = mat->textures[0]->LevelData(0);
        image.levels[level].assign(data.begin(), data.end());
        image.generated[level] = false;
    }
    void Download(const BufferTextureCopy&, const StagingData&) {}
    void ScaleUp(u32 new_scale) {
        if (res_scale == new_scale || new_scale == 1) {
            return;
        }
        res_scale = new_scale;
        images[Scaled].emplace(GetScaledWidth(), GetScaledHeight(), levels);
        current = Scaled;
    }
};

struct FakeSampler {
    FakeSampler(FakeRuntime&, const SamplerParams&) {}
};

struct FakeFramebuffer : FramebufferParams {
    FakeFramebuffer(FakeRuntime&, const FramebufferParams& params, FakeSurface*, FakeSurface*)
        : FramebufferParams(params) {}
    u32 Scale() const {
        return 1;
    }
};

struct FakeRuntime {
    u64 tick = 1;
    u32 custom_mipmap_runs = 0; ///< GenerateMipmaps on a replacement
    std::vector<u8> staging = std::vector<u8>(1 << 20);
    u64 GetResourceTick() {
        return tick;
    }
    u64 GetResourceFreeTick() {
        return tick + 1;
    }
    void Finish() {}
    StagingData FindStaging(u32 size, bool) {
        return StagingData{.size = size,
                           .offset = 0,
                           .mapped = std::span<u8>{staging.data(), std::min<std::size_t>(
                                                                       size, staging.size())}};
    }
    bool Reinterpret(FakeSurface&, FakeSurface&, const TextureCopy&) {
        return true;
    }
    bool ClearTexture(FakeSurface&, const TextureClear&) {
        return true;
    }
    bool CopyTextures(FakeSurface&, FakeSurface&, std::span<const TextureCopy>) {
        return true;
    }
    bool CopyTextures(FakeSurface&, FakeSurface&, const TextureCopy&) {
        return true;
    }
    bool BlitTextures(FakeSurface&, FakeSurface&, const TextureBlit&) {
        return true;
    }
    /// Every level below the first of the sampled image, made from the one above it
    void GenerateMipmaps(FakeSurface& surface) {
        if (surface.IsCustom()) {
            custom_mipmap_runs++;
        }
        FakeImage& image = *surface.images[surface.current];
        for (std::size_t i = 1; i < image.levels.size(); i++) {
            const std::size_t size = std::max<std::size_t>(image.levels[i - 1].size() / 4, 4);
            image.levels[i].assign(size, image.levels[i - 1].empty() ? 0 : image.levels[i - 1][0]);
            image.generated[i] = true;
        }
    }
    bool NeedsConversion(const FakeSurface&) const {
        return false;
    }
    bool SupportsCustomFormat(CustomPixelFormat) const {
        return true;
    }
};

struct FakeDebugScope {
    template <typename... T>
    FakeDebugScope(FakeRuntime&, Common::Vec4f, fmt::format_string<T...>, T...) {}
    FakeDebugScope(FakeRuntime&, Common::Vec4f, std::string_view) {}
};

struct FakeTraits {
    using Runtime = FakeRuntime;
    using Surface = FakeSurface;
    using Sampler = FakeSampler;
    using Framebuffer = FakeFramebuffer;
    using DebugScope = FakeDebugScope;
};

class FakeWindow final : public Frontend::EmuWindow {
public:
    void PollEvents() override {}
};

class FakeRenderer final : public RendererBase {
public:
    FakeRenderer(Core::System& system, Frontend::EmuWindow& window)
        : RendererBase(system, window, nullptr) {}
    RasterizerInterface* Rasterizer() override {
        return nullptr;
    }
    void SwapBuffers() override {}
    void TryPresent(int, bool) override {}
};

/// The settings these tests change, put back after each
struct SettingsGuard {
    Settings::GraphicsAPI api = Settings::values.graphics_api.GetValue();
    u32 factor = Settings::values.resolution_factor.GetValue();
    Settings::TextureFilter filter = Settings::values.texture_filter.GetValue();
    bool custom = Settings::values.custom_textures.GetValue();
    bool dump = Settings::values.dump_textures.GetValue();

    SettingsGuard(Settings::TextureFilter new_filter, u32 new_factor) {
        Settings::values.graphics_api.SetValue(Settings::GraphicsAPI::Vulkan);
        Settings::values.resolution_factor.SetValue(new_factor);
        Settings::values.texture_filter.SetValue(new_filter);
        Settings::values.custom_textures.SetValue(true);
        Settings::values.dump_textures.SetValue(false);
    }
    ~SettingsGuard() {
        Settings::values.graphics_api.SetValue(api);
        Settings::values.resolution_factor.SetValue(factor);
        Settings::values.texture_filter.SetValue(filter);
        Settings::values.custom_textures.SetValue(custom);
        Settings::values.dump_textures.SetValue(dump);
    }
};

constexpr u32 kFactor = 3;
constexpr u32 kGuestSize = 32; ///< the game's texture, RGBA8
constexpr u32 kPackScale = 4;  ///< the pack's texture is 4x

/// A cache with a fake runtime over the system's memory, and a manager with no pack
struct Rig {
    std::vector<std::unique_ptr<CustomTexture>> textures; ///< the pack's files, outlive it all
    Core::System& system = Core::System::GetInstance();
    Memory::MemorySystem memory{system};
    std::unique_ptr<CustomTexManager> manager;
    FakeRuntime runtime;
    Pica::RegsInternal regs{};
    FakeWindow window;
    std::unique_ptr<FakeRenderer> renderer;
    std::unique_ptr<RasterizerCache<FakeTraits>> cache;

    Rig(bool async, bool skip_mipmap) {
        if (!system.GetImageInterface()) {
            system.RegisterImageInterface(std::make_shared<Frontend::ImageInterface>());
        }
        manager = std::make_unique<CustomTexManager>(system);
        CustomTexManagerTestAccess::Setup(*manager, async, skip_mipmap);
        renderer = std::make_unique<FakeRenderer>(system, window);
        cache = std::make_unique<RasterizerCache<FakeTraits>>(memory, *manager, runtime, regs,
                                                              *renderer);
    }

    /// Fills `size` bytes of FCRAM at `offset` with bytes from `seed`
    std::span<u8> Fill(u32 offset, u32 size, u8 seed) {
        u8* const data = memory.GetFCRAMPointer(offset);
        for (u32 i = 0; i < size; i++) {
            data[i] = static_cast<u8>(seed + i * 7);
        }
        return {data, size};
    }

    /// A pack texture of `size` x `size` RGBA8 for the guest bytes `guest` (a level of a texture)
    std::vector<u8> AddPackTexture(std::span<const u8> guest, u32 size, u8 seed,
                                   bool pending = false) {
        auto& texture =
            textures.emplace_back(std::make_unique<CustomTexture>(*system.GetImageInterface()));
        texture->width = size;
        texture->height = size;
        texture->format = CustomPixelFormat::RGBA8;
        texture->file_format = CustomFileFormat::PNG;
        texture->type = MapType::Color;
        texture->data.resize(size * size * 4);
        for (std::size_t i = 0; i < texture->data.size(); i++) {
            texture->data[i] = static_cast<u8>(seed ^ (i * 13));
        }
        texture->levels.assign(1, CustomLevel{.offset = 0,
                                              .size = texture->data.size(),
                                              .width = size,
                                              .height = size});
        const u64 hash = CustomTexManager::NewHash(guest);
        texture->hashes = {hash};
        Material& material = CustomTexManagerTestAccess::Add(*manager, hash);
        material.AddMapTexture(texture.get());
        if (pending) {
            material.state = DecodeState::Pending; // a worker is reading it
        }
        return texture->data;
    }

    SurfaceId Texture(u32 offset, u32 max_level = 0) {
        Pica::Texture::TextureInfo info{};
        info.physical_address = Memory::FCRAM_PADDR + offset;
        info.width = kGuestSize;
        info.height = kGuestSize;
        info.format = Pica::TexturingRegs::TextureFormat::RGBA8;
        info.SetDefaultStride();
        return cache->GetTextureSurface(info, max_level);
    }

    FakeSurface& Get(SurfaceId id) {
        return cache->GetSurface(id);
    }

    /// The workers finished reading every pending material; the next frame uploads them
    void FinishLoading() {
        for (const auto& texture : textures) {
            for (const u64 hash : texture->hashes) {
                if (Material* material = manager->GetMaterial(hash)) {
                    material->LoadFromDisk(true);
                }
            }
        }
        runtime.tick++;
        cache->TickFrame();
    }
};

constexpr u32 kLevel0 = kGuestSize * kGuestSize * 4;

} // namespace

TEST_CASE("Custom textures: a replaced texture with a texture filter on samples the pack image",
          "[video_core][texfilt]") {
    const auto filter = GENERATE(Settings::TextureFilter::NoFilter,
                                 Settings::TextureFilter::Bicubic, Settings::TextureFilter::xBRZ);
    SettingsGuard settings{filter, kFactor};
    Rig rig{false, true};
    const bool filtering = filter != Settings::TextureFilter::NoFilter;
    INFO("filter " << static_cast<int>(filter));

    // A game texture the pack has no replacement for: the filter still upscales it
    rig.Fill(0x100000, kLevel0, 1);
    FakeSurface& plain = rig.Get(rig.Texture(0x100000));
    CHECK_FALSE(plain.IsCustom());
    CHECK(plain.res_scale == (filtering ? kFactor : 1));
    CHECK(plain.filter_runs == (filtering ? 1 : 0));
    CHECK_FALSE(plain.Sampled().levels[0].empty());

    // One the pack replaces: the draw samples the pack's pixels
    const auto guest = rig.Fill(0x200000, kLevel0, 2);
    const std::vector<u8> pack = rig.AddPackTexture(guest, kGuestSize * kPackScale, 0x40);
    const SurfaceId id = rig.Texture(0x200000);
    FakeSurface& replaced = rig.Get(id);
    REQUIRE(replaced.IsCustom());
    // Before the fix, with a filter on: res_scale 3, a second image sampled and never written
    CHECK(replaced.res_scale == 1);
    CHECK_FALSE(replaced.images[FakeSurface::Scaled].has_value());
    CHECK(replaced.Sampled().width == kGuestSize * kPackScale);
    CHECK(replaced.Sampled().levels[0] == pack);
    CHECK(replaced.filter_runs == 0); // a pack texture is not filtered again

    // Found again on the next draw, the same surface
    CHECK(rig.Texture(0x200000) == id);
}

TEST_CASE("Custom textures: a texture made before its replacement loaded samples it once loaded",
          "[video_core][texfilt]") {
    SettingsGuard settings{Settings::TextureFilter::Bicubic, kFactor};
    Rig rig{true, true};

    const auto guest = rig.Fill(0x300000, kLevel0, 3);
    const std::vector<u8> pack =
        rig.AddPackTexture(guest, kGuestSize * kPackScale, 0x50, /*pending=*/true);
    const SurfaceId id = rig.Texture(0x300000);
    {
        // Until it loads, the game's texture, filtered as any other
        FakeSurface& waiting = rig.Get(id);
        CHECK_FALSE(waiting.IsCustom());
        CHECK(waiting.res_scale == kFactor);
        CHECK(waiting.filter_runs == 1);
        CHECK_FALSE(waiting.Sampled().levels[0].empty());
    }

    rig.FinishLoading();
    FakeSurface& replaced = rig.Get(id);
    REQUIRE(replaced.IsCustom());
    CHECK(replaced.res_scale == 1);
    CHECK_FALSE(replaced.images[FakeSurface::Scaled].has_value());
    CHECK(replaced.Sampled().levels[0] == pack);
    CHECK(rig.Texture(0x300000) == id);
}

TEST_CASE("Custom textures: a replaced texture's mip chain is written, with or without skip_mipmap",
          "[video_core][texfilt]") {
    const bool skip_mipmap = GENERATE(true, false);
    const bool async = GENERATE(false, true);
    const auto filter =
        GENERATE(Settings::TextureFilter::NoFilter, Settings::TextureFilter::Bicubic);
    INFO("skip_mipmap " << skip_mipmap << ", async " << async << ", filter "
                        << static_cast<int>(filter));
    SettingsGuard settings{filter, kFactor};
    Rig rig{async, skip_mipmap};

    // 32x32 with two more levels (16x16, 8x8) back to back
    constexpr u32 kLevel1 = kLevel0 / 4;
    constexpr u32 kLevel2 = kLevel1 / 4;
    const auto guest = rig.Fill(0x400000, kLevel0 + kLevel1 + kLevel2, 4);
    const std::vector<u8> pack = rig.AddPackTexture(guest.first(kLevel0), kGuestSize * kPackScale,
                                                    0x60, async);
    const SurfaceId id = rig.Texture(0x400000, 2);
    if (async) {
        rig.FinishLoading();
    }
    FakeSurface& replaced = rig.Get(id);
    REQUIRE(replaced.IsCustom());
    CHECK(replaced.res_scale == 1);
    const FakeImage& image = replaced.Sampled();
    REQUIRE(image.levels.size() == 3);
    CHECK(image.levels[0] == pack);
    // The levels below come from the pack's level 0. Before the fix a pack that keeps mipmaps
    // (skip_mipmap false) with no file for them left them unwritten
    CHECK(image.generated[1]);
    CHECK(image.generated[2]);
    CHECK(rig.runtime.custom_mipmap_runs == 1);
}

TEST_CASE("Custom textures: a level the pack has a file for is not overwritten by the generated one",
          "[video_core][texfilt]") {
    const bool async = GENERATE(false, true);
    SettingsGuard settings{Settings::TextureFilter::Bicubic, kFactor};
    Rig rig{async, /*skip_mipmap=*/false};

    constexpr u32 kLevel1 = kLevel0 / 4;
    constexpr u32 kLevel2 = kLevel1 / 4;
    const auto guest = rig.Fill(0x500000, kLevel0 + kLevel1 + kLevel2, 5);
    const std::vector<u8> pack0 = rig.AddPackTexture(guest.first(kLevel0), kGuestSize * kPackScale,
                                                     0x70, async);
    const std::vector<u8> pack1 = rig.AddPackTexture(
        guest.subspan(kLevel0, kLevel1), kGuestSize * kPackScale / 2, 0x80, async);
    const SurfaceId id = rig.Texture(0x500000, 2);
    if (async) {
        rig.FinishLoading();
    }
    FakeSurface& replaced = rig.Get(id);
    REQUIRE(replaced.IsCustom());
    const FakeImage& image = replaced.Sampled();
    CHECK(image.levels[0] == pack0);
    CHECK(image.levels[1] == pack1);
    CHECK_FALSE(image.generated[1]);
    CHECK(image.generated[2]);
}
