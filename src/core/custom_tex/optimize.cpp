// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame ui3: see optimize.h.

#include "core/custom_tex/optimize.h"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>
#include <json.hpp>
#include "core/custom_tex/parts.h"

namespace CustomTex::Optimize {

namespace fs = std::filesystem;

namespace {

std::string Trimmed(std::string path) {
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

std::string ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

/// The numbers of "<word> A B C" (exactly three), nullopt otherwise.
std::optional<std::vector<u64>> Numbers(std::string_view line, std::string_view word) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
        line.remove_suffix(1);
    }
    if (line.substr(0, word.size()) != word || line.size() <= word.size() ||
        line[word.size()] != ' ') {
        return std::nullopt;
    }
    line.remove_prefix(word.size() + 1);
    std::vector<u64> out;
    while (!line.empty()) {
        const std::size_t end = std::min(line.find(' '), line.size());
        u64 v = 0;
        const auto [p, ec] = std::from_chars(line.data(), line.data() + end, v);
        if (ec != std::errc{} || p != line.data() + end) {
            return std::nullopt;
        }
        out.push_back(v);
        line.remove_prefix(std::min(end + 1, line.size()));
    }
    if (out.size() != 3) {
        return std::nullopt;
    }
    return out;
}

fs::path AskedFile(const std::string& load_dir) {
    return fs::path(load_dir) / "texture-packs" / "optimize-asked.txt";
}

} // namespace

std::string OptimizedPath(const std::string& pack) {
    std::string base = Trimmed(pack);
    constexpr std::string_view kOriginal = "-original";
    if (base.size() > kOriginal.size() &&
        base.compare(base.size() - kOriginal.size(), kOriginal.size(), kOriginal) == 0) {
        base.resize(base.size() - kOriginal.size());
    }
    return base + "-astc4x4";
}

bool IsOptimized(const std::string& pack) {
    std::error_code ec;
    return fs::is_regular_file(fs::path(pack) / kMarker, ec);
}

std::optional<std::string> SourceOf(const std::string& optimized) {
    if (!IsOptimized(optimized)) {
        return std::nullopt;
    }
    try {
        const auto j = nlohmann::json::parse(ReadFile(fs::path(optimized) / kMarker));
        const auto it = j.find("source");
        if (it == j.end() || !it->is_string() || it->get<std::string>().empty()) {
            return std::nullopt;
        }
        const fs::path source = (fs::path(Trimmed(optimized)) / it->get<std::string>()).lexically_normal();
        return Trimmed(source.string());
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

Pair PairOf(const std::string& pack, bool is_png) {
    Pair p;
    if (IsOptimized(pack)) {
        p.optimized = Trimmed(pack);
        p.png = SourceOf(pack).value_or("");
        std::error_code ec;
        if (!p.png.empty() && !fs::is_directory(p.png, ec)) {
            p.png.clear(); // the original was moved or deleted
        }
        return p;
    }
    if (is_png) {
        p.png = Trimmed(pack);
        const std::string copy = OptimizedPath(pack);
        if (IsOptimized(copy)) {
            p.optimized = copy;
        }
    }
    return p;
}

std::optional<Estimate> ParseEstimate(std::string_view line) {
    const auto n = Numbers(line, "estimate");
    if (!n) {
        return std::nullopt;
    }
    return Estimate{(*n)[0], (*n)[1], (*n)[2]};
}

std::optional<Progress> ParseProgress(std::string_view line) {
    const auto n = Numbers(line, "progress");
    if (!n || (*n)[0] > (*n)[1] || (*n)[2] > 1000 || (*n)[1] > 100'000'000) {
        return std::nullopt;
    }
    return Progress{static_cast<int>((*n)[0]), static_cast<int>((*n)[1]), static_cast<int>((*n)[2])};
}

std::string FailureText(int exit_code) {
    switch (exit_code) {
    case kExitNoSpace:
        return "Not Enough Space";
    case kExitNoEncoder:
        return "Encoder Not Built";
    case 2:
        return "Pack Not Readable";
    default:
        return "Conversion Failed";
    }
}

bool CarryParts(const std::string& from, const std::string& to) {
    std::error_code ec;
    const fs::path src = fs::path(from) / Parts::kStateFile;
    const fs::path dst = fs::path(to) / Parts::kStateFile;
    if (!fs::is_directory(to, ec)) {
        return false;
    }
    if (!fs::is_regular_file(src, ec)) {
        fs::remove(dst, ec);
        return !ec;
    }
    const fs::path tmp = dst.string() + ".part";
    fs::copy_file(src, tmp, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        return false;
    }
    fs::rename(tmp, dst, ec);
    return !ec;
}

bool AskedBefore(const std::string& load_dir, const std::string& pack_id) {
    std::istringstream in(ReadFile(AskedFile(load_dir)));
    for (std::string line; std::getline(in, line);) {
        if (line == pack_id) {
            return true;
        }
    }
    return false;
}

void MarkAsked(const std::string& load_dir, const std::string& pack_id) {
    if (pack_id.empty() || pack_id.find('\n') != std::string::npos || AskedBefore(load_dir, pack_id)) {
        return;
    }
    std::error_code ec;
    fs::create_directories(AskedFile(load_dir).parent_path(), ec);
    std::ofstream out(AskedFile(load_dir), std::ios::app);
    out << pack_id << '\n';
}

} // namespace CustomTex::Optimize
