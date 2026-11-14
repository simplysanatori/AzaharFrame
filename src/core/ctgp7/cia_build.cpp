// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ctgp7: see cia_build.h. Layouts from 3dbrew (CIA, Ticket, Title metadata, NCSD,
// NCCH) and Azahar's own readers (core/file_sys/cia_container.cpp, ticket.cpp, title_metadata.cpp).

#include "core/ctgp7/cia_build.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>
#include <cryptopp/sha.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/file_derived.h"

namespace Ctgp7::Cia {

namespace {

constexpr u32 kSigRsa2048Sha256 = 0x00010004;
constexpr std::size_t kSigBlock = 0x140; // type, 0x100 signature, padding to 0x40
constexpr std::size_t kTicketBody = 0x164;
constexpr std::size_t kTmdBody = 0xC4 + 0x24 * 64;
constexpr std::size_t kChunk = 0x30;
constexpr std::size_t kCiaHeader = 0x2020;
constexpr u64 kMaxHeadRead = 16ULL << 20;

constexpr u64 Align64(u64 x) {
    return (x + 63) & ~u64{63};
}

u16 Le16(std::span<const u8> b, std::size_t at) {
    return static_cast<u16>(b[at] | (b[at + 1] << 8));
}
u32 Le32(std::span<const u8> b, std::size_t at) {
    return static_cast<u32>(b[at]) | (static_cast<u32>(b[at + 1]) << 8) |
           (static_cast<u32>(b[at + 2]) << 16) | (static_cast<u32>(b[at + 3]) << 24);
}
u64 Le64(std::span<const u8> b, std::size_t at) {
    return static_cast<u64>(Le32(b, at)) | (static_cast<u64>(Le32(b, at + 4)) << 32);
}
void PutBe16(std::vector<u8>& b, std::size_t at, u16 v) {
    b[at] = static_cast<u8>(v >> 8);
    b[at + 1] = static_cast<u8>(v);
}
void PutBe32(std::vector<u8>& b, std::size_t at, u32 v) {
    for (int i = 0; i < 4; i++) {
        b[at + i] = static_cast<u8>(v >> (24 - 8 * i));
    }
}
void PutBe64(std::vector<u8>& b, std::size_t at, u64 v) {
    PutBe32(b, at, static_cast<u32>(v >> 32));
    PutBe32(b, at + 4, static_cast<u32>(v));
}
void PutLe32(std::vector<u8>& b, std::size_t at, u32 v) {
    for (int i = 0; i < 4; i++) {
        b[at + i] = static_cast<u8>(v >> (8 * i));
    }
}
void PutLe64(std::vector<u8>& b, std::size_t at, u64 v) {
    PutLe32(b, at, static_cast<u32>(v));
    PutLe32(b, at + 4, static_cast<u32>(v >> 32));
}
void PutText(std::vector<u8>& b, std::size_t at, const char* s) {
    std::memcpy(&b[at], s, std::strlen(s));
}

std::array<u8, 32> Sha256(std::span<const u8> data) {
    std::array<u8, 32> out{};
    CryptoPP::SHA256().CalculateDigest(out.data(), data.data(), data.size());
    return out;
}

/// An NCCH header at `at`: fills the title, version, size, crypto and save size.
bool ReadNcch(std::span<const u8> b, u64 at, Source& s) {
    if (at + 0x200 > b.size() || std::memcmp(&b[at + 0x100], "NCCH", 4) != 0) {
        return false;
    }
    const auto h = b.subspan(static_cast<std::size_t>(at));
    s.version = Le16(h, 0x112);
    s.title_id = Le64(h, 0x118);
    const u8 unit_exp = h[0x188 + 6];
    s.encrypted = (h[0x188 + 7] & 0x04) == 0; // NoCrypto
    const u64 unit = 0x200ULL << std::min<u8>(unit_exp, 8);
    const u64 declared = static_cast<u64>(Le32(h, 0x104)) * unit;
    if (declared != 0) {
        s.size = declared;
    }
    const u32 exheader_size = Le32(h, 0x180);
    if (!s.encrypted && exheader_size >= 0x200 && at + 0x200 + 0x1C8 <= b.size()) {
        s.save_size = static_cast<u32>(std::min<u64>(Le64(h, 0x200 + 0x1C0), 0xFFFFFFFFULL));
    }
    return true;
}

} // namespace

Source InspectBytes(std::span<const u8> head, u64 file_size) {
    Source s;
    if (head.size() >= 0x200 && std::memcmp(&head[0x100], "NCSD", 4) == 0) {
        s.kind = Kind::Ncsd;
        const u64 unit = 0x200ULL << std::min<u8>(head[0x188 + 6], 8);
        s.offset = static_cast<u64>(Le32(head, 0x120)) * unit;
        const u64 length = static_cast<u64>(Le32(head, 0x124)) * unit;
        if (s.offset == 0 || length == 0 || s.offset + length > file_size) {
            s.error = "The Cartridge Image Is Incomplete";
            return s;
        }
        if (!ReadNcch(head, s.offset, s)) {
            s.error = "The Cartridge Image Has No Game";
            return s;
        }
        s.size = length; // the partition as the card lays it out
    } else if (head.size() >= 0x200 && std::memcmp(&head[0x100], "NCCH", 4) == 0) {
        s.kind = Kind::Ncch;
        s.offset = 0;
        s.size = file_size;
        ReadNcch(head, 0, s);
        if (s.size > file_size) {
            s.error = "The Game File Is Incomplete";
            return s;
        }
    } else if (head.size() >= 0x20 && Le32(head, 0) == kCiaHeader) {
        s.kind = Kind::Cia;
        // its TMD: the title and version
        const u64 cert = Le32(head, 0x08), tik = Le32(head, 0x0C), tmd = Le32(head, 0x10);
        const u64 tmd_at = Align64(kCiaHeader) + Align64(cert) + Align64(tik);
        if (tmd < kSigBlock + 0xA0 || tmd_at + kSigBlock + 0xA0 > head.size()) {
            s.error = "The CIA Can't Be Read";
            return s;
        }
        const auto b = head.subspan(static_cast<std::size_t>(tmd_at + kSigBlock));
        s.title_id = 0;
        for (int i = 0; i < 8; i++) {
            s.title_id = (s.title_id << 8) | b[0x4C + i];
        }
        s.version = static_cast<u16>((b[0x9C] << 8) | b[0x9D]);
        s.size = file_size;
        return s;
    } else {
        s.error = "Not a 3DS Game Image";
        return s;
    }
    if (s.encrypted) {
        s.error = "This Copy Is Encrypted";
    }
    return s;
}

namespace {
std::unique_ptr<FileUtil::IOFileBase> OpenImage(const std::string& path) {
    std::unique_ptr<FileUtil::IOFileBase> f = std::make_unique<FileUtil::IOFile>(path, "rb");
    if (!f->IsOpen()) {
        return nullptr;
    }
    if (FileUtil::Z3DSReadIOFile::GetUnderlyingFileMagic(f.get()) != std::nullopt) {
        f = std::make_unique<FileUtil::Z3DSReadIOFile>(std::move(f));
    }
    return f;
}

bool ReadAt(FileUtil::IOFileBase& f, u64 at, u8* out, std::size_t n) {
    return f.Seek(static_cast<s64>(at), SEEK_SET) && f.ReadBytes(out, n) == n;
}
} // namespace

Source Inspect(const std::string& path) {
    auto f = OpenImage(path);
    Source s;
    if (!f) {
        s.error = "The Game File Can't Be Opened";
        return s;
    }
    const u64 size = f->GetSize();
    std::vector<u8> head(static_cast<std::size_t>(std::min<u64>(size, 0x10000)));
    if (!ReadAt(*f, 0, head.data(), head.size())) {
        s.error = "The Game File Can't Be Read";
        return s;
    }
    if (head.size() >= 0x200 && std::memcmp(&head[0x100], "NCSD", 4) == 0) {
        // partition 0's headers, wherever they are
        const u64 unit = 0x200ULL << std::min<u8>(head[0x188 + 6], 8);
        const u64 p0 = static_cast<u64>(Le32(head, 0x120)) * unit;
        if (p0 + 0x400 > head.size() && p0 + 0x400 <= std::min(size, kMaxHeadRead)) {
            std::vector<u8> more(static_cast<std::size_t>(p0 + 0x400));
            std::copy(head.begin(), head.end(), more.begin());
            if (!ReadAt(*f, p0, &more[static_cast<std::size_t>(p0)], 0x400)) {
                s.error = "The Game File Can't Be Read";
                return s;
            }
            head = std::move(more);
        }
    }
    return InspectBytes(head, size);
}

std::vector<u8> MakeTicket(u64 title_id, u16 version) {
    std::vector<u8> t(kSigBlock + kTicketBody + 0xAC, 0);
    PutBe32(t, 0, kSigRsa2048Sha256);
    const std::size_t b = kSigBlock;
    PutText(t, b + 0x00, "Root-CA00000003-XS0000000c");
    t[b + 0x7C] = 1;                   // format version
    PutBe64(t, b + 0x90, 0);           // ticket id
    PutBe32(t, b + 0x98, 0);           // console id: a common ticket
    PutBe64(t, b + 0x9C, title_id);
    PutBe16(t, b + 0xA6, version);
    // The content index Azahar reads (ticket.h ContentIndex): one rights field, every content.
    const std::size_t c = b + kTicketBody;
    PutBe16(t, c + 0x00, 1);    // always 1
    PutBe16(t, c + 0x02, 0x14); // header size
    PutBe32(t, c + 0x04, 0xAC); // the index's size
    PutBe32(t, c + 0x08, 0x14); // index headers at
    PutBe16(t, c + 0x0C, 1);    // one of them
    PutBe16(t, c + 0x0E, 0x14); // each this long
    PutBe32(t, c + 0x14, 0x28); // its data at
    PutBe32(t, c + 0x18, 1);    // one entry
    PutBe32(t, c + 0x1C, 0x84); // of this size
    PutBe32(t, c + 0x20, 0x84); // in all
    PutBe16(t, c + 0x24, 3);    // rights
    std::fill(t.begin() + static_cast<std::ptrdiff_t>(c + 0x2C), t.end(), u8{0xFF});
    return t;
}

std::vector<u8> MakeTmd(u64 title_id, u16 version, u64 content_size,
                        const std::array<u8, 32>& content_sha256, u32 save_size) {
    std::vector<u8> t(kSigBlock + kTmdBody + kChunk, 0);
    PutBe32(t, 0, kSigRsa2048Sha256);
    const std::size_t b = kSigBlock;
    PutText(t, b + 0x00, "Root-CA00000003-CP0000000b");
    t[b + 0x40] = 1;                    // format version
    PutBe64(t, b + 0x4C, title_id);
    PutBe32(t, b + 0x54, 0x40);         // a CTR title
    PutLe32(t, b + 0x5A, save_size);    // 3dbrew: little endian
    PutBe16(t, b + 0x9C, version);
    PutBe16(t, b + 0x9E, 1);            // one content
    PutBe16(t, b + 0xA0, 0);            // boot content 0
    // the content chunk: id 0, index 0, type 0 (not encrypted), size, hash
    const std::size_t ch = b + kTmdBody;
    PutBe32(t, ch + 0x00, 0);
    PutBe16(t, ch + 0x04, 0);
    PutBe16(t, ch + 0x06, 0);
    PutBe64(t, ch + 0x08, content_size);
    std::copy(content_sha256.begin(), content_sha256.end(), t.begin() + static_cast<std::ptrdiff_t>(ch + 0x10));
    // content info record 0 covers chunk 0; the info records' hash in the header
    const std::size_t info = b + 0xC4;
    PutBe16(t, info + 0x00, 0);
    PutBe16(t, info + 0x02, 1);
    const auto chunk_hash = Sha256(std::span<const u8>(&t[ch], kChunk));
    std::copy(chunk_hash.begin(), chunk_hash.end(), t.begin() + static_cast<std::ptrdiff_t>(info + 0x04));
    const auto info_hash = Sha256(std::span<const u8>(&t[info], 0x24 * 64));
    std::copy(info_hash.begin(), info_hash.end(), t.begin() + static_cast<std::ptrdiff_t>(b + 0xA4));
    return t;
}

std::string Build(const std::string& image, const std::string& out, const std::atomic_bool* cancel,
                  const std::function<void(u64, u64)>& progress) {
    const Source s = Inspect(image);
    if (!s.error.empty()) {
        return s.error;
    }
    if (s.kind != Kind::Ncsd && s.kind != Kind::Ncch) {
        return "Not a Cartridge Image";
    }
    auto in = OpenImage(image);
    if (!in) {
        return "The Game File Can't Be Opened";
    }
    const std::string part = out + ".part";
    FileUtil::IOFile o(part, "wb");
    if (!o.IsOpen()) {
        return "The Game Can't Be Converted Here";
    }
    const std::vector<u8> ticket = MakeTicket(s.title_id, s.version);
    const std::vector<u8> placeholder = MakeTmd(s.title_id, s.version, s.size, {}, s.save_size);
    std::vector<u8> header(kCiaHeader, 0);
    PutLe32(header, 0x00, static_cast<u32>(kCiaHeader));
    PutLe32(header, 0x08, 0); // no certificate chain
    PutLe32(header, 0x0C, static_cast<u32>(ticket.size()));
    PutLe32(header, 0x10, static_cast<u32>(placeholder.size()));
    PutLe32(header, 0x14, 0); // no meta
    PutLe64(header, 0x18, s.size);
    header[0x20] = 0x80; // content 0 is here
    const auto write_aligned = [&o](const std::vector<u8>& b) {
        if (o.WriteBytes(b.data(), b.size()) != b.size()) {
            return false;
        }
        const std::vector<u8> pad(static_cast<std::size_t>(Align64(b.size()) - b.size()), 0);
        return pad.empty() || o.WriteBytes(pad.data(), pad.size()) == pad.size();
    };
    const u64 tmd_at = Align64(kCiaHeader) + Align64(ticket.size());
    bool ok = write_aligned(header) && write_aligned(ticket) && write_aligned(placeholder);
    CryptoPP::SHA256 hash;
    std::vector<u8> buf(4 << 20);
    u64 done = 0;
    ok = ok && in->Seek(static_cast<s64>(s.offset), SEEK_SET);
    while (ok && done < s.size) {
        if (cancel != nullptr && cancel->load()) {
            o.Close();
            FileUtil::Delete(part);
            return "Cancelled";
        }
        const std::size_t n = static_cast<std::size_t>(std::min<u64>(buf.size(), s.size - done));
        if (in->ReadBytes(buf.data(), n) != n) {
            o.Close();
            FileUtil::Delete(part);
            return "The Game File Can't Be Read";
        }
        hash.Update(buf.data(), n);
        if (o.WriteBytes(buf.data(), n) != n) {
            ok = false;
            break;
        }
        done += n;
        if (progress) {
            progress(done, s.size);
        }
    }
    std::array<u8, 32> digest{};
    hash.Final(digest.data());
    const std::vector<u8> tmd = MakeTmd(s.title_id, s.version, s.size, digest, s.save_size);
    ok = ok && o.Seek(static_cast<s64>(tmd_at), SEEK_SET) && o.WriteBytes(tmd.data(), tmd.size()) == tmd.size();
    ok = o.Close() && ok;
    if (!ok) {
        FileUtil::Delete(part);
        return "Not Enough Space";
    }
    FileUtil::Delete(out);
    if (!FileUtil::Rename(part, out)) {
        FileUtil::Delete(part);
        return "The Game Can't Be Converted Here";
    }
    LOG_INFO(Frontend, "ctgp7: {:016X} v{} converted to a CIA ({} bytes of game)", s.title_id, s.version,
             s.size);
    return {};
}

} // namespace Ctgp7::Cia
