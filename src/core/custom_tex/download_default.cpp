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
      "notes": "Several variants (fonts, Link styles); each shows up as its own pack. Partial pack.",
      "updated": "2021-05-06",
      "verified": false
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
      "notes": "Work in progress. Based on Pokemon Y Resurrection.",
      "updated": "2025-02-28",
      "verified": false
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
      "notes": "Textures from the Citra forum archive.",
      "updated": "2025-02-28",
      "verified": false
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
      "notes": "Textures only; the pack's ReShade files are not used. Version 5.0a. Size not confirmed (Google Drive was over its download limit when checked).",
      "updated": "2026-09-04",
      "verified": true
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
      "notes": "Textures only; the pack's ReShade files are not used. Version 5.0a, for 1080p screens.",
      "updated": "2026-09-04",
      "verified": true
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
      "notes": "Textures only; the pack's ReShade files are not used. Version 3.0b. USA and Europe only.",
      "updated": "2026-01-05",
      "verified": true
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
      "notes": "Textures only; the pack's ReShade files are not used. Version 3.0b, for 1080p screens. USA and Europe only.",
      "updated": "2026-01-05",
      "verified": true
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
      "notes": "Textures only; the pack's ReShade files are not used. Version 1.2.0b.",
      "updated": "2025-03-10",
      "verified": true
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
      "notes": "Textures only; the pack's ReShade files are not used. Version 1.2.0b, for 1080p screens.",
      "updated": "2025-03-10",
      "verified": true
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
      "notes": "Textures only; the pack's ReShade files are not used. Version 2.0b. USA only (the pack is made for the USA game).",
      "updated": "2026-01-05",
      "verified": true
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
      "notes": "Textures only; the pack's ReShade files are not used. Version 2.0b, for 1080p screens. USA only.",
      "updated": "2026-01-05",
      "verified": true
    },
    {
      "id": "oot3d-sse-controller-ui-xbox",
      "name": "SSE Controller UI (Xbox)",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500"
      ],
      "author": "chubbycubby",
      "author_url": "https://gamebanana.com/members/5093056",
      "source": {
        "type": "gamebanana",
        "mod": 701929,
        "file_pattern": "oot3d_xbox_ui_hud_*.zip"
      },
      "size": 452906387,
      "format": "zip",
      "layout": "single",
      "folder": "SSE Controller UI Xbox",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/701929",
      "notes": "Rearranges the button prompts of the Single Screen Experience into the Xbox controller layout, as 4K textures for the USA version. It needs Single Screen Experience and Henriko's Ocarina of Time 3D 4K texture pack, and the buttons must be remapped as the mod page describes. The download is about 450 MB, and the pack's one message file (sys8.qbf) is not installed.",
      "updated": "2026-10-21",
      "verified": false
    },
    {
      "id": "oot3d-sse-controller-ui-switch",
      "name": "SSE Controller UI (Switch)",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500"
      ],
      "author": "chubbycubby",
      "author_url": "https://gamebanana.com/members/5093056",
      "source": {
        "type": "gamebanana",
        "mod": 701929,
        "file_pattern": "oot3d_switch_ui_hud_*.zip"
      },
      "size": 451632616,
      "format": "zip",
      "layout": "single",
      "folder": "SSE Controller UI Switch",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/701929",
      "notes": "Rearranges the button prompts of the Single Screen Experience into the Switch controller layout, as 4K textures for the USA version. It needs Single Screen Experience and Henriko's Ocarina of Time 3D 4K texture pack, and the buttons must be remapped as the mod page describes. The download is about 450 MB, and the pack's one message file (sys8.qbf) is not installed.",
      "updated": "2026-10-21",
      "verified": false
    },
    {
      "id": "oot3d-sse-controller-ui-vanilla",
      "name": "SSE Controller UI (Vanilla)",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500"
      ],
      "author": "chubbycubby",
      "author_url": "https://gamebanana.com/members/5093056",
      "source": {
        "type": "gamebanana",
        "mod": 701929,
        "file_pattern": "oot3d_vanilla_ui_hud_*.zip"
      },
      "size": 451655008,
      "format": "zip",
      "layout": "single",
      "folder": "SSE Controller UI Vanilla",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/701929",
      "notes": "Rearranges the button prompts of the Single Screen Experience into the Vanilla controller layout, as 4K textures for the USA version. It needs Single Screen Experience and Henriko's Ocarina of Time 3D 4K texture pack, and the buttons must be remapped as the mod page describes. The download is about 450 MB, and the pack's one message file (sys8.qbf) is not installed.",
      "updated": "2026-10-21",
      "verified": false
    },
    {
      "id": "oot3d-link-skin-pack",
      "name": "Link Skin Pack",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500"
      ],
      "author": "Art-Does-Not-Compute",
      "author_url": "https://gamebanana.com/members/1716642",
      "source": {
        "type": "gamebanana",
        "mod": 250777,
        "file_pattern": "link_texture_pack_*.rar"
      },
      "size": 25592398,
      "format": "rar",
      "layout": "variants",
      "folder": "Link Skin Pack",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250777",
      "notes": "Several looks for Link as textures for the USA version, each shown as its own pack. Base is the main set and the others replace part of it, and the Legacy textures pack mixes several older styles. It was made for Citra and has not been tested here.",
      "updated": "2020-06-17",
      "verified": false
    },
    {
      "id": "oot3d-touchless-layout-vanilla",
      "name": "Touchless Layout UI",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "HardDanX",
      "author_url": "https://gamebanana.com/members/1877520",
      "source": {
        "type": "gamebanana",
        "mod": 567711,
        "file_pattern": "touchless_layout_ui_vanilla_*.rar"
      },
      "size": 69845512,
      "format": "rar",
      "layout": "variants",
      "folder": "Touchless Layout UI",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/567711",
      "notes": "Redraws the button prompts for Xbox, DualShock, Nintendo or SNES-colour controllers so the touch screen is not needed, for the original UI. Each layout is its own pack, and the one named Unedited textures is the stock reference set, not a layout. It needs the controller remapping and the sensor map described on the mod page.",
      "updated": "2026-07-24",
      "verified": false
    },
    {
      "id": "oot3d-touchless-layout-sse",
      "name": "Touchless Layout UI (Single Screen Experience)",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600"
      ],
      "author": "HardDanX",
      "author_url": "https://gamebanana.com/members/1877520",
      "source": {
        "type": "gamebanana",
        "mod": 567711,
        "file_pattern": "touchless_layout_ui_sse_*.rar"
      },
      "size": 29348467,
      "format": "rar",
      "layout": "variants",
      "folder": "Touchless Layout UI SSE",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/567711",
      "notes": "The same controller layouts for Single Screen Experience, which must be installed first, for the USA and Europe versions. Each layout is its own pack, and the one named Unedited textures is the stock reference set. The author calls this version severely outdated.",
      "updated": "2026-07-24",
      "verified": false
    },
    {
      "id": "oot3d-1998-npc-overhaul",
      "name": "1998 NPC Overhaul",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600"
      ],
      "author": "HugAMuffin",
      "author_url": "https://gamebanana.com/members/1608504",
      "source": {
        "type": "gamebanana",
        "mod": 643864,
        "file_pattern": "1998_-_npc_overhaul_*.rar"
      },
      "size": 13308819,
      "format": "rar",
      "layout": "single",
      "folder": "1998 NPC Overhaul",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/643864",
      "notes": "Redraws the textures of the main NPCs to look closer to the 1998 official art, sorted into folders by character. The page does not name a region and the textures are not in a region folder, so Japan is left out. The author recommends it together with Better Remaster Experience.",
      "updated": "2026-07-24",
      "verified": false
    },
    {
      "id": "mm3d-expressive-face-textures",
      "name": "Expressive Face Textures for Link",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500"
      ],
      "author": "LochNessHamster",
      "author_url": "https://gamebanana.com/members/1082848",
      "source": {
        "type": "gamebanana",
        "mod": 250766,
        "file_pattern": "mm3d_-_expressive_face_textures_v*.zip"
      },
      "size": 3255161,
      "format": "zip",
      "layout": "single",
      "folder": "Expressive Face Textures",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250766",
      "notes": "High-resolution face textures that make Link more expressive, for the USA version. The tunic recolour in the screenshots is part of the Majora's Mask 3D HD project and is not included.",
      "updated": "2021-07-04",
      "verified": false
    },
    {
      "id": "mm3d-travelers-clothes",
      "name": "Travelers Clothes",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500"
      ],
      "author": "Broglerd",
      "author_url": "https://gamebanana.com/members/2015087",
      "source": {
        "type": "gamebanana",
        "mod": 329196,
        "file_pattern": "travelers_clothes_*.rar"
      },
      "size": 3218895,
      "format": "rar",
      "layout": "single",
      "folder": "Travelers Clothes",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/329196",
      "notes": "Gives Link a traveller's outfit as textures for the USA version, built on the Expressive Face Textures for Link mod. Other texture packs for Link's face and clothes cannot be used with it at the same time.",
      "updated": "2026-05-01",
      "verified": false
    },
    {
      "id": "mm3d-project-restoration-ui",
      "name": "Project Restoration UI",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "HugAMuffin",
      "author_url": "https://gamebanana.com/members/1608504",
      "source": {
        "type": "gamebanana",
        "mod": 34052,
        "file_pattern": "project_restoration_ui.rar"
      },
      "size": 633378,
      "format": "rar",
      "layout": "single",
      "folder": "Project Restoration UI",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/34052",
      "notes": "Adds prompts for the controls that Project Restoration introduces, such as swim speed, arrow swapping and the song screen, with an Xbox One layout. It needs Project Restoration, was made for Citra, and the new prompts are small on a shrunk touch screen.",
      "updated": "2019-09-13",
      "verified": false
    },
    {
      "id": "mm3d-omni-ui-project-restoration",
      "name": "Omni UI for Project Restoration",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "HardDanX",
      "author_url": "https://gamebanana.com/members/1877520",
      "source": {
        "type": "gamebanana",
        "mod": 569317,
        "file_pattern": "omni_ui_for_project_restoration_*.rar"
      },
      "size": 26451594,
      "format": "rar",
      "layout": "variants",
      "folder": "Omni UI for Project Restoration",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/569317",
      "notes": "Controller prompts for Project Restoration in Xbox, DualShock, Nintendo or SNES-colour styles, for the USA and Europe versions. It needs Project Restoration and the controller remapping in the ReadMe, and each style is its own pack. Ignore the Unedited textures pack, and the N64 Patch pack only holds changed files for the Xbox or DualShock style.",
      "updated": "2026-04-18",
      "verified": false
    },
    {
      "id": "mm3d-non-intrusive-bottom-screen-hud",
      "name": "Non-Intrusive Bottom Screen HUD",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "vladone97",
      "author_url": "https://gamebanana.com/members/4293709",
      "source": {
        "type": "gamebanana",
        "mod": 688429,
        "file_pattern": "non-intrusive_bottom_screen_hud-*.zip"
      },
      "size": 68774,
      "format": "zip",
      "layout": "single",
      "folder": "Non-Intrusive Bottom Screen HUD",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/688429",
      "notes": "Removes most of the bottom screen's graphics so both screens can be laid over one display, with the lower one set to about 20% transparency. It was made for the Europe version, and the author has not tested the others. Non-paused maps are not removed.",
      "updated": "2026-06-23",
      "verified": false
    },
    {
      "id": "dq8-hd-4x-ui-only",
      "name": "DQ8 HD Texture Pack (4x UI)",
      "title": "Dragon Quest VIII: Journey of the Cursed King",
      "title_ids": [
        "000400000018F100",
        "000400000018F200"
      ],
      "author": "Magic Banana",
      "author_url": "https://gamebanana.com/members/2971173",
      "source": {
        "type": "gamebanana",
        "mod": 489556,
        "file_pattern": "dq8-4xhd_ui-only_*.zip"
      },
      "size": 186675699,
      "format": "zip",
      "layout": "single",
      "folder": "DQ8 HD 4x UI",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/489556",
      "notes": "The UI, item and equipment icons, character portraits and map icons of the 4x HD pack, which is more than the name says. The full pack is hosted by the author on Google Drive and is not included here. It was made on the Europe version and works on the USA one, and preloading it adds under 1 GB of RAM.",
      "updated": "2024-01-18",
      "verified": false
    },
    {
      "id": "dq8-hd-4x-restoration",
      "name": "DQ8 HD Texture Pack (4x Restoration)",
      "title": "Dragon Quest VIII: Journey of the Cursed King",
      "title_ids": [
        "000400000018F100",
        "000400000018F200"
      ],
      "author": "Magic Banana",
      "author_url": "https://gamebanana.com/members/2971173",
      "source": {
        "type": "gamebanana",
        "mod": 489556,
        "file_pattern": "dq8-4xhd_restoration_*.zip"
      },
      "size": 14191795,
      "format": "zip",
      "layout": "single",
      "folder": "DQ8 HD 4x Restoration",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/489556",
      "notes": "4x textures for the changes made by the Restoration Mod (party characters and armor), which do not need that mod installed. It is an add-on to the full HD pack, which the author hosts on Google Drive, so on its own it changes little. It was made on the Europe version and works on the USA one.",
      "updated": "2024-07-17",
      "verified": false
    },
    {
      "id": "dq8-hd-2x-restoration",
      "name": "DQ8 HD Texture Pack (2x Restoration)",
      "title": "Dragon Quest VIII: Journey of the Cursed King",
      "title_ids": [
        "000400000018F100",
        "000400000018F200"
      ],
      "author": "Magic Banana",
      "author_url": "https://gamebanana.com/members/2971173",
      "source": {
        "type": "gamebanana",
        "mod": 489556,
        "file_pattern": "dq8-2xhd_restoration*.zip"
      },
      "size": 5520628,
      "format": "zip",
      "layout": "single",
      "folder": "DQ8 HD 2x Restoration",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/489556",
      "notes": "2x textures for the changes made by the Restoration Mod (party characters and armor), which do not need that mod installed. It is an add-on to the full 2x HD pack, which the author hosts on Google Drive, so on its own it changes little. It was made on the Europe version and works on the USA one.",
      "updated": "2024-01-11",
      "verified": false
    },
    {
      "id": "sm3dl-hd-ui-pack",
      "name": "SM3DL HD UI Pack",
      "title": "Super Mario 3D Land",
      "title_ids": [
        "0004000000054000",
        "0004000000053F00"
      ],
      "author": "GalacticMoblin",
      "author_url": "https://gamebanana.com/members/2212885",
      "source": {
        "type": "gamebanana",
        "mod": 394597,
        "file_pattern": "super_mario_3d_land_hd_ui_pack_*.zip"
      },
      "size": 9401476,
      "format": "zip",
      "layout": "single",
      "folder": "SM3DL HD UI",
      "licence": "CC BY-NC-ND 4.0",
      "notes": "Hand-redrawn HD versions of the game's UI art, made for Citra and Azahar. It is a partial pack that replaces only the UI textures; turn on Custom Textures and Preload Custom Textures.",
      "updated": "2026-11-11",
      "verified": false
    }
  ]
})JSON";
}

} // namespace CustomTex::Download
