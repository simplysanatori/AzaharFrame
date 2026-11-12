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
# A Flat run that did not quit cleanly (the file flat-running is still there) starts the
# next launch in VR, once.
#
# Test settings for one launch from the Steam library, which passes no
# environment of its own: VAR=value lines in session.env in the data folder.
#   AZAHARFRAME_FLAT=1       start with --flat (the desktop window only); 0 forces VR
#   AZAHARFRAME_REFRESH_HZ   90 or 120 (default 120), asked of SteamVR
#   QT_SCALE_FACTOR=1.25    scale Azahar's window, if it is too small
#   AZAHARFRAME_VR_PICKER=1  the old VR game picker instead of Azahar's window in VR
# Headset A/B runs (several launches, each with its own settings): session.queue in the data
# folder, one launch per line, "label: AZAHARFRAME_X=1 AZAHARFRAME_Y=0" (only AZAHARFRAME_
# variables; # comments and empty lines are skipped). Each launch takes the first line, sets its
# variables for that launch only and removes it; the emulator's log of that launch is kept as
# ab-logs/<time>-<label>.txt in the data folder. launch.log says which line ran.
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
  # Read as data, never run: only KEY=VALUE lines, only these names, no expansion of the value.
  while IFS= read -r line || [ -n "$line" ]; do
    line=${line%$'\r'}
    case "$line" in ''|'#'*) continue ;; esac
    line=${line#export }
    key=${line%%=*}
    [ "$key" != "$line" ] || { echo "  (ignored, not KEY=VALUE: ${line:0:40})"; continue; }
    val=${line#*=}
    case "$val" in
      \"*\") val=${val#\"}; val=${val%\"} ;;
      \'*\') val=${val#\'}; val=${val%\'} ;;
    esac
    case "$key" in
      AZAHARFRAME_FLAT|AZAHARFRAME_FLAT_SCREEN|AZAHARFRAME_REFRESH_HZ|AZAHARFRAME_FDM|AZAHARFRAME_PADS| \
      AZAHARFRAME_MEMORY_SCOPE|AZAHARFRAME_MEMORY_MAX_PCT|AZAHARFRAME_MEMORY_RESERVE_MB| \
      AZAHARFRAME_MEMORY_FLOOR_MB|AZAHARFRAME_MEMORY_MAX_MB|AZAHARFRAME_MEMORY_SWAP_MB| \
      AZAHARFRAME_PRELOAD_RESERVE_MB|AZAHARFRAME_IMMERSIVE_FOV|AZAHARFRAME_IMMERSIVE_DISTANCE| \
      AZAHARFRAME_IMMERSIVE_LOCAL|AZAHARFRAME_IMMERSIVE_WIDER|AZAHARFRAME_IMMERSIVE_MEASURE| \
      AZAHARFRAME_MODELS|AZAHARFRAME_MODEL_EYE_PX|AZAHARFRAME_MIRROR_SIZE|AZAHARFRAME_MIRROR_SCALE| \
      AZAHARFRAME_FADE_WINDOW|AZAHARFRAME_KEYBOARD_FONT|AZAHARFRAME_VMA_BLOCK_MIB|AZAHARFRAME_XR_WATCHDOG| \
      AZAHARFRAME_XR_INSTANCE|AZAHARFRAME_XR_WINDOW_EVERY_FRAME|AZAHARFRAME_VR_PICKER| \
      QT_SCALE_FACTOR|XR_RUNTIME_JSON)
        export "$key=$val" ;;
      *) echo "  (ignored, name not allowed: $key)" ;;
    esac
  done < "$DATA/session.env"
fi

