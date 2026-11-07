#!/bin/bash
# Converts an Azahar texture pack to ASTC (KTX files with full mip chains), which Azahar for
# Frame uploads without decoding.  The original pack is not changed; the new one goes next to
# it as <pack>-astc4x4.  Stopping it is safe: a second run carries on where it left off.
#   convert-texture-pack.sh [--block 4x4|6x6|8x6] [--quality fast|medium|thorough]
#                           [--jobs N] [--activate] PACK [OUTPUT]
#   convert-texture-pack.sh --restore PACK
#   convert-texture-pack.sh --help
# PACK is a pack folder with pack.json at its root, for example the one Azahar reads:
#   ~/.local/share/AzaharFrame/user/load/textures/<TitleID>
# --activate makes that folder use the new pack when done; --restore goes back to the original.
# It runs at the lowest CPU and disk priority.  The encoder comes from build-astcenc.sh.
HERE=$(cd "$(dirname "$0")" && pwd)
case "$1" in -h) set -- --help ;; esac
NICE=(nice -n 19)
command -v ionice > /dev/null && NICE+=(ionice -c 3)
exec "${NICE[@]}" python3 -I "$HERE/convert_texture_pack.py" "$@"
