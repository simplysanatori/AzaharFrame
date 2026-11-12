#!/bin/bash
# AzaharFrame flat2 / flatfix: VR -> Flat -> VR three times (and back to Flat once more) against
# xrmock, with 15 s and more of gamepad input in each Flat stint and each way back to VR used once:
#   Flat 1 ends with Menu held 3 s + A ("Switch to VR?"),
#   Flat 2 with the status bar's VR button, pressed twice quickly, while the window's frame never
#     comes back into the dashboard (a fake steam-cef.py that never answers: VR after 5 s anyway),
#   Flat 3 with the chord (both bumpers + View + Menu) on the gamepad.
# For every round: the way back was taken, a new session was made and reached FOCUSED, and the
# controllers reached it (its VR chord came through).
# Each VR session is left with the chord through the mock's input script, which only works when
# the new session gets controller input. MOCK_XR_FOCUS_ONCE=1 makes the mock act as SteamVR does:
# only the first session of an XrInstance gets controller input, so an app that reuses its
# instance gets stuck in the second VR session (and the mock logs an ERROR).
#
# The Flat gamepad goes through SDL (AZAHARFRAME_TEST_FLAT_PAD_SDL=1, citra_qt/frame_mode.cpp):
# an SDL virtual game controller plays the script, and input_common's SDL thread and the frame
# loop read it as they read Steam Input's pad. In each stint: D-pad taps and holds (the game
# list's steps and repeats), the left stick up and down, X, Y, B, a trigger, and in Flat 1 and 3
# the pad taken away and brought back (as Steam Input re-creates its pad). The frame loop must
# log its flat line every second all along (owner's report 2026-11-12: it stopped in Flat after
# game list input and VR never came back), and the watchdog must stay quiet.
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
# VR (seconds since the first session began, about 0.6 s after the start): the chord for 0.2 s
# once in each of the four VR sessions (they begin at about 0.6, 22, 49 and 72 s).
vr_chord() { # AT
    local p
    for p in left/input/bumper right/input/bumper left/input/view right/input/menu; do
        echo "$1 /user/hand/${p}/click 1"
    done
    for p in left/input/bumper right/input/bumper left/input/view right/input/menu; do
        echo "$(echo "$1 + 0.2" | bc) /user/hand/${p}/click 0"
    done
}
{ vr_chord 3.0; vr_chord 26.0; vr_chord 54.0; vr_chord 77.0; } > "$T/vr.script"

# Flat (seconds since the start): 15 s of input from T on.
flat_input() { # T RECONNECT
    local t=$1
    at() { echo "$(echo "$t + $1" | bc) $2 $3"; }
    at 0.0 up 1;    at 0.15 up 0
    at 0.5 down 1;  at 0.65 down 0
    at 1.0 down 1;  at 2.6 down 0       # held: the list's repeat
    at 3.0 up 1;    at 3.9 up 0
    at 4.5 ly 1;    at 5.4 ly 0         # left stick up, then down
    at 6.0 ly -1;   at 6.9 ly 0
    at 7.5 lx 0.6;  at 8.0 lx 0
    at 8.5 x 1;     at 8.7 x 0
    at 9.0 y 1;     at 9.2 y 0
    at 9.5 b 1;     at 9.7 b 0
    at 10.0 rt 1;   at 10.3 rt 0
    if [ "$2" = 1 ]; then
        at 10.6 attach 0; at 11.6 attach 1
    fi
    local k
    for k in 0 1 2; do
        at "12.0 + $k * 0.4" down 1; at "12.15 + $k * 0.4" down 0
        at "13.2 + $k * 0.4" up 1;   at "13.35 + $k * 0.4" up 0
    done
    at 14.5 rx 0.8; at 15.0 rx 0
}
{
    echo "0 menu 0"
    flat_input 4.5 1                      # Flat 1 (from about 3.7 s)
    echo "18.0 menu 1"; echo "21.5 menu 0" # the question at 21 s
    echo "22.0 a 1"; echo "22.3 a 0"       # Yes
    flat_input 28.0 0                     # Flat 2 (from about 27 s)
    echo "44.0 vrbutton 1"; echo "44.3 vrbutton 1" # twice quickly; the dock never answers
    flat_input 56.5 1                     # Flat 3 (from about 55 s)
    for c in lb rb view menu; do echo "72.0 $c 1"; done
    for c in lb rb view menu; do echo "72.3 $c 0"; done
} > "$T/pad.txt"

