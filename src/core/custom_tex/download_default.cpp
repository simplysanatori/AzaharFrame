// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texdl: the texture catalog compiled in (a copy of catalog/textures.json in the
// maintainer's repository, used when the repository cannot be read).

#include "core/custom_tex/download.h"

namespace CustomTex::Download {

const char* DefaultCatalogJson() {
    return R"JSON({
  "schema": 1,
  "packs": [
    {
      "id": "mm3d-hd-deathwrench",
      "name": "Majora's Mask 3D HD",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "DeathWrench",
      "author_url": "https://github.com/DeathWrench/MM3DHD",
      "source": {
        "type": "github_release",
        "repo": "DeathWrench/MM3DHD",
        "tag": "0.0.42b",
        "asset_pattern": "MM3DHD.zip"
      },
      "size": 78109346,
      "sha256": "311c7bc7ab938920743b8867ab96c6ad1365f1d10439fd1b1369f264ecf28690",
      "format": "zip",
      "layout": "variants",
      "folder": "MM3D HD",
      "licence": "Not stated; fan-made, credit the author",
      "notes": "Several variants (fonts, Link styles); each shows up as its own pack. Partial pack."
    },
    {
      "id": "pokemon-xy-hd",
      "name": "Pokemon X and Y HD",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "Ullr8, Gray-Rice",
      "author_url": "https://github.com/Gray-Rice/PokeTex-3DS",
      "source": {
        "type": "github_release",
        "repo": "Gray-Rice/PokeTex-3DS",
        "tag": "xy",
        "asset_pattern": "XY.7z"
      },
      "size": 1834769856,
      "format": "7z",
      "layout": "single",
      "folder": "XY HD",
      "licence": "Not stated; fan-made, credit the authors",
      "notes": "Work in progress. Based on Pokemon Y Resurrection."
    },
    {
      "id": "pokemon-usum-hd",
      "name": "Pokemon Ultra Sun and Ultra Moon HD",
      "title": "Pokemon Ultra Sun / Pokemon Ultra Moon",
      "title_ids": [
        "00040000001B5000",
        "00040000001B5100"
      ],
      "author": "Volya, Gray-Rice",
      "author_url": "https://github.com/Gray-Rice/PokeTex-3DS",
      "source": {
        "type": "github_release",
        "repo": "Gray-Rice/PokeTex-3DS",
        "tag": "usum",
        "asset_pattern": "USUM.7z"
      },
      "size": 124548993,
      "sha256": "f28eef887269c51735f45b0946d07e060f45b954fe61ff6739bc80a6e053d572",
      "format": "7z",
      "layout": "single",
      "folder": "USUM HD",
      "licence": "Not stated; fan-made, credit the authors",
      "notes": "Textures from the Citra forum archive."
    },
    {
      "id": "oot3d-4k-henriko",
      "name": "Ocarina of Time 3D 4K",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "Henriko Magnifico",
      "author_url": "https://www.henrikomagnifico.com/zelda-ocarina-of-time-3d-4k",
      "source": {
        "type": "gdrive_folder",
        "folder_id": "1QQ-4b8SvR2d4VKKQNS-I1WOIB5EaTuzt",
        "file_pattern": "OOT3D 4K * (4K).zip"
      },
      "alternates": [
        {
          "type": "gdrive_folder",
          "folder_id": "1F2lJ_TJyZovp5TtKmqrl24fy97IdEh85",
          "file_pattern": "OOT3D 4K * (4K).zip"
        }
      ],
      "size": 0,
      "format": "zip",
      "layout": "single",
      "folder": "OoT3D 4K",
      "licence": "Not stated; free for emulators, credit the author",
      "notes": "Textures only; the pack's ReShade files are not used. Version 5.0a. Size not confirmed (Google Drive was over its download limit when checked)."
    },
    {
      "id": "oot3d-1080p-henriko",
      "name": "Ocarina of Time 3D 1080p",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "Henriko Magnifico",
      "author_url": "https://www.henrikomagnifico.com/zelda-ocarina-of-time-3d-4k",
      "source": {
        "type": "gdrive_folder",
        "folder_id": "1QQ-4b8SvR2d4VKKQNS-I1WOIB5EaTuzt",
        "file_pattern": "OOT3D 4K * (1080p).zip"
      },
      "alternates": [
        {
          "type": "gdrive_folder",
          "folder_id": "1F2lJ_TJyZovp5TtKmqrl24fy97IdEh85",
          "file_pattern": "OOT3D 4K * (1080p).zip"
        }
      ],
      "size": 1908239755,
      "format": "zip",
      "layout": "single",
      "folder": "OoT3D 1080p",
      "licence": "Not stated; free for emulators, credit the author",
      "notes": "Textures only; the pack's ReShade files are not used. Version 5.0a, for 1080p screens."
    },
    {
      "id": "mm3d-4k-henriko",
      "name": "Majora's Mask 3D 4K",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "Henriko Magnifico",
      "author_url": "https://www.henrikomagnifico.com/zelda-majoras-mask-3d-4k",
      "source": {
        "type": "gdrive_folder",
        "folder_id": "13AxQkZj1YPvPvFWVguMDXWCBSB-ixLo0",
        "file_pattern": "MM 3D 4K * (4K).zip"
      },
      "size": 3687464465,
      "format": "zip",
      "layout": "single",
      "folder": "MM3D 4K",
      "licence": "Not stated; free for emulators, credit the author",
      "notes": "Textures only; the pack's ReShade files are not used. Version 3.0b. USA and Europe only."
    },
    {
      "id": "mm3d-1080p-henriko",
      "name": "Majora's Mask 3D 1080p",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "Henriko Magnifico",
      "author_url": "https://www.henrikomagnifico.com/zelda-majoras-mask-3d-4k",
      "source": {
        "type": "gdrive_folder",
        "folder_id": "13AxQkZj1YPvPvFWVguMDXWCBSB-ixLo0",
        "file_pattern": "MM 3D 4K * (1080p).zip"
      },
      "size": 1744246909,
      "format": "zip",
      "layout": "single",
      "folder": "MM3D 1080p",
      "licence": "Not stated; free for emulators, credit the author",
      "notes": "Textures only; the pack's ReShade files are not used. Version 3.0b, for 1080p screens. USA and Europe only."
    },
    {
      "id": "sm3dl-4k-henriko",
      "name": "Super Mario 3D Land 4K",
      "title": "Super Mario 3D Land",
      "title_ids": [
        "0004000000054000",
        "0004000000053F00"
      ],
      "author": "Henriko Magnifico",
      "author_url": "https://www.henrikomagnifico.com/super-mario-3d-land-hd",
      "source": {
        "type": "gdrive_folder",
        "folder_id": "1pTy-_VovJbgQh3Uotrx9Kd-5Ss-dK1-2",
        "file_pattern": "SM3DL 4K * (4K).zip"
      },
      "size": 183771391,
      "format": "zip",
      "layout": "single",
      "folder": "SM3DL 4K",
      "licence": "Not stated; free for emulators, credit the author",
      "notes": "Textures only; the pack's ReShade files are not used. Version 1.2.0b."
    },
    {
      "id": "sm3dl-1080p-henriko",
      "name": "Super Mario 3D Land 1080p",
      "title": "Super Mario 3D Land",
      "title_ids": [
        "0004000000054000",
        "0004000000053F00"
      ],
      "author": "Henriko Magnifico",
      "author_url": "https://www.henrikomagnifico.com/super-mario-3d-land-hd",
      "source": {
        "type": "gdrive_folder",
        "folder_id": "1pTy-_VovJbgQh3Uotrx9Kd-5Ss-dK1-2",
        "file_pattern": "SM3DL 4K * (1080p).zip"
      },
      "size": 107111023,
      "format": "zip",
      "layout": "single",
      "folder": "SM3DL 1080p",
      "licence": "Not stated; free for emulators, credit the author",
      "notes": "Textures only; the pack's ReShade files are not used. Version 1.2.0b, for 1080p screens."
    },
    {
      "id": "albw-4k-henriko",
      "name": "A Link Between Worlds 4K",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300"
      ],
      "author": "Henriko Magnifico",
      "author_url": "https://www.henrikomagnifico.com/zelda-a-link-between-worlds-4k",
      "source": {
        "type": "gdrive_folder",
        "folder_id": "1Io5Qw5L1Iep7WQja-5G9NyxkFqMkkSLv",
        "file_pattern": "ALBW 4K * (4K).zip"
      },
      "size": 526066444,
      "format": "zip",
      "layout": "single",
      "folder": "ALBW 4K",
      "licence": "Not stated; free for emulators, credit the author",
      "notes": "Textures only; the pack's ReShade files are not used. Version 2.0b. USA only (the pack is made for the USA game)."
    },
    {
      "id": "albw-1080p-henriko",
      "name": "A Link Between Worlds 1080p",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300"
      ],
      "author": "Henriko Magnifico",
      "author_url": "https://www.henrikomagnifico.com/zelda-a-link-between-worlds-4k",
      "source": {
        "type": "gdrive_folder",
        "folder_id": "1Io5Qw5L1Iep7WQja-5G9NyxkFqMkkSLv",
        "file_pattern": "ALBW 4K * (1080p).zip"
      },
      "size": 191955611,
      "format": "zip",
      "layout": "single",
      "folder": "ALBW 1080p",
      "licence": "Not stated; free for emulators, credit the author",
      "notes": "Textures only; the pack's ReShade files are not used. Version 2.0b, for 1080p screens. USA only."
    }
  ]
}
)JSON";
}

} // namespace CustomTex::Download
