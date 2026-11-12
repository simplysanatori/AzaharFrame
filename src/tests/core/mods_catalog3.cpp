// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame catalog3: the catalog's textures block, multipart downloads (multi-volume RAR, split 7z,
// split zip), the free space check, and the new "downloads" / "updated" fields. Fake network; the real
// multipart install is hidden: [.network].

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <catch2/catch_test_macros.hpp>
#include "core/custom_tex/download.h"
#include "core/mods/library.h"

using namespace Mods;
namespace fs = std::filesystem;
using V = std::vector<std::string>;

namespace {

constexpr u64 kTid = 0x0004000000033500ULL;
constexpr const char* kTidText = "0004000000033500";

// Written with 7-Zip (stored): dir/a.txt ("seven-a\n" x100), b.bin ("hello seven").
const unsigned char kSevenZip[] = {
  0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c, 0x00, 0x04, 0x62, 0x51, 0x52, 0xc6,
  0x9d, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x28, 0x5c, 0x34, 0xd7, 0x68, 0x65, 0x6c, 0x6c,
  0x6f, 0x20, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73,
  0x65, 0x76, 0x65, 0x6e, 0x2d, 0x61, 0x0a, 0x73, 0x65, 0x76, 0x65, 0x6e,
  0x2d, 0x61, 0x0a, 0x00, 0x00, 0x81, 0x33, 0x07, 0xae, 0x31, 0x98, 0xfb,
  0xdc, 0x6b, 0xc6, 0x12, 0xb2, 0x90, 0x58, 0x0d, 0xbe, 0xcf, 0xfe, 0xfd,
  0x9d, 0xfe, 0x04, 0xad, 0xde, 0x86, 0x07, 0x70, 0x9d, 0x54, 0x17, 0x96,
  0xbb, 0x8e, 0x43, 0x1a, 0xfc, 0x42, 0xcf, 0x70, 0xee, 0x72, 0xef, 0xb8,
  0x1d, 0x5f, 0x19, 0xd6, 0x35, 0x72, 0xa4, 0x49, 0x56, 0x89, 0x47, 0xde,
  0x05, 0x2e, 0x27, 0x57, 0x5b, 0x4a, 0x29, 0x83, 0xab, 0x64, 0x3e, 0xc7,
  0x8d, 0xbc, 0x50, 0xed, 0x93, 0xa3, 0xea, 0x0f, 0x38, 0x7f, 0xd1, 0x09,
  0xf2, 0x08, 0x47, 0xa1, 0x2c, 0x3e, 0xac, 0x2f, 0xae, 0xdb, 0xed, 0xa6,
  0x29, 0x85, 0x4f, 0xd0, 0xe2, 0xec, 0x59, 0x9c, 0x1c, 0xe4, 0xb1, 0x4a,
  0x8b, 0x4d, 0xbf, 0xc2, 0x11, 0x3b, 0x80, 0x00, 0x00, 0x17, 0x06, 0x83,
  0x2b, 0x01, 0x09, 0x72, 0x00, 0x07, 0x0b, 0x01, 0x00, 0x01, 0x23, 0x03,
  0x01, 0x01, 0x05, 0x5d, 0x00, 0x10, 0x00, 0x00, 0x0c, 0x80, 0x8a, 0x0a,
  0x01, 0x5a, 0xea, 0x9e, 0xbd, 0x00, 0x00
};

const unsigned char kSolidRar[] = {
  0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00, 0x3b, 0xd0, 0x73, 0x08, 0x00,
  0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0xa3, 0x74, 0xc0,
  0x90, 0x35, 0x00, 0x4f, 0x01, 0x00, 0x00, 0x4b, 0x01, 0x00, 0x00, 0x02,
  0x6b, 0xaf, 0x42, 0x9f, 0x85, 0xa4, 0xee, 0x5a, 0x1d, 0x35, 0x10, 0x00,
  0x20, 0x00, 0x00, 0x00, 0x69, 0x6e, 0x73, 0x74, 0x72, 0x75, 0x63, 0x74,
  0x69, 0x6f, 0x6e, 0x73, 0x2e, 0x74, 0x78, 0x74, 0x00, 0xb0, 0xa8, 0xbe,
  0x1c, 0x0e, 0x21, 0x51, 0x50, 0xc8, 0xbd, 0x1c, 0x15, 0x82, 0x41, 0x90,
  0x19, 0x56, 0x0a, 0x84, 0x44, 0x81, 0x27, 0x49, 0x01, 0x42, 0x20, 0x81,
  0x25, 0xb2, 0x09, 0x2c, 0x58, 0x12, 0xb1, 0x06, 0x0a, 0xd6, 0x24, 0x60,
  0x49, 0x46, 0x3b, 0x6d, 0xd2, 0xda, 0x96, 0xdd, 0xa5, 0xbb, 0x60, 0x59,
  0x0f, 0x0d, 0x34, 0xb5, 0x1a, 0x05, 0x2a, 0x31, 0xb7, 0x46, 0x92, 0x5a,
  0xf8, 0x58, 0xea, 0x2c, 0x8b, 0x08, 0xb2, 0x57, 0xd5, 0xf1, 0x53, 0xca,
  0xd8, 0x45, 0xf4, 0x24, 0x24, 0x53, 0xc9, 0x8f, 0xc0, 0xdf, 0x05, 0x97,
  0xf0, 0x0b, 0xbb, 0xdd, 0xd3, 0x36, 0xd8, 0xfc, 0x29, 0xf0, 0xef, 0xe1,
  0x99, 0xde, 0x67, 0x33, 0x9c, 0xcd, 0xdf, 0x8f, 0x73, 0x7b, 0xcc, 0xe7,
  0x7b, 0x99, 0xb9, 0xde, 0xe7, 0x27, 0xf7, 0xff, 0xc2, 0xbf, 0xfc, 0x5a,
  0x90, 0xed, 0x9e, 0x67, 0x0e, 0x4d, 0xf8, 0x99, 0x14, 0xe9, 0xf0, 0x8b,
  0x6a, 0x48, 0xb2, 0x4a, 0x6e, 0xcc, 0x97, 0x62, 0xcc, 0x6c, 0x84, 0xa5,
  0x1b, 0x72, 0x9c, 0xb4, 0x13, 0xc2, 0xdd, 0xb6, 0x0e, 0x0a, 0xc9, 0x13,
  0x97, 0xae, 0x18, 0xa3, 0xcf, 0x2e, 0xdb, 0x92, 0x59, 0x61, 0xc8, 0x91,
  0x62, 0xca, 0x12, 0x15, 0x15, 0x22, 0xec, 0xb1, 0xca, 0x79, 0xe7, 0x23,
  0x8f, 0x58, 0x21, 0x50, 0x65, 0x0f, 0x2f, 0x1e, 0xa4, 0xc0, 0x9a, 0x10,
  0xe2, 0x88, 0xe8, 0x82, 0xd9, 0x35, 0x50, 0x83, 0x61, 0xc0, 0x97, 0x24,
  0x31, 0x19, 0x99, 0x8a, 0xdd, 0xa8, 0x5a, 0x40, 0xe5, 0x2f, 0x4b, 0x4a,
  0xc4, 0x71, 0x12, 0x13, 0x12, 0x8a, 0x2d, 0x41, 0x00, 0x60, 0x6d, 0x25,
  0x96, 0x55, 0xc2, 0x45, 0x32, 0x49, 0x27, 0x64, 0x5f, 0xff, 0x92, 0x5a,
  0x14, 0x93, 0xc2, 0x46, 0x40, 0xd5, 0xd7, 0x50, 0x2e, 0x4a, 0x21, 0x9d,
  0xdb, 0x82, 0x99, 0xa6, 0x58, 0xcf, 0xc2, 0xb6, 0x4d, 0xa7, 0xc5, 0x3d,
  0x13, 0x31, 0xed, 0xc9, 0x40, 0x60, 0x84, 0xd6, 0x52, 0x31, 0xcd, 0xe3,
  0xe4, 0xa2, 0x25, 0xc2, 0x86, 0x48, 0x74, 0xe9, 0xbd, 0x37, 0x6b, 0xa4,
  0xc5, 0xd3, 0xca, 0x69, 0x37, 0x23, 0x82, 0x7c, 0xba, 0xa6, 0x29, 0x45,
  0x55, 0xcd, 0x39, 0xd4, 0x82, 0x64, 0x93, 0x44, 0x8d, 0x29, 0xa1, 0x4b,
  0xd5, 0xe1, 0x2d, 0xd2, 0x81, 0x25, 0x13, 0xc3, 0xc2, 0xf5, 0x23, 0xc5,
  0xc3, 0x93, 0x48, 0x6a, 0x90, 0xe8, 0x32, 0xf2, 0x41, 0x79, 0xfd, 0x00,
  0x8c, 0x32, 0x74, 0xd0, 0x90, 0x3e, 0x00, 0xdc, 0x01, 0x00, 0x00, 0x80,
  0x02, 0x00, 0x00, 0x02, 0x1c, 0xdf, 0x44, 0x21, 0x42, 0xa4, 0xee, 0x5a,
  0x1d, 0x35, 0x19, 0x00, 0x20, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x34,
  0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x44, 0x35, 0x41, 0x30, 0x30,
  0x5c, 0x63, 0x6f, 0x64, 0x65, 0x2e, 0x62, 0x70, 0x73, 0x00, 0xf0, 0x57,
  0x90, 0x67, 0xc1, 0x5f, 0xdd, 0xbd, 0xa8, 0xea, 0xe6, 0x67, 0xa9, 0xa3,
  0x33, 0xbb, 0xfd, 0x88, 0x32, 0xf3, 0x70, 0xed, 0x9f, 0xd9, 0x19, 0xfc,
  0xa8, 0x2a, 0xd5, 0xff, 0xeb, 0x7b, 0xe2, 0xa8, 0xcf, 0xc6, 0xd5, 0x5c,
  0xe7, 0x44, 0xca, 0x24, 0x7d, 0x96, 0x10, 0xa4, 0x73, 0xf4, 0x31, 0x0c,
  0xde, 0xbc, 0xf1, 0x2a, 0x9b, 0xb7, 0x3d, 0x33, 0x6c, 0x68, 0xf4, 0xeb,
  0x2e, 0x32, 0x29, 0x7d, 0x99, 0x38, 0xa6, 0x46, 0x59, 0xde, 0x3d, 0x7a,
  0x27, 0x7c, 0x6a, 0x28, 0x75, 0xa1, 0x8b, 0x8d, 0x3f, 0x8c, 0x62, 0xb3,
  0x64, 0x3d, 0x37, 0x12, 0x7a, 0xb7, 0x4a, 0x6c, 0x8c, 0xb8, 0x01, 0xd6,
  0xdf, 0x46, 0x72, 0x2d, 0x8f, 0x00, 0x6e, 0x3f, 0x1f, 0xc6, 0x3c, 0x9e,
  0xab, 0x4a, 0x89, 0x31, 0x81, 0x88, 0xa4, 0x6c, 0x8e, 0x86, 0xa8, 0x57,
  0xb0, 0x83, 0xb4, 0x5c, 0x19, 0xc0, 0xf7, 0xb8, 0xd5, 0x1f, 0x8e, 0x2f,
  0x51, 0x03, 0x21, 0x64, 0x15, 0xa6, 0xc5, 0x84, 0x12, 0x87, 0xd4, 0x82,
  0x4e, 0xa6, 0xa6, 0x7d, 0x2a, 0xf4, 0xc0, 0xc5, 0x60, 0x22, 0xce, 0xcb,
  0x34, 0x00, 0x5c, 0xfd, 0x03, 0x21, 0xf6, 0x7d, 0x10, 0xfb, 0xe8, 0x00,
  0x71, 0xbe, 0x80, 0xf6, 0xf2, 0xbb, 0x52, 0x55, 0xf0, 0x2b, 0xf5, 0x50,
  0xd2, 0x2b, 0xa0, 0xa1, 0xe5, 0x26, 0x15, 0x04, 0xad, 0xd0, 0xd5, 0x79,
  0x8b, 0x47, 0xfa, 0x9d, 0x60, 0x54, 0x56, 0xe8, 0x8f, 0x51, 0x6e, 0x5f,
  0xd0, 0x4a, 0x5b, 0x01, 0x18, 0x8a, 0xaf, 0x08, 0x19, 0x03, 0x8c, 0x70,
  0xba, 0xdf, 0x03, 0x2b, 0x7c, 0xc1, 0x4a, 0xf2, 0x24, 0xe1, 0xd7, 0x5b,
  0x08, 0x63, 0x95, 0xc4, 0x09, 0x77, 0x21, 0x76, 0x7f, 0x3b, 0xd8, 0x0f,
  0xfb, 0xac, 0xd1, 0x41, 0xdb, 0x23, 0x83, 0xb4, 0x29, 0xf3, 0x30, 0x58,
  0xf9, 0xba, 0x8e, 0x92, 0x92, 0xb1, 0xb3, 0xd5, 0xcf, 0x64, 0x44, 0x07,
  0x7f, 0x51, 0xa9, 0x65, 0xc0, 0xbc, 0xd0, 0x04, 0x3b, 0xad, 0x9a, 0x27,
  0xb9, 0x84, 0x79, 0xa7, 0x57, 0x6b, 0xf1, 0x12, 0x9e, 0xf0, 0xc0, 0x90,
  0x63, 0xdc, 0xca, 0xa5, 0x18, 0xad, 0x0c, 0x0b, 0x70, 0x6b, 0x9e, 0x99,
  0xba, 0x10, 0xe0, 0x46, 0x06, 0x9e, 0x9e, 0xb8, 0x1d, 0xae, 0xda, 0x0a,
  0xab, 0x38, 0x40, 0xb7, 0x75, 0xa6, 0x0d, 0xf5, 0x84, 0x19, 0xb8, 0xe9,
  0xce, 0x69, 0xea, 0xbc, 0xe1, 0x2e, 0xdb, 0x52, 0x6d, 0x4f, 0x90, 0x36,
  0xf0, 0x43, 0x9b, 0x9a, 0x11, 0x1f, 0x71, 0x18, 0x0c, 0xe2, 0xc8, 0xb5,
  0x4a, 0xb5, 0x9e, 0xb4, 0x8e, 0x8d, 0x61, 0xd7, 0x92, 0xbd, 0x14, 0x7b,
  0xe5, 0xb5, 0x7f, 0xc4, 0x5e, 0xd0, 0x7b, 0x61, 0xc5, 0x0a, 0xe2, 0x62,
  0xcf, 0xbb, 0x91, 0x27, 0xa5, 0x2f, 0xf4, 0x02, 0xa5, 0x62, 0x96, 0xcb,
  0xdf, 0x50, 0x35, 0xd9, 0x86, 0x69, 0x3d, 0x5e, 0xe5, 0x04, 0xa5, 0x41,
  0x8a, 0xa4, 0x6e, 0x80, 0xde, 0x01, 0xc1, 0xb6, 0x59, 0xaa, 0x1e, 0x6c,
  0x50, 0x9b, 0x4b, 0x56, 0xff, 0x8b, 0x1d, 0x75, 0xd0, 0xc1, 0x8e, 0xa8,
  0xe9, 0x7d, 0xbb, 0x19, 0x29, 0xdf, 0x2f, 0x65, 0x51, 0xbf, 0x7b, 0x48,
  0x78, 0xd0, 0x25, 0xf2, 0x62, 0x53, 0x86, 0x5a, 0x9f, 0x76, 0x80, 0x53,
  0x59, 0xda, 0x13, 0x2b, 0xbe, 0xd4, 0x49, 0x62, 0x81, 0x3a, 0x45, 0x98,
  0x68, 0xca, 0x23, 0xe0, 0xbb, 0x17, 0x96, 0x67, 0xd6, 0xfa, 0x0e, 0xe7,
  0xef, 0x9b, 0x7b, 0xef, 0xe6, 0x7c, 0x8c, 0xfc, 0xff, 0x40, 0xbc, 0x28,
  0x74, 0xd0, 0x90, 0x44, 0x00, 0x08, 0x00, 0x00, 0x00, 0x80, 0x02, 0x00,
  0x00, 0x02, 0x1c, 0xdf, 0x44, 0x21, 0x42, 0xa4, 0xee, 0x5a, 0x1d, 0x35,
  0x1f, 0x00, 0x20, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30, 0x34, 0x30, 0x30,
  0x30, 0x30, 0x30, 0x30, 0x30, 0x44, 0x35, 0x41, 0x30, 0x30, 0x5c, 0x65,
  0x78, 0x65, 0x66, 0x73, 0x5c, 0x63, 0x6f, 0x64, 0x65, 0x2e, 0x62, 0x70,
  0x73, 0x00, 0xf0, 0x57, 0x90, 0x67, 0xff, 0xfb, 0xf7, 0x73, 0x67, 0x79,
  0xfd, 0x40, 0xc1, 0x56, 0x74, 0xf0, 0x90, 0x3b, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x48, 0xa4,
  0xee, 0x5a, 0x14, 0x30, 0x16, 0x00, 0x10, 0x00, 0x00, 0x00, 0x30, 0x30,
  0x30, 0x34, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x44, 0x35, 0x41,
  0x30, 0x30, 0x5c, 0x65, 0x78, 0x65, 0x66, 0x73, 0x00, 0xf0, 0x05, 0x94,
  0x7c, 0x7c, 0x1a, 0x74, 0xf0, 0x90, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x88, 0xa4, 0xee,
  0x5a, 0x14, 0x30, 0x10, 0x00, 0x10, 0x00, 0x00, 0x00, 0x30, 0x30, 0x30,
  0x34, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x44, 0x35, 0x41, 0x30,
  0x30, 0x00, 0xb0, 0xc1, 0x46, 0x4a, 0xa6, 0x45, 0x74, 0xd0, 0x90, 0x33,
  0x00, 0x5c, 0x01, 0x00, 0x00, 0x26, 0x02, 0x00, 0x00, 0x02, 0x7a, 0x2e,
  0x5c, 0x2c, 0x55, 0xa5, 0xee, 0x5a, 0x1d, 0x35, 0x0e, 0x00, 0x20, 0x00,
  0x00, 0x00, 0x64, 0x69, 0x73, 0x63, 0x6c, 0x61, 0x69, 0x6d, 0x65, 0x72,
  0x2e, 0x74, 0x78, 0x74, 0x00, 0xf0, 0xbb, 0x6e, 0x62, 0x0d, 0x81, 0x0c,
  0xcd, 0x0b, 0xd5, 0x81, 0x17, 0xb8, 0x91, 0xe0, 0xfb, 0xce, 0x82, 0x42,
  0x3a, 0x1e, 0x06, 0x21, 0x1c, 0x01, 0x08, 0x0e, 0x1b, 0xd6, 0x1b, 0xb7,
  0xf1, 0x11, 0x76, 0xcd, 0x24, 0x94, 0x15, 0xf7, 0xb3, 0x80, 0x1a, 0xb6,
  0xc7, 0x2e, 0x63, 0xba, 0x6a, 0x0f, 0x0b, 0xbf, 0x88, 0xe3, 0xc7, 0x8d,
  0xb4, 0xb3, 0x3c, 0x6e, 0x36, 0xf1, 0xd9, 0x8e, 0x63, 0x6e, 0xaf, 0xbe,
  0xf0, 0xf8, 0xd8, 0xd8, 0x66, 0xbd, 0x83, 0xdb, 0x76, 0x6d, 0x74, 0x20,
  0xd0, 0xbc, 0x4a, 0xbd, 0x54, 0xdb, 0xb1, 0x3b, 0x02, 0xb7, 0x97, 0xf4,
  0xa9, 0x97, 0xd3, 0xed, 0x7b, 0x5e, 0xaa, 0x75, 0x5d, 0x91, 0xd0, 0xc9,
  0x5e, 0x42, 0x0a, 0xbd, 0xec, 0x7b, 0xbd, 0x02, 0xde, 0x69, 0xe9, 0xe5,
  0xe4, 0xf2, 0xc4, 0x35, 0x15, 0x60, 0x42, 0xf7, 0x4c, 0x0c, 0x2c, 0xcb,
  0x7f, 0x17, 0x56, 0x25, 0x16, 0x28, 0x59, 0xef, 0x47, 0xf5, 0x7d, 0xf1,
  0x98, 0x91, 0xc2, 0xb3, 0xee, 0xb6, 0x43, 0x0d, 0x1d, 0xc1, 0x6f, 0x64,
  0x04, 0x20, 0xf5, 0xcc, 0x34, 0x0a, 0x03, 0xc8, 0x6c, 0x6d, 0x4e, 0x4a,
  0x83, 0x56, 0x82, 0xa8, 0x63, 0xfd, 0x52, 0x9d, 0xef, 0xdf, 0xd5, 0x77,
  0xc9, 0x63, 0x22, 0x29, 0x91, 0x2c, 0xb8, 0x45, 0xbe, 0xd2, 0xd2, 0x4a,
  0x44, 0x2c, 0xff, 0x76, 0x11, 0x56, 0xc8, 0xd7, 0xf4, 0xa4, 0x1b, 0x9b,
  0x9f, 0x3c, 0xe0, 0x6d, 0x4b, 0x63, 0x99, 0x89, 0xb6, 0xa1, 0x12, 0x02,
  0xac, 0x88, 0xd8, 0x2e, 0x15, 0xdf, 0x6b, 0x1f, 0xfb, 0x6c, 0xa3, 0xbe,
  0xe1, 0xba, 0x29, 0x45, 0x56, 0x58, 0x5e, 0x9d, 0xbf, 0xf1, 0x89, 0xa6,
  0x80, 0xe6, 0x43, 0x9c, 0x9a, 0x71, 0x41, 0x70, 0xc2, 0x50, 0xf7, 0x3d,
  0x4f, 0x34, 0x78, 0xcd, 0x8a, 0x4f, 0xc7, 0x84, 0xb9, 0x06, 0x26, 0xf4,
  0x68, 0x1d, 0x5a, 0xcf, 0x06, 0x53, 0x32, 0xd2, 0x60, 0x39, 0xf2, 0x46,
  0x1c, 0x1b, 0x12, 0x2b, 0xf0, 0x83, 0x03, 0x4f, 0x6a, 0x59, 0x3b, 0x83,
  0x9a, 0x14, 0xd5, 0x7c, 0x35, 0x21, 0xc0, 0x5e, 0xdb, 0xe2, 0xd7, 0x71,
  0xbe, 0x45, 0x30, 0x5a, 0xe2, 0xac, 0x0e, 0x66, 0xfc, 0xda, 0x13, 0xf7,
  0x79, 0xd6, 0x06, 0xbb, 0xad, 0xb3, 0x8e, 0xe5, 0x91, 0x45, 0xf7, 0x07,
  0x64, 0xdc, 0x54, 0x05, 0x66, 0x78, 0x84, 0xc2, 0xe6, 0x0e, 0xf3, 0x49,
  0xeb, 0x24, 0xc7, 0x0a, 0xf5, 0x18, 0xef, 0xff, 0xbd, 0x58, 0x21, 0x8c,
  0xbc, 0x4f, 0x86, 0xbb, 0xc0, 0x6c, 0x3c, 0xff, 0x28, 0xc4, 0x3d, 0x7b,
  0x00, 0x40, 0x07, 0x00
};

std::string TempDir(const char* name) {
    const std::string dir = std::string("/tmp/azf_catalog3_test_") + name + "/";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

std::string Read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

V Tree(const std::string& dir) {
    V out;
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        return out;
    }
    for (const auto& e : fs::recursive_directory_iterator(dir)) {
        if (e.is_regular_file()) {
            out.push_back(e.path().lexically_relative(dir).generic_string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

void Le16(std::string& s, unsigned v) {
    s += static_cast<char>(v & 255);
    s += static_cast<char>((v >> 8) & 255);
}
void Le32(std::string& s, unsigned v) {
    Le16(s, v & 0xffff);
    Le16(s, v >> 16);
}

using Members = std::vector<std::pair<std::string, std::string>>;

/// A zip of stored members.
std::string StoredZip(const Members& members) {
    std::string out, central;
    for (const auto& [name, data] : members) {
        const unsigned offset = static_cast<unsigned>(out.size());
        const unsigned crc = Mods::Crc32(data);
        const unsigned size = static_cast<unsigned>(data.size());
        out += "PK\x03\x04";
        Le16(out, 20), Le16(out, 0), Le16(out, 0), Le16(out, 0), Le16(out, 0x21);
        Le32(out, crc), Le32(out, size), Le32(out, size);
        Le16(out, static_cast<unsigned>(name.size())), Le16(out, 0);
        out += name + data;
        central += "PK\x01\x02";
        Le16(central, 20), Le16(central, 20), Le16(central, 0), Le16(central, 0), Le16(central, 0),
            Le16(central, 0x21);
        Le32(central, crc), Le32(central, size), Le32(central, size);
        Le16(central, static_cast<unsigned>(name.size())), Le16(central, 0), Le16(central, 0);
        Le16(central, 0), Le16(central, 0), Le32(central, 0), Le32(central, offset);
        central += name;
    }
    const unsigned cd_offset = static_cast<unsigned>(out.size());
    out += central;
    out += "PK\x05\x06";
    Le16(out, 0), Le16(out, 0), Le16(out, static_cast<unsigned>(members.size())),
        Le16(out, static_cast<unsigned>(members.size()));
    Le32(out, static_cast<unsigned>(central.size())), Le32(out, cd_offset), Le16(out, 0);
    return out;
}

/// A split zip as Info-ZIP writes it: "PK\x07\x08" first, then the stored members and the central
/// directory cut into disks of `disk_size` bytes (offsets relative to their disk), the end record in
/// the last one. Returns the parts in order (.z01 ... .zip).
V SplitZip(const Members& members, std::size_t disk_size) {
    std::string stream = "PK\x07\x08";
    std::vector<unsigned> offsets;
    for (const auto& [name, data] : members) {
        offsets.push_back(static_cast<unsigned>(stream.size()));
        const unsigned crc = Mods::Crc32(data);
        const unsigned size = static_cast<unsigned>(data.size());
        stream += "PK\x03\x04";
        Le16(stream, 20), Le16(stream, 0), Le16(stream, 0), Le16(stream, 0), Le16(stream, 0x21);
        Le32(stream, crc), Le32(stream, size), Le32(stream, size);
        Le16(stream, static_cast<unsigned>(name.size())), Le16(stream, 0);
        stream += name + data;
    }
    const unsigned cd_start = static_cast<unsigned>(stream.size());
    for (std::size_t i = 0; i < members.size(); ++i) {
        const auto& [name, data] = members[i];
        const unsigned crc = Mods::Crc32(data);
        const unsigned size = static_cast<unsigned>(data.size());
        stream += "PK\x01\x02";
        Le16(stream, 20), Le16(stream, 20), Le16(stream, 0), Le16(stream, 0), Le16(stream, 0),
            Le16(stream, 0x21);
        Le32(stream, crc), Le32(stream, size), Le32(stream, size);
        Le16(stream, static_cast<unsigned>(name.size())), Le16(stream, 0), Le16(stream, 0);
        Le16(stream, static_cast<unsigned>(offsets[i] / disk_size)); // the disk it starts on
        Le16(stream, 0), Le32(stream, 0), Le32(stream, static_cast<unsigned>(offsets[i] % disk_size));
        stream += name;
    }
    const unsigned cd_size = static_cast<unsigned>(stream.size()) - cd_start;
    const std::size_t total_with_end = stream.size() + 22;
    const unsigned last_disk = static_cast<unsigned>((total_with_end - 1) / disk_size);
    stream += "PK\x05\x06";
    Le16(stream, last_disk), Le16(stream, cd_start / static_cast<unsigned>(disk_size)),
        Le16(stream, static_cast<unsigned>(members.size())), Le16(stream, static_cast<unsigned>(members.size()));
    Le32(stream, cd_size), Le32(stream, static_cast<unsigned>(cd_start % disk_size)), Le16(stream, 0);
    V parts;
    for (std::size_t at = 0; at < stream.size(); at += disk_size) {
        parts.push_back(stream.substr(at, disk_size));
    }
    return parts;
}

// ---- RAR 4 volumes (stored, no compression), the format libarchive reads as a multi-volume set

std::string RarHeader(unsigned type, unsigned flags, const std::string& body) {
    std::string raw;
    raw += static_cast<char>(type);
    Le16(raw, flags);
    Le16(raw, static_cast<unsigned>(7 + body.size()));
    raw += body;
    std::string out;
    Le16(out, Mods::Crc32(raw) & 0xffff);
    return out + raw;
}

std::string RarFile(const std::string& name, const std::string& whole, std::size_t from, std::size_t to,
                    bool before, bool after) {
    const std::string chunk = whole.substr(from, to - from);
    std::string body;
    Le32(body, static_cast<unsigned>(chunk.size()));
    Le32(body, static_cast<unsigned>(whole.size()));
    body += static_cast<char>(3); // Unix
    Le32(body, Mods::Crc32(whole));
    Le32(body, 0x5a000000);
    body += static_cast<char>(29);
    body += static_cast<char>(0x30); // stored
    Le16(body, static_cast<unsigned>(name.size()));
    Le32(body, 0100644u); // RAR's Unix attribute is the st_mode itself
    body += name;
    return RarHeader(0x74, 0x8000 | (before ? 1u : 0u) | (after ? 2u : 0u), body) + chunk;
}

/// Three volumes: [a][b first third] [b second third] [b last third][c]. B is cut across volumes.
V RarVolumes(const std::string& a_name, const std::string& a, const std::string& b_name, const std::string& b,
             const std::string& c_name, const std::string& c) {
    const std::size_t t1 = b.size() / 3, t2 = 2 * b.size() / 3;
    const auto volume = [&](bool first, bool last, const std::string& files) {
        std::string v = "Rar!\x1a\x07";
        v += '\0';
        v += RarHeader(0x73, 0x0001 | 0x0010 | (first ? 0x0100u : 0u), std::string(6, '\0'));
        v += files;
        v += RarHeader(0x7b, last ? 0u : 1u, "");
        return v;
    };
    return {volume(true, false, RarFile(a_name, a, 0, a.size(), false, false) + RarFile(b_name, b, 0, t1, false, true)),
            volume(false, false, RarFile(b_name, b, t1, t2, true, true)),
            volume(false, true, RarFile(b_name, b, t2, b.size(), true, false) + RarFile(c_name, c, 0, c.size(), false, false))};
}

std::string Pattern(std::size_t n, unsigned seed) {
    std::string s(n, '\0');
    for (std::size_t i = 0; i < n; ++i) {
        s[i] = static_cast<char>((i * 7 + seed) & 255);
    }
    return s;
}

// ---- a fake network for the parts: host+path -> bytes written to the file

struct FakeFiles {
    std::map<std::string, std::string> files;
    std::map<std::string, int> fail_first; // key -> times to fail before serving
    std::map<std::string, int> hits;
    std::map<std::string, std::string> html; // key -> a 200 text/html page (Drive's confirm form, a wall)
    std::vector<std::string> log;
    Multipart::FileTransport Get() {
        return [this](const std::string& host, const std::string& path, const std::string& dest,
                      const std::atomic_bool*, const Multipart::Progress& progress) {
            Multipart::FileReply r;
            const std::string key = host + path;
            log.push_back(key);
            if (fail_first[key]-- > 0) {
                r.error = "Connection reset";
                return r;
            }
            if (const auto page = html.find(key); page != html.end()) {
                r.status = 200;
                r.html = page->second;
                return r;
            }
            const auto it = files.find(key);
            if (it == files.end()) {
                r.status = 404;
                return r;
            }
            std::ofstream(dest, std::ios::binary | std::ios::trunc) << it->second;
            ++hits[key];
            if (progress) {
                progress(it->second.size(), it->second.size());
            }
            r.status = 200;
            r.total = it->second.size();
            return r;
        };
    }
};

std::string PartUrl(const std::string& file) {
    return "https://raw.githubusercontent.com/o/r/main/" + file;
}

// An entry with a mods part and a textures part, in parts (repository files).
std::string PartsCatalog(const V& names, const char* extra = "") {
    std::string parts;
    for (const auto& n : names) {
        parts += std::string(parts.empty() ? "" : ",") + R"({"type":"github_repo_path","repo":"o/r","ref":"main","path":")" +
                 n + "\"}";
    }
    return std::string(R"({"schema":1,"mods":[{"id":"big-pack","name":"Big Pack","title_ids":["0004000000033500"],
      "author":"A","licence":"MIT","parts":[)") + parts + R"(],
      "install":{"rules":[{"from":"load/mods/{tid}/","to":""}]},
      "textures":{"rules":[{"from":"load/textures/{tid}/","to":""}]})" + extra + "}]}";
}

std::unique_ptr<Library> MakeParts(const std::string& dir, const std::string& json, FakeFiles& net) {
    auto parsed = ParseCatalog(json);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->mods.size() == 1);
    auto library = std::make_unique<Library>(dir, *parsed, [](const std::string&, const std::string&, std::size_t) {
        HttpReply r;
        r.status = 404;
        return r;
    });
    library->SetFileTransport(net.Get());
    library->SetFreeSpace([](const std::string&, u64* out) {
        *out = 1ull << 40;
        return true;
    });
    return library;
}

