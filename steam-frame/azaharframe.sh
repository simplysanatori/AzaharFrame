#!/bin/bash
# Steam shortcut target for Azahar for Frame.  Runs the emulator in VR (azahar --vr) or flat
# (azahar --flat: its normal window on the headset's virtual screen, with OpenXR ready but no
# session, so the VR/flat chord can switch to VR without a restart).
#
# The data folder is ~/.local/share/AzaharFrame (AZAHARFRAME_HOME).  The emulator
# starts inside it, and because it holds a folder called user/, the emulator
# keeps its settings, saves, keys and shader caches there (its portable mode)
# instead of in ~/.local/share/azahar-emu.  This wrapper's own output goes to
# launch.log there; the emulator's log is user/log/azahar_log.txt.
#
# Which mode it starts in: the one used last, kept in the file "mode" in the data folder
# (the single word vr or flat; VR when there is none).  The emulator writes that file when
# the mode is switched while it runs.  To override it for one launch (testing), none of
# which touches the file:  azaharframe.sh --flat | --vr,  or AZAHARFRAME_FLAT=1 | 0.
#
# Test settings for one launch from the Steam library, which passes no
# environment of its own: VAR=value lines in session.env in the data folder.
#   AZAHARFRAME_FLAT=1       start with --flat (the desktop window only); 0 forces VR
#   AZAHARFRAME_REFRESH_HZ   90 or 120 (default 120), asked of SteamVR
#   QT_SCALE_FACTOR=1.25    scale Azahar's window, if it is too small
#   AZAHARFRAME_VR_PICKER=1  the old VR game picker instead of Azahar's window in VR
HERE=$(dirname "$(readlink -f "$0")")
DATA=${AZAHARFRAME_HOME:-$HOME/.local/share/AzaharFrame}
mkdir -p "$DATA/user" || exit 1

# First run only: custom textures on (texture packs go in user/load/textures/<TitleID>/),
# loaded in the background (not preloaded, which costs memory the Frame shares with
# SteamVR).  Never touches a config that exists, so the owner's choices stay.
CONF="$DATA/user/config/qt-config.ini"
if [ ! -e "$CONF" ]; then
  mkdir -p "$DATA/user/config" && cat > "$CONF" << 'EOF'
[Utility]
async_custom_loading=true
async_custom_loading\default=true
custom_textures=true
custom_textures\default=false
preload_textures=false
preload_textures\default=true
EOF
fi
cd "$DATA" || exit 1
[ -f launch.log ] && mv -f launch.log launch.log.prev
exec > launch.log 2>&1
echo "== $(date -Is) Azahar for Frame (SteamAppId=${SteamAppId:-none})"

if [ -f "$DATA/session.env" ]; then
  echo "session.env:"; cat "$DATA/session.env"
  set -a; . "$DATA/session.env"; set +a
fi

# The mode: --flat / --vr on the command line, else AZAHARFRAME_FLAT (1 or 0, from the
# environment or session.env), else the one used last ($DATA/mode), else VR.
MODE=
FWD=()
for a in "$@"; do
  case "$a" in
    --flat) MODE=flat ;;
    --vr) MODE=vr ;;
    *) FWD+=("$a") ;;
  esac
done
if [ -z "$MODE" ]; then
  case "$AZAHARFRAME_FLAT" in
    1) MODE=flat ;;
    0) MODE=vr ;;
    *) MODE=$(head -c 16 "$DATA/mode" 2> /dev/null | tr -d '[:space:]')
       case "$MODE" in flat | vr) ;; *) MODE=vr ;; esac ;;
  esac
fi
echo "mode: $MODE"
export AZAHARFRAME_MODE=$MODE

# Input in each mode.  VR uses the Frame controllers through the headset's own bindings;
# flat gets them from Steam Input as a gamepad (SDL).  The two live in separate input
# profiles of the emulator's config: "Default" for VR and "Flat Gamepad" for flat.  The
# Flat Gamepad profile is added once, and only when it is missing, so nothing you changed
# is overwritten.  It maps the gamepad by position, like the 3DS (the bottom face button
# is B, the right one A, the left one Y, the top one X), and then this picks the profile
# that goes with the mode.  A profile is never edited, only chosen (apart from removing the
# Home binding earlier versions added to Flat Gamepad).
python3 -I - "$CONF" "$MODE" << 'PY' || echo "could not prepare the input profile (see above)"
import os, sys

conf, mode = sys.argv[1], sys.argv[2]
NAME = "Flat Gamepad"
if not os.path.isfile(conf):
    sys.exit(0)
lines = open(conf, encoding="utf-8").read().split("\n")

# Locate [Controls].
start = next((i for i, l in enumerate(lines) if l.strip() == "[Controls]"), None)
if start is None:
    if lines and lines[-1] == "":
        lines.pop()
    lines += ["", "[Controls]", ""]
    start = len(lines) - 2
end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("[")), len(lines))

def get(key):
    for l in lines[start + 1:end]:
        if l.startswith(key + "="):
            return l[len(key) + 1:]
    return None

def put(key, value):
    global end
    for i in range(start + 1, end):
        if lines[i].startswith(key + "="):
            lines[i] = key + "=" + value
            return
    lines.insert(end, key + "=" + value)
    end += 1

