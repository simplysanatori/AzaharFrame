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
      "source": {
        "type": "gamebanana",
        "mod": 732496,
        "file_pattern": "continuous_player_music.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For game update 1.1."
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
      "source": {
        "type": "gamebanana",
        "mod": 732496,
        "file_pattern": "continuous_player_music.zip"
      },
      "version_chip": {
        "label": "1.1",
        "tip": "For game update 1.1."
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
    }
  ]
})AZCAT";
}

} // namespace Mods
