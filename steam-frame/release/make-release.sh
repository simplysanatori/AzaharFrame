#!/bin/bash
# Packages a finished build for the Releases page:
#   steam-frame/release/azaharframe-<version>-linux-arm64.tar.gz
#   steam-frame/release/SHA256SUMS
# The tarball holds the stripped emulator (azahar), the launch wrapper, and at the top
# level install.sh, uninstall.sh, steam-cef.py and dock-icon.py, plus the art in art/ (Steam library art, the
# icon in four sizes, and logo.png and icon.png, which the program itself shows in VR and in its window), and the
# texture pack converter in tools/ (its encoder is built on the Frame on first use), so the unpacked archive installs itself
# (this is how Frametanium runs it).  Files are packed by name, so settings, saves, keys,
# logs and game files cannot end up in it.
#   steam-frame/release/make-release.sh [version [build-dir]]
# The version defaults to 1.0.0.  The build directory defaults to <repo>/build/bin/Release
# (AZAHARFRAME_BUILD names it as well).  Needs no headset and no Steam.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SF=$(cd "$HERE/.." && pwd)
REPO=$(cd "$SF/.." && pwd)
VERSION=${1:-1.0.0}
BUILD=${2:-${AZAHARFRAME_BUILD:-$REPO/build/bin/Release}}
[ -x "$BUILD/azahar" ] || { echo "no azahar program in $BUILD (run steam-frame/build.sh first)" >&2; exit 1; }
for f in "$SF/art/portrait.png" "$SF/art/wide.png" "$SF/art/hero.png" "$SF/art/logo.png" "$SF/art/icon.png" "$SF/art/icon-64.png" "$SF/art/icon-128.png" "$SF/art/icon-256.png" "$SF/dock-icon.py" "$SF/azaharframe.sh" "$SF/install.sh" "$SF/uninstall.sh" "$SF/steam-cef.py" "$SF/tools/convert-texture-pack.sh" "$SF/tools/convert_texture_pack.py" "$SF/tools/build-astcenc.sh" "$SF/tools/af-ktxenc.cpp" "$SF/tools/frame-controller-models.py" "$REPO/LICENSE.txt" "$REPO/THIRD_PARTY_NOTICES.md"; do
  [ -f "$f" ] || { echo "missing $f" >&2; exit 1; }
done

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
strip --strip-unneeded -o "$STAGE/azahar" "$BUILD/azahar"
cp "$SF/azaharframe.sh" "$SF/install.sh" "$SF/uninstall.sh" "$SF/steam-cef.py" "$SF/dock-icon.py" "$STAGE/"
cp "$REPO/LICENSE.txt" "$REPO/THIRD_PARTY_NOTICES.md" "$STAGE/"
mkdir -p "$STAGE/art"
cp "$SF/art/portrait.png" "$SF/art/wide.png" "$SF/art/hero.png" "$SF/art/logo.png" "$SF/art/icon.png" "$SF/art/icon-64.png" "$SF/art/icon-128.png" "$SF/art/icon-256.png" "$STAGE/art/"
mkdir -p "$STAGE/tools"
cp "$SF/tools/convert-texture-pack.sh" "$SF/tools/convert_texture_pack.py" "$SF/tools/build-astcenc.sh" "$SF/tools/af-ktxenc.cpp" "$SF/tools/frame-controller-models.py" "$STAGE/tools/"
chmod +x "$STAGE/azahar" "$STAGE/azaharframe.sh" "$STAGE/install.sh" "$STAGE/uninstall.sh" "$STAGE/dock-icon.py" "$STAGE/tools/convert-texture-pack.sh" "$STAGE/tools/build-astcenc.sh"
echo "$VERSION" > "$STAGE/VERSION"
FILES="azahar azaharframe.sh install.sh uninstall.sh steam-cef.py dock-icon.py VERSION LICENSE.txt THIRD_PARTY_NOTICES.md art tools"

NAME=azaharframe-$VERSION-linux-arm64.tar.gz
tar --owner=0 --group=0 --numeric-owner -czf "$HERE/$NAME" -C "$STAGE" $FILES
(cd "$HERE" && sha256sum "$NAME" | tee SHA256SUMS)
echo "wrote $HERE/$NAME"
