#!/bin/bash
# Packages a finished build for the Releases page:
#   steam-frame/release/citraframe-<version>-linux-arm64.tar.gz
#   steam-frame/release/SHA256SUMS
# The tarball holds the stripped emulator (azahar), the launch wrapper, and at the top
# level install.sh, uninstall.sh and steam-cef.py, so the unpacked archive installs itself
# (this is how Frametanium runs it).  Files are packed by name, so settings, saves, keys,
# logs and game files cannot end up in it.
#   steam-frame/release/make-release.sh [version [build-dir]]
# The version defaults to 1.0.0.  The build directory defaults to <repo>/build/bin/Release
# (CITRAFRAME_BUILD names it as well).  Needs no headset and no Steam.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SF=$(cd "$HERE/.." && pwd)
REPO=$(cd "$SF/.." && pwd)
VERSION=${1:-1.0.0}
BUILD=${2:-${CITRAFRAME_BUILD:-$REPO/build/bin/Release}}
[ -x "$BUILD/azahar" ] || { echo "no azahar program in $BUILD (run steam-frame/build.sh first)" >&2; exit 1; }
for f in "$SF/citraframe.sh" "$SF/install.sh" "$SF/uninstall.sh" "$SF/steam-cef.py" "$REPO/LICENSE.txt" "$REPO/THIRD_PARTY_NOTICES.md"; do
  [ -f "$f" ] || { echo "missing $f" >&2; exit 1; }
done

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
strip --strip-unneeded -o "$STAGE/azahar" "$BUILD/azahar"
cp "$SF/citraframe.sh" "$SF/install.sh" "$SF/uninstall.sh" "$SF/steam-cef.py" "$STAGE/"
cp "$REPO/LICENSE.txt" "$REPO/THIRD_PARTY_NOTICES.md" "$STAGE/"
chmod +x "$STAGE/azahar" "$STAGE/citraframe.sh" "$STAGE/install.sh" "$STAGE/uninstall.sh"
echo "$VERSION" > "$STAGE/VERSION"
FILES="azahar citraframe.sh install.sh uninstall.sh steam-cef.py VERSION LICENSE.txt THIRD_PARTY_NOTICES.md"

NAME=citraframe-$VERSION-linux-arm64.tar.gz
tar --owner=0 --group=0 --numeric-owner -czf "$HERE/$NAME" -C "$STAGE" $FILES
(cd "$HERE" && sha256sum "$NAME" | tee SHA256SUMS)
echo "wrote $HERE/$NAME"
