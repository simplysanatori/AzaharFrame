// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame mega: downloading a mod (or texture pack) its author shares through a public MEGA link,
// no account and nothing re-hosted. MEGA encrypts the file; the key is in the link (after the '#').
//   file link    https://mega.nz/file/<handle>#<key>            (and the old  mega.nz/#!<handle>!<key>)
//   folder link  https://mega.nz/folder/<handle>#<key>[/file/<node>]  (old  mega.nz/#F!<handle>!<key>)
// MEGA's own API (a "g" request) answers with a temporary https address of the encrypted data; the
// data is AES-128-CTR (key and nonce folded from the link key, like megatools). Because CTR can start
// at any 16 byte block and the host answers Range requests, a file that is a zip is read from the
// back (its central directory), then only the entries the catalog entry's install rules keep are
// fetched and decrypted: a 640 MB archive with a 15 MB patch in it costs 15 MB.
// No Qt. Blocking: worker thread only. Hosts: g.api.mega.co.nz and <name>.userstorage.mega.co.nz.

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "common/common_types.h"

namespace Mods::Mega {

constexpr const char* kNotFound = "MEGA Link Not Found";
constexpr const char* kQuota = "MEGA Download Quota Reached, Try Again Later";

struct Link {
    bool folder = false;
    std::string handle; // the file's or folder's public handle
    std::string key;    // raw bytes: 32 for a file, 16 for a folder
    std::string node;   // a folder link that names one file: that node's handle ("" otherwise)
};
/// nullopt for anything that is not a MEGA file or folder link with a well-formed key.
std::optional<Link> ParseLink(std::string_view text);

/// URL-safe base64 (the '-' '_' alphabet, padding optional). nullopt on any other character.
std::optional<std::string> Base64UrlDecode(std::string_view text);

struct FileKey {
    std::array<u8, 16> aes{}; // the AES-128 key: the first half of the link key xor the second
    std::array<u8, 8> nonce{}; // the CTR nonce; the counter (block number) follows it
};
/// From the 32 raw key bytes of a file link: aes = k[0..16) ^ k[16..32) (as megatools does word by
/// word), nonce = k[16..24).
std::optional<FileKey> DeriveFileKey(std::string_view raw_key);

/// AES-128-CTR as MEGA uses it, starting at byte `offset` of the file. Encrypting and decrypting
/// are the same operation.
class CtrDecryptor {
public:
    CtrDecryptor(const FileKey& key, u64 offset);
    ~CtrDecryptor();
    void Process(u8* data, std::size_t size);
    std::string Process(std::string_view data);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

/// One file of a link, resolved through the API.
struct Resolved {
    std::string url; // https, the encrypted data (a Range is appended as "/first-last")
    u64 size = 0;
    std::string name; // "" when the attributes could not be read
    FileKey key;
};

/// A file inside a folder link.
struct FolderFile {
    std::string handle;
    std::string path; // names joined with '/', from the folder's top
    u64 size = 0;
    std::string key;  // raw bytes, 32
};

/// What the API said, as text for the player ("" when it was fine).
std::string ApiErrorText(int code);
/// "[-9]" or "-9" -> -9; nullopt when the body is not an error number.
std::optional<int> ParseApiError(std::string_view body);

/// The reply to {"a":"g","g":1,"ssl":1,"p"/"n":...}: the url, size and name. `error` set (a short
/// text, kNotFound ...) and nullopt on failure. The url must be https on MEGA's storage hosts.
std::optional<Resolved> ParseFileReply(std::string_view body, const FileKey& key, std::string* error);
/// The reply to {"a":"f","c":1,"r":1}: every file of the folder, with its decrypted key and path.
std::optional<std::vector<FolderFile>> ParseFolderReply(std::string_view body, std::string_view folder_key,
                                                        std::string* error);
bool HostAllowed(const std::string& host_with_scheme);

struct Reply {
    int status = 0; // 0: no reply
    std::string body;
    std::string error;
};
struct Net {
    /// POST of the JSON body to https://g.api.mega.co.nz<path>.
    std::function<Reply(const std::string& host, const std::string& path, const std::string& body)> post;
    /// GET of bytes [first, last] (inclusive) of host+path.
    std::function<Reply(const std::string& host, const std::string& path, u64 first, u64 last)> range;
};
Net DefaultNet();

/// Link -> the file to download. A folder link needs `file_pattern` (a name or path glob; the first
/// match in path order) unless the link names one file. `error` explains a failure.
std::optional<Resolved> Resolve(const Net& net, const Link& link, const std::string& file_pattern,
                                std::string* error);

/// Reads decrypted bytes of a resolved file, a Range at a time, counting what arrives.
class RemoteFile {
public:
    RemoteFile(const Net& net, Resolved file, const std::atomic_bool* cancel = nullptr);
    u64 Size() const { return file.size; }
    /// `len` bytes at `offset` (clamped to the end). False with `error` set ("Cancelled", kQuota ...).
    bool Read(u64 offset, u64 len, std::string& out, std::string& error);
    u64 BytesFetched() const { return fetched; }
    std::function<void(u64 fetched)> on_progress;

private:
    const Net& net;
    Resolved file;
    const std::atomic_bool* cancel;
    std::string host, path;
    u64 fetched = 0;
};

struct ZipEntry {
    std::string name;
    u16 method = 0;
    u32 crc = 0;
    u64 comp_size = 0;
    u64 size = 0;
    u64 local_offset = 0;
};
/// The files of the zip (directories left out), from its central directory. False when it is not a
/// zip (`not_zip` set), or is zip64 / encrypted / has an unsafe path (`error`).
bool ListZip(RemoteFile& file, std::vector<ZipEntry>& entries, bool& not_zip, std::string& error);
/// One entry's bytes, checked against its size and CRC32.
bool ReadZipEntry(RemoteFile& file, const ZipEntry& entry, std::string& data, std::string& error);

} // namespace Mods::Mega
