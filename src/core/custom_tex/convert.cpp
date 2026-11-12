// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// AzaharFrame cppconv: see convert.h. A port of convert_texture_pack.py.

#include "core/custom_tex/convert.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <thread>
#include <fmt/format.h>
#include <json.hpp>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#endif
#include "core/custom_tex/optimize.h"

namespace CustomTex::Convert {

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

namespace {

bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool Lexists(const fs::path& p) {
    std::error_code ec;
    return fs::symlink_status(p, ec).type() != fs::file_type::not_found;
}

/// Python's os.path.abspath: absolute, normal, no trailing slash.
std::string Abspath(const std::string& path) {
    fs::path p = fs::absolute(path).lexically_normal();
    std::string s = p.string();
    while (s.size() > 1 && s.back() == '/') {
        s.pop_back();
    }
    return s;
}

/// os.path.realpath
std::string Realpath(const std::string& path) {
    std::error_code ec;
    const fs::path c = fs::weakly_canonical(path, ec);
    return ec ? Abspath(path) : c.string();
}

/// os.path.relpath of two absolute paths (lexical).
std::string Relpath(const std::string& path, const std::string& start) {
    const fs::path r = fs::path(path).lexically_relative(start);
    return r.empty() ? std::string(".") : r.string();
}

bool ReadText(const std::string& path, std::string* text) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream out;
    out << in.rdbuf();
    *text = out.str();
    return true;
}

/// json.load with utf-8-sig and comments allowed.
bool ReadJson(const std::string& path, Json* value, std::string* error) {
    std::string text;
    if (!ReadText(path, &text)) {
        *error = path + ": cannot be read";
        return false;
    }
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) {
        text.erase(0, 3);
    }
    try {
        *value = Json::parse(StripJsonComments(text));
    } catch (const std::exception& e) {
        *error = path + ": " + e.what();
        return false;
    }
    return true;
}

/// json.dump(indent=4, ensure_ascii=False) and a newline, through a .part file.
bool WriteJson(const std::string& path, const Json& value) {
    const std::string tmp = path + ".part";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << value.dump(4) << "\n";
        out.flush();
        if (!out) {
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    return !ec;
}

/// Python truthiness of a JSON value.
bool Truthy(const Json& v) {
    if (v.is_null()) {
        return false;
    }
    if (v.is_boolean()) {
        return v.get<bool>();
    }
    if (v.is_number_integer() || v.is_number_unsigned()) {
        return v.get<long long>() != 0;
    }
    if (v.is_number_float()) {
        return v.get<double>() != 0.0;
    }
    if (v.is_string()) {
        return !v.get_ref<const std::string&>().empty();
    }
    return !v.empty();
}

/// os.link (following a symlink, as Python's does), else a copy that keeps the time; then os.replace.
bool LinkOrCopy(const std::string& src, const std::string& out) {
    const std::string tmp = out + ".part";
    std::error_code ec;
    if (Lexists(tmp)) {
        fs::remove(tmp, ec);
    }
    if (::linkat(AT_FDCWD, src.c_str(), AT_FDCWD, tmp.c_str(), AT_SYMLINK_FOLLOW) != 0) {
        fs::copy_file(src, tmp, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            return false;
        }
        fs::last_write_time(tmp, fs::last_write_time(src, ec), ec);
    }
    fs::rename(tmp, out, ec);
    return !ec;
}

u64 FileSize(const std::string& path) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    return ec ? 0 : size;
}

std::string KtxName(const std::string& rel) {
    return rel.substr(0, rel.size() - 4) + ".ktx";
}

/// Points the symlink `link` at `target` (relative) in one step.
bool SwapLink(const std::string& link, const std::string& target) {
    const std::string tmp = link + ".af-new";
    std::error_code ec;
    if (Lexists(tmp)) {
        fs::remove(tmp, ec);
    }
    fs::create_symlink(Relpath(target, fs::path(link).parent_path().string()), tmp, ec);
    if (ec) {
        return false;
    }
    fs::rename(tmp, link, ec);
    return !ec;
}

} // namespace

