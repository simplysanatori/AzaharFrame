#!/bin/bash
# Builds the texture pack encoder once: ARM's astcenc library (Apache-2.0) from a pinned tag,
# with NEON, and af-ktxenc.cpp (next to this script) linked against it.  Under a minute on
# the Frame, at the lowest priority.  Needs git, cmake and a C++ compiler (g++ or clang++).
#   build-astcenc.sh [--force]        (AZAHARFRAME_JOBS=N limits the compile jobs)
# Result: $AZAHARFRAME_HOME/tools/af-ktxenc (default ~/.local/share/AzaharFrame/tools).
# The astcenc checkout and build stay in $AZAHARFRAME_HOME/tools/src for a rebuild; delete
# that folder to free the space (about 40 MB).
set -euo pipefail
ASTCENC_TAG=5.7.1
HERE=$(cd "$(dirname "$0")" && pwd)
DATA=${AZAHARFRAME_HOME:-$HOME/.local/share/AzaharFrame}
TOOLS=$DATA/tools
SRC=$TOOLS/src/astc-encoder-$ASTCENC_TAG
OUT=$TOOLS/af-ktxenc
FORCE=
case "${1:-}" in --force) FORCE=1 ;; "") ;; *) echo "usage: $0 [--force]" >&2; exit 2 ;; esac

if [ -x "$OUT" ] && [ -z "$FORCE" ] && [ "$OUT" -nt "$HERE/af-ktxenc.cpp" ]; then
  echo "af-ktxenc is already built: $OUT"
  exit 0
fi
for tool in git cmake; do
  command -v "$tool" > /dev/null || { echo "build-astcenc.sh needs $tool" >&2; exit 1; }
done
CXX=${CXX:-}
if [ -z "$CXX" ]; then
  if command -v g++ > /dev/null; then CXX=g++; elif command -v clang++ > /dev/null; then CXX=clang++; fi
fi
[ -n "$CXX" ] || { echo "build-astcenc.sh needs a C++ compiler (g++ or clang++)" >&2; exit 1; }
JOBS=${AZAHARFRAME_JOBS:-$(nproc 2> /dev/null || echo 4)}
NICE="nice -n 19"
command -v ionice > /dev/null && NICE="$NICE ionice -c 3"

mkdir -p "$TOOLS/src"
if [ ! -f "$SRC/Source/astcenc.h" ]; then
  echo "fetching astcenc $ASTCENC_TAG"
  rm -rf "$SRC.tmp"
  git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$ASTCENC_TAG" https://github.com/ARM-software/astc-encoder.git "$SRC.tmp"
  mv "$SRC.tmp" "$SRC"
fi
[ -f "$SRC/Source/ThirdParty/stb_image.h" ] || { echo "astcenc $ASTCENC_TAG has no Source/ThirdParty/stb_image.h" >&2; exit 1; }

# NEON on arm64 (the Frame); elsewhere whatever the compiler targets by default
if [ "$(uname -m)" = aarch64 ]; then ISA=neon; else ISA=native; fi
echo "building the astcenc library ($ISA)"
ISA_OPT=$(echo "$ISA" | tr a-z A-Z)
$NICE cmake -S "$SRC" -B "$SRC/build" -DCMAKE_BUILD_TYPE=Release "-DASTCENC_ISA_$ISA_OPT=ON" \
  -DASTCENC_CLI=OFF -DASTCENC_WERROR=OFF > "$SRC/build.log"
$NICE cmake --build "$SRC/build" --target "astcenc-$ISA-static" -j "$JOBS" >> "$SRC/build.log"
LIB=$(find "$SRC/build" -name "libastcenc-$ISA-static.a" | head -n 1)
[ -n "$LIB" ] || { echo "libastcenc-$ISA-static.a was not built; see $SRC/build.log" >&2; exit 1; }

echo "building af-ktxenc"
$NICE "$CXX" -std=c++17 -O2 -I "$SRC/Source" "$HERE/af-ktxenc.cpp" "$LIB" -lpthread -o "$OUT.new"
mv "$OUT.new" "$OUT"
cp "$SRC/LICENSE.txt" "$TOOLS/astcenc-LICENSE.txt"
echo "built $OUT (astcenc $ASTCENC_TAG)"
