#!/bin/bash
# Checks a release the way Frametanium and FrameDrop use it, in a scratch home folder: a stand-in for Steam's
# debugging port (test/mock-steam-cef.py), a stand-in vrcmd, no real Steam library, SteamVR or data folder touched.
# It never starts the emulator.
#   steam-frame/test-release.sh ARCHIVE.tar.gz   test an archive made by release/make-release.sh; the FrameDrop zip,
#                                                its json and SHA256SUMS are read from beside it when they are there
#   steam-frame/test-release.sh --dummy          first package a dummy program (a copy of a system program, named azahar)
#                                                with release/make-release.sh into a scratch folder, then test that
# Prints PASS / FAIL lines; exit 1 on any FAIL.  AZAHARFRAME_TEST_KEEP=1 keeps the scratch folder.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
case "${1:-}" in
    "" | -h | --help) sed -n '2,/^set -u/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 2 ;;
esac
REAL=$HOME
REAL_STAT=$(stat -c %Y "$REAL/Games/AzaharFrame/app" "$REAL/.config/openvr" "$REAL/.local/share/applications/azaharframe.desktop" 2> /dev/null)
T=$(mktemp -d "${TMPDIR:-/tmp}/azaharframe-release-test.XXXXXX")
MOCK_PID=
cleanup() { [ -n "$MOCK_PID" ] && kill "$MOCK_PID" 2> /dev/null; if [ -n "${AZAHARFRAME_TEST_KEEP:-}" ]; then echo "kept $T"; else rm -rf "$T"; fi; }
trap cleanup EXIT
fails=0
ok() { echo "PASS $1"; }
bad() { echo "FAIL $1"; fails=$((fails + 1)); }
check() { local what=$1; shift; if "$@"; then ok "$what"; else bad "$what"; fi; }
jq() { python3 -I -c 'import json,sys; d=json.load(open(sys.argv[1])); print(eval(sys.argv[2]))' "$@"; }

if [ "$1" = --dummy ]; then
    mkdir -p "$T/build" "$T/out"
    cp "$(type -P true || echo /bin/true)" "$T/build/azahar"
    bash "$HERE/release/make-release.sh" 0.0.0-test "$T/build" "$T/out" > "$T/make.log" 2>&1 || { cat "$T/make.log"; echo "FAIL make-release.sh"; exit 1; }
    ARCHIVE=$T/out/azaharframe-0.0.0-test-linux-arm64.tar.gz
else
    ARCHIVE=$(readlink -f "$1")
fi
[ -f "$ARCHIVE" ] || { echo "no such file: $ARCHIVE" >&2; exit 2; }
OUTDIR=$(dirname "$ARCHIVE")

H=$T/home
mkdir -p "$H/.local/share/Steam/userdata/1234/config"
printf '#!/bin/bash\necho "$@" >> "%s/vrcmd.log"\n' "$T" > "$T/vrcmd"; chmod +x "$T/vrcmd"
python3 -I "$HERE/test/mock-steam-cef.py" 0 "$T/state.json" "$T/mock.log" &
MOCK_PID=$!
for _ in $(seq 50); do [ -s "$T/state.json.port" ] && break; sleep 0.1; done
PORT=$(cat "$T/state.json.port" 2> /dev/null)
[ -n "$PORT" ] || { echo "the Steam stand-in did not start" >&2; exit 2; }
# Only the scratch home: no AZAHARFRAME_APP / AZAHARFRAME_HOME, so the defaults under HOME are what is tested.
export HOME=$H AZAHARFRAME_CEF_PORT=$PORT AZAHARFRAME_VRCMD=$T/vrcmd AZAHARFRAME_VRSERVER_RUNNING=1
unset AZAHARFRAME_APP AZAHARFRAME_HOME AZAHARFRAME_SKIP_STEAM AZAHARFRAME_BUILD AZAHARFRAME_DRY_RUN SteamAppId XDG_CONFIG_HOME XDG_DATA_HOME XDG_CACHE_HOME XDG_STATE_HOME
APP=$H/Games/AzaharFrame/app
DATA=$H/.local/share/AzaharFrame
# SteamVR's own controller models are converted on a real Frame; keep the test off them (and off /opt/steamvr).
mkdir -p "$DATA/user/frame-models"; : > "$DATA/user/frame-models/frame_controller_left.model"; : > "$DATA/user/frame-models/frame_controller_right.model"
nshort() { jq "$T/state.json" 'len(d["shortcuts"])' 2> /dev/null || echo 0; }
sc() { jq "$T/state.json" "list(d['shortcuts'].values())[0]$1"; }

