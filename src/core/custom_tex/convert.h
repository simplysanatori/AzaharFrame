// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// AzaharFrame cppconv: the texture pack converter, "Optimize for Frame", in the app (it was
// steam-frame/tools/convert-texture-pack.sh + convert_texture_pack.py + the af-ktxenc helper).
//
// Every PNG of a pack becomes a .ktx of the same name (ASTC with a full mip chain, ktx_encode.h).
// Every other file is hard-linked or copied. pack.json is copied with the .png names of its
// "textures" map changed to .ktx. The new pack is a sibling folder of the original, named
// <pack>-astc<block> (CustomTex::Optimize::OptimizedPath), with the marker kMarker written last
// (optimize.h); the original is never changed. A second run only redoes PNGs newer than their
// .ktx, so a stopped run carries on where it left off.
//
// Converter::Run works on a pool of threads of its own and reports to a callback; Stop() from
// any thread ends it (a texture being encoded is abandoned, the others that are done stay).
// This module has no Qt: citra_qt/frame_mods_optimize.cpp (the VR menu) and
// citra_qt/frame_texture_pack.h (the Flat game list) run it on a thread of their own.

#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "core/custom_tex/ktx_encode.h"

namespace CustomTex::Convert {

/// Exit codes, as convert_texture_pack.py's (optimize.h's FailureText reads them).
inline constexpr int kOk = 0;
inline constexpr int kFailed = 1;
inline constexpr int kUsage = 2;
inline constexpr int kStopped = 4;
inline constexpr int kNoSpace = 5;

/// The largest PNG side the converter decodes (a bigger one is kept as PNG and counted).
inline constexpr u32 kMaxDimension = 16384;

/// The converted pack's own files, never copied from the original.
inline constexpr const char* kMarker = ".azaharframe-ktx.json";
inline constexpr const char* kPartsState = ".azaharframe-parts.json";

// ---- Pieces (public for the tests) --------------------------------------------------------

/// Width and height from the PNG header, or nullopt (not a PNG, or unreadable).
std::optional<std::pair<u32, u32>> PngSize(const std::string& path);
/// Bytes of a KTX with the full mip chain, as the encoder writes it.
u64 KtxSize(u32 width, u32 height, const BlockSize& block);
/// pack.json is read with comments allowed (Azahar does): drops // and /* */ outside strings.
std::string StripJsonComments(const std::string& text);
/// The converted pack's folder: "<pack>-astc<block>" ("-original" dropped from the pack's name).
std::string DefaultOutput(const std::string& src_real, const std::string& block_name);
/// Relative paths of every file of the pack, folders in name order, files in name order in each
/// (symlinked folders followed once; the markers and *.part files left out).
std::vector<std::string> Scan(const std::string& src_real);
/// `out` is there, not empty, and not older than `src`.
bool IsUpToDate(const std::string& src, const std::string& out);
/// "123 MB", or "1.2 GB" from a billion bytes up.
std::string Human(u64 bytes);

// ---- The conversion -------------------------------------------------------------------------

struct Request {
    std::string pack;   ///< the pack folder (pack.json at its root); a link is followed
    std::string output; ///< where the copy goes; "" = DefaultOutput
    std::string block = "4x4";
    std::string quality = "medium"; ///< fast, medium, thorough, or 0-100
    int jobs = 0;                   ///< textures at a time; <= 0: one less than the CPUs
    bool low_priority = true;       ///< the workers run at nice 19 and idle I/O (Linux)
};

struct Progress {
    int done = 0, total = 0, permille = 0;
};
using ProgressFn = std::function<void(const Progress&)>;

struct Estimate {
    u64 textures = 0, total_bytes = 0, need_bytes = 0;
};
/// How much there is, from the PNG headers (a second or two for a big pack). nullopt (with
/// `error`) when the pack is not readable or is an optimized copy already.
std::optional<Estimate> EstimatePack(const Request& request, std::string* error);

struct Result {
    int code = kOk;
    std::string message;                                       ///< what went wrong, or the summary
    std::string output;                                        ///< the copy's folder
    std::vector<std::pair<std::string, std::string>> kept_as_png; ///< texture, why
    int oversized = 0;                                         ///< PNGs over kMaxDimension, kept as PNG
    bool already_optimized = false;                            ///< the pack is a copy: nothing done
};

class Converter {
public:
    explicit Converter(Request request, ProgressFn progress = {});
    /// Runs to the end: kOk, kStopped (Stop), kNoSpace, kUsage (the arguments), kFailed.
    Result Run();
    /// From any thread, before or during Run. A stopped run removes its half-written files.
    void Stop();

private:
    bool Stopped() const {
        return stop_.load();
    }
    Result RunInner();

    Request request_;
    ProgressFn progress_;
    std::atomic<bool> stop_{false};
    std::mutex encoders_mutex_;
    std::vector<AstcEncoder*> encoders_; ///< the workers', to cancel the texture in hand
};

/// Makes the folder Azahar reads (load/textures/<TitleID>) use `out_dir`, the copy of the pack it
/// holds: a link is repointed; a real folder is renamed <name>-original and a link takes its
/// place. The Customize choices go over to the copy. False with `error`.
bool Activate(const std::string& link, const std::string& out_dir, std::string* error);
/// Undoes Activate: the link points at the original again (the copy stays). False with `error`;
/// `message` says what happened when it did not need doing.
bool Restore(const std::string& link, std::string* message, std::string* error);

/// Lowers the calling thread to nice 19 and idle I/O (Linux; nothing elsewhere).
void LowerThreadPriority();

} // namespace CustomTex::Convert
