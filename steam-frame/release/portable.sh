#!/bin/bash
# The portable part of make-release.sh: sourced by it, defines two functions.  Kept apart so that the packing
# script only has to call them.
#   portable_stage STAGE      puts the bundle (lib/, plugins/, qt.conf) next to the program in STAGE and sets
#                             PORTABLE_FILES to the names to pack.  The bundle is the folder AZAHARFRAME_BUNDLE
#                             (steam-frame/build-portable.sh makes it).  Without one the release would need
#                             the Qt of the machine it was built on: that is only allowed for a local trial
#                             (a warning), and an error when AZAHARFRAME_REQUIRE_PORTABLE=1.
#   portable_verify TARBALL   unpacks the tarball into a scratch folder and runs tools/check-bundle.sh on it,
#                             then starts the program there headless (--version, LD_LIBRARY_PATH unset) and
#                             checks that Qt came from the folder.  Fails (exit 1) on any problem.
# Needs the program's own architecture to run it: on another machine the start is skipped with a note.

portable_stage() {
  local stage=$1
  PORTABLE_FILES=
  local bundle=${AZAHARFRAME_BUNDLE:-}
  if [ -z "$bundle" ] || [ ! -d "$bundle/lib" ]; then
    if [ "${AZAHARFRAME_REQUIRE_PORTABLE:-0}" = 1 ]; then
      echo "make-release: no portable bundle (AZAHARFRAME_BUNDLE=${bundle:-unset}): run steam-frame/build-portable.sh" >&2
      exit 1
    fi
    echo "make-release: WARNING: not a portable release: this program needs the Qt and SDL of the machine it was built on" >&2
    return 0
  fi
  [ -f "$bundle/qt.conf" ] || { echo "make-release: $bundle has no qt.conf" >&2; exit 1; }
  cp -a "$bundle/lib" "$bundle/plugins" "$bundle/qt.conf" "$stage/"
  PORTABLE_FILES="lib plugins qt.conf"
}

portable_verify() {
  local tarball=$1 sf
  sf=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
  [ -n "$PORTABLE_FILES" ] || return 0
  local scratch app
  scratch=$(mktemp -d)
  app=$scratch/app
  mkdir -p "$app"
  tar -xzf "$tarball" -C "$app"
  echo "make-release: checking the bundle"
  if ! "$sf/tools/check-bundle.sh" "$app"; then rm -rf "$scratch" "$tarball"; echo "make-release: the bundle check failed" >&2; exit 1; fi
  if [ "$(uname -m)" = aarch64 ]; then
    local out rc=0
    out=$(cd "$app" && env -u LD_LIBRARY_PATH -u QT_PLUGIN_PATH QT_QPA_PLATFORM=offscreen LD_DEBUG=libs HOME="$scratch/home" \
          timeout 60 ./azahar --version 2>&1) || rc=$?
    if [ "$rc" != 0 ]; then echo "$out" | tail -n 20 >&2; rm -rf "$scratch" "$tarball"; echo "make-release: azahar --version failed (status $rc)" >&2; exit 1; fi
    # every Qt, ICU and SDL library the program really loaded came from the folder
    local outside
    outside=$(grep -oE 'calling init: [^ ]+' <<<"$out" | sed 's/^calling init: //' | grep -E '/lib(Qt6|icu|SDL)' | grep -v "^$app/" || true)
    if [ -n "$outside" ]; then echo "$outside" >&2; rm -rf "$scratch" "$tarball"; echo "make-release: loaded libraries from outside the release" >&2; exit 1; fi
    echo "make-release: azahar --version starts with only its own libraries and the allowlisted ones"
  else
    echo "make-release: not an aarch64 machine, the start test is skipped"
  fi
  rm -rf "$scratch"
}
