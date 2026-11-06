#!/bin/bash
# Runs the CitraFrame binary against the mock OpenXR runtime (tools/xrmock)
# and collects what it did.
#
#   mockrun.sh [--bin PATH] [--rom PATH] [--seconds N] [--script FILE]
#              [--out DIR] [--vvl] [--display xvfb|offscreen|host]
#              [--hz N] [--busy MS] [--dump-frame N|last] [-- extra args]
#
# Runs `BIN --vr ROM [extra args]` under XR_RUNTIME_JSON pointing at the mock
# for N seconds of wall time (default 20), then ends it with SIGTERM (SIGKILL
# after 15 s).  Writes into DIR (default
# ~/Games/CitraFrame/reports/mock/<timestamp>/): emulator.log, trace.txt (the
# mock's trace), dumps/*.png (swapchain images, MOCK_XR_DUMP_*), and
# summary.txt.  Exit status 0 when the run passed (see RESULT in summary.txt).
#
# Waits first (polling every 5 s) while ~/.cache/headset-in-use exists, a
# lasrecomp or Steam game is running, or another run holds
# ~/.cache/af-bench-lock; the lock is held for the run itself only.
#
# --display: xvfb (default; Qt's xcb platform on a private X server),
# offscreen (QT_QPA_PLATFORM=offscreen, which may have no Vulkan surface), or
# host (use the current DISPLAY / session as it is).
# --vvl turns on the Khronos validation layer when it is installed.
# --busy MS sets MOCK_XR_BUSY_MS (error on a release that waits for the GPU).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
XRM=$HERE/xrmock
CF=$HOME/Games/CitraFrame
BIN=$CF/build-xrcore/bin/Release/azahar
BUILD=${XRMOCK_BUILD:-$CF/build-xrmock}
ROM="" SECS=20 SCRIPT="" OUT="" VVL=0 DISP=xvfb HZ="" BUSY="" DUMPF=last

while [ $# -gt 0 ]; do
    case "$1" in
        --bin) BIN=$2; shift 2 ;;
        --rom) ROM=$2; shift 2 ;;
        --seconds) SECS=$2; shift 2 ;;
        --script) SCRIPT=$2; shift 2 ;;
        --out) OUT=$2; shift 2 ;;
        --vvl) VVL=1; shift ;;
        --display) DISP=$2; shift 2 ;;
        --hz) HZ=$2; shift 2 ;;
        --busy) BUSY=$2; shift 2 ;;
        --dump-frame) DUMPF=$2; shift 2 ;;
        --) shift; break ;;
        -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
        *) echo "mockrun: unknown option $1 (extra arguments go after --)" >&2; exit 2 ;;
    esac
done
EXTRA=("$@")

[ -x "$BIN" ] || { echo "mockrun: no binary at $BIN (--bin)" >&2; exit 2; }
[ -z "$ROM" ] || [ -f "$ROM" ] || { echo "mockrun: no ROM at $ROM" >&2; exit 2; }
[ -z "$SCRIPT" ] || [ -f "$SCRIPT" ] || { echo "mockrun: no script at $SCRIPT" >&2; exit 2; }
if [ ! -f "$BUILD/libxrmock.so" ] || [ ! -f "$BUILD/runtime.json" ]; then
    "$XRM/build.sh" "$BUILD" || exit 2
fi
[ -n "$OUT" ] || OUT=$CF/reports/mock/$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT/dumps" || exit 2
SCRIPT_ABS=""; [ -z "$SCRIPT" ] || SCRIPT_ABS=$(readlink -f "$SCRIPT")

CMD=("$BIN" --vr)
[ -z "$ROM" ] || CMD+=("$ROM")
CMD+=("${EXTRA[@]}")
case "$DISP" in
    xvfb)
        command -v xvfb-run >/dev/null || { echo "mockrun: xvfb-run not found (--display offscreen|host)" >&2; exit 2; }
        CMD=(xvfb-run -a -s "-screen 0 1280x720x24" "${CMD[@]}")
        export QT_QPA_PLATFORM=xcb ;;
    offscreen) export QT_QPA_PLATFORM=offscreen ;;
    host) ;;
    *) echo "mockrun: --display must be xvfb, offscreen or host" >&2; exit 2 ;;
esac

export XR_RUNTIME_JSON=$BUILD/runtime.json
export MOCK_XR_TRACE=$OUT/trace.txt
export MOCK_XR_SECONDS=${MOCK_XR_SECONDS:-0}
export MOCK_XR_DUMP_DIR=$OUT/dumps
export MOCK_XR_DUMP_FRAME=$DUMPF
[ -z "$SCRIPT_ABS" ] || export MOCK_XR_INPUT_SCRIPT=$SCRIPT_ABS
[ -z "$HZ" ] || export MOCK_XR_HZ=$HZ
[ -z "$BUSY" ] || export MOCK_XR_BUSY_MS=$BUSY
VVLNOTE=""
if [ "$VVL" = 1 ]; then
    if ls /usr/share/vulkan/explicit_layer.d /usr/share/vulkan/implicit_layer.d /etc/vulkan/explicit_layer.d 2>/dev/null | grep -qi validation; then
        export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
        VVLNOTE="validation layer on"
    else
        VVLNOTE="validation layer NOT installed, run without it"
        VVL=0
        echo "mockrun: $VVLNOTE" >&2
    fi
fi

