# AGENTS.md

This file holds the technical detail moved out of the README: how the port is built, packaged and installed, and where the developer documents are.

## Where things are

| What | File |
|---|---|
| How the port works (one process, Vulkan device, screens into SteamVR, threads, controls, immersive mode, launcher) | [docs/TECHNICAL.md](docs/TECHNICAL.md) |
| Files changed upstream, merging new versions, building, packaging, installer, Frametanium, environment variables, mocks | [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md) |
| Controller mapping, developer version | [src/frame_xr/input/MAPPING.md](src/frame_xr/input/MAPPING.md) |
| Immersive mode, tuning variables | [src/frame_xr/IMMERSIVE.md](src/frame_xr/IMMERSIVE.md) |
| What the logs hold, release checks | [docs/PRIVACY_AUDIT.md](docs/PRIVACY_AUDIT.md) |
| Player documentation | [docs/USER_GUIDE.md](docs/USER_GUIDE.md), [docs/CONTROLS.md](docs/CONTROLS.md) |
| Changes | [docs/CHANGELOG.md](docs/CHANGELOG.md), draft [docs/RELEASE_NOTES_v1.0.0.md](docs/RELEASE_NOTES_v1.0.0.md) |
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

`steam-frame/release/make-release.sh [version [build-dir]]` packs an existing build. The tarball holds the stripped emulator, the launcher, `install.sh`, `uninstall.sh`, the Steam and dock helpers, the art and the texture pack converter, plus `LICENSE.txt` and `THIRD_PARTY_NOTICES.md`. It does not ship the README, so the README's links do not need shipping.

## Installer

`steam-frame/install.sh [--dry-run] [azaharframe-*-linux-arm64.tar.gz]` copies the program to `~/Games/AzaharFrame/app`, adds or updates the library entry through Steam's debugging port (`steam-cef.py`), names the SteamVR dock entry (`dock-icon.py`) and sets the app to 120 Hz. With no argument it installs the folder it is in when that is an unpacked release (how Frametanium runs it), else the newest tarball in `steam-frame/release/`, else the build in `build/bin/Release/`. It never asks a question. When Steam cannot be reached it installs the files, says the entry was skipped and exits 0. It refuses to run while `azahar` runs.

`steam-frame/uninstall.sh [--purge]` removes the library entry, the program folder and the dock icon files. `--purge` also deletes the data folder. Both scripts read `AZAHARFRAME_APP`, `AZAHARFRAME_HOME` and `AZAHARFRAME_SKIP_STEAM` for trial installs in a scratch folder (see [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#install-and-uninstall)).

Frametanium: the release must carry `azaharframe-<version>-linux-arm64.tar.gz`. The catalog id is `azaharframe`, mode "installer". The entry and the upstream suggestion are in [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#frametanium).

## Environment variables and test settings

Settings for one launch go in `~/.local/share/AzaharFrame/session.env` (one `VAR=value` per line), because the Steam library passes no environment. The full list is in [docs/TECHNICAL_NOTES.md](docs/TECHNICAL_NOTES.md#environment-variables). The ones a player may meet are `AZAHARFRAME_FLAT=1` (start in Flat once, `0` forces VR), `AZAHARFRAME_REFRESH_HZ` (90 or 120), `QT_SCALE_FACTOR=1.25` and `AZAHARFRAME_VR_PICKER=1` (the old VR game picker instead of Azahar's window in VR).

## Without a headset

`tools/xrmock/` is a mock OpenXR runtime and `tools/mockrun.sh` runs the emulator against it, and `src/tests/frame_xr/` holds the unit tests. Features that depend on a real headset session (motion controls direction, the keyboard on Azahar's window, the VR and Flat switch, the texture pack optimizer) are built and mock-tested only. Their VERIFY notes are in [docs/USER_GUIDE.md](docs/USER_GUIDE.md) and [docs/CONTROLS.md](docs/CONTROLS.md).

## How this was made

I started this project and used AI (Claude Opus 5.5) to speed up development. I write, review and fix code by hand all the way through, keep the AI's work checked and reined in, and aim to keep the port as lightweight as possible. I test it on my own Steam Frame.
