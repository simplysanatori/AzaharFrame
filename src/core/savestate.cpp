// Copyright 2020-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the LICENSE.txt file included.

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <optional>
#include <sstream>
#include <cryptopp/hex.h>
#include <fmt/ranges.h>
#include "common/archives.h"
#include "common/atomic_file.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/swap.h"
#include "common/transient_memory.h"
#include "common/zstd_compression.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "core/memory.h"
#include "core/movie.h"
#include "core/savestate.h"
#include "network/network.h"

namespace Core {

#pragma pack(push, 1)
struct CSTHeader {
    std::array<u8, 4> filetype{};    /// Unique Identifier to check the file type (always "CST"0x1B)
    u64_le program_id{};             /// ID of the ROM being executed. Also called title_id
    std::array<u8, 20> revision{};   /// Git hash of the revision this savestate was created with
    u64_le time{};                   /// The time when this save state was created
    std::array<u8, 20> build_name{}; /// The build name (Canary/Nightly) with the version number
    u32_le zero{};                   /// Should be zero, just in case.
    std::array<u8, 20> build_version{}; /// Latest build version, used as compatibility.
    u32_le zero_2{};                    /// Should be zero, just in case.

    std::array<u8, 168> reserved{}; /// Make heading 256 bytes so it has consistent size
};
static_assert(sizeof(CSTHeader) == 256, "CSTHeader should be 256 bytes");
#pragma pack(pop)

constexpr std::array<u8, 4> header_magic_bytes{{'C', 'S', 'T', 0x1B}};

// AzaharFrame savestate: the states folder is a parameter (StatePathIn), so the slot list can be
// tested in a folder of its own
std::string StatePathIn(const std::string& states_dir, u64 program_id, u64 movie_id, u32 slot) {
    if (movie_id) {
        return fmt::format("{}{:016X}.movie{:016X}.{:02d}.cst", states_dir, program_id, movie_id,
                           slot);
    } else {
        return fmt::format("{}{:016X}.{:02d}.cst", states_dir, program_id, slot);
    }
}

static std::string GetSaveStatePath(u64 program_id, u64 movie_id, u32 slot) {
    return StatePathIn(FileUtil::GetUserPath(FileUtil::UserPath::StatesDir), program_id, movie_id,
                       slot);
}

static bool ValidateSaveState(const CSTHeader& header, SaveStateInfo& info, u64 program_id,
                              const std::string& path) {
    if (header.filetype != header_magic_bytes) {
        LOG_WARNING(Core, "Invalid save state file {}", path);
        return false;
    }
    info.time = header.time;

    if (header.program_id != program_id) {
        LOG_WARNING(Core, "Save state file isn't for the current game {}", path);
        return false;
    }
    const std::string revision = fmt::format("{:02x}", fmt::join(header.revision, ""));
    const std::string build_name =
        header.zero == 0 ? reinterpret_cast<const char*>(header.build_name.data()) : "";
    const std::string build_version =
        header.zero_2 == 0 ? reinterpret_cast<const char*>(header.build_version.data()) : "";

    if (revision == Common::g_scm_rev) {
        info.status = SaveStateInfo::ValidationStatus::OK;
    } else {
        info.build_name = build_name;
        info.build_version = build_version;

        info.status = Common::g_build_version == info.build_version
                          ? SaveStateInfo::ValidationStatus::RevisionMismatch
                          : SaveStateInfo::ValidationStatus::BuildMismatch;
    }
    return true;
}

// ---- AzaharFrame savestate (savestate.h) ----

namespace {

/// "1.2.0" as numbers ({1, 2, 0}); a "-rc1" style suffix is left out; nullopt when not x.y.z.
std::optional<std::array<u32, 3>> ParseVersion(std::string_view v) {
    std::array<u32, 3> out{};
    std::size_t at = 0;
    for (std::size_t i = 0; i < 3; i++) {
        if (i > 0) {
            if (at >= v.size() || v[at] != '.') {
                return std::nullopt;
            }
            at++;
        }
        const std::size_t from = at;
        u32 n = 0;
        while (at < v.size() && v[at] >= '0' && v[at] <= '9' && at - from < 6) {
            n = n * 10 + static_cast<u32>(v[at] - '0');
            at++;
        }
        if (at == from) {
            return std::nullopt;
        }
        out[i] = n;
    }
    if (at != v.size() && v[at] != '-') {
        return std::nullopt;
    }
    return out;
}

} // namespace

// AzaharFrame savemem
StateReadBuf::StateReadBuf(std::span<u8> bytes) {
    char* begin = reinterpret_cast<char*>(bytes.data());
    setg(begin, begin, begin + bytes.size());
}

StateWriteBuf::StateWriteBuf(std::size_t reserve) : hold(0) {
    out.reserve(reserve);
}

std::streamsize StateWriteBuf::xsputn(const char* s, std::streamsize n) {
    out.append(s, static_cast<std::size_t>(n));
    if (out.size() - counted >= (4u << 20)) {
        counted = out.size();
        hold.Set(counted);
    }
    return n;
}

Common::TransientMemory::Hold StateWriteBuf::TakeHold() {
    hold.Set(out.size());
    return std::move(hold);
}

StateWriteBuf::int_type StateWriteBuf::overflow(int_type c) {
    if (!traits_type::eq_int_type(c, traits_type::eof())) {
        out.push_back(traits_type::to_char_type(c));
    }
    return traits_type::not_eof(c);
}

// AzaharFrame savemem
std::size_t RemoveStaleStateTemps(const std::string& states_dir, u64 program_id) {
    namespace fs = std::filesystem;
    const std::string prefix = fmt::format("{:016X}.", program_id);
    constexpr std::string_view suffix = ".cst.tmp";
    std::size_t removed = 0;
    std::error_code ec;
    for (fs::directory_iterator it{states_dir, ec}, end; !ec && it != end; it.increment(ec)) {
        const std::string name = it->path().filename().string();
        if (name.size() > prefix.size() + suffix.size() && name.starts_with(prefix) &&
            name.ends_with(suffix) && it->is_regular_file(ec)) {
            std::error_code rm;
            if (fs::remove(it->path(), rm)) {
                LOG_INFO(Core, "Removed {}: a save state's write cut short", name);
                removed++;
            }
        }
    }
    return removed;
}

// AzaharFrame savemem
std::size_t ReserveForState(std::size_t last_size) {
    // All the guest memory a state can hold (Memory::MemorySystem::serialize: FCRAM at its New
    // 3DS size, VRAM, the New 3DS extra RAM, DSP RAM), and room for the rest: the page tables
    // (several MiB each), the kernel's objects, the services, the GPU's registers.
    constexpr std::size_t kGuest = Memory::FCRAM_N3DS_SIZE + Memory::VRAM_SIZE +
                                   Memory::N3DS_EXTRA_RAM_SIZE + Memory::DSP_RAM_SIZE;
    return std::max(last_size + (32u << 20), kGuest + (96u << 20));
}

std::string FrameVersionOf(std::string_view build_name, std::string_view build_version) {
    if (build_version.size() > kFrameBuildTag.size() &&
        build_version.substr(0, kFrameBuildTag.size()) == kFrameBuildTag) {
        return std::string(build_version.substr(kFrameBuildTag.size()));
    }
    // Before 1.2.0 only build_name was written: one of our releases is "1.<minor>.<patch>".
    const auto v = ParseVersion(build_name);
    if (v && (*v)[0] == 1) {
        return std::string(build_name);
    }
    return {};
}

std::string SaveStateMismatchTitle(std::string_view version, std::string_view ours) {
    const auto a = ParseVersion(version);
    const auto b = ParseVersion(ours);
    if (a && b && *a != *b) {
        return *a < *b ? "This Save State Is From an Older Version"
                       : "This Save State Is From a Newer Version";
    }
    return "This Save State Is From Another Version";
}

std::string SaveStateMismatchBody(std::string_view version) {
    const std::string made = version.empty()
                                 ? std::string("It was made with another version of Azahar.")
                                 : "It was made with Azahar for Frame " + std::string(version) + ".";
    return made + " It may not load, and it could corrupt this game's progress. Save states are "
                  "NOT a replacement for saving in the game.";
}

std::string SaveStateFromText(std::string_view version) {
    return version.empty() ? std::string("From Another Version") : "From " + std::string(version);
}

std::array<u8, 256> MakeStateHeader(u64 program_id, std::string_view revision_hex,
                                    std::string_view build_name, std::string_view build_version,
                                    u64 time) {
    CSTHeader header{};
    header.filetype = header_magic_bytes;
    header.program_id = program_id;
    std::string rev_bytes;
    CryptoPP::StringSource ss(std::string(revision_hex), true,
                              new CryptoPP::HexDecoder(new CryptoPP::StringSink(rev_bytes)));
    std::memcpy(header.revision.data(), rev_bytes.data(),
                std::min(rev_bytes.size(), sizeof(header.revision)));
    header.time = time;
    std::memcpy(header.build_name.data(), build_name.data(),
                std::min(build_name.size(), sizeof(header.build_name) - 1));
    std::memcpy(header.build_version.data(), build_version.data(),
                std::min(build_version.size(), sizeof(header.build_version) - 1));
    std::array<u8, 256> out{};
    std::memcpy(out.data(), &header, sizeof(header));
    return out;
}

std::array<u8, 256> CurrentStateHeader(u64 program_id, u64 time) {
    // build_version "AZF 1.2.0", so a later build can name the version
    const std::string build_fullname = Common::g_build_fullname;
    return MakeStateHeader(program_id, Common::g_scm_rev, build_fullname,
                           std::string(kFrameBuildTag) + build_fullname, time);
}

void WriteStateFile(const std::string& path, std::span<const u8> header,
                    std::span<const u8> body) {
    // AzaharFrame advfix: atomically, as the autosaves are (common/atomic_file.h): <path>.tmp is
    // written and fsynced, then renamed over the slot. A failed write (a full disk, a crash or a
    // freeze mid-write) leaves the slot as it was; a slot saved over is replaced, header and all.
    std::string file;
    file.reserve(header.size() + body.size());
    file.append(reinterpret_cast<const char*>(header.data()), header.size());
    file.append(reinterpret_cast<const char*>(body.data()), body.size());
    if (!Common::WriteFileAtomic(path, file)) {
        throw std::runtime_error("Could not write " + path);
    }
}

// AzaharFrame savemem
std::size_t WriteStateFileFrom(const std::string& path, std::span<const u8> header,
                               std::span<const u8> raw) {
    Common::AtomicFileWriter file{path};
    const auto put = [&file](std::span<const u8> bytes) {
        return file.Write(
            std::string_view{reinterpret_cast<const char*>(bytes.data()), bytes.size()});
    };
    if (!put(header) || !Common::Compression::CompressDataZSTDDefaultTo(raw, put) ||
        !file.Commit()) {
        throw std::runtime_error("Could not write " + path);
    }
    return file.Written();
}

// AzaharFrame advfix
std::string SaveFailedBody(SaveFailure failure) {
    switch (failure) {
    case SaveFailure::NoSpace:
        return std::string("Not Enough Space\n") + kSaveFailedBody;
    case SaveFailure::Write:
        return std::string("Couldn't Write the File\n") + kSaveFailedBody;
    case SaveFailure::Unknown:
        break;
    }
    return kSaveFailedBody;
}

SaveFailure WriteFailureAt(const std::string& path, std::size_t needed) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::path(path).parent_path();
    while (!dir.empty() && !fs::exists(dir, ec) && dir != dir.parent_path()) {
        dir = dir.parent_path(); // a folder not made yet: the free space where it would be
    }
    const fs::space_info space = fs::space(dir.empty() ? fs::path(".") : dir, ec);
    if (!ec && space.available < needed) {
        return SaveFailure::NoSpace;
    }
    return SaveFailure::Write;
}

