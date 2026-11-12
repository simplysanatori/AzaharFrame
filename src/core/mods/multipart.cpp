// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <httplib.h>
#ifdef __linux__
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include <fmt/format.h>
#include "common/request_path.h"
#include "core/mods/multipart.h"

namespace fs = std::filesystem;

namespace Mods::Multipart {

std::string SizeText(u64 bytes) {
    constexpr double mb = 1024.0 * 1024.0;
    if (bytes < 1024ull * 1024 * 1024) {
        return fmt::format("{} MB", std::max<u64>(1, static_cast<u64>(bytes / mb + 0.5)));
    }
    return fmt::format("{:.1f} GB", static_cast<double>(bytes) / (1024.0 * mb));
}

u64 RequiredBytes(u64 parts_bytes, u64 extracted_bytes) {
    const u64 sum = parts_bytes + extracted_bytes;
    return sum + sum / 10;
}

std::string SpaceError(u64 parts_bytes, u64 extracted_bytes, u64 free_bytes) {
    const u64 need = RequiredBytes(parts_bytes, extracted_bytes);
    if (need <= free_bytes) {
        return {};
    }
    return fmt::format("Not Enough Space: Needs {} Free", SizeText(need));
}

bool FreeBytes(const std::string& dir, u64* out) {
    std::error_code ec;
    fs::path p = dir;
    while (!p.empty() && !fs::exists(p, ec)) {
        p = p.parent_path();
    }
    if (p.empty()) {
        return false;
    }
    const auto info = fs::space(p, ec);
    if (ec) {
        return false;
    }
    *out = static_cast<u64>(info.available);
    return true;
}

namespace {
std::string Lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}
bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}
bool AllDigits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}
} // namespace

std::string SplitKindOfName(const std::string& raw) {
    const std::string n = Lower(raw);
    const auto dot = n.rfind('.');
    if (dot == std::string::npos) {
        return {};
    }
    const std::string ext = n.substr(dot + 1);
    if (ext == "rar") {
        return "rar";
    }
    if (ext.size() == 3 && ext[0] == 'r' && AllDigits(ext.substr(1))) {
        return "rar"; // .r00 .r01
    }
    if (AllDigits(ext) && ext.size() == 3 && EndsWith(n.substr(0, dot), ".7z")) {
        return "7z";
    }
    if (ext.size() >= 3 && ext[0] == 'z' && AllDigits(ext.substr(1))) {
        return "zip"; // .z01
    }
    if (ext == "zip") {
        return "zip";
    }
    if (ext == "7z") {
        return "7z";
    }
    return {};
}

namespace {

u32 Rd32(const std::string& s, std::size_t at) {
    return static_cast<u32>(static_cast<unsigned char>(s[at])) |
           static_cast<u32>(static_cast<unsigned char>(s[at + 1])) << 8 |
           static_cast<u32>(static_cast<unsigned char>(s[at + 2])) << 16 |
           static_cast<u32>(static_cast<unsigned char>(s[at + 3])) << 24;
}
u32 Rd16(const std::string& s, std::size_t at) {
    return static_cast<u32>(static_cast<unsigned char>(s[at])) |
           static_cast<u32>(static_cast<unsigned char>(s[at + 1])) << 8;
}
void Wr32(std::string& s, std::size_t at, u32 v) {
    for (int i = 0; i < 4; ++i) {
        s[at + static_cast<std::size_t>(i)] = static_cast<char>((v >> (8 * i)) & 255);
    }
}
void Wr16(std::string& s, std::size_t at, u32 v) {
    s[at] = static_cast<char>(v & 255);
    s[at + 1] = static_cast<char>((v >> 8) & 255);
}

} // namespace

