#!/bin/bash
# Removes Azahar for Frame's Steam library entry and program folder.  The data folder
# (~/.local/share/AzaharFrame: your settings, saves, keys, shader caches and logs) stays
# unless --purge.  Your game files are never touched, wherever they are.
#   steam-frame/uninstall.sh [--purge]
# Environment: AZAHARFRAME_APP, AZAHARFRAME_HOME and AZAHARFRAME_SKIP_STEAM as in install.sh.
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

# steam-cef.py is next to this script, or in the program folder when this copy is the installed one.
CEFPY=$HERE/steam-cef.py
[ -f "$CEFPY" ] || CEFPY=$DEST/steam-cef.py

if [ -n "$AZAHARFRAME_SKIP_STEAM" ]; then
  echo "AZAHARFRAME_SKIP_STEAM set: Steam left alone"
elif curl -s --max-time 3 127.0.0.1:8080/json > /dev/null; then
  CEF="python3 $CEFPY"
  ID=$($CEF "(() => { for (const a of (appStore.allApps || [])) { if (a.display_name === '$NAME' && a.app_type === 1073741824) return a.appid; } return 0; })()" 2> /dev/null || echo 0)
  if [ -n "$ID" ] && [ "$ID" != 0 ]; then
    $CEF "(() => { SteamClient.Apps.RemoveShortcut($ID); return 1; })()" > /dev/null
    echo "removed the '$NAME' entry (appid $ID)"
  else
    echo "Steam has no '$NAME' entry"
  fi
else
  echo "Steam is not reachable, so the '$NAME' entry stays; remove it in Steam, or run this again with Steam up."
fi

# The dock icon files (icon-theme PNGs, azaharframe.desktop, SteamVR's app manifest); dock-icon.py
# is next to this script, or in the program folder when this copy is the installed one.
DOCKPY=$HERE/dock-icon.py
[ -f "$DOCKPY" ] || DOCKPY=$DEST/dock-icon.py
if [ -n "$AZAHARFRAME_SKIP_STEAM" ]; then
  echo "AZAHARFRAME_SKIP_STEAM set: the dock icon files and SteamVR's manifest left alone"
elif [ -f "$DOCKPY" ]; then
  python3 -I "$DOCKPY" remove "$DEST" || echo "note: could not remove the dock icon files (see above)"
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