HeaderCheck CheckStateHeader(std::span<const u8> data, u64 program_id,
                             std::string_view revision_hex, bool allow_mismatch) {
    if (data.size() < sizeof(CSTHeader)) {
        return HeaderCheck::NotAState;
    }
    CSTHeader header;
    std::memcpy(&header, data.data(), sizeof(header));
    if (header.filetype != header_magic_bytes) {
        return HeaderCheck::NotAState;
    }
    if (header.program_id != program_id) {
        return HeaderCheck::OtherGame;
    }
    const std::string revision = fmt::format("{:02x}", fmt::join(header.revision, ""));
    if (revision != revision_hex && !allow_mismatch) {
        return HeaderCheck::OtherBuild;
    }
    return HeaderCheck::Ok;
}

void RestoreGuarded(const std::function<void()>& restore) {
    try {
        restore();
    } catch (const std::exception& e) {
        LOG_ERROR(Core, "Save state not read: {}", e.what());
        throw SaveStateHalfLoaded();
    } catch (...) {
        LOG_ERROR(Core, "Save state not read: an unknown error");
        throw SaveStateHalfLoaded();
    }
}

std::vector<SaveStateInfo> ListSaveStates(u64 program_id, u64 movie_id) {
    return ListSaveStatesIn(FileUtil::GetUserPath(FileUtil::UserPath::StatesDir), program_id,
                            movie_id);
}

