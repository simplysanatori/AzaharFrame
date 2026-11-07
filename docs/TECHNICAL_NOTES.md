# AzaharFrame: technical notes

Azahar for Frame is a fork of [CitraVR](https://github.com/amwatson/CitraVR) that runs on the Valve Steam Frame. CitraVR is the Meta Quest build of [Azahar](https://github.com/azahar-emu/azahar). This document is for the person who maintains the fork: what is changed in the emulator's files, how to merge new versions, how to build and package. How the port works is in [TECHNICAL.md](TECHNICAL.md).

## Where the code is

New code, in files of its own:

| Path | What |
|---|---|
| `src/frame_xr/` | the OpenXR session, the frame loop, input (`input/`), the game picker (`picker/`, `ui/`), immersive mode |
| `src/video_core/vulkan_hooks.h` | the instance and device hooks, and the screen sink |
| `src/video_core/vr_immersive.h` | the head pose snapshot for immersive mode |
| `src/video_core/renderer_vulkan/vk_xr_target.*` | the three OpenXR swapchains and the release thread |
| `src/video_core/renderer_vulkan/renderer_vulkan_xr.cpp` | `RenderToXr()` |
| `src/input_common/frame_xr.*` | the Frame controller input engine |
| `src/citra_qt/configuration/configure_frame_vr.*`, `src/citra_qt/frame_branding.h` | the **VR** group in the Graphics tab, the Azahar for Frame icon |
| `src/tests/frame_xr/` | unit tests for the input mapper, the screen geometry and the picker |
| `third_party/openxr/` | the OpenXR headers the code is built against (Apache-2.0 OR MIT) |
| `tools/xrmock/`, `tools/mockrun.sh` | a mock OpenXR runtime for running without a headset |
| `steam-frame/` | installer, launcher, dock icon, build and release scripts, the library art |
| `docs/` | these documents |

## Upstream files changed

The base is Azahar's `2127.0-alpha4` tag. Every change this fork made in a file that exists upstream is marked `AzaharFrame` on or next to the line (80 marks), so that a merge conflict is easy to resolve and the list below is easy to check:

```sh
git grep -n "AzaharFrame" -- src CMakeLists.txt externals ':!src/frame_xr' ':!src/tests'
git diff --stat 2127.0-alpha4...HEAD -- src CMakeLists.txt externals ':!src/android'
```

Files with `AzaharFrame` marks (53 files differ from the tag in `src`, `CMakeLists.txt` and `externals`; these are the ones this fork changed):

| File | Change |
|---|---|
| `CMakeLists.txt`, `src/CMakeLists.txt` | `ENABLE_FRAME_XR` (on by default on Linux aarch64) and its compile definition, the `frame_xr` subdirectory |
| `src/citra_qt/CMakeLists.txt`, `src/video_core/CMakeLists.txt`, `src/input_common/CMakeLists.txt`, `src/tests/CMakeLists.txt` | the new source files and tests |
| `src/citra_qt/citra_qt.cpp` | `--vr` option, starts and stops `frame_xr`, hands it the boot and shutdown calls, the touch sink, the game folders for the picker, the Frame controls profile, the 3D depth default, the name and icon |
| `src/citra_qt/aboutdialog.cpp` | the Azahar for Frame icon |
| `src/citra_qt/configuration/config.{h,cpp}` | reads and saves the `[VR]` section (CitraVR's keys) |
| `src/citra_qt/configuration/configure_graphics.{h,cpp}` | the **VR** group |
| `src/common/settings.{h,cpp}` | the `vr_*` immersive settings, `vr_hide_bottom_screen`, `vr_top_aspect` and `vr_immersive_game_fov` as per-game settings |
| `src/input_common/main.cpp` | registers the `frame_xr` engine, its pollers and texts; resets the GC adapter before static destruction |
| `src/input_common/gcadapter/gc_adapter.{h,cpp}` | a failed libusb init no longer crashes at exit |
| `src/video_core/gpu.cpp` | applies the newest head pose after each frame |
| `src/video_core/renderer_base.cpp` | two resolution steps more in immersive mode |
| `src/video_core/rasterizer_accelerated.cpp` | uploads the PICA uniforms again when the head pose changes |
| `src/video_core/renderer_vulkan/vk_platform.cpp`, `vk_instance.cpp` | the instance and device hooks; a device it did not create is not destroyed |
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

<!-- VERIFY: the cherry-pick procedure on the first real CitraVR update -->

## Building

All on the Frame, with the tools SteamOS has: a C++ compiler (clang or gcc), CMake 3.24 or newer, Ninja, Qt 6 and SDL2 development files, Python 3, and SteamVR installed for running it.

```sh
steam-frame/build.sh 1.0.0      # configures build/, builds, packages steam-frame/release/azaharframe-1.0.0-linux-arm64.tar.gz and SHA256SUMS
steam-frame/install.sh          # installs that tarball and adds the Steam entry
```

The build uses Ninja, Release, `ENABLE_FFMPEG=OFF`, `USE_AUDIOTRACK=OFF`, Vulkan on and OpenGL off, tests, the room server, the web service, Discord and scripting off. `ENABLE_FRAME_XR` is on by default on Linux aarch64. It runs at the lowest priority and waits while the headset is in use (`~/.cache/headset-in-use`, or a game running under Steam), because a build next to a VR session ruins the session. `ccache` is used when it is installed (`~/.cache/azaharframe/ccache`). `CF_EXTRA_CMAKE_FLAGS` adds CMake flags, for example `-DENABLE_TESTS=ON` to build the unit tests in `src/tests/frame_xr`.

<!-- VERIFY: the build as written needs ENABLE_FRAME_XR and the OpenXR headers path; check CMake configures from a clean checkout -->

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

## Environment variables

Set these in `~/.local/share/AzaharFrame/session.env` (one `VAR=value` per line) when you start from the Steam library, which passes none.

| Variable | Read by | What |
|---|---|---|
| `AZAHARFRAME_FLAT=1` or `0` | `azaharframe.sh` | start without `--vr` (the desktop window only), or force VR, for one launch; otherwise the mode comes from the file `mode` in the data folder (`vr` or `flat`, VR when it is missing), and `--flat` / `--vr` on the command line beat both |
| `AZAHARFRAME_REFRESH_HZ` | `azaharframe.sh` | 90 or 120 (default 120), set in SteamVR for the entry |
| `AZAHARFRAME_IMMERSIVE_FOV`, `_DISTANCE`, `_LOCAL` | `frame_xr/immersive.cpp` | immersive mode tuning (the FOV one overrides the Immersive Game FOV setting), see [IMMERSIVE.md](../src/frame_xr/IMMERSIVE.md) |
| `AZAHARFRAME_OPENXR_LOADER` | `frame_xr/xr_loader.cpp` | path of an OpenXR loader to use before SteamVR's |
| `AZAHARFRAME_ART` | picker, Qt icon | folder with `logo.png` and `icon.png` (default: `art/` next to the program) |
| `XR_RUNTIME_JSON` | the OpenXR loader | the runtime; the launcher sets SteamVR's when nothing else names one |
| `QT_SCALE_FACTOR` | Qt | scales the settings window |
| `AZAHARFRAME_XR_NO_GPU_WAIT=1` | `vk_xr_target.cpp` | releases images before the GPU is done. A negative test, never for playing |
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
3. **The release** carries `azaharframe-<version>-linux-arm64.tar.gz` (and may carry `SHA256SUMS`). Nothing else matching `*arm64*` should be attached, or the asset pattern in the entry has to be exact.

After the first install through Frametanium the library entry is the same one `install.sh` makes by hand ("Azahar for Frame"), so installing both ways does not add a second entry.

**No shader warm-up script.** GalaxyFrame ships `warm-shaders.sh` because its shaders can be compiled ahead of play. Azahar's disk cache is filled only by drawing a game on the emulator, so there is nothing to run before the first launch.

## Before publishing a release

Run the checks in [PRIVACY_AUDIT.md](PRIVACY_AUDIT.md) on the tracked tree, the history that is pushed, and the tarball.
