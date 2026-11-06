#!/bin/bash
# Builds the mock and runs xrmock_test against it through mockrun.sh.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
TOOLS=$(cd "$HERE/../.." && pwd)
BUILD=${XRMOCK_BUILD:-$HOME/Games/AzaharFrame/build-xrmock}
"$TOOLS/xrmock/build.sh" "$BUILD"
OUT=${1:-$BUILD/test-run}
rm -rf "$OUT"
XRMOCK_TEST_INPUT=1 XRMOCK_BUILD=$BUILD "$TOOLS/mockrun.sh" --bin "$BUILD/xrmock_test" --seconds 20 --display host \
    --script "$HERE/input.script" --dump-frame 60 --out "$OUT"
ls "$OUT"/dumps/sc0_f*.png >/dev/null
grep -q 'test: .*: ok' "$OUT/emulator.log"
echo "xrmock test: ok"
