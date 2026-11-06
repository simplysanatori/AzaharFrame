# CitraFrame: technical notes

CitraFrame is a fork of [CitraVR](https://github.com/amwatson/CitraVR) that runs on the Valve Steam Frame. CitraVR is the Meta Quest build of [Azahar](https://github.com/azahar-emu/azahar). This document is for the person who maintains the fork: what is changed in the emulator's files, how to merge new versions, how to build and package. How the port works is in [TECHNICAL.md](TECHNICAL.md).

## Where the code is

New code, in files of its own:

| Path | What |
|---|---|
| `src/frame_xr/` | the OpenXR session, the frame loop, input, the game picker, immersive mode |
| `src/video_core/vulkan_hooks.h` | the instance and device hooks |
| `src/video_core/renderer_vulkan/vk_xr_target.*` | the three OpenXR swapchains |
| `src/video_core/renderer_vulkan/renderer_vulkan_xr.cpp` | `RenderToXr()` |
| `src/input_common/frame_xr.*` | the Frame controller input engine |
| `third_party/openxr/` | the OpenXR headers the code is built against (Apache-2.0) |
| `tools/xrmock/`, `tools/mockrun.sh` | a mock OpenXR runtime for running without a headset |
| `steam-frame/` | installer, launcher, build and release scripts |
| `docs/` | these documents |

<!-- VERIFY: every path above exists after the XR lanes land -->

## Upstream files changed

Every change in a file that exists upstream is marked `// CitraFrame` on or next to the line, so that a merge conflict is easy to resolve and the list below is easy to check:

```sh
git grep -n "// CitraFrame" -- src CMakeLists.txt externals
```

| File | Change |
|---|---|
| `src/video_core/renderer_vulkan/vk_platform.cpp` | calls the `create_instance` hook when one is set |
| `src/video_core/renderer_vulkan/vk_instance.cpp` | calls the `pick_physical_device` and `create_device` hooks; does not destroy a device it did not create |
| `src/video_core/renderer_vulkan/renderer_vulkan.h` | `RenderToXr()` and the XR target member |
| `src/video_core/renderer_vulkan/renderer_vulkan.cpp` | calls `RenderToXr()` from `SwapBuffers()` |
| `src/input_common/main.cpp` | registers the `frame_xr` engine |
| `src/citra_qt/citra_qt.cpp` | `--vr` option, starts `frame_xr`, hands it the boot and shutdown calls, the Enter VR action |
| `src/video_core/CMakeLists.txt`, `src/input_common/CMakeLists.txt`, `src/citra_qt/CMakeLists.txt`, `CMakeLists.txt` | the new source files, `ENABLE_FRAME_XR` (on by default on Linux aarch64) |
| `src/frame_xr/` calls into `rasterizer_accelerated.*` | the head pose snapshot for immersive mode (`SetVRData`) |

<!-- VERIFY: regenerate this table from `git grep "// CitraFrame"` after the last XR lane lands. Also check the binary is still named azahar. -->

The CitraVR changes that came with the fork and that CitraFrame keeps unchanged are in these files (found with `git grep -il "vr_"`): `src/common/settings.h` (the `vr_*` settings and `factor_3d`), `src/common/common_paths.h` (the log file name), `src/common/stall_telemetry.h`, `src/core/hle/kernel/shared_page.cpp` and `src/core/hle/service/hid/hid.cpp` (the forced 3D slider), `src/video_core/rasterizer_accelerated.{h,cpp}` and `rasterizer_interface.h` (`SetVRData`), `src/video_core/renderer_vulkan/vk_rasterizer.cpp`, `src/video_core/renderer_opengl/gl_rasterizer.cpp`, `src/video_core/shader/generator/glsl_shader_gen.cpp` and `shader_uniforms.h` (the immersive factor). The Android app under `src/android/` is not built.

## Merging new versions

The remotes are `azahar` (azahar-emu/azahar), `citravr` (amwatson/CitraVR) and `origin` (this fork).

**Where it stands.** The branch `steam-frame` last took Azahar 2127.0-alpha4 in the merge commit `c7873cd44` ("Merge Azahar 2127.0-alpha4 into the Steam Frame branch"). Its second parent is Azahar's `e3c916e41`, the commit the `2127.0-alpha4` tag points at. CitraVR master as of that merge is `f71bf657d`.

**The content-level merge base.** CitraVR carries Azahar's code, but under rewritten commit ids: its history was rebuilt, so `git merge-base citravr/master azahar/master` finds an old commit (`3a4ebb141`) and a plain merge of either side shows a large false difference. That is why the two are never merged the same way:

1. **A new Azahar release (tag).** Merge the tag. Azahar's history is intact in this repository, so the merge base is the previous tag and only real conflicts appear, in the files marked `// CitraFrame` and in the CitraVR files above.

   ```sh
   git fetch azahar --tags
   git switch -c merge/azahar-<tag> steam-frame
   git merge <tag>
   git grep -n "// CitraFrame" -- src      # every marker must survive
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
steam-frame/build.sh 1.0.0      # configures build/, builds, packages steam-frame/release/citraframe-1.0.0-linux-arm64.tar.gz and SHA256SUMS
steam-frame/install.sh          # installs that tarball and adds the Steam entry
```

The build uses Ninja, Release, `ENABLE_FFMPEG=OFF`, `USE_AUDIOTRACK=OFF`, Vulkan on and OpenGL off, tests, the room server, the web service, Discord and scripting off. It runs at the lowest priority and waits while the headset is in use (`~/.cache/headset-in-use`), because a build next to a VR session ruins the session. `ccache` is used when it is installed. The build directory, the flags and the job count can be changed with the environment variables listed at the top of `steam-frame/build.sh`.

The release tarball holds `azahar` (stripped), `citraframe.sh`, `install.sh`, `uninstall.sh`, `steam-cef.py`, `VERSION`, `LICENSE.txt` and `THIRD_PARTY_NOTICES.md`. `steam-frame/release/make-release.sh [version [build-dir]]` packs an existing build without building. Tarballs and `SHA256SUMS` in `steam-frame/release/` are ignored by git.

<!-- VERIFY: the build as written needs ENABLE_FRAME_XR and the OpenXR headers path; check CMake configures from a clean checkout -->

**Trial install.** `install.sh` can install into a scratch folder without touching Steam or SteamVR:

```sh
CITRAFRAME_APP=/tmp/cf/app CITRAFRAME_HOME=/tmp/cf/home CITRAFRAME_SKIP_STEAM=1 steam-frame/install.sh TARBALL
```

**Frametanium.** The tarball carries `install.sh` and `uninstall.sh` at its top level, which is what Frametanium runs after unpacking. The archive name contains `arm64`. `install.sh` never asks a question and installs the files even when Steam is not running; run it again with Steam up to add the library entry.

**No shader warm-up script.** GalaxyFrame ships `warm-shaders.sh` because its shaders can be compiled ahead of play. Azahar's disk cache is filled only by drawing a game on the emulator, so there is nothing to run before the first launch.

Run the checks in [PRIVACY_AUDIT.md](PRIVACY_AUDIT.md) on a release before publishing it.
