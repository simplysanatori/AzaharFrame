#!/bin/bash
# Runs inside the build container (steam-frame/build-portable.sh starts it): copies the source to the
# working volume, builds Azahar for Frame against the container's Qt with RUNPATH $ORIGIN/lib, and stages
# the Qt libraries and plugins beside it.
# Mounts: /src-ro (the checkout, read only), /ext-ro (a checkout whose externals/ is filled, read only,
# optional), /work (source copy and build tree, kept between runs), /ccache, /out (the result).
# Environment: JOBS, GIT_COMMIT, GIT_TAG (written to GIT-COMMIT and GIT-TAG for the version text),
# AZAHARFRAME_VERSION (a release's version: written to GIT-TAG instead of GIT_TAG), CF_EXTRA_CMAKE_FLAGS.
set -euo pipefail
JOBS=${JOBS:-4}
SRC=/work/src
BLD=/work/build
HERE="$(cd "$(dirname "$0")" && pwd)"
QT="${QT_PREFIX:-/opt/qt/prefix}"

echo "== source ($(git -C /src-ro rev-parse --short HEAD 2> /dev/null || echo "${GIT_COMMIT:0:9}"))"
mkdir -p "$SRC" "$BLD" /out
# The tracked files, except the submodule folders (those are filled below).  Excluded folders are not deleted.
rsync -a --delete --exclude='/.git' --exclude='/build*/' --exclude='/wt/' --exclude='/externals/*/' \
      --exclude='/steam-frame/release/*.tar.gz' --exclude='/steam-frame/release/SHA256SUMS' \
      /src-ro/ "$SRC/"
mkdir -p "$SRC/externals"
for d in /src-ro/externals/*/; do
    n="$(basename "$d")"
    if [ -n "$(ls -A "$d" 2> /dev/null)" ]; then from="$d"
    elif [ -d "/ext-ro/externals/$n" ] && [ -n "$(ls -A "/ext-ro/externals/$n")" ]; then from="/ext-ro/externals/$n/"
    else echo "externals/$n is empty (git submodule update --init, or mount a checkout that has it at /ext-ro)"; exit 1; fi
    mkdir -p "$SRC/externals/$n"
    # no .git: a submodule's .git file points into the checkout's .git, which is not copied (openal-soft
    # would then need .git/modules/openal-soft/index and stop the build)
    rsync -a --delete --delete-excluded --exclude=.git "$from" "$SRC/externals/$n/"
done
# dist/compatibility_list is a submodule as well (the game list the Qt window shows; the build embeds its json)
CL=dist/compatibility_list
if [ -n "$(ls -A "/src-ro/$CL" 2> /dev/null)" ]; then from="/src-ro/$CL/"
elif [ -n "$(ls -A "/ext-ro/$CL" 2> /dev/null)" ]; then from="/ext-ro/$CL/"
else echo "$CL is empty (git submodule update --init, or mount a checkout that has it at /ext-ro)"; exit 1; fi
mkdir -p "$SRC/$CL"
rsync -a --delete --exclude=.git "$from" "$SRC/$CL/"
# the version text: no .git here, so the build reads these two files.  A release names its version
# (AZAHARFRAME_VERSION, e.g. 1.1.1), so that "azahar -v" prints "Azahar for Frame 1.1.1"; otherwise the tag
# or short hash git describe gave.
echo "${GIT_COMMIT:-unknown}" > "$SRC/GIT-COMMIT"
echo "${AZAHARFRAME_VERSION:-${GIT_TAG:-unknown}}" > "$SRC/GIT-TAG"

export CCACHE_DIR=/ccache CCACHE_MAXSIZE=10G CCACHE_BASEDIR=/work
PREFIX_MAP="-ffile-prefix-map=$SRC/= -ffile-prefix-map=$BLD/=build/"
RPATH='$ORIGIN/lib'

echo "== configure (Qt $(basename "$(dirname "$QT")") at $QT, $(c++ --version | head -n 1))"
nice -n 19 cmake -S "$SRC" -B "$BLD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_C_FLAGS="$PREFIX_MAP" -DCMAKE_CXX_FLAGS="$PREFIX_MAP" -DAZAHARFRAME_RELEASE=ON \
    -DCMAKE_PREFIX_PATH="$QT" -DUSE_SYSTEM_QT=ON \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DCMAKE_INSTALL_RPATH="$RPATH" \
    -DCMAKE_EXE_LINKER_FLAGS="-Wl,--enable-new-dtags" \
    -DENABLE_FFMPEG=OFF -DUSE_AUDIOTRACK=OFF \
    -DENABLE_QT=ON -DENABLE_QT_TRANSLATION=ON -DENABLE_QT_UPDATE_CHECKER=OFF \
    -DENABLE_VULKAN=ON -DENABLE_OPENGL=OFF -DENABLE_SOFTWARE_RENDERER=ON \
    -DENABLE_TESTS=OFF -DENABLE_ROOM=OFF -DENABLE_ROOM_STANDALONE=OFF \
    -DENABLE_WEB_SERVICE=OFF -DENABLE_DISCORD_RPC=OFF -DENABLE_SCRIPTING=OFF \
    -DENABLE_LTO=OFF -DCITRA_WARNINGS_AS_ERRORS=OFF \
    -DENABLE_CUBEB=ON -DENABLE_OPENAL=ON -DENABLE_SDL2=ON -DUSE_SYSTEM_SDL2=OFF \
    ${CF_EXTRA_CMAKE_FLAGS:-}

echo "== build -j$JOBS"
# ninja runs beside this script so that a stop (build-portable.sh stops the container when the headset
# is used or the time slot is over) interrupts it cleanly; the next run goes on where it was.
nice -n 19 ninja -C "$BLD" -j"$JOBS" &
NINJA=$!
trap 'kill -INT "$NINJA" 2> /dev/null' TERM INT
rc=0
while kill -0 "$NINJA" 2> /dev/null; do wait "$NINJA" && rc=0 || rc=$?; done
trap - TERM INT
[ "$rc" = 0 ] || { echo "ninja stopped or failed (status $rc)"; exit "$rc"; }

echo "== stage"
BIN="$BLD/bin/Release/azahar"
[ -x "$BIN" ] || { echo "no $BIN"; exit 1; }
rm -rf /out/bin /out/bundle
mkdir -p /out/bin
cp "$BIN" /out/bin/azahar
"$HERE/stage-bundle.sh" "$QT" /out/bin/azahar /out/bundle "$SRC/steam-frame/tools/host-libs.allow"
{
    echo "built: $(date -u +%FT%TZ)"
    echo "source: ${GIT_COMMIT:-unknown}"
    echo "compiler: $(c++ --version | head -n 1)"
    echo "glibc: $(ldd --version | head -n 1)"
    echo "qt: $(basename "$(dirname "$QT")") (tree sha256 $(cat /opt/qt-tree.sha256))"
} > /out/BUILD-INFO.txt
cat /out/BUILD-INFO.txt