std::optional<std::pair<u32, u32>> PngSize(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    unsigned char head[24];
    in.read(reinterpret_cast<char*>(head), sizeof(head));
    static const unsigned char kSig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (in.gcount() < 24 || std::memcmp(head, kSig, 8) != 0 || std::memcmp(head + 12, "IHDR", 4) != 0) {
        return std::nullopt;
    }
    const auto be = [&](int at) {
        return (u32{head[at]} << 24) | (u32{head[at + 1]} << 16) | (u32{head[at + 2]} << 8) | head[at + 3];
    };
    return std::make_pair(be(16), be(20));
}

u64 KtxSize(u32 width, u32 height, const BlockSize& block) {
    u64 total = 64;
    while (true) {
        total += 4 + u64{(width + block.x - 1) / block.x} * ((height + block.y - 1) / block.y) * 16;
        if (width == 1 && height == 1) {
            return total;
        }
        width = std::max(width / 2, 1u);
        height = std::max(height / 2, 1u);
    }
}

std::string StripJsonComments(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    const size_t n = text.size();
    bool in_str = false;
    while (i < n) {
        const char c = text[i];
        if (in_str) {
            out.push_back(c);
            if (c == '\\' && i + 1 < n) {
                out.push_back(text[i + 1]);
                i++;
            } else if (c == '"') {
                in_str = false;
            }
        } else if (c == '"') {
            in_str = true;
            out.push_back(c);
        } else if (text.compare(i, 2, "//") == 0) {
            while (i < n && text[i] != '\n') {
                i++;
            }
            continue;
        } else if (text.compare(i, 2, "/*") == 0) {
            const size_t end = text.find("*/", i + 2);
            i = end == std::string::npos ? n : end + 2;
            continue;
        } else {
            out.push_back(c);
        }
        i++;
    }
    return out;
}

std::string DefaultOutput(const std::string& src_real, const std::string& block_name) {
    static const std::string kOriginal = "-original";
    const std::string base =
        EndsWith(src_real, kOriginal) ? src_real.substr(0, src_real.size() - kOriginal.size()) : src_real;
    return base + "-astc" + block_name;
}

std::vector<std::string> Scan(const std::string& src_real) {
    std::vector<std::string> files;
    std::set<std::string> seen;
    // os.walk(topdown, followlinks=True): a folder, then its sub folders in name order
    std::function<void(const fs::path&)> walk = [&](const fs::path& root) {
        if (!seen.insert(Realpath(root.string())).second) {
            return; // a link back to a folder already walked
        }
        std::vector<std::string> dirs, names;
        std::error_code ec;
        for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code ec2;
            (it->is_directory(ec2) ? dirs : names).push_back(it->path().filename().string());
        }
        std::sort(dirs.begin(), dirs.end());
        std::sort(names.begin(), names.end());
        for (const std::string& name : names) {
            if (name == kMarker || name == kPartsState || EndsWith(name, ".part")) {
                continue;
            }
            files.push_back(Relpath((root / name).string(), src_real));
        }
        for (const std::string& dir : dirs) {
            walk(root / dir);
        }
    };
    walk(fs::path(src_real));
    return files;
}

bool IsUpToDate(const std::string& src, const std::string& out) {
    std::error_code ec;
    if (fs::file_size(out, ec) == 0 || ec) {
        return false;
    }
    const auto out_time = fs::last_write_time(out, ec);
    if (ec) {
        return false;
    }
    const auto src_time = fs::last_write_time(src, ec);
    return !ec && out_time >= src_time;
}

std::string Human(u64 bytes) {
    if (bytes >= 1'000'000'000) {
        return fmt::format("{:.1f} GB", static_cast<double>(bytes) / 1e9);
    }
    return fmt::format("{:.0f} MB", static_cast<double>(bytes) / 1e6);
}

