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
      "id": "ocarina-single-screen-experience",
      "name": "Single Screen Experience",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "M-1",
      "author_url": "https://gamebanana.com/members/1544353",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/695893",
      "verified": true,
      "downloads": 53068,
      "updated": "2026-11",
      "source": {
        "type": "gamebanana",
        "mod": 695893,
        "file_pattern": "singlescreen_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/*/Emulator/load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "textures": {
        "rules": [
          {
            "from": "*/*/Emulator/load/textures/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Puts the whole game on one screen with no touch screen needed, adds quality-of-life changes and an HD font, for the USA, Europe and Japan versions. It replaces any other code patch for the game, such as Ocarina ReFramed. Its three optional HD UI textures are installed too."
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
      "downloads": 193,
      "updated": "2026-10",
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
      "downloads": 82,
      "updated": "2026-10",
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
      "downloads": 158682,
      "updated": "2022-01",
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
      "notes": "The Korean release is not supported. Conflicts with the other Project Restoration builds.",
      "version_chip": {
        "label": "1.0",
        "tip": "For the original 1.0 game, or 1.0 with the 1.1 update.",
        "code_crc32": [
          "c75021d9"
        ]
      }
    },
    {
      "id": "mm3d-project-restoration-v110",
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
      "downloads": 158682,
      "updated": "2022-01",
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
      "notes": "The Korean release is not supported. Conflicts with the other Project Restoration builds.",
      "version_chip": {
        "label": "Card 1.1",
        "tip": "For game cards with 1.1 built in.",
        "code_crc32": [
          "ffdf1ee8"
        ]
      }
    },
    {
      "id": "mm3d-project-restoration-v101",
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
      "downloads": 158682,
      "updated": "2022-01",
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
      "notes": "The Korean release is not supported. Conflicts with the other Project Restoration builds.",
      "version_chip": {
        "label": "Fallback",
        "tip": "Try this if the first one doesn't work.",
        "code_crc32": [
          "e972fd45"
        ]
      }
    },
    {
      "id": "oot3d-vanilla-plus-link-remodel",
      "name": "Vanilla+ Link Remodel",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "HugAMuffin",
      "author_url": "https://gamebanana.com/members/1608504",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/705890",
      "verified": false,
      "downloads": 9089,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 705890,
        "file_pattern": "link_remodel_vanilla_plus_*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "actor/",
            "to": "romfs/actor/"
          },
          {
            "from": "misc/",
            "to": "romfs/misc/"
          }
        ]
      },
      "notes": "Replaces Link and the models of his items and equipment with more detailed ones, keeping the original texture layout so texture mods still fit. The files are not tied to a region, and the page names no region, so treat the Europe and Japan versions as untested. It replaces the same model files as Better Link and Better Remaster Experience, so use only one of them."
    },
    {
      "id": "oot3d-better-link-v3",
      "name": "Better Link v3",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "skilarbabcock",
      "author_url": "https://gamebanana.com/members/1672388",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/379045",
      "verified": false,
      "downloads": 8770,
      "updated": "2022-11",
      "source": {
        "type": "gamebanana",
        "mod": 379045,
        "file_pattern": "betterlinkv3_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "BetterLinkv3/USA/NormalRandomizerFriendly/Citra/user/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "BetterLinkv3/*/Citra/user/load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Remodels Link, the Master Sword and other equipment, with a build for each region. The USA install is the randomizer-friendly build, though the author says the randomizer's colour options do not work with it. It replaces the same model files as Vanilla+ and Better Remaster Experience, so use only one of them."
    },
    {
      "id": "oot3d-better-link-v3-longer-sword",
      "name": "Better Link v3 (Longer Sword Hitbox)",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500"
      ],
      "author": "skilarbabcock",
      "author_url": "https://gamebanana.com/members/1672388",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/379045",
      "verified": false,
      "downloads": 8770,
      "updated": "2022-11",
      "source": {
        "type": "gamebanana",
        "mod": 379045,
        "file_pattern": "betterlinkv3_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "BetterLinkv3/USA/LongerSwordHitbox/Citra/user/load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "The same Better Link v3 models for the USA version, with a code patch that lengthens Link's sword hitbox. The code patch replaces any other one for the game, such as Single Screen Experience's, so this suits a game without it."
    },
    {
      "id": "oot3d-better-remaster-experience",
      "name": "Better Remaster Experience",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600"
      ],
      "author": "Redstormmars",
      "author_url": "https://gamebanana.com/members/4963393",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/633569",
      "verified": false,
      "downloads": 2598,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 633569,
        "file_pattern": "oot_3d_better_remaster_experience_*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "*/*/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Brings back looks from the N64 game, with a darker Forest Temple, a revised Link and changed scenes and models, for the USA and Europe versions. Its New 3DS C-stick camera patch (code.ips and exheader.bin) is not installed, so it does not clash with Single Screen Experience. It replaces the same model files as Vanilla+ and Better Link, so use only one of them."
    },
    {
      "id": "mm3d-dirtier-stock-pot-inn",
      "name": "Dirtier Stock Pot Inn",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "TheTigershark",
      "author_url": "https://gamebanana.com/members/5683774",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/713840",
      "verified": false,
      "downloads": 71,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 713840,
        "file_pattern": "dirtier_stock_pot_inn_3ds_port.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Makes the Stock Pot Inn dirtier and more run down, closer to the N64 version, by replacing its scene files. It is a port of the PC mod by Takeshre. The author names the Europe and USA versions."
    },
    {
      "id": "mm3d-legend-of-lillie",
      "name": "The Legend of Lillie",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500"
      ],
      "author": "Camyoshi",
      "author_url": "https://gamebanana.com/members/3247750",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/685525",
      "verified": false,
      "downloads": 313,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 685525,
        "file_pattern": "the_legend_of_lillie_majora_s_mask_3d_v*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces Link and his four masked forms with Lillie from Pokemon Sun and Moon, plus new expressions, text and voice clips. It is for the USA version only, since the changed dialogue works only there. It replaces Link's model files, so it clashes with other Link-model mods."
    },
    {
      "id": "kiu-cd-quality-soundtrack",
      "name": "CD Quality Soundtrack",
      "title": "Kid Icarus: Uprising",
      "title_ids": [
        "0004000000030100"
      ],
      "author": "Zara Nerd",
      "author_url": "https://gamebanana.com/members/6074804",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/734332",
      "verified": false,
      "downloads": 9,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 734332,
        "file_pattern": "kid_icarus_uprising_cd_quality_soundtrack.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the music with 44.1 kHz versions cut from a lossless album rip, for the USA version. Two tracks (the Vault theme and the one after Dark Pit joins in Chapter 21) are unchanged, and some tracks may click or pop where they loop. The download is about 650 MB."
    },
    {
      "id": "kiu-dark-pit-over-pit",
      "name": "Dark Pit Over Pit",
      "title": "Kid Icarus: Uprising",
      "title_ids": [
        "0004000000030100"
      ],
      "author": "M-1",
      "author_url": "https://gamebanana.com/members/1544353",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/248300",
      "verified": false,
      "downloads": 425,
      "updated": "2017-11",
      "source": {
        "type": "gamebanana",
        "mod": 248300,
        "file_pattern": "840_luma.zip"
      },
      "install": {
        "rules": [
          {
            "from": "luma/titles/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Puts Dark Pit's textures over Pit's. It is for the USA version only, since the file sits in a US-named folder. The author says it is safe for online play."
    },
    {
      "id": "dq8-even-easier-zoom-evac",
      "name": "Even Easier Zoom + Evac",
      "title": "Dragon Quest VIII: Journey of the Cursed King",
      "title_ids": [
        "000400000018F100",
        "000400000018F200"
      ],
      "author": "AlyenOllie",
      "author_url": "https://gamebanana.com/members/2785266",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/464464",
      "verified": false,
      "downloads": 273,
      "updated": "2023-08",
      "source": {
        "type": "gamebanana",
        "mod": 464464,
        "file_pattern": "easierzoom_evac.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets every party member use Zoom and Evac from level 1 at 0 MP, which makes Chimaera Wings pointless. The Hero's Zoom skill slot now gives 5 maximum HP instead. It replaces skill and parameter files, so it can clash with other gameplay mods."
    },
    {
      "id": "dq8-guest-party-name-fix",
      "name": "Guest Party Member Name Fix",
      "title": "Dragon Quest VIII: Journey of the Cursed King",
      "title_ids": [
        "000400000018F100",
        "000400000018F200"
      ],
      "author": "AlyenOllie",
      "author_url": "https://gamebanana.com/members/2785266",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/464067",
      "verified": false,
      "downloads": 223,
      "updated": "2023-08",
      "source": {
        "type": "gamebanana",
        "mod": 464067,
        "file_pattern": "namefix.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Fixes the names of guest party members and fusion monsters in the battle display, so Prince Charmles is no longer called Damon. It replaces the monster table and word tables, so it can clash with other mods that edit them."
    },
    {
      "id": "dq8-playable-evil-jessica",
      "name": "Playable Evil Jessica",
      "title": "Dragon Quest VIII: Journey of the Cursed King",
      "title_ids": [
        "000400000018F100",
        "000400000018F200"
      ],
      "author": "AlyenOllie",
      "author_url": "https://gamebanana.com/members/2785266",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/565132",
      "verified": false,
      "downloads": 78,
      "updated": "2025-03",
      "source": {
        "type": "gamebanana",
        "mod": 565132,
        "file_pattern": "romfs_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Retextures Jessica with Evil Jessica's textures, including evil versions of her alternate costumes. It may not work with the HD texture pack."
    },
    {
      "id": "mk7-sonic",
      "name": "Sonic",
      "title": "Mario Kart 7",
      "title_ids": [
        "0004000000030800",
        "0004000000030700",
        "0004000000030600"
      ],
      "author": "Blurro",
      "author_url": "https://gamebanana.com/members/1672097",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/249447",
      "verified": false,
      "downloads": 7297,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 249447,
        "file_pattern": "sonic_release.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Driver/",
            "to": "romfs/Driver/"
          },
          {
            "from": "*/Kart/",
            "to": "romfs/Kart/"
          },
          {
            "from": "*/Sound/",
            "to": "romfs/Sound/"
          },
          {
            "from": "*/UI/",
            "to": "romfs/UI/"
          },
          {
            "from": "*/Thankyou3D.szs",
            "to": "romfs/Thankyou3D.szs"
          }
        ]
      },
      "notes": "Replaces Donkey Kong with Sonic: the driver, kart, emblem, voice clips and menu art (North America, Europe and Japan). It works on the unmodified game and does not need CTGP-7. Donkey Kong is gone while it is installed; the Christmas hat version and the CTGP-7 pack are not installed."
    },
    {
      "id": "sm3dl-white-screen-fix",
      "name": "White Screen Fix",
      "title": "Super Mario 3D Land",
      "title_ids": [
        "0004000000054000"
      ],
      "author": "ffman1985",
      "author_url": "https://gamebanana.com/members/5494707",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/677712",
      "verified": false,
      "downloads": 245,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 677712,
        "file_pattern": "*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "Romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Stops the game crashing to a white screen after beating Bowser in World 8, by removing the shining-star effect from the file select layout (North America only). The shining stars still show on the save's info page."
    },
    {
      "id": "sm3dl-new-super-mario-3d-land",
      "name": "New Super Mario 3D Land",
      "title": "Super Mario 3D Land",
      "title_ids": [
        "0004000000054000",
        "0004000000053F00"
      ],
      "author": "Nick64 Mods",
      "author_url": "https://gamebanana.com/members/2394426",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/429633",
      "verified": false,
      "downloads": 3767,
      "updated": "2025-04",
      "source": {
        "type": "gamebanana",
        "mod": 429633,
        "file_pattern": "nsm3dl_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "-TITLE ID-/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Four new worlds with custom levels, textures and music. The author tested it on a real 3DS and says it should work in an emulator; use a new save. Replaces other Super Mario 3D Land level mods, which edit the same files."
    },
    {
      "id": "sm3dl-super-mario-escape",
      "name": "Super Mario Escape",
      "title": "Super Mario 3D Land",
      "title_ids": [
        "0004000000054000",
        "0004000000053F00"
      ],
      "author": "Nick64 Mods",
      "author_url": "https://gamebanana.com/members/2394426",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/589964",
      "verified": false,
      "downloads": 671,
      "updated": "2026-02",
      "source": {
        "type": "gamebanana",
        "mod": 589964,
        "file_pattern": "supermarioescape_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A new adventure of about one and a half worlds, with changed Toad Houses and item boxes and a secret stage. Use a new save. Replaces other Super Mario 3D Land level mods, which edit the same files."
    },
    {
      "id": "sm3dl-super-mario-expedition",
      "name": "Super Mario Expedition",
      "title": "Super Mario 3D Land",
      "title_ids": [
        "0004000000054000",
        "0004000000053F00"
      ],
      "author": "Cy8ro",
      "author_url": "https://gamebanana.com/members/1748596",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/376715",
      "verified": false,
      "downloads": 7749,
      "updated": "2022-12",
      "source": {
        "type": "gamebanana",
        "mod": 376715,
        "file_pattern": "sme_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A one-world mod built around exploration. Its optional custom world map file (GameSystemDataTable.szs) is not installed, since it needs a fresh save. Replaces other Super Mario 3D Land level mods, which edit the same files."
    },
    {
      "id": "sm3dl-super-mario-expedition-2",
      "name": "Super Mario Expedition 2",
      "title": "Super Mario 3D Land",
      "title_ids": [
        "0004000000054000",
        "0004000000053F00"
      ],
      "author": "Cy8ro",
      "author_url": "https://gamebanana.com/members/1748596",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/570759",
      "verified": false,
      "downloads": 4622,
      "updated": "2025-01",
      "source": {
        "type": "gamebanana",
        "mod": 570759,
        "file_pattern": "super_mario_expedition_2_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A one-world sequel to Super Mario Expedition with five new levels and a credits level, plus its own title logo and music. Replaces other Super Mario 3D Land level mods, which edit the same files."
    },
    {
      "id": "fates-ghosting-shader-fix-us-se",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Fates Special Edition (USA)",
      "title_ids": [
        "0004000000179800"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/51424",
      "verified": false,
      "downloads": 23256,
      "updated": "2020-12",
      "source": {
        "type": "gamebanana",
        "mod": 51424,
        "file_pattern": "shaderfix.zip"
      },
      "install": {
        "rules": [
          {
            "from": "US SE/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting around characters at higher internal resolutions (North America Special Edition). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "fates-ghosting-shader-fix-eu-se",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Fates Special Edition (Europe)",
      "title_ids": [
        "000400000017A800"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/51424",
      "verified": false,
      "downloads": 23256,
      "updated": "2020-12",
      "source": {
        "type": "gamebanana",
        "mod": 51424,
        "file_pattern": "shaderfix.zip"
      },
      "install": {
        "rules": [
          {
            "from": "EU SE/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting around characters at higher internal resolutions (Europe Special Edition). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "fates-ghosting-shader-fix-jp",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Fates (Japan)",
      "title_ids": [
        "000400000012DE00",
        "000400000012DC00",
        "000400000012DD00"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/51424",
      "verified": false,
      "downloads": 23256,
      "updated": "2020-12",
      "source": {
        "type": "gamebanana",
        "mod": 51424,
        "file_pattern": "shaderfix.zip"
      },
      "install": {
        "rules": [
          {
            "from": "JPN SE CQ BR/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting around characters at higher internal resolutions (Japan: Special Edition, Birthright and Conquest). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "fates-ghosting-shader-fix-us-bc",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Fates Birthright / Conquest (USA)",
      "title_ids": [
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/51424",
      "verified": false,
      "downloads": 23256,
      "updated": "2020-12",
      "source": {
        "type": "gamebanana",
        "mod": 51424,
        "file_pattern": "shaderfix.zip"
      },
      "install": {
        "rules": [
          {
            "from": "US CQ BR/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting around characters at higher internal resolutions (North America Birthright and Conquest). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "fates-ghosting-shader-fix-eu-bc",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Fates Birthright / Conquest (Europe)",
      "title_ids": [
        "0004000000179500",
        "0004000000179700"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/51424",
      "verified": false,
      "downloads": 23256,
      "updated": "2020-12",
      "source": {
        "type": "gamebanana",
        "mod": 51424,
        "file_pattern": "shaderfix.zip"
      },
      "install": {
        "rules": [
          {
            "from": "EU CQ BR/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting around characters at higher internal resolutions (Europe Birthright and Conquest). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "fates-paralogue-unlocker",
      "name": "Paralogue Unlocker",
      "title": "Fire Emblem Fates",
      "title_ids": [
        "0004000000179800",
        "000400000017A800",
        "000400000012DE00",
        "0004000000179400",
        "0004000000179600",
        "0004000000179500",
        "0004000000179700",
        "000400000012DC00",
        "000400000012DD00"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/51421",
      "verified": false,
      "downloads": 19272,
      "updated": "2020-01",
      "source": {
        "type": "gamebanana",
        "mod": 51421,
        "file_pattern": "gay_fates_ips_patches_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Lifts the same-sex marriage restriction on paralogues, for the Special Edition, Birthright and Conquest in all three regions. It is a code patch, so it replaces any other code patch for the game. Unofficial Gay Fates already includes this on the North America and Europe Special Edition."
    },
    {
      "id": "fates-icon-project",
      "name": "Fates Icon Project",
      "title": "Fire Emblem Fates",
      "title_ids": [
        "0004000000179800",
        "000400000017A800",
        "000400000012DE00",
        "0004000000179400",
        "0004000000179600",
        "0004000000179500",
        "0004000000179700",
        "000400000012DC00",
        "000400000012DD00"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/34160",
      "verified": false,
      "downloads": 29911,
      "updated": "2021-05",
      "source": {
        "type": "gamebanana",
        "mod": 34160,
        "file_pattern": "regular_version_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives every item its own hand-made icon, over 200 in all. This is the plain version; the page does not name a region. It replaces GameData.bin.lz, so it clashes with Unofficial Gay Fates, whose own icon packs on the page are not installed."
    },
    {
      "id": "fates-icon-project-weapon-triangle",
      "name": "Fates Icon Project (Weapon Triangle)",
      "title": "Fire Emblem Fates",
      "title_ids": [
        "0004000000179800",
        "000400000017A800",
        "000400000012DE00",
        "0004000000179400",
        "0004000000179600",
        "0004000000179500",
        "0004000000179700",
        "000400000012DC00",
        "000400000012DD00"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/34160",
      "verified": false,
      "downloads": 29911,
      "updated": "2021-05",
      "source": {
        "type": "gamebanana",
        "mod": 34160,
        "file_pattern": "background_version_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The Fates Icon Project with a weapon triangle background behind each icon. The page does not name a region. It replaces GameData.bin.lz, so it clashes with Unofficial Gay Fates and with the plain icon version."
    },
    {
      "id": "fates-unofficial-gay-fates",
      "name": "Unofficial Gay Fates",
      "title": "Fire Emblem Fates",
      "title_ids": [
        "0004000000179800",
        "000400000017A800",
        "000400000012DE00",
        "0004000000179400",
        "0004000000179600",
        "0004000000179500",
        "0004000000179700",
        "000400000012DC00",
        "000400000012DD00"
      ],
      "author": "Milfnuggy",
      "author_url": "https://gamebanana.com/members/1683519",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/51420",
      "verified": false,
      "downloads": 57852,
      "updated": "2026-07",
      "source": {
        "type": "gamebanana",
        "mod": 51420,
        "file_pattern": "unofficial_gay_fates_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "*/Unofficial Gay Fates/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "A standalone fan expansion with about a thousand same-sex and platonic supports and custom endings, for every Fates edition. On the Special Edition in North America and Europe it ships its own code patch, so it replaces other code patches; the other editions need the Paralogue Unlocker. On a Europe copy in another language than English, rename romfs/m/@U to your language letter. The optional add-ons are not installed."
    },
    {
      "id": "echoes-60fps-us",
      "name": "True 60 FPS (No Speedup)",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4000"
      ],
      "author": "Phenceon",
      "author_url": "https://gamebanana.com/members/2483164",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/720762",
      "verified": false,
      "downloads": 231,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 720762,
        "file_pattern": "fire_emblem_echoes_60_fps_patches_*.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For game update 1.1."
      },
      "install": {
        "rules": [
          {
            "from": "Fire Emblem Echoes Shadows of Valentia USA v1.1/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Runs the game at 60 FPS without speeding up the game itself (North America). It needs game update 1.1, and is a code patch that replaces any other code patch, such as the ghosting shader fix."
    },
    {
      "id": "echoes-60fps-eu",
      "name": "True 60 FPS (No Speedup)",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4100"
      ],
      "author": "Phenceon",
      "author_url": "https://gamebanana.com/members/2483164",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/720762",
      "verified": false,
      "downloads": 231,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 720762,
        "file_pattern": "fire_emblem_echoes_60_fps_patches_*.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For game update 1.1.",
        "code_crc32": [
          "23cc4afb"
        ]
      },
      "install": {
        "rules": [
          {
            "from": "Fire Emblem Echoes Shadows of Valentia EUR v1.1/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Runs the game at 60 FPS without speeding up the game itself (Europe). It needs game update 1.1, and is a code patch that replaces any other code patch, such as the ghosting shader fix."
    },
    {
      "id": "echoes-60fps-jp",
      "name": "True 60 FPS (No Speedup)",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001A2B00"
      ],
      "author": "Phenceon",
      "author_url": "https://gamebanana.com/members/2483164",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/720762",
      "verified": false,
      "downloads": 231,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 720762,
        "file_pattern": "fire_emblem_echoes_60_fps_patches_*.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For game update 1.1."
      },
      "install": {
        "rules": [
          {
            "from": "Fire Emblem Echoes Mou Hitori no Eiyuu Ou JPN v1.1/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Runs the game at 60 FPS without speeding up the game itself (Japan). It needs game update 1.1, and is a code patch that replaces any other code patch, such as the ghosting shader fix."
    },
    {
      "id": "echoes-ghosting-shader-fix-us-v10",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4000"
      ],
      "author": "ilikepizza107",
      "author_url": "https://gamebanana.com/members/2003295",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/519419",
      "verified": false,
      "downloads": 2886,
      "updated": "2024-06",
      "source": {
        "type": "gamebanana",
        "mod": 519419,
        "file_pattern": "patches_*.zip"
      },
      "version_chip": {
        "label": "1.0",
        "tip": "For game version 1.0."
      },
      "install": {
        "rules": [
          {
            "from": "USv1.0.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting and blown-out brightness that emulators show at higher resolutions (North America, game version 1.0). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "echoes-ghosting-shader-fix-us-v11",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4000"
      ],
      "author": "ilikepizza107",
      "author_url": "https://gamebanana.com/members/2003295",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/519419",
      "verified": false,
      "downloads": 2886,
      "updated": "2024-06",
      "source": {
        "type": "gamebanana",
        "mod": 519419,
        "file_pattern": "patches_*.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For game version 1.1."
      },
      "install": {
        "rules": [
          {
            "from": "USv1.1.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting and blown-out brightness that emulators show at higher resolutions (North America, game version 1.1). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "echoes-ghosting-shader-fix-eu-v10",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4100"
      ],
      "author": "ilikepizza107",
      "author_url": "https://gamebanana.com/members/2003295",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/519419",
      "verified": false,
      "downloads": 2886,
      "updated": "2024-06",
      "source": {
        "type": "gamebanana",
        "mod": 519419,
        "file_pattern": "patches_*.zip"
      },
      "version_chip": {
        "label": "1.0",
        "tip": "For the game without its update (version 1.0).",
        "code_crc32": [
          "40d93b41"
        ]
      },
      "install": {
        "rules": [
          {
            "from": "EUv1.0.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting and blown-out brightness that emulators show at higher resolutions (Europe, game version 1.0). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "echoes-ghosting-shader-fix-eu-v11",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4100"
      ],
      "author": "ilikepizza107",
      "author_url": "https://gamebanana.com/members/2003295",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/519419",
      "verified": false,
      "downloads": 2886,
      "updated": "2024-06",
      "source": {
        "type": "gamebanana",
        "mod": 519419,
        "file_pattern": "patches_*.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For the game with its 1.1 update.",
        "code_crc32": [
          "23cc4afb"
        ]
      },
      "install": {
        "rules": [
          {
            "from": "EUv1.1.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting and blown-out brightness that emulators show at higher resolutions (Europe, game version 1.1). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "echoes-ghosting-shader-fix-jp-v10",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001A2B00"
      ],
      "author": "ilikepizza107",
      "author_url": "https://gamebanana.com/members/2003295",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/519419",
      "verified": false,
      "downloads": 2886,
      "updated": "2024-06",
      "source": {
        "type": "gamebanana",
        "mod": 519419,
        "file_pattern": "patches_*.zip"
      },
      "version_chip": {
        "label": "1.0",
        "tip": "For game version 1.0."
      },
      "install": {
        "rules": [
          {
            "from": "JPv1.0.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting and blown-out brightness that emulators show at higher resolutions (Japan, game version 1.0). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "echoes-ghosting-shader-fix-jp-v11",
      "name": "Ghosting Shader Fix",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001A2B00"
      ],
      "author": "ilikepizza107",
      "author_url": "https://gamebanana.com/members/2003295",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/519419",
      "verified": false,
      "downloads": 2886,
      "updated": "2024-06",
      "source": {
        "type": "gamebanana",
        "mod": 519419,
        "file_pattern": "patches_*.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For game version 1.1."
      },
      "install": {
        "rules": [
          {
            "from": "JPv1.1.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Removes the ghosting and blown-out brightness that emulators show at higher resolutions (Japan, game version 1.1). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "echoes-gender-lock-removal",
      "name": "Gender-lock Removal v3",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4000"
      ],
      "author": "LJ_Reflet",
      "author_url": "https://gamebanana.com/members/1976044",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/313495",
      "verified": false,
      "downloads": 2662,
      "updated": "2025-07",
      "source": {
        "type": "gamebanana",
        "mod": 313495,
        "file_pattern": "echoesglr_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets male units become Pegasus Knights and Clerics and female units become Mercenaries, Archers and Soldiers, with new models and textures for the added classes. The page names the North America game only. Overclasses do not work with the new classes, and some spells cause slight lag."
    },
    {
      "id": "echoes-sov-skin-tweaks",
      "name": "SoV Skin Tweaks",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4000",
        "00040000001B4100"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/253291",
      "verified": false,
      "downloads": 2056,
      "updated": "2020-05",
      "source": {
        "type": "gamebanana",
        "mod": 253291,
        "file_pattern": "sov_skin_tweaks.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Makes the skin of Atlas, Boey and Gray less green so it matches their portraits. The page does not name a region, so it is offered for North America and Europe."
    },
    {
      "id": "echoes-valentian-playable-bosses",
      "name": "Valentian Playable Bosses",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4000",
        "00040000001C6800",
        "00040000001A2B00"
      ],
      "author": "Oersted",
      "author_url": "https://gamebanana.com/members/1705581",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/658361",
      "verified": false,
      "downloads": 243,
      "updated": "2026-11",
      "source": {
        "type": "gamebanana",
        "mod": 658361,
        "file_pattern": "hydra_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Adds bosses that appear only the first time you enter Ram Woods and Novis Cemetery, so it needs a new save. The author lists North America, Korea and the Chinese version (also the Japan ID); the Europe version is not listed. Some of the new units reuse base textures."
    },
    {
      "id": "echoes-continuous-player-phase-music",
      "name": "Continuous Player Phase Music",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4100"
      ],
      "author": "SSJ2PhilCollins",
      "author_url": "https://gamebanana.com/members/5993835",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/732496",
      "verified": false,
      "downloads": 15,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 732496,
        "file_pattern": "continuous_player_music.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For game update 1.1.",
        "code_crc32": [
          "23cc4afb"
        ]
      },
      "install": {
        "rules": [
          {
            "from": "Continuous_Player_Music.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Keeps the Player Phase music playing through the enemy phase, and during support actions such as healing. Made for game update 1.1; it does not work inside DLC content or Thabes Labyrinth. The page names no region, but its offsets match the Europe game."
    },
    {
      "id": "echoes-continuous-player-phase-music-combat",
      "name": "Continuous Player Phase Music (With Combat)",
      "title": "Fire Emblem Echoes: Shadows of Valentia",
      "title_ids": [
        "00040000001B4100"
      ],
      "author": "SSJ2PhilCollins",
      "author_url": "https://gamebanana.com/members/5993835",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/732496",
      "verified": false,
      "downloads": 15,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 732496,
        "file_pattern": "continuous_player_music.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For game update 1.1.",
        "code_crc32": [
          "23cc4afb"
        ]
      },
      "install": {
        "rules": [
          {
            "from": "Continuous_Player_Music With Combat.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "The Continuous Player Phase Music patch that also replaces the generic combat music. Made for game update 1.1; it does not work inside DLC content or Thabes Labyrinth. The page names no region, but its offsets match the Europe game."
    },
    {
      "id": "awakening-60fps-us",
      "name": "True 60 FPS (No Speedup)",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "00040000000A0500"
      ],
      "author": "Phenceon",
      "author_url": "https://gamebanana.com/members/2483164",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/720758",
      "verified": false,
      "downloads": 547,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 720758,
        "file_pattern": "fire_emblem_awakening_60_fps_patches*.zip"
      },
      "version_chip": {
        "label": "1.0",
        "tip": "For game version 1.0."
      },
      "install": {
        "rules": [
          {
            "from": "Fire Emblem Awakening USA v1.0/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Runs the game at 60 FPS without speeding up the game itself (North America, game version 1.0). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "awakening-60fps-eu",
      "name": "True 60 FPS (No Speedup)",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "000400000009F100"
      ],
      "author": "Phenceon",
      "author_url": "https://gamebanana.com/members/2483164",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/720758",
      "verified": false,
      "downloads": 547,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 720758,
        "file_pattern": "fire_emblem_awakening_60_fps_patches*.zip"
      },
      "version_chip": {
        "label": "1.0",
        "tip": "For game version 1.0."
      },
      "install": {
        "rules": [
          {
            "from": "Fire Emblem Awakening EUR v1.0/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Runs the game at 60 FPS without speeding up the game itself (Europe, game version 1.0). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "awakening-60fps-jp",
      "name": "True 60 FPS (No Speedup)",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "0004000000072000"
      ],
      "author": "Phenceon",
      "author_url": "https://gamebanana.com/members/2483164",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/720758",
      "verified": false,
      "downloads": 547,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 720758,
        "file_pattern": "fire_emblem_awakening_60_fps_patches*.zip"
      },
      "version_chip": {
        "label": "1.0",
        "tip": "For game version 1.0."
      },
      "install": {
        "rules": [
          {
            "from": "Fire Emblem Kakusei JPN v1.0/code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Runs the game at 60 FPS without speeding up the game itself (Japan, game version 1.0). It is a code patch, so it replaces any other code patch for the game."
    },
    {
      "id": "awakening-textures-plus",
      "name": "Awakening Textures+",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "00040000000A0500",
        "000400000009F100",
        "0004000000072000"
      ],
      "author": "Mathcat, Moonling",
      "author_url": "https://gamebanana.com/members/1770166",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/252179",
      "verified": false,
      "downloads": 11816,
      "updated": "2024-10",
      "source": {
        "type": "gamebanana",
        "mod": 252179,
        "file_pattern": "*_awakening_textures_plus_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Files to Install/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives more characters and classes their own textures, and adds a few custom models. It shares bs/ComboTbl.bin.lz with Unofficial Gay Awakening, so the app will ask before replacing one with the other."
    },
    {
      "id": "awakening-spotpass-paralogues-na",
      "name": "SpotPass Paralogues Unlocked",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "00040000000A0500"
      ],
      "author": "Arrbes",
      "author_url": "https://gamebanana.com/members/1808041",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/384484",
      "verified": false,
      "downloads": 7884,
      "updated": "2022-06",
      "source": {
        "type": "gamebanana",
        "mod": 384484,
        "file_pattern": "*_na_.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Unlocks the Side Stories that normally need SpotPass: Side Story 18 opens at Chapter 25 and the rest once you reach the endgame (North America). Made for a vanilla playthrough."
    },
    {
      "id": "awakening-spotpass-paralogues-eu",
      "name": "SpotPass Paralogues Unlocked",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "000400000009F100"
      ],
      "author": "Arrbes",
      "author_url": "https://gamebanana.com/members/1808041",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/384484",
      "verified": false,
      "downloads": 7884,
      "updated": "2022-06",
      "source": {
        "type": "gamebanana",
        "mod": 384484,
        "file_pattern": "*_eur_.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Unlocks the Side Stories that normally need SpotPass: Side Story 18 opens at Chapter 25 and the rest once you reach the endgame (Europe). Made for a vanilla playthrough."
    },
    {
      "id": "awakening-unofficial-gay-awakening-na",
      "name": "Unofficial Gay Awakening",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "00040000000A0500"
      ],
      "author": "Manakete_Smoocher",
      "author_url": "https://gamebanana.com/members/1828619",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/424187",
      "verified": false,
      "downloads": 35526,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 424187,
        "file_pattern": "uga_full_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/m/E/",
            "to": "romfs/m/E/"
          },
          {
            "from": "*/romfs/",
            "to": "romfs/"
          },
          {
            "from": "*/code patches/NA.ips",
            "to": "code.ips"
          },
          {
            "from": "*/citra only/RelianceList.bin.lz",
            "to": "romfs/data/RelianceList.bin.lz"
          }
        ]
      },
      "notes": "Adds 500+ supports, romance options and new story scenes, and includes the emulator-only support log file (North America). It uses a code patch, so it replaces other code patches such as the 60 FPS patch. The optional add-ons are not installed."
    },
    {
      "id": "awakening-unofficial-gay-awakening-eu",
      "name": "Unofficial Gay Awakening",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "000400000009F100"
      ],
      "author": "Manakete_Smoocher",
      "author_url": "https://gamebanana.com/members/1828619",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/424187",
      "verified": false,
      "downloads": 35526,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 424187,
        "file_pattern": "uga_full_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/m/E/",
            "to": "romfs/m/U/"
          },
          {
            "from": "*/romfs/",
            "to": "romfs/"
          },
          {
            "from": "*/code patches/EU.ips",
            "to": "code.ips"
          },
          {
            "from": "*/citra only/RelianceList.bin.lz",
            "to": "romfs/data/RelianceList.bin.lz"
          }
        ]
      },
      "notes": "Adds 500+ supports, romance options and new story scenes, and includes the emulator-only support log file (Europe). The new text is English only, so set the game to English. It uses a code patch, so it replaces other code patches. The optional add-ons are not installed."
    },
    {
      "id": "awakening-unofficial-gay-awakening-jp",
      "name": "Unofficial Gay Awakening",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "0004000000072000"
      ],
      "author": "Manakete_Smoocher",
      "author_url": "https://gamebanana.com/members/1828619",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/424187",
      "verified": false,
      "downloads": 35526,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 424187,
        "file_pattern": "uga_full_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/m/E/",
            "to": "romfs/m/"
          },
          {
            "from": "*/romfs/",
            "to": "romfs/"
          },
          {
            "from": "*/code patches/JP.ips",
            "to": "code.ips"
          },
          {
            "from": "*/citra only/RelianceList.bin.lz",
            "to": "romfs/data/RelianceList.bin.lz"
          }
        ]
      },
      "notes": "Adds 500+ supports, romance options and new story scenes, and includes the emulator-only support log file (Japan). All added text is in English. It uses a code patch, so it replaces other code patches. The optional add-ons are not installed."
    },
    {
      "id": "awakening-remove-avatar-creation",
      "name": "Remove Avatar Creation",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "00040000000A0500"
      ],
      "author": "The_Kid9786",
      "author_url": "https://gamebanana.com/members/3069823",
      "licence": "MIT",
      "homepage": "https://gamebanana.com/mods/736542",
      "verified": false,
      "downloads": 1,
      "updated": "2026-11",
      "source": {
        "type": "gamebanana",
        "mod": 736542,
        "file_pattern": "fe13_removeavatarcreation.zip"
      },
      "install": {
        "rules": [
          {
            "from": "code.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Skips the avatar creation screen when you start a new game and uses the default Robin instead (North America only). It is a code patch, so it replaces any other code patch for the game, including the 60 FPS patch."
    },
    {
      "id": "kiu-cstick-gyro",
      "name": "C-Stick & Gyro",
      "title": "Kid Icarus: Uprising",
      "title_ids": [
        "0004000000030100",
        "0004000000030200",
        "0004000000030000"
      ],
      "author": "Darkshade",
      "author_url": "https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/",
      "licence": "Not stated",
      "homepage": "https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/",
      "verified": false,
      "version": "v172",
      "updated": "2026-10",
      "source": {
        "type": "mega",
        "link": "https://mega.nz/file/mS5QwCzb#bKRm4ltwCj8XN2Xd23os8yFiOaJD_zvA5xDTH5yF8rQ",
        "file_pattern": "*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Emulator/1 - Install First - Base Patch/*/load/mods/{tid}/exefs/",
            "to": "exefs/"
          },
          {
            "from": "*/Emulator/1 - Install First - Base Patch/*/load/mods/{tid}/exheader.bin",
            "to": "exheader.bin"
          },
          {
            "from": "*/Emulator/1 - Install First - Base Patch/*/load/mods/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Aim with the right stick (C-Stick) or gyro. For the USA, Europe and Japan versions. Replaces the game's code, so it clashes with other code patches. Hosted by the author on MEGA; only your region's files are downloaded."
    },
    {
      "id": "kiu-cstick-gyro-widescreen-16-9",
      "name": "Widescreen 16:9",
      "title": "Kid Icarus: Uprising",
      "title_ids": [
        "0004000000030100",
        "0004000000030200",
        "0004000000030000"
      ],
      "author": "Darkshade",
      "author_url": "https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/",
      "licence": "Not stated",
      "homepage": "https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/",
      "verified": false,
      "version": "v172",
      "updated": "2026-10",
      "source": {
        "type": "mega",
        "link": "https://mega.nz/file/mS5QwCzb#bKRm4ltwCj8XN2Xd23os8yFiOaJD_zvA5xDTH5yF8rQ",
        "file_pattern": "*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Emulator/2 - Optional - Widescreen/16-9 - Widescreen/*/load/mods/{tid}/exefs/",
            "to": "exefs/"
          },
          {
            "from": "*/Emulator/2 - Optional - Widescreen/16-9 - Widescreen/*/load/mods/{tid}/exheader.bin",
            "to": "exheader.bin"
          },
          {
            "from": "*/Emulator/1 - Install First - Base Patch/*/load/mods/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The C-Stick & Gyro patch with a 16:9 widescreen view. Includes the C-Stick & Gyro patch and replaces it, so install only one of the three."
    },
    {
      "id": "kiu-cstick-gyro-ultrawide-21-9",
      "name": "Ultrawide 21:9",
      "title": "Kid Icarus: Uprising",
      "title_ids": [
        "0004000000030100",
        "0004000000030200",
        "0004000000030000"
      ],
      "author": "Darkshade",
      "author_url": "https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/",
      "licence": "Not stated",
      "homepage": "https://gbatemp.net/threads/kid-icarus-uprising-c-stick-gyro-patch.685775/",
      "verified": false,
      "version": "v172",
      "updated": "2026-10",
      "source": {
        "type": "mega",
        "link": "https://mega.nz/file/mS5QwCzb#bKRm4ltwCj8XN2Xd23os8yFiOaJD_zvA5xDTH5yF8rQ",
        "file_pattern": "*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Emulator/2 - Optional - Widescreen/21-9 - Ultrawide/*/load/mods/{tid}/exefs/",
            "to": "exefs/"
          },
          {
            "from": "*/Emulator/2 - Optional - Widescreen/21-9 - Ultrawide/*/load/mods/{tid}/exheader.bin",
            "to": "exheader.bin"
          },
          {
            "from": "*/Emulator/1 - Install First - Base Patch/*/load/mods/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The C-Stick & Gyro patch with a 21:9 ultrawide view. Includes the C-Stick & Gyro patch and replaces it, so install only one of the three."
    },
    {
      "id": "pokemon-xy-ash-greninja",
      "name": "Ash-Greninja",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "gastky",
      "author_url": "https://gamebanana.com/members/3265045",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/502123",
      "verified": false,
      "downloads": 5469,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 502123,
        "file_pattern": "ash_greninja_mod_*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "Ash Greninja Mod/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Adds Ash-Greninja as a new Mega Evolution for Greninja, using the Gen 7 stats and Protean, triggered by a new Greninjite found on the starter Froakie. It needs a base X or Y install and changes no trainers or wild Pokemon. The download is about 624 MB."
    },
    {
      "id": "pokemon-y-delta-over-serena",
      "name": "Delta over Serena",
      "title": "Pokemon Y",
      "title_ids": [
        "0004000000055E00"
      ],
      "author": "DesireDelta",
      "author_url": "https://gamebanana.com/members/1505014",
      "licence": "Not stated",
      "homepage": "https://gamebanana.com/mods/610259",
      "verified": false,
      "downloads": 3438,
      "updated": "2025-07",
      "source": {
        "type": "gamebanana",
        "mod": 610259,
        "file_pattern": "delta_over_serena.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Swaps Serena's default hair, skirt and legs for the model of the author's character Delta; her other outfits still work. It is for Pokemon Y only. Its zip uses Deflate64 compression, which the system bsdtar cannot unpack."
    },
    {
      "id": "pokemon-y-delta-lite-over-serena",
      "name": "Delta Lite over Serena",
      "title": "Pokemon Y",
      "title_ids": [
        "0004000000055E00"
      ],
      "author": "DesireDelta",
      "author_url": "https://gamebanana.com/members/1505014",
      "licence": "Not stated",
      "homepage": "https://gamebanana.com/mods/610259",
      "verified": false,
      "downloads": 3438,
      "updated": "2025-07",
      "source": {
        "type": "gamebanana",
        "mod": 610259,
        "file_pattern": "delta_lite_over_serena.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The lighter variant of Delta over Serena, which swaps Serena's default hair, skirt and legs for the author's character Delta. It is for Pokemon Y only and conflicts with the full Delta mod. Its zip uses Deflate64 compression, which the system bsdtar cannot unpack."
    },
    {
      "id": "pokemon-x-shiny-lock-removal",
      "name": "Shiny Lock Removal",
      "title": "Pokemon X",
      "title_ids": [
        "0004000000055D00"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486567",
      "verified": false,
      "downloads": 3308,
      "updated": "2025-01",
      "source": {
        "type": "gamebanana",
        "mod": 486567,
        "file_pattern": "x_citra_shiny_lock_removal_40a3b.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from the static and gift Pokemon, such as the legendaries, Snorlax and Lucario, by replacing the game code and DllField.cro. It only works with the base 1.0.0 game, so no update data may be installed. Replaces other code patches for the game."
    },
    {
      "id": "pokemon-x-shiny-lock-removal-trades",
      "name": "Shiny Lock Removal (with In-Game Trades)",
      "title": "Pokemon X",
      "title_ids": [
        "0004000000055D00"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486567",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 486567,
        "file_pattern": "x_citra_shiny_lock_removal_trades.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The same shiny lock removal plus the in-game trade Pokemon, which needs a deeper code edit. A side effect is that trainers' Pokemon can also be shiny, so use it only when hunting a trade. It only works with the base 1.0.0 game, so no update data may be installed.",
      "downloads": 3308,
      "updated": "2025-01"
    },
    {
      "id": "pokemon-y-shiny-lock-removal",
      "name": "Shiny Lock Removal",
      "title": "Pokemon Y",
      "title_ids": [
        "0004000000055E00"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486567",
      "verified": false,
      "downloads": 3308,
      "updated": "2025-01",
      "source": {
        "type": "gamebanana",
        "mod": 486567,
        "file_pattern": "y_citra_shiny_lock_removal_e2e2b.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from the static and gift Pokemon, such as the legendaries, Snorlax and Lucario, by replacing the game code and DllField.cro. It only works with the base 1.0.0 game, so no update data may be installed. Replaces other code patches for the game."
    },
    {
      "id": "pokemon-y-shiny-lock-removal-trades",
      "name": "Shiny Lock Removal (with In-Game Trades)",
      "title": "Pokemon Y",
      "title_ids": [
        "0004000000055E00"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486567",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 486567,
        "file_pattern": "y_citra_shiny_lock_removal_trades.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The same shiny lock removal plus the in-game trade Pokemon, which needs a deeper code edit. A side effect is that trainers' Pokemon can also be shiny, so use it only when hunting a trade. It only works with the base 1.0.0 game, so no update data may be installed.",
      "downloads": 3308,
      "updated": "2025-01"
    },
    {
      "id": "pokemon-xy-accurate-sizes",
      "name": "Accurate Pokemon Sizes",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "The Pod",
      "author_url": "https://gamebanana.com/members/2034086",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/697028",
      "verified": false,
      "downloads": 528,
      "updated": "2026-07",
      "source": {
        "type": "gamebanana",
        "mod": 697028,
        "file_pattern": "accuratepokemonsizesxy.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Scales the battle size of every Pokemon to its real-world size, in the style of True Scale, by replacing one model data file. The download is about 903 MB. It may clash with other mods that change Pokemon models."
    },
    {
      "id": "pokemon-omega-ruby-shiny-lock-removal",
      "name": "Shiny Lock Removal",
      "title": "Pokemon Omega Ruby",
      "title_ids": [
        "000400000011C400"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486570",
      "verified": false,
      "downloads": 3668,
      "updated": "2025-01",
      "source": {
        "type": "gamebanana",
        "mod": 486570,
        "file_pattern": "omegaruby_citra_shiny_lock_removal_96c59.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from the static and gift Pokemon, such as Poochyena, Cosplay Pikachu, Primal Kyogre and Groudon, Rayquaza and Deoxys, by replacing the game code and DllField.cro. It only works with the base 1.0.0 game, so no update data may be installed. Replaces other code patches for the game."
    },
    {
      "id": "pokemon-omega-ruby-shiny-lock-removal-trades",
      "name": "Shiny Lock Removal (with In-Game Trades)",
      "title": "Pokemon Omega Ruby",
      "title_ids": [
        "000400000011C400"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486570",
      "verified": false,
      "downloads": 3668,
      "updated": "2025-01",
      "source": {
        "type": "gamebanana",
        "mod": 486570,
        "file_pattern": "omegaruby_citra_shiny_lock_removal_trades.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The same shiny lock removal plus the in-game trade Pokemon (Makuhita, Skitty and Corsola), which needs a deeper code edit. A side effect is that trainers' Pokemon can also be shiny, so use it only when hunting a trade. It only works with the base 1.0.0 game, so no update data may be installed."
    },
    {
      "id": "pokemon-alpha-sapphire-shiny-lock-removal",
      "name": "Shiny Lock Removal",
      "title": "Pokemon Alpha Sapphire",
      "title_ids": [
        "000400000011C500"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486570",
      "verified": false,
      "downloads": 3668,
      "updated": "2025-01",
      "source": {
        "type": "gamebanana",
        "mod": 486570,
        "file_pattern": "alphasapphire_citra_shiny_lock_removal_d65fc.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from the static and gift Pokemon, such as Poochyena, Cosplay Pikachu, Primal Kyogre and Groudon, Rayquaza and Deoxys, by replacing the game code and DllField.cro. It only works with the base 1.0.0 game, so no update data may be installed. Replaces other code patches for the game."
    },
    {
      "id": "pokemon-alpha-sapphire-shiny-lock-removal-trades",
      "name": "Shiny Lock Removal (with In-Game Trades)",
      "title": "Pokemon Alpha Sapphire",
      "title_ids": [
        "000400000011C500"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486570",
      "verified": false,
      "downloads": 3668,
      "updated": "2025-01",
      "source": {
        "type": "gamebanana",
        "mod": 486570,
        "file_pattern": "alphasapphire_citra_shiny_lock_removal_trades.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The same shiny lock removal plus the in-game trade Pokemon (Makuhita, Skitty and Corsola), which needs a deeper code edit. A side effect is that trainers' Pokemon can also be shiny, so use it only when hunting a trade. It only works with the base 1.0.0 game, so no update data may be installed."
    },
    {
      "id": "pokemon-oras-may-contest-dress",
      "name": "May's Contest Dress",
      "title": "Pokemon Omega Ruby / Pokemon Alpha Sapphire",
      "title_ids": [
        "000400000011C400",
        "000400000011C500"
      ],
      "author": "Moonling",
      "author_url": "https://gamebanana.com/members/1698465",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/375892",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 375892,
        "file_pattern": "romfs_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces May's world and battle models with her Contest dress by swapping two model files. The scuba gear, goggles and Mega Ring are missing from the world model and the Mega Ring always shows on the battle model, as the page lists. It conflicts with other mods that change May's model.",
      "downloads": 2578,
      "updated": "2022-05"
    },
    {
      "id": "pokemon-oras-emerald-may",
      "name": "Emerald May",
      "title": "Pokemon Omega Ruby / Pokemon Alpha Sapphire",
      "title_ids": [
        "000400000011C400",
        "000400000011C500"
      ],
      "author": "Ghost1971185",
      "author_url": "https://gamebanana.com/members/1971185",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/413784",
      "verified": false,
      "downloads": 2451,
      "updated": "2022-12",
      "source": {
        "type": "gamebanana",
        "mod": 413784,
        "file_pattern": "emerald_may_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "a/",
            "to": "romfs/a/"
          }
        ]
      },
      "notes": "Replaces May's model with her Emerald-era design by swapping two model files. The author says some details, such as the shoes, are not exact. It conflicts with other mods that change May's model."
    },
    {
      "id": "pokemon-oras-accurate-sizes",
      "name": "Accurate Pokemon Sizes",
      "title": "Pokemon Omega Ruby / Pokemon Alpha Sapphire",
      "title_ids": [
        "000400000011C400",
        "000400000011C500"
      ],
      "author": "The Pod",
      "author_url": "https://gamebanana.com/members/2034086",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/697065",
      "verified": false,
      "downloads": 470,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 697065,
        "file_pattern": "accuratepokemonsizesoras.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Scales the battle size of every Pokemon to its real-world size, in the style of True Scale, by replacing one model data file. The download is about 570 MB. It may clash with other mods that change Pokemon models."
    },
    {
      "id": "mh4u-charge-blade-fast-morph-gp-usa",
      "name": "Charge Blade Fast Morph GP",
      "title": "Monster Hunter 4 Ultimate",
      "title_ids": [
        "0004000000126300"
      ],
      "author": "JackUnderTheHood",
      "author_url": "https://gamebanana.com/members/5774966",
      "licence": "CC BY-NC-SA 4.0",
      "homepage": "https://gamebanana.com/mods/703777",
      "verified": false,
      "downloads": 350,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 703777,
        "file_pattern": "mh4u_usa_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Azahar/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Adds a Guard Point to the Charge Blade's Fast Morph Slash, including the directional version, while keeping its fast animation. It is a code patch for MH4U USA 1.1 only, so match your game version, and it conflicts with other code patches for the game."
    },
    {
      "id": "mh4u-charge-blade-fast-morph-gp-eur",
      "name": "Charge Blade Fast Morph GP",
      "title": "Monster Hunter 4 Ultimate",
      "title_ids": [
        "0004000000126100"
      ],
      "author": "JackUnderTheHood",
      "author_url": "https://gamebanana.com/members/5774966",
      "licence": "CC BY-NC-SA 4.0",
      "homepage": "https://gamebanana.com/mods/703777",
      "verified": false,
      "downloads": 350,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 703777,
        "file_pattern": "mh4u_eur_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Azahar/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Adds a Guard Point to the Charge Blade's Fast Morph Slash, including the directional version, while keeping its fast animation. It is a code patch for MH4U Europe 1.1 only, so match your game version, and it conflicts with other code patches for the game."
    },
    {
      "id": "mh4u-charge-blade-fast-morph-gp-jpn",
      "name": "Charge Blade Fast Morph GP",
      "title": "Monster Hunter 4G",
      "title_ids": [
        "000400000011D700"
      ],
      "author": "JackUnderTheHood",
      "author_url": "https://gamebanana.com/members/5774966",
      "licence": "CC BY-NC-SA 4.0",
      "homepage": "https://gamebanana.com/mods/703777",
      "verified": false,
      "downloads": 350,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 703777,
        "file_pattern": "mh4g_jpn_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Azahar/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Adds a Guard Point to the Charge Blade's Fast Morph Slash, including the directional version, while keeping its fast animation. It is a code patch for MH4G Japan 1.2 only, so match your game version, and it conflicts with other code patches for the game."
    },
    {
      "id": "mh4u-weapon-honing-life-usa",
      "name": "Weapon Honing: Life",
      "title": "Monster Hunter 4 Ultimate",
      "title_ids": [
        "0004000000126300"
      ],
      "author": "JackUnderTheHood",
      "author_url": "https://gamebanana.com/members/5774966",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/711049",
      "verified": false,
      "downloads": 269,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 711049,
        "file_pattern": "mh4u_usa_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Azahar/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Reworks the Life honing so it heals 7.5% of eligible damage with a short cooldown and a green glow, like the Health Regen augment in Monster Hunter World. It also bundles the Fast Morph GP and Charged Double Slash GP changes, so do not install those separately. It is a code patch for MH4U USA 1.1 only and conflicts with other code patches."
    },
    {
      "id": "mh4u-weapon-honing-life-eur",
      "name": "Weapon Honing: Life",
      "title": "Monster Hunter 4 Ultimate",
      "title_ids": [
        "0004000000126100"
      ],
      "author": "JackUnderTheHood",
      "author_url": "https://gamebanana.com/members/5774966",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/711049",
      "verified": false,
      "downloads": 269,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 711049,
        "file_pattern": "mh4u_eur_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Azahar/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Reworks the Life honing so it heals 7.5% of eligible damage with a short cooldown and a green glow, like the Health Regen augment in Monster Hunter World. It also bundles the Fast Morph GP and Charged Double Slash GP changes, so do not install those separately. It is a code patch for MH4U Europe 1.1 only and conflicts with other code patches."
    },
    {
      "id": "mh4u-weapon-honing-life-jpn",
      "name": "Weapon Honing: Life",
      "title": "Monster Hunter 4G",
      "title_ids": [
        "000400000011D700"
      ],
      "author": "JackUnderTheHood",
      "author_url": "https://gamebanana.com/members/5774966",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/711049",
      "verified": false,
      "downloads": 269,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 711049,
        "file_pattern": "mh4g_jpn_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Azahar/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Reworks the Life honing so it heals 7.5% of eligible damage with a short cooldown and a green glow, like the Health Regen augment in Monster Hunter World. It also bundles the Fast Morph GP and Charged Double Slash GP changes, so do not install those separately. It is a code patch for MH4G Japan 1.2 only and conflicts with other code patches."
    },
    {
      "id": "mh4u-charged-double-slash-gp-usa",
      "name": "Charged Double Slash GP",
      "title": "Monster Hunter 4 Ultimate",
      "title_ids": [
        "0004000000126300"
      ],
      "author": "JackUnderTheHood",
      "author_url": "https://gamebanana.com/members/5774966",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/705472",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 705472,
        "file_pattern": "mh4u_usa_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Azahar/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Adds a Guard Point to the Charge Blade's Charged Double Slash while it is charged, and includes the Fast Morph Slash GP. Its files are named for Weapon Honing: Life and the code patch matches that mod's bundle, so it also carries the honing change. It is a code patch for MH4U USA 1.1 only and conflicts with other code patches.",
      "downloads": 82,
      "updated": "2026-08"
    },
    {
      "id": "mh4u-charged-double-slash-gp-eur",
      "name": "Charged Double Slash GP",
      "title": "Monster Hunter 4 Ultimate",
      "title_ids": [
        "0004000000126100"
      ],
      "author": "JackUnderTheHood",
      "author_url": "https://gamebanana.com/members/5774966",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/705472",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 705472,
        "file_pattern": "mh4u_eur_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Azahar/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Adds a Guard Point to the Charge Blade's Charged Double Slash while it is charged, and includes the Fast Morph Slash GP. Its files are named for Weapon Honing: Life and the code patch matches that mod's bundle, so it also carries the honing change. It is a code patch for MH4U Europe 1.1 only and conflicts with other code patches.",
      "downloads": 82,
      "updated": "2026-08"
    },
    {
      "id": "mh4u-charged-double-slash-gp-jpn",
      "name": "Charged Double Slash GP",
      "title": "Monster Hunter 4G",
      "title_ids": [
        "000400000011D700"
      ],
      "author": "JackUnderTheHood",
      "author_url": "https://gamebanana.com/members/5774966",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/705472",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 705472,
        "file_pattern": "mh4g_jpn_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Azahar/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "*/load/mods/{tid}/",
            "to": ""
          },
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Adds a Guard Point to the Charge Blade's Charged Double Slash while it is charged, and includes the Fast Morph Slash GP. Its files are named for Weapon Honing: Life and the code patch matches that mod's bundle, so it also carries the honing change. It is a code patch for MH4G Japan 1.2 only and conflicts with other code patches.",
      "downloads": 82,
      "updated": "2026-08"
    },
    {
      "id": "bravely-default-1ap-norende-2min",
      "name": "1 AP Support Abilities + Norende in 2 Minutes",
      "title": "Bravely Default",
      "title_ids": [
        "00040000000FC500",
        "00040000000FC600",
        "00040000000DB600"
      ],
      "author": "Lv14Mimic",
      "author_url": "https://gamebanana.com/members/4389696",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/587363",
      "verified": false,
      "downloads": 476,
      "updated": "2025-04",
      "source": {
        "type": "gamebanana",
        "mod": 587363,
        "file_pattern": "1_ap_support_abilities_norende_in_2_minutes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Makes every Support Ability cost 1 AP and sets all Norende Village timers to 2 minutes, so one villager is enough. It replaces the same files as the other two variants, and the author tested it on emulators, the USA version and the one with the Japanese title ID. It breaks mods that replace the same romfs files."
    },
    {
      "id": "bravely-default-1ap-support-abilities",
      "name": "1 AP Support Abilities",
      "title": "Bravely Default",
      "title_ids": [
        "00040000000FC500",
        "00040000000FC600",
        "00040000000DB600"
      ],
      "author": "Lv14Mimic",
      "author_url": "https://gamebanana.com/members/4389696",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/587363",
      "verified": false,
      "downloads": 476,
      "updated": "2025-04",
      "source": {
        "type": "gamebanana",
        "mod": 587363,
        "file_pattern": "all_support_abilities_cost_1_ap_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Makes every Support Ability cost 1 AP by replacing the ability tables. It is a balance change that makes the game much easier. It breaks mods that replace the same romfs files."
    },
    {
      "id": "bravely-default-norende-2min",
      "name": "Norende in 2 Minutes",
      "title": "Bravely Default",
      "title_ids": [
        "00040000000FC500",
        "00040000000FC600",
        "00040000000DB600"
      ],
      "author": "Lv14Mimic",
      "author_url": "https://gamebanana.com/members/4389696",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/587363",
      "verified": false,
      "downloads": 476,
      "updated": "2025-04",
      "source": {
        "type": "gamebanana",
        "mod": 587363,
        "file_pattern": "norende_in_2_minutes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Sets all Norende Village timers to 2 minutes, so the village can be rebuilt with a single villager. Use it instead of the combined mod, not with it. It breaks mods that replace the same romfs files."
    },
    {
      "id": "bravely-default-hardmode",
      "name": "Hardmode",
      "title": "Bravely Default",
      "title_ids": [
        "00040000000FC500"
      ],
      "author": "Nahnegnal",
      "author_url": "https://gamebanana.com/members/3740542",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/538489",
      "verified": false,
      "downloads": 303,
      "updated": "2025-04",
      "source": {
        "type": "gamebanana",
        "mod": 538489,
        "file_pattern": "bravely_default_hardmode_v*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A hard-mode hack with stronger enemies, faster village unlocks, improved jobs and changed late-game boss AI. It is for the USA version only. Its RAR cannot be fully unpacked with the system bsdtar (one file errors), so the install may fail."
    },
    {
      "id": "bravely-default-usa-999999-limit",
      "name": "999,999 Damage and Heal",
      "title": "Bravely Default",
      "title_ids": [
        "00040000000FC500"
      ],
      "author": "struktured",
      "author_url": "https://gamebanana.com/members/4503858",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/727665",
      "verified": false,
      "downloads": 11,
      "updated": "2026-11",
      "source": {
        "type": "gamebanana",
        "mod": 727665,
        "file_pattern": "bd-999k-limit-*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Raises the damage and healing cap from 9,999 to 999,999 and shows all six digits; maximum HP and MP are unchanged. It is a code patch for the USA base game only, and old save states do not work with it. It replaces other code patches for the game."
    },
    {
      "id": "persona-q-true-undub-usa",
      "name": "True Undub (USA)",
      "title": "Persona Q: Shadow of the Labyrinth",
      "title_ids": [
        "0004000000123400"
      ],
      "author": "HxCannon",
      "author_url": "https://gamebanana.com/members/2458257",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/516347",
      "verified": false,
      "downloads": 1543,
      "updated": "2024-06",
      "source": {
        "type": "gamebanana",
        "mod": 516347,
        "file_pattern": "pq_-_true_undub_patched_.rar"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          },
          {
            "from": "*/[US VER] Patched files/code.bin",
            "to": "exefs/code.bin"
          },
          {
            "from": "*/[US VER] Patched files/exheader.bin",
            "to": "exheader.bin"
          }
        ]
      },
      "notes": "Restores the Japanese voices for the game and its DLC and keeps English-subtitled cutscenes. It ships a patched code.bin and exheader.bin that load the audio, so it replaces other code patches. The download is about 1.2 GB; the author's Mod Compendium files are not needed.",
      "size": 1184516283
    },
    {
      "id": "persona-q-true-undub-eur",
      "name": "True Undub (Europe)",
      "title": "Persona Q: Shadow of the Labyrinth",
      "title_ids": [
        "0004000000149F00"
      ],
      "author": "HxCannon",
      "author_url": "https://gamebanana.com/members/2458257",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/516347",
      "verified": false,
      "downloads": 1543,
      "updated": "2024-06",
      "source": {
        "type": "gamebanana",
        "mod": 516347,
        "file_pattern": "pq_-_true_undub_patched_.rar"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          },
          {
            "from": "*/[EUR VER] Patched files/code.bin",
            "to": "exefs/code.bin"
          },
          {
            "from": "*/[EUR VER] Patched files/exheader.bin",
            "to": "exheader.bin"
          }
        ]
      },
      "notes": "Restores the Japanese voices for the game and its DLC and keeps English-subtitled cutscenes. It ships a patched code.bin and exheader.bin for the European version, which the author says can have problems detecting DLC. The download is about 1.2 GB.",
      "size": 1184516283
    },
    {
      "id": "persona-q-spanish-translation",
      "name": "Spanish Translation",
      "title": "Persona Q: Shadow of the Labyrinth",
      "title_ids": [
        "0004000000123400",
        "0004000000149F00"
      ],
      "author": "HxCannon",
      "author_url": "https://gamebanana.com/members/2458257",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/511619",
      "verified": false,
      "downloads": 485,
      "updated": "2024-05",
      "source": {
        "type": "gamebanana",
        "mod": 511619,
        "file_pattern": "pq_spanish_translation_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "code.bin",
            "to": "exefs/code.bin"
          },
          {
            "from": "exheader.bin",
            "to": "exheader.bin"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Translates the text and the textures to Spanish. It carries a patched code.bin and exheader.bin, so it replaces other code patches and clashes with other text mods. The download is about 396 MB, and the page does not say which region it is built for."
    },
    {
      "id": "persona-q-portuguese-br-english",
      "name": "Portuguese (Brazil) Translation (English Voices)",
      "title": "Persona Q: Shadow of the Labyrinth",
      "title_ids": [
        "0004000000123400"
      ],
      "author": "Ahtheerr",
      "author_url": "https://gamebanana.com/members/3673388",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/579957",
      "verified": false,
      "downloads": 413,
      "updated": "2025-03",
      "source": {
        "type": "gamebanana",
        "mod": 579957,
        "file_pattern": "pq_ingles.zip"
      },
      "install": {
        "rules": [
          {
            "from": "PQ_INGLES/{tid}/code.bin",
            "to": "exefs/code.bin"
          },
          {
            "from": "PQ_INGLES/{tid}/exheader.bin",
            "to": "exheader.bin"
          },
          {
            "from": "PQ_INGLES/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A full Brazilian Portuguese translation of the game by Phantasie Translate, built on the English version of the game audio. It carries a patched code.bin and exheader.bin, so it replaces other code patches and clashes with other text mods. It is for the USA version, and GameBanana has not finished scanning this file."
    },
    {
      "id": "persona-q-portuguese-br-japanese",
      "name": "Portuguese (Brazil) Translation (Japanese Voices)",
      "title": "Persona Q: Shadow of the Labyrinth",
      "title_ids": [
        "0004000000123400"
      ],
      "author": "Ahtheerr",
      "author_url": "https://gamebanana.com/members/3673388",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/579957",
      "verified": false,
      "downloads": 413,
      "updated": "2025-03",
      "source": {
        "type": "gamebanana",
        "mod": 579957,
        "file_pattern": "pq_japones.zip"
      },
      "install": {
        "rules": [
          {
            "from": "PQ_JAPONES/{tid}/code.bin",
            "to": "exefs/code.bin"
          },
          {
            "from": "PQ_JAPONES/{tid}/exheader.bin",
            "to": "exheader.bin"
          },
          {
            "from": "PQ_JAPONES/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A full Brazilian Portuguese translation of the game by Phantasie Translate, built on the Japanese version of the game audio. It carries a patched code.bin and exheader.bin, so it replaces other code patches and clashes with other text mods. It is for the USA version, and GameBanana has not finished scanning this file."
    },
    {
      "id": "kirby-robobot-restored-moves",
      "name": "Restored Moves",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "JC Players",
      "author_url": "https://gamebanana.com/members/2149390",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/442680",
      "verified": false,
      "downloads": 1360,
      "updated": "2026-01",
      "source": {
        "type": "gamebanana",
        "mod": 442680,
        "file_pattern": "robobot_restored_moves_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Brings back moves and effects that were cut from the game, such as the Revolution Beam, a mid-air Ninja Reverse Slash, the Fire Breath spiral, Spark's lightning dash and the Hammer triple attack. It replaces one script file, so it clashes with other mods that edit the same mint code. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "kirby-robobot-retexture-custom-apple",
      "name": "Kirby Retexture: Apple",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Apple/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Apple\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-black-white",
      "name": "Kirby Retexture: Black & White",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Black & White/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Black & White\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-carbon-blue",
      "name": "Kirby Retexture: Carbon Blue",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Carbon Blue/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Carbon Blue\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-carbon-pink",
      "name": "Kirby Retexture: Carbon Pink",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Carbon Pink/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Carbon Pink\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-daredevil",
      "name": "Kirby Retexture: Daredevil",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Daredevil/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Daredevil\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-elfilin",
      "name": "Kirby Retexture: Elfilin",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Elfilin/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Elfilin\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-galacta",
      "name": "Kirby Retexture: Galacta",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Galacta/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Galacta\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-inverted",
      "name": "Kirby Retexture: Inverted",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Inverted/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Inverted\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-lean",
      "name": "Kirby Retexture: Lean",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Lean/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Lean\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-mirror",
      "name": "Kirby Retexture: Mirror",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Mirror/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Mirror\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-raspberry",
      "name": "Kirby Retexture: Raspberry",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Raspberry/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Raspberry\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-custom-suplex",
      "name": "Kirby Retexture: Suplex",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Custom/Suplex/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Suplex\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-amiibo-dedede",
      "name": "Kirby Retexture: Amiibo - Dedede",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Amiibo - Dedede/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Amiibo - Dedede\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-amiibo-meta-knight",
      "name": "Kirby Retexture: Amiibo - Meta Knight",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Amiibo - Meta Knight/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Amiibo - Meta Knight\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-amiibo-waddle-dee",
      "name": "Kirby Retexture: Amiibo - Waddle Dee",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Amiibo - Waddle Dee/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Amiibo - Waddle Dee\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-kirby-battle-royale-orange",
      "name": "Kirby Retexture: Kirby Battle Royale - Orange",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Kirby Battle Royale - Orange/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Kirby Battle Royale - Orange\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-kirby-battle-royale-white-black",
      "name": "Kirby Retexture: Kirby Battle Royale - White & Black",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Kirby Battle Royale - White & Black/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Kirby Battle Royale - White & Black\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-kirby-fighters-chalk",
      "name": "Kirby Retexture: Kirby Fighters - Chalk",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Kirby Fighters - Chalk/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Kirby Fighters - Chalk\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-kirby-fighters-red",
      "name": "Kirby Retexture: Kirby Fighters - Red",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Kirby Fighters - Red/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Kirby Fighters - Red\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-modern-blue",
      "name": "Kirby Retexture: Modern - Blue",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Modern - Blue/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Modern - Blue\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-modern-green",
      "name": "Kirby Retexture: Modern - Green",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Modern - Green/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Modern - Green\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-modern-modern-yellow",
      "name": "Kirby Retexture: Modern - Yellow",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Modern/Modern - Yellow/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Modern - Yellow\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-carbon",
      "name": "Kirby Retexture: Carbon",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Carbon/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Carbon\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-chalk",
      "name": "Kirby Retexture: Chalk",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Chalk/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Chalk\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-cherry",
      "name": "Kirby Retexture: Cherry",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Cherry/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Cherry\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-chocolate",
      "name": "Kirby Retexture: Chocolate",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Chocolate/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Chocolate\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-citrus",
      "name": "Kirby Retexture: Citrus",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Citrus/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Citrus\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-emerald",
      "name": "Kirby Retexture: Emerald",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Emerald/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Emerald\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-grape",
      "name": "Kirby Retexture: Grape",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Grape/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Grape\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-green",
      "name": "Kirby Retexture: Green",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Green/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Green\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-ivory",
      "name": "Kirby Retexture: Ivory",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Ivory/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Ivory\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-lavender",
      "name": "Kirby Retexture: Lavender",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Lavender/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Lavender\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-ocean",
      "name": "Kirby Retexture: Ocean",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Ocean/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Ocean\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-orange",
      "name": "Kirby Retexture: Orange",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Orange/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Orange\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-red",
      "name": "Kirby Retexture: Red",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Red/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Red\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-sapphire",
      "name": "Kirby Retexture: Sapphire",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Sapphire/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Sapphire\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-shadow",
      "name": "Kirby Retexture: Shadow",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Shadow/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Shadow\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-snow",
      "name": "Kirby Retexture: Snow",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Snow/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Snow\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-white",
      "name": "Kirby Retexture: White",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/White/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"White\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-retexture-sprays-yellow",
      "name": "Kirby Retexture: Yellow",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/377858",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 377858,
        "file_pattern": "kprkirbyretextures.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KPRKirbyRetextures/Sprays/Yellow/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours Kirby with the \"Yellow\" skin from a pack of 40, by replacing his base model texture; install only one skin at a time. The pack was made with Citra, and the cheek textures stay the original. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1105,
      "updated": "2022-05"
    },
    {
      "id": "kirby-robobot-moded-planet-robobot",
      "name": "Moded Planet Robobot",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "Princess RipBlue",
      "author_url": "https://gamebanana.com/members/1678189",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/309235",
      "verified": false,
      "downloads": 891,
      "updated": "2024-04",
      "source": {
        "type": "gamebanana",
        "mod": 309235,
        "file_pattern": "moded_planet_robobot_20.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives characters older-style textures and colours, adds custom music, changes the opening movie and adds the UFO copy essence to the True Arena, as many replaced model and sound files. The download is about 295 MB, and the author says it does not work fully outside Citra."
    },
    {
      "id": "kirby-robobot-shadow-kirby",
      "name": "Shadow Kirby",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "JC Players",
      "author_url": "https://gamebanana.com/members/2149390",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/441977",
      "verified": false,
      "downloads": 665,
      "updated": "2023-05",
      "source": {
        "type": "gamebanana",
        "mod": 441977,
        "file_pattern": "shadow_kirby.rar"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives Kirby a Shadow Kirby look, in the Triple Deluxe colouring, by replacing his base texture. It only affects the main modes, because the shadow texture glitches on the 3D Rumble model. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "kirby-robobot-shadow-kirby-kf2",
      "name": "Shadow Kirby (Fighters 2 Colours)",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "JC Players",
      "author_url": "https://gamebanana.com/members/2149390",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/441977",
      "verified": false,
      "downloads": 665,
      "updated": "2023-05",
      "source": {
        "type": "gamebanana",
        "mod": 441977,
        "file_pattern": "kf2_shadow_kirby.rar"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives Kirby a Shadow Kirby look in the Kirby Fighters 2 colouring, by replacing his base texture. It only affects the main modes, because the shadow texture glitches on the 3D Rumble model. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "kirby-robobot-galacta-knightmare",
      "name": "Galacta Knightmare",
      "title": "Kirby: Planet Robobot",
      "title_ids": [
        "0004000000183600"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/354304",
      "verified": false,
      "downloads": 577,
      "updated": "2022-02",
      "source": {
        "type": "gamebanana",
        "mod": 354304,
        "file_pattern": "galactaknightmare.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces Meta Knight's model with Galacta Knight, retextures the goggles, floaty and cloak, and changes the Meta Knightmare enemies and the name text. Galacta's eyes can clip out of his mask in cutscenes because of the different mask shape. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "kirby-triple-deluxe-palette-blue",
      "name": "Kirby Palette: Blue",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/312916",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 312916,
        "file_pattern": "ktdpalettes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KTDPalettes/Blue/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets you play as the Blue Kirby palette from the Kirby Fighters minigame, by replacing his base model texture; install one palette at a time. It was made with Citra and the cheeks keep the original colour. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1346,
      "updated": "2021-08"
    },
    {
      "id": "kirby-triple-deluxe-palette-chalk",
      "name": "Kirby Palette: Chalk",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/312916",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 312916,
        "file_pattern": "ktdpalettes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KTDPalettes/Chalk/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets you play as the Chalk Kirby palette from the Kirby Fighters minigame, by replacing his base model texture; install one palette at a time. It was made with Citra and the cheeks keep the original colour. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1346,
      "updated": "2021-08"
    },
    {
      "id": "kirby-triple-deluxe-palette-green",
      "name": "Kirby Palette: Green",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/312916",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 312916,
        "file_pattern": "ktdpalettes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KTDPalettes/Green/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets you play as the Green Kirby palette from the Kirby Fighters minigame, by replacing his base model texture; install one palette at a time. It was made with Citra and the cheeks keep the original colour. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1346,
      "updated": "2021-08"
    },
    {
      "id": "kirby-triple-deluxe-palette-navy",
      "name": "Kirby Palette: Navy",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/312916",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 312916,
        "file_pattern": "ktdpalettes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KTDPalettes/Navy/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets you play as the Navy Kirby palette from the Kirby Fighters minigame, by replacing his base model texture; install one palette at a time. It was made with Citra and the cheeks keep the original colour. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1346,
      "updated": "2021-08"
    },
    {
      "id": "kirby-triple-deluxe-palette-red",
      "name": "Kirby Palette: Red",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/312916",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 312916,
        "file_pattern": "ktdpalettes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KTDPalettes/Red/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets you play as the Red Kirby palette from the Kirby Fighters minigame, by replacing his base model texture; install one palette at a time. It was made with Citra and the cheeks keep the original colour. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1346,
      "updated": "2021-08"
    },
    {
      "id": "kirby-triple-deluxe-palette-shadow",
      "name": "Kirby Palette: Shadow",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/312916",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 312916,
        "file_pattern": "ktdpalettes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KTDPalettes/Shadow/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets you play as the Shadow Kirby palette from the Kirby Fighters minigame, by replacing his base model texture; install one palette at a time. It was made with Citra and the cheeks keep the original colour. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1346,
      "updated": "2021-08"
    },
    {
      "id": "kirby-triple-deluxe-palette-yellow",
      "name": "Kirby Palette: Yellow",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "DudeLuke",
      "author_url": "https://gamebanana.com/members/1911763",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/312916",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 312916,
        "file_pattern": "ktdpalettes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "KTDPalettes/Yellow/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets you play as the Yellow Kirby palette from the Kirby Fighters minigame, by replacing his base model texture; install one palette at a time. It was made with Citra and the cheeks keep the original colour. The page does not say which regions it was made for, so other versions are untested.",
      "downloads": 1346,
      "updated": "2021-08"
    },
    {
      "id": "kirby-triple-deluxe-restored-moves",
      "name": "Restored Moves",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "JC Players",
      "author_url": "https://gamebanana.com/members/2149390",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/442679",
      "verified": false,
      "downloads": 1227,
      "updated": "2026-01",
      "source": {
        "type": "gamebanana",
        "mod": 442679,
        "file_pattern": "tdx_restored_moves_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Brings back moves and effects that were cut from the game, such as the Revolution Beam, the Spear triple throw, the Ice Ball, Spark's up and down lightning attacks and the Hammer triple attack. It replaces one script file, so it clashes with other mods that edit the same mint code. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "kirby-triple-deluxe-fighters-hats",
      "name": "Kirby Fighters Deluxe Hats",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "Vyroz",
      "author_url": "https://gamebanana.com/members/2041940",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/341584",
      "verified": false,
      "downloads": 611,
      "updated": "2021-12",
      "source": {
        "type": "gamebanana",
        "mod": 341584,
        "file_pattern": "pizza_hats_in_parufe_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces some ability hats, such as Beam, Bomb, Cutter, Hammer, Ninja and Sword, with the ones from Kirby Fighters Deluxe, by swapping model files. It was made with Citra. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "kirby-triple-deluxe-100x-deluxe",
      "name": "Kirby 100X Deluxe",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "JC Players",
      "author_url": "https://gamebanana.com/members/2149390",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/517051",
      "verified": false,
      "downloads": 552,
      "updated": "2024-05",
      "source": {
        "type": "gamebanana",
        "mod": 517051,
        "file_pattern": "jam_tdx.rar"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A joke mod from a mod jam: Kirby has Mach abilities and everything is 100 times faster, bosses are harder, and there are text edits, changed animations and new music. The author says some changes crash on real hardware. The files use the USA text folder, so other regions are untested."
    },
    {
      "id": "kirby-triple-deluxe-ability-selector",
      "name": "Ability Selector",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00",
        "000400000010C000",
        "000400000010BE00"
      ],
      "author": "min is mini",
      "author_url": "https://gamebanana.com/members/2126211",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/704998",
      "verified": false,
      "downloads": 111,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 704998,
        "file_pattern": "ability_select.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Adds an ability selector: press Up and Y to open the mix state, then Y again to cycle through every ability, including Normal and Hypernova. It replaces one script file, so it clashes with other mint code mods. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "kirby-battle-royale-playable-story-npcs",
      "name": "Playable Story NPCs",
      "title": "Kirby Battle Royale",
      "title_ids": [
        "00040000001C2000",
        "00040000001C2100"
      ],
      "author": "Bloomstorm",
      "author_url": "https://gamebanana.com/members/2344960",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/412763",
      "verified": false,
      "downloads": 408,
      "updated": "2023-03",
      "source": {
        "type": "gamebanana",
        "mod": 412763,
        "file_pattern": "playablestorynpcs_mod.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Adds four NPC abilities, Bandana Waddle Dee, Soldier Waddle Dee, Axe Knight and Sandbag, by editing the mint code and the ability select layout. It is made for the USA and Europe 3.0 versions, and catching Axe Knight in a Coin Clash can crash the game."
    },
    {
      "id": "kirby-battle-royale-three-stupid-abilities",
      "name": "3 Stupid Abilities",
      "title": "Kirby Battle Royale",
      "title_ids": [
        "00040000001C2000",
        "00040000001C2100"
      ],
      "author": "JC Players",
      "author_url": "https://gamebanana.com/members/2149390",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/485525",
      "verified": false,
      "downloads": 83,
      "updated": "2023-12",
      "source": {
        "type": "gamebanana",
        "mod": 485525,
        "file_pattern": "romfs_*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives the Hammer, Sleep and Tornado abilities much bigger hitboxes and some fast movement, by replacing three script files. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "kirby-battle-royale-fighters-palettes",
      "name": "Kirby Fighters Deluxe Palettes",
      "title": "Kirby Battle Royale",
      "title_ids": [
        "00040000001C2000"
      ],
      "author": "min is mini",
      "author_url": "https://gamebanana.com/members/2126211",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/505608",
      "verified": false,
      "downloads": 73,
      "updated": "2024-04",
      "source": {
        "type": "gamebanana",
        "mod": 505608,
        "file_pattern": "kfdx_palettes.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives Kirby palette swaps that match Kirby Fighters Deluxe, such as blue and green, by replacing his model and colour files. The HUD stays unchanged and the Mirror ability still shows the original colours. It is for the USA version only."
    },
    {
      "id": "kirby-battle-royale-playable-axe-knight",
      "name": "Playable Axe Knight",
      "title": "Kirby Battle Royale",
      "title_ids": [
        "00040000001C2100"
      ],
      "author": "Bloomstorm",
      "author_url": "https://gamebanana.com/members/2344960",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/412285",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 412285,
        "file_pattern": "playableaxeknight.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Unlocks Axe Knight as an ability you get with the random \"?\" pick, by editing the mint code, in single player and multiplayer but not story mode. It is for the Europe 3.0 version only, and the author marks it outdated in favour of Playable Story NPCs.",
      "downloads": 52,
      "updated": "2022-11"
    },
    {
      "id": "kirby-battle-royale-green-screen-room",
      "name": "Green Screen Room",
      "title": "Kirby Battle Royale",
      "title_ids": [
        "00040000001C2000",
        "00040000001C2100"
      ],
      "author": "JC Players",
      "author_url": "https://gamebanana.com/members/2149390",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/643063",
      "verified": false,
      "downloads": 23,
      "updated": "2025-12",
      "source": {
        "type": "gamebanana",
        "mod": 643063,
        "file_pattern": "kbr_green.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Turns the Training Room into a green screen room with a close top view, edited Kirby animations, a no-hat Qibby and Bandana Dee in place of the Spear Waddle Dee. The Qibby move crashes if you charge it. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "luigis-mansion-original-toad-voice",
      "name": "Original Toad Voice",
      "title": "Luigi's Mansion",
      "title_ids": [
        "00040000001D1900"
      ],
      "author": "JomSpoons",
      "author_url": "https://gamebanana.com/members/1550501",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/678581",
      "verified": false,
      "downloads": 24,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 678581,
        "file_pattern": "original_toad_voice_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Restores Toad's voice from the GameCube release by replacing the game's sound archive. It is for the USA version, and the author says not to install it through OnionFS because that breaks the C-Stick."
    },
    {
      "id": "luigis-mansion-dark-moon-blue-robomb",
      "name": "Blue Robomb",
      "title": "Luigi's Mansion: Dark Moon",
      "title_ids": [
        "0004000000055F00",
        "0004000000076500",
        "0004000000076400"
      ],
      "author": "feldright",
      "author_url": "https://gamebanana.com/members/2407878",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/521709",
      "verified": false,
      "downloads": 40,
      "updated": "2024-06",
      "source": {
        "type": "gamebanana",
        "mod": 521709,
        "file_pattern": "blue_robomb.rar"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the texture of the normal Robombs in the ScareScraper with a blue one by editing the level bundle. The author calls it a hex-editing experiment and says it works on any region of the game."
    },
    {
      "id": "mario-luigi-bowsers-inside-story-60fps-v12",
      "name": "60 fps Patch (Game v1.2)",
      "title": "Mario & Luigi: Bowser's Inside Story + Bowser Jr.'s Journey",
      "title_ids": [
        "00040000001D1400"
      ],
      "author": "struktured",
      "author_url": "https://gamebanana.com/members/4503858",
      "licence": "MIT",
      "homepage": "https://gamebanana.com/mods/653851",
      "verified": false,
      "downloads": 891,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 653851,
        "file_pattern": "bis_60fps_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/game_v1.2/60fps_v12_full_r.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Unlocks 60 fps with a code patch; holding R drops back to 30 fps where the game misbehaves. It is for the USA game with the 1.2 update installed, and the wrong version freezes on a black screen. It replaces other code patches for the game."
    },
    {
      "id": "mario-luigi-bowsers-inside-story-60fps-v10",
      "name": "60 fps Patch (Game v1.0)",
      "title": "Mario & Luigi: Bowser's Inside Story + Bowser Jr.'s Journey",
      "title_ids": [
        "00040000001D1400"
      ],
      "author": "struktured",
      "author_url": "https://gamebanana.com/members/4503858",
      "licence": "MIT",
      "homepage": "https://gamebanana.com/mods/653851",
      "verified": false,
      "downloads": 891,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 653851,
        "file_pattern": "bis_60fps_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/game_v1.0/60fps_v10_full_r.ips",
            "to": "code.ips"
          }
        ]
      },
      "notes": "Unlocks 60 fps with a code patch; holding R drops back to 30 fps where the game misbehaves. It is for the USA game without the update, which is version 1.0, and the wrong version freezes on a black screen. It replaces other code patches for the game."
    },
    {
      "id": "mario-luigi-bowsers-inside-story-dx-hard-mode",
      "name": "DX Hard Mode",
      "title": "Mario & Luigi: Bowser's Inside Story + Bowser Jr.'s Journey",
      "title_ids": [
        "00040000001D1400",
        "00040000001D1500"
      ],
      "author": "Danius",
      "author_url": "https://gamebanana.com/members/5313560",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/660439",
      "verified": false,
      "downloads": 647,
      "updated": "2026-06",
      "source": {
        "type": "gamebanana",
        "mod": 660439,
        "file_pattern": "bis_dx_hard_mode_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "*/*/{tid}/code.bin",
            "to": "exefs/code.bin"
          },
          {
            "from": "*/*/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A hard-mode hack with 1.5 times enemy HP and defence, 2.5 times power, three extra levels, faster and harder attacks, an item limit of 10 and Easy Mode removed. It is early work in progress, replaces the game code and many data files, and is for the version 1.2 game in the USA and Europe."
    },
    {
      "id": "mario-luigi-dream-team-sm64-mario-luigi",
      "name": "SM64 Mario and Luigi",
      "title": "Mario & Luigi: Dream Team",
      "title_ids": [
        "00040000000D5A00",
        "00040000000D9000"
      ],
      "author": "NintendoGameTuber",
      "author_url": "https://gamebanana.com/members/1672096",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/440303",
      "verified": false,
      "downloads": 282,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 440303,
        "file_pattern": "romfs_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives Mario and Luigi their Super Mario 64 models in the Giant Battles by replacing the HugeBattle model files. The extra Textures folder on the page is source material and is not installed. The page does not say which regions it was made for, so other versions are untested."
    },
    {
      "id": "mario-luigi-dream-team-suspicious-drunk-driving",
      "name": "Mario & Louis: Suspicious Drunk Driving",
      "title": "Mario & Luigi: Dream Team",
      "title_ids": [
        "00040000000D5A00"
      ],
      "author": "Zeta64",
      "author_url": "https://gamebanana.com/members/1823133",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/381382",
      "verified": false,
      "downloads": 355,
      "updated": "2022-06",
      "source": {
        "type": "gamebanana",
        "mod": 381382,
        "file_pattern": "v101_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "v1.0.1/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "textures": {
        "rules": [
          {
            "from": "v1.0.1/textures/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "A Google Translate text hack of the game, with the text put through many rounds of machine translation, plus redrawn textures. The author warns it has swearing and adult themes. It is for the USA version and the text in the English folder."
    },
    {
      "id": "mario-luigi-dream-team-noexp-coins",
      "name": "NoExp/Coins",
      "title": "Mario & Luigi: Dream Team",
      "title_ids": [
        "00040000000D5A00"
      ],
      "author": "Nahnegnal",
      "author_url": "https://gamebanana.com/members/3740542",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/606932",
      "verified": false,
      "downloads": 70,
      "updated": "2025-07",
      "source": {
        "type": "gamebanana",
        "mod": 606932,
        "file_pattern": "dream_team_*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/exefs/",
            "to": "exefs/"
          }
        ]
      },
      "notes": "Removes the experience and coin drops from enemies with a code patch. It is for the USA version and replaces other code patches for the game. Its solid RAR cannot be unpacked by the system bsdtar, so the install may fail."
    },
    {
      "id": "mario-luigi-paper-jam-book-of-mario-bros-hd",
      "name": "Book of Mario Bros. (HD Textures)",
      "title": "Mario & Luigi: Paper Jam",
      "title_ids": [
        "0004000000132800"
      ],
      "author": "Zeta64",
      "author_url": "https://gamebanana.com/members/1823133",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/381172",
      "verified": false,
      "downloads": 204,
      "updated": "2024-12",
      "source": {
        "type": "gamebanana",
        "mod": 381172,
        "file_pattern": "book_of_mario_bros_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/HD textures version/load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "textures": {
        "rules": [
          {
            "from": "*/HD textures version/load/textures/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "A Google Translate hack of Paper Jam that puts the text through many rounds of machine translation, and also redraws many textures to match. It is for the European version only, and the author warns it has adult content. Its static.crr file is not installed."
    },
    {
      "id": "mario-luigi-paper-jam-book-of-mario-bros-lowres",
      "name": "Book of Mario Bros. (Low-Res Textures)",
      "title": "Mario & Luigi: Paper Jam",
      "title_ids": [
        "0004000000132800"
      ],
      "author": "Zeta64",
      "author_url": "https://gamebanana.com/members/1823133",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/381172",
      "verified": false,
      "downloads": 204,
      "updated": "2024-12",
      "source": {
        "type": "gamebanana",
        "mod": 381172,
        "file_pattern": "book_of_mario_bros_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/lowres textures version/load/mods/{tid}/",
            "to": ""
          }
        ]
      },
      "textures": {
        "rules": [
          {
            "from": "*/lowres textures version/load/textures/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "A Google Translate hack of Paper Jam that puts the text through many rounds of machine translation, and also redraws many textures to match. It is for the European version only, and the author warns it has adult content. Its static.crr file is not installed."
    },
    {
      "id": "mario-luigi-paper-jam-noexp-coins",
      "name": "NoExp/Coins",
      "title": "Mario & Luigi: Paper Jam",
      "title_ids": [
        "0004000000132800"
      ],
      "author": "Nahnegnal",
      "author_url": "https://gamebanana.com/members/3740542",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/606933",
      "verified": false,
      "downloads": 59,
      "updated": "2025-07",
      "source": {
        "type": "gamebanana",
        "mod": 606933,
        "file_pattern": "paper_jam_*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the experience and coin drops from enemies by replacing the DB data file. It is for the European version, and it clashes with other mods that replace the same file. Its solid RAR cannot be unpacked by the system bsdtar, so the install may fail."
    },
    {
      "id": "nsmb2-crystal-crescents",
      "name": "Super Mario and the Crystal Crescents",
      "title": "New Super Mario Bros. 2",
      "title_ids": [
        "000400000007AE00",
        "000400000007AF00",
        "000400000007AD00",
        "00040000000B8900",
        "0004000000137E00",
        "0004000000137F00",
        "0004000000137D00"
      ],
      "author": "Rimea",
      "author_url": "https://gamebanana.com/members/1677676",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/551330",
      "verified": false,
      "downloads": 4338,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 551330,
        "file_pattern": "crystal_crescents.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A one-world mod with eight new levels, custom graphics, a new soundtrack and a custom world map. It covers the standard, Gold and Special editions of the game. It replaces other New Super Mario Bros. 2 level mods, which edit the same files, and the download is 141 MB."
    },
    {
      "id": "nsmb2-lost-levels-part1",
      "name": "Super Mario Bros. Next: The Lost Levels (Part 1)",
      "title": "New Super Mario Bros. 2",
      "title_ids": [
        "000400000007AE00",
        "000400000007AF00",
        "000400000007AD00",
        "00040000000B8900",
        "0004000000137E00",
        "0004000000137F00",
        "0004000000137D00"
      ],
      "author": "hus_mighty",
      "author_url": "https://gamebanana.com/members/1715957",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/51507",
      "verified": false,
      "downloads": 4135,
      "updated": "2022-05",
      "source": {
        "type": "gamebanana",
        "mod": 51507,
        "file_pattern": "smbntll1_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The first half of a collection of levels cut from the Super Mario Bros. Next project, with new courses, models and music. Parts 1 and 2 are separate halves and overwrite each other, so use one at a time. Replaces other New Super Mario Bros. 2 level mods."
    },
    {
      "id": "nsmb2-lost-levels-part2",
      "name": "Super Mario Bros. Next: The Lost Levels (Part 2)",
      "title": "New Super Mario Bros. 2",
      "title_ids": [
        "000400000007AE00",
        "000400000007AF00",
        "000400000007AD00",
        "00040000000B8900",
        "0004000000137E00",
        "0004000000137F00",
        "0004000000137D00"
      ],
      "author": "hus_mighty",
      "author_url": "https://gamebanana.com/members/1715957",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/51507",
      "verified": false,
      "downloads": 4135,
      "updated": "2022-05",
      "source": {
        "type": "gamebanana",
        "mod": 51507,
        "file_pattern": "smbntll2_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "The second half of the Lost Levels collection from the Super Mario Bros. Next project, with more cut courses, models and music. Parts 1 and 2 overwrite each other, so use one at a time. Replaces other New Super Mario Bros. 2 level mods; the download is 121 MB."
    },
    {
      "id": "nsmb2-nsmb-3ds",
      "name": "New Super Mario Bros. 3DS",
      "title": "New Super Mario Bros. 2",
      "title_ids": [
        "000400000007AE00",
        "000400000007AF00",
        "000400000007AD00",
        "00040000000B8900",
        "0004000000137E00",
        "0004000000137F00",
        "0004000000137D00"
      ],
      "author": "Nick64 Mods",
      "author_url": "https://gamebanana.com/members/2394426",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/443673",
      "verified": false,
      "downloads": 2152,
      "updated": "2024-02",
      "source": {
        "type": "gamebanana",
        "mod": 443673,
        "file_pattern": "new_super_mario_bros_3ds_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A two-world mod that changes every stage, the cannons and the Toad houses. It covers the standard, Gold and Special editions. Replaces other New Super Mario Bros. 2 level mods; the download is 144 MB, and a few preview images in the pack are copied into the mod folder but are never used."
    },
    {
      "id": "nsmb2-sm64-mario-luigi",
      "name": "SM64 Mario and SM64 Luigi",
      "title": "New Super Mario Bros. 2",
      "title_ids": [
        "000400000007AE00",
        "000400000007AF00",
        "000400000007AD00",
        "00040000000B8900",
        "0004000000137E00",
        "0004000000137F00",
        "0004000000137D00"
      ],
      "author": "NintendoGameTuber",
      "author_url": "https://gamebanana.com/members/1672096",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/414366",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 414366,
        "file_pattern": "romfs_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Swaps the playable Mario and Luigi for their Super Mario 64 models, including the cutscene models. It covers the standard, Gold and Special editions. It replaces the player models, so it clashes with other character mods.",
      "downloads": 2007,
      "updated": "2026-05"
    },
    {
      "id": "nsmb2-retro-remix",
      "name": "New Super Mario Bros. 2: Retro Remix",
      "title": "New Super Mario Bros. 2",
      "title_ids": [
        "000400000007AE00",
        "000400000007AF00",
        "000400000007AD00",
        "00040000000B8900",
        "0004000000137E00",
        "0004000000137F00",
        "0004000000137D00"
      ],
      "author": "Luigi23",
      "author_url": "https://gamebanana.com/members/2981650",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/545127",
      "verified": false,
      "downloads": 1338,
      "updated": "2025-09",
      "source": {
        "type": "gamebanana",
        "mod": 545127,
        "file_pattern": "nsmb2retroremix_v*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Remakes levels from Super Mario Bros., The Lost Levels, Super Mario Bros. 3, Super Mario World and New Super Mario Bros. in every world, each with three star coins. It covers the standard, Gold and Special editions. The newest regular version is downloaded; the Gold variant is not installed, and it replaces other level mods."
    },
    {
      "id": "pmss-ttyd-style",
      "name": "Paper Mario: Sticker Star TTYD Style",
      "title": "Paper Mario: Sticker Star",
      "title_ids": [
        "00040000000A5F00"
      ],
      "author": "Juggling gamer",
      "author_url": "https://gamebanana.com/members/2807970",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/461906",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 461906,
        "file_pattern": "paper_mario_sticker_star_ttyd_style_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/MODS/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "textures": {
        "rules": [
          {
            "from": "*/Texture Pack/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Restyles the game after the first three Paper Mario games: new models, sprites, text, music and a full retexture of the world. It is built for the European game, so the North American and Japanese versions are not covered. The pack is about 846 MB, and its optional emblem textures are not installed.",
      "downloads": 1532,
      "updated": "2023-10"
    },
    {
      "id": "pmss-mario-animation-revamp",
      "name": "Mario Animation Revamp",
      "title": "Paper Mario: Sticker Star",
      "title_ids": [
        "00040000000A5E00",
        "00040000000A5F00"
      ],
      "author": "Br123 T T",
      "author_url": "https://gamebanana.com/members/3670785",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/564315",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 564315,
        "file_pattern": "mario_revamp_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/Animations revamp/Smooth/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives Mario smoother animations and extra scripted moves for the North American and European versions. Only the Smooth animation set is installed; the Choppy set, the extra back animations and the Paper Mario 64 and TTYD battle animations are not, because they overlap or can cause cutscene glitches. It replaces other Mario animation or script mods.",
      "downloads": 766,
      "updated": "2026-05"
    },
    {
      "id": "pmss-infinite-jump-hammer",
      "name": "Infinite Jump and Hammer",
      "title": "Paper Mario: Sticker Star",
      "title_ids": [
        "00040000000A5E00",
        "00040000000A5F00",
        "00040000000A5D00"
      ],
      "author": "JoeL.",
      "author_url": "https://gamebanana.com/members/5105121",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/646570",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 646570,
        "file_pattern": "infinite_hammer_and_jump_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives you back the jump or hammer sticker as soon as you use it in battle, including the shiny versions, so they never run out. It edits only battle scripts. Only the North American version was checked by the author; the European and Japanese title IDs are included on the assumption that the scripts are shared.",
      "downloads": 622,
      "updated": "2026-01"
    },
    {
      "id": "pmss-ai-sticker-star",
      "name": "AI Sticker Star",
      "title": "Paper Mario: Sticker Star",
      "title_ids": [
        "00040000000A5E00"
      ],
      "author": "Mayro",
      "author_url": "https://gamebanana.com/members/1569921",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/392990",
      "verified": false,
      "downloads": 616,
      "updated": "2022-07",
      "source": {
        "type": "gamebanana",
        "mod": 392990,
        "file_pattern": "ai_sticker_star.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the game's English text with AI-generated text, written from the first few words of each line. It includes swearing and sensitive topics and does not change images such as the Talk prompt. It edits the US English messages only, so it works on the North American version."
    },
    {
      "id": "pmss-skippable-dialogue",
      "name": "Skippable Dialogue",
      "title": "Paper Mario: Sticker Star",
      "title_ids": [
        "00040000000A5E00",
        "00040000000A5F00",
        "00040000000A5D00"
      ],
      "author": "FlareDoesThing",
      "author_url": "https://gamebanana.com/members/2028441",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/606337",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 606337,
        "file_pattern": "skippable_dialogue_mod_for_pmss.rar"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets you skip almost every line of dialogue. It replaces a single script file, so it clashes with other script mods. The author only states that it works on Sticker Star, so the European and Japanese title IDs are unconfirmed.",
      "downloads": 613,
      "updated": "2025-08"
    },
    {
      "id": "smm3ds-super-luigi-maker",
      "name": "Super Luigi Maker",
      "title": "Super Mario Maker for Nintendo 3DS",
      "title_ids": [
        "00040000001A0400",
        "00040000001A0500",
        "00040000001A0300"
      ],
      "author": "Ultmate_Mario",
      "author_url": "https://gamebanana.com/members/1644463",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/248298",
      "verified": false,
      "downloads": 2217,
      "updated": "2018-04",
      "source": {
        "type": "gamebanana",
        "mod": 248298,
        "file_pattern": "super_luigi_maker_3ds_port.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Ports the Wii U Super Luigi Maker look to three of the four game styles by replacing the player models. The New Super Mario Bros. U style still shows Mario and the flags do not work, as the author notes. It clashes with other model mods for this game."
    },
    {
      "id": "smm3ds-all-stars-smb1-smb3",
      "name": "All-Stars Mod (SMB1 + SMB3)",
      "title": "Super Mario Maker for Nintendo 3DS",
      "title_ids": [
        "00040000001A0400",
        "00040000001A0500",
        "00040000001A0300"
      ],
      "author": "realguest3920",
      "author_url": "https://gamebanana.com/members/1651072",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/248299",
      "verified": false,
      "downloads": 2078,
      "updated": "2019-01",
      "source": {
        "type": "gamebanana",
        "mod": 248299,
        "file_pattern": "super_mario_maker_3ds_all_stars_mod_*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "Model/",
            "to": "romfs/Model/"
          },
          {
            "from": "Sound/",
            "to": "romfs/Sound/"
          },
          {
            "from": "Textures/",
            "to": "romfs/Textures/"
          },
          {
            "from": "TileData/",
            "to": "romfs/TileData/"
          },
          {
            "from": "PaletteIconTextures/",
            "to": "romfs/PaletteIconTextures/"
          }
        ]
      },
      "notes": "Replaces the 8-bit Super Mario Bros. and Super Mario Bros. 3 styles with the 16-bit All-Stars art. It is unfinished: the enemy sprites, sounds, question blocks and brick blocks are not ported yet. The archive is 624 MB and its layout was read from the file list only."
    },
    {
      "id": "smm3ds-smm2-themes",
      "name": "Super Mario Maker 2 Themes",
      "title": "Super Mario Maker for Nintendo 3DS",
      "title_ids": [
        "00040000001A0400",
        "00040000001A0500",
        "00040000001A0300"
      ],
      "author": "Ghost2083043",
      "author_url": "https://gamebanana.com/members/2083043",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/357832",
      "verified": false,
      "downloads": 1691,
      "updated": "2022-02",
      "source": {
        "type": "gamebanana",
        "mod": 357832,
        "file_pattern": "smm2ds_release_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "A2D/",
            "to": "romfs/A2D/"
          },
          {
            "from": "Sound/",
            "to": "romfs/Sound/"
          },
          {
            "from": "Textures/",
            "to": "romfs/Textures/"
          },
          {
            "from": "TileData/",
            "to": "romfs/TileData/"
          }
        ]
      },
      "notes": "Brings the Super Mario Maker 2 themes and music into the 3DS game as loose game files. The archive has no romfs folder, so the install rules place its folders inside romfs. The 95 MB download replaces a lot of the game's tile and sound data, so it clashes with other style mods."
    },
    {
      "id": "smm3ds-wonder-gamestyle",
      "name": "Super Mario Bros. Wonder Gamestyle",
      "title": "Super Mario Maker for Nintendo 3DS",
      "title_ids": [
        "00040000001A0400",
        "00040000001A0500",
        "00040000001A0300"
      ],
      "author": "Ghost3976408",
      "author_url": "https://gamebanana.com/members/3976408",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/568031",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 568031,
        "file_pattern": "smb_wonder_gamestyle_in_smm3ds_.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the whole New Super Mario Bros. U game style with Super Mario Bros. Wonder themes, tilesets, semi-solids, sound effects and music. It also changes the editor music, backgrounds and palette icons. The download is 275 MB and the style it replaces is gone while it is installed.",
      "downloads": 1179,
      "updated": "2025-01"
    },
    {
      "id": "smm3ds-ultimate-moveset",
      "name": "Ultimate Moveset in All Gamestyles",
      "title": "Super Mario Maker for Nintendo 3DS",
      "title_ids": [
        "00040000001A0400",
        "00040000001A0500",
        "00040000001A0300"
      ],
      "author": "Ghost2209397",
      "author_url": "https://gamebanana.com/members/2209397",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/394392",
      "verified": false,
      "downloads": 436,
      "updated": "2022-08",
      "source": {
        "type": "gamebanana",
        "mod": 394392,
        "file_pattern": "ultimate_mario_all_styles_.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives every game style an expanded moveset, such as wall jump in the Super Mario Bros. style and spin jump in the Super Mario Bros. 3 style. It is four small data files. It clashes with other mods that edit the same move data."
    },
    {
      "id": "mpit-klonoa-over-mario",
      "name": "Klonoa over Mario",
      "title": "Mario Party: Island Tour",
      "title_ids": [
        "00040000000F8100",
        "00040000000F8200",
        "00040000000DD400"
      ],
      "author": "Miokai",
      "author_url": "https://gamebanana.com/members/2105676",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/470317",
      "verified": false,
      "downloads": 138,
      "updated": "2023-09",
      "source": {
        "type": "gamebanana",
        "mod": 470317,
        "file_pattern": "klonoa_over_mario_*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces Mario's model with Klonoa. It is a model swap only and was made for the North American game; the author says it also runs on a real console. The European and Japanese title IDs are included but unconfirmed, and Mario is gone while it is installed."
    },
    {
      "id": "mpit-sm64-mario",
      "name": "Mario (Super Mario 64)",
      "title": "Mario Party: Island Tour",
      "title_ids": [
        "00040000000F8100",
        "00040000000F8200",
        "00040000000DD400"
      ],
      "author": "ExoDendrite",
      "author_url": "https://gamebanana.com/members/3165165",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/725818",
      "verified": false,
      "downloads": 16,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 725818,
        "file_pattern": "sm64-mario-island-tour.zip"
      },
      "install": {
        "rules": [
          {
            "from": "system/",
            "to": "romfs/system/"
          }
        ]
      },
      "notes": "Replaces Mario's model with the Super Mario 64 version, with working mesh animations. It is a model swap only, and the zip has no romfs folder, so the install rules add it. The European and Japanese title IDs are included but unconfirmed."
    },
    {
      "id": "mpit-waluigi-rgba4",
      "name": "Waluigi (Mario Party 3), RGBA4",
      "title": "Mario Party: Island Tour",
      "title_ids": [
        "00040000000F8100",
        "00040000000F8200",
        "00040000000DD400"
      ],
      "author": "ExoDendrite",
      "author_url": "https://gamebanana.com/members/3165165",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/727118",
      "verified": false,
      "downloads": 13,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 727118,
        "file_pattern": "n64-waluigi-island-tour.zip"
      },
      "install": {
        "rules": [
          {
            "from": "system/",
            "to": "romfs/system/"
          }
        ]
      },
      "notes": "Swaps one character model for Waluigi as he appears in Mario Party 3, using RGBA4 textures for better quality. It is a model swap only and takes a slot, so that character is gone while it is installed. Use the ETC1A4 entry instead for the lighter version; the two overwrite each other."
    },
    {
      "id": "mpit-waluigi-etc1a4",
      "name": "Waluigi (Mario Party 3), ETC1A4",
      "title": "Mario Party: Island Tour",
      "title_ids": [
        "00040000000F8100",
        "00040000000F8200",
        "00040000000DD400"
      ],
      "author": "ExoDendrite",
      "author_url": "https://gamebanana.com/members/3165165",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/727118",
      "verified": false,
      "downloads": 13,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 727118,
        "file_pattern": "n64-waluigi-island-tour-etc1a4.zip"
      },
      "install": {
        "rules": [
          {
            "from": "system/",
            "to": "romfs/system/"
          }
        ]
      },
      "notes": "The same Mario Party 3 Waluigi model swap, using ETC1A4 textures that are lighter on the system but a little lower in quality. It takes a character slot, so that character is gone while it is installed. It overwrites the RGBA4 version, so use only one."
    },
    {
      "id": "mpit-robo-mario",
      "name": "Robo-Mario",
      "title": "Mario Party: Island Tour",
      "title_ids": [
        "00040000000F8100",
        "00040000000F8200",
        "00040000000DD400"
      ],
      "author": "ExoDendrite",
      "author_url": "https://gamebanana.com/members/3165165",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/725107",
      "verified": false,
      "downloads": 8,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 725107,
        "file_pattern": "robo-mario-island-tour.zip"
      },
      "install": {
        "rules": [
          {
            "from": "system/",
            "to": "romfs/system/"
          }
        ]
      },
      "notes": "Puts the Robo-Mario model from Mario Kart Arcade GP in Mario's slot. It is a model swap only, and the zip has no romfs folder, so the install rules add it. Mario is gone while it is installed."
    },
    {
      "id": "mpsr-fire-bros",
      "name": "Fire Bros.",
      "title": "Mario Party: Star Rush",
      "title_ids": [
        "000400000019BD00",
        "000400000019BE00",
        "0004000000193900"
      ],
      "author": "ExoDendrite",
      "author_url": "https://gamebanana.com/members/3165165",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/549305",
      "verified": false,
      "downloads": 142,
      "updated": "2024-10",
      "source": {
        "type": "gamebanana",
        "mod": 549305,
        "file_pattern": "firebros.zip"
      },
      "install": {
        "rules": [
          {
            "from": "common/",
            "to": "romfs/common/"
          }
        ]
      },
      "notes": "Retextures Mario, Luigi, Wario and Waluigi as their fire variants. There is no menu art yet, and the zip has no romfs folder, so the install rules add it. The European and Japanese title IDs are included but unconfirmed."
    },
    {
      "id": "mpsr-morgana-over-yoshi",
      "name": "Morgana (Persona 5) over Yoshi",
      "title": "Mario Party: Star Rush",
      "title_ids": [
        "000400000019BD00",
        "000400000019BE00",
        "0004000000193900"
      ],
      "author": "Miokai",
      "author_url": "https://gamebanana.com/members/2105676",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/419993",
      "verified": false,
      "downloads": 67,
      "updated": "2023-01",
      "source": {
        "type": "gamebanana",
        "mod": 419993,
        "file_pattern": "morgana_over_yoshi.rar"
      },
      "install": {
        "rules": [
          {
            "from": "*/common/",
            "to": "romfs/common/"
          },
          {
            "from": "*/flows/",
            "to": "romfs/flows/"
          }
        ]
      },
      "notes": "Replaces Yoshi with Morgana from Persona 5 and changes some of the menu art. The author says it only works in the emulator and not on a real 3DS, and that it is unfinished. Yoshi is gone while it is installed."
    },
    {
      "id": "mpsr-klonoa",
      "name": "Klonoa",
      "title": "Mario Party: Star Rush",
      "title_ids": [
        "000400000019BD00",
        "000400000019BE00",
        "0004000000193900"
      ],
      "author": "Miokai",
      "author_url": "https://gamebanana.com/members/2105676",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/450474",
      "verified": false,
      "downloads": 67,
      "updated": "2023-06",
      "source": {
        "type": "gamebanana",
        "mod": 450474,
        "file_pattern": "klonoa_*.rar"
      },
      "install": {
        "rules": [
          {
            "from": "*/common/",
            "to": "romfs/common/"
          }
        ]
      },
      "notes": "Puts Klonoa in Mario's slot. The author notes it has no menu art yet, and Mario is gone while it is installed. The European and Japanese title IDs are included but unconfirmed."
    },
    {
      "id": "mpsr-red-yoshi",
      "name": "Red Yoshi",
      "title": "Mario Party: Star Rush",
      "title_ids": [
        "000400000019BD00",
        "000400000019BE00",
        "0004000000193900"
      ],
      "author": "Theovennce2",
      "author_url": "https://gamebanana.com/members/1965179",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/582392",
      "verified": false,
      "downloads": 41,
      "updated": "2025-03",
      "source": {
        "type": "gamebanana",
        "mod": 582392,
        "file_pattern": "redyoshi_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "common/",
            "to": "romfs/common/"
          }
        ]
      },
      "notes": "Retextures the green Yoshi red, which then shows in the minigames. It replaces the normal Yoshi texture file, so it clashes with other Yoshi mods. The contents are a single texture file, and the European and Japanese title IDs are included but unconfirmed."
    },
    {
      "id": "mpsr-no-amiibo-icon",
      "name": "No Amiibo Icon for Characters",
      "title": "Mario Party: Star Rush",
      "title_ids": [
        "000400000019BD00"
      ],
      "author": "Darkness Illusion",
      "author_url": "https://gamebanana.com/members/1507865",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/633531",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 633531,
        "file_pattern": "no_amiibo_tag_mod.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/titles/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Removes the small amiibo tag that sits above a character's head after you use their amiibo. It replaces one UI file and works on the North American version. It clashes with other mods that edit the same menu file.",
      "downloads": 23,
      "updated": "2025-11"
    },
    {
      "id": "mto-sm64-mario",
      "name": "SM64 Mario",
      "title": "Mario Tennis Open",
      "title_ids": [
        "000400000007C700",
        "000400000007C800",
        "0004000000064D00"
      ],
      "author": "NintendoGameTuber",
      "author_url": "https://gamebanana.com/members/1672096",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/592969",
      "verified": false,
      "downloads": 55,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 592969,
        "file_pattern": "sm64_mario_mto.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces Mario's player model with the Super Mario 64 version, including the trophy ceremony model. It is two model files, so it clashes with other Mario model mods. The author names no region, so the European and Japanese title IDs are included but unconfirmed."
    },
    {
      "id": "mto-sm64-luigi",
      "name": "SM64 Luigi",
      "title": "Mario Tennis Open",
      "title_ids": [
        "000400000007C700",
        "000400000007C800",
        "0004000000064D00"
      ],
      "author": "NintendoGameTuber",
      "author_url": "https://gamebanana.com/members/1672096",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/593197",
      "verified": false,
      "downloads": 39,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 593197,
        "file_pattern": "sm64_luigi_mto.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces Luigi's player model with the Super Mario 64 version, including the ceremony model. It is two model files and clashes with other Luigi model mods. The author names no region, so the European and Japanese title IDs are included but unconfirmed."
    },
    {
      "id": "mto-sm64-metal-mario",
      "name": "SM64 Metal Mario",
      "title": "Mario Tennis Open",
      "title_ids": [
        "000400000007C700",
        "000400000007C800",
        "0004000000064D00"
      ],
      "author": "NintendoGameTuber",
      "author_url": "https://gamebanana.com/members/1672096",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/592980",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 592980,
        "file_pattern": "sm64_metal_mario_mto.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Metal Mario player model with the Super Mario 64 version, including the ceremony model. It is two model files and clashes with other Metal Mario mods. The author names no region, so the European and Japanese title IDs are included but unconfirmed.",
      "downloads": 27,
      "updated": "2026-05"
    },
    {
      "id": "mto-sonic-over-diddy",
      "name": "Sonic over Diddy Kong",
      "title": "Mario Tennis Open",
      "title_ids": [
        "000400000007C700",
        "000400000007C800",
        "0004000000064D00"
      ],
      "author": "ouch9922",
      "author_url": "https://gamebanana.com/members/4987006",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/636384",
      "verified": false,
      "downloads": 55,
      "updated": "2025-11",
      "source": {
        "type": "gamebanana",
        "mod": 636384,
        "file_pattern": "mario_tennis_open_-_sonic_over_diddy_kong.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/model/",
            "to": "romfs/model/"
          },
          {
            "from": "*/snd/",
            "to": "romfs/snd/"
          }
        ]
      },
      "notes": "Replaces Diddy Kong with Sonic the Hedgehog, with new character and ceremony models plus voice and sound files. The zip has no romfs folder, so the install rules add it. Diddy Kong is gone while it is installed, and the European and Japanese title IDs are unconfirmed."
    },
    {
      "id": "mto-rosalina-over-peach",
      "name": "Rosalina over Peach",
      "title": "Mario Tennis Open",
      "title_ids": [
        "000400000007C700",
        "000400000007C800",
        "0004000000064D00"
      ],
      "author": "ouch9922",
      "author_url": "https://gamebanana.com/members/4987006",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/636574",
      "verified": false,
      "downloads": 44,
      "updated": "2026-04",
      "source": {
        "type": "gamebanana",
        "mod": 636574,
        "file_pattern": "mario_tennis_open_-_rosalina_over_peach.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/model/",
            "to": "romfs/model/"
          },
          {
            "from": "*/snd/",
            "to": "romfs/snd/"
          }
        ]
      },
      "notes": "Replaces Peach with Rosalina, with new character and ceremony models plus sound files. The zip has no romfs folder, so the install rules add it. Peach is gone while it is installed, and the European and Japanese title IDs are unconfirmed."
    },
    {
      "id": "tetris-axis-katyusha-threemix",
      "name": "Katyusha Threemix over The Gypsy Dance",
      "title": "Tetris Axis",
      "title_ids": [
        "0004000000043800"
      ],
      "author": "Imora",
      "author_url": "https://gamebanana.com/members/1774141",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/579500",
      "verified": false,
      "downloads": 25,
      "updated": "2026-02",
      "source": {
        "type": "gamebanana",
        "mod": 579500,
        "file_pattern": "bcsar_and_text_replacement.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Gypsy Dance track in Capture Mode with the previously unused Katyusha Threemix, and changes the matching name in the English text. The author only tested the North American version, and it replaces the whole sound archive, so it clashes with other music mods. The download is 74 MB."
    },
    {
      "id": "rh-ptbr-translation",
      "name": "Brazilian Portuguese Translation",
      "title": "Radiant Historia: Perfect Chronology",
      "title_ids": [
        "00040000001C8F00"
      ],
      "author": "Ahtheerr",
      "author_url": "https://gamebanana.com/members/3673388",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/640252",
      "verified": false,
      "downloads": 951,
      "updated": "2025-12",
      "source": {
        "type": "gamebanana",
        "mod": 640252,
        "file_pattern": "radiant_historia_ptbr_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A Brazilian Portuguese translation covering the story, side quests, items and most menus. It replaces the game's message and font files, and the contents are about 17,000 small files. GameBanana has not marked the file as scanned clean, which the app requires, so the download is refused until that changes."
    },
    {
      "id": "eo2u-hd-remaster-portraits",
      "name": "HD Remaster Portraits",
      "title": "Etrian Odyssey II Untold: The Fafnir Knight",
      "title_ids": [
        "000400000015F200",
        "000400000016E900",
        "0004000000120500",
        "000400000016D700"
      ],
      "author": "Nicole Beans",
      "author_url": "https://gamebanana.com/members/2102923",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/487683",
      "verified": false,
      "downloads": 384,
      "updated": "2024-01",
      "source": {
        "type": "gamebanana",
        "mod": 487683,
        "file_pattern": "etrianhd-to-3d_regional_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/*/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Adds the new class portraits from the Etrian Odyssey HD Remaster by overwriting the extra DLC portrait slots. It needs the Yggdrasil Veteran DLC portrait pack installed, otherwise only one new portrait shows. The 1.1 GB download replaces the game's main data archive.",
      "size": 1155422774
    },
    {
      "id": "pkmn-sun-shiny-lock-removal",
      "name": "Shiny Lock Removal",
      "title": "Pokémon Sun",
      "title_ids": [
        "0004000000164800"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486577",
      "verified": false,
      "downloads": 975,
      "updated": "2023-12",
      "source": {
        "type": "gamebanana",
        "mod": 486577,
        "file_pattern": "sun_citra_shiny_lock_removal_37bae.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from the game, so every legendary and gift Pokémon can be shiny. It ships a patched game code file, so it only works with the base 1.0.0 game; delete any installed update data first. It clashes with other code patches and does not include the Zygarde and in-game trade version."
    },
    {
      "id": "pkmn-moon-shiny-lock-removal",
      "name": "Shiny Lock Removal",
      "title": "Pokémon Moon",
      "title_ids": [
        "0004000000175E00"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486577",
      "verified": false,
      "downloads": 975,
      "updated": "2023-12",
      "source": {
        "type": "gamebanana",
        "mod": 486577,
        "file_pattern": "moon_citra_shiny_lock_removal_7744b.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from the game, so every legendary and gift Pokémon can be shiny. It ships a patched game code file, so it only works with the base 1.0.0 game; delete any installed update data first. It clashes with other code patches and does not include the Zygarde and in-game trade version."
    },
    {
      "id": "pkmn-sun-shiny-lock-removal-v12",
      "name": "Shiny Lock Removal (version 1.2)",
      "title": "Pokémon Sun",
      "title_ids": [
        "0004000000164800"
      ],
      "author": "itsskinnymems",
      "author_url": "https://gamebanana.com/members/2669985",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/513467",
      "verified": false,
      "downloads": 650,
      "updated": "2025-06",
      "source": {
        "type": "gamebanana",
        "mod": 513467,
        "file_pattern": "pok_mon_sun_12_-_citra_normal__331fb.rar"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from version 1.2 of the game, so every legendary and gift Pokémon can be shiny. It needs the 1.2 update installed and crashes loading a save on any other version. It is a code patch, so it clashes with other code patches, and it does not include the Zygarde and in-game trade version."
    },
    {
      "id": "pkmn-moon-shiny-lock-removal-v12",
      "name": "Shiny Lock Removal (version 1.2)",
      "title": "Pokémon Moon",
      "title_ids": [
        "0004000000175E00"
      ],
      "author": "itsskinnymems",
      "author_url": "https://gamebanana.com/members/2669985",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/513467",
      "verified": false,
      "downloads": 650,
      "updated": "2025-06",
      "source": {
        "type": "gamebanana",
        "mod": 513467,
        "file_pattern": "pok_mon_moon_12_-_citra_normal__312a4.rar"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from version 1.2 of the game, so every legendary and gift Pokémon can be shiny. It needs the 1.2 update installed and crashes loading a save on any other version. It is a code patch, so it clashes with other code patches, and it does not include the Zygarde and in-game trade version."
    },
    {
      "id": "pkmn-ultrasun-shiny-lock-removal",
      "name": "Shiny Lock Removal",
      "title": "Pokémon Ultra Sun",
      "title_ids": [
        "00040000001B5000"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486586",
      "verified": false,
      "downloads": 4388,
      "updated": "2025-12",
      "source": {
        "type": "gamebanana",
        "mod": 486586,
        "file_pattern": "ultrasun_citra_shiny_lock_removal_3dcd4.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from the game, so every legendary and gift Pokémon can be shiny. It ships a patched game code file, so it only works with the base 1.0.0 game; delete any installed update data first. It clashes with other code patches and does not include the Zygarde and in-game trade version."
    },
    {
      "id": "pkmn-ultramoon-shiny-lock-removal",
      "name": "Shiny Lock Removal",
      "title": "Pokémon Ultra Moon",
      "title_ids": [
        "00040000001B5100"
      ],
      "author": "crashandcortex",
      "author_url": "https://gamebanana.com/members/1495358",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/486586",
      "verified": false,
      "downloads": 4388,
      "updated": "2025-12",
      "source": {
        "type": "gamebanana",
        "mod": 486586,
        "file_pattern": "ultramoon_citra_shiny_lock_removal_40871.zip"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from the game, so every legendary and gift Pokémon can be shiny. It ships a patched game code file, so it only works with the base 1.0.0 game; delete any installed update data first. It clashes with other code patches and does not include the Zygarde and in-game trade version."
    },
    {
      "id": "pkmn-ultrasun-shiny-lock-removal-v12",
      "name": "Shiny Lock Removal (version 1.2)",
      "title": "Pokémon Ultra Sun",
      "title_ids": [
        "00040000001B5000"
      ],
      "author": "itsskinnymems",
      "author_url": "https://gamebanana.com/members/2669985",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/513470",
      "verified": false,
      "downloads": 2181,
      "updated": "2025-06",
      "source": {
        "type": "gamebanana",
        "mod": 513470,
        "file_pattern": "pok_mon_ultra_sun_12_-_citra_normal__8cccc.rar"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from version 1.2 of the game, so every legendary and gift Pokémon can be shiny. It needs the 1.2 update installed and crashes loading a save on any other version. It is a code patch, so it clashes with other code patches, and it does not include the Zygarde and in-game trade version."
    },
    {
      "id": "pkmn-ultramoon-shiny-lock-removal-v12",
      "name": "Shiny Lock Removal (version 1.2)",
      "title": "Pokémon Ultra Moon",
      "title_ids": [
        "00040000001B5100"
      ],
      "author": "itsskinnymems",
      "author_url": "https://gamebanana.com/members/2669985",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/513470",
      "verified": false,
      "downloads": 2181,
      "updated": "2025-06",
      "source": {
        "type": "gamebanana",
        "mod": 513470,
        "file_pattern": "pok_mon_ultra_moon_12_-_citra_normal__c0897.rar"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/",
            "to": "exefs/"
          },
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Removes the shiny lock from version 1.2 of the game, so every legendary and gift Pokémon can be shiny. It needs the 1.2 update installed and crashes loading a save on any other version. It is a code patch, so it clashes with other code patches, and it does not include the Zygarde and in-game trade version."
    },
    {
      "id": "pkmn-sm-trans-ride-gear",
      "name": "Trans Pride Ride Gear",
      "title": "Pokémon Sun / Pokémon Moon",
      "title_ids": [
        "0004000000164800",
        "0004000000175E00"
      ],
      "author": "Primm",
      "author_url": "https://gamebanana.com/members/2053313",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/435809",
      "verified": false,
      "downloads": 377,
      "updated": "2023-04",
      "source": {
        "type": "gamebanana",
        "mod": 435809,
        "file_pattern": "trans_pride_ride_gear.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Recolours the Ride Gear in the light pink and blue of the trans flag, for both the girl and boy characters. It replaces four model files, so it clashes with other Ride Gear mods. It is a game-file replacement, not a texture pack."
    },
    {
      "id": "pkmn-sm-playable-lillie",
      "name": "Playable Lillie",
      "title": "Pokémon Sun / Pokémon Moon",
      "title_ids": [
        "0004000000164800",
        "0004000000175E00"
      ],
      "author": "Camyoshi",
      "author_url": "https://gamebanana.com/members/3247750",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/679771",
      "verified": false,
      "downloads": 135,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 679771,
        "file_pattern": "playable_lillie_sm_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the male player character with Lillie, with matching overworld, battle and Ride outfit models for every clothing piece except glasses. Only the model files are installed; the optional dialogue changes that refer to Lillie are not. It clashes with other player model mods."
    },
    {
      "id": "pkmn-usum-accurate-sizes",
      "name": "Accurate Pokémon Sizes",
      "title": "Pokémon Ultra Sun / Pokémon Ultra Moon",
      "title_ids": [
        "00040000001B5000",
        "00040000001B5100"
      ],
      "author": "The Pod",
      "author_url": "https://gamebanana.com/members/2034086",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/694458",
      "verified": false,
      "downloads": 1154,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 694458,
        "file_pattern": "accuratepokemonsizes_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Adjusts battle and overworld sizes so Pokémon match their real-world sizes. It replaces the Pokémon model archive, so any other mod that edits that file needs merging with it first. The download is 703 MB, and the author notes that Salandit shows only outlines."
    },
    {
      "id": "pkmn-us-uhuru-gift-korean",
      "name": "Uhuru Gift (Korean), Gen 8 and 9 Expansion",
      "title": "Pokémon Ultra Sun",
      "title_ids": [
        "00040000001B5000"
      ],
      "author": "kingksb0120",
      "author_url": "https://gamebanana.com/members/5932136",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/723487",
      "verified": false,
      "source": {
        "type": "gdrive",
        "file_id": "163yDRwkKdzk9rStlPZ1s_Bp2CoIjZHES"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "An unofficial Korean edition of Aqua's Gen 8 and 9 expansion, adding Terastal, Dynamax, tournaments, Boss Rush and a roguelike tower. It needs Pokémon Ultra Sun (USA) version 1.0 with no update data. The mod-folder build is about 6.5 GB on Google Drive, and it replaces the game code and data.",
      "downloads": 201,
      "updated": "2026-10",
      "size": 6494646872
    },
    {
      "id": "albw-young-link-hero-of-time",
      "name": "Hero of Time: Young Link",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Takeshre",
      "author_url": "https://gamebanana.com/members/2184805",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/659766",
      "verified": false,
      "downloads": 6030,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 659766,
        "file_pattern": "young_link.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Swaps Link for the Young Link of Ocarina of Time and Majora's Mask, with a Kokiri Sword, Deku Shield, Hero's Shield and more. Only one Link model can be active at a time, so it clashes with the Adult Link entry and other Link mods. The voice pack is a separate download and is not installed."
    },
    {
      "id": "albw-adult-link-hero-of-time",
      "name": "Hero of Time: Adult Link",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Takeshre",
      "author_url": "https://gamebanana.com/members/2184805",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/659766",
      "verified": false,
      "downloads": 6030,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 659766,
        "file_pattern": "adult_link.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Swaps Link for the Adult Link of Ocarina of Time, with matching equipment. Only one Link model can be active at a time, so it clashes with the Young Link entry and other Link mods. The voice pack is a separate download and is not installed."
    },
    {
      "id": "albw-toon-link",
      "name": "Toon Link Between Worlds",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Takeshre",
      "author_url": "https://gamebanana.com/members/2184805",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/484137",
      "verified": false,
      "downloads": 2418,
      "updated": "2025-01",
      "source": {
        "type": "gamebanana",
        "mod": 484137,
        "file_pattern": "toon_link_between_worlds.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Lets you play as Toon Link using the Tri Force Heroes model, with Wind Waker-style swords and shields and edited cutscenes. It clashes with other Link model mods. The Wind Waker voice pack is a separate download and is not installed."
    },
    {
      "id": "albw-fighters-shield-basic",
      "name": "Fighter's Shield over Basic Shield",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Takeshre",
      "author_url": "https://gamebanana.com/members/2184805",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/479645",
      "verified": false,
      "downloads": 1087,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 479645,
        "file_pattern": "fighter_over_basic_shield_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Adds the Fighter's Shield from A Link to the Past in place of the basic orange shield. The three shield variants overwrite each other, so use only one. The separate files for manual merging are not installed."
    },
    {
      "id": "albw-fighters-shield-hylian",
      "name": "Fighter's Shield over Hylian Shield",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Takeshre",
      "author_url": "https://gamebanana.com/members/2184805",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/479645",
      "verified": false,
      "downloads": 1087,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 479645,
        "file_pattern": "fighter_over_hylian_shield_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Adds the Fighter's Shield from A Link to the Past in place of the Hylian Shield. The three shield variants overwrite each other, so use only one. The separate files for manual merging are not installed."
    },
    {
      "id": "albw-fighters-shield-and-red",
      "name": "Fighter's Shield and Red Shield",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Takeshre",
      "author_url": "https://gamebanana.com/members/2184805",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/479645",
      "verified": false,
      "downloads": 1087,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 479645,
        "file_pattern": "fighter_and_red_shields_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the basic orange shield with the Fighter's Shield and the Hylian Shield with a red shield. The three shield variants overwrite each other, so use only one. The separate files for manual merging are not installed."
    },
    {
      "id": "albw-linkle-hooded",
      "name": "A Linkle between Worlds: Hooded Linkle",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Potocto",
      "author_url": "https://gamebanana.com/members/2097852",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/694590",
      "verified": false,
      "downloads": 1064,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 694590,
        "file_pattern": "hooded_linkle_8358e.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Changes Link to Linkle with the green hood, with her voice and some dialogue tweaks. The four Linkle variants overwrite each other, so use only one. The download is 60 MB and replaces many of the game's model archives."
    },
    {
      "id": "albw-linkle-hoodless",
      "name": "A Linkle between Worlds: Hoodless Linkle",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Potocto",
      "author_url": "https://gamebanana.com/members/2097852",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/694590",
      "verified": false,
      "downloads": 1064,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 694590,
        "file_pattern": "hoodless_linkle_7d39e.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Changes Link to Linkle without the hood, with her voice and some dialogue tweaks. The four Linkle variants overwrite each other, so use only one. The download is 60 MB and replaces many of the game's model archives."
    },
    {
      "id": "albw-linkle-hooded-pink",
      "name": "A Linkle between Worlds: Hooded Linkle (Pink Hair)",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Potocto",
      "author_url": "https://gamebanana.com/members/2097852",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/694590",
      "verified": false,
      "downloads": 1064,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 694590,
        "file_pattern": "hooded_linkle_pink_e932d.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Changes Link to Linkle with the green hood and pink hair, with her voice and some dialogue tweaks. The four Linkle variants overwrite each other, so use only one. The download is 60 MB and replaces many of the game's model archives."
    },
    {
      "id": "albw-linkle-hoodless-pink",
      "name": "A Linkle between Worlds: Hoodless Linkle (Pink Hair)",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Potocto",
      "author_url": "https://gamebanana.com/members/2097852",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/694590",
      "verified": false,
      "downloads": 1064,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 694590,
        "file_pattern": "hoodless_linkle_pink_a8984.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Changes Link to Linkle without the hood and with pink hair, with her voice and some dialogue tweaks. The four Linkle variants overwrite each other, so use only one. The download is 60 MB and replaces many of the game's model archives."
    },
    {
      "id": "albw-wolf-link",
      "name": "Wolf Link between Worlds",
      "title": "The Legend of Zelda: A Link Between Worlds",
      "title_ids": [
        "00040000000EC300",
        "00040000000EC400",
        "00040000000EC200"
      ],
      "author": "Potocto",
      "author_url": "https://gamebanana.com/members/2097852",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/679885",
      "verified": false,
      "downloads": 289,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 679885,
        "file_pattern": "wolf_link_albw_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces Link with Wolf Link, a commissioned mod that the author says also works on console. It replaces the Link model archives, so it clashes with other Link model mods."
    },
    {
      "id": "tfh-light-blue-pink-link-na",
      "name": "Light Blue + Pink Link Combined (North America)",
      "title": "The Legend of Zelda: Tri Force Heroes",
      "title_ids": [
        "0004000000176F00"
      ],
      "author": "Pebnut",
      "author_url": "https://gamebanana.com/members/2648384",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/500913",
      "verified": false,
      "downloads": 483,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 500913,
        "file_pattern": "lbp_na.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Recolours two of the Links light blue and pink at once, by editing the shared costume and course textures for the North America version. It replaces many of the game's costume files, so it clashes with other Link recolour mods. The matching colour cheat is a separate download and is not installed."
    },
    {
      "id": "tfh-light-blue-pink-link-eu",
      "name": "Light Blue + Pink Link Combined (Europe)",
      "title": "The Legend of Zelda: Tri Force Heroes",
      "title_ids": [
        "0004000000177000"
      ],
      "author": "Pebnut",
      "author_url": "https://gamebanana.com/members/2648384",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/500913",
      "verified": false,
      "downloads": 483,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 500913,
        "file_pattern": "lbp_eu.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Recolours two of the Links light blue and pink at once, by editing the shared costume and course textures for the Europe version. It replaces many of the game's costume files, so it clashes with other Link recolour mods. The matching colour cheat is a separate download and is not installed."
    },
    {
      "id": "tfh-light-blue-pink-link-jp",
      "name": "Light Blue + Pink Link Combined (Japan)",
      "title": "The Legend of Zelda: Tri Force Heroes",
      "title_ids": [
        "0004000000176E00"
      ],
      "author": "Pebnut",
      "author_url": "https://gamebanana.com/members/2648384",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/500913",
      "verified": false,
      "downloads": 483,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 500913,
        "file_pattern": "lbp_jp.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Recolours two of the Links light blue and pink at once, by editing the shared costume and course textures for the Japan version. It replaces many of the game's costume files, so it clashes with other Link recolour mods. The matching colour cheat is a separate download and is not installed."
    },
    {
      "id": "tfh-light-blue-pink-link-kr",
      "name": "Light Blue + Pink Link Combined (Korea)",
      "title": "The Legend of Zelda: Tri Force Heroes",
      "title_ids": [
        "0004000000182B00"
      ],
      "author": "Pebnut",
      "author_url": "https://gamebanana.com/members/2648384",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/500913",
      "verified": false,
      "downloads": 483,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 500913,
        "file_pattern": "lbp_kr.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Recolours two of the Links light blue and pink at once, by editing the shared costume and course textures for the Korea version. It replaces many of the game's costume files, so it clashes with other Link recolour mods. The matching colour cheat is a separate download and is not installed."
    },
    {
      "id": "tfh-hero-of-time-outfits",
      "name": "Hero of Time Outfits",
      "title": "The Legend of Zelda: Tri Force Heroes",
      "title_ids": [
        "0004000000176F00",
        "0004000000177000",
        "0004000000176E00",
        "0004000000182B00"
      ],
      "author": "Takeshre",
      "author_url": "https://gamebanana.com/members/2184805",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/664643",
      "verified": false,
      "downloads": 206,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 664643,
        "file_pattern": "hero_of_time_outfits.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Adds Young Link and Adult Link as outfits, replacing the Kokiri Clothes and the Sword Suit by default. You can swap them to other slots by renaming the files, as the included list explains. Disable other costume mods first; the Adult Link voice pack is a separate download and is not installed."
    },
    {
      "id": "tfh-lbw-link-blonde",
      "name": "LBW Link (Blonde)",
      "title": "The Legend of Zelda: Tri Force Heroes",
      "title_ids": [
        "0004000000176F00"
      ],
      "author": "Kobatuwu",
      "author_url": "https://gamebanana.com/members/1720019",
      "licence": "Not stated",
      "homepage": "https://gamebanana.com/mods/499743",
      "verified": false,
      "downloads": 156,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 499743,
        "file_pattern": "lbw_link_blonde_.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Adds the Link Between Worlds design in place of the Hero's Tunic, with blonde hair for all the Links. The model was built from an unused High Fairy costume. It is made for the North American version and overwrites the Multi-Coloured Hair version."
    },
    {
      "id": "tfh-lbw-link-multicolour",
      "name": "LBW Link (Multi-Coloured Hair)",
      "title": "The Legend of Zelda: Tri Force Heroes",
      "title_ids": [
        "0004000000176F00"
      ],
      "author": "Kobatuwu",
      "author_url": "https://gamebanana.com/members/1720019",
      "licence": "Not stated",
      "homepage": "https://gamebanana.com/mods/499743",
      "verified": false,
      "downloads": 156,
      "updated": "2024-03",
      "source": {
        "type": "gamebanana",
        "mod": 499743,
        "file_pattern": "lbw_link_multi_colored_hair_.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Adds the Link Between Worlds design in place of the Hero's Tunic, with a different hair colour for each Link. The model was built from an unused High Fairy costume. It is made for the North American version and overwrites the Blonde version."
    },
    {
      "id": "tfh-blond-links",
      "name": "Blond Links",
      "title": "The Legend of Zelda: Tri Force Heroes",
      "title_ids": [
        "0004000000176F00",
        "0004000000177000",
        "0004000000176E00",
        "0004000000182B00"
      ],
      "author": "Takeshre",
      "author_url": "https://gamebanana.com/members/2184805",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/670896",
      "verified": false,
      "downloads": 123,
      "updated": "2026-04",
      "source": {
        "type": "gamebanana",
        "mod": 670896,
        "file_pattern": "blond_links.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Keeps Link's hair blond in multiplayer whatever colour the player is, and removes the yellow stripe from the Hero's Tunic hat. It also updates the matching UI and communication icons for the North American, European, Japanese and Korean versions. It clashes with other costume mods,."
    },
    {
      "id": "tfh-pink-link",
      "name": "Pink Link Mod",
      "title": "The Legend of Zelda: Tri Force Heroes",
      "title_ids": [
        "0004000000176F00"
      ],
      "author": "0atmeal",
      "author_url": "https://gamebanana.com/members/2067350",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/351573",
      "verified": false,
      "downloads": 122,
      "updated": "2023-06",
      "source": {
        "type": "gamebanana",
        "mod": 351573,
        "file_pattern": "pink_link_mod_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Recolours the red Link pink across all 39 outfits, the multiplayer emotes, the loading screen icons, the Poes and the platforms. The author says it only works with the English North American version. It clashes with other Link recolour mods."
    },
    {
      "id": "acnl-clean-texture-pack",
      "name": "Clean Texture Pack",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": "maniman303",
      "author_url": "https://gamebanana.com/members/1682573",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/40778",
      "verified": false,
      "downloads": 4270,
      "updated": "2020-11",
      "source": {
        "type": "gamebanana",
        "mod": 40778,
        "file_pattern": "acnl_clean_texture_pack_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the grass, snow, cliff and sky textures in a New Horizons-inspired style, cleans up the villagers' fur and redoes a few wallpapers, the giant teddy bears and the TIY banner. Its files come from the Europe Welcome amiibo release, so the USA and Europe versions of the base game and of Welcome amiibo are listed. The author calls it a work in progress, and it replaces the same files as other New Leaf look mods."
    },
    {
      "id": "acnl-wolfsbane-cut-unisex",
      "name": "Wolfsbane Cut (Unisex)",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/595080",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 595080,
        "file_pattern": "wolfsbane_cut_3ds_fix_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Zelda wig with a longer, unisex wolf-cut hairstyle that can be coloured and has a glowing star charm. This is the build the author made for the 3DS; a no-star version and a note for Japanese games are on the page but not installed. You need to own the Zelda wig in your game to see it.",
      "downloads": 1654,
      "updated": "2026-03"
    },
    {
      "id": "acnl-census-menu-restoration",
      "name": "Census Menu Restoration",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": "pearl23",
      "author_url": "https://gamebanana.com/members/1782773",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/522625",
      "verified": false,
      "downloads": 1484,
      "updated": "2025-08",
      "source": {
        "type": "gamebanana",
        "mod": 522625,
        "file_pattern": "census_menu_restored_v4.rar"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Restores the Census menu with its values and title text. It needs a save editor (ACSE) or the Vapecord plugin to switch the menu on in your save, so it does nothing on its own. This is the newest of four archive versions on the page."
    },
    {
      "id": "acnl-elf-ears-lighter",
      "name": "Elf Ears (Lighter Tones)",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/600563",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 600563,
        "file_pattern": "elf_ears_lighter_tones_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Superhero mask accessory with pointed ears made for lighter skin tones. It is an accessory, so it can be worn with a hat or wig, and you need the Superhero mask in your game to see it. The five ear variants replace the same file, so install only one.",
      "downloads": 1049,
      "updated": "2026-10"
    },
    {
      "id": "acnl-elf-ears-darker",
      "name": "Elf Ears (Darker Tones)",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/600563",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 600563,
        "file_pattern": "elf_ears_darker_tones_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Superhero mask accessory with pointed ears made for darker skin tones. It is an accessory, so it can be worn with a hat or wig, and you need the Superhero mask in your game to see it. The five ear variants replace the same file, so install only one.",
      "downloads": 1049,
      "updated": "2026-10"
    },
    {
      "id": "acnl-elf-ears-medium",
      "name": "Elf Ears (Medium or Tan Tones)",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/600563",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 600563,
        "file_pattern": "elf_ears_medium_or_tan_tones_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Superhero mask accessory with pointed ears made for medium or tan skin tones. It is an accessory, so it can be worn with a hat or wig, and you need the Superhero mask in your game to see it. The five ear variants replace the same file, so install only one.",
      "downloads": 1049,
      "updated": "2026-10"
    },
    {
      "id": "acnl-elf-ears-zombie",
      "name": "Elf Ears (Fairy (Zombie, Green))",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/600563",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 600563,
        "file_pattern": "fairy_elf_ears_zombie_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Superhero mask accessory with pointed ears made for green skin colour, as fairy ears. It is an accessory, so it can be worn with a hat or wig, and you need the Superhero mask in your game to see it. The five ear variants replace the same file, so install only one.",
      "downloads": 1049,
      "updated": "2026-10"
    },
    {
      "id": "acnl-elf-ears-rose",
      "name": "Elf Ears (Fairy (Rose, Pink))",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/600563",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 600563,
        "file_pattern": "fairy_elf_ears_rose_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Superhero mask accessory with pointed ears made for pink skin colour, as fairy ears. It is an accessory, so it can be worn with a hat or wig, and you need the Superhero mask in your game to see it. The five ear variants replace the same file, so install only one.",
      "downloads": 1049,
      "updated": "2026-10"
    },
    {
      "id": "acnl-baby-twintails-blackberry",
      "name": "Baby Twintails (Blackberry)",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/621588",
      "verified": false,
      "downloads": 883,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 621588,
        "file_pattern": "baby_twintails_blackberry_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Pigtail wig with a bigger, retouched twintails hairstyle with bangs, in black. The wig cannot be recoloured in game, which is why each colour is its own download. The four colours replace the same file, so install only one."
    },
    {
      "id": "acnl-baby-twintails-blondie",
      "name": "Baby Twintails (Blondie)",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/621588",
      "verified": false,
      "downloads": 883,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 621588,
        "file_pattern": "baby_twintails_blondie_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Pigtail wig with a bigger, retouched twintails hairstyle with bangs, in blonde. The wig cannot be recoloured in game, which is why each colour is its own download. The four colours replace the same file, so install only one."
    },
    {
      "id": "acnl-baby-twintails-brownie",
      "name": "Baby Twintails (Brownie)",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/621588",
      "verified": false,
      "downloads": 883,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 621588,
        "file_pattern": "baby_twintails_brownie_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Pigtail wig with a bigger, retouched twintails hairstyle with bangs, in brown. The wig cannot be recoloured in game, which is why each colour is its own download. The four colours replace the same file, so install only one."
    },
    {
      "id": "acnl-baby-twintails-bubblegum",
      "name": "Baby Twintails (Bubblegum)",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400",
        "0004000000198E00",
        "0004000000198F00"
      ],
      "author": ".+MangoMikkii+.",
      "author_url": "https://gamebanana.com/members/3715646",
      "licence": "Free for personal use only; not for sale or reposting",
      "homepage": "https://gamebanana.com/mods/621588",
      "verified": false,
      "downloads": 883,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 621588,
        "file_pattern": "baby_twintails_bubblegum_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the Pigtail wig with a bigger, retouched twintails hairstyle with bangs, in pink. The wig cannot be recoloured in game, which is why each colour is its own download. The four colours replace the same file, so install only one."
    },
    {
      "id": "sf64-star-dog-modpack",
      "name": "Star Dog Modpack",
      "title": "Star Fox 64 3D",
      "title_ids": [
        "0004000000049000"
      ],
      "author": "CONINJA_2002",
      "author_url": "https://gamebanana.com/members/2101710",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/679753",
      "verified": false,
      "downloads": 32,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 679753,
        "file_pattern": "star_dog_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/mods/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "textures": {
        "rules": [
          {
            "from": "*/textures/{tid}/",
            "to": ""
          }
        ]
      },
      "notes": "Replaces some voice clips with new fan recordings (Molly, Goldie, Chica and Coninja so far) and also replaces some textures, the character portraits. The author says it works only on Azahar, and it is an early access release, so most characters are still to come. It is for the USA version only, as its folders are named for that title ID."
    },
    {
      "id": "sf64-final-speech-restoration",
      "name": "Final Speech Restoration",
      "title": "Star Fox 64 3D",
      "title_ids": [
        "0004000000049000",
        "0004000000049100"
      ],
      "author": "Redstormmars",
      "author_url": "https://gamebanana.com/members/4963393",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/673226",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 673226,
        "file_pattern": "final_speech_restoration.rar"
      },
      "install": {
        "rules": [
          {
            "from": "*/*/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces four of the game's speech lines with the uncompressed N64 recordings, which the author prefers for their intonation. It covers the USA and Europe versions. The author made it for a real 3DS, so it has not been confirmed on Azahar.",
      "downloads": 242,
      "updated": "2026-04"
    },
    {
      "id": "msr-no-bloom",
      "name": "No Bloom",
      "title": "Metroid: Samus Returns",
      "title_ids": [
        "00040000001BB200",
        "00040000001BFB00",
        "00040000001BFC00"
      ],
      "author": "sabre230",
      "author_url": "https://gamebanana.com/members/1816626",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/537215",
      "verified": false,
      "downloads": 549,
      "updated": "2024-08",
      "source": {
        "type": "gamebanana",
        "mod": 537215,
        "file_pattern": "nobloom_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "pp_mixer_ctr.bshdat",
            "to": "romfs/system/shd/pp_mixer_ctr.bshdat"
          }
        ]
      },
      "notes": "Hides the bloom glow by changing one post-processing shader, which suits a reshade or a cleaner look. The author says it has not been tested widely and may break things or not work on a real 3DS. The file sits loose in the download, so the install folder is taken from the older version of the same mod."
    },
    {
      "id": "sg-shadow-the-hedgehog-hover",
      "name": "Shadow the Hedgehog (Hover)",
      "title": "Sonic Generations",
      "title_ids": [
        "0004000000062300"
      ],
      "author": "DanielGamerDX",
      "author_url": "https://gamebanana.com/members/2580969",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/543786",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 543786,
        "file_pattern": "shadow_the_hedgehog_hover_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "textures": {
        "rules": [
          {
            "from": "*/textures/",
            "to": ""
          }
        ]
      },
      "notes": "Makes Shadow playable in all the Modern stages with hover running animations, adds a Modern Sonic boss fight in Radical Highway, and lets Super Shadow and Super Sonic team up against the final boss. It also replaces some textures and a large game archive (about 590 MB), and works with the English, French, German, Spanish and Italian voices. The two versions replace the same files, so install only one; the page names no region.",
      "downloads": 2846,
      "updated": "2024-10"
    },
    {
      "id": "sg-shadow-the-hedgehog-skate",
      "name": "Shadow the Hedgehog (Skate)",
      "title": "Sonic Generations",
      "title_ids": [
        "0004000000062300"
      ],
      "author": "DanielGamerDX",
      "author_url": "https://gamebanana.com/members/2580969",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/543786",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 543786,
        "file_pattern": "shadow_the_hedgehog_skate_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "textures": {
        "rules": [
          {
            "from": "*/textures/",
            "to": ""
          }
        ]
      },
      "notes": "Makes Shadow playable in all the Modern stages with skate running animations, adds a Modern Sonic boss fight in Radical Highway, and lets Super Shadow and Super Sonic team up against the final boss. It also replaces some textures and a large game archive (about 590 MB), and works with the English, French, German, Spanish and Italian voices. The two versions replace the same files, so install only one; the page names no region.",
      "downloads": 2846,
      "updated": "2024-10"
    },
    {
      "id": "sg-legacy-skin-pack",
      "name": "Legacy Skin Pack",
      "title": "Sonic Generations",
      "title_ids": [
        "0004000000062300"
      ],
      "author": "DanielGamerDX",
      "author_url": "https://gamebanana.com/members/2580969",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/562735",
      "verified": false,
      "downloads": 1214,
      "updated": "2025-08",
      "source": {
        "type": "gamebanana",
        "mod": 562735,
        "file_pattern": "legacy_skin_pack_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "textures": {
        "rules": [
          {
            "from": "*/textures/",
            "to": ""
          }
        ]
      },
      "notes": "Swaps in Sonic Jam Classic Sonic and Sonic Adventure Modern Sonic models, ported from the Sonic X Shadow Generations DLC, including in cutscenes. It also replaces some textures and a large game archive (about 590 MB). The author could not test it on a real 3DS, and the page names no region."
    },
    {
      "id": "sg-definitive-sonic-models",
      "name": "Definitive Sonic Models",
      "title": "Sonic Generations",
      "title_ids": [
        "0004000000062300"
      ],
      "author": "DanielGamerDX",
      "author_url": "https://gamebanana.com/members/2580969",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/555882",
      "verified": false,
      "downloads": 1205,
      "updated": "2024-12",
      "source": {
        "type": "gamebanana",
        "mod": 555882,
        "file_pattern": "definitive_sonic_models.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the low-poly gameplay models of Classic and Modern Sonic, and their Super forms, with the detailed cutscene models. This is the recommended version; an older \"high quality models\" version on the page is not installed. It is one 590 MB game archive, so it clashes with other Sonic Generations model mods."
    },
    {
      "id": "yw1-brazilian-portuguese",
      "name": "Brazilian Portuguese",
      "title": "YO-KAI WATCH",
      "title_ids": [
        "0004000000167700"
      ],
      "author": "Math_kk",
      "author_url": "https://gamebanana.com/members/2056987",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/425090",
      "verified": false,
      "source": {
        "type": "gdrive",
        "file_id": "1ebsopLJ-XQlu9dO9nuF37PzTE1PAsX3V"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Translates the story, interface and more into Brazilian Portuguese (the Traducoes Komainu patch, version 1.3.2). It works only with the USA version 1.0, so do not install the game update. The download is a 238 MB archive on the author's Google Drive.",
      "downloads": 3196,
      "updated": "2023-07",
      "size": 237598614
    },
    {
      "id": "yw1-yo-kaizo-watch",
      "name": "Yo-Kaizo Watch",
      "title": "YO-KAI WATCH",
      "title_ids": [
        "0004000000167800",
        "000400000017C200"
      ],
      "author": "Woganog",
      "author_url": "https://gamebanana.com/members/2259228",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/461653",
      "verified": false,
      "downloads": 1550,
      "updated": "2023-08",
      "source": {
        "type": "gamebanana",
        "mod": 461653,
        "file_pattern": "ykzw.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A hard mode with much stronger bosses, a weaker Holy Exporb, changed boss rewards and new textures, movies and sounds. It is for the Europe version and only works as intended with the game language set to English (GB). It replaces the main game archive (about 600 MB), and the author asks players not to use the Infinite Attack Exploit."
    },
    {
      "id": "yw1-undub",
      "name": "Undub",
      "title": "YO-KAI WATCH",
      "title_ids": [
        "0004000000167700",
        "0004000000167800",
        "000400000017C200"
      ],
      "author": "Ocleg",
      "author_url": "https://gamebanana.com/members/3000817",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/495701",
      "verified": false,
      "downloads": 1328,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 495701,
        "file_pattern": "yw1_undub_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Restores the Japanese voices and Yo-kai names, undubs the story text and textures, and brings back the uncensored NPC designs, the Japanese title background, the Terror Time icons and the original intro. Update the game to the latest version first. It is a 1 GB download that replaces the main game archive; the page names no region, and it ships both the English and the Europe sound banks."
    },
    {
      "id": "yw1-revisited",
      "name": "Revisited",
      "title": "YO-KAI WATCH",
      "title_ids": [
        "0004000000167800",
        "000400000017C200"
      ],
      "author": "Whisperito",
      "author_url": "https://gamebanana.com/members/2801205",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/505814",
      "verified": false,
      "source": {
        "type": "gdrive_folder",
        "folder_id": "1C_izKEJAirzJHBOH0l9699DyOVWKaWzz",
        "file_pattern": "yw1_a.fa"
      },
      "install": {
        "rules": [
          {
            "from": "yw1_a.fa",
            "to": "romfs/yw1_a.fa"
          }
        ]
      },
      "notes": "A quality-of-life pack: a Yo-kai Watch 2 style interface (not every menu), every Exporb for sale in Whatta Find, Pandanoko always in the Wayfarer Manor VIP room, Orcanos's fight marks and Pandanoko's dialogue artwork. The download is one loose game archive (573 MB) on the author's Google Drive, and the Europe version is inferred from its size because the page does not say.",
      "downloads": 920,
      "updated": "2024-04",
      "size": 572551388
    },
    {
      "id": "yw1-kirbendo-french-dub",
      "name": "French Dub by Kirbendo",
      "title": "YO-KAI WATCH",
      "title_ids": [
        "0004000000167800",
        "000400000017C200"
      ],
      "author": "Kirbendo",
      "author_url": "https://gamebanana.com/members/2374565",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/543417",
      "verified": false,
      "downloads": 787,
      "updated": "2024-09",
      "source": {
        "type": "gamebanana",
        "mod": 543417,
        "file_pattern": "romfs_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the cutscene voices with a French dub by the YouTuber Kirbendo, and each Yo-kai now says an extra line when you befriend it, voiced by community members. It also replaces some item and medal graphics. French only, for the Europe version, and the download is 387 MB."
    },
    {
      "id": "yw2-yo-kaizo-watch-2",
      "name": "Yo-Kaizo Watch 2",
      "title": "YO-KAI WATCH 2",
      "title_ids": [
        "000400000019AB00",
        "000400000019AC00",
        "000400000019B000",
        "000400000019B100"
      ],
      "author": "Woganog",
      "author_url": "https://gamebanana.com/members/2259228",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/422109",
      "verified": false,
      "downloads": 3298,
      "updated": "2023-02",
      "source": {
        "type": "gamebanana",
        "mod": 422109,
        "file_pattern": "v1_b1970.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A hard mode with buffed main bosses and other bosses, nerfed items, lower experience and a cheat word (Cheater) for the password items. This is the English (GB) build for the Europe versions of Bony Spirits and Fleshy Souls, and the French build is a separate download. The download is 896 MB and replaces the game's main archives."
    },
    {
      "id": "yw2-ultimate-boss-mod",
      "name": "Ultimate Boss Mod",
      "title": "YO-KAI WATCH 2",
      "title_ids": [
        "000400000019A900",
        "000400000019AA00"
      ],
      "author": "ClutchZ",
      "author_url": "https://gamebanana.com/members/3506225",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/529122",
      "verified": false,
      "downloads": 3079,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 529122,
        "file_pattern": "ultimate_boss_mod_version_21_final_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/*/yw2_a.fa",
            "to": "romfs/yw2_a.fa"
          },
          {
            "from": "*/*/yw2_lg_en.fa",
            "to": "romfs/yw2_lg_en.fa"
          }
        ]
      },
      "notes": "Replaces normal Yo-kai with bosses so that every boss can be befriended without a save editor, with the scaling balanced in the final version 2.1. It carries the English text archive, so the USA versions are listed; the page does not say whether it is for Bony Spirits or Fleshy Souls, so both are. The download is 1 GB."
    },
    {
      "id": "yw3-project-ultimate",
      "name": "Project Ultimate (Game Archive Version)",
      "title": "YO-KAI WATCH 3",
      "title_ids": [
        "00040000001D6700"
      ],
      "author": "mekkiNG",
      "author_url": "https://gamebanana.com/members/1957696",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/504143",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 504143,
        "file_pattern": "yw3udemo2faversion_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/yw_lg_en.fa",
            "to": "romfs/yw_lg_en.fa"
          },
          {
            "from": "romfs/snd/",
            "to": "romfs/snd/"
          }
        ]
      },
      "notes": "A fan mod (Demo 2) that adds many new Yo-kai, in its ready-to-use game archive version; the other download needs the YWML mod loader and is not used. The author archived it in August 2026, so no more updates are coming. It is a 313 MB download for the English (USA) version.",
      "downloads": 5643,
      "updated": "2026-03"
    },
    {
      "id": "yw3-undub",
      "name": "Undub",
      "title": "YO-KAI WATCH 3",
      "title_ids": [
        "00040000001D6700"
      ],
      "author": "Younes-Bel",
      "author_url": "https://gamebanana.com/members/2035267",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/455126",
      "verified": false,
      "downloads": 749,
      "updated": "2023-07",
      "source": {
        "type": "gamebanana",
        "mod": 455126,
        "file_pattern": "yo-kai_watch_3_undub.rar"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Changes all audio to Japanese, changes the BBQ text and art to USA and Blasters to Busters, and swaps Jibanyan B, Komasan B, Usapyon B and the Bustersmobile for the Japanese versions. This is the older GameBanana build: the page says to take version 1.1 from MediaFire, which the app cannot download. It only works on the base game, not the updated one, and is 389 MB."
    },
    {
      "id": "yw3-yo-kaizo-watch-3",
      "name": "Yo-Kaizo Watch 3",
      "title": "YO-KAI WATCH 3",
      "title_ids": [
        "00040000001D6700"
      ],
      "author": "charashift/opalfoxxo",
      "author_url": "https://gamebanana.com/members/2745524",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/498483",
      "verified": false,
      "source": {
        "type": "gdrive_folder",
        "folder_id": "1CPtgpI1IbxNLfIBuiH5G6NwFdBD86iAv",
        "file_pattern": "yw_a.fa"
      },
      "parts": [
        {
          "type": "gdrive_folder",
          "folder_id": "1CPtgpI1IbxNLfIBuiH5G6NwFdBD86iAv",
          "file_pattern": "yw_a.fa"
        },
        {
          "type": "gdrive_folder",
          "folder_id": "1CPtgpI1IbxNLfIBuiH5G6NwFdBD86iAv",
          "file_pattern": "yw_lg_en.fa"
        }
      ],
      "install": {
        "rules": [
          {
            "from": "yw_a.fa",
            "to": "romfs/yw_a.fa"
          },
          {
            "from": "yw_lg_en.fa",
            "to": "romfs/yw_lg_en.fa"
          }
        ]
      },
      "notes": "A Kaizo difficulty mod with boss stat changes, post-game rematches, Blasters T and Zombie Night, plus playable Zombie Yo-kai, Crabbycat, Yopplebot, Sternyan and Emperor mode, and some recoloured harder bosses. It is two loose game archives (about 2.1 GB together) on the author's Google Drive, for the English (USA) version. The whole download is very large.",
      "downloads": 689,
      "updated": "2024-05",
      "size": 2146915176
    },
    {
      "id": "yw3-katie-fumi-over-hailey-inaho",
      "name": "Katie and Fumi over Hailey and Inaho",
      "title": "YO-KAI WATCH 3",
      "title_ids": [
        "00040000001D6700"
      ],
      "author": "heartbreakhavoc",
      "author_url": "https://gamebanana.com/members/2115914",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/564113",
      "verified": false,
      "downloads": 648,
      "updated": "2024-12",
      "source": {
        "type": "gamebanana",
        "mod": 564113,
        "file_pattern": "katie_our_queen_updated_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "yw_a.fa",
            "to": "romfs/yw_a.fa"
          }
        ]
      },
      "notes": "Replaces every Hailey and Inaho model except the cutscene ones with Katie and Fumi from Yo-kai Watch 2, and swaps the BBQ house for the Katie and Fumi one. It replaces the main game archive and is a 1.5 GB download. The page does not name a region, so the USA version is listed."
    },
    {
      "id": "smt4-undub",
      "name": "Undub",
      "title": "Shin Megami Tensei IV",
      "title_ids": [
        "00040000000E5C00"
      ],
      "author": "Kesnos",
      "author_url": "https://gamebanana.com/members/1723576",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/579153",
      "verified": false,
      "downloads": 770,
      "updated": "2026-01",
      "source": {
        "type": "gamebanana",
        "mod": 579153,
        "file_pattern": "undub.7z"
      },
      "install": {
        "rules": [
          {
            "from": "*/{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Restores the Japanese audio and movies in the USA version, as a re-upload of an undub patch whose original download went down. It replaces the sound banks and movie files (about 540 MB), and the original thread has the instructions."
    },
    {
      "id": "smt4-early-design-flynn",
      "name": "Early Design Flynn",
      "title": "Shin Megami Tensei IV",
      "title_ids": [
        "00040000000E5C00"
      ],
      "author": "ZerosPlays7",
      "author_url": "https://gamebanana.com/members/2050103",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/344612",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 344612,
        "file_pattern": "early_design_flynn_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Turns the Samurai uniform and its menu portraits into the Early Design look from the official artwork. The author says it works on 3DS and Citra, and it is for the USA version. It replaces the player model and a few menu images.",
      "downloads": 186,
      "updated": "2021-12"
    },
    {
      "id": "smt4-main-menu-hd",
      "name": "Main Menu HD",
      "title": "Shin Megami Tensei IV",
      "title_ids": [
        "00040000000E5C00"
      ],
      "author": "Quantum_Dog",
      "author_url": "https://gamebanana.com/members/1847570",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/462950",
      "verified": false,
      "source": {
        "type": "gamebanana",
        "mod": 462950,
        "file_pattern": "smt_4_main_menu_hd.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the title menu graphics with HD versions. The author says it probably does not work on a real 3DS. A colour version is on the page but is not installed.",
      "downloads": 183,
      "updated": "2023-08"
    },
    {
      "id": "smt4a-nanashi-she-her",
      "name": "Nanashi She/Her Text and Dialogue",
      "title": "Shin Megami Tensei IV: Apocalypse",
      "title_ids": [
        "000400000019A200"
      ],
      "author": "Paisley_Park",
      "author_url": "https://gamebanana.com/members/1739807",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/454847",
      "verified": false,
      "downloads": 105,
      "updated": "2023-08",
      "source": {
        "type": "gamebanana",
        "mod": 454847,
        "file_pattern": "smt4afemprotagtext_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Changes the text and dialogue so Nanashi is referred to as she/her, with feminine wording elsewhere; the voice lines are not changed and there are no story changes. The author tested it on Citra, and it does not work with DLC events."
    },
    {
      "id": "smt4a-zeus",
      "name": "Zeus",
      "title": "Shin Megami Tensei IV: Apocalypse",
      "title_ids": [
        "000400000019A200"
      ],
      "author": "saad14",
      "author_url": "https://gamebanana.com/members/1998893",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/396271",
      "verified": false,
      "downloads": 92,
      "updated": "2022-08",
      "source": {
        "type": "gamebanana",
        "mod": 396271,
        "file_pattern": "zeusapocalpyse.rar"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces Thor with Zeus, including the model, portraits and profile text. The author tested it on Citra. It is a very small download."
    },
    {
      "id": "smt4a-better-intro",
      "name": "Better Intro",
      "title": "Shin Megami Tensei IV: Apocalypse",
      "title_ids": [
        "000400000019A200"
      ],
      "author": "Quantum_Dog",
      "author_url": "https://gamebanana.com/members/1847570",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/463419",
      "verified": false,
      "downloads": 92,
      "updated": "2023-08",
      "source": {
        "type": "gamebanana",
        "mod": 463419,
        "file_pattern": "smt_4a_better_intro.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the intro movie with the game's trailer, which the author finds a better introduction. It swaps one 18 MB video file. The page does not say whether it works on a real 3DS."
    },
    {
      "id": "smt4a-hd-nanashi-hunter-suit",
      "name": "HD Nanashi Hunter Suit",
      "title": "Shin Megami Tensei IV: Apocalypse",
      "title_ids": [
        "000400000019A200"
      ],
      "author": "Quantum_Dog",
      "author_url": "https://gamebanana.com/members/1847570",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/462655",
      "verified": false,
      "downloads": 85,
      "updated": "2023-08",
      "source": {
        "type": "gamebanana",
        "mod": 462655,
        "file_pattern": "hd_nanashi_hunter_suit.zip"
      },
      "install": {
        "rules": [
          {
            "from": "*/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces the body, head and legs models of Nanashi's old hunter suit with higher-detail ones. The author says it probably does not work on a real 3DS and that only this one outfit is covered so far."
    },
    {
      "id": "kh3d-riku-aqua",
      "name": "Riku to Aqua",
      "title": "Kingdom Hearts 3D: Dream Drop Distance",
      "title_ids": [
        "000400000008D300",
        "0004000000095500"
      ],
      "author": "Cagaroo",
      "author_url": "https://gamebanana.com/members/5286717",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/700639",
      "verified": false,
      "downloads": 312,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 700639,
        "file_pattern": "romfs_0a013.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces Riku with Aqua in most places, with retargeted Birth by Sleep animations. This installs the Aqua-only game files; her extra Keyblades and the 625 MB voice lines are separate downloads and are not installed. It replaces the same file as Sora (KH2) and the KH1 outfits, so use only one character mod."
    },
    {
      "id": "kh3d-sora-kh2",
      "name": "Sora (KH2)",
      "title": "Kingdom Hearts 3D: Dream Drop Distance",
      "title_ids": [
        "000400000008D300",
        "0004000000095500"
      ],
      "author": "Cagaroo",
      "author_url": "https://gamebanana.com/members/5286717",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/700640",
      "verified": false,
      "downloads": 275,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 700640,
        "file_pattern": "romfs_23e4f.zip"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Replaces Sora with his Kingdom Hearts II look, with retargeted KH2 animations. This installs the Sora-only game files; the page also offers a combined Sora and Aqua pack that is not used. It replaces the same file as Riku to Aqua and the KH1 outfits, so use only one character mod."
    },
    {
      "id": "kh3d-kh2-inspired-textures-recolor",
      "name": "KH2-Inspired Textures (Recolor)",
      "title": "Kingdom Hearts 3D: Dream Drop Distance",
      "title_ids": [
        "000400000008D300",
        "0004000000095500"
      ],
      "author": "Solt11",
      "author_url": "https://gamebanana.com/members/2609834",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/515459",
      "verified": false,
      "downloads": 264,
      "updated": "2025-03",
      "source": {
        "type": "gamebanana",
        "mod": 515459,
        "file_pattern": "ddd-kh2-tex-*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "1 - Recolor/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Changes Sora's and Riku's looks toward their Kingdom Hearts II appearances; this version mostly changes the colour scheme and keeps the original details. The files are a replacement character model archive rather than texture images, so it clashes with the other character mods. The three versions replace the same file, so install only one; the page names no region."
    },
    {
      "id": "kh3d-kh2-inspired-textures-adapt",
      "name": "KH2-Inspired Textures (KH2 Adapt)",
      "title": "Kingdom Hearts 3D: Dream Drop Distance",
      "title_ids": [
        "000400000008D300",
        "0004000000095500"
      ],
      "author": "Solt11",
      "author_url": "https://gamebanana.com/members/2609834",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/515459",
      "verified": false,
      "downloads": 264,
      "updated": "2025-03",
      "source": {
        "type": "gamebanana",
        "mod": 515459,
        "file_pattern": "ddd-kh2-tex-*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "2 - KH2 Adapt/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Changes Sora's and Riku's looks toward their Kingdom Hearts II appearances; this version is very close to Kingdom Hearts II but keeps the X on Sora's shirt and the Dream Eater symbol on Riku's. The files are a replacement character model archive rather than texture images, so it clashes with the other character mods. The three versions replace the same file, so install only one; the page names no region."
    },
    {
      "id": "kh3d-kh2-inspired-textures-full",
      "name": "KH2-Inspired Textures (KH2 Full)",
      "title": "Kingdom Hearts 3D: Dream Drop Distance",
      "title_ids": [
        "000400000008D300",
        "0004000000095500"
      ],
      "author": "Solt11",
      "author_url": "https://gamebanana.com/members/2609834",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/515459",
      "verified": false,
      "downloads": 264,
      "updated": "2025-03",
      "source": {
        "type": "gamebanana",
        "mod": 515459,
        "file_pattern": "ddd-kh2-tex-*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "3 - KH2 Full/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Changes Sora's and Riku's looks toward their Kingdom Hearts II appearances; this version is very close to Kingdom Hearts II throughout. The files are a replacement character model archive rather than texture images, so it clashes with the other character mods. The three versions replace the same file, so install only one; the page names no region."
    },
    {
      "id": "kh3d-kh1-hud-outfits",
      "name": "KH1 Outfits HUD Update",
      "title": "Kingdom Hearts 3D: Dream Drop Distance",
      "title_ids": [
        "000400000008D300",
        "0004000000095500"
      ],
      "author": "Sven Cakemann",
      "author_url": "https://gamebanana.com/members/1323070",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/487120",
      "verified": false,
      "downloads": 209,
      "updated": "2024-01",
      "source": {
        "type": "gamebanana",
        "mod": 487120,
        "file_pattern": "romfs_ddd_kh1hud.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Updates the HUD icons to the Kingdom Hearts 1 outfit versions and gives the models blinking faces, building on Isaac_MG's KH1 outfits mod. It has its own copy of the outfit models, so it replaces Play as KH1 Sora and Riku rather than adding to it. Some icon pixels are missing in the corners and the HP bar may clip."
    },
    {
      "id": "kh3d-play-as-kh1-sora-riku",
      "name": "Play as KH1 Sora and Riku",
      "title": "Kingdom Hearts 3D: Dream Drop Distance",
      "title_ids": [
        "000400000008D300",
        "0004000000095500"
      ],
      "author": "Isaac_MG",
      "author_url": "https://gamebanana.com/members/2364136",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/454656",
      "verified": false,
      "downloads": 207,
      "updated": "2023-07",
      "source": {
        "type": "gamebanana",
        "mod": 454656,
        "file_pattern": "khddd_playaskh1.zip"
      },
      "install": {
        "rules": [
          {
            "from": "chara_pc.rbin",
            "to": "romfs/chara_pc.rbin"
          }
        ]
      },
      "notes": "Changes Sora's and Riku's fighting appearance to their Kingdom Hearts 1 models by replacing one 19 MB character file. It replaces the same file as the other character mods, so use only one of them. The page names no region, so the USA and Europe versions are listed."
    },
    {
      "id": "dq7-advanced-vocations-keep-skills",
      "name": "Advanced Vocations Keep Spells and Skills",
      "title": "Dragon Quest VII: Fragments of the Forgotten Past",
      "title_ids": [
        "000400000018EF00"
      ],
      "author": "The Count of Real Numbers",
      "author_url": "https://gamebanana.com/members/3432580",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/638979",
      "verified": false,
      "downloads": 69,
      "updated": "2025-12",
      "source": {
        "type": "gamebanana",
        "mod": 638979,
        "file_pattern": "exefs_*.7z"
      },
      "install": {
        "rules": [
          {
            "from": "exefs/code.bin",
            "to": "exefs/code.bin"
          }
        ]
      },
      "notes": "Stops characters forgetting the spells and skills they learned from advanced human vocations when they change vocation. It replaces the game's code, so it clashes with any other code patch, and the page names no region, so the USA version is listed."
    },
    {
      "id": "dq7-max-tame-rate",
      "name": "Max Tame Rate",
      "title": "Dragon Quest VII: Fragments of the Forgotten Past",
      "title_ids": [
        "000400000018EF00"
      ],
      "author": "The Count of Real Numbers",
      "author_url": "https://gamebanana.com/members/3432580",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/642707",
      "verified": false,
      "downloads": 24,
      "updated": "2025-12",
      "source": {
        "type": "gamebanana",
        "mod": 642707,
        "file_pattern": "high_tame_rate.7z"
      },
      "install": {
        "rules": [
          {
            "from": "romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "Gives every tameable monster a 25% tame rate by replacing the monster parameter file. It could clash with other mods that edit monster data, and the page names no region, so the USA version is listed."
    },
    {
      "id": "layton-miracle-mask-turkish",
      "name": "Turkish Patch",
      "title": "Professor Layton and the Miracle Mask",
      "title_ids": [
        "00040000000A8600"
      ],
      "author": "Reqrefusion",
      "author_url": "https://gamebanana.com/members/5134648",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/701527",
      "verified": false,
      "downloads": 38,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 701527,
        "file_pattern": "professor_layton_and_the_miracle_mask_t_rk_e_yama_*.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/romfs/",
            "to": "romfs/"
          },
          {
            "from": "exefs/code.bin",
            "to": "exefs/code.bin"
          },
          {
            "from": "exefs/icon.bin",
            "to": "exefs/icon.bin"
          }
        ]
      },
      "notes": "A Turkish translation for the European (UK) version, by Reqrefusion. It replaces the game's code and icon as well as the text archives, so it clashes with other code patches. The download also has a banner and logo that Azahar does not use, and these are skipped."
    },
    {
      "id": "phoenix-wright-trilogy-turkish",
      "name": "Turkish Patch",
      "title": "Phoenix Wright: Ace Attorney Trilogy",
      "title_ids": [
        "0004000000133300"
      ],
      "author": "Reqrefusion",
      "author_url": "https://gamebanana.com/members/5134648",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/705559",
      "verified": false,
      "downloads": 47,
      "updated": "2026-11",
      "source": {
        "type": "gamebanana",
        "mod": 705559,
        "file_pattern": "ace_attorney_trilogy_turkce_yama.zip"
      },
      "install": {
        "rules": [
          {
            "from": "{tid}/code.bin",
            "to": "exefs/code.bin"
          },
          {
            "from": "{tid}/romfs/",
            "to": "romfs/"
          }
        ]
      },
      "notes": "A Turkish translation for the European 3DS release, ported from the Switch patch by Reqrefusion. It replaces the game's code, so it clashes with other code patches, and its 132 MB data file makes the download about 113 MB. It is for the Europe version only."
    }
  ]
}
)AZCAT";
}

} // namespace Mods
