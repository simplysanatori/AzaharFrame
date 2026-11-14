// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame savemem: RasterizerCache::ClearAll, which every save state calls (System::serialize,
// gpu->ClearAll(true)), must free the surfaces it drops. It used to clear only the page table, so
// each save left the game's whole working set of textures and render targets behind, alive (with
// their GPU images) until the cache was destroyed, and the game made them all again: on the Frame
// the memory grew by hundreds of MB with every quick save. The cache is driven here with a fake
// runtime whose surfaces count themselves.

#include <span>
#include <string_view>
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include "common/vector_math.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/image_interface.h"
#include "core/memory.h"
#include "video_core/custom_textures/custom_tex_manager.h"
#include "video_core/pica/regs_internal.h"
#include "video_core/rasterizer_cache/framebuffer_base.h"
#include "video_core/rasterizer_cache/rasterizer_cache.h"
#include "video_core/rasterizer_cache/sampler_params.h"
#include "video_core/rasterizer_cache/surface_base.h"
#include "video_core/rasterizer_cache/utils.h"
#include "video_core/renderer_base.h"

namespace {

using namespace VideoCore;

struct FakeRuntime;

/// A surface that counts how many exist (its "GPU image").
struct FakeSurface : SurfaceBase {
    static inline int live = 0;
    FakeSurface(FakeRuntime&, const SurfaceParams& params, const SurfaceFlagBits& flags = {})
        : SurfaceBase(params, flags) {
        live++;
    }
    FakeSurface(FakeRuntime&, const SurfaceBase& surface, const Material*) : SurfaceBase(surface) {
        live++;
    }
    FakeSurface(FakeSurface&& other) noexcept : SurfaceBase(std::move(other)) {
        live++;
    }
    FakeSurface& operator=(FakeSurface&&) noexcept = default;
    FakeSurface(const FakeSurface&) = delete;
    FakeSurface& operator=(const FakeSurface&) = delete;
    ~FakeSurface() {
        live--;
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
    void Upload(const BufferTextureCopy&, const StagingData&) {}
    void UploadCustom(const Material*, u32) {}
    void Download(const BufferTextureCopy&, const StagingData&) {}
    void ScaleUp(u32 new_scale) {
        res_scale = new_scale;
    }
    u32 GetInternalBytesPerPixel() const {
        return 4;
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
    std::vector<u8> staging = std::vector<u8>(1 << 20);
    u64 GetResourceTick() {
        return tick;
    }
    u64 GetResourceFreeTick() {
        return tick + 1; // every submission done: whatever was sentenced may go
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
    void GenerateMipmaps(FakeSurface&) {}
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

/// A 128x128 RGBA8 render target at `addr` (a tiled colour buffer, as a game's).
SurfaceParams Target(PAddr addr) {
    SurfaceParams p{};
    p.addr = addr;
    p.width = 128;
    p.height = 128;
    p.stride = 128;
    p.is_tiled = true;
    p.texture_type = TextureType::Texture2D;
    p.pixel_format = PixelFormat::RGBA8;
    p.type = SurfaceType::Color;
    p.res_scale = 1;
    p.UpdateParams();
    return p;
}

} // namespace

TEST_CASE("Rasterizer cache: a save's ClearAll frees what it drops", "[video_core][savemem]") {
    Core::System& system = Core::System::GetInstance();
    Memory::MemorySystem memory{system};
    if (!system.GetImageInterface()) {
        system.RegisterImageInterface(std::make_shared<Frontend::ImageInterface>());
    }
    CustomTexManager custom_tex_manager{system};
    FakeRuntime runtime;
    Pica::RegsInternal regs{};
    FakeWindow window;
    FakeRenderer renderer{system, window};
    RasterizerCache<FakeTraits> cache{memory, custom_tex_manager, runtime, regs, renderer};

    const int null_surfaces = FakeSurface::live; // the cache's own null handles
    constexpr int kWorkingSet = 40;              // a game's textures and render targets
    const auto frame = [&] {
        for (int i = 0; i < kWorkingSet; i++) {
            const SurfaceId id = cache.GetSurface(Target(Memory::FCRAM_PADDR + i * 0x10000),
                                                  ScaleMatch::Ignore, false);
            REQUIRE(id);
        }
        runtime.tick++;
        cache.TickFrame();
    };

    frame();
    REQUIRE(FakeSurface::live == null_surfaces + kWorkingSet);
    frame(); // the same surfaces again: found, none made
    REQUIRE(FakeSurface::live == null_surfaces + kWorkingSet);

    // Ten saves in a row, the game playing a few frames between them
    for (int save = 0; save < 10; save++) {
        cache.ClearAll(true);
        for (int f = 0; f < 3; f++) {
            frame();
        }
        INFO("save " << save);
        // Before the fix: kWorkingSet more after each save, never freed
        CHECK(FakeSurface::live == null_surfaces + kWorkingSet);
    }

    // A save with nothing played after it: the dropped surfaces go at the next collection
    cache.ClearAll(true);
    runtime.tick++;
    cache.TickFrame();
    CHECK(FakeSurface::live == null_surfaces);
}

// AzaharFrame savemem: a state load's crash (Kid Icarus, Turnip in vkUpdateDescriptorSets): a
// disabled cube texture unit was bound with GetSampler(NULL_SURFACE_CUBE_ID), sampler 1, which a
// fresh cache (right after the load) does not have: the "handle" came from past the array. The
// renderer now asks for NULL_SAMPLER_ID, and an id that names no sampler gets the null sampler.
TEST_CASE("Rasterizer cache: a sampler id that names no sampler gets the null sampler",
          "[video_core][savemem]") {
    Core::System& system = Core::System::GetInstance();
    Memory::MemorySystem memory{system};
    if (!system.GetImageInterface()) {
        system.RegisterImageInterface(std::make_shared<Frontend::ImageInterface>());
    }
    CustomTexManager custom_tex_manager{system};
    FakeRuntime runtime;
    Pica::RegsInternal regs{};
    FakeWindow window;
    FakeRenderer renderer{system, window};
    RasterizerCache<FakeTraits> cache{memory, custom_tex_manager, runtime, regs, renderer};

    FakeSampler& null_sampler = cache.GetSampler(NULL_SAMPLER_ID);
    // A fresh cache has the null sampler only
    CHECK(&cache.GetSampler(SamplerId{1}) == &null_sampler);
    CHECK(&cache.GetSampler(NULL_SURFACE_CUBE_ID) == &null_sampler);
    CHECK(&cache.GetSampler(SamplerId{4096}) == &null_sampler);
    CHECK(&cache.GetSampler(SamplerId{}) == &null_sampler); // an invalid id too
}