void LowerThreadPriority() {
#ifdef __linux__
    const pid_t tid = static_cast<pid_t>(::syscall(SYS_gettid));
    ::setpriority(PRIO_PROCESS, static_cast<id_t>(tid), 19);
    // ionice -c 3 (idle): ioprio_set(IOPRIO_WHO_PROCESS, tid, class 3 << 13)
#ifdef SYS_ioprio_set
    ::syscall(SYS_ioprio_set, 1, tid, 3 << 13);
#endif
#endif
}

// ---- The conversion -------------------------------------------------------------------------

Converter::Converter(Request request, ProgressFn progress)
    : request_(std::move(request)), progress_(std::move(progress)) {}

void Converter::Stop() {
    stop_ = true;
    std::lock_guard lock(encoders_mutex_);
    for (AstcEncoder* encoder : encoders_) {
        encoder->Cancel();
    }
}

namespace {

/// What a request means: the checks of convert_texture_pack.py's main() before the work.
struct Plan {
    std::string src, out;
    BlockSize block;
    float quality = 0;
    std::string error;
    int code = kOk;
    bool already = false;
};

Plan MakePlan(const Request& r) {
    Plan p;
    const auto block = ParseBlock(r.block);
    const auto quality = ParseQuality(r.quality);
    if (!block) {
        p.error = "Azahar reads ASTC 4x4, 6x6 and 8x6 only, not " + r.block;
        p.code = kUsage;
        return p;
    }
    if (!quality) {
        p.error = "bad quality " + r.quality;
        p.code = kUsage;
        return p;
    }
    p.block = *block;
    p.quality = *quality;
    const std::string link = Abspath(r.pack.empty() ? std::string("/") : r.pack);
    std::error_code ec;
    if (!fs::is_directory(link, ec)) {
        p.error = "no such folder: " + link;
        p.code = kUsage;
        return p;
    }
    p.src = Realpath(link);
    if (fs::is_regular_file(fs::path(p.src) / kMarker, ec)) {
        p.error = link + " is already an optimized pack";
        p.already = true;
        return p;
    }
    p.out = r.output.empty() ? DefaultOutput(p.src, r.block) : Abspath(r.output);
    const std::string out_real = Realpath(p.out);
    if (out_real == p.src || out_real.compare(0, p.src.size() + 1, p.src + "/") == 0) {
        p.error = "the output folder must be outside the pack";
        p.code = kUsage;
    }
    return p;
}

} // namespace

std::optional<Estimate> EstimatePack(const Request& request, std::string* error) {
    const Plan plan = MakePlan(request);
    if (!plan.error.empty()) {
        *error = plan.error;
        return std::nullopt;
    }
    const std::string config_path = plan.src + "/pack.json";
    std::error_code ec;
    if (fs::is_regular_file(config_path, ec)) {
        Json config;
        if (!ReadJson(config_path, &config, error)) {
            return std::nullopt;
        }
    }
    Estimate est;
    for (const std::string& rel : Scan(plan.src)) {
        if (!EndsWith(rel, ".png")) {
            continue;
        }
        const std::string src = plan.src + "/" + rel;
        const auto dims = PngSize(src);
        const u64 size = dims ? KtxSize(dims->first, dims->second, plan.block) : FileSize(src);
        est.textures++;
        est.total_bytes += size;
        if (!IsUpToDate(src, plan.out + "/" + KtxName(rel))) {
            est.need_bytes += size;
        }
    }
    return est;
}

Result Converter::Run() {
    Result r = RunInner();
    return r;
}

