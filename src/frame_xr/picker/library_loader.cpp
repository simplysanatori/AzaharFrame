// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version

#include "frame_xr/picker/library_loader.h"

#include <cstring>
#include <memory>
#include <vector>

#include "common/string_util.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"

namespace frame_xr::picker {

std::optional<GameMeta> ReadMetadataWithLoader(const std::string& path) {
    std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(path);
    if (!loader) {
        return std::nullopt;
    }

    // Same filters as GameListWorker::AddFstEntriesToGameList.
    bool executable = false;
    const Loader::ResultStatus res = loader->IsExecutable(executable);
    if (!executable && res != Loader::ResultStatus::ErrorEncrypted) {
        return std::nullopt;
    }

    u64 program_id = 0;
    loader->ReadProgramId(program_id);

    std::vector<u8> smdh_buffer;
    loader->ReadIcon(smdh_buffer, true);

    const bool system_title = ((program_id >> 32) & 0xFFFFFFFF) == 0x00040010;
    const bool valid = Loader::IsValidSMDH(smdh_buffer);
    Loader::SMDH smdh{};
    if (valid) {
        std::memcpy(&smdh, smdh_buffer.data(), sizeof(smdh));
        if (system_title && !smdh.flags.visible) {
            return std::nullopt;
        }
    } else if (system_title) {
        return std::nullopt;
    }

    GameMeta meta;
    std::string title;
    if (loader->ReadTitle(title, true) == Loader::ResultStatus::Success) {
        meta.title = title;
    }
    if (valid) {
        if (meta.title.empty()) {
            const auto short_title = smdh.GetShortTitle(Loader::SMDH::TitleLanguage::English);
            meta.title = Common::UTF16ToUTF8(std::u16string_view(short_title.data()));
        }
        meta.icon_w = kIconSize;
        meta.icon_h = kIconSize;
        const std::vector<u16> icon = smdh.GetIcon(true);
        meta.icon_rgba = Rgb565ToRgba(std::vector<uint16_t>(icon.begin(), icon.end()));
    }
    return meta;
}

} // namespace frame_xr::picker
