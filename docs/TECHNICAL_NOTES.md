# AzaharFrame: technical notes

Azahar for Frame is a fork of [CitraVR](https://github.com/amwatson/CitraVR) that runs on the Valve Steam Frame. CitraVR is the Meta Quest build of [Azahar](https://github.com/azahar-emu/azahar). This document is for the person who maintains the fork: what is changed in the emulator's files, how to merge new versions, how to build and package. How the port works is in [TECHNICAL.md](TECHNICAL.md).

## Where the code is

New code, in files of its own:

| Path | What |
|---|---|
| `src/frame_xr/` | the OpenXR session, the frame loop, input (`input/`), the game picker (`picker/`, `ui/`), immersive mode, the controller models, beams, controls help and stats (`vis/`) |
| `src/video_core/vulkan_hooks.h` | the instance and device hooks, and the screen sink |
| `src/video_core/vr_immersive.h` | the head pose snapshot for immersive mode |
| `src/video_core/renderer_vulkan/vk_xr_target.*` | the three OpenXR swapchains, the spill images and the handoff thread (`vk_xr_handoff.h`: its rules) |
| `src/video_core/renderer_vulkan/renderer_vulkan_xr.cpp` | `RenderToXr()` |
| `src/input_common/frame_xr.*` | the Frame controller input engine |
| `src/frame_xr/input/mouse_pointer.*` | the mouse's Pointer mode, adapted from Frametop's pointer helper (MIT, see [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md#frametop)) |
| `src/citra_qt/configuration/configure_frame_vr.*`, `src/citra_qt/frame_branding.h` | the **VR** group in the Graphics tab, the Azahar for Frame icon |
| `src/citra_qt/frame_texture_pack.h` | **Optimize Texture Pack for Frame...** and **Use Original Texture Pack** in the game list menu |
| `src/tests/frame_xr/` | unit tests for the input mapper, the screen geometry and the picker |
| `third_party/openxr/` | the OpenXR headers the code is built against (Apache-2.0 OR MIT) |
| `tools/xrmock/`, `tools/mockrun.sh` | a mock OpenXR runtime for running without a headset |
| `steam-frame/` | installer, launcher, dock icon, build and release scripts, the library art |
| `steam-frame/tools/` | the texture pack converter: `convert-texture-pack.sh` and `convert_texture_pack.py`, the encoder `af-ktxenc.cpp` and `build-astcenc.sh` |
| `docs/` | these documents |

## Upstream files changed

The base is Azahar's `2127.0-alpha4` tag. Every change this fork made in a file that exists upstream is marked `AzaharFrame` on or next to the line (the first command below lists them), so that a merge conflict is easy to resolve and the list below is easy to check:

```sh
git grep -n "AzaharFrame" -- src CMakeLists.txt externals ':!src/frame_xr' ':!src/tests'
git diff --stat 2127.0-alpha4...HEAD -- src CMakeLists.txt externals ':!src/android'
```

Files with `AzaharFrame` marks (53 files differ from the tag in `src`, `CMakeLists.txt` and `externals`; these are the ones this fork changed):

| File | Change |
|---|---|
| `CMakeLists.txt`, `src/CMakeLists.txt` | `ENABLE_FRAME_XR` (on by default on Linux aarch64) and its compile definition, the `frame_xr` subdirectory |
| `src/citra_qt/CMakeLists.txt`, `src/video_core/CMakeLists.txt`, `src/input_common/CMakeLists.txt`, `src/tests/CMakeLists.txt` | the new source files and tests |
| `src/citra_qt/citra_qt.cpp` | `--vr` option, starts and stops `frame_xr`, hands it the boot and shutdown calls, the touch sink, the game folders for the picker, the Frame controls profile, the 3D depth default, the name and icon, the controls and stats it sends the VR panels, the **Show Stats** switch from VR |
| `src/citra_qt/applets/swkbd.{h,cpp}`, `mii_selector.cpp` | in VR the game keyboard in the headset (`frame_xr/swkbd.h`) instead of the dialog, and the first Mii instead of the Mii Selector dialog; the game keyboard closes when the game stops (`citra_qt.cpp`) |
| `src/citra_qt/aboutdialog.cpp` | the Azahar for Frame icon |
| `src/citra_qt/configuration/config.{h,cpp}` | reads and saves the `[VR]` section (CitraVR's keys) |
| `src/citra_qt/configuration/configure_graphics.{h,cpp}` | the **VR** group |
| `src/citra_qt/bootmanager.{h,cpp}` | the preload reports its own Preload stage and `PreloadSkipped` when it did not happen |
| `src/citra_qt/configuration/configure_enhancements.{h,cpp}` | turning Preload on asks **Preload Textures?** first |
| `src/common/settings.{h,cpp}` | the `vr_*` immersive settings, `vr_hide_bottom_screen`, `vr_top_aspect`, `vr_immersive_game_fov`, `vr_immersive_wider_view` and `vr_immersive_squeeze` as per-game settings, the global `vr_show_controllers` and `vr_show_stats` |
| `src/input_common/main.cpp` | registers the `frame_xr` engine, its pollers and texts; resets the GC adapter before static destruction |
| `src/input_common/gcadapter/gc_adapter.{h,cpp}` | a failed libusb init no longer crashes at exit |
| `src/video_core/gpu.cpp` | applies the newest head pose after each frame |
| `src/video_core/renderer_base.cpp` | two resolution steps more in immersive mode |
| `src/video_core/rasterizer_accelerated.cpp` | uploads the PICA uniforms again when the head pose changes; mode 3 patches only draws whose register holds a world camera (`vr_view_matrix.h`, new) |
| `src/video_core/renderer_vulkan/vk_platform.cpp`, `vk_instance.cpp` | the instance and device hooks; a device it did not create is not destroyed; `textureCompressionASTC_LDR` and `textureCompressionBC` enabled, and a compressed custom format counts as supported only with its feature |
| `src/video_core/custom_textures/{material,custom_format,custom_tex_manager}.{h,cpp}` | DDS and KTX keep their whole mip chain; texture data (PNG too) is dropped after upload and read again when needed; a byte limit for uploads in one frame; DDS allowed with `skip_mipmap`; a file that fails to load fails its material; a preload only when the pack fits in memory, stopped when memory runs low, and its KTX/DDS copies dropped after upload ([Preload and the memory limit](#preload-and-the-memory-limit)) |
| `src/video_core/rasterizer_cache/rasterizer_cache.h` | no generated mipmaps for a texture with a stored mip chain; a custom format the GPU cannot sample keeps the original texture |
| `src/video_core/renderer_vulkan/vk_texture_runtime.cpp`, `src/video_core/renderer_opengl/gl_texture_runtime.cpp` | `UploadCustom` uploads every stored level; the image of a custom texture has no more levels than its file |
| `src/citra_qt/game_list.cpp` | the texture pack entry in the game's menu |
| `src/video_core/renderer_vulkan/vk_swapchain.cpp` | Mailbox or Immediate presentation of the desktop window in VR |
| `src/video_core/renderer_vulkan/renderer_vulkan.{h,cpp}` | `RenderToXr()` from `SwapBuffers()`, the XR target, the queue lock, no second window in VR |
| `src/video_core/shader/generator/shader_uniforms.h` | `vr_immersive_mode_factor` defaults to 1 (it was 0, which divides by zero outside CitraVR's Android app) |
| `src/citra_qt/main.ui`, `aboutdialog.ui` | the name "Azahar for Frame" (no mark possible in the XML) |

Not in the table, because the tag has no such file: `AI-POLICY.md`, `.github/workflows/` and the pull request template are removed, and the issue templates and `README.md` are rewritten.

The CitraVR changes that came with the fork and that Azahar for Frame keeps unchanged carry no mark. They are in: `src/common/settings.{h,cpp}` (the `vr_*` settings, `factor_3d` 50 and `resolution_factor` Auto as defaults), `src/common/stall_telemetry.h`, `src/common/logging/{filter.cpp,types.h}` (the `VR` log class), `src/core/perf_stats.{h,cpp}`, `src/core/frontend/{emu_window.cpp,framebuffer_layout.cpp}`, `src/core/hle/kernel/shared_page.cpp` and `src/core/hle/service/hid/hid.cpp` (the 3D depth the game reads; about 0 in the Other Games immersive mode), `src/video_core/rasterizer_accelerated.{h,cpp}` and `rasterizer_interface.h` (`SetVRData`), `renderer_base.h`, `renderer_vulkan/vk_{pipeline_cache,rasterizer}.*`, `renderer_opengl/*` (shader telemetry and the cache save call), `shader/generator/glsl_shader_{gen,decompiler}.cpp` and `shader_gen.cpp` (the immersive factor), and `src/audio_core/CMakeLists.txt`, `src/common/CMakeLists.txt`, `externals/CMakeLists.txt`. The Android app under `src/android/` is not built.

## Merging new versions

The remotes are `azahar` (azahar-emu/azahar), `citravr` (amwatson/CitraVR) and `origin` (this repository).

**Where it stands.** The branch `steam-frame` last took Azahar 2127.0-alpha4 in the merge commit `c7873cd44` ("Merge Azahar 2127.0-alpha4 into the Steam Frame branch"). Its second parent is Azahar's `e3c916e41`, the commit the `2127.0-alpha4` tag points at. CitraVR master as of that merge is `f71bf657d`.

**The content-level merge base.** CitraVR carries Azahar's code, but under rewritten commit ids: its history was rebuilt, so `git merge-base citravr/master azahar/master` finds an old commit (`3a4ebb141`) and a plain merge of either side shows a large false difference. That is why the two are never merged the same way:

1. **A new Azahar release (tag).** Merge the tag. Azahar's history is intact in this repository, so the merge base is the previous tag and only real conflicts appear, in the files marked `// AzaharFrame` and in the CitraVR files above.

   ```sh
   git fetch azahar --tags
   git switch -c merge/azahar-<tag> steam-frame
   git merge <tag>
   git grep -n "// AzaharFrame" -- src      # every marker must survive
   git grep -n "vr_immersive_mode_factor"   # the CitraVR shader change must survive
   ```

2. **New CitraVR commits.** Do not merge `citravr/master`. List what CitraVR added since the last sync and take the commits that touch the CitraVR files above (or the files this fork uses), one at a time:

   ```sh
   git fetch citravr
   git log --oneline f71bf657d..citravr/master
   git cherry-pick <commit>        # or: git diff <commit>^ <commit> | git apply -3
   ```

   If CitraVR has itself moved to a newer Azahar, take Azahar from step 1 and only the VR commits from here. After a sync, update the two commit ids in this section.

After either merge, rebuild with `steam-frame/build.sh`, run a game, and check the markers and the immersive shader change as above.

## Building

All on the Frame, with the tools SteamOS has: a C++ compiler (clang or gcc), CMake 3.24 or newer, Ninja, Qt 6 and SDL2 development files, Python 3, and SteamVR installed for running it.

```sh
steam-frame/build.sh 1.2.0      # configures build/, builds, packages steam-frame/release/azaharframe-1.2.0-linux-arm64.tar.gz and SHA256SUMS
steam-frame/install.sh          # installs that tarball and adds the Steam entry
```

The build uses Ninja, Release, `ENABLE_FFMPEG=OFF`, `USE_AUDIOTRACK=OFF`, Vulkan on and OpenGL off, tests, the room server, the web service, Discord and scripting off. `ENABLE_FRAME_XR` is on by default on Linux aarch64. It runs at the lowest priority and waits while the headset is in use (`~/.cache/headset-in-use`, or a game running under Steam), because a build next to a VR session ruins the session. `ccache` is used when it is installed (`~/.cache/azaharframe/ccache`). `CF_EXTRA_CMAKE_FLAGS` adds CMake flags, for example `-DENABLE_TESTS=ON` to build the unit tests in `src/tests/frame_xr`.

`ENABLE_FRAME_XR` needs no flag: `CMakeLists.txt` turns it on for Linux aarch64 (with Vulkan and Qt on), and `src/frame_xr/CMakeLists.txt` takes the OpenXR headers from `third_party/openxr/include` in the tree.

That build links the Qt 6 and SDL2 SteamOS has. The release is the portable build, `steam-frame/build-portable.sh 1.2.0`, which carries its own Qt and carries SDL3 and sdl2-compat; see [AGENTS.md](../AGENTS.md#portable-build).

`steam-frame/release/make-release.sh [version [build-dir]]` packs an existing build without building or touching Steam. Tarballs and `SHA256SUMS` in `steam-frame/release/` are ignored by git.

The release tarball holds, at its top level and without a folder around them:

| File | What |
|---|---|
| `azahar` | the emulator, stripped |
| `azaharframe.sh` | the Steam shortcut's target |
| `install.sh`, `uninstall.sh` | install and remove (see below) |
| `steam-cef.py`, `dock-icon.py` | the Steam library entry and the SteamVR dock icon |
| `VERSION`, `LICENSE.txt`, `THIRD_PARTY_NOTICES.md` | |
| `art/` | the library art (`portrait`, `wide`, `hero`, `logo`) and the icon in four sizes; the program itself reads `logo.png` and `icon.png` |
| `tools/` | the texture pack converter (see [Texture packs in ASTC](#texture-packs-in-astc)) and `frame-controller-models.py` |
| `framedrop-run.sh`, `make-framedrop-zip.py` | FrameDrop's start script and the zip builder (see [FrameDrop](#framedrop)) |
| `lib/`, `plugins/`, `qt.conf` | the portable build's own Qt, ICU and plugins |

Files are packed by name, so settings, saves, keys, logs and game files cannot end up in it.

## Install and uninstall

`install.sh [--dry-run] [tarball]` copies the program and the helper scripts to `~/Games/AzaharFrame/app`, adds or updates the library entry "Azahar for Frame" through Steam's debugging port (`steam-cef.py`), sets its art and icon, writes the SteamVR dock icon files (`dock-icon.py install`), and sets SteamVR's per-app settings for the entry. Without an argument it installs the folder it is in when that is an unpacked release, else the newest tarball in `steam-frame/release/`, else the build in `build/bin/Release`. It never asks a question and exits 0 when Steam cannot be reached, after installing the files. Running it again updates the same entry, and your games, saves, keys and settings in `~/.local/share/AzaharFrame` are never touched. It refuses to run while the emulator is running.

`--dry-run` (or `AZAHARFRAME_DRY_RUN=1`) prints every file it would copy and every Steam and SteamVR call (with the JavaScript for Steam), contacts nothing and changes nothing.

`uninstall.sh [--purge]` removes the library entry (when Steam is reachable), the dock icon files (`azaharframe.desktop`, the `azaharframe` and `azahar` PNGs in `~/.local/share/icons/hicolor`, the SteamVR app manifest and its line in SteamVR's `appconfig.json`) and the program folder. `--purge` also deletes `~/.local/share/AzaharFrame` and `~/.cache/azaharframe`. SteamVR's per-app settings are left alone.

**Trial install.** Both scripts can work in a scratch folder without touching Steam or SteamVR:

```sh
export HOME=/tmp/af/h XDG_DATA_HOME=/tmp/af/h/.local/share
export AZAHARFRAME_APP=/tmp/af/app AZAHARFRAME_HOME=/tmp/af/home AZAHARFRAME_SKIP_STEAM=1
steam-frame/install.sh TARBALL
steam-frame/uninstall.sh --purge
```

With `AZAHARFRAME_SKIP_STEAM=1` the dock icon steps are skipped too (installing needs the entry's appid). To try them alone, with `HOME` at a scratch folder and `AZAHARFRAME_VRSERVER_RUNNING=0`, run `dock-icon.py install DEST --appid N` and `dock-icon.py remove DEST`.

## Texture packs in ASTC

Azahar decodes a PNG texture on a worker thread when a surface first needs it: about 0.4 s for 2048×2048 and 1.6 s for 4096×4096 on the Frame, while the game shows the original. Upstream keeps the decoded RGBA8 copy for the rest of the session; here it is dropped after upload (see Memory below), but every redraw of a new surface decodes again, and a large pack cannot be preloaded (the Ocarina of Time 3D 4K pack is 18 GB decoded). A KTX file in ASTC needs no decoding: reading it is the whole job.

**Format.** ASTC 4x4 by default (8 bits a texel, a quarter of RGBA8), 6x6 or 8x6 as options; those three are the ASTC sizes Azahar maps (`ASTC4`, `ASTC6`, `ASTC8` = 8x6). KTX 1, because dds-ktx reads KTX 1 only. Adreno has ASTC LDR in hardware; Turnip reports `textureCompressionASTC_LDR`, which Azahar did not enable on the device, so I enable it (and `textureCompressionBC`) and count a compressed format as supported only when its feature is on and `vkGetPhysicalDeviceFormatProperties` gives it `SAMPLED_IMAGE`. Without it the format is logged once at start and those textures keep the original.

**Colour.** Azahar uploads a custom PNG as `R8G8B8A8_UNORM`, the stored bytes with no sRGB decoding. The converter therefore encodes with the linear LDR profile and writes `GL_COMPRESSED_RGBA_ASTC_*_KHR`, which becomes `ASTC_*_UNORM_BLOCK`: the GPU returns the same values the PNG held. (An sRGB encode would come out darker; dds-ktx would not read the sRGB KTX format anyway.) Alpha is kept as stored.

**Flip.** With `flip_png_files`, Azahar turns PNGs upside down after decoding, and only PNGs. The encoder flips the image before compressing, so the KTX shows the same way; `pack.json` is copied unchanged, since any PNG kept in the pack still needs the flag.

**Mips.** For a pack with `skip_mipmap` Azahar generated the chain on the GPU with linear blits, which a compressed image cannot take. The encoder makes the whole chain down to 1×1 the same way (each level the average of the 2×2 texels above it, in the stored 8-bit values) and compresses every level. In the emulator, `LoadDDS` now finds every level of a DDS or KTX itself, because `ddsktx_get_sub` sizes compressed levels in 4x4 blocks, which is wrong for ASTC 6x6 and 8x6. `UploadCustom` copies all the levels the image has in one command, and mipmaps are never generated for a material with a stored chain. When a file has fewer levels than the guest surface, the custom image gets only as many levels as the file (`Material::ImageLevels`), so no level is left unwritten; a single-level DDS in a pack that has one file per level (no `skip_mipmap`) keeps the old behaviour. This does not depend on `skip_mipmap`, which also no longer rejects DDS files.

**Memory.** After a KTX or DDS material is uploaded, and no other upload waits for it, its file data is freed and the material goes back to unloaded; the next surface that needs it reads the file again on a worker (a few milliseconds). PNGs are freed the same way and decoded again when needed (memguard: keeping them grew memory with every texture the game had shown, up to the whole pack decoded). A file mapped to more than one hash in `pack.json` is kept. After a preload, KTX and DDS copies are freed and PNGs kept. A frame also stops starting uploads past 96 MiB, so a burst of large textures is spread over a few frames.

**The converter.** `convert_texture_pack.py` (run by `convert-texture-pack.sh` with `python3 -I`, standard library only, at nice 19 and idle I/O priority) walks the pack, runs one `af-ktxenc` per PNG on all CPUs but one (largest first), links or copies every other file, and rewrites `.png` names in the `textures` map of `pack.json` to `.ktx`. A KTX is written as `.part` and renamed, and a PNG whose KTX is newer is skipped, so a stopped run carries on. A PNG the encoder cannot read is copied as it is. The new pack gets `.azaharframe-ktx.json` (format, quality, and the original's path relative to it); the emulator ignores it. `--activate` repoints `load/textures/<TitleID>` when it is a symlink (my own setup: `textures/<TitleID>` links to `textures-4k/<TitleID>`), or renames a real folder to `<TitleID>-original` and puts a symlink in its place; `--restore` reads the marker and undoes either.

**The encoder.** `af-ktxenc.cpp` links ARM's astcenc library (pinned to 5.7.1, built with NEON by `build-astcenc.sh`) and decodes PNGs with the stb_image that astcenc ships, so it needs nothing that SteamOS lacks besides git, CMake and a compiler. One texture per process, single-threaded, at astcenc's `medium` effort by default. Python has no image library on SteamOS (no Pillow, ImageMagick or libvips), which is why the mip chain is made in C++.

Numbers for the Ocarina of Time 3D 4K pack (2308 PNGs, 4.5 GB, 4.53 billion texels in level 0): 6.03 GB as ASTC 4x4 with mips (from `--estimate`), about 2.7 GB as 6x6, 2.0 GB as 8x6. A 4096×4096 texture is 22.4 MB as ASTC 4x4 with its 13 levels (10.0 MB as 6x6), against 89 MB of GPU memory as RGBA8 with generated mips. Encoding one 4096×4096 texture on one Frame core, with a build running beside it at the same priority: 18 s at `medium`, 6.4 s at `fast`, 22 s for 6x6 at `medium`. For the whole pack that is about 82 CPU minutes at `medium` (about 15 to 20 minutes on seven cores) and 29 at `fast`. Decoded and compared with the flipped PNG, level 0 is 47.5 to 48 dB PSNR in colour at `medium` (fast 0.7 dB less, 6x6 about 40.5 dB), and the channel means match to 0.05, so nothing is lightened or darkened. Building astcenc and the encoder took 14 s with two jobs.

## Preload and the memory limit

Preload Custom Textures reads every file of the pack into memory before the game starts. The Frame shares its 15.3 GB with SteamVR, passthrough, the compositor and the GPU. Two starts of Ocarina of Time 3D with the 4K pack as ASTC (2308 files, 6.03 GB, about 5.6 GB of file data held in memory) froze the headset at 1957 of 2310: the old limit allowed all of RAM but 2 GB, the system thrashed in zram for over a minute (one display frame took 57 s), and a watchdog rebooted it. No process was killed.

- **The budget** (`video_core/custom_textures/preload_budget.h`, unit-tested in `src/tests/frame_xr/preload_budget.cpp`). The pack's cost is worked out before anything is read (`CustomTexManager::PreloadEstimate`): a DDS or KTX file as it is on disk, a PNG decoded (width x height x 4 from its header), each file once. It must fit, with 10 % to spare, in MemAvailable less a reserve of 4 GB (or a quarter of RAM, if more; `AZAHARFRAME_PRELOAD_RESERVE_MB`), and in what this process's cgroup still allows under its MemoryHigh or MemoryMax (page cache not counted) less 1.5 GB (or 20 % of the limit). Otherwise nothing is preloaded and the pack loads as needed, with the toast **Texture Pack Too Large to Preload** (the status bar in Flat). While it reads, the same room is read again every 8 materials or 64 MB; under a reserve it stops, drops what it read, loads as needed, and says **Preload Stopped: Low Memory**. Each file's page cache is dropped once read (`POSIX_FADV_DONTNEED`), so the pack is not held twice.
- **After a preload** the CPU copy of a KTX or DDS material is freed once it is uploaded (the GPU copy is in the same RAM); a PNG keeps its decoded copy, which is what the preload saves.
- **The question.** Turning Preload on asks **Preload Textures?** with the pack's cost, the memory in use, Azahar's limit and whether the pack fits now, from the same functions (`Summarize`, `MakeConfirmText`): the VR menu's Textures segment (`MenuCommand::AskPreload`, `ModsDialogKind::Preload`) and Flat's Enhancements checkbox (`citra_qt/frame_preload_confirm.cpp`). Off by default.
- **The memory limit** (`azaharframe.sh`). The emulator runs in a user scope, `systemd-run --user --scope -p MemoryHigh -p MemoryMax -p MemorySwapMax`. MemoryMax is the smaller of 65 % of RAM (`AZAHARFRAME_MEMORY_MAX_PCT`, 30 to 90) and MemAvailable at launch less 2 GB for SteamVR and the compositor (`AZAHARFRAME_MEMORY_RESERVE_MB`), but at least 4 GB (`AZAHARFRAME_MEMORY_FLOOR_MB`: a game with its GPU memory is about 2.2 GB here, and writing a save state adds about 0.9 GB for a moment); `AZAHARFRAME_MEMORY_MAX_MB` sets it outright. MemoryHigh is 1/12 less, with 1 GB of swap (`AZAHARFRAME_MEMORY_SWAP_MB`). GPU memory is charged to the scope too: msm's buffers are shmem, and the scope's `memory.stat` shmem matches `drm-total-memory` in the emulator's fdinfo. systemd-run executes the emulator itself, so its process, environment, folder, parent (Steam's reaper) and `oom_score_adj` stay as they were; only the cgroup changes. A trial scope runs first (about 10 ms); when systemd-run is missing or fails, the emulator runs as before. `launch.log` says which (`memory: ...`). `AZAHARFRAME_MEMORY_SCOPE=0` turns it off. A runaway emulator is then killed by the kernel (exit status 137) instead of freezing the headset. The scope is not collected, so its result can be read: on `oom-kill` the launcher writes the limit to `out-of-memory` in the data folder, and the next start shows **Azahar Ran Out of Memory** once (VR and Flat), naming the pack and Preload as the usual cause. When a game stops, the log has `memory after stop: RSS ... (heap, files, shared), GPU ..., group ..., available ...` (after `malloc_trim`) and the custom texture data still held.
- **A way out while a game starts** (`frame_xr/startup_escape.h`). Until the game's screens are live (preload, shader cache) the game's controls do not run. Menu or View held 3 s stops the game (Stopping... from 1.5 s); if the game still runs 10 s later, Azahar quits.
- **Tests.** `steam-frame/test/launch-mode-test.sh` covers the scope. Mock runs: `AZAHARFRAME_PRELOAD_FORCE=1` skips the check before the preload (the check while it reads stays), `AZAHARFRAME_TEST_PRELOAD_DELAY_MS` waits after each material, `AZAHARFRAME_TEST_PRELOAD_SHOT=DIR:SECONDS` saves the Flat question; `frame_xr_menu_dump` draws the VR one with this machine's numbers (`AZAHARFRAME_DUMP_PRELOAD_BIG`, `_SMALL`: the packs' bytes). Run anything with a real large pack inside a capped scope (`systemd-run --user --scope -p MemoryMax=3G -p MemorySwapMax=0 ...`).

## Environment variables

Set these in `~/.local/share/AzaharFrame/session.env` (one `VAR=value` per line) when you start from the Steam library, which passes none.

| Variable | Read by | What |
|---|---|---|
| `AZAHARFRAME_FLAT=1` or `0` | `azaharframe.sh` | start without `--vr` (the desktop window only), or force VR, for one launch; otherwise the mode comes from the file `mode` in the data folder (`vr` or `flat`, VR when it is missing), and `--flat` / `--vr` on the command line beat both |
| `AZAHARFRAME_REFRESH_HZ` | `azaharframe.sh` | 90 or 120 (default 120), set in SteamVR for the entry |
| `AZAHARFRAME_MEMORY_SCOPE=0`, `AZAHARFRAME_MEMORY_MAX_PCT`, `AZAHARFRAME_MEMORY_RESERVE_MB`, `AZAHARFRAME_MEMORY_FLOOR_MB`, `AZAHARFRAME_MEMORY_MAX_MB`, `AZAHARFRAME_MEMORY_SWAP_MB` | `azaharframe.sh` | the memory limit: none, the most as a share of RAM (default 65), what is kept free for SteamVR (default 2048), the least (default 4096), a fixed limit, swap in MB (default 1024); see [Preload and the memory limit](#preload-and-the-memory-limit) |
| `AZAHARFRAME_PRELOAD_RESERVE_MB` | `custom_tex_manager.cpp` | memory a texture preload leaves to the rest of the system (default 4096 or a quarter of RAM) |
| `AZAHARFRAME_PRELOAD_FORCE=1`, `AZAHARFRAME_TEST_PRELOAD_DELAY_MS`, `AZAHARFRAME_TEST_PRELOAD_SHOT` | `custom_tex_manager.cpp`, `citra_qt.cpp` | preload tests (see the same section). Never for playing |
| `AZAHARFRAME_FDM=1` | `azaharframe.sh`, `citra_qt.cpp` | leaves Valve's FDM injection layer on (by default `DISABLE_VULKAN_FDM_INJECTION_LAYER=1` turns it off, see [AGENTS.md](../AGENTS.md)) |
| `AZAHARFRAME_IMMERSIVE_FOV`, `_DISTANCE`, `_LOCAL` | `frame_xr/immersive.cpp` | immersive mode tuning (the FOV one overrides the Immersive Game FOV setting), see [IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md) |
| `AZAHARFRAME_PADS=0` | `frame_xr/input/devices.cpp` | other gamepads stay out of the game in VR (they are still listed in Edit Controls), see [MAPPING.md](../src/frame_xr/input/MAPPING.md#other-gamepads-in-vr-azaharframe-pads) |
| `AZAHARFRAME_OPENXR_LOADER` | `frame_xr/xr_loader.cpp` | path of an OpenXR loader to use before SteamVR's |
| `AZAHARFRAME_ART` | picker, Qt icon | folder with `logo.png` and `icon.png` (default: `art/` next to the program) |
| `AZAHARFRAME_TOOLS` | game list menu | folder with `convert-texture-pack.sh` (default: `tools/` next to the program) |
| `AF_KTXENC` | `convert_texture_pack.py` | the encoder to use (default: `~/.local/share/AzaharFrame/tools/af-ktxenc`) |
| `XR_RUNTIME_JSON` | the OpenXR loader | the runtime; the launcher sets SteamVR's when nothing else names one |
| `QT_SCALE_FACTOR` | Qt | scales the settings window |
| `AZAHARFRAME_FLAT_SCREEN` | `citra_qt/frame_mode.cpp` | where SteamVR shows the window in Flat (Game Mode only): `theater` (default; the big screen, through `steam-cef.py --screen`, which runs `vrcmd --dock-overlay`), `dashboard` (only inside the dashboard, as before) or `off` (Steam is not asked) |
| `AZAHARFRAME_XR_INSTANCE=keep` | `frame_xr/instance_policy.h` | one XrInstance for the whole run, as in 1.1 (by default each VR session gets a new one: SteamVR gives the controllers only to the first session of an instance) |
| `AZAHARFRAME_MODELS` | `frame_xr/vis/controller_models.cpp` | folder with `frame_controller_left.model` and `frame_controller_right.model` (default: `user/frame-models` in the data folder) |
| `AZAHARFRAME_MODEL_EYE_PX` | `frame_xr/vis/controller_models.cpp` | the most pixels one eye of the controller layer may be high, 200 or more (default 1000). Below it the layer is about half the recommended size |
| `AZAHARFRAME_MODEL_TEXTURE`, `AZAHARFRAME_MODEL_TRIANGLES` | `frame-controller-models.py` | the models' texture size (default 1024) and a triangle budget |
| `AZAHARFRAME_XR_NO_GPU_WAIT=1` | `vk_xr_target.cpp` | releases images before the GPU is done. A negative test, never for playing |
| `AZAHARFRAME_XR_WATCHDOG=1` | `frame_xr/watchdog.cpp` | logs a frame loop phase that lasts over 50 ms, with the kernel state, wait channel and system call of the frame loop's thread and of the emulator's busy threads. For finding display stalls |
| `AZAHARFRAME_DEBUG_STALL=<where>:<ms>[:<s>]` | `common/debug_stall.h` | sleeps `ms` every `s` seconds (default 5) at `tick` (emu thread after a frame, no lock), `submit` (inside the queue lock), `acquire` (XR frame drawn, not yet handed off) or `release` (the handoff thread, before a release). A test, never for playing |
| `AZAHARFRAME_VMA_BLOCK_MIB` | `vk_instance.cpp` | the size of VMA's memory blocks in MiB (default 32 on the XR device; `0`: VMA's own, 256), to compare |
| `AZAHARFRAME_TEST_RESOLUTIONS` | `citra_qt.cpp` | sets Internal Resolution at those seconds after start (`20:1,35:4`, seconds:value), as the settings would. For mock runs |
| `AZAHARFRAME_TEST_SHAPES` | `citra_qt.cpp` | sets Screen Shape at those seconds after start (`20:3,30:0`, seconds:value), as the settings would. For mock runs |
| `AZAHARFRAME_TEST_FLAT_PAD` | `citra_qt/frame_mode.cpp`, `frame_xr/controls.cpp` | a file of gamepad events (`<seconds since start> <control> <value>`; controls a b x y menu view lb rb ls rs up down left right lt rt lx ly rx ry) read instead of SDL's gamepad in flat, and a log line ("xr test: flat: the game sees ...") for each change of what the game is sent. For mock runs |
| `AZAHARFRAME_TEST_BOOTS`, `AZAHARFRAME_TEST_SECONDS` | `citra_qt.cpp` | boot the listed games (`a.3ds\|b.cci`) in turn, each for that many seconds. For the headless runs |

For scripts: `AZAHARFRAME_APP`, `AZAHARFRAME_HOME`, `AZAHARFRAME_SKIP_STEAM`, `AZAHARFRAME_DRY_RUN` (`install.sh`, `uninstall.sh`), `AZAHARFRAME_BUILD` (`install.sh`, `make-release.sh`), `AZAHARFRAME_BUILD_DIR`, `AZAHARFRAME_JOBS`, `AZAHARFRAME_NO_WAIT`, `AZAHARFRAME_NO_BUILD` (`build.sh`), `AZAHARFRAME_VRCMD` and `AZAHARFRAME_VRSERVER_RUNNING` (`dock-icon.py`).

## Frametanium

[Frametanium](https://github.com/Cogential/frametanium) installs a port from a GitHub release. For an "installer" entry it downloads the asset whose name matches, unpacks it into `~/.local/share/frametanium/apps/<id>`, and runs the `install.sh` at the top of the archive from that folder with no input and a 15 minute limit. On an update the old folder is kept as `.previous`, the new one is unpacked beside it, and `install.sh` runs again; if it fails the old folder comes back. On removal it runs `uninstall.sh` from the same folder, then deletes the folder. It refuses to update or remove while the program in that folder runs, and `install.sh` and `uninstall.sh` also refuse while `azahar` runs.

The tarball fits that: the files are at its top level, the name has `linux-arm64` and ends in `.tar.gz`, `SHA256SUMS` is not mistaken for a download, `install.sh` recognises an unpacked release by `azahar` and `azaharframe.sh` next to it and installs it in place of a tarball, `uninstall.sh` finds `dock-icon.py` and `steam-cef.py` next to itself, and the data folder is outside everything Frametanium replaces. Frametanium's `--purge` is not reachable; its remove keeps `~/.local/share/AzaharFrame`. The program folder is kept twice on disk (the unpacked release in Frametanium's folder, and the installed copy), plus `.previous`.

What the owner adds so that Frametanium offers it:

1. **On the Frame**, `~/.config/frametanium/catalog.json` takes one more entry, after the other ports:

   ```json
   {"id": "azaharframe", "name": "Azahar for Frame (3DS VR)", "repo": "simplysanatori/AzaharFrame",
    "asset": "azaharframe-*-linux-arm64.tar.gz", "auto_asset": true, "mode": "installer",
    "prereleases": false, "gameid": "AzaharFrame",
    "description": "My port: Nintendo 3DS games in VR, Azahar with CitraVR's VR mode, native on the Frame."}
   ```

   or, from the Frametanium page or headset app, add `simplysanatori/AzaharFrame` with mode "installer". The release must be published, not a draft. Until the repository is public Frametanium needs a GitHub token (read access to releases is enough) in its `config.json`, `github_token`.
2. **Upstream**, `suggestions.json` in Cogential/frametanium takes `{"repo": "simplysanatori/AzaharFrame", "name": "Azahar for Frame (Nintendo 3DS VR)", "note": "Azahar with CitraVR's VR mode, native; needs your own 3DS games"}`, as in the open pull request for the other ports.
3. **The release** carries `azaharframe-<version>-linux-arm64.tar.gz` (and may carry `SHA256SUMS`). The FrameDrop zip ends in `.zip`, so the asset pattern in the entry does not match it.

After the first install through Frametanium the library entry is the same one `install.sh` makes by hand ("Azahar for Frame"), so installing both ways does not add a second entry.

**No shader warm-up script.** GalaxyFrame ships `warm-shaders.sh` because its shaders can be compiled ahead of play. Azahar's disk cache is filled only by drawing a game on the emulator, so there is nothing to run before the first launch.

## FrameDrop

[FrameDrop](https://framedropvr.com) installs from a manifest. `make-release.sh` also writes `azaharframe-<version>-framedrop-linux-arm64.zip` and `azaharframe.framedrop.json` with `release/make-framedrop-zip.py`: the zip holds the tarball's files plus `run.sh` (`steam-frame/framedrop-run.sh`) at the top, and the manifest (`framedrop.install/v1`) gives the zip's release URL for the tag and its sha256. Attach all three to the release. The install link is `https://framedropvr.com/install?manifest=` followed by the URL-encoded `https://github.com/simplysanatori/AzaharFrame/releases/latest/download/azaharframe.framedrop.json`.

FrameDrop adds its own library entry for `run.sh`. On the first start, and whenever the zip's `VERSION` differs from the one it installed last (kept in `~/.local/state/framedrop-azaharframe/installed-version`), `run.sh` runs `install.sh` with `AZAHARFRAME_SKIP_STEAM=1`, so no second entry is made, and logs to `install.log` in the same folder. Then it starts the installed `azaharframe.sh`. It also gives back the executable bits a zip unpack can lose. If the install fails it starts the version installed before. `FRAMEDROP_RUN_DRY=1` installs if needed and prints what it would start.

## Before publishing a release

Run the checks in [PRIVACY_AUDIT.md](PRIVACY_AUDIT.md) on the tracked tree, the history that is pushed, and the tarball.
