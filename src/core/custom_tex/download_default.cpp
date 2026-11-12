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
    }
  ]
}
)JSON";
}

} // namespace CustomTex::Download