const std::string kRomfs = Pattern(3000, 1);
const std::string kPng = "PNG-data-1";

Members PackMembers() {
    return {{"load/mods/0004000000033500/romfs/a.bin", kRomfs},
            {"load/mods/0004000000033500/exefs/code.ips", "PATCH-3"},
            {"load/textures/0004000000033500/UI/tex1_1x1_AAAA_1_mip0.png", kPng},
            {"load/textures/0004000000033500/tex1_2x2_BBBB_1_mip0.png", "PNG-data-2"},
            {"readme.txt", "ignored"}};
}

} // namespace

// ---------------------------------------------------------------------------------------------

TEST_CASE("Catalog3: the new catalog fields are parsed", "[mods][catalog3]") {
    std::vector<std::string> warnings;
    const auto c = ParseCatalog(R"({"schema":1,"mods":[
 {"id":"a","name":"A","title_ids":["0004000000033500"],"author":"x","downloads":1234,"updated":"2026-09",
  "source":{"type":"gamebanana","mod":5,"file_pattern":"*.zip"},
  "textures":{"rules":[{"from":"*/tex/{tid}/","to":""},{"from":"x.png","to":"UI/x.png"}]}},
 {"id":"b","name":"B","title_ids":["0004000000033500"],"updated":"2026-09-14","downloads":-5,
  "source":{"type":"gamebanana","mod":6}},
 {"id":"c","name":"C","title_ids":["0004000000033500"],"updated":"2026-13",
  "source":{"type":"gamebanana","mod":7}},
 {"id":"d","name":"D","title_ids":["0004000000033500"],
  "parts":[{"type":"gamebanana","mod":8,"file_pattern":"*.part1.rar"},
           {"type":"gamebanana","mod":8,"file_pattern":"*.part2.rar"}]},
 {"id":"e","name":"E","title_ids":["0004000000033500"],"source":{"type":"gamebanana","mod":9},"textures":{"rules":[]}},
 {"id":"f","name":"F","title_ids":["0004000000033500"],"source":{"type":"gamebanana","mod":9},
  "textures":{"rules":[{"from":"../x","to":""}]}},
 {"id":"g","name":"G","title_ids":["0004000000033500"],"parts":[{"type":"bogus"}]},
 {"id":"h","name":"H","title_ids":["0004000000033500"],"parts":[]}
]})",
                           nullptr, &warnings);
    REQUIRE(c.has_value());
    const auto* a = c->Find("a");
    REQUIRE(a != nullptr);
    CHECK(a->downloads == 1234);
    CHECK(a->updated == "2026-09");
    REQUIRE(a->texture_rules.size() == 2);
    CHECK(a->texture_rules[0].from == "*/tex/{tid}/");
    CHECK(a->texture_rules[1].to == "UI/x.png");
    CHECK(a->parts.empty());
    CHECK(c->Find("b")->updated == "2026-09-14");
    CHECK(c->Find("b")->downloads == 0);   // not an unsigned number
    CHECK(c->Find("c")->updated.empty());  // month 13
    const auto* d = c->Find("d");
    REQUIRE(d != nullptr);
    REQUIRE(d->parts.size() == 2);
    CHECK(d->parts[1].asset_pattern == "*.part2.rar");
    CHECK(d->source.gb_mod == 8); // the first part stands for the source
    CHECK(c->Find("e") == nullptr);   // a textures block with no rules
    CHECK(c->Find("f") == nullptr);   // an unsafe rule
    CHECK(c->Find("g") == nullptr);   // a part that is no source
    CHECK(c->Find("h") == nullptr);   // no source at all
    CHECK(warnings.size() == 4);

    // the compiled-in catalog: Single Screen Experience carries its HD UI textures
    const auto def = DefaultCatalog();
    const auto* ss = def.Find("ocarina-single-screen-experience");
    REQUIRE(ss != nullptr);
    REQUIRE(ss->texture_rules.size() == 1);
    CHECK(MapTexturePath("SingleScreen_3.2.2/USA/Emulator/load/textures/0004000000033500/UI/"
                         "tex1_128x128_8064FDFB5065B51D_4_mip0.png",
                         ss->texture_rules, 0x0004000000033500ULL) ==
          "UI/tex1_128x128_8064FDFB5065B51D_4_mip0.png");
    CHECK(MapTexturePath("SingleScreen_3.2.2/EUR/Emulator/load/textures/0004000000033600/UI/t.png",
                         ss->texture_rules, 0x0004000000033500ULL).empty()); // another region's
}

