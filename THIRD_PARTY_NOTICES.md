# Third-party notices

Azahar for Frame, and the program built from it, contain or are derived from the
following works, used under their licences. Each licence text is in the
repository next to the code, at the path given.

## CitraVR

Azahar for Frame is a fork of CitraVR by amwatson
(https://github.com/amwatson/CitraVR), a VR front end for the 3DS emulator on
the Meta Quest. The renderer changes that make the 3DS picture work in VR (the
immersive mode factor in the vertex shader, the view matrix hook in the
rasterizer, the forced 3D slider) and the layout of the screens in VR come from
it. CitraVR is released under the GNU General Public License, version 3 or any
later version: see [LICENSE.txt](LICENSE.txt). CitraVR's own README is kept as
[README.upstream.md](README.upstream.md).

## Azahar and Citra

The emulator is Azahar (https://github.com/azahar-emu/azahar), which continues
the Citra project (https://github.com/citra-emu/citra). CitraVR carries Azahar
2127.0-alpha4, which Azahar for Frame is merged with. Citra's code is under the GNU
General Public License, version 2 or later; Azahar's changes are under version 3
or any later version, and so is Azahar for Frame as a whole. Source files carry the
notice "Copyright Citra Emulator Project / Azahar Emulator Project, licensed
under GPLv3 or any later version". The GPLv2 text is kept in the repository
in [misc/licenses/gplv2.txt](misc/licenses/gplv2.txt).

## Artwork

The Steam library art, the README cover and the social image in `steam-frame/art/` and `docs/images/` are made from the pieces below. The sources and what was changed are in [steam-frame/art/CREDITS.md](steam-frame/art/CREDITS.md).

- **Azahar logo and wordmark.** The Azahar Emu Logo, (c) 2024 angyartanddraw and PabloMK7, under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Changed: the word "Emulator" in the official lockup is replaced with "for Frame", in the same typeface and style. The flower and "Azahar" are unchanged.
- **Typeface.** Blooming Grove Bold by Nathan Eady, released into the public domain by the author. It is the font in Azahar's wordmark.
- **Backgrounds.** Community uploads for "Azahar (Emulator)" on [SteamGridDB](https://www.steamgriddb.com/game/5480678) by GarfieldGaming0624: hero 132546 and grid 554631. The hero is scaled down and used as the background of the wide image, the cover and the social image. In the portrait the "Azahar Emulator" text is replaced by "Azahar for Frame".

## Qt themes

The dark window themes are QDarkStyleSheet (MIT): [dist/qt_themes/qdarkstyle/LICENSE.md](dist/qt_themes/qdarkstyle/LICENSE.md) and [dist/qt_themes/qdarkstyle_midnight_blue/LICENSE.rst](dist/qt_themes/qdarkstyle_midnight_blue/LICENSE.rst). The other themes are Azahar's and Citra's.

## Qt

The settings window is Qt 6 (https://www.qt.io), under the GNU Lesser General
Public License, version 3. Azahar for Frame does not include Qt: the program links the
Qt 6 libraries that SteamOS provides.

## OpenXR

Azahar for Frame talks to SteamVR through OpenXR (https://www.khronos.org/openxr/).

- The OpenXR headers used to build the VR code come from the Khronos
  OpenXR-SDK-Source (https://github.com/KhronosGroup/OpenXR-SDK-Source), under
  the Apache License 2.0 (dual licensed with MIT for part of the files). The
  copy in `externals/openxr-sdk-src/` keeps its licence files in
  `externals/openxr-sdk-src/LICENSES/`. <!-- VERIFY: add the vendored header path (third_party/openxr) once the XR code lands -->
- The OpenXR loader that runs on the Frame is SteamVR's own
  (`/opt/steamvr/bin/linuxarm64/libopenxr_loader.so`). It is not part of this
  repository or of the release.

## The headset keyboard

The keyboard in VR is the Compact keyboard of Moonlight XR (https://github.com/simplysanatori/moonlight-xr-steam-frame), by the same author, under the GNU General Public License, version 3: its layout, rules and look are ported to `src/frame_xr/keyboard/`.

- Its labels are drawn with stb_truetype by Sean Barrett (https://github.com/nothings/stb), v1.26, vendored unchanged as `src/frame_xr/keyboard/stb_truetype.h`. MIT or public domain; the text is at the end of that file.
- The font is Noto Sans Regular (SIL Open Font License 1.1), read from SteamOS's own `/usr/share/fonts/noto/`. It is not part of this repository or of the release.

## SDL

SDL 2 (https://www.libsdl.org) reads game controllers and, in the Azahar
build, plays sound. zlib licence. The program links the SDL 2 library SteamOS
provides; the copy in `externals/sdl2/SDL/` is Azahar's pin
([licence](externals/sdl2/SDL/LICENSE.txt)).

## Azahar's external libraries

The libraries below are in `externals/` (each a Git submodule of Azahar, listed
in [.gitmodules](.gitmodules)). The first table is what the Steam Frame build
compiles in (Vulkan, Qt, SDL2, OpenAL and Cubeb sound, no FFmpeg, no OpenGL, no
Discord, no web service, no tests). The second is what is in the source tree but
not built for the Frame. Licences are as published by each project; the text is
in the file named.

### Built into the Steam Frame program

| Library | Used for | Licence | Licence file |
|---|---|---|---|
| [Boost](https://github.com/azahar-emu/ext-boost) | containers, serialization | BSL-1.0 | `externals/boost/LICENSE_1_0.txt` |
| [Dynarmic](https://github.com/azahar-emu/dynarmic) | ARM11 recompiler (Arm64 backend) | 0BSD | `externals/dynarmic/LICENSE.txt` |
| [oaknut](https://github.com/merryhime/oaknut) | Arm64 assembler used by Dynarmic | MIT | `externals/oaknut/LICENSE` |
| [Teakra](https://github.com/wwylele/teakra) | DSP emulation | MIT | `externals/teakra/LICENSE` |
| [nihstro](https://github.com/neobrain/nihstro) | PICA shader assembler | BSD-3-Clause | `externals/nihstro/license.txt` |
| [SoundTouch](https://github.com/azahar-emu/soundtouch) | audio time stretching | LGPL-2.1 | `externals/soundtouch/COPYING.TXT` |
| [OpenAL Soft](https://github.com/azahar-emu/openal-soft) | audio output | LGPL-2.0 | `externals/openal-soft/COPYING` |
| [Cubeb](https://github.com/mozilla/cubeb) | audio output | ISC | `externals/cubeb/LICENSE` |
| [fmt](https://github.com/fmtlib/fmt) | formatting | MIT | `externals/fmt/LICENSE` |
| [enet](https://github.com/lsalzman/enet) | networking (local multiplayer) | MIT | `externals/enet/LICENSE` |
| [cpp-jwt](https://github.com/arun11299/cpp-jwt) | tokens for the network code | MIT | `externals/cpp-jwt/LICENSE` |
| [cryptopp-modern](https://github.com/cryptopp-modern/cryptopp-modern) | AES and hashing for the 3DS file formats | BSL-1.0 | `externals/cryptopp/LICENSE` |
| [LibreSSL](https://github.com/azahar-emu/ext-libressl-portable) | TLS | ISC and OpenSSL-style | `externals/libressl/COPYING` |
| [libusb](https://github.com/libusb/libusb) | USB devices (GameCube adapter) | LGPL-2.1 | `externals/libusb/libusb/COPYING` |
| [inih](https://github.com/benhoyt/inih) | ini files | BSD-3-Clause | `externals/inih/inih/LICENSE.txt` |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON | MIT | `externals/json/json.hpp` |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | HTTP | MIT | `externals/httplib/README.md` |
| [Zstandard](https://github.com/facebook/zstd) | compression | BSD-3-Clause (dual with GPLv2) | `externals/zstd/LICENSE` |
| [xxHash](https://github.com/Cyan4973/xxHash) | hashing | BSD-2-Clause | `externals/xxHash/LICENSE` |
| [libyuv](https://github.com/lemenkov/libyuv) | colour conversion | BSD-3-Clause | `externals/libyuv/LICENSE` |
| [lodepng](https://github.com/lvandeve/lodepng) | PNG | zlib | `externals/lodepng/lodepng/LICENSE` |
| [dds-ktx](https://github.com/septag/dds-ktx) | DDS and KTX textures | BSD-2-Clause | `externals/dds-ktx/LICENSE` |
| [FAAD2](https://github.com/knik0/faad2) | AAC decoding | GPL-2.0 or later | `externals/faad2/faad2/COPYING` |
| [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) | Vulkan | Apache-2.0 and MIT | `externals/vulkan-headers/LICENSE.md` |
| [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | GPU memory | MIT | `externals/vma/LICENSE.txt` |
| [glslang](https://github.com/KhronosGroup/glslang) | GLSL to SPIR-V for the shader cache | BSD-3-Clause, Apache-2.0 and others | `externals/glslang/LICENSE.txt` |
| [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) | SPIR-V | Apache-2.0 | `externals/spirv-tools/LICENSE` |
| [SPIRV-Headers](https://github.com/KhronosGroup/SPIRV-Headers) | SPIR-V | MIT-style (Khronos) | `externals/spirv-headers/LICENSE` |
| [sirit](https://github.com/azahar-emu/sirit) | SPIR-V emitter | BSD-3-Clause | `externals/sirit/sirit/LICENSE.txt` |
| [Microprofile](https://bitbucket.org/jonasmeyer/microprofile) | profiler | public domain (Unlicense) | `externals/microprofile/README.md` |
| [microtar](https://github.com/azahar-emu/microtar) | tar files | MIT | `externals/microtar/LICENSE` |
| [dllwalker](https://github.com/azahar-emu/dllwalker) | module walking | BSD-2-Clause | `externals/dllwalker/LICENSE.txt` |
| [gamemode](https://github.com/FeralInteractive/gamemode) | client header for Feral GameMode | BSD-3-Clause | `externals/gamemode/include/gamemode_client.h` |

### In the source tree, not built for the Frame

| Library | Why it is not built | Licence | Licence file |
|---|---|---|---|
| [Catch2](https://github.com/catchorg/Catch2) | tests are off | BSL-1.0 | `externals/catch2/LICENSE.txt` |
| [discord-rpc](https://github.com/azahar-emu/discord-rpc) | Discord presence is off | MIT | `externals/discord-rpc/LICENSE` |
| [glad](https://github.com/Dav1dde/glad) | OpenGL loader; OpenGL is off | MIT / public domain | `externals/glad/Readme.md` |
| [libretro-common](https://github.com/libretro/libretro-common) | RetroArch core only | MIT | `externals/libretro-common/` |
| [libadrenotools](https://github.com/bylaws/libadrenotools) | Android only | BSD-2-Clause | `externals/libadrenotools/LICENSE` |
| [Xbyak](https://github.com/herumi/xbyak) | x86 build only (the Frame is Arm64) | BSD-3-Clause | `externals/xbyak/COPYRIGHT` |
| getopt | Windows only | LGPL | `externals/getopt/getopt.c` |

The `dist/compatibility_list` submodule is Azahar's list of tested games and is
not part of the program.

## Texture pack encoder

The texture pack converter in `steam-frame/tools/` (in the release, `tools/`) is a script and
the source of a small encoder, `af-ktxenc.cpp`. Neither contains third-party code. On first
use, `build-astcenc.sh` downloads and builds these on the Frame; they are not in this
repository or in the release:

| Library | Used for | Licence | Licence file |
|---|---|---|---|
| [astcenc](https://github.com/ARM-software/astc-encoder) 5.7.1, by Arm | ASTC compression | Apache-2.0 | `LICENSE.txt` in its source; the build copies it to `~/.local/share/AzaharFrame/tools/astcenc-LICENSE.txt` |
| [stb_image](https://github.com/nothings/stb) 2.30, by Sean Barrett (the copy in astcenc's `Source/ThirdParty/`) | PNG decoding | public domain or MIT, at your choice | the end of `stb_image.h` |

## Not included

- Game data, game keys, firmware or BIOS dumps. Azahar for Frame contains none.
  Games, and the keys that decrypt some of them, come from your own 3DS.
- Texture packs. The converter works on packs you already have; no pack, and
  nothing made from one, is part of this repository or the release.
- Valve's SteamVR and OpenXR loader, which are on the Frame and are not copied.
- The Noto Sans font of the headset keyboard, which is SteamOS's.

3DS, Nintendo and their logos are trademarks of Nintendo. Azahar for Frame is an
unofficial fan project, not affiliated with or endorsed by Nintendo, Valve, the
Azahar team or the CitraVR author.
