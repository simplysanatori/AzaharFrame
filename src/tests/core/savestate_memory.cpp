// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame savemem: a save state written with little memory (the archive into one reserved
// string, compressed straight into the file) must be the same kind of file as before, read the
// same way by this build and by older ones; a file written the old way must still load; and the
// memory a save holds for a moment is counted as transient (the memory watch leaves it out).

#include <filesystem>
#include <fstream>
#include <istream>
#include <sstream>
#include <string>
#include <vector>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include <catch2/catch_test_macros.hpp>
#include <unistd.h>
#include "common/archives.h"
#include "common/atomic_file.h"
#include "common/scm_rev.h"
#include "common/transient_memory.h"
#include "common/zstd_compression.h"
#include "core/autosave.h"
#include "core/memory.h"
#include "core/savestate.h"

namespace fs = std::filesystem;
using namespace Core;

namespace {

constexpr u64 kTitle = 0x0004000000030200ULL;

struct TempDir {
    fs::path path;
    explicit TempDir(const char* name)
        : path(fs::temp_directory_path() /
               (std::string("azf_savemem_") + name + "_" + std::to_string(getpid()))) {
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    std::string File(const char* name) const {
        return (path / name).string();
    }
};

/// A stand-in for the system: some scalars, a big block of "guest memory" (as FCRAM's
/// make_binary_object), many small objects (as the page tables and the kernel's).
struct FakeSystem {
    u32 cores = 2;
    std::string name;
    std::vector<u8> fcram;
    std::vector<u32> small;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & cores;
        ar & name;
        ar & fcram;
        ar & small;
    }
    bool operator==(const FakeSystem&) const = default;
};

FakeSystem MakeSystem() {
    FakeSystem s;
    s.cores = 4;
    s.name = "Kid Icarus";
    s.fcram.resize(24u << 20);
    u32 x = 12345;
    for (std::size_t i = 0; i < s.fcram.size(); i++) {
        x = x * 1103515245u + 12345u;
        s.fcram[i] = (i % 4096 < 3000) ? 0 : static_cast<u8>(x >> 24); // mostly empty, as FCRAM
    }
    for (u32 i = 0; i < 200000; i++) {
        s.small.push_back(i * 7);
    }
    return s;
}

/// The archive as System::SerializeState makes it (StateWriteBuf).
std::string Serialize(const FakeSystem& s) {
    StateWriteBuf buf{ReserveForState(0)};
    {
        std::ostream stream{&buf};
        oarchive oa{stream};
        oa << s;
    }
    return std::move(buf.out);
}

std::string ReadAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

std::span<const u8> Bytes(const std::string& s) {
    return {reinterpret_cast<const u8*>(s.data()), s.size()};
}

} // namespace

TEST_CASE("Save state memory: a streamed save loads as before", "[savestate][savemem]") {
    TempDir dir("roundtrip");
    const FakeSystem system = MakeSystem();
    const std::string raw = Serialize(system);
    const auto header = CurrentStateHeader(kTitle, 1794000000);

    // This build's writer: compressed into the file as it is written
    const std::string path = dir.File("0004000000030200.00.cst");
    const std::size_t size = WriteStateFileFrom(path, header, Bytes(raw));
    const std::string file = ReadAll(path);
    REQUIRE(file.size() == size);
    REQUIRE(!fs::exists(path + ".tmp"));
    CHECK(file.size() < raw.size() / 2); // compressed

    // The header as before: this game, this build
    CHECK(CheckStateHeader(Bytes(file), kTitle, Common::g_scm_rev, false) == HeaderCheck::Ok);
    CHECK(std::equal(header.begin(), header.end(), file.begin()));
    const SaveStateInfo info = GetSaveStateInfoAt(path, kTitle);
    CHECK(info.slot == 0);
    CHECK(info.status == SaveStateInfo::ValidationStatus::OK);
    CHECK(info.time == 1794000000);

    // The body is one zstd frame with its content size: what every reader (this build's, older
    // builds', upstream Azahar's: GetDecompressedSize, then DecompressDataZSTD) needs
    const auto body = Bytes(file).subspan(256);
    CHECK(Common::Compression::GetDecompressedSize(body) == raw.size());
    std::vector<u8> decompressed = Common::Compression::DecompressDataZSTD(body);
    REQUIRE(decompressed.size() == raw.size());
    CHECK(std::equal(decompressed.begin(), decompressed.end(),
                     reinterpret_cast<const u8*>(raw.data())));

    // Read in place, as LoadStateAt now does (StateReadBuf)...
    {
        StateReadBuf buf{decompressed};
        std::istream stream{&buf};
        iarchive ia{stream};
        FakeSystem loaded;
        ia >> loaded;
        CHECK(loaded == system);
    }
    // ...and from an istringstream's copy, as older builds do
    {
        std::istringstream stream{
            std::string{reinterpret_cast<char*>(decompressed.data()), decompressed.size()},
            std::ios_base::binary};
        iarchive ia{stream};
        FakeSystem loaded;
        ia >> loaded;
        CHECK(loaded == system);
    }
}