// AzaharFrame savestate: reads every slot's header from its file, each time (nothing is kept), so a
// slot saved over shows the new state's build at once
std::vector<SaveStateInfo> ListSaveStatesIn(const std::string& states_dir, u64 program_id,
                                            u64 movie_id) {
    std::vector<SaveStateInfo> result;
    result.reserve(SaveStateSlotCount);
    for (u32 slot = 0; slot <= SaveStateSlotCount; ++slot) {
        const auto path = StatePathIn(states_dir, program_id, movie_id, slot);
        if (!FileUtil::Exists(path)) {
            continue;
        }

        SaveStateInfo info;
        info.slot = slot;

        FileUtil::IOFile file(path, "rb");
        if (!file) {
            LOG_ERROR(Core, "Could not open file {}", path);
            continue;
        }
        CSTHeader header;
        if (file.GetSize() < sizeof(header)) {
            LOG_ERROR(Core, "File too small {}", path);
            continue;
        }
        if (file.ReadBytes(&header, sizeof(header)) != sizeof(header)) {
            LOG_ERROR(Core, "Could not read from file {}", path);
            continue;
        }
        if (!ValidateSaveState(header, info, program_id, path)) {
            continue;
        }

        result.emplace_back(std::move(info));
    }
    return result;
}

SaveStateInfo GetSaveStateInfo(u64 program_id, u64 movie_id, u32 slot) {
    return GetSaveStateInfoIn(FileUtil::GetUserPath(FileUtil::UserPath::StatesDir), program_id,
                              movie_id, slot);
}

