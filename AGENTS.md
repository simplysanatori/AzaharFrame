# AGENTS.md

This file holds the technical detail moved out of the README: how the port is built, packaged and installed, and where the developer documents are.

## Where things are

| What | File |
|---|---|
| How the port works (one process, Vulkan device, screens into SteamVR, threads, controls, controller models and VR panels, immersive mode, launcher) | [docs/TECHNICAL.md](docs/TECHNICAL.md) |
| Files changed upstream, merging new versions, building, packaging, installer, Frametanium, environment variables, mocks | [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md) |
| Controller mapping, developer version | [src/frame_xr/input/MAPPING.md](src/frame_xr/input/MAPPING.md) |
| Immersive mode, tuning variables | [src/frame_xr/IMMERSIVE.md](src/frame_xr/IMMERSIVE.md) |
| What the logs hold, release checks | [docs/PRIVACY_AUDIT.md](docs/PRIVACY_AUDIT.md) |
| Player documentation | [docs/USER_GUIDE.md](docs/USER_GUIDE.md), [docs/CONTROLS.md](docs/CONTROLS.md) |
| Changes | [docs/CHANGELOG.md](docs/CHANGELOG.md), [docs/RELEASE_NOTES_v1.0.0.md](docs/RELEASE_NOTES_v1.0.0.md) |
| Third-party licences and art sources | [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), [steam-frame/art/CREDITS.md](steam-frame/art/CREDITS.md) |

[README.upstream.md](README.upstream.md) is CitraVR's README, kept as it was when this fork was made. Do not edit it.

## Branch and layout

The branch is `steam-frame`. A directory named `steam-frame/` also exists, so put `--` after revisions in git commands (`git log steam-frame --`). The new code is in `src/frame_xr/`, with the Vulkan hooks in `src/video_core/` and the Frame input engine in `src/input_common/frame_xr.*`. The installer, launcher, build and release scripts are in `steam-frame/`. Changes to upstream files are marked `AzaharFrame`; the list and the merge procedure are in [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md).

## Build

On the Frame, with a C++ compiler (clang or gcc), CMake 3.24 or newer, Ninja, the Qt 6 and SDL2 development files and Python 3:

```sh
steam-frame/build.sh 1.0.0
steam-frame/install.sh
```

