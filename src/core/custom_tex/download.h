// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame texdl: downloading 3DS texture packs from where their authors host them. No Qt.
// Nothing is rehosted. A curated catalog (catalog/textures.json in the maintainer's repo, with a copy
// compiled in) says which pack serves which title and where its author keeps it:
//   github_release  a release asset of the author's repository
//   https           a direct https link (the catalog entry may name extra exact hosts for redirects)
//   gdrive          a Google Drive file id
//   gdrive_folder   a public Google Drive folder and a file name pattern in it (listed with the folder's
//                   embedded view, no API key; subfolders searched, capped)
// An entry may list `alternates` (more sources for the same file, e.g. a mirror) tried when the first
// fails with a quota, not-found or not-shared error.
// Hosts: github.com and GitHub's asset hosts, drive.google.com, drive.usercontent.google.com, plus
// the exact hosts a catalog entry names. https only, no wildcards. Contacted only on a click.
//
// API for the VR menu's Textures segment ("Download Packs"). Everything runs on one worker thread
// owned by the Downloader; every query below is safe from any thread, none blocks long.
//
//   Downloader dl;                                    // once per session; loads the compiled-in catalog
//   dl.RefreshCatalogAsync();                         // optional, one GET; the list updates when done
//   dl.ListForTitle(title_id)   -> vector<PackInfo>   // available packs: name, author, size, state
//   dl.Start(pack_id, title_id) -> bool               // false when busy / unknown / not for this title
//   dl.GetState()               -> State              // phase, text, bytes_done / bytes_total, error
//   dl.Cancel()                                       // stops soon; the partial download is kept
//   dl.DiscardPartial(pack_id)                        // frees the kept partial download
//   dl.Delete(title_id, folder) -> Result             // removes a downloaded pack's folder
//
// Phases: Idle, Resolving, Downloading, Verifying, Extracting, Done, Failed, Cancelled. State::text
// is a short label for the line under the progress bar ("Downloading", "Checking", "Unpacking"); State::error is
// set in Failed ("Google Drive: too many downloads of this file today, try again later"). After Done the
// pack shows up in CustomTex::ListPacks (it is installed into <load>/texture-packs/<TITLEID>/<folder>/,
// the managed library; Azahar still reads load/textures/<TITLEID>, which the player's pack choice
// points at). A game restart applies it (CustomTex::kReloadSupported).
//
// Downloads go to <load>/texture-downloads/<id>.part and resume with a Range request when the host
// allows it; the free disk space is checked first; the archive is deleted after unpacking.

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "common/common_types.h"

namespace CustomTex::Download {

struct Source {
    enum class Type { GithubRelease, Https, Gdrive, GdriveFolder };
    Type type = Type::GithubRelease;
    std::string repo;          // "owner/name"
    std::string tag = "latest";
    std::string asset_pattern; // '*' = any text
    std::string url;           // Https
    std::string file_id;       // Gdrive
    std::string folder_id;     // GdriveFolder: a public folder, listed without an API key
    std::string file_pattern;  // GdriveFolder: file name to pick ('*' = any text), searched in subfolders too
    std::vector<std::string> hosts; // extra exact hosts (bare names) the download may be redirected to
};

enum class Layout {
    Single,   // one pack: the folder that holds the textures (wrapper folders are dropped)
    Variants, // each folder under the common parent is its own pack
};

struct Entry {
    std::string id;
    std::string name;
    std::string title;
    std::vector<u64> title_ids;
    std::string author;
    std::string author_url;
    Source source;
    std::vector<Source> alternates; // tried in order when `source` fails (quota, gone, not shared)
    u64 size = 0;       // bytes, 0 unknown (an estimate; the real size comes from the host)
    std::string sha256; // lower-case hex, "" unknown
    std::string format; // "auto", "zip", "7z", "rar"
    Layout layout = Layout::Single;
    std::string folder; // the pack folder's base name
    std::string licence;
    std::string notes;

