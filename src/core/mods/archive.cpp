// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>
#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif
#include "core/mods/archive.h"

namespace Mods {

u32 Crc32(std::string_view data) {
    static const auto table = [] {
        std::array<u32, 256> t{};
        for (u32 i = 0; i < 256; ++i) {
            u32 c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            t[i] = c;
        }
        return t;
    }();
    u32 crc = 0xFFFFFFFFu;
    for (const char ch : data) {
        crc = table[(crc ^ static_cast<u8>(ch)) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

ArchiveKind DetectArchive(std::string_view b) {
    if (b.size() >= 4 && b.substr(0, 2) == "PK") {
        return ArchiveKind::Zip;
    }
    if (b.size() >= 6 && std::memcmp(b.data(), "7z\xBC\xAF\x27\x1C", 6) == 0) {
        return ArchiveKind::SevenZip;
    }
    if (b.size() >= 6 && b.substr(0, 4) == "Rar!") {
        return ArchiveKind::Rar;
    }
    return ArchiveKind::Unknown;
}

namespace {

// Inflate after Mark Adler's "puff": canonical Huffman codes decoded bit by bit.
struct Huffman {
    short count[16];
    short symbol[288];
};

struct State {
    const u8* in;
    std::size_t in_size;
    std::size_t in_pos = 0;
    u32 bit_buf = 0;
    int bit_cnt = 0;
    std::string* out;
    std::size_t max_out;
    bool error = false;
};

int Bits(State& s, int need) {
    long val = s.bit_buf;
    while (s.bit_cnt < need) {
        if (s.in_pos >= s.in_size) {
            s.error = true;
            return 0;
        }
        val |= static_cast<long>(s.in[s.in_pos++]) << s.bit_cnt;
        s.bit_cnt += 8;
    }
    s.bit_buf = static_cast<u32>(val >> need);
    s.bit_cnt -= need;
    return static_cast<int>(val & ((1L << need) - 1));
}

int Decode(State& s, const Huffman& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; ++len) {
        code |= Bits(s, 1);
        if (s.error) {
            return -1;
        }
        const int count = h.count[len];
        if (code - count < first) {
            return h.symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s.error = true;
    return -1;
}

// Returns 0 for a complete code, >0 incomplete, <0 over-subscribed.
int Construct(Huffman& h, const short* length, int n) {
    short offs[16];
    for (int len = 0; len <= 15; ++len) {
        h.count[len] = 0;
    }
    for (int sym = 0; sym < n; ++sym) {
        ++h.count[length[sym]];
    }
    if (h.count[0] == n) {
        return 0;
    }
    int left = 1;
    for (int len = 1; len <= 15; ++len) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) {
            return left;
        }
    }
    offs[1] = 0;
    for (int len = 1; len < 15; ++len) {
        offs[len + 1] = offs[len] + h.count[len];
    }
    for (int sym = 0; sym < n; ++sym) {
        if (length[sym] != 0) {
            h.symbol[offs[length[sym]]++] = static_cast<short>(sym);
        }
    }
    return left;
}

bool Codes(State& s, const Huffman& lencode, const Huffman& distcode) {
    static constexpr short lens[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                       31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static constexpr short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                       2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static constexpr short dists[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,
                                        33,  49,  65,  97,  129, 193,  257,  385,  513,  769,
                                        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static constexpr short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                       6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int symbol = Decode(s, lencode);
        if (s.error || symbol < 0) {
            return false;
        }
        if (symbol < 256) {
            if (s.out->size() >= s.max_out) {
                return false;
            }
            s.out->push_back(static_cast<char>(symbol));
        } else if (symbol == 256) {
            return true;
        } else {
            symbol -= 257;
            if (symbol >= 29) {
                return false;
            }
            const int len = lens[symbol] + Bits(s, lext[symbol]);
            symbol = Decode(s, distcode);
            if (s.error || symbol < 0 || symbol >= 30) {
                return false;
            }
            const std::size_t dist = static_cast<std::size_t>(dists[symbol] + Bits(s, dext[symbol]));
            if (s.error || dist > s.out->size() || s.out->size() + len > s.max_out) {
                return false;
            }
            std::size_t from = s.out->size() - dist;
            for (int i = 0; i < len; ++i) {
                s.out->push_back((*s.out)[from + i]);
            }
        }
    }
}

bool Stored(State& s) {
    s.bit_buf = 0;
    s.bit_cnt = 0;
    if (s.in_pos + 4 > s.in_size) {
        return false;
    }
    unsigned len = s.in[s.in_pos] | (s.in[s.in_pos + 1] << 8);
    const unsigned nlen = s.in[s.in_pos + 2] | (s.in[s.in_pos + 3] << 8);
    s.in_pos += 4;
    if (len != (~nlen & 0xFFFF) || s.in_pos + len > s.in_size || s.out->size() + len > s.max_out) {
        return false;
    }
    s.out->append(reinterpret_cast<const char*>(s.in + s.in_pos), len);
    s.in_pos += len;
    return true;
}

bool Fixed(State& s) {
    static const auto codes = [] {
        std::pair<Huffman, Huffman> c;
        short lengths[288];
        int sym = 0;
        for (; sym < 144; ++sym) lengths[sym] = 8;
        for (; sym < 256; ++sym) lengths[sym] = 9;
        for (; sym < 280; ++sym) lengths[sym] = 7;
        for (; sym < 288; ++sym) lengths[sym] = 8;
        Construct(c.first, lengths, 288);
        for (sym = 0; sym < 30; ++sym) lengths[sym] = 5;
        Construct(c.second, lengths, 30);
        return c;
    }();
    return Codes(s, codes.first, codes.second);
}

bool Dynamic(State& s) {
    static constexpr short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int nlen = Bits(s, 5) + 257;
    const int ndist = Bits(s, 5) + 1;
    const int ncode = Bits(s, 4) + 4;
    if (s.error || nlen > 286 || ndist > 30) {
        return false;
    }
    short lengths[320] = {};
    for (int i = 0; i < ncode; ++i) {
        lengths[order[i]] = static_cast<short>(Bits(s, 3));
    }
    if (s.error) {
        return false;
    }
    Huffman lencode, distcode;
    if (Construct(lencode, lengths, 19) != 0) {
        return false;
    }
    int index = 0;
    while (index < nlen + ndist) {
        int symbol = Decode(s, lencode);
        if (s.error || symbol < 0) {
            return false;
        }
        if (symbol < 16) {
            lengths[index++] = static_cast<short>(symbol);
        } else {
            int len = 0, rep;
            if (symbol == 16) {
                if (index == 0) {
                    return false;
                }
                len = lengths[index - 1];
                rep = 3 + Bits(s, 2);
            } else if (symbol == 17) {
                rep = 3 + Bits(s, 3);
            } else {
                rep = 11 + Bits(s, 7);
            }
            if (s.error || index + rep > nlen + ndist) {
                return false;
            }
            while (rep--) {
                lengths[index++] = static_cast<short>(len);
            }
        }
    }
    if (lengths[256] == 0) {
        return false;
    }
    int err = Construct(lencode, lengths, nlen);
    if (err != 0 && (err < 0 || nlen != lencode.count[0] + lencode.count[1])) {
        return false;
    }
    err = Construct(distcode, lengths + nlen, ndist);
    if (err != 0 && (err < 0 || ndist != distcode.count[0] + distcode.count[1])) {
        return false;
    }
    return Codes(s, lencode, distcode);
}

u32 Le16(const u8* p) {
    return p[0] | (p[1] << 8);
}
u32 Le32(const u8* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<u32>(p[3]) << 24);
}

} // namespace

std::optional<std::string> Inflate(std::string_view in, std::size_t max_size) {
    std::string out;
    State s{reinterpret_cast<const u8*>(in.data()), in.size()};
    s.out = &out;
    s.max_out = max_size;
    int last;
    do {
        last = Bits(s, 1);
        const int type = Bits(s, 2);
        if (s.error) {
            return std::nullopt;
        }
        bool ok;
        switch (type) {
        case 0:
            ok = Stored(s);
            break;
        case 1:
            ok = Fixed(s);
            break;
        case 2:
            ok = Dynamic(s);
            break;
        default:
            ok = false;
        }
        if (!ok) {
            return std::nullopt;
        }
    } while (!last);
    return out;
}

std::optional<std::string> SafeRelativePath(std::string_view path) {
    std::string p(path);
    for (char& c : p) {
        if (c == '\\') {
            c = '/';
        }
    }
    std::string out;
    std::size_t start = 0;
    while (start <= p.size()) {
        const auto slash = p.find('/', start);
        const std::string part = p.substr(start, slash == std::string::npos ? slash : slash - start);
        if (part == "..") {
            return std::nullopt;
        }
        if (part.find(':') != std::string::npos || part.find('\0') != std::string::npos) {
            return std::nullopt;
        }
        if (part.empty() && start == 0 && !p.empty()) { // leading '/'
            return std::nullopt;
        }
        if (!part.empty() && part != ".") {
            if (!out.empty()) {
                out += '/';
            }
            out += part;
        }
        if (slash == std::string::npos) {
            break;
        }
        start = slash + 1;
    }
    return out;
}

ExtractResult ExtractZip(std::string_view zip, std::size_t max_total) {
    ExtractResult result;
    const auto fail = [&](const char* message) {
        result.files.clear();
        result.error = message;
        return result;
    };
    const u8* d = reinterpret_cast<const u8*>(zip.data());
    const std::size_t n = zip.size();
    if (n < 22) {
        return fail("Not a zip file");
    }
    // End of central directory: scan back over a possible comment
    std::size_t eocd = std::string::npos;
    const std::size_t lowest = n > 22 + 65535 ? n - 22 - 65535 : 0;
    for (std::size_t i = n - 22 + 1; i-- > lowest;) {
        if (Le32(d + i) == 0x06054b50u) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) {
        return fail("Not a zip file");
    }
    const u32 entries = Le16(d + eocd + 10);
    const u32 cd_size = Le32(d + eocd + 12);
    const u32 cd_offset = Le32(d + eocd + 16);
    if (entries == 0xFFFF || cd_size == 0xFFFFFFFFu || cd_offset == 0xFFFFFFFFu) {
        return fail("Zip64 archives are not supported");
    }
    if (static_cast<std::size_t>(cd_offset) + cd_size > n) {
        return fail("Damaged zip file");
    }
    std::size_t total = 0;
    std::size_t pos = cd_offset;
    for (u32 e = 0; e < entries; ++e) {
        if (pos + 46 > n || Le32(d + pos) != 0x02014b50u) {
            return fail("Damaged zip file");
        }
        const u32 flags = Le16(d + pos + 8);
        const u32 method = Le16(d + pos + 10);
        const u32 crc = Le32(d + pos + 16);
        const u32 csize = Le32(d + pos + 20);
        const u32 usize = Le32(d + pos + 24);
        const u32 name_len = Le16(d + pos + 28);
        const u32 extra_len = Le16(d + pos + 30);
        const u32 comment_len = Le16(d + pos + 32);
        const u32 local = Le32(d + pos + 42);
        if (pos + 46 + name_len + extra_len + comment_len > n) {
            return fail("Damaged zip file");
        }
        const std::string name(zip.substr(pos + 46, name_len));
        pos += 46 + name_len + extra_len + comment_len;
        if (!name.empty() && (name.back() == '/' || name.back() == '\\')) {
            continue;
        }
        if (flags & 1) {
            return fail("Encrypted zip files are not supported");
        }
        if (csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu || local == 0xFFFFFFFFu) {
            return fail("Zip64 archives are not supported");
        }
        const auto safe = SafeRelativePath(name);
        if (!safe || safe->empty()) {
            return fail("The archive holds an unsafe file path");
        }
        total += usize;
        if (total > max_total) {
            return fail("The archive is too large");
        }
        if (static_cast<std::size_t>(local) + 30 > n || Le32(d + local) != 0x04034b50u) {
            return fail("Damaged zip file");
        }
        const std::size_t data_pos = static_cast<std::size_t>(local) + 30 + Le16(d + local + 26) +
                                     Le16(d + local + 28);
        if (data_pos + csize > n) {
            return fail("Damaged zip file");
        }
        const std::string_view raw = zip.substr(data_pos, csize);
        ArchiveFile file{*safe, {}};
        if (method == 0) {
            if (csize != usize) {
                return fail("Damaged zip file");
            }
            file.data.assign(raw);
        } else if (method == 8) {
            auto out = Inflate(raw, usize);
            if (!out || out->size() != usize) {
                return fail("Damaged zip file");
            }
            file.data = std::move(*out);
        } else {
            return fail("This zip file uses an unsupported compression method");
        }
        if (Crc32(file.data) != crc) {
            return fail("Zip file checksum mismatch");
        }
        result.files.push_back(std::move(file));
    }
    return result;
}

namespace {

namespace fs = std::filesystem;

const char* KindName(ArchiveKind kind) {
    switch (kind) {
    case ArchiveKind::SevenZip:
        return "7z";
    case ArchiveKind::Rar:
        return "rar";
    case ArchiveKind::Zip:
        return "zip";
    default:
        return "This archive";
    }
}

#ifndef _WIN32
struct RunResult {
    bool started = false;
    bool timed_out = false;
    bool too_much_output = false;
    bool cancelled = false; // AzaharFrame texdl
    int status = -1; // exit code
    std::string output; // stdout and stderr
};

// No shell: posix_spawn with a fixed argv. The environment is ours minus everything that steers the
// dynamic loader, so bsdtar resolves libarchive and friends from the system, never from the app
// folder's lib/ (the launcher already unsets LD_LIBRARY_PATH; this covers other callers).
RunResult Run(const std::string& tool, const std::vector<std::string>& args, int timeout_seconds,
              std::size_t max_output, const std::atomic_bool* cancel = nullptr) {
    RunResult r;
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) {
        return r;
    }
    std::vector<std::string> env_store;
    for (char** e = environ; e != nullptr && *e != nullptr; ++e) {
        if (std::strncmp(*e, "LD_", 3) != 0) {
            env_store.emplace_back(*e);
        }
    }
    std::vector<char*> envp;
    for (auto& s : env_store) {
        envp.push_back(s.data());
    }
    envp.push_back(nullptr);
    std::vector<std::string> argv_store{tool};
    argv_store.insert(argv_store.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (auto& s : argv_store) {
        argv.push_back(s.data());
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fds[1], 1);
    posix_spawn_file_actions_adddup2(&fa, fds[1], 2);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    pid_t pid = 0;
    const int rc = posix_spawn(&pid, tool.c_str(), &fa, nullptr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&fa);
    close(fds[1]);
    if (rc != 0) {
        close(fds[0]);
        return r;
    }
    r.started = true;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
    bool killed = false;
    for (;;) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                              deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) {
            r.timed_out = true;
            kill(pid, SIGKILL);
            killed = true;
            break;
        }
        if (cancel != nullptr && cancel->load()) { // AzaharFrame texdl
            r.cancelled = true;
            kill(pid, SIGKILL);
            killed = true;
            break;
        }
        pollfd p{fds[0], POLLIN, 0};
        const int pr = poll(&p, 1, static_cast<int>(std::min<long long>(left, cancel ? 250 : 1000)));
        if (pr < 0 && errno != EINTR) {
            break;
        }
        if (pr > 0) {
            char buf[4096];
            const ssize_t n = read(fds[0], buf, sizeof(buf));
            if (n <= 0) {
                break; // end of output
            }
            if (r.output.size() + static_cast<std::size_t>(n) > max_output) {
                r.too_much_output = true;
                kill(pid, SIGKILL);
                killed = true;
                break;
            }
            r.output.append(buf, static_cast<std::size_t>(n));
        }
    }
    close(fds[0]);
    int st = 0;
    waitpid(pid, &st, 0);
    if (!killed && WIFEXITED(st)) {
        r.status = WEXITSTATUS(st);
    }
    return r;
}
#endif

struct TempDir {
    std::string path;
    ~TempDir() {
        if (!path.empty()) {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    }
};


#ifndef _WIN32
// AzaharFrame texdl: the listing checks of ExtractWithBsdtar, shared with ExtractFileToDir. "" = fine.
std::string CheckListing(const std::string& tool, const std::string& archive, ArchiveKind kind,
                         std::size_t max_total, int timeout_seconds,
                         const std::atomic_bool* cancel = nullptr) {
    const auto names = Run(tool, {"-tf", archive}, timeout_seconds, 16u * 1024 * 1024, cancel);
    if (!names.started) {
        return (std::string(KindName(kind)) + " needs bsdtar, which wasn't found");
    }
    if (names.cancelled) {
        return "Cancelled";
    }
    if (names.timed_out) {
        return ("Unpacking took too long");
    }
    if (names.too_much_output) {
        return ("The archive holds too many files");
    }
    if (names.status != 0) {
        return ("The archive is damaged or not readable");
    }
    const auto verbose = Run(tool, {"-tvf", archive}, timeout_seconds, 32u * 1024 * 1024, cancel);
    if (verbose.cancelled) {
        return "Cancelled";
    }
    if (verbose.status != 0 || verbose.timed_out || verbose.too_much_output) {
        return ("The archive is damaged or not readable");
    }
    std::istringstream ns(names.output);
    std::string line;
    std::size_t count = 0;
    while (std::getline(ns, line)) {
        if (line.empty()) {
            continue;
        }
        ++count;
        const auto safe = SafeRelativePath(line);
        if (!safe || (safe->empty() && line != "./")) {
            return ("The archive holds an unsafe file path");
        }
    }
    std::istringstream vs(verbose.output);
    std::size_t listed_total = 0;
    while (std::getline(vs, line)) {
        if (line.empty()) {
            continue;
        }
        if (line[0] != '-' && line[0] != 'd') { // links, devices, fifos
            return ("The archive holds links or special files");
        }
        // mode links owner group size ...
        std::istringstream ls(line);
        std::string mode, links, owner, group, size;
        ls >> mode >> links >> owner >> group >> size;
        if (!size.empty() && std::all_of(size.begin(), size.end(), [](char ch) { return ch >= '0' && ch <= '9'; }) &&
            size.size() < 16) {
            listed_total += static_cast<std::size_t>(std::stoull(size));
            if (listed_total > max_total) {
                return ("The archive is too large");
            }
        }
    }
    if (count == 0) {
        return ("The archive is empty");
    }

    return {};
}
#endif

} // namespace

std::string FindBsdtar() {
    static std::once_flag once;
    static std::string found;
    std::call_once(once, [] {
#ifndef _WIN32
        std::vector<std::string> dirs{"/usr/bin", "/bin", "/usr/local/bin"};
        if (const char* path = std::getenv("PATH")) {
            std::string p(path);
            std::size_t start = 0;
            while (start <= p.size()) {
                const auto colon = p.find(':', start);
                const std::string d = p.substr(start, colon == std::string::npos ? colon : colon - start);
                if (!d.empty() && d.front() == '/') {
                    dirs.push_back(d);
                }
                if (colon == std::string::npos) {
                    break;
                }
                start = colon + 1;
            }
        }
        for (const auto& d : dirs) {
            const std::string candidate = d + "/bsdtar";
            if (access(candidate.c_str(), X_OK) == 0 && fs::is_regular_file(candidate)) {
                found = candidate;
                return;
            }
        }
#endif
    });
    return found;
}

ExtractResult ExtractWithBsdtar(std::string_view bytes, ArchiveKind kind, std::size_t max_total,
                                int timeout_seconds, const std::string& tool_override) {
    ExtractResult result;
    const auto fail = [&](std::string message) {
        result.files.clear();
        result.error = std::move(message);
        return result;
    };
#ifdef _WIN32
    (void)bytes, (void)max_total, (void)timeout_seconds, (void)tool_override;
    return fail(std::string(KindName(kind)) + " archives are not supported here");
#else
    const std::string tool = tool_override.empty() ? FindBsdtar() : tool_override;
    if (tool.empty() || access(tool.c_str(), X_OK) != 0) {
        return fail(std::string(KindName(kind)) + " needs bsdtar, which wasn't found");
    }
    std::string templ = (fs::temp_directory_path() / "azahar-mod-XXXXXX").string();
    if (mkdtemp(templ.data()) == nullptr) {
        return fail("Cannot make a temporary folder");
    }
    TempDir temp{templ};
    const std::string archive = temp.path + "/archive.bin";
    const std::string out_dir = temp.path + "/out";
    {
        std::ofstream f(archive, std::ios::binary);
        f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!f) {
            return fail("Cannot write the temporary file");
        }
    }
    std::error_code ec;
    fs::create_directory(out_dir, ec);

    // 1. Names and the long listing, checked before anything is written.
    if (const auto problem = CheckListing(tool, archive, kind, max_total, timeout_seconds); !problem.empty()) {
        return fail(problem);
    }

    // 2. Extract with no owner or mode from the archive.
    const auto ex = Run(tool, {"-xf", archive, "-C", out_dir, "--no-same-owner", "--no-same-permissions"},
                        timeout_seconds, 1024 * 1024);
    if (ex.timed_out) {
        return fail("Unpacking took too long");
    }
    if (ex.status != 0) {
        return fail("The archive could not be unpacked");
    }

    // 3. Read back; the extracted tree is checked again (symlinks, escapes, sizes).
    std::size_t total = 0;
    const fs::path root = fs::weakly_canonical(out_dir, ec);
    for (fs::recursive_directory_iterator it(out_dir, fs::directory_options::none, ec), end; !ec && it != end;
         it.increment(ec)) {
        const auto st = it->symlink_status(ec);
        if (ec) {
            break;
        }
        if (fs::is_symlink(st) || (!fs::is_directory(st) && !fs::is_regular_file(st))) {
            return fail("The archive holds links or special files");
        }
        if (fs::is_directory(st)) {
            continue;
        }
        const std::string rel = it->path().lexically_relative(root).generic_string();
        const auto safe = SafeRelativePath(rel);
        if (!safe || safe->empty() || safe->rfind("../", 0) == 0) {
            return fail("The archive holds an unsafe file path");
        }
        const auto size = fs::file_size(it->path(), ec);
        total += size;
        if (ec || total > max_total) {
            return fail("The archive is too large");
        }
        std::ifstream f(it->path(), std::ios::binary);
        ArchiveFile file{*safe, {}};
        file.data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        if (file.data.size() != size) {
            return fail("The archive could not be unpacked");
        }
        result.files.push_back(std::move(file));
    }
    if (ec) {
        return fail("The archive could not be unpacked");
    }
    return result;
#endif
}

// AzaharFrame texdl: extraction to disk for big archives (texture packs, 0.5 to 6 GB). Same listing
// checks as ExtractWithBsdtar, then bsdtar writes straight into `out_dir`, which is checked again
// afterwards (no links, no special files, nothing outside, total under max_total).
std::string ExtractFileToDir(const std::string& archive_path, ArchiveKind kind, const std::string& out_dir,
                             std::uint64_t max_total, int timeout_seconds, const std::atomic_bool* cancel,
                             const std::string& tool_override) {
#ifdef _WIN32
    (void)archive_path, (void)out_dir, (void)max_total, (void)timeout_seconds, (void)cancel, (void)tool_override;
    return std::string(KindName(kind)) + " archives are not supported here";
#else
    const std::string tool = tool_override.empty() ? FindBsdtar() : tool_override;
    if (tool.empty() || access(tool.c_str(), X_OK) != 0) {
        return std::string(KindName(kind)) + " needs bsdtar, which wasn't found";
    }
    if (const auto problem = CheckListing(tool, archive_path, kind, static_cast<std::size_t>(max_total),
                                          timeout_seconds, cancel);
        !problem.empty()) {
        return problem;
    }
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        return "Cannot make the folder for the pack";
    }
    const auto ex = Run(tool, {"-xf", archive_path, "-C", out_dir, "--no-same-owner", "--no-same-permissions"},
                        timeout_seconds, 1024 * 1024, cancel);
    if (ex.cancelled) {
        return "Cancelled";
    }
    if (ex.timed_out) {
        return "Unpacking took too long";
    }
    if (ex.status != 0) {
        return "The archive could not be unpacked";
    }
    std::uint64_t total = 0;
    for (fs::recursive_directory_iterator it(out_dir, fs::directory_options::none, ec), end; !ec && it != end;
         it.increment(ec)) {
        const auto st = it->symlink_status(ec);
        if (ec) {
            break;
        }
        if (fs::is_symlink(st) || (!fs::is_directory(st) && !fs::is_regular_file(st))) {
            return "The archive holds links or special files";
        }
        if (fs::is_regular_file(st)) {
            total += fs::file_size(it->path(), ec);
            if (ec || total > max_total) {
                return "The archive is too large";
            }
        }
    }
    return ec ? "The archive could not be unpacked" : std::string{};
#endif
}

ExtractResult ExtractArchive(std::string_view bytes, ArchiveKind kind) {
    if (kind == ArchiveKind::Zip) {
        return ExtractZip(bytes);
    }
    return ExtractWithBsdtar(bytes, kind);
}

} // namespace Mods