TEST_CASE("Catalog3: texture catalog fields", "[mods][catalog3]") {
    namespace Dl = CustomTex::Download;
    using Dl::UpdatedText;
    const auto c = Dl::ParseCatalog(R"({"schema":1,"packs":[
 {"id":"p1","name":"P1","title_ids":["0004000000033500"],"downloads":77,"updated":"2025-03","size":5000000000,
  "source":{"type":"gamebanana","mod":1,"file_pattern":"*.part1.rar"},
  "parts":[{"type":"gamebanana","mod":1,"file_pattern":"*.part1.rar"},{"type":"gamebanana","mod":1,"file_pattern":"*.part2.rar"}]},
 {"id":"p2","name":"P2","title_ids":["0004000000033500"],"updated":"2025-14",
  "parts":[{"type":"gamebanana","mod":2}]},
 {"id":"p3","name":"P3","title_ids":["0004000000033500"],"parts":[{"type":"nope"}]}
]})");
    REQUIRE(c.has_value());
    const auto* p1 = c->Find("p1");
    REQUIRE(p1 != nullptr);
    CHECK(p1->downloads == 77);
    CHECK(p1->updated == "2025-03");
    CHECK(p1->parts.size() == 2);
    CHECK(UpdatedText("2025-03") == "Updated Mar 2025");
    const auto* p2 = c->Find("p2");
    REQUIRE(p2 != nullptr);
    CHECK(p2->updated.empty());
    CHECK(p2->source.gb_mod == 2); // parts only
    CHECK(c->Find("p3") == nullptr);
}