# AzaharFrame doubling: one line of session.queue for this launch (see the top of this file).
QUEUE=$DATA/session.queue
AB_LABEL=
if [ -f "$QUEUE" ]; then
  AB_LINE=$(grep -m 1 -v -E '^[[:space:]]*(#|$)' "$QUEUE")
  if [ -n "$AB_LINE" ]; then
    awk '!taken && $0 !~ /^[[:space:]]*(#|$)/ {taken = 1; next} {print}' "$QUEUE" > "$QUEUE.new" &&
      mv -f "$QUEUE.new" "$QUEUE"
    AB_VARS=${AB_LINE#*:}
    AB_LABEL=${AB_LINE%%:*}
    [ "$AB_LABEL" = "$AB_LINE" ] && { AB_LABEL=ab; AB_VARS=$AB_LINE; }
    AB_LABEL=$(printf '%s' "$AB_LABEL" | tr -c 'A-Za-z0-9_.-' '-' | head -c 40)
    echo "session.queue: $AB_LABEL:$AB_VARS"
    for kv in $AB_VARS; do
      case "$kv" in
        AZAHARFRAME_[A-Z0-9_]*=*) export "$kv" ;;
        *) echo "session.queue: '$kv' left out (only AZAHARFRAME_ variables)" ;;
      esac
    done
    echo "session.queue: $(grep -c -v -E '^[[:space:]]*(#|$)' "$QUEUE") more launches queued"
  fi
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
       case "$MODE" in flat | vr) ;; *) MODE=vr ;; esac
       # AzaharFrame flat: the emulator keeps the file flat-running while it runs in Flat and
       # removes it when it quits cleanly (or goes to VR). Still there: the last Flat run ended
       # in a crash, a kill or out of memory, so VR this once (the mode file stays as it is;
       # the next launch is Flat again). Overrides (--flat, AZAHARFRAME_FLAT) are not changed.
       if [ "$MODE" = flat ] && [ -e "$DATA/flat-running" ]; then
         echo "mode: the last Flat run did not quit cleanly (crash, kill or out of memory): VR this once"
         MODE=vr
       fi ;;
  esac
fi
rm -f "$DATA/flat-running"
echo "mode: $MODE"
export AZAHARFRAME_MODE=$MODE
# Test driver: AZAHARFRAME_TEST_MODE_ONLY=1 stops here (steam-frame/test/launch-mode-test.sh).
[ "$AZAHARFRAME_TEST_MODE_ONLY" = 1 ] && exit 0

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

# Valve's FDM injection layer foveates the app's own Vulkan render passes as high as a swapchain
# over about 1000 px (at 6x both are 1440): off, unless AZAHARFRAME_FDM=1 (session.env).
if [ "${AZAHARFRAME_FDM:-}" = 1 ]; then
  unset DISABLE_VULKAN_FDM_INJECTION_LAYER
else
  export DISABLE_VULKAN_FDM_INJECTION_LAYER=1
fi
# Steam injects Mesa overrides meant for games; they break native apps.
unset MESA_GL_VERSION_OVERRIDE MESA_GLSL_VERSION_OVERRIDE
# Steam points LD_LIBRARY_PATH at its own runtime; the emulator wants the system's libraries.
unset LD_LIBRARY_PATH QT_PLUGIN_PATH QML_IMPORT_PATH QML2_IMPORT_PATH QT_QPA_PLATFORMTHEME
# The window class of the flat Qt window (settings): azaharframe, so the SteamVR dock finds the
# Azahar for Frame icon and name (azahar --frame-dock-icon installs the matching icon and desktop file).
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
# this wrapper made the marker.  AzaharFrame integ3: AZAHARFRAME_MARK is another path for it, so
# tests never make or remove the real one (steam-frame/test/*.sh set it to a throwaway file).
MARK=${AZAHARFRAME_MARK:-$HOME/.cache/headset-in-use}
MADE_MARK=
# AzaharFrame memguard: a marker older than this boot was left by a launch that never got to
# remove it (the headset froze and rebooted): nothing holds it now, so it is taken over (and
# removed at the end like one made here). A marker from this boot is someone else's: left alone.
BOOT=$(awk '/^btime / {print $2}' /proc/stat 2> /dev/null)
if [ -e "$MARK" ] && [ -n "$BOOT" ] && [ "$(stat -c %Y "$MARK" 2> /dev/null || echo "$BOOT")" -lt "$BOOT" ]; then
  echo "headset marker: left from before this boot, taken over"
  rm -f "$MARK"
