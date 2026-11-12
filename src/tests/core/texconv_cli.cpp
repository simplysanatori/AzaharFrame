// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
//
// AzaharFrame cppconv: core/custom_tex/convert.h as a program with the options of the old
// convert-texture-pack.sh (--block, --quality, --jobs, --activate, --restore, --estimate,
// --machine), so a pack converted by both can be compared. A development tool; not shipped.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "core/custom_tex/convert.h"

namespace C = CustomTex::Convert;

int main(int argc, char** argv) {
    C::Request request;
    bool estimate = false, machine = false, activate = false, restore = false;
    std::string pack, output;
    int positional = 0;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--block" && i + 1 < argc) {
            request.block = argv[++i];
        } else if (arg == "--quality" && i + 1 < argc) {
            request.quality = argv[++i];
        } else if (arg == "--jobs" && i + 1 < argc) {
            request.jobs = std::atoi(argv[++i]);
        } else if (arg == "--estimate") {
            estimate = true;
        } else if (arg == "--machine") {
            machine = true;
        } else if (arg == "--activate") {
            activate = true;
        } else if (arg == "--restore") {
            restore = true;
        } else if (arg == "--no-nice") {
            request.low_priority = false;
        } else if (!arg.empty() && arg[0] == '-') {
            std::fprintf(stderr, "usage: af_texconv [--block 4x4|6x6|8x6] [--quality Q] [--jobs N] "
                                 "[--no-nice] [--estimate] [--machine] [--activate|--restore] PACK [OUTPUT]\n");
            return 2;
        } else {
            (positional++ == 0 ? pack : output) = arg;
        }
    }
    if (pack.empty()) {
        return 2;
    }
    if (restore) {
        std::string message, error;
        const bool ok = C::Restore(pack, &message, &error);
        std::printf("%s\n", ok ? message.c_str() : error.c_str());
        return ok ? 0 : 1;
    }
    request.pack = pack;
    request.output = output;
    if (estimate) {
        std::string error;
        const auto est = C::EstimatePack(request, &error);
        if (!est) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        std::printf("estimate %llu %llu %llu\n", static_cast<unsigned long long>(est->textures),
                    static_cast<unsigned long long>(est->total_bytes),
                    static_cast<unsigned long long>(est->need_bytes));
        return 0;
    }
    const auto start = std::chrono::steady_clock::now();
    C::Converter converter(request, [&](const C::Progress& p) {
        if (machine) {
            std::printf("progress %d %d %d\n", p.done, p.total, p.permille);
            std::fflush(stdout);
        }
    });
    const C::Result r = converter.Run();
    for (const auto& [rel, why] : r.kept_as_png) {
        std::printf("kept as PNG: %s: %s\n", rel.c_str(), why.c_str());
    }
    std::printf("%s\n", r.message.c_str());
    if (r.code == 0 && activate) {
        std::string error;
        if (!C::Activate(pack, r.output, &error)) {
            std::fprintf(stderr, "error: %s\n", error.c_str());
            return 1;
        }
    }
    std::fprintf(stderr, "seconds %.2f\n",
                 std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    return r.code;
}