# The window's frame (steam-cef.py): a fake that answers at once, but never for the second move
# back into the dashboard (Flat 2's way back).
MOCK_XR_FOCUS_ONCE=1 AZAHARFRAME_TEST_FLAT_PAD=$T/pad.txt AZAHARFRAME_TEST_FLAT_PAD_SDL=1 \
    AZAHARFRAME_TEST_CEF=$HERE/fake-steam-cef.py AZ_FAKE_CEF_COUNT=$T/cef-count \
    AZ_FAKE_CEF_HANG_DASHBOARD=2 \
    "$MOCKRUN" --bin "$BIN" --seconds 82 --script "$T/vr.script" --out "$OUT" > "$OUT.out" 2>&1
RC=$?
LOG=$OUT/app_azahar_log.txt
# A second, short-lived process may rotate the log at the end: the run's own one says "xr: started".
grep -aq 'xr: started' "$LOG" 2>/dev/null || [ ! -f "$OUT/app_azahar_log.old.txt" ] || LOG=$OUT/app_azahar_log.old.txt
TRACE=$OUT/trace.txt
FAILS=0
check() { # NAME WANT GOT
    if [ "$2" = "$3" ]; then echo "ok   $1 ($3)"; else echo "FAIL $1: want $2, got $3"; FAILS=$((FAILS + 1)); fi
}
at_least() { # NAME MIN GOT
    if [ "$3" -ge "$2" ]; then echo "ok   $1 ($3, at least $2)"; else echo "FAIL $1: want at least $2, got $3"; FAILS=$((FAILS + 1)); fi
}
count() { grep -ac -- "$1" "$2" 2>/dev/null || true; }
echo "mock run: $OUT (mockrun exit $RC)"
[ -f "$LOG" ] || { echo "FAIL no app log in $OUT"; exit 1; }
check "VR sessions made" 4 "$(count 'xr: session made (session 1 of this XR instance)' "$LOG")"
check "  none on a used instance" 0 "$(count 'xr: session made (session [2-9]' "$LOG")"
check "  each one FOCUSED" 4 "$(count 'xr: session VISIBLE -> FOCUSED' "$LOG")"
check "XR instances made by the mock" 4 "$(count 'createInstance extensions' "$TRACE")"
check "XR instances destroyed for Flat" 4 "$(count 'xr: XR instance destroyed' "$LOG")"
check "VR -> Flat by the VR chord (input in each session)" 4 "$(count 'both bumpers + View + Menu (home): switching' "$LOG")"
check "Flat -> VR by Menu held + A" 1 "$(count 'Switch to VR? A: Yes' "$LOG")"
check "  Switch to VR? asked" 1 "$(count 'Menu held 3 s: asking Switch to VR?' "$LOG")"
check "Flat -> VR by the status bar's VR button (pressed twice)" 2 "$(count "xr: flat: the status bar's VR button" "$LOG")"
check "  the second press while the first is under way" 1 "$(count 'xr: VR asked for again; the switch is under way' "$LOG")"
check "  the dock that never answered: VR after 5 s anyway" 1 "$(count 'did not come back into the dashboard within 5 s; VR anyway' "$LOG")"
check "Flat -> VR by the chord on the gamepad" 1 "$(count 'both bumpers + View + Menu (flat, no game): switching' "$LOG")"
# Every round in order: the way back, a new session, FOCUSED, then the VR chord (input arrived).
ROUNDS=$(grep -a -e "Switch to VR? A: Yes" -e "xr: flat: the status bar's VR button" \
        -e 'both bumpers + View + Menu (flat, no game)' -e 'xr: VR again: making a session' \
        -e 'xr: session made (session 1' -e 'xr: session VISIBLE -> FOCUSED' \
        -e 'both bumpers + View + Menu (home)' "$LOG" |
    awk '/Switch to VR\? A: Yes/ {route = "Menu held + A"; step = 1; next}
         /status bar.s VR button/ {if (step != 1) {route = "the VR button"; step = 1}; next}
         /\(flat, no game\)/ {route = "the gamepad chord"; step = 1; next}
         /VR again: making a session/ {if (step == 1) step = 2; next}
         /session made/ {if (step == 2) step = 3; next}
         /-> FOCUSED/ {if (step == 3) step = 4; next}
         /\(home\)/ {if (step == 4) {print route; step = 0}; next}')
for r in "Menu held + A" "the VR button" "the gamepad chord"; do
    check "round by $r: new session, FOCUSED, input in VR" 1 "$(printf '%s\n' "$ROUNDS" | grep -cx "$r")"
done
check "VR again (after Flat)" 3 "$(count 'xr: VR again: making a session' "$LOG")"
check "sessions ended for Flat" 4 "$(count 'xr: ending the session (flat)' "$LOG")"
check "no session refused" 0 "$(count 'xr: no session, staying flat' "$LOG")"
check "mock ERROR lines (input focus included)" 0 "$(count ' ERROR ' "$TRACE")"
# The gamepad went through SDL, came back after each time it was taken away, and moved.
check "SDL test pad attached (once, then after each reconnect)" 3 "$(count 'xr test: SDL test pad attached (index [0-9]' "$LOG")"
at_least "  the flat gamepad is the SDL pad" 1 "$(count 'xr: flat: gamepad 045e:028e' "$LOG")"
CHANGES=$(grep -a 'xr: flat 1.0s' "$LOG" | sed -n 's/.*gamepad, \([0-9]*\) changes.*/\1/p' | awk '{n += $1} END {print n + 0}')
at_least "  gamepad changes in the flat lines" 60 "$CHANGES"
# The frame loop kept going: a flat line every second in every Flat stint, no watchdog line.
GAP=$(grep -a -e 'xr: flat 1.0s' -e 'xr: now flat' -e 'xr: VR again: making a session' "$LOG" |
    awk '{t = substr($2, 1, length($2) - 1) + 0}
         /now flat|VR again/ {last = 0; next}
         {if (last > 0 && t - last > max) max = t - last; last = t}
         END {printf "%.0f", max * 1000}')
at_least "flat lines in the three Flat stints" 45 "$(count 'xr: flat 1.0s' "$LOG")"
if [ "${GAP:-0}" -le 1500 ]; then echo "ok   longest gap between flat lines ${GAP} ms"; else echo "FAIL longest gap between flat lines ${GAP} ms (the frame loop stalled)"; FAILS=$((FAILS + 1)); fi
check "watchdog: frame loop stalls in Flat" 0 "$(count 'xr: watchdog: the frame loop has not ticked' "$LOG")"
echo "info game list steps: $(count 'xr: game list: \(up\|down\) to' "$LOG"), list nav: $(grep -a 'xr: game list: controller' "$LOG" | tail -1 | sed 's/.*xr: game list: //')"
echo "info mice: $(grep -a 'xr: mouse:' "$LOG" | sed 's/.*xr: mouse: //' | sort -u | tr '\n' ';')"
check "the frame loop ended at the stop" 1 "$(count 'xr: frame loop ended' "$LOG")"
check "  not left behind" 0 "$(count 'did not stop within' "$LOG")"
[ "$FAILS" = 0 ] && echo "all passed" || { echo "$FAILS failed"; exit 1; }
