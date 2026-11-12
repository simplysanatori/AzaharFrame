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

# AzaharFrame memguard: the memory scope. The stand-in writes what it sees: its cgroup, that
# cgroup's memory.max, its folder, an exported variable; it exits 7.
cat > "$T/app/azahar" << SH
#!/bin/bash
cg=\$(sed -n 's/^0:://p' /proc/self/cgroup)
echo "\$cg" > "$T/cg"
cat "/sys/fs/cgroup\$cg/memory.max" > "$T/max" 2> /dev/null || echo none > "$T/max"
cat "/sys/fs/cgroup\$cg/memory.swap.max" > "$T/swap" 2> /dev/null || echo none > "$T/swap"
pwd > "$T/cwd"
echo "\$AZAHARFRAME_MODE" > "$T/env"
[ -e "\$AZAHARFRAME_MARK" ] && echo yes > "$T/seen" || echo no > "$T/seen"
exit 7
SH
chmod +x "$T/app/azahar"
launch() { # [VAR=value...]: one launch with the stand-in, Flat
  rm -f "$T/cg" "$T/max" "$T/swap" "$T/cwd" "$T/env" "$T/seen"
  env -u AZAHARFRAME_FLAT -u AZAHARFRAME_TEST_MODE_ONLY -u SteamAppId -u AZAHARFRAME_MEMORY_SCOPE \
    -u AZAHARFRAME_MEMORY_MAX_PCT -u AZAHARFRAME_MEMORY_RESERVE_MB -u AZAHARFRAME_MEMORY_FLOOR_MB \
    -u AZAHARFRAME_MEMORY_MAX_MB -u AZAHARFRAME_MEMORY_SWAP_MB \
    AZAHARFRAME_HOME="$T" AZAHARFRAME_MEMINFO="$T/meminfo" XR_RUNTIME_JSON=/dev/null "$@" \
    bash "$T/app/azaharframe.sh" --flat > /dev/null 2>&1
}
meminfo() { printf 'MemTotal: %s kB\nMemAvailable: %s kB\n' "$1" "$2" > "$T/meminfo"; }
MiB=1048576
if command -v systemd-run > /dev/null && systemd-run --user --scope --quiet --collect true 2> /dev/null; then
  meminfo 16000000 16000000
  launch
  check "scope: the emulator runs in an azaharframe scope" yes "$(grep -q '/azaharframe-[0-9]*\.scope$' "$T/cg" && echo yes || echo "no ($(cat "$T/cg"))")"
  check "  MemoryMax is 65 % of RAM when most is free" "$((10156 * MiB))" "$(cat "$T/max")"
  check "  launch.log says so" 1 "$(grep -c '^memory: scope azaharframe-' "$T/launch.log")"
  check "  2048 MiB of swap" "$((2048 * MiB))" "$(cat "$T/swap")"
  check "  the folder is the data folder" "$T" "$(cat "$T/cwd")"
  check "  the environment is kept" flat "$(cat "$T/env")"
  check "  the test marker was there" yes "$(cat "$T/seen")"
  check "  the exit status comes through" 1 "$(grep -c '^exit status 7$' "$T/launch.log")"
  check "  the test marker is gone after" no "$([ -e "$AZAHARFRAME_MARK" ] && echo yes || echo no)"
  launch AZAHARFRAME_MEMORY_MAX_PCT=50
  check "AZAHARFRAME_MEMORY_MAX_PCT=50" "$((7812 * MiB))" "$(cat "$T/max")"
  launch AZAHARFRAME_MEMORY_MAX_PCT=99 AZAHARFRAME_MEMORY_RESERVE_MB=0
  check "  99 is held to 90" "$((14062 * MiB))" "$(cat "$T/max")"
  meminfo 16000000 12000000
  launch
  check "11718 MiB free: MemoryMax is that less 2048 for SteamVR" "$((9670 * MiB))" "$(cat "$T/max")"
  check "  logged" 1 "$(grep -c '(11718 MiB free less 2048 MiB for SteamVR)' "$T/launch.log")"
  launch AZAHARFRAME_MEMORY_RESERVE_MB=4096
  check "  AZAHARFRAME_MEMORY_RESERVE_MB=4096" "$((7622 * MiB))" "$(cat "$T/max")"
  meminfo 16000000 4000000
  launch
  check "3906 MiB free: the floor, 4096 MiB" "$((4096 * MiB))" "$(cat "$T/max")"
  check "  logged as tight" 1 "$(grep -c 'memory is tight' "$T/launch.log")"
  launch AZAHARFRAME_MEMORY_FLOOR_MB=2048
  check "  AZAHARFRAME_MEMORY_FLOOR_MB=2048" "$((2048 * MiB))" "$(cat "$T/max")"
  meminfo 16000000 16000000
  check "  a clean run leaves no out-of-memory file" no "$([ -e "$T/out-of-memory" ] && echo yes || echo no)"
  # A stand-in that runs into a 64 MiB limit: the scope says oom-kill, the next start is told.
  printf '#!/bin/bash\nexec python3 -I -c "b = bytearray(300 * 1024 * 1024); b[::4096] = bytes(len(b[::4096]) * [1])"\n' > "$T/app/azahar"
  launch AZAHARFRAME_MEMORY_MAX_MB=64 AZAHARFRAME_MEMORY_SWAP_MB=0
  check "out of memory at a 64 MiB limit: the exit status is 137" 1 "$(grep -c '^exit status 137$' "$T/launch.log")"
  check "  logged" 1 "$(grep -c 'ran out of memory: killed at its limit of 64 MiB' "$T/launch.log")"
  check "  out-of-memory holds the limit" 64 "$(cat "$T/out-of-memory" 2> /dev/null)"
  check "  no failed scope is left" 0 "$(systemctl --user list-units --all --no-legend 'azaharframe-*.scope' 2> /dev/null | grep -c failed)"
  rm -f "$T/out-of-memory"
  # A stand-in that waits: TERM to the wrapper (Steam's Stop) reaches it inside the scope.
  printf '#!/bin/bash\ntrap "echo term > %s/term; exit 0" TERM\nwhile :; do sleep 0.1; done\n' "$T" > "$T/app/azahar"
  rm -f "$T/term"
  env -u AZAHARFRAME_FLAT -u AZAHARFRAME_TEST_MODE_ONLY -u SteamAppId AZAHARFRAME_HOME="$T" \
    XR_RUNTIME_JSON=/dev/null bash "$T/app/azaharframe.sh" --flat > /dev/null 2>&1 &
  W=$!
  for _ in $(seq 50); do pgrep -f "$T/app/azahar" > /dev/null && break; sleep 0.1; done
  sleep 0.5
  kill -TERM "$W"; wait "$W"
  check "  TERM to the wrapper reaches the emulator in its scope" term "$(cat "$T/term" 2> /dev/null)"
  printf '#!/bin/bash\nsed -n "s/^0:://p" /proc/self/cgroup > %s/cg\n' "$T" > "$T/app/azahar"
