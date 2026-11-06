#!/bin/bash
# Steam shortcut target for Azahar for Frame.  Runs the emulator in VR (azahar --vr).
#
# The data folder is ~/.local/share/AzaharFrame (AZAHARFRAME_HOME).  The emulator
# starts inside it, and because it holds a folder called user/, the emulator
# keeps its settings, saves, keys and shader caches there (its portable mode)
# instead of in ~/.local/share/azahar-emu.  This wrapper's own output goes to
# launch.log there; the emulator's log is user/log/azahar_log.txt.
#
# Test settings for one launch from the Steam library, which passes no
# environment of its own: VAR=value lines in session.env in the data folder.
#   AZAHARFRAME_FLAT=1       start without --vr (the desktop window only)
#   AZAHARFRAME_REFRESH_HZ   90 or 120 (default 120), asked of SteamVR
#   QT_SCALE_FACTOR=1.25    scale the settings window, if it is too small
HERE=$(dirname "$(readlink -f "$0")")
DATA=${AZAHARFRAME_HOME:-$HOME/.local/share/AzaharFrame}
mkdir -p "$DATA/user" || exit 1
cd "$DATA" || exit 1
[ -f launch.log ] && mv -f launch.log launch.log.prev
exec > launch.log 2>&1
echo "== $(date -Is) Azahar for Frame (SteamAppId=${SteamAppId:-none})"

if [ -f "$DATA/session.env" ]; then
  echo "session.env:"; cat "$DATA/session.env"
  set -a; . "$DATA/session.env"; set +a
fi

# Steam injects Mesa overrides meant for games; they break native apps.
unset MESA_GL_VERSION_OVERRIDE MESA_GLSL_VERSION_OVERRIDE
# Steam points LD_LIBRARY_PATH at its own runtime; the emulator wants the system's libraries.
unset LD_LIBRARY_PATH QT_PLUGIN_PATH QML_IMPORT_PATH QML2_IMPORT_PATH QT_QPA_PLATFORMTHEME
# The OpenXR runtime: SteamVR's.  Only when the default lookup would find none.
if [ -z "$XR_RUNTIME_JSON" ] && [ ! -e "$HOME/.config/openxr/1/active_runtime.json" ] &&
   [ -e /opt/steamvr/steamxr_linuxarm64.json ]; then
  export XR_RUNTIME_JSON=/opt/steamvr/steamxr_linuxarm64.json
fi
echo "XR_RUNTIME_JSON=${XR_RUNTIME_JSON:-default lookup}"

# SteamVR's per-app settings, every launch: the 3DS runs at 60 frames a second,
# shown on every refresh of the display, with no throttling and no motion
# smoothing.  SteamAppId is set by Steam when launched from the library.
HZ=${AZAHARFRAME_REFRESH_HZ:-120}
case "$HZ" in 90 | 120) ;; *) HZ=120 ;; esac
VRCMD=/opt/steamvr/bin/linuxarm64/vrcmd
if [ -n "$SteamAppId" ] && [ -x "$VRCMD" ]; then
  for kv in preferredRefreshRate=$HZ framesToThrottle=0 additionalFramesToPredict=0 motionSmoothingOverride=2; do
    "$VRCMD" --set-settings-int "steam.app.$SteamAppId.${kv%=*}" "${kv#*=}" > /dev/null 2>&1 || echo "could not set SteamVR's ${kv%=*}"
  done
fi

# Tell other work on the Frame that the headset is in use.  Only removed again if
# this wrapper made the marker.
MARK=$HOME/.cache/headset-in-use
MADE_MARK=
if [ ! -e "$MARK" ] && mkdir -p "$HOME/.cache" && : > "$MARK"; then MADE_MARK=1; fi

ARGS=(--vr)
[ "$AZAHARFRAME_FLAT" = 1 ] && ARGS=()
# Not exec: the wrapper stays to remove the marker.  A signal for the wrapper
# (Steam stopping the app) goes on to the emulator.
"$HERE/azahar" "${ARGS[@]}" "$@" &
PID=$!
trap 'kill -TERM "$PID" 2> /dev/null' TERM INT HUP
rc=0
while kill -0 "$PID" 2> /dev/null; do  # a signal interrupts wait before the emulator is gone
  wait "$PID"
  rc=$?
done
trap - TERM INT HUP
[ -n "$MADE_MARK" ] && rm -f "$MARK"
echo "exit status $rc"
exit $rc
