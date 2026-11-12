#!/bin/bash
# The portable part of make-release.sh: sourced by it, defines two functions.  Kept apart so that the packing
# script only has to call them.
#   portable_stage STAGE      puts the bundle (lib/, plugins/, qt.conf) next to the program in STAGE and sets
#                             PORTABLE_FILES to the names to pack.  The bundle is the folder AZAHARFRAME_BUNDLE
#                             (steam-frame/build-portable.sh makes it).  Without one the release would need
#                             the Qt of the machine it was built on: that is only allowed for a local trial
#                             (a warning), and an error when AZAHARFRAME_REQUIRE_PORTABLE=1.
#   portable_verify TARBALL   unpacks the tarball into a scratch folder and runs tools/check-bundle.sh on it,
#                             then starts the program there headless (-v, LD_LIBRARY_PATH unset) and
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
  # the licences of the libraries the bundle carries (SDL3, sdl2-compat)
  if [ -d "$bundle/licenses" ]; then mkdir -p "$stage/licenses"; cp -a "$bundle/licenses/." "$stage/licenses/"; fi
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
          timeout 60 ./azahar -v 2>&1) || rc=$?
    if [ "$rc" != 0 ]; then echo "$out" | tail -n 20 >&2; rm -rf "$scratch" "$tarball"; echo "make-release: azahar -v failed (status $rc)" >&2; exit 1; fi
    # every Qt, ICU and SDL library the program really loaded came from the folder
    local outside
    outside=$(grep -oE 'calling init: [^ ]+' <<<"$out" | sed 's/^calling init: //' | grep -E '/lib(Qt6|icu|SDL)' | grep -v "^$app/" || true)
    if [ -n "$outside" ]; then echo "$outside" >&2; rm -rf "$scratch" "$tarball"; echo "make-release: loaded libraries from outside the release" >&2; exit 1; fi
    # SDL: sdl2-compat opens SDL3 when SDL starts, which `azahar -v` never does.  Load the bundled libraries the same way
    # here: SDL2 through its own path, then SDL3's version must be 3.4.10 or newer (the Steam Controller's touchpad),
    # and the libSDL3.so.0 the loader opened must be the one in the folder.
    if [ -e "$app/lib/libSDL2-2.0.so.0" ]; then
      local sdlout sdlv
      sdlout=$(cd "$app" && env -u LD_LIBRARY_PATH LD_DEBUG=libs HOME="$scratch/home" python3 -I -c '
import ctypes, sys
l2 = ctypes.CDLL(sys.argv[1] + "/lib/libSDL2-2.0.so.0")
class V(ctypes.Structure): _fields_ = [("major", ctypes.c_uint8), ("minor", ctypes.c_uint8), ("patch", ctypes.c_uint8)]
v = V(); l2.SDL_GetVersion(ctypes.byref(v))
l3 = ctypes.CDLL(sys.argv[1] + "/lib/libSDL3.so.0"); l3.SDL_GetVersion.restype = ctypes.c_int
n = l3.SDL_GetVersion()
print("SDLVERSION sdl2 %d.%d.%d sdl3 %d.%d.%d" % (v.major, v.minor, v.patch, n // 1000000, n // 1000 % 1000, n % 1000))
sys.exit(0 if n >= 3004010 else 3)   # the Steam Controller touchpad needs 3.4.10
' "$app" 2>&1) || { echo "$sdlout" | tail -n 8 >&2; rm -rf "$scratch" "$tarball"; echo "make-release: the bundled SDL did not load, or SDL3 is older than 3.4.10" >&2; exit 1; }
      sdlv=$(grep -oE 'SDLVERSION .*' <<<"$sdlout")
      outside=$(grep -oE 'calling init: [^ ]+' <<<"$sdlout" | sed 's/^calling init: //' | grep -E '/libSDL' | grep -v "^$app/" || true)
      if [ -n "$outside" ]; then echo "$outside" >&2; rm -rf "$scratch" "$tarball"; echo "make-release: SDL loaded from outside the release" >&2; exit 1; fi
      echo "make-release: bundled SDL loads: $sdlv"
    fi
    # -v prints "Azahar for Frame <version>" (the first line is the program's; LD_DEBUG writes the rest)
    local said
    said=$(grep -E '^Azahar for Frame ' <<<"$out" | head -n 1)
    if [ -z "$said" ]; then echo "$out" | tail -n 5 >&2; rm -rf "$scratch" "$tarball"; echo "make-release: azahar -v did not print its name" >&2; exit 1; fi
    # a real release names its version in the program
    if [ "${AZAHARFRAME_REQUIRE_PORTABLE:-0}" = 1 ] && [ -f "$app/VERSION" ] && [ "$said" != "Azahar for Frame $(head -n 1 "$app/VERSION")" ]; then
      rm -rf "$scratch" "$tarball"; echo "make-release: azahar -v says \"$said\", not the release version $(head -n 1 "$app/VERSION") (build with AZAHARFRAME_VERSION)" >&2; exit 1
    fi
    echo "make-release: azahar -v ($said) starts with only its own libraries and the allowlisted ones"
  else
    echo "make-release: not an aarch64 machine, the start test is skipped"
  fi
  rm -rf "$scratch"
}
