#!/bin/bash
# Removes Azahar for Frame's Steam library entry and program folder.  The data folder
# (~/.local/share/AzaharFrame: your settings, saves, keys, shader caches and logs) stays
# unless --purge.  Your game files are never touched, wherever they are.
#   steam-frame/uninstall.sh [--purge]
# Environment: AZAHARFRAME_APP, AZAHARFRAME_HOME, AZAHARFRAME_SKIP_STEAM, AZAHARFRAME_CEF_PORT and
# AZAHARFRAME_VRCMD as in install.sh.
# If Steam is not running the entry stays; run this again with Steam up, or remove it in Steam.
# SteamVR's per-app settings for the entry are left alone.  The dock icon files go too:
# ~/.local/share/applications/azaharframe.desktop, the azaharframe and azahar PNGs in
# ~/.local/share/icons/hicolor, and the SteamVR app manifest.
# It never asks a question, so Frametanium can run it.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DEST=${AZAHARFRAME_APP:-$HOME/Games/AzaharFrame/app}
DATA=${AZAHARFRAME_HOME:-$HOME/.local/share/AzaharFrame}
NAME="Azahar for Frame"
PURGE=0
case "$1" in --purge) PURGE=1 ;; "") ;; *) echo "usage: $0 [--purge]" >&2; exit 2 ;; esac
if pgrep -f "^$DEST/azahar" > /dev/null 2>&1; then echo "Azahar for Frame is running; quit it first" >&2; exit 1; fi

# Never remove anything from a folder that is not plainly this program's: an empty or system path
# (a mistyped AZAHARFRAME_APP or AZAHARFRAME_HOME) stops here before anything is touched.
safe_dir() {
  case "$1" in
    "" | / | /home | /usr | /etc | /var | /opt | "$HOME" | "$HOME/" | "$HOME/.local" | "$HOME/.local/share" | "$HOME/Games") return 1 ;;
  esac
  case "$1" in /*) return 0 ;; *) return 1 ;; esac
}
safe_dir "$DEST" || { echo "refusing to remove '$DEST': set AZAHARFRAME_APP to the program folder" >&2; exit 1; }
safe_dir "$DATA" || { echo "refusing to remove '$DATA': set AZAHARFRAME_HOME to the data folder" >&2; exit 1; }

# The program does the Steam and dock icon work (azahar --frame-steam-shortcut, --frame-dock-icon):
# the one next to this script (an unpacked release), or the installed one.
AZ=$HERE/azahar
[ -x "$AZ" ] || AZ=$DEST/azahar

if [ -n "$AZAHARFRAME_SKIP_STEAM" ]; then
  echo "AZAHARFRAME_SKIP_STEAM set: Steam left alone"
elif curl -s --max-time 3 "127.0.0.1:${AZAHARFRAME_CEF_PORT:-8080}/json" > /dev/null; then
  ID=
  if [ -x "$AZ" ]; then ID=$("$AZ" --frame-steam-shortcut find "$NAME" 2> /dev/null || echo 0); fi
  if [ -n "$ID" ] && [ "$ID" != 0 ]; then
    "$AZ" --frame-steam-shortcut remove "$ID" > /dev/null
    echo "removed the '$NAME' entry (appid $ID)"
  else
    echo "Steam has no '$NAME' entry"
  fi
else
  echo "Steam is not reachable, so the '$NAME' entry stays; remove it in Steam, or run this again with Steam up."
fi

# The dock icon files (icon-theme PNGs, azaharframe.desktop, SteamVR's app manifest).
if [ -n "$AZAHARFRAME_SKIP_STEAM" ]; then
  echo "AZAHARFRAME_SKIP_STEAM set: the dock icon files and SteamVR's manifest left alone"
elif [ -x "$AZ" ]; then
  "$AZ" --frame-dock-icon remove "$DEST" || echo "note: could not remove the dock icon files (see above)"
fi

# Remove the program folder last: this script may be running from it.
if [ -d "$DEST" ]; then rm -rf "$DEST"; echo "removed $DEST"; fi
if [ "$PURGE" = 1 ]; then
  echo "--purge: removing $DATA (settings, saves, keys, shader caches, logs)"
  rm -rf "$DATA"
  rm -rf "$HOME/.cache/azaharframe"
else
  echo "kept your data in $DATA; --purge removes it"
fi