TEST_CASE("Catalog3: the texture rules map archive paths", "[mods][catalog3]") {
    const std::vector<Rule> rules{{"*/Emulator/load/textures/{tid}/", ""}, {"extra/x.png", "UI/x.png"},
                                  {"hd/", "HD/"}};
    const u64 tid = kTid;
    CHECK(MapTexturePath("Pack/Emulator/load/textures/0004000000033500/UI/tex1_a.png", rules, tid) == "UI/tex1_a.png");
    CHECK(MapTexturePath("Pack/Emulator/load/textures/0004000000033500/tex1_b.png", rules, tid) == "tex1_b.png");
    CHECK(MapTexturePath("Pack/Emulator/load/textures/0004000000033600/tex1_b.png", rules, tid).empty());
    CHECK(MapTexturePath("extra/x.png", rules, tid) == "UI/x.png");
    CHECK(MapTexturePath("hd/a/b.png", rules, tid) == "HD/a/b.png");
    CHECK(MapTexturePath("other/file.png", rules, tid).empty());
    CHECK(MapTexturePath("hd/../../x.png", rules, tid).empty());
    CHECK(MapTexturePath("hd/a.png", {}, tid).empty()); // no rules, no textures
    // the mod rules are untouched by it
    CHECK(MapPath("a/load/mods/0004000000033500/romfs/x.bin", {{"*/load/mods/{tid}/", ""}}, tid) == "romfs/x.bin");
}

