#!/bin/bash
# Builds SDL3 and sdl2-compat from pinned official release tarballs (libsdl-org on GitHub) into a prefix, for the
# portable release: the program links SDL2 through sdl2-compat, which loads SDL3 (libSDL3.so.0) from its own
# folder, so the release carries both in lib/ instead of the static SDL 2.32 Azahar pins.  The reason: the
# 2026 Steam Controller's touchpad needs SDL3 3.4.10 or newer, and SteamOS's own SDL3 is older and moves.
#   steam-frame/portable/build-sdl.sh <prefix> <source cache dir> [jobs]
# Installs <prefix>/lib (libSDL3.so.0, libSDL2-2.0.so.0, stripped), <prefix>/include, <prefix>/lib/cmake/SDL2
# (what CMake's find_package(SDL2) reads) and <prefix>/licenses (both zlib licences).  Runs in the build
# container (Ubuntu 24.04, glibc 2.39: the oldest any SteamOS has had), and on any aarch64 Linux with the SDL
# development packages.  Skips the build when <prefix> already holds this pin set.
# To change a version: take the tarball and its sha256 from the release page of libsdl-org/SDL (or
# sdl2-compat) and change the four lines below together; SDL3 must stay at 3.4.10 or newer.
set -euo pipefail
SDL3_VERSION=3.4.20
SDL3_SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
COMPAT_VERSION=2.32.74
COMPAT_SHA256=ec68abde77e2e459c8abc4f9587b8976b5d2a657b2bfda2fb3a079b0f8924588

PREFIX=$(mkdir -p "${1:?usage: build-sdl.sh <prefix> <source cache dir> [jobs]}" && cd "$1" && pwd)
CACHE=$(mkdir -p "${2:?source cache dir}" && cd "$2" && pwd)
JOBS=${3:-${JOBS:-4}}
STAMP="sdl3 $SDL3_VERSION $SDL3_SHA256 sdl2-compat $COMPAT_VERSION $COMPAT_SHA256"
if [ "$(cat "$PREFIX/.pins" 2> /dev/null)" = "$STAMP" ] && [ -e "$PREFIX/lib/libSDL3.so.0" ] && [ -e "$PREFIX/lib/libSDL2-2.0.so.0" ]; then
  echo "sdl: $PREFIX already has SDL3 $SDL3_VERSION and sdl2-compat $COMPAT_VERSION"
  exit 0
fi
fetch() {  # name url sha256
  if [ ! -f "$CACHE/$1" ]; then curl -fsSL -o "$CACHE/$1.part" "$2" && mv "$CACHE/$1.part" "$CACHE/$1"; fi
  echo "$3  $CACHE/$1" | sha256sum -c --quiet || { echo "sdl: $1 does not match its pinned sha256" >&2; rm -f "$CACHE/$1"; exit 1; }
}
fetch "SDL3-$SDL3_VERSION.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL3_VERSION/SDL3-$SDL3_VERSION.tar.gz" "$SDL3_SHA256"
fetch "sdl2-compat-$COMPAT_VERSION.tar.gz" "https://github.com/libsdl-org/sdl2-compat/releases/download/release-$COMPAT_VERSION/sdl2-compat-$COMPAT_VERSION.tar.gz" "$COMPAT_SHA256"

W=$(mktemp -d "$CACHE/build.XXXXXX")
trap 'rm -rf "$W"' EXIT
mkdir -p "$W/sdl3" "$W/compat"
tar -xzf "$CACHE/SDL3-$SDL3_VERSION.tar.gz" -C "$W/sdl3" --strip-components 1
tar -xzf "$CACHE/sdl2-compat-$COMPAT_VERSION.tar.gz" -C "$W/compat" --strip-components 1
rm -rf "$PREFIX/lib" "$PREFIX/include" "$PREFIX/licenses" "$PREFIX/.pins"

# SDL3: shared only.  XTest off: the build image has no libxtst-dev (not worth an image rebuild: SDL3 only
# uses it for synthetic key events on X11).  RPATH off here; stage-bundle.sh gives every bundled library RUNPATH $ORIGIN.
cmake -S "$W/sdl3" -B "$W/sdl3/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL_TESTS=OFF -DSDL_RPATH=OFF \
    -DSDL_X11_XTEST=OFF
cmake --build "$W/sdl3/build" -j"$JOBS"
cmake --install "$W/sdl3/build" --strip

# sdl2-compat: libSDL2-2.0.so.0 on top of the SDL3 above; it opens libSDL3.so.0 when the program starts SDL.
cmake -S "$W/compat" -B "$W/compat/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DSDL2COMPAT_TESTS=OFF -DSDL2COMPAT_STATIC=OFF -DSDL2COMPAT_INSTALL_TESTS=OFF -DSDL2COMPAT_X11=ON
cmake --build "$W/compat/build" -j"$JOBS"
cmake --install "$W/compat/build" --strip

mkdir -p "$PREFIX/licenses"
cp "$W/sdl3/LICENSE.txt" "$PREFIX/licenses/SDL3-LICENSE.txt"
cp "$W/compat/LICENSE.txt" "$PREFIX/licenses/sdl2-compat-LICENSE.txt"
for f in lib/libSDL3.so.0 lib/libSDL2-2.0.so.0 lib/cmake/SDL2/SDL2Config.cmake; do
  [ -e "$PREFIX/$f" ] || { echo "sdl: $PREFIX/$f was not built" >&2; exit 1; }
done
echo "$STAMP" > "$PREFIX/.pins"
echo "sdl: SDL3 $SDL3_VERSION and sdl2-compat $COMPAT_VERSION in $PREFIX/lib"
