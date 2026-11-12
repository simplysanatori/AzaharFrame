#!/bin/bash
# Puts Igor Pavlov's official 7-Zip command line (7zz, the static linux-arm64 build) into a release's
# tools folder. The program uses it, after bsdtar, for archives bsdtar cannot read (Deflate64 zips,
# solid RAR4, RAR5 with a 1 GB dictionary). Pinned version and sha256; the download is cached.
#   steam-frame/tools/fetch-7zip.sh <dest tools dir> [<dest licenses dir>]
# Writes <tools>/7zz (the statically linked 7zzs of the package: no libraries, nothing for
# check-bundle.sh to resolve) and <licenses>/7zip-License.txt (default <tools>/../licenses).
set -euo pipefail
VERSION=25.01
FILE=7z2501-linux-arm64.tar.xz
URL=https://www.7-zip.org/a/$FILE
SHA256=39c5140f02ce4436599303c59a149f654cb1bbc47cdc105a942120d747ae040d
DEST=${1:?usage: fetch-7zip.sh <tools dir> [<licenses dir>]}
LIC=${2:-$DEST/../licenses}
CACHE=${XDG_CACHE_HOME:-$HOME/.cache}/azaharframe/7zip
mkdir -p "$CACHE" "$DEST" "$LIC"
if [ ! -f "$CACHE/$FILE" ] || [ "$(sha256sum "$CACHE/$FILE" | cut -d' ' -f1)" != "$SHA256" ]; then
  curl -fsSL --max-time 300 -o "$CACHE/$FILE.tmp" "$URL"
  mv "$CACHE/$FILE.tmp" "$CACHE/$FILE"
fi
[ "$(sha256sum "$CACHE/$FILE" | cut -d' ' -f1)" = "$SHA256" ] || { echo "fetch-7zip: $FILE does not match the pinned sha256" >&2; rm -f "$CACHE/$FILE"; exit 1; }
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
tar -xf "$CACHE/$FILE" -C "$T" 7zzs License.txt
install -m 755 "$T/7zzs" "$DEST/7zz"
install -m 644 "$T/License.txt" "$LIC/7zip-License.txt"
echo "7-Zip $VERSION: $DEST/7zz"