else
  echo "skip scope checks: systemd-run --user does not work here"
  printf '#!/bin/bash\nsed -n "s/^0:://p" /proc/self/cgroup > %s/cg\n' "$T" > "$T/app/azahar"
fi
launch AZAHARFRAME_MEMORY_SCOPE=0
check "AZAHARFRAME_MEMORY_SCOPE=0: no scope" no "$(grep -q 'azaharframe-' "$T/cg" && echo yes || echo no)"
check "  logged" 1 "$(grep -c '^memory: no scope' "$T/launch.log")"
launch AZAHARFRAME_SYSTEMD_RUN=/nonexistent/systemd-run
check "no systemd-run: runs directly" no "$(grep -q 'azaharframe-' "$T/cg" 2> /dev/null && echo yes || echo no)"
check "  the emulator ran" yes "$([ -s "$T/cg" ] && echo yes || echo no)"
check "  logged" 1 "$(grep -c 'not found, running without a memory limit' "$T/launch.log")"
printf '#!/bin/bash\necho "Failed to connect to bus" >&2\nexit 1\n' > "$T/fake-systemd-run"
chmod +x "$T/fake-systemd-run"
launch AZAHARFRAME_SYSTEMD_RUN="$T/fake-systemd-run"
check "systemd-run fails: runs directly" yes "$([ -s "$T/cg" ] && echo yes || echo no)"
check "  logged with its reason" 1 "$(grep -c 'systemd-run --user failed (Failed to connect to bus)' "$T/launch.log")"

# AzaharFrame memguard: a headset marker from before this boot is taken over and removed; one from
# this boot is someone else's and stays.
BOOT=$(awk '/^btime / {print $2}' /proc/stat)
: > "$AZAHARFRAME_MARK"; touch -d "@$((BOOT - 60))" "$AZAHARFRAME_MARK"
launch AZAHARFRAME_MEMORY_SCOPE=0
check "a marker from before this boot: taken over" 1 "$(grep -c 'left from before this boot, taken over' "$T/launch.log")"
check "  and removed at the end" no "$([ -e "$AZAHARFRAME_MARK" ] && echo yes || echo no)"
: > "$AZAHARFRAME_MARK"
launch AZAHARFRAME_MEMORY_SCOPE=0
check "a marker from this boot: left alone" yes "$([ -e "$AZAHARFRAME_MARK" ] && echo yes || echo no)"
check "  not taken over" 0 "$(grep -c 'taken over' "$T/launch.log")"
rm -f "$AZAHARFRAME_MARK"
check "the real headset marker is still untouched" "$MARK_BEFORE" "$(mark_state)"
[ "$FAILS" = 0 ] && echo "all passed" || { echo "$FAILS failed"; exit 1; }