The first build takes about an hour and loads every core. It waits while the headset is in use (`~/.cache/headset-in-use`, or a game running under Steam) and runs at the lowest priority. `build.sh` packs `steam-frame/release/azaharframe-1.0.0-linux-arm64.tar.gz` and `SHA256SUMS`, and `install.sh` installs that tarball. The options (`AZAHARFRAME_BUILD_DIR`, `AZAHARFRAME_JOBS`, `AZAHARFRAME_NO_WAIT`, `AZAHARFRAME_NO_BUILD`, `CF_EXTRA_CMAKE_FLAGS`) are in `steam-frame/build.sh` and [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#building).

`steam-frame/release/make-release.sh [version [build-dir]]` packs an existing build. The tarball holds the stripped emulator, the launcher, `install.sh`, `uninstall.sh`, the Steam and dock helpers, the art, the texture pack converter, the controller model converter, `framedrop-run.sh` and `make-framedrop-zip.py`, plus `LICENSE.txt` and `THIRD_PARTY_NOTICES.md`, and in a portable build `lib/`, `plugins/` and `qt.conf`. It does not ship the README, so the README's links do not need shipping.

## Release assets

`make-release.sh` writes three files to attach, and `SHA256SUMS` for all three:

- `azaharframe-<version>-linux-arm64.tar.gz`: for `install.sh` and Frametanium.
- `azaharframe-<version>-framedrop-linux-arm64.zip`: the same files plus `run.sh` (`steam-frame/framedrop-run.sh`), for FrameDrop.
- `azaharframe.framedrop.json`: FrameDrop's manifest, the zip's release URL and sha256.

The release is made with `steam-frame/build-portable.sh <version>` (below), so it runs on SteamOS Stable, Beta and Preview. FrameDrop's side is in [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#framedrop).

## Portable build

A SteamOS update can change or drop what a program built on the Frame links to (Qt 6.8 to 6.10, SDL2 to sdl2-compat, ICU, FFmpeg 8, a glibc bump). The release therefore carries its own Qt and is built to run on the oldest glibc any SteamOS has had. The same approach as Moonlight XR for Frame.

```sh
steam-frame/build-portable.sh 1.0.0      # container build, bundle, tarball, check
steam-frame/tools/check-bundle.sh DIR    # check any unpacked release
```

- **Build environment**: `steam-frame/portable/Containerfile` (Ubuntu 24.04 pinned by digest, GCC 13, glibc 2.39; Qt 6.10.3 from aqtinstall, pinned by version, with the hash of the installed tree printed and checkable through `QT_TREE_SHA256`). `build-portable.sh` builds the image once, then runs `portable/container-build.sh` in it (source copied to a work volume, build tree and ccache in `~/.cache/azaharframe/portable`), then `portable/stage-bundle.sh` collects the Qt libraries, ICU and plugins, and `release/make-release.sh` packs them (`release/portable.sh` holds the portable part). SDL2 is the static copy in `externals/sdl2` (`USE_SYSTEM_SDL2=OFF`), so the program needs no SDL library. The program links with RUNPATH `$ORIGIN/lib`; `qt.conf` makes the app folder Qt's prefix, so Qt finds `plugins/` there and never in the system. `azaharframe.sh` already unsets `LD_LIBRARY_PATH`, `QT_PLUGIN_PATH` and `QT_QPA_PLATFORMTHEME`, so nothing else is needed there.
- **Shared machine**: the script waits for `~/.cache/headset-in-use` and the bench lock, uses at most 4 jobs, and stops the container when the headset is used (run it again to resume). In CI set `AZAHARFRAME_NO_LOCK=1` (`.github/workflows/portable.yml`).
- **What comes from SteamOS** (`steam-frame/tools/host-libs.allow`, each named in every `steam-frame/tools/host-sonames/*.txt`): glibc; libstdc++, libgcc_s and libatomic (the compiler runtime; built with GCC 13 so every symbol exists in SteamOS's GCC 15); the Mesa/glvnd stack (libGL, libEGL, libGLX, libOpenGL, libGLdispatch) and the Vulkan loader, which must match the GPU driver; Wayland, X11 and xcb client libraries and xkbcommon, which Qt's platform plugins talk to the display server with; libSM and libICE (X session management, which Qt's xcb plugin links); D-Bus and GLib; the PulseAudio client library (Qt Multimedia links it; PipeWire serves it); fontconfig and FreeType; and a few long-stable libraries (zlib, zstd, expat, libpng, HarfBuzz, brotli). Everything else is bundled.
- **The limits** `check-bundle.sh` enforces: glibc 2.39, GLIBCXX 3.4.34, CXXABI 1.3.15, GCC 15.0.0, no Qt symbol version newer than the bundled Qt, no RPATH, relative RUNPATH only, every needed library in `lib/` or on the allowlist and present on every known SteamOS build, `qt.conf` correct, and every library resolved (with `LD_LIBRARY_PATH` unset) from the folder or the allowlist. `make-release.sh` unpacks the tarball, runs it, then starts `azahar --version` headless (`QT_QPA_PLATFORM=offscreen`) and checks that Qt came from the folder. The release fails on any problem; `AZAHARFRAME_REQUIRE_PORTABLE=1` (set by `build-portable.sh`) also fails a build with no bundle, which otherwise packs a non-portable tarball with a warning.
- **When SteamOS updates**: add its library list to `steam-frame/tools/host-sonames/` (`ls /usr/lib | grep -E '^(lib.*\.so\.[0-9]+|ld-linux-aarch64\.so\.1)$' | sort`, with the header line like the others). The check then covers it. If an allowlisted library disappears from a build, the check fails and the library must be bundled instead.
- **Opened at run time, so not seen by the check; all stay on the system**: the OpenXR loader (`/opt/steamvr/bin/linuxarm64/libopenxr_loader.so`, SteamVR's), the Vulkan loader and driver (`libvulkan.so.1`), the audio backends of cubeb, OpenAL and SDL (PulseAudio, PipeWire, ALSA, JACK, sndio, PortAudio, GameMode), SDL's `libudev`, X11 and Wayland, Qt's TLS plugin (`libssl.so.3`, unused: the web service is off). Qt Multimedia has no backend in the bundle (Qt's FFmpeg one needs FFmpeg 7 libraries, and FFmpeg 8 is on the SteamOS Preview), so the 3DS camera lists no cameras. The scripts need `python3` (standard library; `python3-gobject` with GdkPixbuf only for `tools/frame-controller-models.py`, the beams still show without it), `curl` (`uninstall.sh` only), `vrcmd` (SteamVR), and `git`, `cmake` and a C++ compiler for the texture pack converter's encoder on first use.
- **Qt translations and themes**: only the portal platform theme is bundled (no KDE or GTK theme: those are the system's Qt and GTK build and cannot load into another Qt), so the flat window uses Qt's own look.

## Installer

`steam-frame/install.sh [--dry-run] [azaharframe-*-linux-arm64.tar.gz]` copies the program to `~/Games/AzaharFrame/app`, adds or updates the library entry through Steam's debugging port (`steam-cef.py`), names the SteamVR dock entry (`dock-icon.py`) and sets the app to 120 Hz. With no argument it installs the folder it is in when that is an unpacked release (how Frametanium runs it), else the newest tarball in `steam-frame/release/`, else the build in `build/bin/Release/`. It never asks a question. When Steam cannot be reached it installs the files, says the entry was skipped and exits 0. It refuses to run while `azahar` runs.

`steam-frame/uninstall.sh [--purge]` removes the library entry, the program folder and the dock icon files. `--purge` also deletes the data folder. Both scripts read `AZAHARFRAME_APP`, `AZAHARFRAME_HOME` and `AZAHARFRAME_SKIP_STEAM` for trial installs in a scratch folder (see [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#install-and-uninstall)).

Frametanium: the release must carry `azaharframe-<version>-linux-arm64.tar.gz`. The catalog id is `azaharframe`, mode "installer". The entry and the upstream suggestion are in [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#frametanium).

FrameDrop: `run.sh` at the top of the zip installs with `AZAHARFRAME_SKIP_STEAM=1` on the first start and after each version change, then starts `azaharframe.sh`. FrameDrop's own library entry is the only one. See [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#framedrop).

## Quick Save and Load, passthrough, presets

- **View, then a bumper** is in the mapper (`src/frame_xr/input/mapper.{h,cpp}`): View pressed with no bumper down is held back `chord_grace_ns` (150 ms, a tap is sent late as a pulse); while it is down the Right Bumper edge sets `commands.quick_save`, the Left `quick_load` (one per press, `quick_cooldown_ns` 1 s), and bumper and View stay away from the game until let go (`swallow_bumper_`, `swallow_view_`, which also restarts the stop hold). `VrUi::RequestQuick` (`controls.cpp`, game and flat) calls `FrontendHooks::quick_save` / `quick_load`; citra_qt queues them to the GUI thread, where `g_frame_xr_quick_save` / `_load` (`citra_qt.cpp`) trigger Azahar's own `action_Quick_Save` / `action_Quick_Load`. Quick Save is confirmed when slot 0's file shows a new time; Quick Load says "No Quick Save" when `actions_load_state[0]` is disabled. The message is a toast (`FrameXr::ShowToast`) in VR with the menu closed, else the status bar's message label for 3 s (`statusBar()->showMessage` has no room: every widget there is permanent). The first-time savestate warning is a dialog, so in VR the in-game menu is opened before it. Tests: `src/tests/frame_xr/input_mapper.cpp`.
- **View, then the Right Trigger** is fast forward, in the same mapper block style (`toggle_fast_forward`, `quick_cooldown_ns`, `swallow_trigger_r_`); `VrUi::RequestFastForward` calls `FrontendHooks::toggle_fast_forward`, and `g_frame_xr_fast_forward` (`citra_qt.cpp`) owns the state: `SetTurboEnabled` and `FrameXr::SetFastForwardShown` (the "Fast Forward" and "Normal Speed" toasts, rows of the toast atlas in `hud.cpp`, 2 s each). Off when the game stops, the in-game menu opens (flat: Menu held pauses) or the mode switches.
- **The VR / flat chord fires once per press**: `ChordLatch` (`src/frame_xr/input/chord_latch.h`), armed in `VrUi::RequestToggleVr`, `Create` and `DestroyXr`, hides both bumpers, View and Menu until all four are up at least 1 s into the new mode. Tests: `src/tests/frame_xr/input_mapper.cpp`.
- **The window in Flat** goes to SteamVR's theater (`steam-cef.py --screen theater`, `vrcmd --dock-overlay`), and back into the dashboard before VR again (`FrameMode::LeaveFlat` delays `SetVr(true)` until then); see the comment in `citra_qt/frame_mode.h` and `AZAHARFRAME_FLAT_SCREEN`.
- **The row in the in-game menu** is a `QToolBar` that `FrameMirror` adds under the menu bar (`frame_mirror.cpp`), shown only while `vr_showing && game_running && menu_open`. `FrameMirror::AddVrMenuButton(label, callback)` adds a button to its right and returns it (the controls help and stats use it). `citra_qt.cpp` adds Quick Save, Quick Load and Passthrough.
- **Background** (`Settings::values.vr_background`, global only, `[VR]` key `vr_background`: 0 Black, 1 Passthrough) is read every frame by `Hud` (`frame_xr/hud.{h,cpp}`). The runtime shows the room only when the frame ends with `XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND` and has a projection layer, so `Hud` adds a 16x16 transparent one first, in the same swapchain as the toasts (SteamVR allows 16 swapchains per session; this tree uses 14 once the controller models show). `Runtime::alpha_blend_supported` comes from `xrEnumerateEnvironmentBlendModes` at start and is logged ("environment blend modes"). `tools/xrmock` offers alpha blending with `XRMOCK_ALPHA_BLEND=1`.
- **Presets** (`frame_xr/presets.{h,cpp}`): title ids with an Immersive Mode and View Matrix Register, applied in `BootGame` after the per-game config loaded, only if `UsingGlobal()` and the global value is still the default.
- Rendering notes (SPD13's SteamVR findings, checked against this tree): quad layers may be depth-sorted, so nothing may share a plane (the dim quad of the in-game menu is 2 cm behind the window); swapchains over 1000 px high may get Valve's foveation (the screens are 960 px at 4x and 1440 at 6x); the lower screen is not filtered down.

## Immersive screen placement

`src/frame_xr/immersive_layout.{h,cpp}` holds the maths, the grip gesture and the per-game file (`immersive-placement.txt` in the user directory); `immersive.cpp` applies it; [src/frame_xr/IMMERSIVE.md](src/frame_xr/IMMERSIVE.md#placement) explains it. The rule behind it: in modes 2 and 3 the world only stays put when a head turn moves the picture by the angle the game camera turned, so nothing may change the screen's angle against the game's view without paying for it.

- Distance scales the quad, so the angle is kept.
- Pitch turns the quad about the eye and is multiplied into the camera rotation given to the game (`GameCameraRotation`), so the camera looks where the screen is. Both view-matrix hooks (mode 2 replaces the register's rotation, mode 3 multiplies the game's matrix) take it as a plain rotation.
- Size scales the quad against the game's view, and the head's yaw and pitch get a gain of game angle / screen angle (`ScaleYawPitch`, the roll is kept), which keeps the middle of the picture locked. The game's view never widens with it by default: past its own view a game's culling and scenery run out (black boxes in Super Mario 3D Land). The cost, documented for players as "the world stays where it is": big turns cover more or less of the game world. Gain 0.4 to 2.0, size 0.6 to 2.5.
- Per game (`Options`): **Wider View** (`vr_immersive_wider_view`) lets the periphery factor follow the size between 1.0 and 1.4 first, as earlier builds did (no preset turns it on); **Squeeze Width** (`vr_immersive_squeeze`, 10 or 20 %) narrows the quad at the same height and raises the yaw gain to gain / (1 - squeeze).
- The default puts the quad's sides at +-37 degrees (`kComfortHalfWidthDeg`, the HUD stays out of the lens edge) and the whole quad inside the view both eyes see, from the runtime's per-eye FOV, 5 degrees below its middle, never larger than the game's own angle. The tunables are the `k*` constants at the top of `immersive_layout.h`.
- The file's `view` lines are sizes against that default; `head` lines come from earlier builds (sizes against the old full-view default) and are converted once the view is known (`SizeFromOldDefault`, the screen keeps its angle), then saved as `view`.
- Screens are submitted farther first (`ScreenLayers::bottom_first`): quad layers have no depth test. A hidden bottom screen is never submitted (`Layout::Decide`).
- `Immersive::SetBottomFocused` pauses immersive mode while the bottom screen is the screen in focus.

Tests: `src/tests/frame_xr/immersive_layout.cpp`.

## Known issues

- The 3DS camera lists no cameras: Qt Multimedia has no backend in the bundle (see Portable build).
- Mario Kart 7's menus show white panels in mode 3 (Full (6DoF): Other Games, the preset for it).
- gamescope asserts if the volume key is pressed while no window holds keyboard focus (a SteamOS bug). frame_mirror.cpp keeps every VR-time top-level off the gamescope screen so the main window always holds focus; do not unmap the main window in VR.
- In Flat the game reads the Flat Gamepad profile through SDL directly, so it also sees the buttons of the View and Menu holds and chords (Select, Start, L, R, ZR). The mapper sees the same gamepad only for the holds and chords.

## The VR panels and controller models

`src/frame_xr/vis/` holds them; [docs/TECHNICAL.md](docs/TECHNICAL.md#controllers-beams-the-controls-help-and-the-stats) explains the design. Keep its rules: no CPU waits on the XR thread, the queue lock only around submits and swapchain calls, at most one image per swapchain in flight, and an image released only after its fence has signalled. The controller pass is the one exception to "no GPU work on the XR thread in a game". It adds three swapchains; SteamVR allows 16 per session, so share the atlas before adding a new one. The help text comes from `help_content.cpp` (unit-tested in `src/tests/frame_xr/vis.cpp`): a new control or chord goes in both `BuildHelp` (the list) and `BuildHelpDiagram` (the diagram's labels), as well as in [docs/CONTROLS.md](docs/CONTROLS.md). The layout test in `vis.cpp` fails if a new label no longer fits.

## Environment variables and test settings

Settings for one launch go in `~/.local/share/AzaharFrame/session.env` (one `VAR=value` per line), because the Steam library passes no environment. The full list is in [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#environment-variables). The ones a player may meet are `AZAHARFRAME_FLAT=1` (start in Flat once, `0` forces VR), `AZAHARFRAME_REFRESH_HZ` (90 or 120), `QT_SCALE_FACTOR=1.25` and `AZAHARFRAME_VR_PICKER=1` (the old VR game picker instead of Azahar's window in VR).

## Without a headset

`tools/xrmock/` is a mock OpenXR runtime and `tools/mockrun.sh` runs the emulator against it, and `src/tests/frame_xr/` holds the unit tests. The mock has no SteamVR render models: run `python3 -I steam-frame/tools/frame-controller-models.py DIR` on the Frame once and point `AZAHARFRAME_MODELS` at `DIR` to see the controllers in a mock.

## How this was made

I started this project and used AI (Claude Opus 5.5) to speed up development. I write, review and fix code by hand all the way through, keep the AI's work checked and reined in, and aim to keep the port as lightweight as possible. I test it on my own Steam Frame.