std::string FixSplitZip(const std::string& joined_path, const std::vector<u64>& part_sizes) {
    std::error_code ec;
    const u64 size = fs::file_size(joined_path, ec);
    if (ec || part_sizes.empty()) {
        return "Cannot read the joined archive";
    }
    // The end of central directory record is in the last part (22 bytes + comment).
    const u64 tail_len = std::min<u64>(part_sizes.back(), 22 + 65535);
    std::string tail(tail_len, '\0');
    {
        std::ifstream in(joined_path, std::ios::binary);
        in.seekg(static_cast<std::streamoff>(size - tail_len));
        in.read(tail.data(), static_cast<std::streamsize>(tail_len));
        if (!in) {
            return "Cannot read the joined archive";
        }
    }
    std::size_t eocd = std::string::npos;
    for (std::size_t i = tail.size() >= 22 ? tail.size() - 22 + 1 : 0; i-- > 0;) {
        if (Rd32(tail, i) == 0x06054b50 && i + 22 + Rd16(tail, i + 20) == tail.size()) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) {
        return "Not a zip archive";
    }
    const u32 disk = Rd16(tail, eocd + 4);
    const u32 cd_disk = Rd16(tail, eocd + 6);
    if (disk == 0 && cd_disk == 0 && part_sizes.size() == 1) {
        return {};
    }
    if (disk + 1 != part_sizes.size() && disk != 0) {
        return "The parts are not all there";
    }
    const u32 cd_size = Rd32(tail, eocd + 12);
    const u32 cd_off = Rd32(tail, eocd + 16);
    if (cd_size == 0xFFFFFFFFu || cd_off == 0xFFFFFFFFu || Rd16(tail, eocd + 10) == 0xFFFF) {
        return "Unsupported split zip (zip64)";
    }
    std::vector<u64> base(part_sizes.size(), 0);
    for (std::size_t i = 1; i < part_sizes.size(); ++i) {
        base[i] = base[i - 1] + part_sizes[i - 1];
    }
    if (cd_disk >= part_sizes.size()) {
        return "The parts are not all there";
    }
    const u64 cd_start = base[cd_disk] + cd_off;
    if (cd_start + cd_size > size) {
        return "The parts are not all there";
    }
    std::string cd(cd_size, '\0');
    {
        std::ifstream in(joined_path, std::ios::binary);
        in.seekg(static_cast<std::streamoff>(cd_start));
        in.read(cd.data(), static_cast<std::streamsize>(cd_size));
        if (!in) {
            return "Cannot read the joined archive";
        }
    }
    std::size_t at = 0;
    while (at + 46 <= cd.size() && Rd32(cd, at) == 0x02014b50) {
        const u32 start_disk = Rd16(cd, at + 34);
        const u32 off = Rd32(cd, at + 42);
        if (off == 0xFFFFFFFFu) {
            return "Unsupported split zip (zip64)";
        }
        if (start_disk >= part_sizes.size()) {
            return "The parts are not all there";
        }
        Wr32(cd, at + 42, static_cast<u32>(base[start_disk] + off));
        Wr16(cd, at + 34, 0);
        at += 46 + Rd16(cd, at + 28) + Rd16(cd, at + 30) + Rd16(cd, at + 32);
    }
    // The joined file is one disk: the central directory rewritten in place (it may reach into the
    // part holding the end record, so the record's own fields are patched after it, not from a copy
    // read before), then the end record.
    std::fstream io(joined_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!io) {
        return "Cannot write the joined archive";
    }
    io.seekp(static_cast<std::streamoff>(cd_start));
    io.write(cd.data(), static_cast<std::streamsize>(cd.size()));
    const u64 eocd_at = size - tail_len + eocd;
    std::string disks(4, '\0'); // this disk, the disk the directory starts on
    io.seekp(static_cast<std::streamoff>(eocd_at + 4));
    io.write(disks.data(), 4);
    std::string entries(2, '\0');
    Wr16(entries, 0, Rd16(tail, eocd + 10)); // entries on this disk = all of them
    io.seekp(static_cast<std::streamoff>(eocd_at + 8));
    io.write(entries.data(), 2);
    std::string offset(4, '\0');
    Wr32(offset, 0, static_cast<u32>(cd_start));
    io.seekp(static_cast<std::streamoff>(eocd_at + 16));
    io.write(offset.data(), 4);
    io.flush();
    return io ? std::string{} : "Cannot write the joined archive";
}

std::string JoinParts(const std::vector<std::string>& parts, const std::string& out_path,
                      const std::atomic_bool* cancel, const std::function<void(u64, u64)>& progress) {
    if (parts.empty()) {
        return "No parts to join";
    }
    std::error_code ec;
    u64 total = 0;
    std::vector<u64> sizes;
    for (const auto& p : parts) {
        const u64 s = fs::file_size(p, ec);
        if (ec) {
            return "A part is missing";
        }
        sizes.push_back(s);
        total += s;
    }
    std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return "Cannot write the joined archive";
    }
    std::vector<char> buf(1 << 20);
    u64 done = 0;
    for (const auto& p : parts) {
        std::ifstream in(p, std::ios::binary);
        while (in) {
            if (cancel != nullptr && cancel->load()) {
                return "Cancelled";
            }
            in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
            const auto n = in.gcount();
            if (n > 0) {
                out.write(buf.data(), n);
                done += static_cast<u64>(n);
                if (progress) {
                    progress(done, total);
                }
            }
        }
        if (!out) {
            return "Not Enough Space: Needs more room";
        }
    }
    out.close();
    if (!out) {
        return "Cannot write the joined archive";
    }
    // A split zip: .zip last, parts .z01 ... before it (or "PK\x07\x08" at the start of the first).
    std::ifstream first(parts.front(), std::ios::binary);
    char head[4]{};
    first.read(head, 4);
    const bool zip_like = first.gcount() == 4 && head[0] == 'P' && head[1] == 'K';
    if (zip_like && parts.size() > 1) {
        return FixSplitZip(out_path, sizes);
    }
    return {};
}

