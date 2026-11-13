// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <fmt/format.h>
#include <json.hpp>
#include "core/cheats/online.h"
#include "core/mods/archive.h"
#include "core/mods/catalog.h"
#include "core/mods/mega.h"

namespace Mods::Mega {

namespace {

constexpr u64 kChunk = 2u * 1024 * 1024;
constexpr const char* kApiHost = "https://g.api.mega.co.nz";

bool HandleOk(std::string_view h) {
    return h.size() >= 4 && h.size() <= 16 &&
           std::all_of(h.begin(), h.end(), [](char c) {
               return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
           });
}

std::string Fold(std::string_view key32) { // the 16 byte AES key of a 32 byte file key
    std::string out(16, '\0');
    for (std::size_t i = 0; i < 16; ++i) {
        out[i] = static_cast<char>(key32[i] ^ key32[i + 16]);
    }
    return out;
}

} // namespace

std::optional<std::string> Base64UrlDecode(std::string_view text) {
    std::string out;
    u32 acc = 0;
    int bits = 0;
    while (!text.empty() && text.back() == '=') {
        text.remove_suffix(1);
    }
    for (const char c : text) {
        int v;
        if (c >= 'A' && c <= 'Z') {
            v = c - 'A';
        } else if (c >= 'a' && c <= 'z') {
            v = c - 'a' + 26;
        } else if (c >= '0' && c <= '9') {
            v = c - '0' + 52;
        } else if (c == '-' || c == '+') {
            v = 62;
        } else if (c == '_' || c == ',' || c == '/') {
            v = 63;
        } else {
            return std::nullopt;
        }
        acc = (acc << 6) | static_cast<u32>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out += static_cast<char>((acc >> bits) & 0xFF);
        }
    }
    if (text.size() % 4 == 1) {
        return std::nullopt;
    }
    return out;
}

