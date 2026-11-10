#!/bin/bash
# Builds Azahar for Frame on the Steam Frame (Linux aarch64) and packages it for the Releases
# page (the tarball for Frametanium, the FrameDrop zip and json, SHA256SUMS) into
# AZAHARFRAME_OUT (default ~/Games/AzaharFrame/release-out); see release/make-release.sh.
#   steam-frame/build.sh [version]
# Needs, from SteamOS's packages: clang or gcc, CMake 3.24 or newer, Ninja, Qt 6 and SDL2
# development files, Python 3.  ccache is used when it is installed.
# The build takes a long time the first time (about an hour on the Frame) and loads every
# core, so it waits while the headset is in use (~/.cache/headset-in-use, or a game running
# under Steam) and runs at the lowest priority.
# Environment:
#   AZAHARFRAME_BUILD_DIR=DIR   the CMake build directory (default <repo>/build)
#   AZAHARFRAME_JOBS=N          parallel jobs (default 4)
#   AZAHARFRAME_NO_WAIT=1       do not wait for the headset (do not use this while you play)
#   AZAHARFRAME_NO_BUILD=1      only package the existing build
#   CF_EXTRA_CMAKE_FLAGS=...   more CMake flags
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
VERSION=${1:-1.1.0}
BLD=${AZAHARFRAME_BUILD_DIR:-$REPO/build}
JOBS=${AZAHARFRAME_JOBS:-4}

if [ -z "$AZAHARFRAME_NO_BUILD" ]; then
  mkdir -p "$HOME/.cache/azaharframe/ccache" "$BLD"
  exec 9> "$HOME/.cache/azaharframe-build.lock"
  flock 9

  # A build running beside a VR session ruins it.
  if [ -z "$AZAHARFRAME_NO_WAIT" ]; then
    while [ -e "$HOME/.cache/headset-in-use" ] || pgrep -f SteamLaunch > /dev/null 2>&1; do
      echo "$(date +%T) headset in use, waiting 60 s..."
      sleep 60
    done
  fi

  export CCACHE_DIR=$HOME/.cache/azaharframe/ccache
  export CCACHE_MAXSIZE=10G
  LAUNCHER=()
  if command -v ccache > /dev/null; then
    LAUNCHER=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
  fi

  # "azahar -v" prints the release's version ("Azahar for Frame 1.1.0"), not the commit: GIT_TAG is what
  # CMakeModules/GenerateBuildInfo.cmake takes the version from.
  # The binary names its sources relative to the checkout, not with this machine's paths.
  PREFIX_MAP="-ffile-prefix-map=$REPO/= -ffile-prefix-map=$BLD/=build/"

  # Vulkan only (the Frame's SteamVR is Vulkan), no FFmpeg (no video recording), no Android audio.
  # The OpenXR code (ENABLE_FRAME_XR) is on by default on Linux aarch64.
  nice -n 19 cmake -S "$REPO" -B "$BLD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    "${LAUNCHER[@]}" \
    -DCMAKE_C_FLAGS="$PREFIX_MAP" -DCMAKE_CXX_FLAGS="$PREFIX_MAP" -DAZAHARFRAME_RELEASE=ON \
    -DGIT_TAG="$VERSION" \
    -DENABLE_FFMPEG=OFF -DUSE_AUDIOTRACK=OFF \
    -DENABLE_QT=ON -DENABLE_QT_TRANSLATION=ON -DENABLE_QT_UPDATE_CHECKER=OFF \
    -DENABLE_VULKAN=ON -DENABLE_OPENGL=OFF -DENABLE_SOFTWARE_RENDERER=ON \
    -DENABLE_TESTS=OFF -DENABLE_ROOM=OFF -DENABLE_ROOM_STANDALONE=OFF \
    -DENABLE_WEB_SERVICE=OFF -DENABLE_DISCORD_RPC=OFF -DENABLE_SCRIPTING=OFF \
    -DENABLE_LTO=OFF -DCITRA_WARNINGS_AS_ERRORS=OFF \
    -DENABLE_CUBEB=ON -DENABLE_OPENAL=ON -DENABLE_SDL2=ON -DUSE_SYSTEM_SDL2=ON \
    ${CF_EXTRA_CMAKE_FLAGS}
  nice -n 19 ninja -C "$BLD" -j"$JOBS"
fi

"$HERE/release/make-release.sh" "$VERSION" "$BLD/bin/Release"
