// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#ifdef __linux__
#include <cerrno>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#include "common/logging/log.h"
#include "frame_xr/input/phys_keyboard.h"

// AzaharFrame input2
namespace FrameXr::Input {

namespace {

// Linux's input-event-codes.h, kept here so the translator builds (and is tested) anywhere.
enum : int {
    kEsc = 1,
    kBackspace = 14,
    kTab = 15,
    kEnter = 28,
    kLeftCtrl = 29,
    kLeftShift = 42,
    kRightShift = 54,
    kLeftAlt = 56,
    kSpace = 57,
    kCapsLock = 58,
    kNumLock = 69,
    kKp7 = 71,
    kKp8 = 72,
    kKp9 = 73,
    kKp4 = 75,
    kKp5 = 76,
    kKp6 = 77,
    kKp1 = 79,
    kKp2 = 80,
    kKp3 = 81,
    kKp0 = 82,
    kKpDot = 83,
    kKpEnter = 96,
    kRightCtrl = 97,
    kRightAlt = 100,
    kHome = 102,
    kLeft = 105,
    kRight = 106,
    kEnd = 107,
    kDelete = 111,
    kLeftMeta = 125,
    kRightMeta = 126,
    kKeyA = 30,
    kKeyZ = 44,
    kEvKey = 1,
};

// The US layout from KEY_1 (2) to KEY_SLASH (53): unshifted, shifted; 0 for a key that types
// nothing.
constexpr const char* kPlain = "\0\0"
                               "1234567890-=\0\0"
                               "qwertyuiop[]\0\0"
                               "asdfghjkl;'`\0\\"
                               "zxcvbnm,./";
constexpr const char* kShifted = "\0\0"
                                 "!@#$%^&*()_+\0\0"
                                 "QWERTYUIOP{}\0\0"
                                 "ASDFGHJKL:\"~\0|"
                                 "ZXCVBNM<>?";
constexpr int kTableFirst = 0;
constexpr int kTableLast = 53;

bool IsLetter(int code) {
    return (code >= 16 && code <= 25) || (code >= 30 && code <= 38) || (code >= 44 && code <= 50);
}

} // namespace

char UsLayoutChar(int code, bool shift, bool caps_lock, bool num_lock) {
    if (code >= 2 && code <= kTableLast) {
        // Caps Lock turns letters only; Shift with it gives lower case again.
        const bool upper = IsLetter(code) ? shift != caps_lock : shift;
        const char c = (upper ? kShifted : kPlain)[code - kTableFirst];
        return c;
    }
    switch (code) {
    case kSpace:
        return ' ';
    case 55: // keypad *
        return '*';
    case 74: // keypad -
        return '-';
    case 78: // keypad +
        return '+';
    case 98: // keypad /
        return '/';
    case 86: // KEY_102ND (ISO keyboards' extra key, \ and | on a US layout)
        return shift ? '|' : '\\';
    default:
        break;
    }
    if (num_lock) {
        switch (code) {
        case kKp7:
            return '7';
        case kKp8:
            return '8';
        case kKp9:
            return '9';
        case kKp4:
            return '4';
        case kKp5:
            return '5';
        case kKp6:
            return '6';
        case kKp1:
            return '1';
        case kKp2:
            return '2';
        case kKp3:
            return '3';
        case kKp0:
            return '0';
        case kKpDot:
            return '.';
        default:
            break;
        }
    }
    return 0;
}

bool KeyTranslator::Shift() const {
    return down_[kLeftShift] || down_[kRightShift];
}

bool KeyTranslator::Command() const {
    return down_[kLeftCtrl] || down_[kRightCtrl] || down_[kLeftAlt] || down_[kRightAlt] ||
           down_[kLeftMeta] || down_[kRightMeta];
}

void KeyTranslator::SetLocks(bool caps_lock, bool num_lock) {
    caps_ = caps_lock;
    num_ = num_lock;
}

void KeyTranslator::Reset() {
    down_ = {};
}

KeyAction KeyTranslator::Key(int code, int value) {
    if (code < 0 || code >= static_cast<int>(down_.size())) {
        return {};
    }
    bool& down = down_[static_cast<std::size_t>(code)];
    if (value == 0) {
        down = false;
        return {};
    }
    if (value == 2 && !down) {
        return {}; // held from before reading started: never repeats
    }
    const bool press = value == 1;
    down = true;
    switch (code) {
    case kCapsLock:
        if (press) {
            caps_ = !caps_;
        }
        return {};
    case kNumLock:
        if (press) {
            num_ = !num_;
        }
        return {};
    case kBackspace:
        return {KeyActionKind::Backspace, {}};
    case kDelete:
        return {KeyActionKind::Delete, {}};
    case kLeft:
        return {KeyActionKind::Left, {}};
    case kRight:
        return {KeyActionKind::Right, {}};
    case kHome:
        return {KeyActionKind::Home, {}};
    case kEnd:
        return {KeyActionKind::End, {}};
    case kEnter:
    case kKpEnter:
        if (!press) {
            return {}; // no repeats: one answer
        }
        return {Shift() ? KeyActionKind::NewLine : KeyActionKind::Enter, {}};
    case kEsc:
        return press ? KeyAction{KeyActionKind::Escape, {}} : KeyAction{};
    case kTab:
        return {};
    default:
        break;
    }
    // The keypad's digits without Num Lock are its arrows and Home, End, Delete.
    if (!num_) {
        switch (code) {
        case kKp4:
            return {KeyActionKind::Left, {}};
        case kKp6:
            return {KeyActionKind::Right, {}};
        case kKp7:
            return {KeyActionKind::Home, {}};
        case kKp1:
            return {KeyActionKind::End, {}};
        case kKpDot:
            return {KeyActionKind::Delete, {}};
        default:
            break;
        }
    }
    if (Command()) {
        return {};
    }
    const char c = UsLayoutChar(code, Shift(), caps_, num_);
    if (c == 0) {
        return {};
    }
    return {KeyActionKind::Text, std::string(1, c)};
}

namespace {

bool ProcBit(const std::string& words, int bit) {
    std::vector<std::string> list;
    std::istringstream in(words);
    std::string w;
    while (in >> w) {
        list.push_back(w);
    }
    const std::size_t index = static_cast<std::size_t>(bit / 64);
    if (index >= list.size()) {
        return false;
    }
    const unsigned long long v = std::strtoull(list[list.size() - 1 - index].c_str(), nullptr, 16);
    return ((v >> (bit % 64)) & 1ULL) != 0;
}

constexpr unsigned kValveVendor = 0x28de;

} // namespace

std::vector<KeyboardNode> ParseKeyboards(const std::string& text) {
    std::vector<KeyboardNode> out;
    std::istringstream in(text);
    std::string line;
    KeyboardNode cur;
    unsigned vendor = 0;
    std::string ev, key;
    const auto finish = [&] {
        bool keys = !cur.event.empty() && vendor != kValveVendor && ProcBit(ev, kEvKey);
        for (const int k : {kKeyA, kKeyZ, kSpace, kEnter, kBackspace}) {
            keys = keys && ProcBit(key, k);
        }
        if (keys) {
            out.push_back(cur);
        }
        cur = {};
        vendor = 0;
        ev.clear();
        key.clear();
    };
    const auto value = [&line](std::size_t from) {
        std::string v = line.substr(std::min(from, line.size()));
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r')) {
            v.pop_back();
        }
        return v;
    };
    while (std::getline(in, line)) {
        if (line.empty() || line == "\r") {
            finish();
        } else if (line.rfind("I:", 0) == 0) {
            const std::size_t p = line.find("Vendor=");
            if (p != std::string::npos) {
                vendor = static_cast<unsigned>(std::strtoul(line.c_str() + p + 7, nullptr, 16));
            }
        } else if (line.rfind("N: Name=", 0) == 0) {
            std::string name = value(8);
            if (name.size() >= 2 && name.front() == '"' && name.back() == '"') {
                name = name.substr(1, name.size() - 2);
            }
            cur.name = name;
        } else if (line.rfind("H: Handlers=", 0) == 0) {
            std::istringstream words(line.substr(12));
            std::string w;
            while (words >> w) {
                if (w.rfind("event", 0) == 0) {
                    cur.event = w;
                }
            }
        } else if (line.rfind("B: EV=", 0) == 0) {
            ev = value(6);
        } else if (line.rfind("B: KEY=", 0) == 0) {
            key = value(7);
        }
    }
    finish();
    return out;
}

