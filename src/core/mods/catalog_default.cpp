// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame moddata: the catalog compiled in. A copy of catalog/mods.json of
// simplysanatori/3ds-game-fixes; keep the two in step (a test parses this copy).

#include "core/mods/catalog.h"

namespace Mods {

const char* DefaultCatalogJson() {
    return R"AZCAT({
  "schema": 1,
  "mods": [
    {
      "id": "stella-glow-usa-no-bloom",
      "name": "No Bloom",
      "title": "Stella Glow (USA)",
      "title_ids": [
        "0004000000173700"
      ],
      "author": "simplysanatori",
      "licence": "MIT",
      "homepage": "https://github.com/simplysanatori/3ds-game-fixes/tree/main/games/stella-glow-usa",
      "verified": true,
      "source": {
        "type": "owner_repo_path",
        "path": "games/stella-glow-usa/patch/0004000000173700/code.ips"
      },
      "install": {
        "rules": [
          {
            "from": "code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the glow and doubled outlines around characters at higher internal resolutions."
    },
    {
      "id": "ocarina-reframed-single-screen",
      "name": "Ocarina ReFramed (Single Screen)",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400",
        "000400000008F800",
        "000400000008F900"
      ],
      "author": "OTPR26",
      "licence": "GPL-3.0-or-later (source); HUD art and game data are not GPL",
      "homepage": "https://github.com/OTPR26/OOT3DHud",
      "verified": false,
      "source": {
        "type": "github_release",
        "repo": "OTPR26/OOT3DHud",
        "tag": "latest",
        "asset_pattern": "Ocarina-ReFramed-*-Single-Screen-Mods.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Covers all regions (North America, Europe, Japan, Korea, Taiwan); the folder for the game's own title ID is used. Replaces any other code patch for the game."
    },
    {
      "id": "ocarina-reframed-dual-screen",
      "name": "Ocarina ReFramed (Dual Screen)",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400",
        "000400000008F800",
        "000400000008F900"
      ],
      "author": "OTPR26",
      "licence": "GPL-3.0-or-later (source); HUD art and game data are not GPL",
      "homepage": "https://github.com/OTPR26/OOT3DHud",
      "verified": false,
      "source": {
        "type": "github_release",
        "repo": "OTPR26/OOT3DHud",
        "tag": "latest",
        "asset_pattern": "Ocarina-ReFramed-*-Dual-Screen-Mods.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Covers all regions (North America, Europe, Japan, Korea, Taiwan); the folder for the game's own title ID is used. Replaces any other code patch for the game."
    },
    {
      "id": "mm3d-project-restoration-v100",
      "name": "Project Restoration",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600",
        "00040000000D6E00"
      ],
      "author": "leoetlino",
      "licence": "GPL-2.0",
      "homepage": "https://github.com/leoetlino/project-restoration",
      "verified": false,
      "source": {
        "type": "github_release",
        "repo": "leoetlino/project-restoration",
        "tag": "v1.5.9",
        "asset_pattern": "mm3d_project_restoration_v*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "v100/citra/code.bps",
            "to": "code.bps"
          },
          {
            "from": "v100/citra/exheader.bin",
            "to": "exheader.bin"
          }
        ]
      },
      "notes": "MM3D 1.0, or 1.1 with the update. The Korean release is not supported."
    },
    {
      "id": "mm3d-project-restoration-v110",
      "name": "Project Restoration (Card 1.1)",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600",
        "00040000000D6E00"
      ],
      "author": "leoetlino",
      "licence": "GPL-2.0",
      "homepage": "https://github.com/leoetlino/project-restoration",
      "verified": false,
      "source": {
        "type": "github_release",
        "repo": "leoetlino/project-restoration",
        "tag": "v1.5.9",
        "asset_pattern": "mm3d_project_restoration_v*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "v110/citra/code.bps",
            "to": "code.bps"
          },
          {
            "from": "v110/citra/exheader.bin",
            "to": "exheader.bin"
          }
        ]
      },
      "notes": "MM3D 1.1 game card (1.1 preinstalled). The Korean release is not supported."
    },
    {
      "id": "mm3d-project-restoration-v101",
      "name": "Project Restoration (v101)",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600",
        "00040000000D6E00"
      ],
      "author": "leoetlino",
      "licence": "GPL-2.0",
      "homepage": "https://github.com/leoetlino/project-restoration",
      "verified": false,
      "source": {
        "type": "github_release",
        "repo": "leoetlino/project-restoration",
        "tag": "v1.5.9",
        "asset_pattern": "mm3d_project_restoration_v*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "v101/citra/code.bps",
            "to": "code.bps"
          },
          {
            "from": "v101/citra/exheader.bin",
            "to": "exheader.bin"
          }
        ]
      },
      "notes": "Only if the first one does not work. The Korean release is not supported."
    }
  ]
}
)AZCAT";
}

} // namespace Mods