# ---- 0. the archive, unpacked as Frametanium does
U=$H/.local/share/frametanium/apps/azaharframe
mkdir -p "$U" && tar -xzf "$ARCHIVE" -C "$U"
VER=$(head -n 1 "$U/VERSION" 2> /dev/null)
check "archive: install.sh, uninstall.sh and VERSION at the top" test -x "$U/install.sh" -a -x "$U/uninstall.sh" -a -n "$VER"
check "archive: name carries the VERSION" test "$(basename "$ARCHIVE")" = "azaharframe-$VER-linux-arm64.tar.gz"
for f in azahar azaharframe.sh steam-cef.py dock-icon.py LICENSE.txt THIRD_PARTY_NOTICES.md art/portrait.png art/icon.png art/icon-64.png tools/convert-texture-pack.sh tools/frame-controller-models.py; do
    check "archive has $f" test -e "$U/$f"
done
check "archive: no keys, saves, settings or game files" test -z "$(cd "$U" && find . \( -iname '*.3ds' -o -iname '*.cci' -o -iname '*.cia' -o -iname '*.cxi' -o -iname 'aes_keys*' -o -iname 'seeddb*' -o -iname 'boot9*' -o -iname '*.sav' -o -iname 'qt-config.ini' -o -iname '*.log' -o -iname user \) | head -1)"
check "archive: nothing is a symbolic link" test -z "$(find "$U" -type l | head -1)"
check "archive: the program has no /home/ path inside" sh -c "! strings -a '$U/azahar' | grep -qF /home/"

# ---- 0b. privacy-check.sh: finds what it should in a planted folder, and finds nothing in the archive
echo "zzdummyname" > "$T/patterns"
mkdir -p "$T/planted"
printf '%s\n' 'mail someone@example.org' 'lan 192.168.1.5' 'path /home/steamos/x' 'rom Some Game (USA) (En,Fr).3ds' 'name zzdummyname' \
    'ok 149214925+simplysanatori@users.noreply.github.com' 'ok list of types: `.3ds`, `.cci`, `.3dsx`' > "$T/planted/notes.txt"
AZAHARFRAME_PRIVACY_PATTERNS=$T/patterns bash "$HERE/privacy-check.sh" "$T/planted" > "$T/pc1.log" 2>&1; rc=$?
check "privacy-check: planted folder fails" test $rc -eq 1
check "privacy-check: finds the e-mail, LAN address, home folder, game file name, region tag and personal pattern" sh -c "
    grep -q 'notes.txt:1: email' '$T/pc1.log' && grep -q 'notes.txt:2: LAN address' '$T/pc1.log' && grep -q 'notes.txt:3: home folder' '$T/pc1.log' &&
    grep -q 'notes.txt:4: 3DS game file name' '$T/pc1.log' && grep -q 'notes.txt:4: 3DS game region tag' '$T/pc1.log' && grep -q 'notes.txt:5: personal pattern 1' '$T/pc1.log'"
check "privacy-check: lets the maintainer's no-reply address and bare file types through" sh -c "! grep -q 'notes.txt:[67]' '$T/pc1.log'"
check "privacy-check: does not print the personal pattern's match" sh -c "! grep 'personal pattern' '$T/pc1.log' | grep -q zzdummyname"
AZAHARFRAME_PRIVACY_PATTERNS=$T/patterns bash "$HERE/privacy-check.sh" "$ARCHIVE" "$U" > "$T/pc2.log" 2>&1; rc=$?
check "privacy-check: the release archive and its unpacked folder are clean" test $rc -eq 0

# ---- 1. a fresh install from the unpacked folder: no terminal, no questions
(cd "$U" && bash ./install.sh < /dev/null > "$T/i1.log" 2>&1); rc=$?
check "fresh install: exit 0" test $rc -eq 0
for f in azahar azaharframe.sh steam-cef.py dock-icon.py uninstall.sh install.sh VERSION LICENSE.txt THIRD_PARTY_NOTICES.md art/portrait.png art/wide.png art/hero.png art/logo.png art/icon.png art/icon-64.png art/icon-128.png art/icon-256.png tools/convert-texture-pack.sh azaharframe.vrmanifest; do
    check "installed $f" test -e "$APP/$f"
