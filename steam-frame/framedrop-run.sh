#!/bin/sh
# run.sh: what FrameDrop (or Steam) starts for Azahar for Frame when it was installed as a FrameDrop zip.
#
# FrameDrop unpacks the zip onto the Frame and adds its own library entry for this run.sh.  The first time it
# runs, and again whenever the VERSION file in this folder differs from the version it installed last, it runs
# ./install.sh without a terminal and with the Steam library step turned off (AZAHARFRAME_SKIP_STEAM=1), so there
# is no second library entry.  Then it starts the installed launch wrapper (azaharframe.sh), which sets SteamVR's
# per-app settings (120 Hz) for the entry that started it.  Everything the installer prints goes to the log file
# named below.  Your games, saves, keys and settings stay in ~/.local/share/AzaharFrame across updates.
#
# Environment (all optional):
#   AZAHARFRAME_APP       where the program is installed (the installer's default is used otherwise)
#   FRAMEDROP_RUN_DRY=1   install if needed, print the launcher it would start, and stop
cd "$(dirname "$0")" || exit 1
HERE=$(pwd)
VERSION=$(head -n 1 VERSION 2> /dev/null | tr -d '\r' | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
[ -n "$VERSION" ] || VERSION=unknown
STATE=${XDG_STATE_HOME:-$HOME/.local/state}/framedrop-azaharframe
LOG=$STATE/install.log
mkdir -p "$STATE" 2> /dev/null

LAUNCHER=${AZAHARFRAME_APP:-$HOME/Games/AzaharFrame/app}/azaharframe.sh

# A FrameDrop unpack can lose the executable bits; give them back to programs and scripts.
python3 -I - "$HERE" << 'PY' 2> /dev/null
import os, stat, sys
for d, _, files in os.walk(sys.argv[1]):
    for n in files:
        p = os.path.join(d, n)
        try:
            if os.path.islink(p):
                continue
            with open(p, "rb") as f:
                h = f.read(4)
            if h == b"\x7fELF" or h[:2] == b"#!":
                os.chmod(p, os.stat(p).st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
        except OSError:
            pass
PY

if [ ! -x "$LAUNCHER" ] || [ "$(cat "$STATE/installed-version" 2> /dev/null)" != "$VERSION" ]; then
    echo "Azahar for Frame $VERSION: installing (log: $LOG)"
    {
        echo "== $(date '+%Y-%m-%d %H:%M:%S') run.sh: install.sh from $HERE"
        AZAHARFRAME_SKIP_STEAM=1 bash ./install.sh < /dev/null
    } >> "$LOG" 2>&1
    rc=$?
    if [ "$rc" = 0 ]; then
        printf '%s\n' "$VERSION" > "$STATE/installed-version"
    else
        echo "Azahar for Frame: the installer failed (exit $rc); last lines of $LOG:" >&2
        tail -n 8 "$LOG" >&2
        [ -x "$LAUNCHER" ] || exit "$rc"
        echo "Azahar for Frame: starting the version that was installed before." >&2
    fi
fi

[ -x "$LAUNCHER" ] || { echo "Azahar for Frame: $LAUNCHER is missing" >&2; exit 1; }
if [ -n "$FRAMEDROP_RUN_DRY" ]; then echo "would start: $LAUNCHER $*"; exit 0; fi
exec "$LAUNCHER" "$@"