TEST_CASE("Catalog3: a mod with textures installs both parts and removes them", "[mods][catalog3]") {
    const std::string dir = TempDir("tex");
    FakeFiles net;
    // one archive (a zip) as the single part
    net.files[PartUrl("pack.zip")] = StoredZip(PackMembers());
    auto library_owner = MakeParts(dir, PartsCatalog({"pack.zip"}), net);
    Library& library = *library_owner;
    const std::string mods = dir + "mods/" + kTidText + "/";
    const std::string tex = dir + "textures/" + kTidText + "/big-pack/";
    // (a zip needs bsdtar through the parts path)
    if (FindBsdtar().empty()) {
        SKIP("no bsdtar");
    }
    const auto out = library.Install(kTid, "big-pack");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    CHECK(out.textures_installed);
    CHECK(Tree(mods) == V{"exefs/code.ips", "romfs/a.bin"});
    CHECK(Tree(dir + "textures/") ==
          V{std::string(kTidText) + "/big-pack/UI/tex1_1x1_AAAA_1_mip0.png",
            std::string(kTidText) + "/big-pack/tex1_2x2_BBBB_1_mip0.png"});
    CHECK(Read(tex + "UI/tex1_1x1_AAAA_1_mip0.png") == kPng);
    CHECK(Read(mods + "romfs/a.bin") == kRomfs);
    const auto loaded = library.ListLoaded(kTid);
    REQUIRE(loaded.size() == 1);
    CHECK(loaded[0].texture_files.size() == 2);
    CHECK_FALSE(fs::exists(dir + "mod-downloads/big-pack")); // the work folder is gone

    // removing the mod removes both parts
    REQUIRE(library.Remove(kTid, "big-pack").ok);
    CHECK(Tree(mods).empty());
    CHECK(Tree(dir + "textures/").empty());
    CHECK(Tree(dir + "mod-library/").empty());

    // a deactivated mod comes back from the library, both parts
    REQUIRE(library.Install(kTid, "big-pack").result == InstallResult::Ok);
    REQUIRE(library.Deactivate(kTid, "big-pack").ok);
    CHECK(Tree(mods).empty());
    CHECK(Tree(dir + "textures/").empty());
    net.files.clear(); // from the library, no network
    const auto again = library.Install(kTid, "big-pack");
    REQUIRE(again.result == InstallResult::Ok);
    CHECK(again.textures_installed);
    CHECK(Tree(dir + "textures/").size() == 2);
}

TEST_CASE("Catalog3: textures through the ordinary one-file download", "[mods][catalog3]") {
    const std::string dir = TempDir("tex1");
    auto parsed = ParseCatalog(R"({"schema":1,"mods":[{"id":"one","name":"One","title_ids":["0004000000033500"],
      "author":"A","licence":"MIT",
      "source":{"type":"github_release","repo":"o/r","tag":"latest","asset_pattern":"p.zip"},
      "install":{"rules":[{"from":"load/mods/{tid}/","to":""}]},
      "textures":{"rules":[{"from":"load/textures/{tid}/","to":""}]}}]})");
    REQUIRE(parsed.has_value());
    const std::string zip = StoredZip(PackMembers());
    Library library(dir, *parsed, [&](const std::string& host, const std::string& path, std::size_t) {
        HttpReply r;
        r.status = 200;
        if (host + path == "https://api.github.com/repos/o/r/releases/latest") {
            r.body = R"({"tag_name":"v1","assets":[{"name":"p.zip","browser_download_url":"https://github.com/o/r/releases/download/v1/p.zip"}]})";
        } else if (host + path == "https://github.com/o/r/releases/download/v1/p.zip") {
            r.body = zip;
        } else {
            r.status = 404;
        }
        return r;
    });
    const auto out = library.Install(kTid, "one");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    CHECK(out.textures_installed);
    CHECK(Tree(dir + "mods/" + kTidText + "/") == V{"exefs/code.ips", "romfs/a.bin"});
    CHECK(Tree(dir + "textures/" + kTidText + "/one/") ==
          V{"UI/tex1_1x1_AAAA_1_mip0.png", "tex1_2x2_BBBB_1_mip0.png"});
    const auto a = library.ListAvailable(kTid);
    REQUIRE(a.size() == 1);
    CHECK(a[0].has_textures);
    CHECK(a[0].active);
}

TEST_CASE("Catalog3: two texture parts that write one file clash", "[mods][catalog3]") {
    const std::string dir = TempDir("texclash");
    const auto entry = [](const char* id, const char* name, const char* file) {
        return std::string(R"({"id":")") + id + R"(","name":")" + name +
               R"(","title_ids":["0004000000033500"],"author":"A",
  "source":{"type":"github_repo_path","repo":"o/r","ref":"main","path":")" + id + "/" + file +
               R"("},"textures":{"rules":[{"from":")" + file + R"(","to":"UI/)" + file + R"("}]}})";
    };
    auto parsed = ParseCatalog(R"({"schema":1,"mods":[)" + entry("one", "One", "tex1_1x1_AAAA_1_mip0.png") + "," +
                               entry("two", "Two", "tex1_1x1_AAAA_1_mip0.png") + "," +
                               entry("three", "Three", "tex1_9x9_CCCC_1_mip0.png") + "]}");
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->mods.size() == 3);
    Library library(dir, *parsed, [&](const std::string&, const std::string& path, std::size_t) {
        HttpReply r;
        r.status = 200;
        for (const char* id : {"one", "two", "three"}) {
            if (path.find(std::string("/main/") + id + "/") != std::string::npos) {
                r.body = id;
                return r;
            }
        }
        r.status = 404;
        return r;
    });
    const std::string tex = dir + "textures/" + kTidText + "/";
    REQUIRE(library.Install(kTid, "one").result == InstallResult::Ok);
    CHECK(Read(tex + "one/UI/tex1_1x1_AAAA_1_mip0.png") == "one");
    const auto clash = library.Install(kTid, "two");
    REQUIRE(clash.result == InstallResult::Conflict);
    CHECK(clash.conflict_id == "one");
    CHECK(clash.conflict_name == "One");
    CHECK(clash.conflict_path == "textures/UI/tex1_1x1_AAAA_1_mip0.png");
    CHECK_FALSE(clash.conflict_unmanaged);
    CHECK(Read(tex + "one/UI/tex1_1x1_AAAA_1_mip0.png") == "one"); // nothing changed
    CHECK(Tree(tex + "two/").empty());
    REQUIRE(library.Install(kTid, "three").result == InstallResult::Ok); // another file: no clash
    REQUIRE(library.Install(kTid, "two", true).result == InstallResult::Ok); // Replace
    CHECK(Read(tex + "two/UI/tex1_1x1_AAAA_1_mip0.png") == "two");
    CHECK(Tree(tex + "one/").empty()); // One is deactivated, its textures are out
    CHECK(library.ListLoaded(kTid).size() == 3);
}

TEST_CASE("Catalog3: the free space check", "[mods][catalog3]") {
    constexpr u64 kGiB = 1024ull * 1024 * 1024;
    using namespace Multipart;
    // (parts + unpacked) x 1.1
    CHECK(RequiredBytes(1000, 1000) == 2200);
    CHECK(SpaceError(2'245'000'000ull, 2'245'000'000ull, 100 * kGiB).empty());
    CHECK(SpaceError(2'245'000'000ull, 2'245'000'000ull, 4 * kGiB) == "Not Enough Space: Needs 4.6 GB Free");
    CHECK(SpaceError(2'245'000'000ull, 2'245'000'000ull, RequiredBytes(2'245'000'000ull, 2'245'000'000ull)).empty());
    CHECK(SpaceError(100ull << 20, 100ull << 20, 0) == "Not Enough Space: Needs 220 MB Free");
    CHECK(SizeText(5 * kGiB / 2) == "2.5 GB");

    // through the library: the error is the install's error, before anything is downloaded
    const std::string dir = TempDir("space");
    FakeFiles net;
    net.files[PartUrl("pack.zip")] = StoredZip(PackMembers());
    auto library_owner = MakeParts(dir, PartsCatalog({"pack.zip"}), net);
    Library& library = *library_owner;
    library.SetFreeSpace([](const std::string&, u64* out) {
        *out = 1000;
        return true;
    });
    // the sizes come from the host for a GameBanana part; a repository file has none, so use GameBanana
    auto parsed = ParseCatalog(R"({"schema":1,"mods":[{"id":"gb","name":"GB","title_ids":["0004000000033500"],
      "author":"A","parts":[{"type":"gamebanana","mod":5,"file_pattern":"*.001"},
                             {"type":"gamebanana","mod":5,"file_pattern":"*.002"}]}]})");
    REQUIRE(parsed.has_value());
    const std::string profile = R"({"_sVersion":"1","_aFiles":[
      {"_sFile":"x.7z.001","_nFilesize":2245000000,"_sAvResult":"clean","_tsDateAdded":5,"_sDownloadUrl":"https://gamebanana.com/dl/1"},
      {"_sFile":"x.7z.002","_nFilesize":100,"_sAvResult":"clean","_tsDateAdded":5,"_sDownloadUrl":"https://gamebanana.com/dl/2"}]})";
    Library gb(dir, *parsed, [&](const std::string& host, const std::string& path, std::size_t) {
        HttpReply r;
        r.status = (host + path) == "https://gamebanana.com/apiv11/Mod/5/ProfilePage" ? 200 : 404;
        r.body = profile;
        return r;
    });
    gb.SetFreeSpace([](const std::string&, u64* out) {
        *out = 4ull * kGiB;
        return true;
    });
    FakeFiles none;
    gb.SetFileTransport(none.Get());
    const auto out = gb.Install(kTid, "gb");
    REQUIRE(out.result == InstallResult::Error);
    CHECK(out.error == "Not Enough Space: Needs 4.6 GB Free");
    CHECK(ShortFailure(out.error) == "Not Enough Space: Needs 4.6 GB Free");
    CHECK(none.log.empty()); // nothing was downloaded
    CHECK(gb.GetState().text == "Install failed: Not Enough Space: Needs 4.6 GB Free");
}

TEST_CASE("Catalog3: split 7z parts are joined and unpacked", "[mods][catalog3]") {
    if (FindBsdtar().empty()) {
        SKIP("no bsdtar");
    }
    const std::string whole(reinterpret_cast<const char*>(kSevenZip), sizeof(kSevenZip));
    const std::size_t cut1 = 300, cut2 = 700; // .7z.001 .002 .003
    const std::string dir = TempDir("split7z");
    FakeFiles net;
    net.files[PartUrl("p.7z.001")] = whole.substr(0, cut1);
    net.files[PartUrl("p.7z.002")] = whole.substr(cut1, cut2 - cut1);
    net.files[PartUrl("p.7z.003")] = whole.substr(cut2);
    // this archive has dir/a.txt and b.bin: map them
    auto json = std::string(R"({"schema":1,"mods":[{"id":"s7","name":"S7","title_ids":["0004000000033500"],"author":"A",
      "parts":[{"type":"github_repo_path","repo":"o/r","ref":"main","path":"p.7z.001"},
               {"type":"github_repo_path","repo":"o/r","ref":"main","path":"p.7z.002"},
               {"type":"github_repo_path","repo":"o/r","ref":"main","path":"p.7z.003"}],
      "install":{"rules":[{"from":"dir/a.txt","to":"romfs/a.txt"},{"from":"b.bin","to":"romfs/b.bin"}]}}]})");
    auto library_owner = MakeParts(dir, json, net);
    Library& library = *library_owner;
    const auto out = library.Install(kTid, "s7");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    const std::string mods = dir + "mods/" + kTidText + "/";
    CHECK(Tree(mods) == V{"romfs/a.txt", "romfs/b.bin"});
    std::string expect;
    for (int i = 0; i < 100; ++i) {
        expect += "seven-a\n";
    }
    CHECK(Read(mods + "romfs/a.txt") == expect);
    CHECK(Read(mods + "romfs/b.bin") == "hello seven");
    // the parts were fetched in order
    REQUIRE(net.log.size() == 3);
    CHECK(net.log[0].find("p.7z.001") != std::string::npos);
    CHECK(net.log[2].find("p.7z.003") != std::string::npos);
    CHECK(Tree(dir + "mod-downloads/").empty());
}

