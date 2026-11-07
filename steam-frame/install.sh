#!/bin/bash
# Installs Azahar for Frame on the Steam Frame: copies the emulator, the launch wrapper and
# the helper scripts to ~/Games/AzaharFrame/app and adds an "Azahar for Frame" entry to the
# Steam library, set to 120 Hz in SteamVR.  Safe to run again: it updates the same entry
# and leaves your games, saves, keys and settings alone.
#   steam-frame/install.sh [--dry-run] [azaharframe-*-linux-arm64.tar.gz]
# Without an argument it installs the folder it is in when that is an unpacked release
# (the tarball carries this script at its top level, which is how Frametanium runs it),
# else the newest tarball in steam-frame/release/, else the build in build/bin/Release/.
# --dry-run prints every file it would copy and every Steam / SteamVR call it would make
# (with the JavaScript for Steam's debugging port) and changes nothing.
# It never asks a question.  When Steam cannot be reached it installs the files, says
# that the Steam entry was skipped and exits 0; run it again with Steam up.
#
# For a trial install (checks, packaging) these environment variables redirect it:
#   AZAHARFRAME_APP=DIR        where the program goes (default ~/Games/AzaharFrame/app)
#   AZAHARFRAME_HOME=DIR       the data folder (default ~/.local/share/AzaharFrame)
#   AZAHARFRAME_BUILD=DIR      the build to install from a checkout (default <repo>/build/bin/Release)
#   AZAHARFRAME_SKIP_STEAM=1   leave Steam's library and SteamVR's settings alone
#   AZAHARFRAME_DRY_RUN=1      the same as --dry-run
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DEST=${AZAHARFRAME_APP:-$HOME/Games/AzaharFrame/app}
DATA=${AZAHARFRAME_HOME:-$HOME/.local/share/AzaharFrame}
SKIP_STEAM=${AZAHARFRAME_SKIP_STEAM:-}
NAME="Azahar for Frame"
VRCMD=/opt/steamvr/bin/linuxarm64/vrcmd
DRY=${AZAHARFRAME_DRY_RUN:-}
case "$1" in -h | --help) sed -n '2,/^set -e/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;; esac
if [ "$1" = --dry-run ]; then DRY=1; shift; fi
if [ "$DRY" = 0 ]; then DRY=; fi

# In a dry run these print instead of acting.  `act` runs (or prints) one command line,
# `put SRC DST` puts a file in place in one step (in case it is in use).
act() { if [ -n "$DRY" ]; then echo "[dry run] would run: $*"; else "$@"; fi; }
put() {
  if [ -n "$DRY" ]; then echo "[dry run] would copy $1 -> $2"; else cp "$1" "$2.new" && mv "$2.new" "$2"; fi
}
# cef_art APPID ARTDIR: the art step of steam-cef.py (portrait, hero, logo, wide, icon).  A dry run
# runs steam-cef.py's own --dry-run from the release, which lists each Steam call and does not connect.
cef_art() {
  if [ -n "$DRY" ]; then
    python3 -I "$SRC_DIR/steam-cef.py" --art "$1" "$ART_SRC" --dry-run
    echo "[dry run]   (the icon would be read from $DEST/art/icon.png once the art is copied there)"
  else
    python3 "$DEST/steam-cef.py" --art "$1" "$2"
  fi
}

TARBALL=$1
UNPACKED=
BUILD=
if [ -z "$TARBALL" ]; then
  if [ -x "$HERE/azahar" ] && [ -f "$HERE/azaharframe.sh" ]; then
    UNPACKED=$HERE                                         # an unpacked release (Frametanium)
  else
    TARBALL=$(ls "$HERE"/release/azaharframe-*-linux-arm64.tar.gz 2> /dev/null | sort -V | tail -1)
    if [ -z "$TARBALL" ]; then
      BUILD=${AZAHARFRAME_BUILD:-$HERE/../build/bin/Release}
      [ -x "$BUILD/azahar" ] || { echo "usage: $0 azaharframe-*-linux-arm64.tar.gz (from the Releases page), or build first with steam-frame/build.sh" >&2; exit 1; }
    fi
  fi