namespace {
std::mutex g_priority_mutex;
PriorityBackend g_priority;
} // namespace

PriorityBackend RealPriorityBackend() {
    PriorityBackend b;
#ifdef __linux__
    b.thread_id = [] { return static_cast<int>(::syscall(SYS_gettid)); };
    b.set_priority = [](int which, int who, int priority) {
        return ::setpriority(static_cast<__priority_which_t>(which), static_cast<id_t>(who), priority);
    };
    b.set_io_priority = [](int which, int who, int ioprio) {
        return static_cast<long>(::syscall(SYS_ioprio_set, which, who, ioprio));
    };
#else
    b.thread_id = [] { return 0; };
    b.set_priority = [](int, int, int) { return -1; };
    b.set_io_priority = [](int, int, int) { return -1L; };
#endif
    return b;
}

void SetPriorityBackend(PriorityBackend backend) {
    std::lock_guard lock{g_priority_mutex};
    g_priority = std::move(backend);
}

bool LowerThisThreadPriority() {
    PriorityBackend b;
    {
        std::lock_guard lock{g_priority_mutex};
        b = g_priority.thread_id ? g_priority : RealPriorityBackend();
    }
    const int tid = b.thread_id();
    const bool nice_ok = b.set_priority(kPrioProcess, tid, kLowNice) == 0;
    const bool io_ok = b.set_io_priority(kIoprioWhoProcess, tid, kIoprioClassIdle << kIoprioClassShift) == 0;
    return nice_ok && io_ok;
}

namespace {
const char* CaBundle() {
    static constexpr std::array<const char*, 4> candidates{
        "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt", "/etc/ssl/cert.pem",
        "/etc/ssl/ca-bundle.pem"};
    for (const char* path : candidates) {
        std::error_code ec;
        if (fs::exists(path, ec)) {
            return path;
        }
    }
    return nullptr;
}
} // namespace

FileReply StreamGet(const std::string& host, const std::string& path, const std::string& dest,
                    const std::atomic_bool* cancel, const Progress& progress) {
    FileReply reply;
    const char* ca = CaBundle();
    if (ca == nullptr) {
        reply.error = "No certificate store found";
        return reply;
    }
    FILE* file = nullptr;
    try {
        httplib::Client client(host);
        client.set_connection_timeout(15);
        client.set_read_timeout(60);
        client.set_write_timeout(30);
        client.set_follow_location(false);
        client.set_ca_cert_path(ca);
        client.enable_server_certificate_verification(true);
        client.set_url_encode(false);
        const httplib::Headers headers{{"Accept-Encoding", "identity"}};
        u64 done = 0;
        bool stopped = false;
        const auto res = client.Get(
            Common::RequestPath(path), headers,
            [&](const httplib::Response& r) {
                reply.status = r.status;
                reply.location = r.get_header_value("Location");
                if (r.status == 200) {
                    const std::string cl = r.get_header_value("Content-Length");
                    reply.total = cl.empty() ? 0 : std::strtoull(cl.c_str(), nullptr, 10);
                    file = std::fopen(dest.c_str(), "wb");
                    if (file == nullptr) {
                        reply.error = "Cannot write the download file";
                        return false;
                    }
                }
                return true;
            },
            [&](const char* data, std::size_t len) {
                if (cancel != nullptr && cancel->load()) {
                    stopped = true;
                    return false;
                }
                if (file != nullptr) {
                    if (std::fwrite(data, 1, len, file) != len) {
                        reply.error = "Not Enough Space: the disk is full";
                        return false;
                    }
                    done += len;
                    if (progress) {
                        progress(done, reply.total);
                    }
                }
                return true;
            });
        if (file != nullptr) {
            std::fclose(file);
            file = nullptr;
        }
        if (stopped) {
            reply.error = "Cancelled";
        } else if (!res && reply.error.empty()) {
            reply.error = httplib::to_string(res.error());
        } else if (res && reply.status == 200 && reply.total != 0 && done != reply.total) {
            reply.error = "Download incomplete";
        }
    } catch (const std::exception& e) {
        if (file != nullptr) {
            std::fclose(file);
        }
        reply.error = e.what();
    }
    return reply;
}

} // namespace Mods::Multipart