TEST_CASE("Catalog3: multi-volume RAR parts are joined and unpacked, textures too", "[mods][catalog3]") {
    if (FindBsdtar().empty()) {
        SKIP("no bsdtar");
    }
    const std::string b = Pattern(9000, 5); // cut across three volumes
    const auto vols = RarVolumes("load/mods/0004000000033500/exefs/code.ips", "PATCH-RAR",
                                 "load/textures/0004000000033500/UI/tex1_8x8_DDDD_1_mip0.png", b,
                                 "load/mods/0004000000033500/romfs/c.bin", "ccc");
    const std::string dir = TempDir("rar");
    FakeFiles net;
    net.files[PartUrl("p.part1.rar")] = vols[0];
    net.files[PartUrl("p.part2.rar")] = vols[1];
    net.files[PartUrl("p.part3.rar")] = vols[2];
    auto library_owner = MakeParts(dir, PartsCatalog({"p.part1.rar", "p.part2.rar", "p.part3.rar"}), net);
    Library& library = *library_owner;
    const auto out = library.Install(kTid, "big-pack");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    CHECK(out.textures_installed);
    const std::string mods = dir + "mods/" + kTidText + "/";
    CHECK(Tree(mods) == V{"exefs/code.ips", "romfs/c.bin"});
    CHECK(Read(mods + "exefs/code.ips") == "PATCH-RAR");
    CHECK(Read(dir + "textures/" + kTidText + "/big-pack/UI/tex1_8x8_DDDD_1_mip0.png") == b);
    CHECK(Multipart::SplitKindOfName("a.part1.rar") == "rar");
    CHECK(Multipart::SplitKindOfName("a.r00") == "rar");
    CHECK(Multipart::SplitKindOfName("a.7z.001") == "7z");
    CHECK(Multipart::SplitKindOfName("a.z01") == "zip");
    CHECK(Multipart::SplitKindOfName("a.txt").empty());
}

TEST_CASE("Catalog3: split zip parts are joined and unpacked", "[mods][catalog3]") {
    if (FindBsdtar().empty()) {
        SKIP("no bsdtar");
    }
    Members members = PackMembers();
    members.push_back({"load/mods/0004000000033500/romfs/big.bin", Pattern(2500, 9)});
    const V parts = SplitZip(members, 1000);
    REQUIRE(parts.size() >= 4);

    const std::string dir = TempDir("splitzip");
    FakeFiles net;
    V names;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const std::string name = i + 1 < parts.size() ? "p.z" + std::string(i + 1 < 10 ? "0" : "") + std::to_string(i + 1) : "p.zip";
        net.files[PartUrl(name)] = parts[i];
        names.push_back(name);
    }
    auto library_owner = MakeParts(dir, PartsCatalog(names), net);
    Library& library = *library_owner;
    const auto out = library.Install(kTid, "big-pack");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    const std::string mods = dir + "mods/" + kTidText + "/";
    CHECK(Tree(mods) == V{"exefs/code.ips", "romfs/a.bin", "romfs/big.bin"});
    CHECK(Read(mods + "romfs/big.bin") == Pattern(2500, 9));
    CHECK(Read(mods + "romfs/a.bin") == kRomfs);
    CHECK(Tree(dir + "textures/" + kTidText + "/big-pack/").size() == 2);
}

TEST_CASE("Catalog3: Info-ZIP split zip", "[mods][catalog3]") {
    // a real zip -s set, when the zip tool exists
    if (FindBsdtar().empty() || !fs::exists("/usr/bin/zip")) {
        SKIP("no bsdtar or zip");
    }
    const std::string dir = TempDir("realzip");
    fs::create_directories(dir + "src/load/mods/0004000000033500/romfs");
    std::string big;
    for (int i = 0; i < 40000; ++i) {
        big += static_cast<char>((i * 131 + i / 7) & 255);
    }
    std::ofstream(dir + "src/load/mods/0004000000033500/romfs/big.bin", std::ios::binary) << big;
    std::ofstream(dir + "src/load/mods/0004000000033500/romfs/big2.bin", std::ios::binary) << big + "x";
    REQUIRE(std::system(("cd " + dir + "src && zip -q -0 -r -s 64k ../x.zip load").c_str()) == 0);
    V names;
    FakeFiles net;
    for (const auto& e : fs::directory_iterator(dir)) {
        const std::string n = e.path().filename().string();
        if (n.rfind("x.z", 0) == 0) {
            names.push_back(n);
            net.files[PartUrl(n)] = Read(e.path().string());
        }
    }
    std::sort(names.begin(), names.end(), [](const auto& a, const auto& b) {
        const auto key = [](const std::string& s) { return s == "x.zip" ? std::string("x.zz") : s; };
        return key(a) < key(b);
    });
    REQUIRE(names.size() >= 2);
    auto library_owner = MakeParts(dir + "lib/", PartsCatalog(names), net);
    Library& library = *library_owner;
    const auto out = library.Install(kTid, "big-pack");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    CHECK(Read(dir + "lib/mods/" + kTidText + "/romfs/big.bin") == big);
    CHECK(Read(dir + "lib/mods/" + kTidText + "/romfs/big2.bin") == big + "x");
}

TEST_CASE("Catalog3: a part that fails is tried once more, then the install fails", "[mods][catalog3]") {
    if (FindBsdtar().empty()) {
        SKIP("no bsdtar");
    }
    const std::string whole(reinterpret_cast<const char*>(kSevenZip), sizeof(kSevenZip));
    const std::string json = R"({"schema":1,"mods":[{"id":"s7","name":"S7","title_ids":["0004000000033500"],"author":"A",
      "parts":[{"type":"github_repo_path","repo":"o/r","ref":"main","path":"p.7z.001"},
               {"type":"github_repo_path","repo":"o/r","ref":"main","path":"p.7z.002"}],
      "install":{"rules":[{"from":"b.bin","to":"romfs/b.bin"}]}}]})";
    SECTION("one failure is retried") {
        const std::string dir = TempDir("retry");
        FakeFiles net;
        net.files[PartUrl("p.7z.001")] = whole.substr(0, 400);
        net.files[PartUrl("p.7z.002")] = whole.substr(400);
        net.fail_first[PartUrl("p.7z.002")] = 1;
        auto library_owner = MakeParts(dir, json, net);
    Library& library = *library_owner;
        const auto out = library.Install(kTid, "s7");
        INFO(out.error);
        REQUIRE(out.result == InstallResult::Ok);
        CHECK(std::count(net.log.begin(), net.log.end(), PartUrl("p.7z.002")) == 2);
        CHECK(Read(dir + "mods/" + kTidText + "/romfs/b.bin") == "hello seven");
    }
    SECTION("two failures end it") {
        const std::string dir = TempDir("retry2");
        FakeFiles net;
        net.files[PartUrl("p.7z.001")] = whole.substr(0, 400);
        net.files[PartUrl("p.7z.002")] = whole.substr(400);
        net.fail_first[PartUrl("p.7z.001")] = 2;
        auto library_owner = MakeParts(dir, json, net);
    Library& library = *library_owner;
        const auto out = library.Install(kTid, "s7");
        REQUIRE(out.result == InstallResult::Error);
        CHECK(out.error == "Connection reset");
        CHECK(library.GetState().text == "Install failed: Connection reset");
        CHECK(std::count(net.log.begin(), net.log.end(), PartUrl("p.7z.001")) == 2);
        CHECK(std::count(net.log.begin(), net.log.end(), PartUrl("p.7z.002")) == 0);
        CHECK(Tree(dir + "mods/").empty());
        CHECK_FALSE(fs::exists(dir + "mod-downloads/s7"));
        CHECK(ShortFailure(out.error) == "No Connection");
    }
    SECTION("a missing part") {
        const std::string dir = TempDir("retry3");
        FakeFiles net;
        net.files[PartUrl("p.7z.001")] = whole.substr(0, 400);
        auto library_owner = MakeParts(dir, json, net);
    Library& library = *library_owner;
        const auto out = library.Install(kTid, "s7");
        REQUIRE(out.result == InstallResult::Error);
        CHECK(out.error == "Not found");
    }
}

TEST_CASE("Catalog3: JoinParts and FixSplitZip", "[mods][catalog3]") {
    const std::string dir = TempDir("join");
    const V parts{"abc", "defg", "h"};
    V paths;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        paths.push_back(dir + "p" + std::to_string(i));
        std::ofstream(paths.back(), std::ios::binary) << parts[i];
    }
    u64 last = 0;
    REQUIRE(Multipart::JoinParts(paths, dir + "joined", nullptr, [&](u64 done, u64) { last = done; }).empty());
    CHECK(Read(dir + "joined") == "abcdefgh");
    CHECK(last == 8);
    CHECK(Multipart::JoinParts({dir + "nope"}, dir + "j2") == "A part is missing");
    // a single-disk zip is left as it is
    const std::string zip = StoredZip({{"a.txt", "hi"}});
    std::ofstream(dir + "one.zip", std::ios::binary) << zip;
    CHECK(Multipart::FixSplitZip(dir + "one.zip", {zip.size()}).empty());
    CHECK(Read(dir + "one.zip") == zip);
    CHECK(Multipart::FixSplitZip(dir + "joined", {8}) == "Not a zip archive");
}

