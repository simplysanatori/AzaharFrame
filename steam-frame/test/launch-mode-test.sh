#!/bin/bash
# AzaharFrame flat: the start mode azaharframe.sh picks, with the crash safety net (the file
# flat-running left by a Flat run that did not quit cleanly: VR once). Uses a throwaway data
# folder; runs no emulator (AZAHARFRAME_TEST_MODE_ONLY).   test/launch-mode-test.sh
HERE=$(dirname "$(readlink -f "$0")")/..
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
FAILS=0
# AzaharFrame integ3: the real headset marker must be left as it is (other work on the Frame
# waits on it); the launcher is pointed at a throwaway one.
REAL_MARK=$HOME/.cache/headset-in-use
mark_state() { stat -c '%Y %i' "$REAL_MARK" 2> /dev/null || echo none; }
MARK_BEFORE=$(mark_state)
export AZAHARFRAME_MARK=$T/headset-in-use
# run [ARGS...]: the mode launch.log names
run() {
  env -u AZAHARFRAME_FLAT AZAHARFRAME_HOME="$T" AZAHARFRAME_TEST_MODE_ONLY=1 \
    bash "$HERE/azaharframe.sh" "$@" > /dev/null 2>&1
  sed -n 's/^mode: \(vr\|flat\)$/\1/p' "$T/launch.log"
}
check() { # NAME WANT GOT
  if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: want $2, got $3"; FAILS=$((FAILS + 1)); fi
}
echo flat > "$T/mode"
check "Flat last, quit cleanly: Flat" flat "$(run)"
: > "$T/flat-running"
check "Flat last, not quit cleanly: VR" vr "$(run)"
check "  and it says why" 1 "$(grep -c 'did not quit cleanly' "$T/launch.log")"
check "  the marker is gone" no "$([ -e "$T/flat-running" ] && echo yes || echo no)"
check "  the mode file still says flat" flat "$(cat "$T/mode")"
check "the launch after that: Flat again" flat "$(run)"
echo vr > "$T/mode"; : > "$T/flat-running"
check "VR last with a marker: VR" vr "$(run)"
echo flat > "$T/mode"; : > "$T/flat-running"
check "--flat with a marker: Flat (an override)" flat "$(run --flat)"
rm -f "$T/mode"
check "no mode file: VR" vr "$(run)"
# A whole launch with a stand-in emulator (a copy of the wrapper beside a script called azahar):
# the marker is AZAHARFRAME_MARK while it runs and gone after, the real one is left alone.
mkdir -p "$T/app" && cp "$HERE/azaharframe.sh" "$T/app/"
printf '#!/bin/bash\n[ -e "$AZAHARFRAME_MARK" ] && echo yes > "%s/seen" || echo no > "%s/seen"\n' "$T" "$T" > "$T/app/azahar"
chmod +x "$T/app/azahar"
env -u AZAHARFRAME_FLAT -u AZAHARFRAME_TEST_MODE_ONLY -u SteamAppId AZAHARFRAME_HOME="$T" XR_RUNTIME_JSON=/dev/null \
  bash "$T/app/azaharframe.sh" --flat > /dev/null 2>&1
check "a launch: the stand-in emulator ran with the test marker made" yes "$(cat "$T/seen" 2> /dev/null)"
check "  and the test marker is gone after" no "$([ -e "$AZAHARFRAME_MARK" ] && echo yes || echo no)"
check "the real headset marker is untouched" "$MARK_BEFORE" "$(mark_state)"
[ "$FAILS" = 0 ] && echo "all passed" || { echo "$FAILS failed"; exit 1; }
