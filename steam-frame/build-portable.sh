#!/bin/bash
# Builds the portable Azahar for Frame release: the program with its own Qt, in the container described by
# steam-frame/portable/Containerfile (Ubuntu 24.04, glibc 2.39, Qt 6.10.3), and packs it for the Releases page
# with steam-frame/release/make-release.sh, which checks the result with steam-frame/tools/check-bundle.sh.
#   steam-frame/build-portable.sh [version]
# Needs podman (or docker: CONTAINER_ENGINE=docker).  The first run builds the image (apt and a Qt download)
# and the program from nothing, about two hours on the Frame; later runs go on from the compiler cache and
# the build tree in ~/.cache/azaharframe/portable.
#
# The Frame is shared: this waits for ~/.cache/headset-in-use to go and for the shared bench lock
# (~/.cache/af-bench-lock, the one azbuild.sh uses), builds with at most 4 jobs at the lowest priority,
# and stops the container (cleanly, ninja goes on next time) when the headset is used or the time
# slot is over.  Run it again to resume.  AZAHARFRAME_NO_LOCK=1 skips the lock and waiting (CI runners).
# Environment:
#   CONTAINER_ENGINE=podman|docker      (default podman)
#   AZAHARFRAME_PORTABLE_CACHE=DIR      work volume, compiler cache and output (default ~/.cache/azaharframe/portable)
#   AZAHARFRAME_JOBS=N                  parallel jobs (default 4, at most 4 on the Frame)
#   AZAHARFRAME_SLOT_MIN=N              minutes to build before letting go of the lock for a while (default 20)
#   AZAHARFRAME_EXTERNALS_FROM=DIR      a checkout whose externals/ is filled, when this is a worktree
#                                       (default: the main checkout of this repository)
#   AZAHARFRAME_REQUIRE_PORTABLE        set to 1 here: make-release.sh then fails if the check fails
#   CF_EXTRA_CMAKE_FLAGS=...            more CMake flags
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
VERSION=${1:-1.0.0}
ENGINE=${CONTAINER_ENGINE:-podman}
CACHE=${AZAHARFRAME_PORTABLE_CACHE:-$HOME/.cache/azaharframe/portable}
JOBS=${AZAHARFRAME_JOBS:-4}
SLOT_MIN=${AZAHARFRAME_SLOT_MIN:-20}
command -v "$ENGINE" > /dev/null || { echo "build-portable: $ENGINE is not installed" >&2; exit 1; }
mkdir -p "$CACHE/work" "$CACHE/ccache" "$CACHE/out"

# --- the shared machine: same rules as azbuild.sh -------------------------------------------------
AF_LOCK=$HOME/.cache/af-bench-lock
AF_STAMP=$HOME/.cache/azaharframe/af-lock-released
AF_PRIORITY=$HOME/.cache/af-bench-priority
AF_HELD=
release_lock() {
  if [ -n "$AF_HELD" ]; then rm -f "$AF_LOCK"; mkdir -p "$(dirname "$AF_STAMP")"; date +%s > "$AF_STAMP"; AF_HELD=; fi
}
machine_busy() {
  [ -e "$HOME/.cache/headset-in-use" ] && return 0
  [ -f "$AF_PRIORITY" ] && ! grep -qx "azahar slot" "$AF_PRIORITY" && return 0
  # a game running under Steam (the always-on terminal is not a game)
  pgrep -af SteamLaunch | grep -v pgrep | grep -v "claude-rc-terminal" | grep -qv "azbuild\|bash -c" && return 0
  return 1
}
take_lock() {
  [ -n "${AZAHARFRAME_NO_LOCK:-}" ] && return 0
  [ -n "$AF_HELD" ] && return 0
  if [ -f "$AF_STAMP" ]; then
    local since=$(( $(date +%s) - $(cat "$AF_STAMP") ))
    if [ "$since" -lt 600 ]; then
      echo "$(date +%T) bench lock released ${since}s ago, waiting $((600 - since))s"
      sleep $((600 - since))
    fi
  fi
  while true; do
    local said=
    while machine_busy; do
      [ -z "$said" ] && echo "$(date +%T) headset or another test is using the machine, waiting (5 s polls)..."
      said=1; sleep 5
    done
    if ( set -o noclobber; echo azaharframe > "$AF_LOCK" ) 2> /dev/null; then
      AF_HELD=1
      if machine_busy; then release_lock; rm -f "$AF_STAMP"; continue; fi
      return 0
    fi
    sleep 5
  done
}
CONTAINER=
stop_container() { [ -n "$CONTAINER" ] && "$ENGINE" stop -t 60 "$CONTAINER" > /dev/null 2>&1 || true; }
on_exit() { stop_container; release_lock; }
trap on_exit EXIT
trap 'exit 130' INT TERM HUP