    bool ServesTitle(u64 title_id) const;
};

struct Catalog {
    int schema = 1;
    std::vector<Entry> packs;
    const Entry* Find(std::string_view id) const;
};

/// Parses catalog/textures.json; entries that break the rules (bad id, title ID, repo, url, Drive id)
/// are dropped with a note in `warnings`. nullopt when the text is not a catalog.
std::optional<Catalog> ParseCatalog(std::string_view json, std::string* error = nullptr,
                                    std::vector<std::string>* warnings = nullptr);
const char* DefaultCatalogJson();
Catalog DefaultCatalog();

constexpr const char* kOwnerRepo = "simplysanatori/3ds-game-fixes";
constexpr const char* kCatalogPath = "catalog/textures.json";

// ---------------------------------------------------------------------------------------------
// Pure parts (tested without a network)

/// "https://host" is allowed: GitHub's hosts, the two Google Drive hosts, or one of `extra` (bare names).
bool HostAllowed(const std::string& host_with_scheme, const std::vector<std::string>& extra = {});
bool SplitUrl(const std::string& url, std::string& host_with_scheme, std::string& path);

bool DriveIdOk(std::string_view id);
/// https://drive.usercontent.google.com/download?id=ID&export=download&confirm=t
std::string DriveUrl(std::string_view file_id);

struct DrivePage {
    enum class Kind { Unknown, Confirm, Quota, NoAccess, NotFound };
    Kind kind = Kind::Unknown;
    std::string url;   // Confirm: the https address of the real download
    std::string error; // Quota, NoAccess, NotFound, Unknown: a short message for the player
};
/// Reads the HTML page Drive answers with instead of a file: the "can't scan for viruses" form (the
/// form action and its hidden fields become the new address), a quota message, or a sign-in wall.
DrivePage ParseDrivePage(std::string_view html);

struct DriveItem {
    std::string id;
    std::string name;
    bool is_folder = false;
};
/// The entries of Drive's embedded folder view (https://drive.google.com/embeddedfolderview?id=ID):
/// id, name and whether it is a subfolder. Empty for a page with no entries.
std::vector<DriveItem> ParseDriveFolder(std::string_view html);
/// A lister for one folder id; nullopt when it cannot be read.
using DriveLister = std::function<std::optional<std::vector<DriveItem>>(const std::string& folder_id)>;
/// Finds the file whose name matches `pattern` in the folder or its subfolders (breadth first, names
/// sorted, at most `max_depth` levels and `max_listings` folder reads). `error` says why not.
std::optional<DriveItem> PickDriveFile(const DriveLister& list, const std::string& folder_id,
                                       const std::string& pattern, std::string* error, int max_depth = 3,
                                       int max_listings = 40);

struct PackFolder {
    std::string dir;  // folder inside the archive ("" = its top)
    std::string name; // variant label, "" for a single pack
};
/// Where the textures are in an archive, from its file paths ('/' separated, files only): the
/// folders that hold tex1_*.png|dds|ktx. Single: the folder of a pack.json that covers every texture
/// folder, else the lowest common parent of them. Variants: each child of that parent. Empty when
/// there are no textures.
std::vector<PackFolder> FindPackFolders(const std::vector<std::string>& files, Layout layout);

/// Bytes to have free before a download of `archive_size` bytes (archive plus its unpacked copy plus
/// room to spare).
u64 RequiredSpace(u64 archive_size, u64 already_downloaded = 0);
/// "" when `required` fits in `available`, else "Not enough free space: 3.2 GB needed, 1.1 GB free".
std::string FreeSpaceError(u64 required, u64 available);
/// Free bytes of the file system holding `dir` (nullopt on error).
std::optional<u64> FreeSpace(const std::string& dir);

/// "gb" style text for sizes: "78 MB", "1.8 GB".
std::string SizeText(u64 bytes);

std::string SanitizeFolderName(std::string_view name);

// ---------------------------------------------------------------------------------------------

struct PackInfo {
    std::string id;
    std::string name;
    std::string author;
    std::string author_url;
    std::string licence;
    std::string notes;
    u64 size = 0;
    std::string size_text;          // "1.8 GB", "" unknown
    bool installed = false;         // a folder from this entry is in the library
    std::vector<std::string> folders; // installed folders (names) for Delete
    bool partial = false;           // a kept partial download exists
};

struct State {
    enum class Phase { Idle, Resolving, Downloading, Verifying, Extracting, Done, Failed, Cancelled };
    Phase phase = Phase::Idle;
    std::string pack_id;
    std::string text;
    u64 bytes_done = 0;
    u64 bytes_total = 0; // 0 unknown
    std::string error;   // Failed only
    std::vector<std::string> installed; // Done: the folder names
    bool Busy() const {
        return phase == Phase::Resolving || phase == Phase::Downloading || phase == Phase::Verifying ||
               phase == Phase::Extracting;
    }
};

struct Result {
    bool ok = false;
    std::string error;
};

class Downloader {
public:
    /// `load_dir` empty = FileUtil's LoadDir.
    explicit Downloader(std::string load_dir = {}, Catalog catalog = DefaultCatalog());
    ~Downloader();
    Downloader(const Downloader&) = delete;
    Downloader& operator=(const Downloader&) = delete;

    std::vector<PackInfo> ListForTitle(u64 title_id) const;
    bool Start(const std::string& pack_id, u64 title_id);
    State GetState() const;
    void Cancel();
    void DiscardPartial(const std::string& pack_id);
    /// Removes <load>/texture-packs/<TITLEID>/<folder> (a folder one of this module's downloads made);
    /// deactivates it first when it is the one the game loads.
    Result Delete(u64 title_id, const std::string& folder);

    void RefreshCatalogAsync();
    void SetCatalog(Catalog catalog);

    std::string LoadDir() const { return load_dir_; }

private:
    void Run(Entry entry, u64 title_id);
    std::string FetchSource(const Entry& entry, const Source& source, const std::string& part);
    void SetState(State::Phase phase, const std::string& text);
    void Fail(const std::string& error);

    std::string load_dir_;
    mutable std::mutex mutex_;
    Catalog catalog_;
    State state_;
    std::atomic_bool cancel_{false};
    std::thread worker_;
    std::thread refresher_;
    bool refreshing_ = false;
    std::atomic<u64> done_{0};
    std::atomic<u64> total_{0};
};

// Installing a finished archive (exposed for tests).

/// Unpacks `archive_path` (zip, 7z or rar, kind from the file's first bytes), finds the textures and
/// moves each pack folder to <load_dir>/texture-packs/<TITLEID>/<name>. `installed` gets the folder
/// names. "" on success, else the error ("No textures found in the download", "Cancelled", ...).
std::string InstallArchive(const std::string& archive_path, const Entry& entry, u64 title_id,
                           const std::string& load_dir, const std::atomic_bool* cancel,
                           std::vector<std::string>* installed, const std::string& tool_override = {});

} // namespace CustomTex::Download