SaveStateInfo GetSaveStateInfoIn(const std::string& states_dir, u64 program_id, u64 movie_id,
                                 u32 slot) {
    SaveStateInfo info =
        GetSaveStateInfoAt(StatePathIn(states_dir, program_id, movie_id, slot), program_id);
    if (info.slot == 0) {
        info.slot = slot;
    }
    return info;
}

// AzaharFrame autosave: any state file (an autosave's), slot 0 when it is one of this game
SaveStateInfo GetSaveStateInfoAt(const std::string& path, u64 program_id) {
    SaveStateInfo info{};
    info.slot = std::numeric_limits<u32>::max();

    if (!FileUtil::Exists(path)) {
        return info;
    }

    FileUtil::IOFile file(path, "rb");
    if (!file) {
        LOG_ERROR(Core, "Could not open file {}", path);
        return info;
    }
    CSTHeader header;
    if (file.GetSize() < sizeof(header)) {
        LOG_ERROR(Core, "File too small {}", path);
        return info;
    }
    if (file.ReadBytes(&header, sizeof(header)) != sizeof(header)) {
        LOG_ERROR(Core, "Could not read from file {}", path);
        return info;
    }
    if (ValidateSaveState(header, info, program_id, path)) {
        info.slot = 0;
    }
    return info;
}

