// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame jsoncrash: a texture pack's pack.json, read without ever throwing. A file without
// "options" (the one written for downloaded packs holds only a name and an author), with another
// type where a flag or a table should be, or that is not JSON at all, is a legacy pack: it used to
// be indexed through a const reference and abort the app with a json type_error 305.

#pragma once

#include <exception>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <json.hpp>

namespace VideoCore::PackConfig {

struct Config {
    bool valid = false; ///< "options" were there: not a legacy pack
    bool skip_mipmap = true;
    bool flip_png_files = true;
    bool use_new_hash = false;
    /// texture key (hex string) -> file names
    std::vector<std::pair<std::string, std::vector<std::string>>> textures;
};

inline Config Parse(std::string_view text) {
    Config out;
    try {
        const nlohmann::json json = nlohmann::json::parse(text, nullptr, false, true);
        if (!json.is_object()) {
            return out;
        }
        const auto options = json.find("options");
        if (options == json.end() || !options->is_object()) {
            return out;
        }
        const auto flag = [&](const char* key, bool& dest) {
            const auto it = options->find(key);
            if (it != options->end() && it->is_boolean()) {
                dest = it->get<bool>();
            }
        };
        flag("skip_mipmap", out.skip_mipmap);
        flag("flip_png_files", out.flip_png_files);
        flag("use_new_hash", out.use_new_hash);
        out.valid = true;
        const auto textures = json.find("textures");
        if (textures != json.end() && textures->is_object()) {
            for (const auto& [key, value] : textures->items()) {
                std::vector<std::string> files;
                if (value.is_string()) {
                    files.push_back(value.get<std::string>());
                } else if (value.is_array()) {
                    for (const auto& f : value) {
                        if (f.is_string()) {
                            files.push_back(f.get<std::string>());
                        }
                    }
                } else {
                    files.clear();
                }
                out.textures.emplace_back(key, std::move(files));
            }
        }
    } catch (const std::exception&) {
        out.valid = false;
    }
    return out;
}

} // namespace VideoCore::PackConfig