fi
[ -z "$TARBALL" ] || [ -f "$TARBALL" ] || { echo "no such file: $TARBALL" >&2; exit 1; }

if pgrep -f "^$DEST/azahar" > /dev/null 2>&1; then
  echo "Azahar for Frame is running. Quit it and run install.sh again." >&2
  exit 1
fi

[ -z "$DRY" ] || echo "[dry run] nothing below is done; HOME=$HOME DEST=$DEST DATA=$DATA"
act mkdir -p "$DEST" "$DATA/user"

FILES="azahar azaharframe.sh steam-cef.py dock-icon.py uninstall.sh install.sh VERSION LICENSE.txt THIRD_PARTY_NOTICES.md"
# The Steam library art (portrait, wide, hero, logo) and the icon in the sizes the dock and the
# window need; the program also reads art/logo.png (in-VR picker) and art/icon.png (window icon).
ARTFILES="portrait.png wide.png hero.png logo.png icon.png icon-64.png icon-128.png icon-256.png"
ART_SRC=
SRC_DIR=
if [ -n "$UNPACKED" ]; then
  echo "installing the unpacked release in $UNPACKED"
  ART_SRC=$UNPACKED/art
  SRC_DIR=$UNPACKED
  if [ "$(readlink -f "$UNPACKED")" != "$(readlink -f "$DEST")" ]; then
    for f in $FILES; do if [ -f "$UNPACKED/$f" ]; then put "$UNPACKED/$f" "$DEST/$f"; fi; done
    act mkdir -p "$DEST/art"
    for f in $ARTFILES; do if [ -f "$ART_SRC/$f" ]; then put "$ART_SRC/$f" "$DEST/art/$f"; else echo "warning: art/$f is not in the release"; fi; done
  else
    ART_SRC=$DEST/art
    SRC_DIR=$DEST
  fi
elif [ -n "$TARBALL" ]; then
  echo "installing $TARBALL"
  STAGE=$(mktemp -d)
  trap 'rm -rf "$STAGE"' EXIT
  tar -xzf "$TARBALL" -C "$STAGE"
  [ -x "$STAGE/azahar" ] || { echo "$TARBALL has no azahar program" >&2; exit 1; }
  ART_SRC=$STAGE/art
  SRC_DIR=$STAGE
  for f in $FILES; do if [ -f "$STAGE/$f" ]; then put "$STAGE/$f" "$DEST/$f"; fi; done
  act mkdir -p "$DEST/art"
  for f in $ARTFILES; do if [ -f "$ART_SRC/$f" ]; then put "$ART_SRC/$f" "$DEST/art/$f"; else echo "warning: art/$f is not in the release"; fi; done
else
  echo "installing the build in $BUILD"
  ART_SRC=$HERE/art
  SRC_DIR=$HERE
  if [ -n "$DRY" ]; then
    echo "[dry run] would copy $BUILD/azahar (stripped) -> $DEST/azahar"
  else
    strip --strip-unneeded -o "$DEST/azahar.new" "$BUILD/azahar" && mv "$DEST/azahar.new" "$DEST/azahar"
  fi
  for f in azaharframe.sh steam-cef.py dock-icon.py uninstall.sh install.sh; do put "$HERE/$f" "$DEST/$f"; done
  if [ -f "$HERE/../LICENSE.txt" ]; then put "$HERE/../LICENSE.txt" "$DEST/LICENSE.txt"; fi
  if [ -f "$HERE/../THIRD_PARTY_NOTICES.md" ]; then put "$HERE/../THIRD_PARTY_NOTICES.md" "$DEST/THIRD_PARTY_NOTICES.md"; fi
  act mkdir -p "$DEST/art"
  for f in $ARTFILES; do if [ -f "$ART_SRC/$f" ]; then put "$ART_SRC/$f" "$DEST/art/$f"; else echo "warning: art/$f is missing"; fi; done