TEST_CASE("Catalog3: downloads and updated reach the menu's list", "[mods][catalog3]") {
    const std::string dir = TempDir("avail");
    auto parsed = ParseCatalog(R"({"schema":1,"mods":[{"id":"a","name":"A","title_ids":["0004000000033500"],
      "author":"x","downloads":4321,"updated":"2026-07","source":{"type":"gamebanana","mod":5},
      "textures":{"rules":[{"from":"t/","to":""}]}},
      {"id":"b","name":"B","title_ids":["0004000000033500"],"author":"y",
      "parts":[{"type":"gamebanana","mod":6},{"type":"gamebanana","mod":6,"file_pattern":"*.002"}]}]})");
    REQUIRE(parsed.has_value());
    Library library(dir, *parsed);
    const auto list = library.ListAvailable(kTid);
    REQUIRE(list.size() == 2);
    const auto a = std::find_if(list.begin(), list.end(), [](const auto& m) { return m.id == "a"; });
    CHECK(a->downloads == 4321);
    CHECK(a->updated == "2026-07");
    CHECK(a->has_textures);
    CHECK_FALSE(a->multipart);
    const auto b = std::find_if(list.begin(), list.end(), [](const auto& m) { return m.id == "b"; });
    CHECK(b->multipart);
    CHECK(b->downloads == 0);
}

// The real thing, by hand: ./tests "[.network]" (GameBanana's API and hosts). The parts path with a
// real 2.8 MB file (Etrian Odyssey Nexus HD Textures' hd_texts.7z, GameBanana 40630): resolve, space
// check, streamed download, join, bsdtar, the textures rules, into a dev dir.
TEST_CASE("Catalog3: real multipart install", "[.network]") {
    if (FindBsdtar().empty()) {
        SKIP("no bsdtar");
    }
    const std::string dir = TempDir("real");
    auto parsed = ParseCatalog(R"({"schema":1,"mods":[{"id":"eo-texts","name":"EO Texts","title_ids":["0004000000033500"],
      "author":"x","parts":[{"type":"gamebanana","mod":40630,"file_pattern":"hd_texts.7z"}],
      "textures":{"rules":[{"from":"HD Texts/","to":""}]}}]})");
    REQUIRE(parsed.has_value());
    Library library(dir, *parsed);
    const auto out = library.Install(kTid, "eo-texts");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    CHECK(out.textures_installed);
    const auto tree = Tree(dir + "textures/" + kTidText + "/eo-texts/");
    INFO(tree.size());
    CHECK(tree.size() >= 30);
    CHECK(tree.front().rfind("tex1_", 0) == 0);
    CHECK(Tree(dir + "mods/").empty());
    REQUIRE(library.Remove(kTid, "eo-texts").ok);
    CHECK(Tree(dir + "textures/").empty());
}

// ---------------------------------------------------------------------------------------------

TEST_CASE("Catalog3: parts win over source, size and the raw GitHub host", "[mods][catalog3]") {
    const auto c = ParseCatalog(R"({"schema":1,"mods":[
 {"id":"x","name":"X","title_ids":["0004000000033500"],"size":5000000000,
  "source":{"type":"gamebanana","mod":1,"file_pattern":"*.part1.rar"},
  "parts":[{"type":"gamebanana","mod":1,"file_pattern":"*.part1.rar"},{"type":"gamebanana","mod":1,"file_pattern":"*.part2.rar"},
           {"type":"gamebanana","mod":1,"file_pattern":"*.part3.rar"},{"type":"gamebanana","mod":1,"file_pattern":"*.part4.rar"}]}]})");
    REQUIRE(c.has_value());
    const auto* x = c->Find("x");
    REQUIRE(x != nullptr);
    CHECK(x->parts.size() == 4);
    CHECK(x->size == 5'000'000'000ull);
    CHECK(x->source.asset_pattern == "*.part1.rar");
    const auto list = Library(TempDir("sz"), *c).ListAvailable(kTid);
    REQUIRE(list.size() == 1);
    CHECK(list[0].size == 5'000'000'000ull);
    CHECK(list[0].multipart);
    // a texture pack from a raw GitHub file (https source)
    namespace Dl = CustomTex::Download;
    CHECK(Dl::HostAllowed("https://raw.githubusercontent.com"));
    CHECK_FALSE(Dl::HostAllowed("https://raw.githubusercontent.com.evil.example"));
    const auto packs = Dl::ParseCatalog(R"({"schema":1,"packs":[{"id":"r","name":"R","title_ids":["0004000000033500"],
      "source":{"type":"https","url":"https://raw.githubusercontent.com/o/r/main/pack.zip"}}]})");
    REQUIRE(packs.has_value());
    REQUIRE(packs->packs.size() == 1);
}

TEST_CASE("Catalog3: a GameBanana file that is not scanned yet is refused, and says why", "[mods][catalog3]") {
    const std::string profile = R"({"_sVersion":"1","_aFiles":[
      {"_sFile":"a.zip","_nFilesize":10,"_sAvResult":"unknown","_tsDateAdded":5,"_sDownloadUrl":"https://gamebanana.com/dl/1"}]})";
    const auto fake = [&](const std::string& host, const std::string& path, std::size_t) {
        HttpReply r;
        r.status = (host + path) == "https://gamebanana.com/apiv11/Mod/5/ProfilePage" ? 200 : 404;
        r.body = profile;
        return r;
    };
    SECTION("one file") {
        auto parsed = ParseCatalog(R"({"schema":1,"mods":[{"id":"gb","name":"GB","title_ids":["0004000000033500"],
          "author":"A","source":{"type":"gamebanana","mod":5,"file_pattern":"*.zip"}}]})");
        REQUIRE(parsed.has_value());
        Library library(TempDir("unscanned"), *parsed, fake);
        const auto out = library.Install(kTid, "gb");
        CHECK(out.result == InstallResult::Error);
        CHECK(out.error == "GameBanana Hasn't Scanned This File Yet");
        CHECK(ShortFailure(out.error) == "GameBanana Hasn't Scanned This File Yet");
    }
    SECTION("a part") {
        auto parsed = ParseCatalog(R"({"schema":1,"mods":[{"id":"gb","name":"GB","title_ids":["0004000000033500"],
          "author":"A","parts":[{"type":"gamebanana","mod":5,"file_pattern":"*.zip"}]}]})");
        REQUIRE(parsed.has_value());
        Library library(TempDir("unscanned2"), *parsed, fake);
        FakeFiles none;
        library.SetFileTransport(none.Get());
        const auto out = library.Install(kTid, "gb");
        CHECK(out.result == InstallResult::Error);
        CHECK(out.error == "GameBanana Hasn't Scanned This File Yet");
        CHECK(none.log.empty());
    }
}

TEST_CASE("Catalog3: a download over the memory cap goes to disk", "[mods][catalog3]") {
    // the listed size is over 128 MB, so the file is streamed (the fake serves a short file: the size
    // check then fails, which shows the streaming path was taken)
    const std::string profile = R"({"_sVersion":"1","_aFiles":[
      {"_sFile":"a.zip","_nFilesize":900000000,"_sAvResult":"clean","_tsDateAdded":5,"_sDownloadUrl":"https://gamebanana.com/dl/1"}]})";
    auto parsed = ParseCatalog(R"({"schema":1,"mods":[{"id":"gb","name":"GB","title_ids":["0004000000033500"],
      "author":"A","source":{"type":"gamebanana","mod":5,"file_pattern":"*.zip"}}]})");
    REQUIRE(parsed.has_value());
    bool memory_download = false;
    Library library(TempDir("big"), *parsed, [&](const std::string& host, const std::string& path, std::size_t) {
        HttpReply r;
        r.status = 404;
        if ((host + path) == "https://gamebanana.com/apiv11/Mod/5/ProfilePage") {
            r.status = 200;
            r.body = profile;
        } else if (path.find("/dl/") != std::string::npos) {
            memory_download = true;
        }
        return r;
    });
    FakeFiles net;
    net.files["https://gamebanana.com/dl/1"] = "short";
    library.SetFileTransport(net.Get());
    library.SetFreeSpace([](const std::string&, u64* out) {
        *out = 1ull << 40;
        return true;
    });
    const auto out = library.Install(kTid, "gb");
    CHECK(out.result == InstallResult::Error);
    CHECK(out.error == "Download incomplete (the size is not the one listed)");
    CHECK_FALSE(memory_download);
    REQUIRE(net.log.size() == 1);
    CHECK(net.log[0] == "https://gamebanana.com/dl/1");
}

// ---------------------------------------------------------------------------------------------
// Round 2: https and Google Drive sources, loose parts, the 7zz fallback

namespace {
constexpr const char* kDriveId = "1AbCdEfGhIjKlMnOpQrStUv";
std::string DriveGet(const std::string& id) {
    return "https://drive.usercontent.google.com/download?id=" + id + "&export=download&confirm=t";
}
std::string ConfirmPage(const std::string& id) {
    return "<html><body><form id=\"download-form\" action=\"https://drive.usercontent.google.com/download\" "
           "method=\"get\"><input type=\"hidden\" name=\"id\" value=\"" + id +
           "\"><input type=\"hidden\" name=\"export\" value=\"download\"><input type=\"hidden\" name=\"confirm\" "
           "value=\"t\"><input type=\"hidden\" name=\"uuid\" value=\"u-1\"></form></body></html>";
}
std::string DriveGetConfirmed(const std::string& id) {
    return "https://drive.usercontent.google.com/download?id=" + id + "&export=download&confirm=t&uuid=u-1";
}
std::string OneEntry(const std::string& source_json, const char* rules = R"("rules":[{"from":"load/mods/{tid}/","to":""}])") {
    return std::string(R"({"schema":1,"mods":[{"id":"big-pack","name":"Big Pack","title_ids":["0004000000033500"],
      "author":"A","licence":"MIT",)") + source_json + R"(,"install":{)" + rules + "}}]}";
}
} // namespace