done
check "installed programs are executable" test -x "$APP/azahar" -a -x "$APP/azaharframe.sh" -a -x "$APP/uninstall.sh"
check "installed VERSION matches" test "$(head -n 1 "$APP/VERSION")" = "$VER"
check "data folder made" test -d "$DATA/user"
check "one shortcut" test "$(nshort)" = 1
APPID=$(jq "$T/state.json" 'list(d["shortcuts"])[0]')
check "shortcut is Azahar for Frame" test "$(sc '["name"]')" = "Azahar for Frame"
check "shortcut runs the wrapper" test "$(sc '["exe"]')" = "\"$APP/azaharframe.sh\""
check "shortcut starts in the program folder" test "$(sc '["start"]')" = "\"$APP\""
check "library art set (portrait, hero, logo, wide)" test "$(sc '["art"]' | grep -o "'[0-3]': [1-9]" | cut -c2 | sort | tr -d '\n')" = 0123
check "icon set from the installed art" test "$(sc '["icon"]')" = "$APP/art/icon.png"
check "SteamVR refresh rate set for the entry" grep -q "steam.app.$APPID.preferredRefreshRate 120" "$T/vrcmd.log"
check "SteamVR throttling and smoothing keys set" sh -c "grep -q 'steam.app.$APPID.framesToThrottle 0' '$T/vrcmd.log' && grep -q 'steam.app.$APPID.motionSmoothingOverride 2' '$T/vrcmd.log'"
check "SteamVR told about the app manifest" grep -q -- "--appmanifest $APP/azaharframe.vrmanifest" "$T/vrcmd.log"
check "manifest names the entry's app key" grep -q "steam.app.$APPID" "$APP/azaharframe.vrmanifest"
check "icon theme PNG and desktop file made" test -f "$H/.local/share/icons/hicolor/256x256/apps/azaharframe.png" -a -f "$H/.local/share/applications/azaharframe.desktop"
check "the real program folder and SteamVR config were not touched" test "$(stat -c %Y "$REAL/Games/AzaharFrame/app" "$REAL/.config/openvr" "$REAL/.local/share/applications/azaharframe.desktop" 2> /dev/null)" = "$REAL_STAT"

# ---- 2. again: the same entry and appid, nothing duplicated, data kept
mkdir -p "$DATA/user/config"; echo "keep = 1" > "$DATA/user/config/qt-config.ini"; echo save > "$DATA/user/marker"
SUM1=$(cd "$APP" && find . -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum)
(cd "$U" && bash ./install.sh < /dev/null > "$T/i2.log" 2>&1); rc=$?
check "second install: exit 0" test $rc -eq 0
check "second install: still one shortcut, same appid" test "$(nshort)" = 1 -a "$(jq "$T/state.json" 'list(d["shortcuts"])[0]')" = "$APPID"
check "second install: said it was updating the entry" grep -q "updating it" "$T/i2.log"
check "second install: installed files unchanged" test "$(cd "$APP" && find . -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum)" = "$SUM1"
check "second install: settings and saves kept" test "$(cat "$DATA/user/config/qt-config.ini")" = "keep = 1" -a -f "$DATA/user/marker"
# from the installed copy too (how a user re-runs it)
(cd "$APP" && bash ./install.sh < /dev/null > "$T/i2b.log" 2>&1); rc=$?
check "install.sh run from the program folder: exit 0, one shortcut" test $rc -eq 0 -a "$(nshort)" = 1

# ---- 3. Steam not running: the files go in, the entry is skipped, exit 0
(cd "$U" && AZAHARFRAME_APP=$T/app-nosteam AZAHARFRAME_CEF_PORT=1 bash ./install.sh < /dev/null > "$T/i3.log" 2>&1); rc=$?
check "Steam down: exit 0" test $rc -eq 0
check "Steam down: program installed, entry skipped, says so" sh -c "test -x '$T/app-nosteam/azahar' && grep -q 'Steam was not reachable' '$T/i3.log' && test '$(nshort)' = 1"
rm -rf "$T/app-nosteam"
(cd "$U" && AZAHARFRAME_APP=$T/app-skip AZAHARFRAME_SKIP_STEAM=1 bash ./install.sh < /dev/null > "$T/i4.log" 2>&1); rc=$?
check "AZAHARFRAME_SKIP_STEAM: exit 0, no new entry" test $rc -eq 0 -a "$(nshort)" = 1 -a -x "$T/app-skip/azahar"
rm -rf "$T/app-skip"