size = int(get("profiles\\size") or 0)
if size == 0:  # the emulator makes its Default profile on first read; make it explicit
    put("profiles\\1\\name", "Default")
    put("profiles\\1\\name\\default", "false")
    size = 1
names = [(get("profiles\\%d\\name" % n) or "Default").strip('"') for n in range(1, size + 1)]

if NAME not in names:
    n = size + 1
    pre = "profiles\\%d\\" % n
    def sdl(**kw):
        kw.update(api="controller", engine="sdl", maptype="all")
        return '"' + ",".join("%s:%s" % (k, kw[k]) for k in sorted(kw)) + '"'
    # SDL game controller buttons: A 0, B 1, X 2, Y 3, Back 4, Start 6, left stick 7,
    # left/right shoulder 9/10, d-pad up/down/left/right 11..14; axes: left stick 0/1,
    # right stick 2/3, left/right trigger 4/5.
    def btn(index, name):
        return sdl(button=index, name=name)
    def trig(axis, name):
        return sdl(axis=axis, name=name, direction="+", threshold="0.500000")
    def stick(x, y, nx, ny):
        return sdl(axis_x=x, axis_y=y, name_x=nx, name_y=ny)
    binds = {
        "button_a": btn(1, "B"), "button_b": btn(0, "A"),
        "button_x": btn(3, "Y"), "button_y": btn(2, "X"),
        "button_up": btn(11, "D-Pad Up"), "button_down": btn(12, "D-Pad Down"),
        "button_left": btn(13, "D-Pad Left"), "button_right": btn(14, "D-Pad Right"),
        "button_l": btn(9, "LB"), "button_r": btn(10, "RB"),
        "button_zl": trig(4, "Left Trigger"), "button_zr": trig(5, "Right Trigger"),
        "button_start": btn(6, "Start"), "button_select": btn(4, "Back/Select"),
        "button_home": "[empty]",  # no Home, as in VR
        "circle_pad": stick(0, 1, "Left Stick X", "Left Stick Y"),
        "c_stick": stick(2, 3, "Right Stick X", "Right Stick Y"),
    }
    put(pre + "name", NAME)
    put(pre + "name\\default", "false")
    put(pre + "input_maptype", "0")  # all controllers
    put(pre + "input_maptype\\default", "false")
    for k, v in binds.items():
        put(pre + k, v)
        put(pre + k + "\\default", "false")
    size = n
    names.append(NAME)
    put("profiles\\size", str(size))
    print("added the input profile '%s'" % NAME)

# Earlier versions put 3DS Home on the left stick click, as VR no longer has it: drop that
# binding when it is still exactly the one they added.
OLD_HOME = '"api:controller,button:7,engine:sdl,maptype:all,name:LS Click"'
pre = "profiles\\%d\\" % (names.index(NAME) + 1)
if get(pre + "button_home") == OLD_HOME:
    put(pre + "button_home", "[empty]")
    print("removed Home from the input profile '%s'" % NAME)

flat_at = names.index(NAME)          # 0-based, as in the "profile" key
want = flat_at if mode == "flat" else next((i for i, nm in enumerate(names) if nm != NAME), 0)
try:
    cur = int(get("profile") or 0)
except ValueError:
    cur = 0
if get("profile\\default") == "true":
    cur = 0
if cur != want:
    put("profile", str(want))
    put("profile\\default", "false")
    print("input profile: %s" % names[want])
new = "\n".join(lines)
if not new.endswith("\n"):
    new += "\n"
if new != open(conf, encoding="utf-8").read():
    tmp = conf + ".new"
    open(tmp, "w", encoding="utf-8").write(new)
    os.replace(tmp, conf)
PY

# Steam injects Mesa overrides meant for games; they break native apps.
unset MESA_GL_VERSION_OVERRIDE MESA_GLSL_VERSION_OVERRIDE
# Steam points LD_LIBRARY_PATH at its own runtime; the emulator wants the system's libraries.
unset LD_LIBRARY_PATH QT_PLUGIN_PATH QML_IMPORT_PATH QML2_IMPORT_PATH QT_QPA_PLATFORMTHEME
# The window class of the flat Qt window (settings): azaharframe, so the SteamVR dock finds the
# Azahar for Frame icon and name (dock-icon.py installs the matching icon and desktop file).
export RESOURCE_NAME=azaharframe
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
VRCMD=${AZAHARFRAME_VRCMD:-/opt/steamvr/bin/linuxarm64/vrcmd}
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

# The Steam Frame controllers drawn in VR: made from SteamVR's own models once (install.sh does it
# too), in the background so this start does not wait; the next start draws them.
MODELS=$DATA/user/frame-models
if [ ! -f "$MODELS/frame_controller_right.model" ] && [ -f "$HERE/tools/frame-controller-models.py" ] &&
   [ -d /opt/steamvr/drivers/frame_controller/resources/rendermodels ]; then
  echo "controller models: making them in the background"
  nice -n 19 python3 -I "$HERE/tools/frame-controller-models.py" "$MODELS" > "$DATA/frame-models.log" 2>&1 &
fi

ARGS=(--vr)
[ "$MODE" = flat ] && ARGS=(--flat)
# Not exec: the wrapper stays to remove the marker.  A signal for the wrapper
# (Steam stopping the app) goes on to the emulator.
"$HERE/azahar" "${ARGS[@]}" "${FWD[@]}" &
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
