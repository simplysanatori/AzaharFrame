#!/bin/bash
# AzaharFrame flat2: VR -> Flat -> VR three times (and back to Flat once more) against xrmock, with
# the controllers driving every switch: the VR chord (both bumpers + View + Menu) through the mock's
# input script in each VR session, and Menu held 3 s + A ("Switch to VR?") through the test gamepad
# in each Flat stint. MOCK_XR_FOCUS_ONCE=1 makes the mock act as SteamVR does: only the first
# session of an XrInstance gets controller input, so an app that reuses its instance gets stuck in
# the second VR session (and the mock logs an ERROR).
#
#   steam-frame/test/vr-flat-roundtrip.sh --bin /abs/path/to/azahar [--out DIR]
#
# Needs: the headset not in use (mockrun waits), xvfb-run. Uses its own xrmock build
# (XRMOCK_BUILD, default ~/Games/AzaharFrame/build-xrmock-flat2) made from this tree's tools/xrmock.
# Exit 0 when every check passed.
set -u
HERE=$(dirname "$(readlink -f "$0")")
REPO=$(readlink -f "$HERE/../..")
BIN="" OUT=""
while [ $# -gt 0 ]; do
    case "$1" in
        --bin) BIN=$2; shift 2 ;;
        --out) OUT=$2; shift 2 ;;
        *) echo "usage: $0 --bin /abs/path/azahar [--out DIR]" >&2; exit 2 ;;
    esac
done
[ -x "$BIN" ] || { echo "no binary at $BIN (--bin)" >&2; exit 2; }
CF=$HOME/Games/AzaharFrame
[ -n "$OUT" ] || OUT=$CF/reports/mock/roundtrip-$(date +%Y%m%d-%H%M%S)
export XRMOCK_BUILD=${XRMOCK_BUILD:-$CF/build-xrmock-flat2}
"$REPO/tools/xrmock/build.sh" "$XRMOCK_BUILD" > /dev/null || { echo "xrmock build failed" >&2; exit 2; }
MOCKRUN=${MOCKRUN:-$CF/mockgo.sh}
[ -x "$MOCKRUN" ] || MOCKRUN=$REPO/tools/mockrun.sh

T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
export AZAHARFRAME_MARK=$T/headset-in-use  # AzaharFrame integ3: never the real marker
# VR: seconds since the first session began (about 0.5 s after the start). The chord pressed for
# 0.2 s, once in each of the four VR sessions.
cat > "$T/vr.script" << 'EOF'
4.0  /user/hand/left/input/bumper/click  1
4.0  /user/hand/right/input/bumper/click 1
4.0  /user/hand/left/input/view/click    1
4.0  /user/hand/right/input/menu/click   1
4.2  /user/hand/left/input/bumper/click  0
4.2  /user/hand/right/input/bumper/click 0
4.2  /user/hand/left/input/view/click    0
4.2  /user/hand/right/input/menu/click   0
15.0 /user/hand/left/input/bumper/click  1
15.0 /user/hand/right/input/bumper/click 1
15.0 /user/hand/left/input/view/click    1
15.0 /user/hand/right/input/menu/click   1
15.2 /user/hand/left/input/bumper/click  0
15.2 /user/hand/right/input/bumper/click 0
15.2 /user/hand/left/input/view/click    0
15.2 /user/hand/right/input/menu/click   0
26.5 /user/hand/left/input/bumper/click  1
26.5 /user/hand/right/input/bumper/click 1
26.5 /user/hand/left/input/view/click    1
26.5 /user/hand/right/input/menu/click   1
26.7 /user/hand/left/input/bumper/click  0
26.7 /user/hand/right/input/bumper/click 0
26.7 /user/hand/left/input/view/click    0
26.7 /user/hand/right/input/menu/click   0
38.0 /user/hand/left/input/bumper/click  1
38.0 /user/hand/right/input/bumper/click 1
38.0 /user/hand/left/input/view/click    1
38.0 /user/hand/right/input/menu/click   1
38.2 /user/hand/left/input/bumper/click  0
38.2 /user/hand/right/input/bumper/click 0
38.2 /user/hand/left/input/view/click    0
38.2 /user/hand/right/input/menu/click   0
EOF
# Flat: seconds since the start. Menu held 3.5 s (the question at 3 s), then A: Yes.
cat > "$T/pad.txt" << 'EOF'
0    menu 0
7.0  menu 1
10.5 menu 0
11.0 a 1
11.3 a 0
18.5 menu 1
22.0 menu 0
22.5 a 1
22.8 a 0
30.0 menu 1
33.5 menu 0
34.0 a 1
34.3 a 0
EOF

MOCK_XR_FOCUS_ONCE=1 AZAHARFRAME_TEST_FLAT_PAD=$T/pad.txt \
    "$MOCKRUN" --bin "$BIN" --seconds 44 --script "$T/vr.script" --out "$OUT" > "$OUT.out" 2>&1
RC=$?
LOG=$OUT/app_azahar_log.txt
TRACE=$OUT/trace.txt
FAILS=0
check() { # NAME WANT GOT
    if [ "$2" = "$3" ]; then echo "ok   $1 ($3)"; else echo "FAIL $1: want $2, got $3"; FAILS=$((FAILS + 1)); fi
}
count() { grep -ac -- "$1" "$2" 2>/dev/null || true; }
echo "mock run: $OUT (mockrun exit $RC)"
[ -f "$LOG" ] || { echo "FAIL no app log in $OUT"; exit 1; }
check "VR sessions made" 4 "$(count 'xr: session made (session 1 of this XR instance)' "$LOG")"
check "  none on a used instance" 0 "$(count 'xr: session made (session [2-9]' "$LOG")"
check "XR instances made by the mock" 4 "$(count 'createInstance extensions' "$TRACE")"
check "XR instances destroyed for Flat" 4 "$(count 'xr: XR instance destroyed' "$LOG")"
check "VR -> Flat by the VR chord" 4 "$(count 'both bumpers + View + Menu (home): switching' "$LOG")"
check "Flat -> VR by Menu held + A" 3 "$(count 'Switch to VR? A: Yes' "$LOG")"
check "Switch to VR? asked" 3 "$(count 'Menu held 3 s: asking Switch to VR?' "$LOG")"
check "VR again (after Flat)" 3 "$(count 'xr: VR again: making a session' "$LOG")"
check "sessions ended for Flat" 4 "$(count 'xr: ending the session (flat)' "$LOG")"
check "no session refused" 0 "$(count 'xr: no session, staying flat' "$LOG")"
check "mock ERROR lines (input focus included)" 0 "$(count ' ERROR ' "$TRACE")"
check "the frame loop ended at the stop" 1 "$(count 'xr: frame loop ended' "$LOG")"
check "  not left behind" 0 "$(count 'did not stop within' "$LOG")"
[ "$FAILS" = 0 ] && echo "all passed" || { echo "$FAILS failed"; exit 1; }