TEST_CASE("Catalog3 r2: https, gdrive and gdrive_folder sources parse", "[mods][catalog3]") {
    auto ok = ParseCatalog(OneEntry(R"("source":{"type":"https","url":"https://dl.example.org/files/p.zip","hosts":["cdn.example.org"]})"));
    REQUIRE(ok);
    REQUIRE(ok->mods.size() == 1);
    CHECK(ok->mods[0].source.type == Source::Type::Https);
    CHECK(ok->mods[0].source.url == "https://dl.example.org/files/p.zip");
    CHECK(ok->mods[0].source.hosts == V{"dl.example.org", "cdn.example.org"});
    CHECK_FALSE(ok->mods[0].verified);

    ok = ParseCatalog(OneEntry(std::string(R"("source":{"type":"gdrive","file_id":")") + kDriveId + R"("})"));
    REQUIRE(ok);
    CHECK(ok->mods[0].source.type == Source::Type::Gdrive);
    CHECK(ok->mods[0].source.file_id == kDriveId);

    ok = ParseCatalog(OneEntry(std::string(R"("source":{"type":"gdrive_folder","folder_id":")") + kDriveId +
                               R"(","file_pattern":"*.7z"})"));
    REQUIRE(ok);
    CHECK(ok->mods[0].source.type == Source::Type::GdriveFolder);
    CHECK(ok->mods[0].source.folder_id == kDriveId);
    CHECK(ok->mods[0].source.asset_pattern == "*.7z");

    ok = ParseCatalog(OneEntry(std::string(R"("parts":[{"type":"gdrive","file_id":")") + kDriveId +
                               R"(","name":"a.fa"},{"type":"gdrive","file_id":")" + kDriveId + R"(2","name":"b.fa"}])"));
    REQUIRE(ok);
    CHECK(ok->mods[0].parts.size() == 2);
    CHECK(ok->mods[0].parts[1].name == "b.fa");

    // refused
    CHECK(ParseCatalog(OneEntry(R"("source":{"type":"https","url":"http://dl.example.org/p.zip"})"))->mods.empty());
    CHECK(ParseCatalog(OneEntry(R"("source":{"type":"https","url":"https://dl.example.org/../p.zip"})"))->mods.empty());
    CHECK(ParseCatalog(OneEntry(R"("source":{"type":"https","url":"https://dl.example.org/p.zip","hosts":["*.example.org"]})"))->mods.empty());
    CHECK(ParseCatalog(OneEntry(R"("source":{"type":"gdrive","file_id":"x"})"))->mods.empty());
    CHECK(ParseCatalog(OneEntry(std::string(R"("source":{"type":"gdrive_folder","folder_id":")") + kDriveId + R"("})"))->mods.empty());
    CHECK(ParseCatalog(OneEntry(std::string(R"("parts":[{"type":"gdrive","file_id":")") + kDriveId + R"(","name":"../x"}])"))->mods.empty());
}

TEST_CASE("Catalog3 r2: an https source is downloaded to disk and unpacked", "[mods][catalog3]") {
    const std::string dir = TempDir("r2https");
    FakeFiles net;
    net.files["https://dl.example.org/p.zip"] = StoredZip(PackMembers());
    auto lib = MakeParts(dir, OneEntry(R"("source":{"type":"https","url":"https://dl.example.org/p.zip"})",
                                       R"("rules":[{"from":"load/mods/{tid}/","to":""}])"), net);
    const auto out = lib->Install(kTid, "big-pack");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    CHECK(Tree(dir + "mods/" + kTidText + "/") == V{"exefs/code.ips", "romfs/a.bin"});
    CHECK(Read(dir + "mods/" + kTidText + "/romfs/a.bin") == kRomfs);
}

TEST_CASE("Catalog3 r2: a Drive file goes through the confirm page", "[mods][catalog3]") {
    const std::string dir = TempDir("r2gdrive");
    FakeFiles net;
    net.html[DriveGet(kDriveId)] = ConfirmPage(kDriveId);
    net.files[DriveGetConfirmed(kDriveId)] = StoredZip(PackMembers());
    auto lib = MakeParts(dir, OneEntry(std::string(R"("source":{"type":"gdrive","file_id":")") + kDriveId + R"("})"), net);
    const auto out = lib->Install(kTid, "big-pack");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    CHECK(Read(dir + "mods/" + kTidText + "/romfs/a.bin") == kRomfs);
    CHECK(net.hits[DriveGetConfirmed(kDriveId)] == 1);
}

TEST_CASE("Catalog3 r2: a Drive quota page is reported", "[mods][catalog3]") {
    const std::string dir = TempDir("r2quota");
    FakeFiles net;
    net.html[DriveGet(kDriveId)] =
        "<html><body>Sorry, you can't view or download this file at this time. Too many users have viewed or "
        "downloaded this file recently.</body></html>";
    auto lib = MakeParts(dir, OneEntry(std::string(R"("source":{"type":"gdrive","file_id":")") + kDriveId + R"("})"), net);
    const auto out = lib->Install(kTid, "big-pack");
    CHECK(out.result == InstallResult::Error);
    CHECK_FALSE(out.error.empty());
    CHECK_FALSE(fs::exists(dir + "mods/" + kTidText));
}

TEST_CASE("Catalog3 r2: parts that are loose files are placed by the install rules", "[mods][catalog3]") {
    const std::string dir = TempDir("r2loose");
    const std::string id2 = std::string(kDriveId) + "2";
    FakeFiles net;
    net.files[DriveGet(kDriveId)] = Pattern(2000, 4);
    net.files[DriveGet(id2)] = Pattern(500, 5);
    auto lib = MakeParts(dir,
                         OneEntry(std::string(R"("parts":[{"type":"gdrive","file_id":")") + kDriveId +
                                      R"(","name":"big.fa"},{"type":"gdrive","file_id":")" + id2 + R"(","name":"small.fa"}])",
                                  R"("rules":[{"from":"*","to":"romfs/"}])"),
                         net);
    const auto out = lib->Install(kTid, "big-pack");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    const std::string mods = dir + "mods/" + kTidText + "/";
    CHECK(Tree(mods) == V{"romfs/big.fa", "romfs/small.fa"});
    CHECK(Read(mods + "romfs/big.fa") == Pattern(2000, 4));
    CHECK(Read(mods + "romfs/small.fa") == Pattern(500, 5));
}

TEST_CASE("Catalog3 r2: a Drive folder part is found by its file pattern", "[mods][catalog3]") {
    const std::string dir = TempDir("r2folder");
    FakeFiles net;
    const std::string file_id = "1FileIdFileIdFileId00000001";
    net.files[DriveGet(file_id)] = Pattern(700, 6);
    const std::string page =
        "<html><body><div class=\"flip-entries\"><div class=\"flip-entry\" id=\"entry-" + file_id +
        "\" tabindex=\"0\" role=\"link\"><div class=\"flip-entry-info\"><a href=\"https://drive.google.com/file/d/" +
        file_id + "/view\" target=\"_blank\"><div class=\"flip-entry-title\">patch_01.fa</div></a></div></div>"
        "</div></body></html>";
    auto parsed = ParseCatalog(OneEntry(std::string(R"("parts":[{"type":"gdrive_folder","folder_id":")") + kDriveId +
                                            R"(","file_pattern":"*.fa"}])",
                                        R"("rules":[{"from":"*","to":"romfs/"}])"));
    REQUIRE(parsed);
    Library lib(dir, *parsed, [&](const std::string& host, const std::string& path, std::size_t) {
        HttpReply r;
        if (host == "https://drive.google.com" && path == std::string("/embeddedfolderview?id=") + kDriveId) {
            r.status = 200;
            r.body = page;
        } else {
            r.status = 404;
        }
        return r;
    });
    lib.SetFileTransport(net.Get());
    lib.SetFreeSpace([](const std::string&, u64* out) {
        *out = 1ull << 40;
        return true;
    });
    const auto out = lib.Install(kTid, "big-pack");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    CHECK(Tree(dir + "mods/" + kTidText + "/") == V{"romfs/patch_01.fa"});
}

TEST_CASE("Catalog3 r2: loose parts with the same name are refused", "[mods][catalog3]") {
    const std::string dir = TempDir("r2dup");
    const std::string id2 = std::string(kDriveId) + "2";
    FakeFiles net;
    net.files[DriveGet(kDriveId)] = "one";
    net.files[DriveGet(id2)] = "two";
    auto lib = MakeParts(dir,
                         OneEntry(std::string(R"("parts":[{"type":"gdrive","file_id":")") + kDriveId +
                                      R"(","name":"x.fa"},{"type":"gdrive","file_id":")" + id2 + R"(","name":"x.fa"}])",
                                  R"("rules":[{"from":"*","to":"romfs/"}])"),
                         net);
    CHECK(lib->Install(kTid, "big-pack").result == InstallResult::Error);
}

TEST_CASE("Catalog3 r2: 7zz reads a solid RAR4 that bsdtar refuses", "[mods][catalog3]") {
    if (FindSevenZip().empty()) {
        SKIP("no 7zz ($AZAHAR_7ZZ)");
    }
    const std::string bytes(reinterpret_cast<const char*>(kSolidRar), sizeof(kSolidRar));
    REQUIRE(DetectArchive(bytes) == ArchiveKind::Rar);
    const auto r = ExtractArchive(bytes, ArchiveKind::Rar);
    INFO(r.error);
    REQUIRE(r.error.empty());
    std::vector<std::string> names;
    for (const auto& f : r.files) {
        names.push_back(f.path);
    }
    std::sort(names.begin(), names.end());
    CHECK(names == V{"00040000000D5A00/code.bps", "00040000000D5A00/exefs/code.bps", "disclaimer.txt",
                     "instructions.txt"});

    // and the on-disk path
    const std::string dir = TempDir("r2solid");
    std::ofstream(dir + "a.rar", std::ios::binary) << bytes;
    REQUIRE(ExtractFileToDir(dir + "a.rar", ArchiveKind::Rar, dir + "out", 1 << 20).empty());
    CHECK(Tree(dir + "out/").size() == 4);
}

TEST_CASE("Catalog3 r2: the real archives bsdtar cannot read", "[.network]") {
    // fetched by hand into $R2_ARCHIVES (default /tmp/r2arch): Deflate64 zip, solid RAR4 x2, RAR with
    // an invalid symbol, RAR5 with a 1 GB dictionary
    const char* env = std::getenv("R2_ARCHIVES");
    const std::string base = env ? env : "/tmp/r2arch";
    if (FindSevenZip().empty()) {
        SKIP("no 7zz");
    }
    for (const auto& [file, kind] : std::vector<std::pair<std::string, ArchiveKind>>{
             {"delta.zip", ArchiveKind::Zip}, {"dream.rar", ArchiveKind::Rar}, {"paper.rar", ArchiveKind::Rar},
             {"hard.rar", ArchiveKind::Rar}, {"bd.rar", ArchiveKind::Rar}}) {
        if (!fs::exists(base + "/" + file)) {
            WARN("missing " + file);
            continue;
        }
        const std::string out = TempDir(("r2real_" + file.substr(0, file.find('.'))).c_str()) + "out";
        const std::string problem = ExtractFileToDir(base + "/" + file, kind, out, 20ull << 30);
        INFO(file << ": " << problem);
        CHECK(problem.empty());
        CHECK_FALSE(Tree(out + "/").empty());
    }
}