. "$XRM/lock.sh"
cf_wait_and_lock --games
START=$(date +%s)
MARK=$OUT/.start; : > "$MARK"
setsid "${CMD[@]}" > "$OUT/emulator.log" 2>&1 < /dev/null &
PID=$!
STATUS=""
for ((i = 0; i < SECS * 2; i++)); do
    if ! kill -0 "$PID" 2>/dev/null; then break; fi
    sleep 0.5
done
if kill -0 "$PID" 2>/dev/null; then
    EARLY=0
    kill -TERM -- "-$PID" 2>/dev/null || kill -TERM "$PID" 2>/dev/null
    for ((i = 0; i < 30; i++)); do kill -0 "$PID" 2>/dev/null || break; sleep 0.5; done
    if kill -0 "$PID" 2>/dev/null; then
        kill -KILL -- "-$PID" 2>/dev/null || kill -KILL "$PID" 2>/dev/null
        STATUS=killed
    fi
else
    EARLY=1
fi
wait "$PID" 2>/dev/null; RC=$?
[ -n "$STATUS" ] || STATUS=$RC
END=$(date +%s)
cf_unlock

# The emulator's own log, when it writes one in the usual place.
for d in "$HOME/.local/share/azahar-emu/log" "$HOME/.local/share/CitraFrame/log"; do
    for f in "$d"/*.txt; do
        [ -f "$f" ] && [ "$f" -nt "$MARK" ] && cp "$f" "$OUT/app_$(basename "$f")"
    done
done
rm -f "$MARK"

# ---- summary ----
S=$OUT/summary.txt
T=$OUT/trace.txt
ERRS=$(grep -c ' ERROR ' "$T" 2>/dev/null); ERRS=${ERRS:-0}
WARNS=$(grep -c ' WARN ' "$T" 2>/dev/null); WARNS=${WARNS:-0}
VUID=$(cat "$OUT"/emulator.log "$OUT"/app_*.txt 2>/dev/null | grep -c 'VUID-'); VUID=${VUID:-0}
NSEC=$(grep -c ' summary t=' "$T" 2>/dev/null); NSEC=${NSEC:-0}
RELTOTAL=$(awk '/ total sc[0-9]+ /{for(i=1;i<=NF;i++) if($i ~ /^releases=/){split($i,a,"="); n+=a[2]}} END{print n+0}' "$T" 2>/dev/null)
{
    echo "mockrun $(date -d "@$START" '+%Y-%m-%d %H:%M:%S'), ${SECS}s requested, $((END - START))s elapsed"
    echo "command: ${CMD[*]}"
    echo "display: $DISP${VVLNOTE:+; $VVLNOTE}${SCRIPT:+; input script $SCRIPT}"
    echo "exit status: $STATUS$([ "$EARLY" = 1 ] && echo ' (exited before the time was up)')"
    echo
    echo "== mock ($T) =="
    echo "ERROR lines: $ERRS, WARN lines: $WARNS, per-second summaries: $NSEC"
    grep -m 10 ' ERROR ' "$T" 2>/dev/null | sed 's/^/  /'
    grep -m 5 ' WARN ' "$T" 2>/dev/null | sed 's/^/  /'
    echo "per-second averages (the first and last second left out when there are more than 3):"
    awk '/ summary t=/{n++; for(i=1;i<=NF;i++){split($i,a,"="); if(a[1]=="waits")w[n]=a[2]; if(a[1]=="ends")e[n]=a[2]; if(a[1]=="quads")q[n]=a[2]; if($i ~ /^sc[0-9]+=/) r[n","a[1]]=a[2]; if($i ~ /^sc[0-9]+=/) names[a[1]]=1}}
         END{lo=1; hi=n; if(n>3){lo=2; hi=n-1} c=hi-lo+1; if(c<1){print "  (no full second recorded)"; exit}
             for(k=lo;k<=hi;k++){sw+=w[k]; se+=e[k]; sq+=q[k]; for(m in names) sr[m]+=r[k","m]}
             printf "  waitFrame %.1f/s, endFrame %.1f/s, quad layers per endFrame %.2f\n", sw/c, se/c, se? sq/se : 0;
             for(m in names) printf "  releases %s %.1f/s\n", m, sr[m]/c}' "$T" 2>/dev/null
    grep ' total ' "$T" 2>/dev/null | sed 's/^f[0-9]* /  /'
    grep -h '^xrmock:' "$OUT/emulator.log" 2>/dev/null | sed 's/^/  /'
    echo
    echo "== emulator log =="
    echo "validation messages (VUID): $VUID"
    echo "lines with error/critical: $(grep -ciE '\[(error|critical)\]|^error|critical' "$OUT/emulator.log" 2>/dev/null)"
    echo
    echo "== dumps =="
    ls -l "$OUT/dumps" 2>/dev/null | awk 'NR>1{print "  " $5 " " $9}'
    echo
    BAD=""
    [ "$ERRS" -gt 0 ] && BAD="$BAD mock errors;"
    if [ "$VVL" = 1 ] && [ "$VUID" -gt 0 ]; then BAD="$BAD validation errors;"; fi
    case "$STATUS" in 0|143|15) ;; *) BAD="$BAD exit status $STATUS;" ;; esac
    [ "$RELTOTAL" -gt 0 ] || BAD="$BAD no swapchain image was released;"
    if [ -z "$BAD" ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL:$BAD"; fi
} > "$S"
cat "$S"
grep -q '^RESULT: PASS' "$S"