# ---- 4. uninstall from the unpacked folder (Frametanium's removal): entry, program and dock icon go, data stays
(cd "$U" && bash ./uninstall.sh < /dev/null > "$T/u1.log" 2>&1); rc=$?
check "uninstall: exit 0" test $rc -eq 0
check "uninstall: shortcut removed" test "$(nshort)" = 0
check "uninstall: program folder removed" test ! -e "$APP"
check "uninstall: dock icon files removed" test ! -e "$H/.local/share/icons/hicolor/256x256/apps/azaharframe.png" -a ! -e "$H/.local/share/applications/azaharframe.desktop"
check "uninstall: data kept" test -f "$DATA/user/marker"
(cd "$U" && bash ./uninstall.sh < /dev/null > "$T/u2.log" 2>&1); rc=$?
check "uninstall again: exit 0" test $rc -eq 0
(cd "$U" && bash ./install.sh < /dev/null > /dev/null 2>&1)
(cd "$U" && bash ./uninstall.sh --purge < /dev/null > "$T/u3.log" 2>&1); rc=$?
check "uninstall --purge: exit 0, data folder gone" test $rc -eq 0 -a ! -e "$DATA" -a ! -e "$APP" -a "$(nshort)" = 0

# ---- 5. FrameDrop: the zip, its json and sums beside the archive; run.sh installs without a second entry
Z=$(ls "$OUTDIR"/azaharframe-*-framedrop-linux-arm64.zip 2> /dev/null | head -1)
J=$OUTDIR/azaharframe.framedrop.json
if [ -n "$Z" ]; then
    check "FrameDrop zip is named for the version" test "$(basename "$Z")" = "azaharframe-$VER-framedrop-linux-arm64.zip"
    check "FrameDrop json: schema, name, sha256 of the zip, release url" python3 -I - "$J" "$Z" "$VER" << 'PY'
import hashlib, json, os, sys
j, z, ver = sys.argv[1:4]
d = json.load(open(j))
assert d["schema"] == "framedrop.install/v1" and d["name"] and len(d["files"]) == 1, d
f = d["files"][0]
assert f["sha256"] == hashlib.sha256(open(z, "rb").read()).hexdigest(), "sha256 differs"
assert f["url"] == "https://github.com/simplysanatori/AzaharFrame/releases/download/v%s/%s" % (ver, os.path.basename(z)), f["url"]
PY
    if [ -f "$OUTDIR/SHA256SUMS" ]; then
        check "SHA256SUMS verifies" sh -c "cd '$OUTDIR' && sha256sum -c --quiet SHA256SUMS"
    fi
    AZAHARFRAME_PRIVACY_PATTERNS=$T/patterns bash "$HERE/privacy-check.sh" "$Z" "$J" > "$T/pc3.log" 2>&1
    check "privacy-check: the FrameDrop zip and json are clean" test $? -eq 0
    FD=$H/framedrop && mkdir -p "$FD" && python3 -I -c 'import sys,zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' "$Z" "$FD"
    check "FrameDrop zip: run.sh and install.sh at the top, same VERSION" test -f "$FD/run.sh" -a -f "$FD/install.sh" -a "$(head -n 1 "$FD/VERSION")" = "$VER"
    chmod -x "$FD/install.sh" "$FD/azahar" "$FD/run.sh" 2> /dev/null   # an unpack that lost the exec bits
    (cd "$FD" && FRAMEDROP_RUN_DRY=1 sh ./run.sh < /dev/null > "$T/fd1.log" 2>&1); rc=$?
    check "FrameDrop run.sh: exit 0" test $rc -eq 0
    check "FrameDrop run.sh: would start the installed wrapper" grep -q "would start: $APP/azaharframe.sh" "$T/fd1.log"
    check "FrameDrop run.sh: installed with no entry of its own" test -x "$APP/azahar" -a "$(nshort)" = 0
    check "FrameDrop run.sh: exec bits given back" test -x "$FD/install.sh"
    (cd "$FD" && FRAMEDROP_RUN_DRY=1 sh ./run.sh < /dev/null > "$T/fd2.log" 2>&1); rc=$?
    check "FrameDrop run.sh again: starts without installing again" test $rc -eq 0 -a "$(grep -c 'run.sh: install.sh' "$H/.local/state/framedrop-azaharframe/install.log")" = 1
    (cd "$U" && bash ./uninstall.sh --purge < /dev/null > /dev/null 2>&1)
else
    echo "note: no FrameDrop zip beside the archive; the FrameDrop checks were skipped"
fi

echo
if [ "$fails" = 0 ]; then echo "all passed"; else echo "$fails FAILED (AZAHARFRAME_TEST_KEEP=1 keeps the logs)"; fi
[ "$fails" = 0 ]