fi
# The texture pack converter (Optimize Texture Pack for Frame in the game list menu).  Its
# encoder is built on first use into $DATA/tools, which stays across updates.
TOOLFILES="convert-texture-pack.sh convert_texture_pack.py build-astcenc.sh af-ktxenc.cpp frame-controller-models.py"
TOOLS_SRC=$SRC_DIR/tools
if [ "$(readlink -f "$TOOLS_SRC")" != "$(readlink -f "$DEST/tools")" ]; then
  act mkdir -p "$DEST/tools"
  for f in $TOOLFILES; do if [ -f "$TOOLS_SRC/$f" ]; then put "$TOOLS_SRC/$f" "$DEST/tools/$f"; fi; done
fi
for f in convert-texture-pack.sh build-astcenc.sh; do
  if [ -n "$DRY" ] || [ -f "$DEST/tools/$f" ]; then act chmod +x "$DEST/tools/$f"; fi
done
act chmod +x "$DEST/azahar" "$DEST/azaharframe.sh" "$DEST/install.sh" "$DEST/uninstall.sh" "$DEST/dock-icon.py"
DOCK_PY=$DEST/dock-icon.py
[ -z "$DRY" ] || DOCK_PY=$SRC_DIR/dock-icon.py   # nothing is in $DEST in a dry run

# The Steam Frame controllers drawn in VR (Show Controllers): converted from SteamVR's own models on
# this Frame (Valve's, so never shipped) into $DATA/user/frame-models.  Without SteamVR or GdkPixbuf
# the beams still show; azaharframe.sh tries again in the background on a later start.
MODELS=$DATA/user/frame-models
if [ -f "$MODELS/frame_controller_left.model" ] && [ -f "$MODELS/frame_controller_right.model" ]; then
  echo "controller models: already in $MODELS"
elif [ -d /opt/steamvr/drivers/frame_controller/resources/rendermodels ]; then
  act python3 -I "$TOOLS_SRC/frame-controller-models.py" "$MODELS" || echo "warning: the controller models could not be made (the beams still show)"
else
  echo "note: SteamVR's Frame controller models are not here; the controllers are not drawn in VR"
fi

# The emulator is linked against the system's Qt 6 and SDL2.
MISSING=$(ldd "$DEST/azahar" 2> /dev/null | grep 'not found' || true)
[ -z "$MISSING" ] || echo "warning: libraries the emulator needs are missing:
$MISSING"

APPID=none
STEAM_NOTE=
# Steam picks the appid itself when AddShortcut creates the entry; it is not crc32(Exe + AppName)
# (that is only what Steam computes for an entry read from shortcuts.vdf), so a dry run cannot
# know it.  It shows this placeholder (the smallest id a shortcut can have) in its place.
DRY_APPID=2147483648
CEF="python3 $DEST/steam-cef.py"
FIND_JS="(() => { for (const a of (appStore.allApps || [])) { if (a.display_name === '$NAME' && a.app_type === 1073741824) return a.appid; } return 0; })()"
UPDATE_JS="(appid) => { SteamClient.Apps.SetShortcutExe(appid, '\"$DEST/azaharframe.sh\"'); SteamClient.Apps.SetShortcutStartDir(appid, '\"$DEST\"'); return 1; }"
# AddShortcut puts the quotes around the program and folder itself; SetShortcutExe and
# SetShortcutStartDir store the text as given, so those two get them.
ADD_JS="(async () => { const id = await SteamClient.Apps.AddShortcut('$NAME', '$DEST/azaharframe.sh', '$DEST', ''); SteamClient.Apps.SetShortcutName(id, '$NAME'); return id; })()"
if [ -n "$SKIP_STEAM" ]; then
  STEAM_NOTE="AZAHARFRAME_SKIP_STEAM is set: Steam and SteamVR were left alone."
elif [ -n "$DRY" ]; then
  echo "[dry run] Steam is not contacted; its debugging port (127.0.0.1:8080) is assumed reachable"
  echo "[dry run] would call Steam (CEF) to look for an existing entry: $FIND_JS"
  echo "[dry run] if '$NAME' exists it would be updated: $UPDATE_JS applied to its appid"
  echo "[dry run] otherwise it would be added: $ADD_JS"
  APPID=$DRY_APPID
  echo "[dry run] the entry's appid would be whatever Steam assigns when it adds the entry (shown below as the placeholder $APPID)"
