// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame versions: the table compiled in, {title, raw version -> what the game shows}. Every
// entry carries the source it was checked against. The online catalog's "versions" section (same
// shape) extends and overrides it. Only entries that were verified belong here.

namespace Cheats::Sharkive {

const char* DefaultVersionsJson() {
    return R"AZVER({
  "versions": [
    {
      "title_ids": ["001B4100"],
      "raw": 1024,
      "shown": "1.1",
      "source": "Fire Emblem Echoes, European release, with its update installed: the title screen shows ver 1.1 (checked in a run of the emulator)"
    },
    {
      "title_ids": ["00030800", "00030700", "00030600"],
      "raw": 1040,
      "shown": "1.1",
      "source": "3dbrew Title list/Patches: Mario Kart 7 [Update Data] v1.1 (v1040), USA, Europe and Japan"
    },
    {
      "title_ids": ["00030800"],
      "raw": 3072,
      "shown": "1.2",
      "source": "Mario Kart 7, USA release: the installed update's SMDH large icon reads Ver. 1.2 (Nintendo's Dec 2022 update, the only one after 1.1 = v1040)"
    }
  ]
}
)AZVER";
}

} // namespace Cheats::Sharkive
