#!/bin/bash
# Packages a finished build for the Releases page, into an output folder outside the repository:
#   azaharframe-<version>-linux-arm64.tar.gz            what Frametanium installs (install.sh and uninstall.sh at the top)
#   azaharframe-<version>-framedrop-linux-arm64.zip     what FrameDrop installs (run.sh at the top)
#   azaharframe.framedrop.json                          FrameDrop's manifest: the zip's release url and sha256
#   SHA256SUMS                                          of the three
# The tarball holds the stripped emulator (azahar), the launch wrapper, and at the top
# level install.sh, uninstall.sh, steam-cef.py and dock-icon.py, plus the art in art/ (Steam library art, the
# icon in four sizes, and logo.png and icon.png, which the program itself shows in VR and in its window), and the
# texture pack converter in tools/ (its encoder is built on the Frame on first use), so the unpacked archive installs itself
# (this is how Frametanium runs it).  Files are packed by name, so settings, saves, keys,
# logs and game files cannot end up in it.  The zip is the same files plus run.sh (framedrop-run.sh).
#   steam-frame/release/make-release.sh [version [build-dir [out-dir]]]
# The version defaults to 1.0.0.  The build directory defaults to <repo>/build/bin/Release
# (AZAHARFRAME_BUILD names it as well).  The output folder defaults to ~/Games/AzaharFrame/release-out
# (AZAHARFRAME_OUT names it as well) and must not be inside the repository.
# It refuses to package a program that has a home folder path ("/home/...") inside it: such a build names
# the machine it was made on.  Build with steam-frame/build.sh, which maps the paths away.
# Needs no headset and no Steam.  Then check the result with steam-frame/privacy-check.sh and test-release.sh.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SF=$(cd "$HERE/.." && pwd)
REPO=$(cd "$SF/.." && pwd)
VERSION=${1:-1.0.0}
BUILD=${2:-${AZAHARFRAME_BUILD:-$REPO/build/bin/Release}}
OUT=${3:-${AZAHARFRAME_OUT:-$HOME/Games/AzaharFrame/release-out}}
OUT=$(readlink -m "$OUT")
case "$OUT/" in "$REPO"/*) echo "the output folder $OUT is inside the repository; name one outside it" >&2; exit 1 ;; esac
mkdir -p "$OUT"
[ -x "$BUILD/azahar" ] || { echo "no azahar program in $BUILD (run steam-frame/build.sh first)" >&2; exit 1; }
for f in "$SF/art/portrait.png" "$SF/art/wide.png" "$SF/art/hero.png" "$SF/art/logo.png" "$SF/art/icon.png" "$SF/art/icon-64.png" "$SF/art/icon-128.png" "$SF/art/icon-256.png" "$SF/dock-icon.py" "$SF/azaharframe.sh" "$SF/install.sh" "$SF/uninstall.sh" "$SF/steam-cef.py" "$SF/framedrop-run.sh" "$SF/release/make-framedrop-zip.py" "$SF/tools/convert-texture-pack.sh" "$SF/tools/convert_texture_pack.py" "$SF/tools/build-astcenc.sh" "$SF/tools/af-ktxenc.cpp" "$SF/tools/frame-controller-models.py" "$REPO/LICENSE.txt" "$REPO/THIRD_PARTY_NOTICES.md"; do
  [ -f "$f" ] || { echo "missing $f" >&2; exit 1; }
done

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
strip --strip-unneeded -o "$STAGE/azahar" "$BUILD/azahar"
# A program built with absolute paths carries the builder's home folder in its assertions and debug strings.
if HOMES=$(strings -a "$STAGE/azahar" | grep -F '/home/' | sort -u | head -n 5) && [ -n "$HOMES" ]; then
  echo "refusing to package $BUILD/azahar: it has /home/ paths inside it, for example:" >&2
  echo "$HOMES" | cut -c1-120 | sed 's/^/  /' >&2
  echo "Build it with steam-frame/build.sh, which maps the checkout and build folders out of the paths (-ffile-prefix-map)." >&2
  exit 1
fi
cp "$SF/azaharframe.sh" "$SF/install.sh" "$SF/uninstall.sh" "$SF/steam-cef.py" "$SF/dock-icon.py" "$STAGE/"
cp "$REPO/LICENSE.txt" "$REPO/THIRD_PARTY_NOTICES.md" "$STAGE/"
mkdir -p "$STAGE/art"
cp "$SF/art/portrait.png" "$SF/art/wide.png" "$SF/art/hero.png" "$SF/art/logo.png" "$SF/art/icon.png" "$SF/art/icon-64.png" "$SF/art/icon-128.png" "$SF/art/icon-256.png" "$STAGE/art/"
mkdir -p "$STAGE/tools"
cp "$SF/tools/convert-texture-pack.sh" "$SF/tools/convert_texture_pack.py" "$SF/tools/build-astcenc.sh" "$SF/tools/af-ktxenc.cpp" "$SF/tools/frame-controller-models.py" "$STAGE/tools/"
chmod +x "$STAGE/azahar" "$STAGE/azaharframe.sh" "$STAGE/install.sh" "$STAGE/uninstall.sh" "$STAGE/dock-icon.py" "$STAGE/tools/convert-texture-pack.sh" "$STAGE/tools/build-astcenc.sh"
echo "$VERSION" > "$STAGE/VERSION"
FILES="azahar azaharframe.sh install.sh uninstall.sh steam-cef.py dock-icon.py VERSION LICENSE.txt THIRD_PARTY_NOTICES.md art tools"
# the program's own Qt and libraries (portable.sh; steam-frame/build-portable.sh)
. "$HERE/portable.sh"
portable_stage "$STAGE"
FILES="$FILES $PORTABLE_FILES"

NAME=azaharframe-$VERSION-linux-arm64.tar.gz
ZIP=azaharframe-$VERSION-framedrop-linux-arm64.zip
tar --owner=0 --group=0 --numeric-owner --sort=name -cf - -C "$STAGE" $FILES | gzip -n > "$OUT/$NAME"
portable_verify "$OUT/$NAME"
python3 -I "$HERE/make-framedrop-zip.py" "$OUT/$NAME" --version "$VERSION" --out "$OUT" --run-sh "$SF/framedrop-run.sh" > /dev/null
(cd "$OUT" && sha256sum "$NAME" "$ZIP" azaharframe.framedrop.json | tee SHA256SUMS)
echo "wrote $OUT/$NAME, $OUT/$ZIP, $OUT/azaharframe.framedrop.json and $OUT/SHA256SUMS"