void System::SaveState(u32 slot) const {
    // AzaharFrame savemem: the serialized system in one string reserved up front (SerializeState),
    // compressed straight into the file as it is written (WriteStateFileFrom). Before, an
    // ostringstream doubled its buffer as it grew, str() copied it, and CompressDataZSTD held a
    // zero-filled buffer of about the same size again: about 3x the state (0.9 GB for 306 MB)
    // for a moment, against about 1x now. The file is the same: the header, then one zstd frame
    // at the same level with the content size.
    Common::TransientMemory::Hold hold;
    std::string raw = SerializeState(&hold);

    const u64 movie_id = movie.GetCurrentMovieID();
    const auto path = GetSaveStatePath(title_id, movie_id, slot);
    // AzaharFrame advfix: a write that fails says why, when it can (the notice's reason)
    const std::size_t needed = raw.size() / 8 + 256; // about what it compresses to, and more
    if (!FileUtil::CreateFullPath(path)) {
        throw SaveStateWriteFailed(WriteFailureAt(path, needed), "Could not create path " + path);
    }

    try {
        WriteStateFileFrom(
            path,
            CurrentStateHeader(
                title_id, static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count())),
            std::span<const u8>{reinterpret_cast<const u8*>(raw.data()), raw.size()});
    } catch (const std::exception& e) {
        throw SaveStateWriteFailed(WriteFailureAt(path, needed), e.what());
    }
}

std::string System::SaveStatePath(u32 slot) const {
    return GetSaveStatePath(title_id, movie.GetCurrentMovieID(), slot);
}

void System::LoadState(u32 slot, bool allow_mismatch) {
    LoadStateAt(GetSaveStatePath(title_id, movie.GetCurrentMovieID(), slot), allow_mismatch);
}

// AzaharFrame autosave: the system serialized, not compressed (the autosave's writer compresses
// it on its own thread)
std::string System::SerializeState(Common::TransientMemory::Hold* hold) const {
    if (app_loader && !app_loader->SupportsSaveStates()) {
        throw std::runtime_error("Savestates are not supported by the current application "
                                 "loader (Artic Base).");
    }
    // AzaharFrame autosave: into one string reserved at the last state's size (an ostringstream
    // doubles its buffer and copies the state again each time, ~300 MB for a big game).
    // AzaharFrame savemem: reserved generously: a reservation is only address space until it is
    // written, so the first save of a run (no last size yet) reserves the most a state can be
    // (KiB to MiB of kernel and service objects on top of all of the guest memory) instead of
    // growing by doubling, which copied the state at each step. The bytes written so far count
    // as transient memory for the memory watch (common/transient_memory.h).
    static std::atomic<std::size_t> last_size{0};
    StateWriteBuf sink{ReserveForState(last_size.load())};
    {
        std::ostream stream{&sink};
        oarchive oa{stream};
        oa&* this;
    }
    last_size = sink.out.size();
    if (hold) {
        *hold = sink.TakeHold(); // AzaharFrame savemem: counted on, with no gap
    }
    return std::move(sink.out);
}

void System::LoadStateAt(const std::string& path, bool allow_mismatch) {
    if (app_loader) {
        if (!app_loader->SupportsSaveStates()) {
            throw std::runtime_error("Savestates are not supported by the current application "
                                     "loader (Artic Base)");
        }
    }
    auto room_member = Network::GetRoomMember().lock();
    if (room_member && room_member->IsConnected()) {
        throw std::runtime_error("Unable to load while connected to multiplayer");
    }

    std::vector<u8> decompressed;
    {
        std::vector<u8> buffer(FileUtil::GetSize(path) - sizeof(CSTHeader));

        FileUtil::IOFile file(path, "rb");

        // load header
        CSTHeader header;
        if (file.ReadBytes(&header, sizeof(header)) != sizeof(header)) {
            throw std::runtime_error("Could not read from file at " + path);
        }

        // validate header
        SaveStateInfo info;
        info.slot = 0;
        // AzaharFrame savestate: strict (this build's revision) unless Load Anyway; the file type
        // and the game are checked either way
        if (!ValidateSaveState(header, info, title_id, path) ||
            (!allow_mismatch && info.status != SaveStateInfo::ValidationStatus::OK)) {
            throw std::runtime_error("Invalid savestate");
        }

        if (file.ReadBytes(buffer.data(), buffer.size()) != buffer.size()) {
            throw std::runtime_error("Could not read from file at " + path);
        }
        decompressed = Common::Compression::DecompressDataZSTD(buffer);
        if (decompressed.empty()) { // nothing read into the system yet: the game goes on
            throw std::runtime_error(kSaveStateNotLoaded);
        }
    }
    // AzaharFrame savemem: read in place (StateReadBuf), not from an istringstream's copy of it
    // (the decompressed state twice in memory while it was read)
    Common::TransientMemory::Hold hold{decompressed.size()};
    StateReadBuf buf{decompressed};
    std::istream stream{&buf};

    // Deserialize (AzaharFrame savestate: a failure here leaves the system half-restored)
    RestoreGuarded([&] {
        iarchive ia{stream};
        ia&* this;
    });
}