# one portable build at a time (a separate lock from azbuild.sh: that one is held by every queued compile)
mkdir -p "$HOME/.cache"
exec 9> "$HOME/.cache/azaharframe-portable.lock"
flock 9

# --- the image -----------------------------------------------------------------------------------
IMAGE=azaharframe-portable:$(sha256sum "$HERE/portable/Containerfile" | cut -c1-12)
if ! "$ENGINE" image inspect "$IMAGE" > /dev/null 2>&1; then
  take_lock
  echo "$(date +%T) building the image $IMAGE (apt and Qt downloads)"
  nice -n 19 "$ENGINE" build -t "$IMAGE" -f "$HERE/portable/Containerfile" "$HERE/portable"
fi

# The Qt in the image is the one that was checked in: its tree hash is pinned in portable/QT_TREE_SHA256.
WANT=$(head -n 1 "$HERE/portable/QT_TREE_SHA256")
HAVE=$("$ENGINE" run --rm "$IMAGE" cat /opt/qt-tree.sha256)
[ "$WANT" = "$HAVE" ] || { echo "build-portable: the Qt in the image differs from the pinned one ($HAVE, expected $WANT)" >&2; exit 1; }

# --- the source ----------------------------------------------------------------------------------
EXT_ARGS=()
EXT=${AZAHARFRAME_EXTERNALS_FROM:-}
if [ -z "$EXT" ] && [ -z "$(ls -A "$REPO/externals/boost" 2> /dev/null)" ]; then
  COMMON=$(git -C "$REPO" rev-parse --path-format=absolute --git-common-dir 2> /dev/null || true)
  [ "$(basename "${COMMON:-x}")" = .git ] && EXT=$(dirname "$COMMON")
fi
[ -n "$EXT" ] && EXT_ARGS=(-v "$EXT:/ext-ro:ro")
GIT_COMMIT=$(git -C "$REPO" rev-parse HEAD 2> /dev/null || echo unknown)
GIT_TAG=$(git -C "$REPO" describe --tags --always 2> /dev/null || echo unknown)

# --- the build, in slots --------------------------------------------------------------------------
while true; do
  take_lock
  SLOT_START=$(date +%s)
  CONTAINER=azaharframe-portable-$$
  echo "$(date +%T) building in $IMAGE, -j$JOBS"
  "$ENGINE" rm -f "$CONTAINER" > /dev/null 2>&1 || true
  "$ENGINE" run --rm --init --name "$CONTAINER" --cpus "$JOBS" \
      -v "$REPO:/src-ro:ro" "${EXT_ARGS[@]}" \
      -v "$CACHE/work:/work" -v "$CACHE/ccache:/ccache" -v "$CACHE/out:/out" \
      -e JOBS="$JOBS" -e GIT_COMMIT="$GIT_COMMIT" -e GIT_TAG="$GIT_TAG" -e AZAHARFRAME_VERSION="$VERSION" -e CF_EXTRA_CMAKE_FLAGS="${CF_EXTRA_CMAKE_FLAGS:-}" \
      "$IMAGE" /src-ro/steam-frame/portable/container-build.sh &
  RUN_PID=$!
  STOP=
  while kill -0 "$RUN_PID" 2> /dev/null; do
    sleep 3
    if [ -z "${AZAHARFRAME_NO_LOCK:-}" ]; then
      if machine_busy; then STOP=busy
      elif [ $(( $(date +%s) - SLOT_START )) -ge $(( SLOT_MIN * 60 )) ]; then STOP=slot; fi
    fi
    if [ -n "$STOP" ]; then
      echo "$(date +%T) stopping the build ($STOP)"
      stop_container
      break
    fi
  done
  rc=0; wait "$RUN_PID" || rc=$?
  CONTAINER=
  if [ -z "$STOP" ]; then
    [ "$rc" = 0 ] || { echo "build-portable: the container build failed (status $rc)" >&2; exit "$rc"; }
    break
  fi
  release_lock
  echo "$(date +%T) bench lock released ($STOP), going on later"
done
release_lock

# --- the release ----------------------------------------------------------------------------------
export AZAHARFRAME_BUNDLE=$CACHE/out/bundle AZAHARFRAME_REQUIRE_PORTABLE=1
"$HERE/release/make-release.sh" "$VERSION" "$CACHE/out/bin"
