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
      "downloads": 13829,
      "updated": "2021-05",
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
      "downloads": 7347,
      "updated": "2025-02",
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
      "downloads": 2768,
      "updated": "2025-02",
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
      "verified": true,
      "recommended": true
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
      "verified": true,
      "recommended": true
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
      "downloads": 14303,
      "updated": "2026-10",
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
      "downloads": 14303,
      "updated": "2026-10",
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
      "downloads": 14303,
      "updated": "2026-10",
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
      "downloads": 6172,
      "updated": "2020-06",
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
      "downloads": 4069,
      "updated": "2026-07",
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
      "downloads": 4069,
      "updated": "2026-07",
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
      "downloads": 2500,
      "updated": "2026-07",
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
      "downloads": 3688,
      "updated": "2021-07",
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
      "downloads": 2198,
      "updated": "2026-05",
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
      "downloads": 1970,
      "updated": "2020-02",
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
      "downloads": 1843,
      "updated": "2026-04",
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
      "downloads": 280,
      "updated": "2026-06",
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
      "verified": false
    },
    {
      "id": "dq8-hd-4x-full",
      "name": "DQ8 HD Texture Pack (4x Full)",
      "title": "Dragon Quest VIII: Journey of the Cursed King",
      "title_ids": [
        "000400000018F100",
        "000400000018F200"
      ],
      "author": "Magic Banana",
      "author_url": "https://gamebanana.com/members/2971173",
      "downloads": 11655,
      "updated": "2024-01",
      "source": {
        "type": "gdrive",
        "file_id": "1iEubG2V-k030pa9GJ0pWWBZiccHtsMJK"
      },
      "size": 6870700825,
      "format": "zip",
      "layout": "single",
      "folder": "DQ8 HD 4x Full",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/489556",
      "notes": "The full 4x pack, version 1.1.0, with every texture and the UI, hosted by the author on Google Drive. It is large, about 6.4 GB, and preloading it takes around 20 GB of RAM, so on the Frame it streams in instead of preloading. It was made on the Europe version and works on the USA one.",
      "verified": true
    },
    {
      "id": "dq8-hd-2x-full",
      "name": "DQ8 HD Texture Pack (2x Full)",
      "title": "Dragon Quest VIII: Journey of the Cursed King",
      "title_ids": [
        "000400000018F100",
        "000400000018F200"
      ],
      "author": "Magic Banana",
      "author_url": "https://gamebanana.com/members/2971173",
      "downloads": 11655,
      "updated": "2024-01",
      "source": {
        "type": "gdrive",
        "file_id": "1jKdyFb-pNoN_di6MB4X8GBVowpEyv9A7"
      },
      "size": 1944626274,
      "format": "zip",
      "layout": "single",
      "folder": "DQ8 HD 2x Full",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/489556",
      "notes": "The full 2x pack, version 1.1.0, with every texture and the UI, hosted by the author on Google Drive. About 1.8 GB, and preloading it takes around 5 GB of RAM. The 4x UI pack on top keeps the menus sharp. It was made on the Europe version and works on the USA one.",
      "verified": true
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
      "downloads": 11655,
      "updated": "2024-01",
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
      "downloads": 11655,
      "updated": "2024-01",
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
      "downloads": 11655,
      "updated": "2024-01",
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
      "downloads": 4811,
      "updated": "2026-11",
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
      "verified": false
    },
    {
      "id": "pokemon-xy-5x-redrawn-ui",
      "name": "5X Redrawn UI HD",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "blivveries",
      "author_url": "https://gamebanana.com/members/1731414",
      "downloads": 25439,
      "updated": "2025-04",
      "source": {
        "type": "gamebanana",
        "mod": 578206,
        "file_pattern": "pokemon_xy_5x_gui_v25.zip"
      },
      "size": 60265058,
      "format": "zip",
      "layout": "single",
      "folder": "XY 5X Redrawn UI",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/578206",
      "notes": "Hand-redrawn and 5x-upscaled HUD, menus, fonts, type icons and Pokemon sprites for the USA, Europe and Japan versions, as custom textures only. The author tested it with Vulkan at 5x resolution and says any Texture Filter option must be turned off or the game can crash.",
      "verified": false
    },
    {
      "id": "pokemon-xy-modern-sprites",
      "name": "Modern Pokemon Sprites",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "stingflyer",
      "author_url": "https://gamebanana.com/members/2457337",
      "downloads": 1638,
      "updated": "2026-06",
      "source": {
        "type": "gamebanana",
        "mod": 627411,
        "file_pattern": "pokemon_sprites_*.zip"
      },
      "size": 13047906,
      "format": "zip",
      "layout": "single",
      "folder": "XY Modern Sprites",
      "layer_on": "pokemon-xy-5x-redrawn-ui",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/627411",
      "notes": "Replaces every Pokemon sprite, including forms and Megas, with modern artwork from the later games. The author tested it on Azahar only and warns that texture filters must be off.",
      "verified": false
    },
    {
      "id": "pokemon-xy-clean-pokedex",
      "name": "Clean Pokedex Background",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "stingflyer",
      "author_url": "https://gamebanana.com/members/2457337",
      "downloads": 1638,
      "updated": "2026-06",
      "source": {
        "type": "gamebanana",
        "mod": 627411,
        "file_pattern": "clean_pokedex.zip"
      },
      "size": 499,
      "format": "zip",
      "layout": "single",
      "folder": "XY Clean Pokedex",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/627411",
      "notes": "A bonus file from the Modern Pokemon Sprites mod: a transparent texture that removes the dotted background of the Pokedex. It is one texture, so it is the same on every region.",
      "verified": false
    },
    {
      "id": "pokemon-oras-emerald-brendan",
      "name": "Emerald Brendan (Textures)",
      "title": "Pokemon Omega Ruby / Pokemon Alpha Sapphire",
      "title_ids": [
        "000400000011C400",
        "000400000011C500"
      ],
      "author": "Remontafb",
      "author_url": "https://gamebanana.com/members/2374192",
      "downloads": 4034,
      "updated": "2022-12",
      "source": {
        "type": "gamebanana",
        "mod": 415289,
        "file_pattern": "000400000011c500.zip"
      },
      "size": 129363,
      "format": "zip",
      "layout": "single",
      "folder": "Emerald Brendan",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/415289",
      "notes": "Reskins Brendan with his Emerald look as custom textures; the original design is by Dionn Runk and credited on the page. Remove any Brendan model mod first or the model shows instead. The file is named for Alpha Sapphire, and Omega Ruby is untested.",
      "verified": false
    },
    {
      "id": "pokemon-oras-modern-sprites",
      "name": "Modern Pokemon Sprites",
      "title": "Pokemon Omega Ruby / Pokemon Alpha Sapphire",
      "title_ids": [
        "000400000011C400",
        "000400000011C500"
      ],
      "author": "stingflyer",
      "author_url": "https://gamebanana.com/members/2457337",
      "downloads": 1063,
      "updated": "2026-06",
      "source": {
        "type": "gamebanana",
        "mod": 629004,
        "file_pattern": "pokemon_sprites_*.zip"
      },
      "size": 13442863,
      "format": "zip",
      "layout": "single",
      "folder": "ORAS Modern Sprites",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/629004",
      "notes": "Replaces the sprites of every Pokemon, including forms and Megas, with modern artwork at 8x resolution; Cosplay Pikachu is not included. The author tested it on Azahar only and has not tried a console.",
      "verified": false
    },
    {
      "id": "pokemon-oras-5x-redrawn-ui",
      "name": "5X Redrawn UI (Incomplete Port)",
      "title": "Pokemon Omega Ruby / Pokemon Alpha Sapphire",
      "title_ids": [
        "000400000011C400",
        "000400000011C500"
      ],
      "author": "stingflyer",
      "author_url": "https://gamebanana.com/members/2457337",
      "downloads": 958,
      "updated": "2025-11",
      "source": {
        "type": "gamebanana",
        "mod": 634437,
        "file_pattern": "5x_hud_v*.zip"
      },
      "size": 3792375,
      "format": "zip",
      "layout": "single",
      "folder": "ORAS 5X Redrawn UI",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/634437",
      "notes": "An unfinished port of the 5X Redrawn UI textures from X and Y to Omega Ruby and Alpha Sapphire, at 8x resolution. Only the parts that carried over work, so some of the HUD stays at the original resolution. The author tested it on Azahar only.",
      "verified": false
    },
    {
      "id": "pokemon-oras-ghibli-style",
      "name": "Ghibli Style Textures",
      "title": "Pokemon Omega Ruby / Pokemon Alpha Sapphire",
      "title_ids": [
        "000400000011C400",
        "000400000011C500"
      ],
      "author": "Pandaa shiverian",
      "author_url": "https://gamebanana.com/members/2279174",
      "downloads": 72,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 726809,
        "file_pattern": "poketexture.zip"
      },
      "size": 68957902,
      "format": "zip",
      "layout": "single",
      "folder": "ORAS Ghibli Style",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/726809",
      "notes": "Gives the game a Studio Ghibli look inspired by Howl's Moving Castle; the first three towns are nearly done and it is still in progress. It is made for the European version and all languages except Japanese and Korean, and the author cannot promise it works on the USA version. The notes are in Spanish.",
      "verified": false
    },
    {
      "id": "bravely-default-hd-microturing",
      "name": "Bravely Default HD Texture Pack",
      "title": "Bravely Default",
      "title_ids": [
        "00040000000FC500",
        "00040000000FC600"
      ],
      "author": "microturing",
      "author_url": "https://gamebanana.com/members/3611666",
      "downloads": 4080,
      "updated": "2024-07",
      "source": {
        "type": "gamebanana",
        "mod": 530500,
        "file_pattern": "bravelydefault.rar"
      },
      "size": 1055719005,
      "format": "rar",
      "layout": "single",
      "folder": "Bravely Default HD",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/530500",
      "notes": "A full HD texture pack by 1vierock, reuploaded by microturing as one large RAR, made for Citra-style custom textures. The download is about 1 GB, and the system bsdtar cannot open the file (unsupported RAR dictionary size), so its layout could not be checked.",
      "verified": false
    },
    {
      "id": "bravely-default-hd-font-icons",
      "name": "HD Font and Icons",
      "title": "Bravely Default",
      "title_ids": [
        "00040000000FC500",
        "00040000000FC600"
      ],
      "author": "Zominion",
      "author_url": "https://gamebanana.com/members/4458515",
      "downloads": 958,
      "updated": "2025-05",
      "source": {
        "type": "gamebanana",
        "mod": 592686,
        "file_pattern": "bravely_default_-_ohdfasi_*.zip"
      },
      "size": 4035538,
      "format": "zip",
      "layout": "single",
      "folder": "Bravely Default OHDFaSI",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/592686",
      "notes": "Redrawn font, numbers, job icons and simple icons for Bravely Default, hand-drawn rather than upscaled. The author calls it an unfinished release, and it has older patch folders inside that overlap the newer files.",
      "verified": false
    },
    {
      "id": "bravely-second-hd-font-icons",
      "name": "HD Font and Icons",
      "title": "Bravely Second: End Layer",
      "title_ids": [
        "000400000017BA00",
        "000400000017BB00"
      ],
      "author": "Zominion",
      "author_url": "https://gamebanana.com/members/4458515",
      "downloads": 958,
      "updated": "2025-05",
      "source": {
        "type": "gamebanana",
        "mod": 592686,
        "file_pattern": "bravely_second_-_ohdfasi_*.zip"
      },
      "size": 3736330,
      "format": "zip",
      "layout": "single",
      "folder": "Bravely Second OHDFaSI",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/592686",
      "notes": "Redrawn font, numbers and simple icons for Bravely Second, adapted from the Bravely Default set. The author could not fully test it and calls it an unfinished release.",
      "verified": false
    },
    {
      "id": "persona-q2-hd-skill-icons",
      "name": "HD Skill Icons",
      "title": "Persona Q2: New Cinema Labyrinth",
      "title_ids": [
        "00040000001D7100"
      ],
      "author": "Century_",
      "author_url": "https://gamebanana.com/members/1764202",
      "downloads": 1157,
      "updated": "2023-08",
      "source": {
        "type": "gamebanana",
        "mod": 371330,
        "file_pattern": "pq2_hd_skill_icons.7z"
      },
      "size": 340760,
      "format": "7z",
      "layout": "single",
      "folder": "PQ2 HD Skill Icons",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/371330",
      "notes": "Redraws every skill icon at 4x resolution as one custom texture for the USA version. It replaces the same texture as the P5 Skill Icons pack, so use only one of them.",
      "verified": false
    },
    {
      "id": "persona-q2-p5-skill-icons",
      "name": "P5 Skill Icons (HD)",
      "title": "Persona Q2: New Cinema Labyrinth",
      "title_ids": [
        "00040000001D7100"
      ],
      "author": "rabbitgirlkris",
      "author_url": "https://gamebanana.com/members/2012756",
      "downloads": 626,
      "updated": "2022-04",
      "source": {
        "type": "gamebanana",
        "mod": 336082,
        "file_pattern": "tex1_1024x512_*.zip"
      },
      "size": 406363,
      "format": "zip",
      "layout": "single",
      "folder": "PQ2 P5 Skill Icons",
      "licence": "CC BY 4.0",
      "homepage": "https://gamebanana.com/mods/336082",
      "notes": "Redraws the battle skill icons in the Persona 5 style as a single HD texture, for use with the HD icon pack. It replaces the same texture as the HD Skill Icons pack. The page lists no region, so the USA folder is assumed; the base-resolution version needs a PC tool and is not included.",
      "verified": false
    },
    {
      "id": "kirby-triple-deluxe-hd-shadowthenhedhog",
      "name": "Kirby Triple Deluxe HD",
      "title": "Kirby: Triple Deluxe",
      "title_ids": [
        "000400000010BF00"
      ],
      "author": "ShadowThenHedHog",
      "author_url": "https://gamebanana.com/members/4762727",
      "downloads": 1331,
      "updated": "2025-10",
      "source": {
        "type": "gamebanana",
        "mod": 626409,
        "file_pattern": "kirby_td_in_hd_*.zip"
      },
      "size": 807700246,
      "format": "zip",
      "layout": "single",
      "folder": "Kirby Triple Deluxe HD",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/626409",
      "notes": "An HD texture pack that upscales most of the in-game textures, including maps, stickers and the UI, for the USA version. The download is about 808 MB. It is texture-only.",
      "verified": false
    },
    {
      "id": "luigis-mansion-dark-moon-marios-mansion",
      "name": "Mario's Mansion",
      "title": "Luigi's Mansion: Dark Moon",
      "title_ids": [
        "0004000000055F00",
        "0004000000076500",
        "0004000000076400"
      ],
      "author": "Zelda mystery",
      "author_url": "https://gamebanana.com/members/3014647",
      "downloads": 274,
      "updated": "2024-02",
      "source": {
        "type": "gamebanana",
        "mod": 495362,
        "file_pattern": "lm2_mario_v1*.7z"
      },
      "size": 3498516,
      "format": "7z",
      "layout": "single",
      "folder": "Marios Mansion",
      "licence": "Not stated",
      "homepage": "https://gamebanana.com/mods/495362",
      "notes": "Lets you play as Mario in Dark Moon by replacing Luigi's textures; it is texture-only, with just a few files. The page does not say which regions it was made for, so other versions are untested.",
      "verified": false
    },
    {
      "id": "luigis-mansion-dark-moon-marios-mansion-hd",
      "name": "Mario's Mansion HD",
      "title": "Luigi's Mansion: Dark Moon",
      "title_ids": [
        "0004000000055F00",
        "0004000000076500",
        "0004000000076400"
      ],
      "author": "Zelda mystery",
      "author_url": "https://gamebanana.com/members/3014647",
      "downloads": 274,
      "updated": "2024-02",
      "source": {
        "type": "gamebanana",
        "mod": 495362,
        "file_pattern": "mario_manison_2_hd-_*.7z"
      },
      "size": 222637705,
      "format": "7z",
      "layout": "single",
      "folder": "Marios Mansion HD",
      "licence": "Not stated",
      "homepage": "https://gamebanana.com/mods/495362",
      "notes": "A higher-resolution version of the Mario's Mansion textures, about 1,750 files, which also replaces many other Dark Moon textures. The download is about 223 MB. The page does not say which regions it was made for, so other versions are untested.",
      "verified": false
    },
    {
      "id": "luigis-mansion-dark-moon-playable-gooigi",
      "name": "Playable Gooigi",
      "title": "Luigi's Mansion: Dark Moon",
      "title_ids": [
        "0004000000055F00",
        "0004000000076500",
        "0004000000076400"
      ],
      "author": "MugginOven",
      "author_url": "https://gamebanana.com/members/5530670",
      "downloads": 37,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 680977,
        "file_pattern": "gooigi_*.zip"
      },
      "size": 30995,
      "format": "zip",
      "layout": "single",
      "folder": "Playable Gooigi",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/680977",
      "notes": "Retextures the player so you play as Gooigi, as 48 custom textures only. The page shows only a short readme and no region, so other versions than the one the author used are untested.",
      "verified": false
    },
    {
      "id": "mario-luigi-paper-jam-4x-texture-pack",
      "name": "4x Texture Pack",
      "title": "Mario & Luigi: Paper Jam",
      "title_ids": [
        "0004000000132700"
      ],
      "author": "zillaHD",
      "author_url": "https://gamebanana.com/members/5517231",
      "downloads": 191,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 679668,
        "file_pattern": "0004000000132700.7z"
      },
      "size": 1408356537,
      "format": "7z",
      "layout": "single",
      "folder": "Paper Jam 4x",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/679668",
      "notes": "A 4x upscale of the game textures by zillaHD, mixing hand-redrawn UI and text with AI upscaling, which turns the character pixel art into smoother 3D-looking art. The download is about 1.4 GB, and GameBanana has not finished scanning it. It is made for the USA version.",
      "verified": false
    },
    {
      "id": "mario-luigi-paper-jam-4x-pixelart-plus",
      "name": "4x PixelArt+ Texture Pack",
      "title": "Mario & Luigi: Paper Jam",
      "title_ids": [
        "0004000000132700"
      ],
      "author": "zillaHD",
      "author_url": "https://gamebanana.com/members/5517231",
      "downloads": 129,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 680601,
        "file_pattern": "0004000000132700-nn.7z"
      },
      "size": 1013271482,
      "format": "7z",
      "layout": "single",
      "folder": "Paper Jam 4x PixelArt+",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/680601",
      "notes": "The alternate version of the 4x texture pack that keeps and sharpens the character pixel art instead of converting it to 3D. The download is about 1 GB. It is made for the USA version, and it replaces the same textures as the other 4x pack.",
      "verified": false
    },
    {
      "id": "mpt100-wario-long-sleeves",
      "name": "Wario Long Sleeves",
      "title": "Mario Party: The Top 100",
      "title_ids": [
        "00040000001C4E00"
      ],
      "author": "Metal-Mario-Fan",
      "author_url": "https://gamebanana.com/members/1692790",
      "downloads": 66,
      "updated": "2021-10",
      "source": {
        "type": "gamebanana",
        "mod": 327798,
        "file_pattern": "00040000001c4e00_*.zip"
      },
      "size": 2129421,
      "format": "zip",
      "layout": "single",
      "folder": "Wario Long Sleeves",
      "licence": "CC BY-NC-ND 4.0",
      "notes": "Retextures Wario's arms as long sleeves, like his classic look, with 58 replacement textures. It is made for the North American version, and the folder inside is named for that game's title ID. Turn on Custom Textures and Preload Custom Textures.",
      "homepage": "https://gamebanana.com/mods/327798",
      "verified": false
    },
    {
      "id": "rh-hd-texture-pack",
      "name": "Radiant Historia HD",
      "title": "Radiant Historia: Perfect Chronology",
      "title_ids": [
        "00040000001C8F00"
      ],
      "author": "Serbagz",
      "author_url": "https://gamebanana.com/members/2108748",
      "downloads": 2222,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 373806,
        "file_pattern": "radiant_historia_hd_texture_pack_*.rar"
      },
      "size": 726954508,
      "format": "rar",
      "layout": "single",
      "folder": "Radiant Historia HD",
      "licence": "CC BY-NC-ND 4.0",
      "notes": "A 4x upscale of the UI, fonts, CGs, character art and world and battle textures, with the official HD CGs used where they exist. The chibi art is still unfinished. It is a 727 MB RAR for the North American version, and its bonus-content archives are not installed. Turn on Custom Textures and Preload Custom Textures.",
      "homepage": "https://gamebanana.com/mods/373806",
      "verified": false
    },
    {
      "id": "toa-genei-lategothic-v2",
      "name": "TOA GenEi LateGo v2 (Font and UI)",
      "title": "Tales of the Abyss 3D",
      "title_ids": [
        "0004000000061300"
      ],
      "author": "rikimtasu",
      "author_url": "https://github.com/rikimtasu",
      "source": {
        "type": "https",
        "url": "https://raw.githubusercontent.com/rikimtasu/toa-3ds-fontcustomtex/main/TOA%20GenEi%20LateGo%20v2.zip"
      },
      "size": 25088672,
      "format": "zip",
      "layout": "single",
      "folder": "TOA GenEi LateGo",
      "licence": "Not stated; fan-made, credit the author",
      "notes": "Replaces the game's font and UI textures with a sharper GenEi LateGo Japanese-Latin font. It is made for the North American version only. The zip is committed to the author's repository rather than published as a release, so it is downloaded from there.",
      "updated": "2026-10-23",
      "homepage": "https://github.com/rikimtasu/toa-3ds-fontcustomtex",
      "verified": false
    },
    {
      "id": "eon-hd-portraits-v3",
      "name": "Etrian Odyssey Nexus HD Portraits (v3)",
      "title": "Etrian Odyssey Nexus",
      "title_ids": [
        "00040000001D4E00",
        "00040000001D5200"
      ],
      "author": "rikimtasu",
      "author_url": "https://gamebanana.com/members/1323038",
      "downloads": 2945,
      "updated": "2022-04",
      "source": {
        "type": "gamebanana",
        "mod": 40630,
        "file_pattern": "hd_portaitsver3.7z"
      },
      "size": 754270273,
      "format": "7z",
      "layout": "single",
      "folder": "EO Nexus HD Portraits",
      "licence": "Not stated",
      "notes": "AI-upscaled 4x character portraits, covering most player and a few NPC textures, in the lighter AnimeSharp version. It was made for the European game and the author says it also works on the North American one. The page is titled Etrian Odyssey Nexus, not Untold; it is 754 MB, and the HD Texts pack is a separate entry.",
      "homepage": "https://gamebanana.com/mods/40630",
      "verified": false
    },
    {
      "id": "eon-hd-texts",
      "name": "Etrian Odyssey Nexus HD Texts",
      "title": "Etrian Odyssey Nexus",
      "title_ids": [
        "00040000001D4E00",
        "00040000001D5200"
      ],
      "author": "rikimtasu",
      "author_url": "https://gamebanana.com/members/1323038",
      "downloads": 2945,
      "updated": "2022-04",
      "source": {
        "type": "gamebanana",
        "mod": 40630,
        "file_pattern": "hd_texts.7z"
      },
      "size": 2816623,
      "format": "7z",
      "layout": "single",
      "folder": "EO Nexus HD Texts",
      "licence": "Not stated",
      "notes": "AI-upscaled text and font textures, a small companion to the HD Portraits pack. It was made for the European game and the author says it also works on the North American one. The page is titled Etrian Odyssey Nexus, not Untold.",
      "homepage": "https://gamebanana.com/mods/40630",
      "verified": false
    },
    {
      "id": "eon-hd-v2-furizu",
      "name": "Etrian Odyssey Nexus HD (v2)",
      "title": "Etrian Odyssey Nexus",
      "title_ids": [
        "00040000001D4E00"
      ],
      "author": "Furizu",
      "author_url": "https://gamebanana.com/members/4659102",
      "downloads": 1587,
      "updated": "2025-07",
      "source": {
        "type": "gamebanana",
        "mod": 608253,
        "file_pattern": "nexushd_v2part1.rar"
      },
      "size": 3125525227,
      "format": "rar",
      "layout": "single",
      "folder": "EO Nexus HD v2",
      "licence": "CC BY-NC-ND 4.0",
      "notes": "A 4x upscale of all the game's textures, with the PlayStation button layout, covering everything up to the 13th labyrinth. It is a four-part RAR of about 3.1 GB that must be fetched in order. Leave Preload Custom Textures off, as the author warns it will load everything and take very long.",
      "homepage": "https://gamebanana.com/mods/608253",
      "verified": false,
      "parts": [
        {
          "type": "gamebanana",
          "mod": 608253,
          "file_pattern": "nexushd_v2part1.rar"
        },
        {
          "type": "gamebanana",
          "mod": 608253,
          "file_pattern": "nexushd_v2part2.rar"
        },
        {
          "type": "gamebanana",
          "mod": 608253,
          "file_pattern": "nexushd_v2part3.rar"
        },
        {
          "type": "gamebanana",
          "mod": 608253,
          "file_pattern": "nexushd_v2part4.rar"
        }
      ]
    },
    {
      "id": "pkmn-sm-usum-shadow-lugia",
      "name": "Shadow Lugia",
      "title": "Pokémon Sun / Moon / Ultra Sun / Ultra Moon",
      "title_ids": [
        "0004000000164800",
        "0004000000175E00",
        "00040000001B5000",
        "00040000001B5100"
      ],
      "author": "Butter_Lettuce",
      "author_url": "https://gamebanana.com/members/2023514",
      "downloads": 598,
      "updated": "2021-12",
      "source": {
        "type": "gamebanana",
        "mod": 346447,
        "file_pattern": "shadowlugia3ds.zip"
      },
      "size": 553409,
      "format": "zip",
      "layout": "single",
      "folder": "Shadow Lugia",
      "licence": "CC BY-NC-ND 4.0",
      "notes": "Recolours Lugia as Shadow Lugia with six replacement textures. The zip also holds a model file for the game's data archive, which is not installed because it needs a manual tool. It was made for the Generation 7 games.",
      "homepage": "https://gamebanana.com/mods/346447",
      "verified": false
    },
    {
      "id": "pkmn-usum-modern-sprites",
      "name": "Modern Pokémon Sprites",
      "title": "Pokémon Ultra Sun / Pokémon Ultra Moon",
      "title_ids": [
        "00040000001B5000",
        "00040000001B5100"
      ],
      "author": "stingflyer",
      "author_url": "https://gamebanana.com/members/2457337",
      "downloads": 1820,
      "updated": "2026-06",
      "source": {
        "type": "gamebanana",
        "mod": 629003,
        "file_pattern": "pokemon_sprites_*.zip"
      },
      "size": 16088355,
      "format": "zip",
      "layout": "single",
      "folder": "USUM Modern Sprites",
      "licence": "CC BY-NC-ND 4.0",
      "notes": "Replaces every Pokémon sprite, including variants and Megas, with modern art at 8x resolution, about 1,000 textures. It does not cover Poké Pelago. Turn off texture filtering, which the author warns can break the sprites; it is tested on Azahar only.",
      "homepage": "https://gamebanana.com/mods/629003",
      "verified": false
    },
    {
      "id": "pkmn-usum-hd-types",
      "name": "Faithful HD Pokémon Types",
      "title": "Pokémon Ultra Sun / Pokémon Ultra Moon",
      "title_ids": [
        "00040000001B5000",
        "00040000001B5100"
      ],
      "author": "MetalTxus",
      "author_url": "https://gamebanana.com/members/2566769",
      "downloads": 1355,
      "updated": "2025-11",
      "source": {
        "type": "gamebanana",
        "mod": 635130,
        "file_pattern": "pokemon_types_*.zip"
      },
      "size": 247685,
      "format": "zip",
      "layout": "single",
      "folder": "USUM HD Types",
      "licence": "CC BY-NC 4.0",
      "notes": "One replacement texture that redraws the type icons at 5x size on a 5-pixel grid, so they scale cleanly. It changes nothing else.",
      "homepage": "https://gamebanana.com/mods/635130",
      "verified": false
    },
    {
      "id": "pkmn-usum-hd-fonts-ui",
      "name": "Faithful HD Fonts and UI (WIP)",
      "title": "Pokémon Ultra Sun / Pokémon Ultra Moon",
      "title_ids": [
        "00040000001B5000",
        "00040000001B5100"
      ],
      "author": "Motorblade",
      "author_url": "https://gamebanana.com/members/1775132",
      "downloads": 1102,
      "updated": "2026-07",
      "source": {
        "type": "gamebanana",
        "mod": 691013,
        "file_pattern": "usum_hd_textures_*.zip"
      },
      "size": 1311131,
      "format": "zip",
      "layout": "single",
      "folder": "USUM HD Fonts and UI",
      "licence": "CC BY-NC-ND 4.0",
      "notes": "A work in progress that upscales the fonts and some UI by 4x, covering English text, most Japanese characters, the cursor, most Poké Ball and Z-Crystal icons and some battle UI. It is made by hand without AI, and much is still unfinished.",
      "homepage": "https://gamebanana.com/mods/691013",
      "verified": false
    },
    {
      "id": "acnl-4k-trueshade",
      "name": "ACNL 4K TrueShade",
      "title": "Animal Crossing: New Leaf",
      "title_ids": [
        "0004000000086300",
        "0004000000086400"
      ],
      "author": "ThisStombi",
      "author_url": "https://gamebanana.com/members/3183375",
      "downloads": 8985,
      "updated": "2025-02",
      "source": {
        "type": "gamebanana",
        "mod": 494239,
        "file_pattern": "acnl_4k_texture_pack_*.zip"
      },
      "size": 350172405,
      "format": "zip",
      "layout": "single",
      "folder": "ACNL 4K TrueShade",
      "licence": "CC BY-ND 4.0",
      "notes": "A 4K texture pack that gives the game more realistic lighting while keeping its cozy style. It needs an internal resolution of at least 8x, and about 20 villagers are not upscaled. The author recommends game version 1.0 to 1.3 rather than 1.4.",
      "homepage": "https://gamebanana.com/mods/494239",
      "verified": false
    },
    {
      "id": "xb3d-dunban-scar-removed",
      "name": "Dunban's Scar Removed",
      "title": "Xenoblade Chronicles 3D",
      "title_ids": [
        "000400000F700100",
        "000400000F700200"
      ],
      "author": "wexiyeb618",
      "author_url": "https://gamebanana.com/members/2641156",
      "downloads": 20,
      "updated": "2025-12",
      "source": {
        "type": "gamebanana",
        "mod": 467914,
        "file_pattern": "dunban_s_scar_removed.zip"
      },
      "size": 28001,
      "format": "zip",
      "layout": "single",
      "folder": "Dunban Scar Removed",
      "licence": "CC0 1.0",
      "homepage": "https://gamebanana.com/mods/467914",
      "notes": "Removes the scar from Dunban's face on every armour set that shows it, with five replacement textures. It is a tiny pack. The page names no region.",
      "verified": false
    },
    {
      "id": "xb3d-hd-title-screen",
      "name": "HD Title Screen",
      "title": "Xenoblade Chronicles 3D",
      "title_ids": [
        "000400000F700100"
      ],
      "author": "Miguel92398",
      "author_url": "https://gamebanana.com/members/1544766",
      "downloads": 155,
      "updated": "2020-05",
      "source": {
        "type": "gamebanana",
        "mod": 9572,
        "file_pattern": "xbc3dhd_title.zip"
      },
      "size": 4523432,
      "format": "zip",
      "layout": "single",
      "folder": "Xenoblade HD Title Screen",
      "licence": "Free to use with credit and a link to the page",
      "homepage": "https://gamebanana.com/mods/9572",
      "notes": "Replaces the title screen artwork with a high-definition version taken from the XBCHD texture pack, using 23 textures. It was made for the USA version, and the author says it was meant for Citra.",
      "verified": false
    },
    {
      "id": "msr-hd-samus",
      "name": "HD Samus",
      "title": "Metroid: Samus Returns",
      "title_ids": [
        "00040000001BB200",
        "00040000001BFB00",
        "00040000001BFC00"
      ],
      "author": "HugAMuffin",
      "author_url": "https://gamebanana.com/members/1608504",
      "downloads": 1669,
      "updated": "2026-04",
      "source": {
        "type": "gamebanana",
        "mod": 428781,
        "file_pattern": "hd_samus_*.rar"
      },
      "size": 8131754,
      "format": "rar",
      "layout": "single",
      "folder": "HD Samus",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/428781",
      "notes": "Upscaled textures for Samus's Power, Varia and Gravity suits and the arm cannon, with extra hand-drawn detail that an upscaler would miss. It was made for Citra with the USA title ID. Its Gravity Suit textures clash with the separate HD Gravity Suit pack.",
      "verified": false
    },
    {
      "id": "msr-hd-gravity-suit",
      "name": "HD Gravity Suit",
      "title": "Metroid: Samus Returns",
      "title_ids": [
        "00040000001BB200",
        "00040000001BFB00",
        "00040000001BFC00"
      ],
      "author": "sabre230",
      "author_url": "https://gamebanana.com/members/1816626",
      "downloads": 171,
      "updated": "2025-02",
      "source": {
        "type": "gamebanana",
        "mod": 573526,
        "file_pattern": "hd-gravity-suit_*.zip"
      },
      "size": 10284254,
      "format": "zip",
      "layout": "single",
      "folder": "HD Gravity Suit",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/573526",
      "notes": "Hand-drawn textures for the Gravity Suit, based on the original concept art. It changes only the suit itself, not the arm cannon or morph ball. The textures sit in the USA title ID folder.",
      "verified": false
    },
    {
      "id": "sg-definitive-character-textures",
      "name": "Definitive Character Textures",
      "title": "Sonic Generations",
      "title_ids": [
        "0004000000062300"
      ],
      "author": "Sonic Beyond",
      "author_url": "https://gamebanana.com/members/2045892",
      "downloads": 915,
      "updated": "2023-12",
      "source": {
        "type": "gamebanana",
        "mod": 478937,
        "file_pattern": "definitive_character_textures_*.rar"
      },
      "size": 263212,
      "format": "rar",
      "layout": "single",
      "folder": "Definitive Character Textures",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/478937",
      "notes": "Retextures Classic Sonic, Modern Sonic and Tails to match their HD Generations looks, with seven textures. It is a small pack and the page names no region.",
      "verified": false
    },
    {
      "id": "sg-hd-textures",
      "name": "HD Textures",
      "title": "Sonic Generations",
      "title_ids": [
        "0004000000062300"
      ],
      "author": "pancharubin",
      "author_url": "https://gamebanana.com/members/2163528",
      "downloads": 813,
      "updated": "2025-09",
      "source": {
        "type": "gamebanana",
        "mod": 619599,
        "file_pattern": "sonic_generations_hd_textures.zip"
      },
      "size": 836165399,
      "format": "zip",
      "layout": "single",
      "folder": "Sonic Generations HD",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/619599",
      "notes": "Upscales the levels, characters and artwork, 1,744 textures in all. The download is large (836 MB), and the page names no region.",
      "verified": false
    },
    {
      "id": "yw2-psychic-specters-upscaled-hd",
      "name": "Upscaled HD",
      "title": "YO-KAI WATCH 2: Psychic Specters",
      "title_ids": [
        "00040000001B2700"
      ],
      "author": "UH06",
      "author_url": "https://gamebanana.com/members/1959118",
      "source": {
        "type": "gamebanana",
        "mod": 420384,
        "file_pattern": "psychicspecters_*.7z"
      },
      "size": 646001577,
      "format": "7z",
      "layout": "single",
      "folder": "Yo-kai Watch 2 Upscaled HD",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/420384",
      "notes": "Waifu2x-upscaled textures for Psychic Specters, the version 2 pack with 2,789 textures for the UI and more areas. It may not look right at 3x native resolution or lower, and it does not work with linear texture filtering. The download is 646 MB.",
      "downloads": 3751,
      "updated": "2023-01",
      "verified": false
    },
    {
      "id": "yw3-redefined",
      "name": "Redefined",
      "title": "YO-KAI WATCH 3",
      "title_ids": [
        "00040000001D6C00",
        "00040000001D6700"
      ],
      "author": "hotcapa",
      "author_url": "https://gamebanana.com/members/4122419",
      "downloads": 992,
      "updated": "2025-05",
      "source": {
        "type": "gamebanana",
        "mod": 597836,
        "file_pattern": "00040000001d6c00.zip"
      },
      "size": 366538681,
      "format": "zip",
      "layout": "single",
      "folder": "Yo-kai Watch 3 Redefined",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/597836",
      "notes": "Retouches the backgrounds, menus and some scenery textures to be cleaner and more detailed while keeping the original style (99 large textures). The author says it is a beta and works only on the Spanish version of the game; the folder is named for title ID 1D6C00, which is not in the usual lists, so the USA ID is listed as well but not confirmed. The page is in Spanish and the download is 367 MB.",
      "verified": false
    },
    {
      "id": "bse-end-layer-hd-texture-pack",
      "name": "End Layer HD Texture Pack",
      "title": "Bravely Second: End Layer",
      "title_ids": [
        "000400000017BA00"
      ],
      "author": "microturing",
      "author_url": "https://gamebanana.com/members/3611666",
      "downloads": 5996,
      "updated": "2024-07",
      "source": {
        "type": "gamebanana",
        "mod": 530531,
        "file_pattern": "bravelydefaultendpart1_*.rar"
      },
      "parts": [
        {
          "type": "gamebanana",
          "mod": 530531,
          "file_pattern": "bravelydefaultendpart1_*.rar"
        },
        {
          "type": "gamebanana",
          "mod": 530531,
          "file_pattern": "bravelydefaultendpart2_*.rar"
        },
        {
          "type": "gamebanana",
          "mod": 530531,
          "file_pattern": "bravelydefaultendpart3_*.rar"
        },
        {
          "type": "gamebanana",
          "mod": 530531,
          "file_pattern": "bravelydefaultendpart4_*.rar"
        },
        {
          "type": "gamebanana",
          "mod": 530531,
          "file_pattern": "bravelydefaultendpart5_*.rar"
        }
      ],
      "size": 4899288397,
      "format": "rar",
      "layout": "single",
      "folder": "End Layer HD",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/530531",
      "notes": "HD textures for characters, monsters, skills, the interface and many areas, about 2,800 textures. The author says it is unfinished and may need switching off in some areas to avoid crashes. The download is five 1 GB parts (4.7 GB in all) and the page warns about extraction errors; the folder is named for the USA title ID.",
      "verified": false
    },
    {
      "id": "smt4-hd",
      "name": "HD",
      "title": "Shin Megami Tensei IV",
      "title_ids": [
        "00040000000E5C00"
      ],
      "author": "AspenVesper",
      "author_url": "https://gamebanana.com/members/4618311",
      "downloads": 3588,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 651730,
        "file_pattern": "smt_iv_hd_*.zip"
      },
      "size": 680668727,
      "format": "zip",
      "layout": "single",
      "folder": "SMT IV HD",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/651730",
      "notes": "A hand-redrawn HD overhaul of 1,583 textures: fonts, UI, character and demon portraits and backgrounds, with some AI upscaling and full DLC support. Cutscenes and 3D textures are not covered. The PlayStation button add-ons on the page are not installed, and the download is 681 MB.",
      "verified": false
    },
    {
      "id": "smt4-persona-5-x-demon-portraits",
      "name": "Persona 5 X Demon Portraits",
      "title": "Shin Megami Tensei IV",
      "title_ids": [
        "00040000000E5C00"
      ],
      "author": "Pascali",
      "author_url": "https://gamebanana.com/members/3721128",
      "downloads": 448,
      "updated": "2024-08",
      "source": {
        "type": "gamebanana",
        "mod": 537150,
        "file_pattern": "p5x_demons_*.zip"
      },
      "size": 205688323,
      "format": "zip",
      "layout": "single",
      "folder": "P5X Demon Portraits",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/537150",
      "notes": "Swaps 113 demon portraits (223 counting bosses and conversations) for the ones from Persona 5: The Phantom X. It can be combined with an HD texture pack if you let this one replace the portrait files. The download is 206 MB.",
      "verified": false
    },
    {
      "id": "smt4a-dark-goth-nanashi-portrait",
      "name": "Dark Goth Nanashi Portrait",
      "title": "Shin Megami Tensei IV: Apocalypse",
      "title_ids": [
        "000400000019A200"
      ],
      "author": "PointBlankVT",
      "author_url": "https://gamebanana.com/members/2066424",
      "downloads": 46,
      "updated": "2026-06",
      "source": {
        "type": "gamebanana",
        "mod": 686074,
        "file_pattern": "dark_goth_nanashi.zip"
      },
      "size": 40165,
      "format": "zip",
      "layout": "single",
      "folder": "Dark Goth Nanashi Portrait",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/686074",
      "notes": "Draws the Dark Goth outfit onto Nanashi's portrait and small icon, at the default resolution. The HD version on the page is not installed. It goes well with the She/Her text mod.",
      "verified": false
    },
    {
      "id": "mgs3-hd-edition",
      "name": "HD Edition",
      "title": "Metal Gear Solid: Snake Eater 3D",
      "title_ids": [
        "0004000000081E00",
        "0004000000082400"
      ],
      "author": "HugAMuffin",
      "author_url": "https://gamebanana.com/members/1608504",
      "source": {
        "type": "gamebanana",
        "mod": 416034,
        "file_pattern": "hd_overhaul.rar"
      },
      "size": 904036689,
      "format": "rar",
      "layout": "single",
      "folder": "MGS3 HD Edition",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/416034",
      "notes": "A hand-made HD overhaul of every area, with over 4,000 textures based on the HD Collection and some careful upscaling. It includes the author's HD NPCs, equipment, camo, animals and Snake mods. The download is 904 MB, and the author recommends Citra Canary or later.",
      "downloads": 3174,
      "updated": "2022-12",
      "verified": false
    },
    {
      "id": "mgs3-hd-edition-virtuous-mission",
      "name": "HD Edition (Jimlafleur)",
      "title": "Metal Gear Solid: Snake Eater 3D",
      "title_ids": [
        "0004000000081E00",
        "0004000000082400"
      ],
      "author": "Jimlafleur89",
      "author_url": "https://gamebanana.com/members/1724241",
      "source": {
        "type": "gamebanana",
        "mod": 150247,
        "file_pattern": "mgs3dpart1.rar"
      },
      "parts": [
        {
          "type": "gamebanana",
          "mod": 150247,
          "file_pattern": "mgs3dpart1.rar"
        },
        {
          "type": "gamebanana",
          "mod": 150247,
          "file_pattern": "mgs3dpart2.zip"
        }
      ],
      "size": 569464978,
      "format": "auto",
      "layout": "single",
      "folder": "MGS3 HD Edition Jimlafleur",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/150247",
      "notes": "An incremental HD texture pack, currently the Virtuous Mission, with 162 textures made with 4K screens in mind. The author says more will follow, and the folder is named for the Europe title ID. It comes as two archives (a rar and a zip, about 570 MB together) that both go into the same texture folder.",
      "downloads": 616,
      "updated": "2020-04",
      "verified": false
    },
    {
      "id": "mgs3-show-some-skin",
      "name": "Show Some Skin",
      "title": "Metal Gear Solid: Snake Eater 3D",
      "title_ids": [
        "0004000000081E00",
        "0004000000082400"
      ],
      "author": "HugAMuffin",
      "author_url": "https://gamebanana.com/members/1608504",
      "downloads": 348,
      "updated": "2022-01",
      "source": {
        "type": "gamebanana",
        "mod": 250788,
        "file_pattern": "show_some_skin_edb31.rar"
      },
      "size": 5301204,
      "format": "rar",
      "layout": "single",
      "folder": "Show Some Skin",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250788",
      "notes": "Gives Snake bare forearms and fingerless gloves, in the Virtuous Mission and the main game, with four textures. This is version 1.1; version 1.2 and a compatibility patch need the author's Haircut mod and are not installed. It does not affect the camo index and the author says it is for Citra.",
      "verified": false
    },
    {
      "id": "mgs3-no-gyro-balance-icons",
      "name": "No Gyro Balance Icons",
      "title": "Metal Gear Solid: Snake Eater 3D",
      "title_ids": [
        "0004000000081E00",
        "0004000000082400"
      ],
      "author": "HugAMuffin",
      "author_url": "https://gamebanana.com/members/1608504",
      "downloads": 215,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 34055,
        "file_pattern": "3ds_balance_icons.rar"
      },
      "size": 1511,
      "format": "rar",
      "layout": "single",
      "folder": "No Gyro Balance Icons",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/34055",
      "notes": "Removes the gyro control icon from the balance screens, since Citra and Azahar do not use gyro controls there. It is seven small textures. The author made it for Citra.",
      "verified": false
    },
    {
      "id": "mgs3-subtle-button-prompts",
      "name": "Subtle Button Prompts",
      "title": "Metal Gear Solid: Snake Eater 3D",
      "title_ids": [
        "0004000000081E00",
        "0004000000082400"
      ],
      "author": "HugAMuffin",
      "author_url": "https://gamebanana.com/members/1608504",
      "downloads": 175,
      "updated": "2026-09",
      "source": {
        "type": "gamebanana",
        "mod": 34054,
        "file_pattern": "subtle_button_prompts_*.rar"
      },
      "size": 530332,
      "format": "rar",
      "layout": "single",
      "folder": "Subtle Button Prompts",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/34054",
      "notes": "Makes the on-screen button prompts semi-transparent so they are less intrusive without being removed, with 30 small textures at 80% transparency. It is a tiny download, and the author made it for Citra.",
      "verified": false
    },
    {
      "id": "fates-hd-combined-ui",
      "name": "Combined HD Pack: UI",
      "title": "Fire Emblem Fates (USA)",
      "title_ids": [
        "0004000000179800",
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "Zoran501",
      "author_url": "https://gamebanana.com/members/1735224",
      "downloads": 20833,
      "updated": "2022-10",
      "source": {
        "type": "gdrive",
        "file_id": "1ApmQDEIT89Yzui-PA0SqFMG5vA3ktU7H"
      },
      "size": 110079817,
      "format": "zip",
      "layout": "single",
      "folder": "Fates HD UI",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/351105",
      "notes": "Redrawn menus, text boxes, skill icons and the like. One part of the Combined HD Pack, made for the USA version.",
      "verified": true
    },
    {
      "id": "fates-hd-combined-fonts",
      "name": "Combined HD Pack: Fonts",
      "title": "Fire Emblem Fates (USA)",
      "title_ids": [
        "0004000000179800",
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "Zoran501",
      "author_url": "https://gamebanana.com/members/1735224",
      "downloads": 20833,
      "updated": "2022-10",
      "source": {
        "type": "gdrive",
        "file_id": "11PQmu6kA9HIhLuHH5z6AS3kCx8BVJnLf"
      },
      "size": 2714035,
      "format": "zip",
      "layout": "single",
      "folder": "Fates HD Fonts",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/351105",
      "notes": "Redrawn fonts. One part of the Combined HD Pack, made for the USA version.",
      "verified": true
    },
    {
      "id": "fates-hd-combined-character-artwork",
      "name": "Combined HD Pack: Character Artwork",
      "title": "Fire Emblem Fates (USA)",
      "title_ids": [
        "0004000000179800",
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "Zoran501",
      "author_url": "https://gamebanana.com/members/1735224",
      "downloads": 20833,
      "updated": "2022-10",
      "source": {
        "type": "gdrive",
        "file_id": "1ny1XiVV0UKa_RyY4hvC_jICi6BxaxBjD"
      },
      "size": 1026237488,
      "format": "zip",
      "layout": "single",
      "folder": "Fates HD Character Artwork",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/351105",
      "notes": "HD character artwork, including the amiibo characters. One part of the Combined HD Pack, made for the USA version.",
      "verified": true
    },
    {
      "id": "fates-hd-combined-face-alpha",
      "name": "Combined HD Pack: Face and Alpha Textures",
      "title": "Fire Emblem Fates (USA)",
      "title_ids": [
        "0004000000179800",
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "Zoran501",
      "author_url": "https://gamebanana.com/members/1735224",
      "downloads": 20833,
      "updated": "2022-10",
      "source": {
        "type": "gdrive",
        "file_id": "1Wrfr1YDPmZQWtPQwkli1Z_bCWacZPL7K"
      },
      "size": 583635161,
      "format": "zip",
      "layout": "single",
      "folder": "Fates HD Face and Alpha Textures",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/351105",
      "notes": "HD faces and alpha textures. One part of the Combined HD Pack, made for the USA version.",
      "verified": true
    },
    {
      "id": "fates-hd-combined-map-floors",
      "name": "Combined HD Pack: Map Floors",
      "title": "Fire Emblem Fates (USA)",
      "title_ids": [
        "0004000000179800",
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "Zoran501",
      "author_url": "https://gamebanana.com/members/1735224",
      "downloads": 20833,
      "updated": "2022-10",
      "source": {
        "type": "gdrive",
        "file_id": "1BkmhoO1m8ymBFQ0n0ZIkZAU5sAcAxjZe"
      },
      "size": 2509938054,
      "format": "zip",
      "layout": "single",
      "folder": "Fates HD Map Floors",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/351105",
      "notes": "HD map floor textures. One part of the Combined HD Pack, made for the USA version.",
      "verified": true
    },
    {
      "id": "fates-hd-combined-my-room",
      "name": "Combined HD Pack: My Room Characters",
      "title": "Fire Emblem Fates (USA)",
      "title_ids": [
        "0004000000179800",
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "Zoran501",
      "author_url": "https://gamebanana.com/members/1735224",
      "downloads": 20833,
      "updated": "2022-10",
      "source": {
        "type": "gdrive",
        "file_id": "1r3lF_tv4ypfeEnRjoWJAD3gQsxB-fcvI"
      },
      "size": 114364027,
      "format": "zip",
      "layout": "single",
      "folder": "Fates HD My Room Characters",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/351105",
      "notes": "HD My Room character textures. One part of the Combined HD Pack, made for the USA version.",
      "verified": true
    },
    {
      "id": "fates-hd-combined-other",
      "name": "Combined HD Pack: Other",
      "title": "Fire Emblem Fates (USA)",
      "title_ids": [
        "0004000000179800",
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "Zoran501",
      "author_url": "https://gamebanana.com/members/1735224",
      "downloads": 20833,
      "updated": "2022-10",
      "source": {
        "type": "gdrive",
        "file_id": "1ONVqDdUUZOk80N2Z8OzDmG9b_oSXc7pZ"
      },
      "size": 757255405,
      "format": "zip",
      "layout": "single",
      "folder": "Fates HD Other",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/351105",
      "notes": "The remaining HD textures. One part of the Combined HD Pack, made for the USA version.",
      "verified": true
    },
    {
      "id": "fates-hd-combined-outfits",
      "name": "Combined HD Pack: Outfits",
      "title": "Fire Emblem Fates (USA)",
      "title_ids": [
        "0004000000179800",
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "Zoran501",
      "author_url": "https://gamebanana.com/members/1735224",
      "downloads": 20833,
      "updated": "2022-10",
      "source": {
        "type": "gdrive",
        "file_id": "14_ftEnfA-ACG5QO3z_zAAD-BrKoSKt49"
      },
      "size": 299304378,
      "format": "zip",
      "layout": "single",
      "folder": "Fates HD Outfits",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/351105",
      "notes": "HD outfit textures. One part of the Combined HD Pack, made for the USA version.",
      "verified": true
    },
    {
      "id": "fates-hd-additions",
      "name": "Fates HD Additions",
      "title": "Fire Emblem Fates (USA)",
      "title_ids": [
        "0004000000179800",
        "0004000000179400",
        "0004000000179600"
      ],
      "author": "L498",
      "author_url": "https://gamebanana.com/members/2091373",
      "downloads": 4721,
      "updated": "2024-07",
      "source": {
        "type": "gamebanana",
        "mod": 403826,
        "file_pattern": "fates_hd_additions_*.zip"
      },
      "size": 243516186,
      "format": "zip",
      "layout": "single",
      "folder": "Fates HD Additions",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/403826",
      "notes": "HD UI, icons and font that go on top of the Combined HD Pack, for the USA version.",
      "verified": false
    },
    {
      "id": "awakening-aahd-effects",
      "name": "Amateur Awakening HD: Effects and Cutscenes",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "00040000000A0500"
      ],
      "author": "L498",
      "author_url": "https://gamebanana.com/members/2091373",
      "downloads": 26582,
      "updated": "2025-10",
      "source": {
        "type": "gdrive",
        "file_id": "1UYdPOB-Ztpv6aSY83djEoATMFYGHxw74"
      },
      "size": 855877470,
      "format": "zip",
      "layout": "single",
      "folder": "AAHD Effects and Cutscenes",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/400809",
      "notes": "Blush, confession, critical hit, cutscene and DLC textures. One part of Amateur Awakening HD, for the USA version.",
      "verified": true
    },
    {
      "id": "awakening-aahd-characters-ui",
      "name": "Amateur Awakening HD: Characters and UI",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "00040000000A0500"
      ],
      "author": "L498",
      "author_url": "https://gamebanana.com/members/2091373",
      "downloads": 26582,
      "updated": "2025-10",
      "source": {
        "type": "gdrive",
        "file_id": "1bRfDWoryYf9Wl4Dk6hxLgO2xDmNk5IWB"
      },
      "size": 725807104,
      "format": "zip",
      "layout": "single",
      "folder": "AAHD Characters and UI",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/400809",
      "notes": "Redrawn character textures and UI. One part of Amateur Awakening HD, for the USA version.",
      "verified": true
    },
    {
      "id": "awakening-aahd-maps",
      "name": "Amateur Awakening HD: Map Textures",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "00040000000A0500"
      ],
      "author": "L498",
      "author_url": "https://gamebanana.com/members/2091373",
      "downloads": 26582,
      "updated": "2025-10",
      "source": {
        "type": "gdrive",
        "file_id": "17ExQwGA8FEK8BAsc-aLNjmto3wNW1A7x"
      },
      "size": 3032430618,
      "format": "zip",
      "layout": "single",
      "folder": "AAHD Map Textures",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/400809",
      "notes": "The large map textures. One part of Amateur Awakening HD, for the USA version.",
      "verified": true
    },
    {
      "id": "awakening-aa3d",
      "name": "Amateur Awakening 3D",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "00040000000A0500"
      ],
      "author": "L498",
      "author_url": "https://gamebanana.com/members/2091373",
      "downloads": 4557,
      "updated": "2026-01",
      "source": {
        "type": "gamebanana",
        "mod": 548234,
        "file_pattern": "aa3d_*.zip"
      },
      "size": 116283084,
      "format": "zip",
      "layout": "single",
      "folder": "Amateur Awakening 3D",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/548234",
      "notes": "3D model textures in the same style, for the USA version; works with Amateur Awakening HD.",
      "verified": false
    },
    {
      "id": "awakening-project-reawakening-eur",
      "name": "Project reAwakening v0.3",
      "title": "Fire Emblem Awakening",
      "title_ids": [
        "000400000009F100"
      ],
      "author": "rdnc",
      "downloads": 5672,
      "updated": "2020-06",
      "source": {
        "type": "gamebanana",
        "mod": 9944,
        "file_pattern": "project_reawakening_v03_*.rar"
      },
      "size": 37972156,
      "format": "rar",
      "layout": "single",
      "folder": "Project reAwakening v0.3",
      "homepage": "https://gamebanana.com/mods/9944",
      "notes": "The HD texture pack for the Europe and Australia versions, last updated in 2020.",
      "verified": false
    },
    {
      "id": "oot3d-blood-in-the-well",
      "name": "Blood in the Well",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "HugAMuffin",
      "author_url": "https://gamebanana.com/members/1608504",
      "downloads": 3940,
      "updated": "2026-02",
      "source": {
        "type": "gamebanana",
        "mod": 150246,
        "file_pattern": "blood_in_the_well.rar"
      },
      "size": 415825,
      "format": "rar",
      "layout": "single",
      "folder": "Blood in the Well",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/150246",
      "notes": "Brings back some of the blood cut from the 3DS version, mostly in the Well.",
      "verified": false
    },
    {
      "id": "oot3d-original-dead-hand",
      "name": "Original Dead Hand",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "LochNessHamster",
      "author_url": "https://gamebanana.com/members/1082848",
      "downloads": 3734,
      "updated": "2021-06",
      "source": {
        "type": "gamebanana",
        "mod": 297645,
        "file_pattern": "original_dead_hand_-_oot3d.zip"
      },
      "size": 740312,
      "format": "zip",
      "layout": "single",
      "folder": "Original Dead Hand",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/297645",
      "notes": "Recreates the Dead Hand's original, more horrific design.",
      "verified": false
    },
    {
      "id": "oot3d-expressive-face-young-link",
      "name": "Expressive Face Textures for Young Link",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "LochNessHamster",
      "author_url": "https://gamebanana.com/members/1082848",
      "downloads": 3441,
      "updated": "2021-06",
      "source": {
        "type": "gamebanana",
        "mod": 250773,
        "file_pattern": "oot3d_-_expressive_face_textures_v2_*.zip"
      },
      "size": 2557644,
      "format": "zip",
      "layout": "single",
      "folder": "Expressive Face Textures for Young Link",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250773",
      "notes": "Higher-resolution face textures that make Young Link more expressive.",
      "verified": false
    },
    {
      "id": "oot3d-art-accurate-link",
      "name": "Art-Accurate Link",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "LochNessHamster",
      "author_url": "https://gamebanana.com/members/1082848",
      "downloads": 2377,
      "updated": "2021-06",
      "source": {
        "type": "gamebanana",
        "mod": 250774,
        "file_pattern": "oot3d_-_art_accurate_link_v20.zip"
      },
      "size": 8324662,
      "format": "zip",
      "layout": "single",
      "folder": "Art-Accurate Link",
      "layer_on": "oot3d-link-skin-pack",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250774",
      "notes": "Redraws Link to match the official art; the author says to install it after the Link Skin Pack.",
      "verified": false
    },
    {
      "id": "oot3d-enhanced-hd-link",
      "name": "Enhanced HD Link",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500"
      ],
      "author": "Darky57",
      "author_url": "https://gamebanana.com/members/2745839",
      "downloads": 2320,
      "updated": "2023-12",
      "source": {
        "type": "gamebanana",
        "mod": 482884,
        "file_pattern": "definitive_3ds_link_*.zip"
      },
      "size": 30912428,
      "format": "zip",
      "layout": "single",
      "folder": "Enhanced HD Link",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/482884",
      "notes": "A hand-made HD Link for the USA version, meant to go over the Henriko HD pack.",
      "verified": false
    },
    {
      "id": "oot3d-vanilla-brightness-tweaks",
      "name": "Vanilla Brightness Tweaks",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "lmfaoman1217",
      "author_url": "https://gamebanana.com/members/2812638",
      "downloads": 1890,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 657902,
        "file_pattern": "oot3d_vanilla_tweaks_*.zip"
      },
      "size": 19276452,
      "format": "zip",
      "layout": "single",
      "folder": "Vanilla Brightness Tweaks",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/657902",
      "notes": "Darkens the overbright areas, with one folder per area so each can be turned off.",
      "verified": false
    },
    {
      "id": "oot3d-redrawn-hd-minimaps",
      "name": "Redrawn HD Minimaps",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "MistSonata",
      "author_url": "https://gamebanana.com/members/3405660",
      "downloads": 1886,
      "updated": "2024-05",
      "source": {
        "type": "gamebanana",
        "mod": 515850,
        "file_pattern": "oot3ds_minimaps_1-1.rar"
      },
      "size": 1739198,
      "format": "rar",
      "layout": "single",
      "folder": "Redrawn HD Minimaps",
      "licence": "CC BY 4.0",
      "homepage": "https://gamebanana.com/mods/515850",
      "notes": "Hand-drawn minimaps made for 4K, without the dungeon maps.",
      "verified": false
    },
    {
      "id": "oot3d-hylian-shield-triforce-fix",
      "name": "Hylian Shield Triforce Fix",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "LochNessHamster",
      "author_url": "https://gamebanana.com/members/1082848",
      "downloads": 1837,
      "updated": "2020-04",
      "source": {
        "type": "gamebanana",
        "mod": 250771,
        "file_pattern": "ocarina_of_time_3d_-_hylian_shield_*.zip"
      },
      "size": 211944,
      "format": "zip",
      "layout": "single",
      "folder": "Hylian Shield Triforce Fix",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250771",
      "notes": "Reshapes the Triforce on the Hylian Shield to match the original.",
      "verified": false
    },
    {
      "id": "oot3d-og-ganon-fight",
      "name": "OG Ganon Fight Texture Pack",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "YaBoiMikey",
      "author_url": "https://gamebanana.com/members/1757292",
      "downloads": 1806,
      "updated": "2020-11",
      "source": {
        "type": "gamebanana",
        "mod": 250778,
        "file_pattern": "423_classic_ganon.rar"
      },
      "size": 8971865,
      "format": "rar",
      "layout": "single",
      "folder": "OG Ganon Fight Texture Pack",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250778",
      "notes": "Textures that bring the Ganon fight closer to the original.",
      "verified": false
    },
    {
      "id": "oot3d-hd-master-sword",
      "name": "HD Hand-Drawn Master Sword",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "LochNessHamster",
      "author_url": "https://gamebanana.com/members/1082848",
      "downloads": 1780,
      "updated": "2021-06",
      "source": {
        "type": "gamebanana",
        "mod": 250769,
        "file_pattern": "hd_master_sword_textures_-_oot3d_-_.zip"
      },
      "size": 469247,
      "format": "zip",
      "layout": "single",
      "folder": "HD Hand-Drawn Master Sword",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250769",
      "notes": "A hand-drawn HD Master Sword and scabbard.",
      "verified": false
    },
    {
      "id": "mm3d-original-fierce-deity",
      "name": "Original Fierce Deity Coloring",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500"
      ],
      "author": "LochNessHamster",
      "author_url": "https://gamebanana.com/members/1082848",
      "downloads": 1269,
      "updated": "2021-06",
      "source": {
        "type": "gamebanana",
        "mod": 250764,
        "file_pattern": "original_fierce_deity_v21_-_mm3d.zip"
      },
      "size": 2954889,
      "format": "zip",
      "layout": "single",
      "folder": "Original Fierce Deity Coloring",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250764",
      "notes": "Returns the Fierce Deity's colours to the original N64 look.",
      "verified": false
    },
    {
      "id": "mm3d-classic-zora-link",
      "name": "Classic Zora Link",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500"
      ],
      "author": "LochNessHamster",
      "author_url": "https://gamebanana.com/members/1082848",
      "downloads": 1118,
      "updated": "2021-06",
      "source": {
        "type": "gamebanana",
        "mod": 250765,
        "file_pattern": "mm3d_-_classic_zora_link_21.zip"
      },
      "size": 4929899,
      "format": "zip",
      "layout": "single",
      "folder": "Classic Zora Link",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250765",
      "notes": "Gives Zora Link the classic N64 look.",
      "verified": false
    },
    {
      "id": "mm3d-classic-deku-link",
      "name": "Classic Deku Link",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500"
      ],
      "author": "LochNessHamster",
      "author_url": "https://gamebanana.com/members/1082848",
      "downloads": 1032,
      "updated": "2021-06",
      "source": {
        "type": "gamebanana",
        "mod": 250760,
        "file_pattern": "mm3d_-_classic_deku_link_v11.zip"
      },
      "size": 1194900,
      "format": "zip",
      "layout": "single",
      "folder": "Classic Deku Link",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250760",
      "notes": "Gives Deku Link the classic N64 look.",
      "verified": false
    },
    {
      "id": "mm3d-link-skin-pack",
      "name": "Link Skin Pack",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500"
      ],
      "author": "Art-Does-Not-Compute",
      "author_url": "https://gamebanana.com/members/1716642",
      "downloads": 1600,
      "updated": "2020-08",
      "source": {
        "type": "gamebanana",
        "mod": 250763,
        "file_pattern": "mm_textures_*.rar"
      },
      "size": 3427341,
      "format": "rar",
      "layout": "variants",
      "folder": "Link Skin Pack",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250763",
      "notes": "Several looks for Link; each shows as its own pack.",
      "verified": false
    },
    {
      "id": "mm3d-adult-link-gloves",
      "name": "Gloves for Young Link",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "Darky57",
      "author_url": "https://gamebanana.com/members/2745839",
      "downloads": 1408,
      "updated": "2023-08",
      "source": {
        "type": "gamebanana",
        "mod": 462941,
        "file_pattern": "adult_link_style_gloves_for_young_link.zip"
      },
      "size": 2341451,
      "format": "zip",
      "layout": "single",
      "folder": "Gloves for Young Link",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/462941",
      "notes": "Gives Young Link gloves, with an adult-style outfit as a second option.",
      "verified": false
    },
    {
      "id": "mm3d-fierce-deity-all-forms",
      "name": "Fierce Deity All Forms",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "Darky57",
      "author_url": "https://gamebanana.com/members/2745839",
      "downloads": 1052,
      "updated": "2023-09",
      "source": {
        "type": "gamebanana",
        "mod": 468401,
        "file_pattern": "fierce_deity_all_forms_*.zip"
      },
      "size": 3264174,
      "format": "zip",
      "layout": "single",
      "folder": "Fierce Deity All Forms",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/468401",
      "notes": "Gives every transformation a Fierce Deity look.",
      "verified": false
    },
    {
      "id": "mm3d-art-accurate-fairies",
      "name": "Art-Accurate Fairies",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "Takeshre",
      "author_url": "https://gamebanana.com/members/2184805",
      "downloads": 1296,
      "updated": "2023-01",
      "source": {
        "type": "gamebanana",
        "mod": 391777,
        "file_pattern": "art-accurate_fairies.zip"
      },
      "size": 61256,
      "format": "zip",
      "layout": "single",
      "folder": "Art-Accurate Fairies",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/391777",
      "notes": "Redraws Tatl, Tael and the recovery fairies to match the official art.",
      "verified": false
    },
    {
      "id": "mm3d-heros-clothes",
      "name": "Hero's Clothes",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "ByTheEightDivines",
      "author_url": "https://gamebanana.com/members/1722968",
      "downloads": 1369,
      "updated": "2020-04",
      "source": {
        "type": "gamebanana",
        "mod": 250762,
        "file_pattern": "heros_clothes_v2.rar"
      },
      "size": 3696917,
      "format": "rar",
      "layout": "single",
      "folder": "Hero's Clothes",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/250762",
      "notes": "A Kokiri-style outfit for Link with chainmail and gauntlets.",
      "verified": false
    },
    {
      "id": "mm3d-mirror-shield-alternatives",
      "name": "Mirror Shield Alternatives",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "Rikster",
      "author_url": "https://gamebanana.com/members/1918817",
      "downloads": 1106,
      "updated": "2023-02",
      "source": {
        "type": "gamebanana",
        "mod": 384697,
        "file_pattern": "alternative_mirror_shields_v11.zip"
      },
      "size": 2454918,
      "format": "zip",
      "layout": "variants",
      "folder": "Mirror Shield Alternatives",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/384697",
      "notes": "Alternative looks for the Mirror Shield, including the Hero's Shield.",
      "verified": false
    },
    {
      "id": "mm3d-mirror-shield-alternatives-lowres",
      "name": "Mirror Shield Alternatives (Low-Res)",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "Rikster",
      "author_url": "https://gamebanana.com/members/1918817",
      "downloads": 1106,
      "updated": "2023-02",
      "source": {
        "type": "gamebanana",
        "mod": 384697,
        "file_pattern": "alternative_mirror_shields_lowres_v11.zip"
      },
      "size": 61667,
      "format": "zip",
      "layout": "variants",
      "folder": "Mirror Shield Alternatives (Low-Res)",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/384697",
      "notes": "The same alternative Mirror Shield looks at low resolution.",
      "verified": false
    },
    {
      "id": "mm3d-vanilla-brightness-tweaks",
      "name": "Vanilla Brightness Tweaks",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "lmfaoman1217",
      "author_url": "https://gamebanana.com/members/2812638",
      "downloads": 954,
      "updated": "2026-05",
      "source": {
        "type": "gamebanana",
        "mod": 666038,
        "file_pattern": "mm3d_vanilla_tweaks_*.zip"
      },
      "size": 41763490,
      "format": "zip",
      "layout": "single",
      "folder": "Vanilla Brightness Tweaks",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/666038",
      "notes": "Darkens the overbright areas, with one folder per area so each can be turned off.",
      "verified": false
    },
    {
      "id": "mm3d-prg-gui-3ds",
      "name": "PRG GUI for Project Restoration (3DS)",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "PRG",
      "author_url": "https://gamebanana.com/members/1317176",
      "downloads": 872,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 569069,
        "file_pattern": "hud_3ds.zip"
      },
      "size": 2536852,
      "format": "zip",
      "layout": "single",
      "folder": "PRG GUI for Project Restoration (3DS)",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/569069",
      "notes": "Button prompts in the 3DS style for Project Restoration, which must be installed.",
      "verified": false
    },
    {
      "id": "mm3d-prg-gui-switch",
      "name": "PRG GUI for Project Restoration (Switch)",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "PRG",
      "author_url": "https://gamebanana.com/members/1317176",
      "downloads": 872,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 569069,
        "file_pattern": "hud_switch.zip"
      },
      "size": 2525301,
      "format": "zip",
      "layout": "single",
      "folder": "PRG GUI for Project Restoration (Switch)",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/569069",
      "notes": "Button prompts in the Switch style for Project Restoration, which must be installed.",
      "verified": false
    },
    {
      "id": "mm3d-prg-gui-switch-color",
      "name": "PRG GUI for Project Restoration (Switch (Color))",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "PRG",
      "author_url": "https://gamebanana.com/members/1317176",
      "downloads": 872,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 569069,
        "file_pattern": "hud_switch_color.zip"
      },
      "size": 2532998,
      "format": "zip",
      "layout": "single",
      "folder": "PRG GUI for Project Restoration (Switch (Color))",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/569069",
      "notes": "Button prompts in the Switch (Color) style for Project Restoration, which must be installed.",
      "verified": false
    },
    {
      "id": "mm3d-prg-gui-xbox-default",
      "name": "PRG GUI for Project Restoration (Xbox)",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "PRG",
      "author_url": "https://gamebanana.com/members/1317176",
      "downloads": 872,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 569069,
        "file_pattern": "hud_xbox_default.zip"
      },
      "size": 2632149,
      "format": "zip",
      "layout": "single",
      "folder": "PRG GUI for Project Restoration (Xbox)",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/569069",
      "notes": "Button prompts in the Xbox style for Project Restoration, which must be installed.",
      "verified": false
    },
    {
      "id": "mm3d-prg-gui-xbox-color",
      "name": "PRG GUI for Project Restoration (Xbox (Color))",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "PRG",
      "author_url": "https://gamebanana.com/members/1317176",
      "downloads": 872,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 569069,
        "file_pattern": "hud_xbox_color.zip"
      },
      "size": 2637935,
      "format": "zip",
      "layout": "single",
      "folder": "PRG GUI for Project Restoration (Xbox (Color))",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/569069",
      "notes": "Button prompts in the Xbox (Color) style for Project Restoration, which must be installed.",
      "verified": false
    },
    {
      "id": "mm3d-prg-gui-deck",
      "name": "PRG GUI for Project Restoration (Steam Deck)",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "PRG",
      "author_url": "https://gamebanana.com/members/1317176",
      "downloads": 872,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 569069,
        "file_pattern": "hud_deck.zip"
      },
      "size": 2632123,
      "format": "zip",
      "layout": "single",
      "folder": "PRG GUI for Project Restoration (Steam Deck)",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/569069",
      "notes": "Button prompts in the Steam Deck style for Project Restoration, which must be installed.",
      "verified": false
    },
    {
      "id": "mm3d-prg-gui-ps",
      "name": "PRG GUI for Project Restoration (PlayStation)",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "PRG",
      "author_url": "https://gamebanana.com/members/1317176",
      "downloads": 872,
      "updated": "2026-03",
      "source": {
        "type": "gamebanana",
        "mod": 569069,
        "file_pattern": "hud_ps.zip"
      },
      "size": 2639713,
      "format": "zip",
      "layout": "single",
      "folder": "PRG GUI for Project Restoration (PlayStation)",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/569069",
      "notes": "Button prompts in the PlayStation style for Project Restoration, which must be installed.",
      "verified": false
    },
    {
      "id": "mm3d-hd-texture-pack-darmani",
      "name": "HD Texture Pack (Deprecated)",
      "title": "The Legend of Zelda: Majora's Mask 3D",
      "title_ids": [
        "0004000000125500",
        "0004000000125600"
      ],
      "author": "Darmani",
      "author_url": "https://gamebanana.com/members/2435860",
      "downloads": 2755,
      "updated": "2024-08",
      "source": {
        "type": "gdrive",
        "file_id": "1uz3R9wCP5hHXvvdA35V-1nw1lV7EFwDE"
      },
      "size": 4233221775,
      "format": "zip",
      "layout": "single",
      "folder": "MM3D HD Darmani",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/422313",
      "notes": "The author has deprecated this full HD pack and calls it glitchy.",
      "verified": true
    },
    {
      "id": "pokemon-xy-faithful-hd-types",
      "name": "Faithful HD Pokemon Types",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "MetalTxus",
      "author_url": "https://gamebanana.com/members/2566769",
      "downloads": 879,
      "updated": "2025-11",
      "source": {
        "type": "gamebanana",
        "mod": 633609,
        "file_pattern": "pokemon_types_xy_*.zip"
      },
      "size": 157869,
      "format": "zip",
      "layout": "single",
      "folder": "Faithful HD Pokemon Types",
      "layer_on": "pokemon-xy-5x-redrawn-ui",
      "licence": "CC BY-NC 4.0",
      "homepage": "https://gamebanana.com/mods/633609",
      "notes": "Redraws the type and status icons at 5x on a 5-pixel grid; it goes on top of the 5X Redrawn UI.",
      "verified": false
    },
    {
      "id": "pokemon-xy-modern-items-sprites",
      "name": "Modern Items Sprites",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "stingflyer",
      "author_url": "https://gamebanana.com/members/2457337",
      "downloads": 606,
      "updated": "2025-11",
      "source": {
        "type": "gamebanana",
        "mod": 634399,
        "file_pattern": "items_sprites_*.zip"
      },
      "size": 10633075,
      "format": "zip",
      "layout": "single",
      "folder": "Modern Items Sprites",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/634399",
      "notes": "Replaces the item sprites in the bag with the modern Gen 9 art, plus the Global Link sprites for items not redrawn.",
      "verified": false
    },
    {
      "id": "pokemon-xy-shadow-ho-oh",
      "name": "Shadow Ho-Oh",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "Lewtwo",
      "author_url": "https://gamebanana.com/members/1750133",
      "downloads": 406,
      "updated": "2020-09",
      "source": {
        "type": "gamebanana",
        "mod": 251638,
        "file_pattern": "004_-_shadow_ho-oh_*.zip"
      },
      "size": 6069938,
      "format": "zip",
      "layout": "single",
      "folder": "Shadow Ho-Oh",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/251638",
      "notes": "A Shadow Lugia-style palette swap for Ho-Oh, made for all the 3DS Pokemon games.",
      "verified": false
    },
    {
      "id": "pokemon-xy-better-chingling-chimecho-shiny",
      "name": "Better Chingling and Chimecho Shiny",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "eeveelover64",
      "author_url": "https://gamebanana.com/members/2078266",
      "downloads": 94,
      "updated": "2025-08",
      "source": {
        "type": "gamebanana",
        "mod": 610984,
        "file_pattern": "better_chingling_chimecho_shiny_citra_edition_.zip"
      },
      "size": 47317,
      "format": "zip",
      "layout": "single",
      "folder": "Better Chingling and Chimecho Shiny",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/610984",
      "notes": "Gives the shiny Chingling and Chimecho a palette that is easier to spot; this is the emulator build.",
      "verified": false
    },
    {
      "id": "oot3d-4k-ui-translated-ps5-layout",
      "name": "4K UI (Translated, PS5 Layout)",
      "title": "The Legend of Zelda: Ocarina of Time 3D",
      "title_ids": [
        "0004000000033500",
        "0004000000033600",
        "0004000000033400"
      ],
      "author": "Yozzui",
      "author_url": "https://gamebanana.com/members/3474050",
      "downloads": 1730,
      "updated": "2026-10",
      "source": {
        "type": "gamebanana",
        "mod": 725250,
        "file_pattern": "ui_ocarina_of_time_3d.zip"
      },
      "size": 453218817,
      "format": "zip",
      "layout": "variants",
      "folder": "4K UI (Translated, PS5 Layout)",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/725250",
      "notes": "A 4K interface in German, Italian, Japanese and Spanish with PS5 button prompts; each language is a part.",
      "verified": false
    },
    {
      "id": "pokemon-xy-crimson-shiny-greninja",
      "name": "Crimson Shiny Greninja",
      "title": "Pokemon X / Pokemon Y",
      "title_ids": [
        "0004000000055D00",
        "0004000000055E00"
      ],
      "author": "DuoForce",
      "author_url": "https://gamebanana.com/members/1557349",
      "downloads": 254,
      "updated": "2023-11",
      "source": {
        "type": "gamebanana",
        "mod": 479095,
        "file_pattern": "crimson_shiny_greninja.7z"
      },
      "size": 37907,
      "format": "7z",
      "layout": "single",
      "folder": "Crimson Shiny Greninja",
      "licence": "CC BY-NC-ND 4.0",
      "homepage": "https://gamebanana.com/mods/479095",
      "notes": "Gives the shiny Greninja a crimson palette.",
      "verified": false
    },
    {
      "id": "dq8-modular-hd-texture-replacements",
      "name": "Modular HD Texture Replacements",
      "title": "Dragon Quest VIII: Journey of the Cursed King",
      "title_ids": [
        "000400000018F100",
        "000400000018F200"
      ],
      "author": "MetalTxus",
      "author_url": "https://gamebanana.com/members/2566769",
      "downloads": 554,
      "updated": "2026-08",
      "source": {
        "type": "gamebanana",
        "mod": 480972,
        "file_pattern": "dq8_3ds_modular_hd_texture_replacements_v1_2_6c.zip"
      },
      "size": 1510854,
      "format": "zip",
      "layout": "variants",
      "folder": "Modular HD Texture Replacements",
      "licence": "CC BY-NC 4.0",
      "homepage": "https://gamebanana.com/mods/480972",
      "notes": "Small HD replacements in separate folders, so you can turn the parts on and off.",
      "verified": false
    },
    {
      "id": "stella-glow-hd-4x",
      "name": "Stella Glow HD",
      "title": "Stella Glow",
      "title_ids": [
        "0004000000173700"
      ],
      "author": "simplysanatori",
      "author_url": "https://github.com/simplysanatori",
      "updated": "2026-11",
      "source": {
        "type": "gdrive",
        "file_id": "1XSi6l-QUmFrzYL4flQe6wZu1NGR859w_"
      },
      "size": 5003167526,
      "sha256": "6105f4517bc5e4a8cb3e97c14654cd2bd0e8ec6abe1e93a4728b23bd8312576f",
      "format": "zip",
      "layout": "single",
      "folder": "Stella Glow HD",
      "licence": "Free for emulators; credit the author",
      "notes": "Every texture upscaled 4x with AI, with the UI text, logos and turn-order portraits redrawn by hand. Version 0.9.0. It is large, about 4.7 GB, so on the Frame it streams in instead of preloading. Made on the USA version.",
      "verified": true,
      "recommended": true
    },
    {
      "id": "kiu-weapons-hd-4x",
      "name": "Kid Icarus Uprising Weapons HD",
      "title": "Kid Icarus: Uprising",
      "title_ids": [
        "0004000000030200"
      ],
      "author": "simplysanatori",
      "author_url": "https://github.com/simplysanatori",
      "updated": "2026-11",
      "source": {
        "type": "gdrive",
        "file_id": "1VJmYZdP38-lo_e6wGDj_XfCVIuQD6wLg"
      },
      "size": 84613857,
      "sha256": "290cb743f7c3e969b180911cadc209af026d4e821032665fbc813d50ef48bc4c",
      "format": "zip",
      "layout": "single",
      "folder": "Kid Icarus Uprising Weapons HD",
      "licence": "Free for emulators; credit the author",
      "notes": "The weapon textures upscaled 4x: decompressed with DXTDecompressor, then upscaled with the 4x-UltraSharp model by Kim2091. Version 0.1.0, an early release. Made on the USA version.",
      "verified": true,
      "recommended": true
    }
  ]
})JSON";
}

} // namespace CustomTex::Download