else
  EXISTING=$($CEF "$FIND_JS" 2> /dev/null || echo 0)
  if [ -n "$EXISTING" ] && [ "$EXISTING" != "0" ]; then
    APPID=$EXISTING
    echo "Steam already has a '$NAME' entry (appid $APPID), updating it"
    $CEF "(() => { SteamClient.Apps.SetShortcutExe($APPID, '\"$DEST/azaharframe.sh\"'); SteamClient.Apps.SetShortcutStartDir($APPID, '\"$DEST\"'); return 1; })()" > /dev/null
  else
    APPID=$($CEF "$ADD_JS" 2> /dev/null || echo 0)
  fi
  case "$APPID" in '' | 0 | *[!0-9]*)
    echo "note: Steam did not return an appid for the entry, so the library entry, its art and the dock icon were skipped; run install.sh again with Steam up"
    APPID=none
    STEAM_NOTE="Steam was not reachable, so the library entry, its art and the dock icon were skipped. Run install.sh again with Steam up."
    ;;
  esac
fi

if [ "$APPID" != none ]; then
  # Library art: portrait, wide, hero, logo (SetCustomArtworkForApp) and the icon the library and
  # SteamVR's dock show (SetShortcutIcon, art/icon.png: a full-bleed 512 px square).  Set live
  # through Steam; run again to set it again.
  if [ -f "$ART_SRC/portrait.png" ]; then
    cef_art "$APPID" "$DEST/art" || echo "note: could not set the library art; run install.sh again with Steam up"
  else
    echo "warning: no art in the release ($ART_SRC/portrait.png), the library entry has no artwork"
  fi
  # SteamVR's dock and dashboard: the VR app's name and icon (an app manifest for steam.app.<appid>)
  # and the flat window's icon-theme PNGs and desktop file; see dock-icon.py.
  if [ -f "$DOCK_PY" ]; then
    python3 -I "$DOCK_PY" install "$DEST" --appid "$APPID" --art "$ART_SRC" ${DRY:+--dry-run} ||
      echo "note: the dock icon was not set up (see above); run install.sh again"
  else
    echo "warning: $DOCK_PY is missing, so the SteamVR dock icon was not set up"
  fi
  # 120 Hz, no throttling, no motion smoothing: the 3DS's 60 frames a second are
  # shown on every other refresh.  azaharframe.sh sets these again at every launch.
  if [ -n "$DRY" ]; then
    for kv in preferredRefreshRate=120 framesToThrottle=0 additionalFramesToPredict=0 motionSmoothingOverride=2; do
      echo "[dry run] would run: $VRCMD --set-settings-int steam.app.$APPID.${kv%=*} ${kv#*=}"
    done
  elif [ -x "$VRCMD" ]; then
    for kv in preferredRefreshRate=120 framesToThrottle=0 additionalFramesToPredict=0 motionSmoothingOverride=2; do
      "$VRCMD" --set-settings-int "steam.app.$APPID.${kv%=*}" "${kv#*=}" > /dev/null 2>&1 ||
        echo "note: could not set SteamVR's ${kv%=*} for the entry (is SteamVR running?); azaharframe.sh sets it at each launch"
    done
  fi
  STEAM_NOTE="Steam library entry \"$NAME\" (appid $APPID) runs $DEST/azaharframe.sh.
Its artwork and icon are set. SteamVR's dock reads the icon when Steam starts: if it still shows
a question mark, restart Steam once."
fi

if [ -n "$DRY" ]; then
  echo
  echo "[dry run] done: nothing was changed."
  exit 0
fi
cat << MSG

Azahar for Frame $(cat "$DEST/VERSION" 2> /dev/null | head -n 1) installed in $DEST.
Your games, saves, keys and settings are in $DATA/user (kept on every update).
$STEAM_NOTE
MSG
