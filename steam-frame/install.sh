#!/bin/bash
# Installs CitraFrame on the Steam Frame: copies the emulator, the launch wrapper and
# the helper scripts to ~/Games/CitraFrame/app and adds a "CitraFrame" entry to the
# Steam library, set to 120 Hz in SteamVR.  Safe to run again: it updates the same entry
# and leaves your games, saves, keys and settings alone.
#   steam-frame/install.sh [citraframe-*-linux-arm64.tar.gz]
# Without an argument it installs the folder it is in when that is an unpacked release
# (the tarball carries this script at its top level, which is how Frametanium runs it),
# else the newest tarball in steam-frame/release/, else the build in build/bin/Release/.
# It never asks a question.  When Steam cannot be reached it installs the files, says
# that the Steam entry was skipped and exits 0; run it again with Steam up.
#
# For a trial install (checks, packaging) these environment variables redirect it:
#   CITRAFRAME_APP=DIR        where the program goes (default ~/Games/CitraFrame/app)
#   CITRAFRAME_HOME=DIR       the data folder (default ~/.local/share/CitraFrame)
#   CITRAFRAME_BUILD=DIR      the build to install from a checkout (default <repo>/build/bin/Release)
#   CITRAFRAME_SKIP_STEAM=1   leave Steam's library and SteamVR's settings alone
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DEST=${CITRAFRAME_APP:-$HOME/Games/CitraFrame/app}
DATA=${CITRAFRAME_HOME:-$HOME/.local/share/CitraFrame}
SKIP_STEAM=${CITRAFRAME_SKIP_STEAM:-}
NAME="CitraFrame"
VRCMD=/opt/steamvr/bin/linuxarm64/vrcmd
case "$1" in -h | --help) sed -n '2,/^set -e/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;; esac

TARBALL=$1
UNPACKED=
BUILD=
if [ -z "$TARBALL" ]; then
  if [ -x "$HERE/azahar" ] && [ -f "$HERE/citraframe.sh" ]; then
    UNPACKED=$HERE                                         # an unpacked release (Frametanium)
  else
    TARBALL=$(ls "$HERE"/release/citraframe-*-linux-arm64.tar.gz 2> /dev/null | sort -V | tail -1)
    if [ -z "$TARBALL" ]; then
      BUILD=${CITRAFRAME_BUILD:-$HERE/../build/bin/Release}
      [ -x "$BUILD/azahar" ] || { echo "usage: $0 citraframe-*-linux-arm64.tar.gz (from the Releases page), or build first with steam-frame/build.sh" >&2; exit 1; }
    fi
  fi
fi
[ -z "$TARBALL" ] || [ -f "$TARBALL" ] || { echo "no such file: $TARBALL" >&2; exit 1; }

if pgrep -f "^$DEST/azahar" > /dev/null 2>&1; then
  echo "CitraFrame is running. Quit it and run install.sh again." >&2
  exit 1
fi

mkdir -p "$DEST" "$DATA/user"

# put SRC DST: each file is put in place in one step, in case it is in use.
put() { cp "$1" "$2.new" && mv "$2.new" "$2"; }
FILES="azahar citraframe.sh steam-cef.py uninstall.sh install.sh VERSION LICENSE.txt THIRD_PARTY_NOTICES.md"
if [ -n "$UNPACKED" ]; then
  echo "installing the unpacked release in $UNPACKED"
  if [ "$(readlink -f "$UNPACKED")" != "$(readlink -f "$DEST")" ]; then
    for f in $FILES; do [ -f "$UNPACKED/$f" ] && put "$UNPACKED/$f" "$DEST/$f"; done
  fi
elif [ -n "$TARBALL" ]; then
  echo "installing $TARBALL"
  STAGE=$(mktemp -d)
  trap 'rm -rf "$STAGE"' EXIT
  tar -xzf "$TARBALL" -C "$STAGE"
  [ -x "$STAGE/azahar" ] || { echo "$TARBALL has no azahar program" >&2; exit 1; }
  for f in $FILES; do [ -f "$STAGE/$f" ] && put "$STAGE/$f" "$DEST/$f"; done
else
  echo "installing the build in $BUILD"
  strip --strip-unneeded -o "$DEST/azahar.new" "$BUILD/azahar" && mv "$DEST/azahar.new" "$DEST/azahar"
  for f in citraframe.sh steam-cef.py uninstall.sh install.sh; do put "$HERE/$f" "$DEST/$f"; done
  [ -f "$HERE/../LICENSE.txt" ] && put "$HERE/../LICENSE.txt" "$DEST/LICENSE.txt"
  [ -f "$HERE/../THIRD_PARTY_NOTICES.md" ] && put "$HERE/../THIRD_PARTY_NOTICES.md" "$DEST/THIRD_PARTY_NOTICES.md"
fi
chmod +x "$DEST/azahar" "$DEST/citraframe.sh" "$DEST/install.sh" "$DEST/uninstall.sh" 2> /dev/null || true

# The emulator is linked against the system's Qt 6 and SDL2.
MISSING=$(ldd "$DEST/azahar" 2> /dev/null | grep 'not found' || true)
[ -z "$MISSING" ] || echo "warning: libraries the emulator needs are missing:
$MISSING"

APPID=none
STEAM_NOTE=
if [ -n "$SKIP_STEAM" ]; then
  STEAM_NOTE="CITRAFRAME_SKIP_STEAM is set: Steam and SteamVR were left alone."
elif ! curl -s --max-time 3 127.0.0.1:8080/json > /dev/null; then
  STEAM_NOTE="Steam's debugging port is not reachable, so the library entry was skipped. Start Steam and run install.sh again."
else
  CEF="python3 $DEST/steam-cef.py"
  EXISTING=$($CEF "(() => { for (const a of (appStore.allApps || [])) { if (a.display_name === '$NAME' && a.app_type === 1073741824) return a.appid; } return 0; })()" 2> /dev/null || echo 0)
  if [ -n "$EXISTING" ] && [ "$EXISTING" != "0" ]; then
    APPID=$EXISTING
    echo "Steam already has a '$NAME' entry (appid $APPID), updating it"
    $CEF "(() => { SteamClient.Apps.SetShortcutExe($APPID, '\"$DEST/citraframe.sh\"'); SteamClient.Apps.SetShortcutStartDir($APPID, '\"$DEST\"'); return 1; })()" > /dev/null
  else
    APPID=$($CEF "(async () => { const id = await SteamClient.Apps.AddShortcut('$NAME', '$DEST/citraframe.sh', '$DEST', ''); SteamClient.Apps.SetShortcutName(id, '$NAME'); return id; })()")
  fi
  # 120 Hz, no throttling, no motion smoothing: the 3DS's 60 frames a second are
  # shown on every other refresh.  citraframe.sh sets these again at every launch.
  if [ -x "$VRCMD" ]; then
    for kv in preferredRefreshRate=120 framesToThrottle=0 additionalFramesToPredict=0 motionSmoothingOverride=2; do
      "$VRCMD" --set-settings-int "steam.app.$APPID.${kv%=*}" "${kv#*=}" > /dev/null 2>&1 ||
        echo "note: could not set SteamVR's ${kv%=*} for the entry (is SteamVR running?); citraframe.sh sets it at each launch"
    done
  fi
  STEAM_NOTE="Steam library entry \"$NAME\" (appid $APPID) runs $DEST/citraframe.sh."
fi

cat << MSG

CitraFrame $(cat "$DEST/VERSION" 2> /dev/null | head -n 1) installed in $DEST.
Your games, saves, keys and settings are in $DATA/user (kept on every update).
$STEAM_NOTE
MSG