TEST_CASE("Save state memory: a state written the old way still loads", "[savestate][savemem]") {
    TempDir dir("old");
    const FakeSystem system = MakeSystem();
    // integ11's System::SaveState: an ostringstream, then CompressDataZSTDDefault, WriteStateFile
    std::ostringstream sstream{std::ios_base::binary};
    {
        oarchive oa{sstream};
        oa << system;
    }
    const std::string old_raw = sstream.str();
    CHECK(old_raw == Serialize(system)); // the archive itself is the same bytes
    const auto compressed = Common::Compression::CompressDataZSTDDefault(Bytes(old_raw));
    const std::string path = dir.File("0004000000030200.01.cst");
    WriteStateFile(path, CurrentStateHeader(kTitle, 1794000000), compressed);

    const std::string file = ReadAll(path);
    REQUIRE(CheckStateHeader(Bytes(file), kTitle, Common::g_scm_rev, false) == HeaderCheck::Ok);
    std::vector<u8> decompressed = Common::Compression::DecompressDataZSTD(Bytes(file).subspan(256));
    StateReadBuf buf{decompressed};
    std::istream stream{&buf};
    iarchive ia{stream};
    FakeSystem loaded;
    ia >> loaded;
    CHECK(loaded == system);

    // The same zstd frame either way: a window and a content size every reader takes
    std::string streamed;
    REQUIRE(Common::Compression::CompressDataZSTDDefaultTo(Bytes(old_raw), [&](auto piece) {
        streamed.append(reinterpret_cast<const char*>(piece.data()), piece.size());
        return true;
    }));
    CHECK(Common::Compression::GetDecompressedSize(Bytes(streamed)) == old_raw.size());
    const auto again = Common::Compression::DecompressDataZSTD(Bytes(streamed));
    CHECK(std::equal(again.begin(), again.end(), reinterpret_cast<const u8*>(old_raw.data()),
                     reinterpret_cast<const u8*>(old_raw.data()) + old_raw.size()));
}

TEST_CASE("Save state memory: a failed write leaves the slot as it was", "[savestate][savemem]") {
    TempDir dir("fail");
    const std::string path = dir.File("slot.cst");
    std::ofstream(path, std::ios::binary) << "the old state";
    const std::string raw(1 << 20, 'x');
    {
        // Not committed (a throw, a crash before the rename): the old file stays, no .tmp
        Common::AtomicFileWriter w{path};
        REQUIRE(w.Ok());
        REQUIRE(w.Write("half a state"));
    }
    CHECK(ReadAll(path) == "the old state");
    CHECK(!fs::exists(path + ".tmp"));
    // The sink refusing stops the compression
    CHECK(!Common::Compression::CompressDataZSTDDefaultTo(Bytes(raw),
                                                          [](auto) { return false; }));
    // A folder that cannot be made: an exception, nothing left behind
    std::ofstream(dir.File("blocked")) << "file";
    CHECK_THROWS(WriteStateFileFrom(dir.File("blocked") + "/x.cst",
                                    CurrentStateHeader(kTitle, 1), Bytes(raw)));
    // A good one replaces it whole
    WriteStateFileFrom(path, CurrentStateHeader(kTitle, 2), Bytes(raw));
    CHECK(GetSaveStateInfoAt(path, kTitle).time == 2);
}

