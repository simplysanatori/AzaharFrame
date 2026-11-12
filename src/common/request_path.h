// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame emerald: a URL's path and query as they go on an HTTP request line.
//
// cpp-httplib encodes a request's path by default, and turns '+', ';', ',' and '\'' into
// %2B, %3B, %2C and %27. A signed URL (GitHub's release-assets.githubusercontent.com redirect,
// an Azure SAS URL) must reach the server byte for byte: its query has "attachment%3B+filename"
// with '+' for a space, and once that '+' becomes %2B the server reads a different value and
// answers 403 AuthenticationFailed. So the clients that follow redirects turn httplib's
// encoding off (set_url_encode(false)) and send RequestPath(path): the text as the server or
// our own code wrote it, with only the bytes that cannot go on a request line (controls, space,
// DEL, non-ASCII) percent-encoded.

#pragma once

#include <string>
#include <string_view>

namespace Common {

inline std::string RequestPath(std::string_view path) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(path.size());
    for (const char ch : path) {
        const auto c = static_cast<unsigned char>(ch);
        if (c <= 0x20 || c >= 0x7F) {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0xF];
        } else {
            out += ch;
        }
    }
    return out;
}

} // namespace Common