fi
if [ ! -e "$MARK" ] && mkdir -p "$(dirname "$MARK")" && : > "$MARK"; then MADE_MARK=1; fi

# The Steam Frame controllers drawn in VR: made from SteamVR's own models once (install.sh does it
# too), in the background so this start does not wait; the next start draws them.
MODELS=$DATA/user/frame-models
if [ ! -f "$MODELS/frame_controller_right.model" ] && [ -x "$HERE/azahar" ] &&
   [ -d /opt/steamvr/drivers/frame_controller/resources/rendermodels ]; then
  echo "controller models: making them in the background"
  nice -n 19 "$HERE/azahar" --frame-controller-models "$MODELS" > "$DATA/frame-models.log" 2>&1 &
fi

ARGS=(--vr)
[ "$MODE" = flat ] && ARGS=(--flat)

# AzaharFrame memguard: the emulator runs in a user systemd scope with a memory limit, so one
# that runs away (a texture pack far too large) is killed by the kernel instead of pushing the
# whole headset into swap until it freezes and reboots (seen twice, no OOM kill: the Frame's
# memory is shared with SteamVR, passthrough and the GPU). MemoryMax is the smaller of
# AZAHARFRAME_MEMORY_MAX_PCT % of RAM (default 65, 30 to 90) and what is free now (MemAvailable)
# less AZAHARFRAME_MEMORY_RESERVE_MB for SteamVR and the compositor (default 2048), but never
# under AZAHARFRAME_MEMORY_FLOOR_MB (default 4096: a game with its GPU memory takes about 2.2 GB
# here, and a save state about 0.9 GB more while it is written).
# MemoryHigh (reclaim and slow down first) is 1/12 under it, and at most
# AZAHARFRAME_MEMORY_SWAP_MB of swap (default 2048). GPU memory counts too: msm's buffers are
# shmem, charged to the scope (its memory.stat shmem matches drm-total-memory in fdinfo).
# systemd-run --scope executes the emulator itself: the same process, environment, folder and
# parent (Steam's reaper tracks it as before), only its cgroup changes. When systemd-run is
# missing or the user manager says no, the emulator runs as before. AZAHARFRAME_MEMORY_SCOPE=0:
# no scope; AZAHARFRAME_MEMORY_MAX_MB: this limit instead (no floor). The emulator reads the
# limit for its texture preload (preload_budget.h). When the kernel kills the emulator for
# memory, the scope's result says oom-kill; the file out-of-memory then tells the next start to
# say so (frame_mode.cpp). Tests: AZAHARFRAME_MEMINFO reads another meminfo file.
SCOPE=()
SYSTEMD_RUN=${AZAHARFRAME_SYSTEMD_RUN:-systemd-run}
MEMINFO=${AZAHARFRAME_MEMINFO:-/proc/meminfo}
MEM_KB=$(awk '/^MemTotal:/ {print $2}' "$MEMINFO" 2> /dev/null)
AVAIL_KB=$(awk '/^MemAvailable:/ {print $2}' "$MEMINFO" 2> /dev/null)
num() { case "$1" in '' | *[!0-9]*) echo "$2" ;; *) echo "$1" ;; esac; }
PCT=$(num "$AZAHARFRAME_MEMORY_MAX_PCT" 65)
[ "$PCT" -lt 30 ] && PCT=30
[ "$PCT" -gt 90 ] && PCT=90
RESERVE_MB=$(num "$AZAHARFRAME_MEMORY_RESERVE_MB" 2048)
FLOOR_MB=$(num "$AZAHARFRAME_MEMORY_FLOOR_MB" 4096)
SWAP_MB=$(num "$AZAHARFRAME_MEMORY_SWAP_MB" 2048)
if [ "${AZAHARFRAME_MEMORY_SCOPE:-1}" = 0 ]; then
  echo "memory: no scope (AZAHARFRAME_MEMORY_SCOPE=0), no memory limit"
elif ! command -v "$SYSTEMD_RUN" > /dev/null 2>&1; then
  echo "memory: $SYSTEMD_RUN not found, running without a memory limit"