std::vector<u8> System::SaveStateBuffer() const {
    // AzaharFrame savemem: serialized as SaveState does (no stream copy), compressed into the
    // result after its header (no buffer of the uncompressed size)
    Common::TransientMemory::Hold hold;
    std::string raw = SerializeState(&hold);
    std::vector<u8> buffer;
    buffer.reserve(raw.size() / 8);
    if (!Common::Compression::CompressDataZSTDDefaultTo(
            std::span<const u8>{reinterpret_cast<const u8*>(raw.data()), raw.size()},
            [&buffer](std::span<const u8> piece) {
                buffer.insert(buffer.end(), piece.begin(), piece.end());
                return true;
            })) {
        buffer.clear();
    }
    std::string().swap(raw);

    CSTHeader header{};
    header.filetype = header_magic_bytes;
    header.program_id = title_id;
    std::string rev_bytes;
    CryptoPP::StringSource ss(Common::g_scm_rev, true,
                              new CryptoPP::HexDecoder(new CryptoPP::StringSink(rev_bytes)));
    std::memcpy(header.revision.data(), rev_bytes.data(), sizeof(header.revision));
    header.time = std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    const std::string build_fullname = Common::g_build_fullname;
    std::memset(header.build_name.data(), 0, sizeof(header.build_name));
    std::memcpy(header.build_name.data(), build_fullname.c_str(),
                std::min(build_fullname.length(), sizeof(header.build_name) - 1));

    const std::string build_version = Common::g_build_version;
    std::memset(header.build_version.data(), 0, sizeof(header.build_version));
    std::memcpy(header.build_version.data(), build_version.c_str(),
                std::min(build_version.length(), sizeof(header.build_version) - 1));

    std::vector<u8> result((u8*)&header, (u8*)&header + sizeof(header));
    std::copy(buffer.begin(), buffer.end(), std::back_inserter(result));

    return result;
}

bool System::LoadStateBuffer(std::vector<u8> buffer, bool allow_mismatch) {
    // AzaharFrame savestate: the checks as LoadState's (CheckStateHeader), the revision skipped
    // only when asked
    switch (CheckStateHeader(buffer, title_id, Common::g_scm_rev, allow_mismatch)) {
    case HeaderCheck::Ok:
        break;
    case HeaderCheck::NotAState:
        LOG_ERROR(Core, "Invalid save state");
        return false;
    case HeaderCheck::OtherGame:
        LOG_ERROR(Core, "Save state isn't for the current game");
        return false;
    case HeaderCheck::OtherBuild:
        LOG_ERROR(Core, "Save state file created from a different revision (core: {})",
                  Common::g_scm_rev);
        return false;
    }

    // AzaharFrame savemem: decompressed from the buffer in place, read in place (no copies)
    auto decompressed = Common::Compression::DecompressDataZSTD(
        std::span<const u8>{buffer}.subspan(sizeof(CSTHeader)));
    std::vector<u8>().swap(buffer);
    if (decompressed.empty()) {
        LOG_ERROR(Core, "Save state not decompressed");
        return false;
    }
    Common::TransientMemory::Hold hold{decompressed.size()};
    StateReadBuf buf{decompressed};
    std::istream sstream{&buf};

    // Deserialize. A failure partway leaves the system half-restored: the caller is told (false)
    // and must not run it on; never an exception out of here.
    try {
        RestoreGuarded([&] {
            iarchive ia{sstream};
            ia&* this;
        });
    } catch (const SaveStateHalfLoaded&) {
        return false;
    }
    return true;
}

} // namespace Core
