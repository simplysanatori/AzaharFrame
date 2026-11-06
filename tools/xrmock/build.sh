#!/bin/bash
# Builds the mock OpenXR runtime and its test program:
#   tools/xrmock/build.sh [out dir]
# Writes libxrmock.so, runtime.json (for XR_RUNTIME_JSON) and xrmock_test into
# the out dir (default ~/Games/CitraFrame/build-xrmock).  Standalone: a C
# compiler, the Vulkan headers and the OpenXR headers kept in include/.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
OUT=${1:-$HOME/Games/CitraFrame/build-xrmock}
CC=${CC:-$(command -v clang || command -v gcc)}
XRINC=${OPENXR_INCLUDE:-$HERE/include}
VKINC=${VULKAN_INCLUDE:-}
for d in "$REPO/externals/vulkan-headers/include" "$HOME/Games/CitraFrame/src/externals/vulkan-headers/include" /usr/include; do
    [ -n "$VKINC" ] && break
    [ -f "$d/vulkan/vulkan.h" ] && VKINC=$d
done
[ -f "$XRINC/openxr/openxr.h" ] || { echo "no OpenXR headers in $XRINC" >&2; exit 1; }
[ -f "$VKINC/vulkan/vulkan.h" ] || { echo "no Vulkan headers (set VULKAN_INCLUDE)" >&2; exit 1; }
. "$HERE/lock.sh"
cf_wait_and_lock
mkdir -p "$OUT"
$CC -O1 -g -Wall -Wextra -shared -fPIC -o "$OUT/libxrmock.so" "$HERE/xrmock.c" "$HERE/xrmock_vk.c" \
    -I "$XRINC" -I "$VKINC" -I "$HERE" -lm -lpthread
$CC -O1 -g -Wall -Wextra -o "$OUT/xrmock_test" "$HERE/test/xrmock_test.c" -I "$XRINC" -I "$VKINC" -ldl -lm
sed "s#@LIB@#$OUT/libxrmock.so#" "$HERE/runtime.json.in" > "$OUT/runtime.json"
cf_unlock
echo "$OUT/runtime.json"
