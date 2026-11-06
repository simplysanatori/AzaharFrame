// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version

#include "frame_xr/picker/logo_loader.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <lodepng.h>

namespace frame_xr::picker {

namespace fs = std::filesystem;

std::string FindArtFile(const std::string& name) {
    std::vector<fs::path> dirs;
    if (const char* env = std::getenv("AZAHARFRAME_ART"); env && *env) {
        dirs.emplace_back(env);
    }
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        const fs::path dir = exe.parent_path();
        dirs.push_back(dir / "art");
        dirs.push_back(dir / ".." / ".." / ".." / "steam-frame" / "art");
    }
    for (const fs::path& dir : dirs) {
        const fs::path p = dir / name;
        if (fs::is_regular_file(p, ec)) {
            return p.string();
        }
    }
    return {};
}

bool LoadPngFile(const std::string& path, std::vector<uint8_t>& rgba, int& width, int& height) {
    rgba.clear();
    width = height = 0;
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),
                                           std::istreambuf_iterator<char>());
    unsigned w = 0, h = 0;
    if (bytes.empty() || lodepng::decode(rgba, w, h, bytes) != 0 || w == 0 || h == 0) {
        rgba.clear();
        return false;
    }
    width = static_cast<int>(w);
    height = static_cast<int>(h);
    return true;
}

} // namespace frame_xr::picker