Result Converter::RunInner() {
    Result result;
    const Plan plan = MakePlan(request_);
    if (plan.already) {
        result.already_optimized = true;
        result.message = plan.error;
        return result;
    }
    if (!plan.error.empty()) {
        result.code = plan.code;
        result.message = plan.error;
        return result;
    }
    const std::string& src = plan.src;
    const std::string& out = plan.out;
    result.output = out;

    const std::string config_path = src + "/pack.json";
    std::error_code ec;
    std::optional<Json> config;
    if (fs::is_regular_file(config_path, ec)) {
        Json value;
        if (!ReadJson(config_path, &value, &result.message)) {
            result.code = kFailed;
            return result;
        }
        config = std::move(value);
    }
    // Azahar's defaults for a pack without pack.json flip PNGs too
    bool flip = true;
    if (config && config->is_object()) {
        const auto options = config->find("options");
        if (options != config->end() && options->is_object()) {
            const auto it = options->find("flip_png_files");
            if (it != options->end()) {
                flip = Truthy(*it);
            }
        }
    }

    const std::vector<std::string> files = Scan(src);
    std::vector<std::string> pngs, others;
    for (const std::string& f : files) {
        if (EndsWith(f, ".png")) {
            pngs.push_back(f);
        } else if (f != "pack.json") {
            others.push_back(f);
        }
    }

    // Work is counted in texels, so the progress and the estimate follow the big textures
    std::map<std::string, u64> work;
    u64 need = 0;
    for (const std::string& rel : pngs) {
        const std::string path = src + "/" + rel;
        const auto dims = PngSize(path);
        work[rel] = dims ? u64{dims->first} * dims->second : FileSize(path);
        const u64 size = dims ? KtxSize(dims->first, dims->second, plan.block) : FileSize(path);
        if (!IsUpToDate(path, out + "/" + KtxName(rel))) {
            need += size;
        }
    }

    fs::create_directories(out, ec);
    const auto space = fs::space(out, ec);
    if (!ec && need > space.available) {
        result.code = kNoSpace;
        result.message = fmt::format("this needs about {} free on the drive of {}; {} is free", Human(need),
                                     out, Human(space.available));
        return result;
    }

    for (const std::string& rel : others) {
        const std::string dst = out + "/" + rel;
        fs::create_directories(fs::path(dst).parent_path(), ec);
        if (!IsUpToDate(src + "/" + rel, dst) && !LinkOrCopy(src + "/" + rel, dst)) {
            result.code = kFailed;
            result.message = dst + ": cannot be written";
            return result;
        }
    }

    std::vector<std::string> todo;
    u64 done_work = 0;
    int done = 0;
    for (const std::string& rel : pngs) {
        const std::string dst = out + "/" + KtxName(rel);
        fs::create_directories(fs::path(dst).parent_path(), ec);
        if (IsUpToDate(src + "/" + rel, dst)) {
            done++;
            done_work += work[rel];
        } else {
            todo.push_back(rel);
        }
    }
    std::stable_sort(todo.begin(), todo.end(),
                     [&](const std::string& a, const std::string& b) { return work[a] > work[b]; });
    u64 total_work = 0;
    for (const auto& [rel, w] : work) {
        total_work += w;
    }
    total_work = std::max<u64>(total_work, 1);

    int jobs = request_.jobs;
    if (jobs <= 0) {
        unsigned cpus = std::thread::hardware_concurrency();
#ifdef __linux__
        cpu_set_t set;
        if (::sched_getaffinity(0, sizeof(set), &set) == 0) {
            cpus = static_cast<unsigned>(CPU_COUNT(&set));
        }
#endif
        jobs = std::max(1, static_cast<int>(cpus) - 1);
    }
    jobs = std::max(1, std::min<int>(jobs, static_cast<int>(std::max<size_t>(todo.size(), 1))));

    std::mutex mutex; // the counters, the failures, the callback
    const int total_files = static_cast<int>(pngs.size());
    const auto report = [&] {
        if (progress_) {
            progress_({done, total_files, static_cast<int>(done_work * 1000 / total_work)});
        }
    };
    {
        std::lock_guard lock(mutex);
        report();
    }

    std::atomic<size_t> next{0};
    const auto worker = [&] {
        if (request_.low_priority) {
            LowerThreadPriority();
        }
        AstcEncoder encoder(plan.block, plan.quality);
        {
            std::lock_guard lock(encoders_mutex_);
            encoders_.push_back(&encoder);
            if (Stopped()) {
                encoder.Cancel();
            }
        }
        while (!Stopped()) {
            const size_t index = next.fetch_add(1);
            if (index >= todo.size()) {
                break;
            }
            const std::string& rel = todo[index];
            std::string error;
            bool cancelled = false;
            bool too_big = false;
            if (const auto dims = PngSize(src + "/" + rel);
                dims && (dims->first > kMaxDimension || dims->second > kMaxDimension)) {
                too_big = true;
                error = fmt::format("{}x{} is larger than {} pixels a side", dims->first, dims->second,
                                    kMaxDimension);
            } else if (!encoder.Error().empty()) {
                error = encoder.Error();
            } else {
                error = encoder.EncodeFile(src + "/" + rel, out + "/" + KtxName(rel), flip, &cancelled);
            }
            if (cancelled || Stopped()) {
                break;
            }
            if (error.empty()) {
                // A PNG kept by an earlier run would be a second file for the same texture
                const std::string kept = out + "/" + rel;
                std::error_code rm;
                if (Lexists(kept)) {
                    fs::remove(kept, rm);
                }
            } else {
                // The PNG goes in as it is, so the pack still shows this texture
                LinkOrCopy(src + "/" + rel, out + "/" + rel);
            }
            std::lock_guard lock(mutex);
            if (!error.empty()) {
                result.kept_as_png.emplace_back(rel, error);
                result.oversized += too_big ? 1 : 0;
            }
            done++;
            done_work += work[rel];
            report();
        }
        std::lock_guard lock(encoders_mutex_);
        encoders_.erase(std::find(encoders_.begin(), encoders_.end(), &encoder));
    };
    {
        std::vector<std::thread> pool;
        for (int i = 0; i < jobs; i++) {
            pool.emplace_back(worker);
        }
        for (std::thread& t : pool) {
            t.join();
        }
    }

    if (Stopped()) {
        for (const std::string& rel : todo) {
            const std::string part = out + "/" + KtxName(rel) + ".part";
            if (Lexists(part)) {
                fs::remove(part, ec);
            }
        }
        result.code = kStopped;
        result.message = "stopped; run this again to carry on where it left off";
        return result;
    }

    // Python's as_completed order was completion order; the list is sorted for a stable log
    std::sort(result.kept_as_png.begin(), result.kept_as_png.end());
    std::set<std::string> failed_names; // base names, as the pack.json rename compares them
    for (const auto& [rel, error] : result.kept_as_png) {
        failed_names.insert(fs::path(rel).filename().string());
    }
    if (config) {
        const auto textures = config->is_object() ? config->find("textures") : config->end();
        if (textures != config->end() && textures->is_object()) {
            const auto rename = [&](Json& v) {
                if (!v.is_string()) {
                    return;
                }
                const std::string name = v.get<std::string>();
                if (!EndsWith(name, ".png") || failed_names.count(fs::path(name).filename().string())) {
                    return;
                }
                v = name.substr(0, name.size() - 4) + ".ktx";
            };
            for (auto& [key, value] : textures->items()) {
                if (value.is_array()) {
                    for (Json& v : value) {
                        rename(v);
                    }
                } else {
                    rename(value);
                }
            }
        }
        if (!WriteJson(out + "/pack.json", *config)) {
            result.code = kFailed;
            result.message = out + "/pack.json: cannot be written";
            return result;
        }
    }

    Json marker = Json::object();
    marker["format"] = "ASTC " + request_.block;
    marker["quality"] = request_.quality;
    marker["source"] = Relpath(src, out);
    marker["textures"] = static_cast<int>(pngs.size());
    marker["kept_as_png"] = static_cast<int>(result.kept_as_png.size());
    if (!WriteJson(out + "/" + kMarker, marker)) {
        result.code = kFailed;
        result.message = out + "/" + kMarker + ": cannot be written";
        return result;
    }
    const std::string state_src = src + "/" + kPartsState;
    const std::string state_out = out + "/" + kPartsState;
    if (fs::is_regular_file(state_src, ec) && !Lexists(state_out)) {
        fs::copy_file(state_src, state_out, ec);
        fs::last_write_time(state_out, fs::last_write_time(state_src, ec), ec);
    }
    u64 size = 0;
    for (fs::recursive_directory_iterator it(out, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code ec2;
        if (it->is_regular_file(ec2)) {
            size += FileSize(it->path().string());
        }
    }
    result.message = fmt::format("done: {} ({})", out, Human(size));
    return result;
}

// ---- Switching ------------------------------------------------------------------------------

bool Activate(const std::string& link_in, const std::string& out_dir_in, std::string* error) {
    const std::string link = Abspath(link_in);
    const std::string out_dir = Abspath(out_dir_in);
    std::error_code ec;
    std::string source;
    if (fs::is_symlink(link, ec)) {
        source = Realpath(link);
        if (!SwapLink(link, out_dir)) {
            *error = link + ": cannot be repointed";
            return false;
        }
    } else {
        source = link + "-original";
        if (Lexists(source)) {
            *error = source + " exists; move it away and run this again";
            return false;
        }
        fs::rename(link, source, ec);
        if (ec) {
            *error = link + ": " + ec.message();
            return false;
        }
        fs::create_symlink(Relpath(out_dir, fs::path(link).parent_path().string()), link, ec);
        if (ec) {
            *error = link + ": " + ec.message();
            return false;
        }
    }
    Json marker;
    std::string json_error;
    if (!ReadJson(out_dir + "/" + kMarker, &marker, &json_error)) {
        marker = Json::object();
    }
    marker["source"] = Relpath(source, out_dir);
    WriteJson(out_dir + "/" + kMarker, marker);
    Optimize::CarryParts(source, out_dir);
    return true;
}

bool Restore(const std::string& link_in, std::string* message, std::string* error) {
    const std::string link = Abspath(link_in);
    const std::string real = Realpath(link);
    const std::string marker_path = real + "/" + kMarker;
    std::error_code ec;
    if (!fs::is_regular_file(marker_path, ec)) {
        *message = link + " is not an optimized pack; nothing to do";
        return true;
    }
    Json marker;
    if (!ReadJson(marker_path, &marker, error)) {
        return false;
    }
    std::string source_rel;
    if (marker.is_object() && marker.contains("source") && marker["source"].is_string()) {
        source_rel = marker["source"].get<std::string>();
    }
    const std::string source = (fs::path(real) / source_rel).lexically_normal().string();
    std::string source_trim = source;
    while (source_trim.size() > 1 && source_trim.back() == '/') {
        source_trim.pop_back();
    }
    if (!fs::is_directory(source_trim, ec) || source_trim == real) {
        *error = "the original pack (" + source_trim + ") is missing";
        return false;
    }
    if (!fs::is_symlink(link, ec)) {
        *error = link + " is not a symlink; nothing was changed";
        return false;
    }
    Optimize::CarryParts(real, source_trim);
    if (source_trim == link + "-original") {
        fs::remove(link, ec);
        fs::rename(source_trim, link, ec);
        if (ec) {
            *error = link + ": " + ec.message();
            return false;
        }
    } else if (!SwapLink(link, source_trim)) {
        *error = link + ": cannot be repointed";
        return false;
    }
    *message = link + " uses the original pack again (" + source_trim + ")";
    return true;
}

} // namespace CustomTex::Convert