TEST_CASE("Save state memory: reserved up front, counted while held", "[savestate][savemem]") {
    constexpr std::size_t kGuest = Memory::FCRAM_N3DS_SIZE + Memory::VRAM_SIZE +
                                   Memory::N3DS_EXTRA_RAM_SIZE + Memory::DSP_RAM_SIZE;
    CHECK(ReserveForState(0) >= kGuest + (64u << 20));      // the first save: no doubling
    CHECK(ReserveForState(321110469) >= 321110469 + (32u << 20)); // Kid Icarus's 306 MiB
    CHECK(ReserveForState(900u << 20) >= (932u << 20));     // a bigger one: its own size

    namespace TM = Common::TransientMemory;
    const auto base = TM::Bytes();
    const auto holds = TM::Holds();
    {
        StateWriteBuf buf{1 << 20};
        CHECK(TM::Holds() == holds + 1); // a save is under way
        std::ostream stream{&buf};
        const std::string chunk(1 << 20, 'a');
        for (int i = 0; i < 10; i++) {
            stream.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        }
        CHECK(TM::Bytes() >= base + (8u << 20)); // counted as it grows (4 MiB steps)
        CHECK(TM::Bytes() <= base + (10u << 20));
        TM::Hold moved{std::uint64_t{5} << 20};
        TM::Hold to{std::move(moved)};
        CHECK(TM::Holds() == holds + 2);
    }
    CHECK(TM::Bytes() == base);
    CHECK(TM::Holds() == holds);
}

TEST_CASE("Save state memory: a slot's save on the writer", "[savestate][savemem]") {
    namespace A = Core::Autosave;
    namespace TM = Common::TransientMemory;
    TempDir dir("writer");
    const auto base = TM::Bytes();
    const std::string raw = Serialize(MakeSystem());

    A::Writer w;
    int told = 0;
    bool ok = false;
    A::Writer::Done seen;
    const auto job = [&](const std::string& path) {
        A::Writer::Job j;
        j.path = path;
        j.header = CurrentStateHeader(kTitle, 1794000000);
        j.hold = std::make_shared<TM::Hold>(raw.size());
        j.raw = raw;
        j.autosave = false;
        j.slot_save = true;
        j.slot = 3;
        j.program_id = kTitle;
        j.then = [&](const A::Writer::Done& d) {
            told++;
            ok = d.ok;
            seen = d;
        };
        return j;
    };
    w.Submit(job(dir.File("0004000000030200.03.cst")));
    w.WaitIdle();
    CHECK(told == 1);
    CHECK(ok);
    CHECK(seen.slot_save);
    CHECK(seen.slot == 3);
    CHECK(!seen.autosave);
    CHECK(seen.raw_bytes == raw.size());
    CHECK(TM::Bytes() == base); // let go once written
    const SaveStateInfo info = GetSaveStateInfoAt(dir.File("0004000000030200.03.cst"), kTitle);
    CHECK(info.slot == 0);
    CHECK(info.time == 1794000000);

    // A write that fails is told too, with why
    std::ofstream(dir.File("blocked")) << "file";
    w.Submit(job(dir.File("blocked") + "/0004000000030200.03.cst"));
    w.WaitIdle();
    CHECK(told == 2);
    CHECK(!ok);
    CHECK(seen.failure == A::Failure::Write);
    CHECK(TM::Bytes() == base);
}