// ---- PhysicalKeyboards ----------------------------------------------------------------------

PhysicalKeyboards::~PhysicalKeyboards() {
    Close();
}

bool PhysicalKeyboards::Active() const {
    return std::any_of(nodes_.begin(), nodes_.end(), [](const Node& n) { return n.fd >= 0; });
}

void PhysicalKeyboards::Close() {
#ifdef __linux__
    for (Node& n : nodes_) {
        if (n.fd >= 0) {
            ::close(n.fd);
        }
    }
#endif
    if (open_) {
        LOG_INFO(Frontend, "xr: game keyboard: physical keyboards no longer read");
    }
    nodes_.clear();
    translator_.Reset();
    open_ = false;
}

void PhysicalKeyboards::Scan(int64_t now) {
    scanned_ns_ = now;
    rescan_ = false;
    std::string text;
    {
        std::ifstream in("/proc/bus/input/devices");
        std::ostringstream all;
        all << in.rdbuf();
        text = all.str();
    }
    const std::vector<KeyboardNode> found = ParseKeyboards(text);
    std::vector<Node> kept;
    for (Node& n : nodes_) {
        const bool still =
            n.fd >= 0 && std::any_of(found.begin(), found.end(),
                                     [&n](const auto& f) { return f.event == n.event; });
        if (still) {
            kept.push_back(n);
        } else {
#ifdef __linux__
            if (n.fd >= 0) {
                ::close(n.fd);
            }
#endif
        }
    }
    for (const KeyboardNode& f : found) {
        if (std::any_of(kept.begin(), kept.end(),
                        [&f](const Node& k) { return k.event == f.event; })) {
            continue;
        }
#ifdef __linux__
        const std::string path = "/dev/input/" + f.event;
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            if (std::find(failed_.begin(), failed_.end(), path) == failed_.end()) {
                failed_.push_back(path); // once per node
                LOG_WARNING(Frontend, "xr: game keyboard: cannot read {} ({})", path,
                            std::strerror(errno));
            }
            continue;
        }
        failed_.erase(std::remove(failed_.begin(), failed_.end(), path), failed_.end());
        // The lock keys as the keyboard shows them now.
        unsigned long leds[(LED_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
        if (::ioctl(fd, EVIOCGLED(sizeof(leds)), leds) >= 0) {
            const auto led = [&leds](int bit) {
                return ((leds[bit / (8 * sizeof(unsigned long))] >>
                         (bit % (8 * sizeof(unsigned long)))) &
                        1UL) != 0;
            };
            translator_.SetLocks(led(LED_CAPSL), led(LED_NUML));
        }
        LOG_INFO(Frontend, "xr: game keyboard: reading {} \"{}\"", path, f.name);
        kept.push_back({f.event, f.name, fd});
#endif
    }
    nodes_ = std::move(kept);
}

std::vector<KeyAction> PhysicalKeyboards::Poll(int64_t now) {
    std::vector<KeyAction> out;
    if (!open_ || rescan_ || now - scanned_ns_ >= 2'000'000'000 || now < scanned_ns_) {
        if (!open_) {
            translator_.Reset();
        }
        open_ = true;
        Scan(now);
    }
#ifdef __linux__
    for (Node& n : nodes_) {
        if (n.fd < 0) {
            continue;
        }
        input_event ev[64];
        while (true) {
            const ssize_t got = ::read(n.fd, ev, sizeof(ev));
            if (got <= 0) {
                if (got < 0 && errno != EAGAIN && errno != EINTR) {
                    LOG_INFO(Frontend, "xr: game keyboard: \"{}\" stopped reading: gone", n.name);
                    ::close(n.fd);
                    n.fd = -1;
                    rescan_ = true; // asleep or unplugged: look again next frame
                }
                break;
            }
            const std::size_t count = static_cast<std::size_t>(got) / sizeof(input_event);
            for (std::size_t i = 0; i < count; i++) {
                if (ev[i].type != EV_KEY) {
                    continue;
                }
                KeyAction a = translator_.Key(ev[i].code, ev[i].value);
                if (a.kind != KeyActionKind::None) {
                    out.push_back(std::move(a));
                }
            }
        }
    }
#endif
    return out;
}

} // namespace FrameXr::Input
