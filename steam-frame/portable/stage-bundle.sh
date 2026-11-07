#!/bin/bash
# Lays out what the program carries so that it does not depend on the Qt SteamOS has: lib/ (the Qt libraries,
# ICU and anything else tools/host-libs.allow does not list), plugins/ (the Qt plugins) and qt.conf (Qt's
# prefix is the app folder).  Runs in the build container, after the build.
#   stage-bundle.sh <Qt prefix> <azahar binary> <out dir> [allowlist]
# Needs patchelf, objdump, ldconfig.
#
# Every library the program, the plugins and the libraries themselves need is copied from the Qt prefix.
# One that is neither in Qt nor on the allowlist is copied from the machine this runs on and reported, so a
# surprise shows (the Ubuntu libraries are glibc 2.39 ones, which any SteamOS runs).
set -euo pipefail
QT="$(cd "${1:?usage: stage-bundle.sh <Qt prefix> <azahar binary> <out dir> [allowlist]}" && pwd)"
BIN="$(readlink -f "${2:?azahar binary}")"
OUT="${3:?out dir}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ALLOW="${4:-$HERE/../tools/host-libs.allow}"
for t in patchelf objdump readelf ldconfig; do command -v "$t" >/dev/null || { echo "missing tool: $t"; exit 1; }; done
[ -f "$QT/lib/libQt6Core.so.6" ] || { echo "no Qt at $QT"; exit 1; }
[ -f "$BIN" ] || { echo "no program at $BIN"; exit 1; }
mapfile -t PATS < <(sed -e 's/#.*//' -e 's/[[:space:]]//g' "$ALLOW" | grep .)
allowed() { local p; for p in "${PATS[@]}"; do [[ "$1" == $p ]] && return 0; done; return 1; }

rm -rf "$OUT"; mkdir -p "$OUT/lib" "$OUT/plugins"
cat > "$OUT/qt.conf" <<'CONF'
; Azahar for Frame: its own Qt (steam-frame/portable), never the system's
[Paths]
Prefix = .
Libraries = lib
Plugins = plugins
CONF

# The plugins: platforms (xcb for the Xwayland session and Game Mode, wayland for the desktop, offscreen and
# minimal for tests), their GL and shell integrations, image formats, icon engines, input contexts, themes
# that need only D-Bus, network information and TLS.
echo "== plugins"
for d in platforms xcbglintegrations wayland-shell-integration wayland-graphics-integration-client \
         wayland-decoration-client tls imageformats iconengines platforminputcontexts platformthemes \
         networkinformation; do
    [ -d "$QT/plugins/$d" ] && cp -a "$QT/plugins/$d" "$OUT/plugins/"
done
find "$OUT/plugins/platforms" -name '*.so' ! -name 'libqxcb.so' ! -name 'libqwayland*.so' \
    ! -name 'libqoffscreen.so' ! -name 'libqminimal.so' -delete
# the GTK theme needs GTK, which SteamOS may not have; the KDE and others would need their own libraries
find "$OUT/plugins/platformthemes" -name '*.so' ! -name 'libqxdgdesktopportal.so' -delete 2> /dev/null || true
# No Qt Multimedia backend: Qt's FFmpeg backend needs FFmpeg 7's libraries and the GStreamer one needs GStreamer,
# and both are what a SteamOS update moves (FFmpeg 8).  Azahar uses Multimedia only for the 3DS camera, which then
# lists no cameras.
for d in "$OUT"/plugins/*/; do echo "  ${d#"$OUT"/}: $(cd "$d" && ls | tr '\n' ' ')"; done

echo "== libraries"
LDC="$(ldconfig -p)"
# every library the program, the plugins and the bundled libraries need, until nothing new comes in
while :; do
    added=0
    while IFS= read -r -d '' f; do
        [ "$(head -c 4 "$f" | od -An -tx1 | tr -d ' \n')" = 7f454c46 ] || continue
        for s in $(objdump -p "$f" | awk '$1 == "NEEDED" { print $2 }'); do
            [ -e "$OUT/lib/$s" ] && continue
            allowed "$s" && continue
            if [ -e "$QT/lib/$s" ]; then
                cp -L "$QT/lib/$s" "$OUT/lib/$s"
            else
                p="$(awk -v s="$s" '$1 == s && /AArch64|aarch64/ { print $NF; exit }' <<<"$LDC")"
                [ -n "$p" ] || { echo "${f#"$OUT"/} needs $s: not in Qt, not on this machine, not allowlisted"; exit 1; }
                cp -L "$p" "$OUT/lib/$s"
                echo "  from the build machine: $s ($p, needed by ${f#"$OUT"/})"
            fi
            chmod 755 "$OUT/lib/$s"
            added=1
        done
    done < <({ printf '%s\0' "$BIN"; find "$OUT" -type f -print0; })
    [ "$added" = 1 ] || break
done

# RUNPATH: libraries find their neighbours in lib/, plugins two folders up.  Qt's own libraries are
# built with $ORIGIN-relative paths already; whatever else is not gets one.  (No strip after this: strip
# after patchelf can damage a file.)
for f in "$OUT"/lib/*; do
    rp="$(readelf -d "$f" | sed -n 's/.*(RUNPATH).*\[\(.*\)\]/\1/p; s/.*(RPATH).*\[\(.*\)\]/\1/p')"
    if [ "$rp" != '$ORIGIN' ]; then patchelf --remove-rpath "$f"; patchelf --set-rpath '$ORIGIN' "$f"; fi
done
while IFS= read -r -d '' f; do
    [ "$(head -c 4 "$f" | od -An -tx1 | tr -d ' \n')" = 7f454c46 ] || continue
    rp="$(readelf -d "$f" | sed -n 's/.*(RUNPATH).*\[\(.*\)\]/\1/p; s/.*(RPATH).*\[\(.*\)\]/\1/p')"
    want='$ORIGIN/../../lib'   # plugins/<group>/x.so
    if [ "$rp" != "$want" ]; then patchelf --remove-rpath "$f"; patchelf --set-rpath "$want" "$f"; fi
done < <(find "$OUT/plugins" -type f -name '*.so' -print0)

echo "  lib/: $(cd "$OUT/lib" && ls | wc -l) files"
echo "staged $OUT ($(du -sh "$OUT" | cut -f1))"