std::optional<Link> ParseLink(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    for (const std::string_view scheme : {"https://", "http://"}) {
        if (text.substr(0, scheme.size()) == scheme) {
            text.remove_prefix(scheme.size());
            break;
        }
    }
    for (const std::string_view host : {"mega.nz/", "mega.co.nz/", "www.mega.nz/"}) {
        if (text.substr(0, host.size()) == host) {
            text.remove_prefix(host.size());
            break;
        }
    }
    Link link;
    std::string_view handle, key, node;
    const auto split = [](std::string_view s, char sep) {
        std::vector<std::string_view> parts;
        while (true) {
            const auto p = s.find(sep);
            parts.push_back(s.substr(0, p));
            if (p == std::string_view::npos) {
                return parts;
            }
            s.remove_prefix(p + 1);
        }
    };
    if (text.substr(0, 2) == "#!" || text.substr(0, 3) == "#F!") { // old style
        link.folder = text[1] == 'F';
        const auto parts = split(text.substr(link.folder ? 3 : 2), '!');
        if (parts.size() != 2) {
            return std::nullopt;
        }
        handle = parts[0];
        key = parts[1];
    } else if (text.substr(0, 5) == "file/" || text.substr(0, 7) == "folder/") {
        link.folder = text[1] == 'o';
        text.remove_prefix(link.folder ? 7 : 5);
        const auto hash = text.find('#');
        if (hash == std::string_view::npos) {
            return std::nullopt;
        }
        handle = text.substr(0, hash);
        key = text.substr(hash + 1);
        if (link.folder) { // "<key>/file/<node>" names one file of the folder
            const auto slash = key.find('/');
            if (slash != std::string_view::npos) {
                const auto rest = key.substr(slash + 1);
                key = key.substr(0, slash);
                if (rest.substr(0, 5) != "file/") {
                    return std::nullopt;
                }
                node = rest.substr(5);
            }
        } else if (key.find('/') != std::string_view::npos) {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }
    const auto raw = Base64UrlDecode(key);
    if (!HandleOk(handle) || !raw || raw->size() != (link.folder ? 16u : 32u) ||
        (!node.empty() && !HandleOk(node))) {
        return std::nullopt;
    }
    link.handle = std::string(handle);
    link.key = *raw;
    link.node = std::string(node);
    return link;
}

std::optional<FileKey> DeriveFileKey(std::string_view raw_key) {
    if (raw_key.size() != 32) {
        return std::nullopt;
    }
    FileKey k;
    const std::string aes = Fold(raw_key);
    std::copy(aes.begin(), aes.end(), k.aes.begin());
    std::copy(raw_key.begin() + 16, raw_key.begin() + 24, k.nonce.begin());
    return k;
}

struct CtrDecryptor::Impl {
    CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption mode;
};

CtrDecryptor::CtrDecryptor(const FileKey& key, u64 offset) : impl(std::make_unique<Impl>()) {
    std::array<u8, 16> iv{};
    std::copy(key.nonce.begin(), key.nonce.end(), iv.begin());
    const u64 block = offset / 16;
    for (int i = 0; i < 8; ++i) {
        iv[15 - i] = static_cast<u8>(block >> (8 * i));
    }
    impl->mode.SetKeyWithIV(key.aes.data(), key.aes.size(), iv.data(), iv.size());
    if (const std::size_t skip = offset % 16) { // start inside a block
        std::array<u8, 16> dummy{};
        impl->mode.ProcessData(dummy.data(), dummy.data(), skip);
    }
}
CtrDecryptor::~CtrDecryptor() = default;

void CtrDecryptor::Process(u8* data, std::size_t size) {
    if (size) {
        impl->mode.ProcessData(data, data, size);
    }
}
std::string CtrDecryptor::Process(std::string_view data) {
    std::string out(data);
    Process(reinterpret_cast<u8*>(out.data()), out.size());
    return out;
}

namespace {

std::string EcbDecrypt(std::string_view key16, std::string_view data) {
    CryptoPP::ECB_Mode<CryptoPP::AES>::Decryption dec;
    dec.SetKey(reinterpret_cast<const CryptoPP::byte*>(key16.data()), key16.size());
    std::string out(data);
    dec.ProcessData(reinterpret_cast<CryptoPP::byte*>(out.data()),
                    reinterpret_cast<const CryptoPP::byte*>(data.data()), data.size());
    return out;
}

/// The attributes ("MEGA{"n":"name",...}") of a node, CBC with a zero IV, or "".
std::string AttrName(std::string_view b64, std::string_view key16) {
    const auto raw = Base64UrlDecode(b64);
    if (!raw || raw->empty() || raw->size() % 16 != 0 || raw->size() > 4096) {
        return {};
    }
    CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption dec;
    const std::array<CryptoPP::byte, 16> iv{};
    dec.SetKeyWithIV(reinterpret_cast<const CryptoPP::byte*>(key16.data()), key16.size(), iv.data(),
                     iv.size());
    std::string plain(raw->size(), '\0');
    dec.ProcessData(reinterpret_cast<CryptoPP::byte*>(plain.data()),
                    reinterpret_cast<const CryptoPP::byte*>(raw->data()), raw->size());
    if (plain.compare(0, 4, "MEGA") != 0) {
        return {};
    }
    plain.erase(0, 4);
    const auto nul = plain.find('\0');
    if (nul != std::string::npos) {
        plain.resize(nul);
    }
    try {
        return nlohmann::json::parse(plain).value("n", "");
    } catch (const std::exception&) {
        return {};
    }
}

} // namespace

std::string ApiErrorText(int code) {
    switch (code) {
    case -9:  // ENOENT: deleted or never existed
    case -16: // EBLOCKED: taken down
    case -8:  // EEXPIRED
    case -14: // EKEY: the key in the link is wrong
    case -11: // EACCESS
        return kNotFound;
    case -17: // EOVERQUOTA
    case -18: // ETEMPUNAVAIL
        return kQuota;
    default:
        return fmt::format("MEGA Error {}", code);
    }
}

std::optional<int> ParseApiError(std::string_view body) {
    try {
        auto j = nlohmann::json::parse(body);
        if (j.is_array() && j.size() == 1) {
            j = j[0];
        }
        if (j.is_number_integer() && j.get<int>() < 0) {
            return j.get<int>();
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

bool HostAllowed(const std::string& host) {
    if (host == kApiHost) {
        return true;
    }
    constexpr std::string_view head = "https://";
    constexpr std::string_view tail = ".userstorage.mega.co.nz";
    if (host.size() <= head.size() + tail.size() || host.compare(0, head.size(), head) != 0 ||
        host.compare(host.size() - tail.size(), tail.size(), tail) != 0) {
        return false;
    }
    const std::string label = host.substr(head.size(), host.size() - head.size() - tail.size());
    return std::all_of(label.begin(), label.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    });
}

namespace {
bool SplitUrl(const std::string& url, std::string& host, std::string& path) {
    constexpr std::string_view scheme = "https://";
    if (url.compare(0, scheme.size(), scheme) != 0) {
        return false;
    }
    const auto slash = url.find('/', scheme.size());
    if (slash == std::string::npos) {
        return false;
    }
    host = url.substr(0, slash);
    path = url.substr(slash);
    return HostAllowed(host);
}
} // namespace

std::optional<Resolved> ParseFileReply(std::string_view body, const FileKey& key, std::string* error) {
    const auto fail = [&](std::string text) -> std::optional<Resolved> {
        if (error) {
            *error = std::move(text);
        }
        return std::nullopt;
    };
    if (const auto code = ParseApiError(body)) {
        return fail(ApiErrorText(*code));
    }
    try {
        auto j = nlohmann::json::parse(body);
        if (j.is_array() && j.size() == 1) {
            j = j[0];
        }
        Resolved r;
        r.key = key;
        r.url = j.at("g").get<std::string>();
        r.size = j.at("s").get<u64>();
        std::string host, path;
        if (!SplitUrl(r.url, host, path)) {
            return fail("MEGA Gave an Unexpected Address");
        }
        if (j.contains("at") && j["at"].is_string()) {
            r.name = AttrName(j["at"].get<std::string>(),
                              std::string_view(reinterpret_cast<const char*>(key.aes.data()), 16));
        }
        return r;
    } catch (const std::exception&) {
        return fail(kNotFound);
    }
}

std::optional<std::vector<FolderFile>> ParseFolderReply(std::string_view body, std::string_view folder_key,
                                                        std::string* error) {
    const auto fail = [&](std::string text) -> std::optional<std::vector<FolderFile>> {
        if (error) {
            *error = std::move(text);
        }
        return std::nullopt;
    };
    if (folder_key.size() != 16) {
        return fail(kNotFound);
    }
    if (const auto code = ParseApiError(body)) {
        return fail(ApiErrorText(*code));
    }
    struct Node {
        std::string handle, parent, name, key;
        int type = 0;
        u64 size = 0;
    };
    std::vector<Node> nodes;
    try {
        auto j = nlohmann::json::parse(body);
        if (j.is_array() && j.size() == 1) {
            j = j[0];
        }
        for (const auto& n : j.at("f")) {
            Node node;
            node.handle = n.value("h", "");
            node.parent = n.value("p", "");
            node.type = n.value("t", 0);
            node.size = n.value("s", static_cast<u64>(0));
            if (node.type > 1) { // the share's root, trash and inbox: not listed
                continue;
            }
            std::string k = n.value("k", "");
            const auto colon = k.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            k.erase(0, colon + 1);
            k.erase(std::min(k.find('/'), k.size())); // one key per owner: the first
            const auto enc = Base64UrlDecode(k);
            if (!enc || (enc->size() != 16 && enc->size() != 32)) {
                continue;
            }
            node.key = EcbDecrypt(folder_key, *enc);
            const std::string aes = node.type == 0 ? Fold(node.key) : node.key;
            node.name = AttrName(n.value("a", ""), aes);
            if (node.handle.empty() || node.name.empty() || node.name.find('/') != std::string::npos) {
                continue;
            }
            nodes.push_back(std::move(node));
        }
    } catch (const std::exception&) {
        return fail(kNotFound);
    }
    std::map<std::string, const Node*> by_handle;
    for (const auto& n : nodes) {
        by_handle[n.handle] = &n;
    }
    std::vector<FolderFile> out;
    for (const auto& n : nodes) {
        if (n.type != 0 || n.key.size() != 32) {
            continue;
        }
        std::string path = n.name;
        const Node* p = n.parent.empty() ? nullptr : by_handle.count(n.parent) ? by_handle[n.parent] : nullptr;
        for (int depth = 0; p && p->type == 1 && depth < 32; ++depth) {
            path = p->name + "/" + path;
            p = p->parent.empty() || !by_handle.count(p->parent) ? nullptr : by_handle[p->parent];
        }
        out.push_back({n.handle, path, n.size, n.key});
    }
    std::sort(out.begin(), out.end(), [](const FolderFile& a, const FolderFile& b) { return a.path < b.path; });
    return out;
}

Net DefaultNet(Transfer::Stopper* stopper) {
    Net net;
    net.post = [stopper](const std::string& host, const std::string& path, const std::string& body) {
        Cheats::Online::RawRequest req;
        req.method = "POST";
        req.body = body;
        req.content_type = "application/json";
        const auto r = Cheats::Online::HttpsRequestRaw(host, path, req, 4 * 1024 * 1024, stopper);
        return Reply{r.status, r.body, r.error};
    };
    net.range = [stopper](const std::string& host, const std::string& path, u64 first, u64 last) {
        Cheats::Online::RawRequest req;
        const auto r = Cheats::Online::HttpsRequestRaw(
            host, fmt::format("{}/{}-{}", path, first, last), req, static_cast<std::size_t>(last - first + 1),
            stopper);
        return Reply{r.status, r.body, r.error};
    };
    return net;
}

namespace {

/// One API call; one retry when MEGA says "try again" (-3) or the connection failed.
bool Api(const Net& net, const std::string& query, const std::string& body, std::string& reply,
         std::string& error) {
    static std::atomic<int> seq{1};
    for (int attempt = 0; attempt < 2; ++attempt) {
        const Reply r = net.post(kApiHost, fmt::format("/cs?id={}{}", seq++, query), body);
        if (r.status == 0 || r.status == 500) {
            if (attempt == 0) {
                continue;
            }
            error = r.error.empty() ? "No reply" : r.error;
            return false;
        }
        if (r.status != 200) {
            error = fmt::format("Server returned {}", r.status);
            return false;
        }
        if (ParseApiError(r.body) == -3 && attempt == 0) {
            continue;
        }
        reply = r.body;
        return true;
    }
    return false;
}

} // namespace

std::optional<Resolved> Resolve(const Net& net, const Link& link, const std::string& file_pattern,
                                std::string* error) {
    std::string err, reply;
    const auto fail = [&](std::string text) -> std::optional<Resolved> {
        if (error) {
            *error = std::move(text);
        }
        return std::nullopt;
    };
    if (!link.folder) {
        const auto key = DeriveFileKey(link.key);
        if (!key) {
            return fail(kNotFound);
        }
        if (!Api(net, "", fmt::format(R"([{{"a":"g","g":1,"ssl":1,"p":"{}"}}])", link.handle), reply, err)) {
            return fail(err);
        }
        return ParseFileReply(reply, *key, error);
    }
    if (!Api(net, "&n=" + link.handle, R"([{"a":"f","c":1,"r":1}])", reply, err)) {
        return fail(err);
    }
    const auto files = ParseFolderReply(reply, link.key, &err);
    if (!files) {
        return fail(err);
    }
    const FolderFile* pick = nullptr;
    for (const auto& f : *files) {
        const std::string base = f.path.substr(f.path.rfind('/') == std::string::npos ? 0 : f.path.rfind('/') + 1);
        if (link.node.empty() ? (GlobMatch(file_pattern, f.path) || GlobMatch(file_pattern, base))
                              : f.handle == link.node) {
            pick = &f;
            break;
        }
    }
    if (!pick) {
        return fail("No matching file in the MEGA folder");
    }
    const auto key = DeriveFileKey(pick->key);
    if (!key) {
        return fail(kNotFound);
    }
    if (!Api(net, "&n=" + link.handle,
             fmt::format(R"([{{"a":"g","g":1,"ssl":1,"n":"{}"}}])", pick->handle), reply, err)) {
        return fail(err);
    }
    auto r = ParseFileReply(reply, *key, error);
    if (r && r->name.empty()) {
        r->name = pick->path.substr(pick->path.rfind('/') == std::string::npos ? 0 : pick->path.rfind('/') + 1);
    }
    return r;
}

RemoteFile::RemoteFile(const Net& n, Resolved f, const std::atomic_bool* c)
    : net(n), file(std::move(f)), cancel(c) {
    SplitUrl(file.url, host, path);
}

bool RemoteFile::Read(u64 offset, u64 len, std::string& out, std::string& error) {
    out.clear();
    if (offset > file.size) {
        error = "Read past the end";
        return false;
    }
    len = std::min(len, file.size - offset);
    out.reserve(static_cast<std::size_t>(len));
    u64 pos = offset;
    while (pos < offset + len) {
        if (cancel && cancel->load()) {
            error = "Cancelled";
            return false;
        }
        const u64 n = std::min<u64>(kChunk, offset + len - pos);
        Reply r;
        for (int attempt = 0; attempt < 2; ++attempt) { // one retry on a dropped connection
            r = net.range(host, path, pos, pos + n - 1);
            if (r.status != 0) {
                break;
            }
        }
        if (r.status == 509 || r.status == 503 || r.status == 429) {
            error = kQuota;
            return false;
        }
        if (r.status == 0) {
            error = r.error.empty() ? "No reply" : r.error;
            return false;
        }
        if (r.status == 404 || r.status == 403) {
            error = kNotFound;
            return false;
        }
        if ((r.status != 206 && r.status != 200) || r.body.size() != n) {
            error = r.status == 200 || r.status == 206 ? "Download incomplete" : fmt::format("Server returned {}", r.status);
            return false;
        }
        CtrDecryptor dec(file.key, pos);
        dec.Process(reinterpret_cast<u8*>(r.body.data()), r.body.size());
        out += r.body;
        pos += n;
        fetched += n;
        if (on_progress) {
            on_progress(fetched);
        }
    }
    return true;
}

namespace {
u16 U16(const std::string& s, std::size_t o) {
    return static_cast<u16>(static_cast<u8>(s[o]) | (static_cast<u8>(s[o + 1]) << 8));
}
u32 U32(const std::string& s, std::size_t o) {
    return static_cast<u32>(U16(s, o)) | (static_cast<u32>(U16(s, o + 2)) << 16);
}
} // namespace

bool ListZip(RemoteFile& file, std::vector<ZipEntry>& entries, bool& not_zip, std::string& error) {
    entries.clear();
    not_zip = false;
    const u64 size = file.Size();
    if (size < 22) {
        not_zip = true;
        return false;
    }
    std::string tail;
    const u64 tail_len = std::min<u64>(size, 22 + 65535);
    if (!file.Read(size - tail_len, tail_len, tail, error)) {
        return false;
    }
    std::size_t eocd = std::string::npos;
    for (std::size_t i = tail.size() - 22 + 1; i-- > 0;) {
        if (U32(tail, i) == 0x06054b50) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) {
        not_zip = true;
        return false;
    }
    const u32 count = U16(tail, eocd + 10), cd_size = U32(tail, eocd + 12), cd_off = U32(tail, eocd + 16);
    if (count == 0xFFFF || cd_size == 0xFFFFFFFFu || cd_off == 0xFFFFFFFFu) {
        error = "Zip64 Archives Are Not Supported";
        return false;
    }
    if (static_cast<u64>(cd_off) + cd_size > size || cd_size > 16u * 1024 * 1024) {
        error = "Bad Archive";
        return false;
    }
    std::string cd;
    if (!file.Read(cd_off, cd_size, cd, error)) {
        return false;
    }
    std::size_t p = 0;
    for (u32 i = 0; i < count; ++i) {
        if (p + 46 > cd.size() || U32(cd, p) != 0x02014b50) {
            error = "Bad Archive";
            return false;
        }
        ZipEntry e;
        const u16 flags = U16(cd, p + 8);
        e.method = U16(cd, p + 10);
        e.crc = U32(cd, p + 16);
        e.comp_size = U32(cd, p + 20);
        e.size = U32(cd, p + 24);
        const u16 name_len = U16(cd, p + 28), extra_len = U16(cd, p + 30), comment_len = U16(cd, p + 32);
        e.local_offset = U32(cd, p + 42);
        if (p + 46 + name_len + extra_len + comment_len > cd.size()) {
            error = "Bad Archive";
            return false;
        }
        e.name = cd.substr(p + 46, name_len);
        p += 46u + name_len + extra_len + comment_len;
        if (e.name.empty() || e.name.back() == '/') {
            continue; // a folder
        }
        if (flags & 1) {
            error = "Encrypted Archives Are Not Supported";
            return false;
        }
        const auto safe = SafeRelativePath(e.name);
        if (!safe) {
            error = "Unsafe Path in Archive";
            return false;
        }
        e.name = *safe;
        entries.push_back(std::move(e));
    }
    return true;
}

bool ReadZipEntry(RemoteFile& file, const ZipEntry& e, std::string& data, std::string& error) {
    std::string head;
    if (!file.Read(e.local_offset, 30, head, error)) {
        return false;
    }
    if (head.size() != 30 || U32(head, 0) != 0x04034b50) {
        error = "Bad Archive";
        return false;
    }
    const u64 start = e.local_offset + 30 + U16(head, 26) + U16(head, 28);
    std::string raw;
    if (start + e.comp_size > file.Size() || !file.Read(start, e.comp_size, raw, error)) {
        if (error.empty()) {
            error = "Bad Archive";
        }
        return false;
    }
    if (e.method == 0) {
        data = std::move(raw);
    } else if (e.method == 8) {
        auto out = Inflate(raw, static_cast<std::size_t>(e.size));
        if (!out) {
            error = "Bad Archive";
            return false;
        }
        data = std::move(*out);
    } else {
        error = "Unsupported Zip Method";
        return false;
    }
    if (data.size() != e.size || Crc32(data) != e.crc) {
        error = "Bad Archive";
        return false;
    }
    return true;
}

} // namespace Mods::Mega