elif [ -z "$MEM_KB" ]; then
  echo "memory: no MemTotal in /proc/meminfo, running without a memory limit"
else
  MAX_MB=$((MEM_KB * PCT / 100 / 1024))
  WHY="${PCT} % of $((MEM_KB / 1024)) MiB"
  if [ -n "$AVAIL_KB" ]; then
    FREE_MB=$((AVAIL_KB / 1024 - RESERVE_MB))
    if [ "$FREE_MB" -lt "$MAX_MB" ]; then
      MAX_MB=$FREE_MB
      WHY="$((AVAIL_KB / 1024)) MiB free less $RESERVE_MB MiB for SteamVR"
    fi
  fi
  if [ -n "$(num "$AZAHARFRAME_MEMORY_MAX_MB" "")" ]; then
    MAX_MB=$AZAHARFRAME_MEMORY_MAX_MB
    WHY="AZAHARFRAME_MEMORY_MAX_MB"
  elif [ "$MAX_MB" -lt "$FLOOR_MB" ]; then
    MAX_MB=$FLOOR_MB
    WHY="the floor; only $((${AVAIL_KB:-0} / 1024)) MiB free, memory is tight"
  fi
  HIGH_MB=$((MAX_MB - MAX_MB / 12))
  UNIT=azaharframe-$$
  # A trial scope first: systemd-run's own failure and the emulator's exit status look alike.
  if PROBE=$("$SYSTEMD_RUN" --user --scope --quiet --collect -p MemoryMax=256M true 2>&1); then
    # Not --collect: a scope the kernel killed for memory stays (failed) until its result is
    # read below. Ones a wrapper never got to read (it was killed too) are cleared here.
    systemctl --user reset-failed 'azaharframe-*.scope' > /dev/null 2>&1
    SCOPE=("$SYSTEMD_RUN" --user --scope --quiet --unit="$UNIT"
           --description="Azahar for Frame" -p MemoryHigh="${HIGH_MB}M" -p MemoryMax="${MAX_MB}M"
           -p MemorySwapMax="${SWAP_MB}M" --)
    echo "memory: scope $UNIT.scope, MemoryMax ${MAX_MB} MiB ($WHY), MemoryHigh ${HIGH_MB} MiB, swap ${SWAP_MB} MiB"
  else
    echo "memory: systemd-run --user failed ($(echo "$PROBE" | head -n 1)), running without a memory limit"
  fi
fi

# Not exec: the wrapper stays to remove the marker.  A signal for the wrapper
# (Steam stopping the app) goes on to the emulator.
"${SCOPE[@]}" "$HERE/azahar" "${ARGS[@]}" "${FWD[@]}" &
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
# AzaharFrame doubling: an A/B launch keeps its emulator log (the next start rotates it away).
if [ -n "$AB_LABEL" ] && [ -f "$DATA/user/log/azahar_log.txt" ]; then
  mkdir -p "$DATA/ab-logs" &&
    cp -p "$DATA/user/log/azahar_log.txt" "$DATA/ab-logs/$(date +%Y%m%d-%H%M%S)-$AB_LABEL.txt" &&
    echo "session.queue: log kept in ab-logs"
fi
# AzaharFrame memguard: why the scope ended. oom-kill: the emulator ran into its memory limit;
# the next start says so once (out-of-memory holds the limit, in MiB).
if [ ${#SCOPE[@]} -gt 0 ]; then
  RESULT=$(systemctl --user show -p Result --value "$UNIT.scope" 2> /dev/null)
  systemctl --user reset-failed "$UNIT.scope" > /dev/null 2>&1
  if [ "$RESULT" = oom-kill ]; then
    echo "the emulator ran out of memory: killed at its limit of ${MAX_MB} MiB; the next start says so"
    echo "$MAX_MB" > "$DATA/out-of-memory"
  elif [ "$rc" = 137 ]; then
    echo "the emulator was killed (SIGKILL, scope result ${RESULT:-unknown})"
  fi
fi
exit $rc
