// Copyright Azahar for Frame contributors
// Licensed under GPLv2 or any later version
// Refer to the misc/licenses/gplv2.txt file included.

// AzaharFrame moddata: zip extraction, catalog parsing, the archive-to-mods-folder mapping (exact
// resulting paths for each layout), and the library (install, manifest, conflicts, replace, remove,
// unmanaged items) with a fake network. The real download test is hidden: [.network].

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <catch2/catch_test_macros.hpp>
#include "core/mods/library.h"

using namespace Mods;
namespace fs = std::filesystem;

namespace {

constexpr u64 kTid = 0x0004000000033500ULL; // OoT3D USA
constexpr const char* kTidText = "0004000000033500";

// Written with Python's zipfile (deflate, level 9): a fixed-Huffman and a dynamic-Huffman member.
const unsigned char kSmallZip[] = {
    0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x4e, 0x95, 0x6b, 0x5d, 0xe3, 0x51,
    0x3d, 0x8d, 0x0a, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x61, 0x2e,
    0x74, 0x78, 0x74, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0xc8, 0x40, 0x27, 0x01, 0x50, 0x4b, 0x01,
    0x02, 0x14, 0x03, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x4e, 0x95, 0x6b, 0x5d, 0xe3, 0x51, 0x3d,
    0x8d, 0x0a, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x61, 0x2e, 0x74, 0x78, 0x74,
    0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x33, 0x00, 0x00, 0x00,
    0x2d, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const unsigned char kBigZip[] = {
    0x50, 0x4b, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x4e, 0x95, 0x6b, 0x5d, 0x39, 0xf2,
    0x5b, 0x42, 0x1f, 0x05, 0x00, 0x00, 0x64, 0x22, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x64, 0x69,
    0x72, 0x2f, 0x62, 0x69, 0x67, 0x2e, 0x74, 0x78, 0x74, 0x7d, 0xda, 0xc1, 0x72, 0xe4, 0x36, 0x0c,
    0x84, 0xe1, 0x3b, 0x9f, 0x42, 0xaf, 0xe6, 0x24, 0x4e, 0xf6, 0xb0, 0xae, 0x6c, 0x39, 0x3e, 0xe4,
    0xf1, 0x93, 0x98, 0x93, 0x9d, 0x8f, 0xfc, 0x45, 0x5f, 0x54, 0x53, 0xb2, 0x44, 0x12, 0x40, 0xa3,
    0xd1, 0x80, 0xfc, 0xfe, 0xe7, 0xdb, 0xef, 0x7f, 0x5d, 0x2f, 0xdf, 0x7f, 0x7c, 0x7b, 0xb9, 0xfe,
    0x78, 0x79, 0x7b, 0xfb, 0xff, 0xfa, 0xfe, 0xdf, 0xfd, 0xf1, 0x79, 0xbd, 0x7e, 0x79, 0xfd, 0x78,
    0xdc, 0xb8, 0x5e, 0xff, 0x7e, 0xfd, 0xf7, 0x3a, 0x7f, 0xcf, 0x07, 0xe7, 0x9d, 0xdf, 0x5e, 0xbf,
    0x7f, 0xbc, 0x8c, 0x79, 0x9d, 0x77, 0x7e, 0xbc, 0x7c, 0xfc, 0xfa, 0x6d, 0xbe, 0xca, 0xcf, 0xf9,
    0xe6, 0x7c, 0x6e, 0x6e, 0x3a, 0x37, 0x9a, 0x8f, 0xbc, 0x73, 0x98, 0x79, 0xe7, 0xf3, 0xc9, 0xc7,
    0x7d, 0x96, 0x71, 0xa3, 0x79, 0x9d, 0x2f, 0xf1, 0xf8, 0x3c, 0xfb, 0xf8, 0x3c, 0xe4, 0x5c, 0x7e,
    0x68, 0x9f, 0xc7, 0x66, 0xe1, 0xb9, 0xcc, 0xfc, 0xe3, 0xe7, 0x8d, 0xdd, 0x80, 0xf9, 0xf3, 0xf9,
    0x80, 0x4b, 0x0f, 0x4e, 0xfe, 0xf0, 0xc5, 0xdc, 0x8c, 0xa5, 0xe7, 0xfb, 0xf3, 0xb7, 0x77, 0xe6,
    0x83, 0xfe, 0x9e, 0xb6, 0xec, 0x9b, 0x8e, 0xae, 0x4e, 0x90, 0xe6, 0x3b, 0xf3, 0x71, 0x5d, 0x3a,
    0x5f, 0xc5, 0xbd, 0xe3, 0xa7, 0x71, 0xd3, 0x8b, 0x43, 0x47, 0x3f, 0x17, 0x1c, 0xc6, 0x4b, 0x04,
    0x68, 0x80, 0x9b, 0xba, 0x85, 0x0b, 0xeb, 0xf4, 0xf9, 0x57, 0xcc, 0x63, 0x93, 0xe5, 0x04, 0xf3,
    0x8e, 0x31, 0x2a, 0x4e, 0x09, 0xcd, 0xdc, 0xcf, 0x47, 0x9e, 0x7e, 0x17, 0x0d, 0x0f, 0x7c, 0xe0,
    0x30, 0xaf, 0xb9, 0x0d, 0xca, 0xc6, 0x21, 0x90, 0xe2, 0x93, 0xc5, 0x9f, 0x87, 0x10, 0x07, 0x73,
    0x3d, 0x43, 0xe7, 0x1d, 0x9d, 0x89, 0x7b, 0x4d, 0xaa, 0xcf, 0xc5, 0x87, 0x0f, 0x02, 0x12, 0xf1,
    0x39, 0x41, 0xe9, 0xe1, 0x82, 0xf4, 0xa2, 0x49, 0xa3, 0xa7, 0x2d, 0xe6, 0xe9, 0x7d, 0x3a, 0xcc,
    0x55, 0xf4, 0x45, 0x91, 0xa1, 0x91, 0x86, 0xce, 0xe3, 0xc9, 0x1c, 0xbb, 0xc1, 0x57, 0xed, 0x98,
    0x7f, 0x94, 0x28, 0xb0, 0xac, 0xa9, 0xc0, 0x9d, 0x11, 0xcb, 0x96, 0xcc, 0x37, 0x69, 0x48, 0x9d,
    0x21, 0x2c, 0x44, 0x34, 0xd9, 0x3f, 0x5c, 0xc1, 0x30, 0xba, 0xaf, 0xc1, 0x87, 0x39, 0x97, 0xdd,
    0x71, 0xaf, 0x8f, 0x34, 0x9f, 0x71, 0x8e, 0xe7, 0x10, 0x3c, 0x00, 0x86, 0x65, 0x4d, 0x35, 0x41,
    0x6e, 0xa4, 0x60, 0x42, 0x13, 0xff, 0x8b, 0x05, 0x9b, 0x57, 0xf3, 0x3c, 0xe5, 0x73, 0x8e, 0x2f,
    0x55, 0x98, 0x5e, 0x1e, 0x2b, 0x80, 0x7d, 0x2c, 0x6c, 0x4e, 0x1b, 0x1a, 0xe1, 0xac, 0x7b, 0xa7,
    0x93, 0xe4, 0x7f, 0x8e, 0x30, 0x5f, 0x82, 0x32, 0xae, 0x2d, 0x93, 0x71, 0x80, 0xce, 0xc1, 0xe7,
    0x94, 0x13, 0x97, 0xbb, 0x81, 0x44, 0xc2, 0x6d, 0x41, 0x7b, 0x86, 0x75, 0xa1, 0x34, 0xdd, 0x98,
    0xfd, 0x63, 0xe6, 0x42, 0x9e, 0xf9, 0x2d, 0x8b, 0x0f, 0x39, 0xde, 0x28, 0xe9, 0x76, 0x57, 0x7e,
    0x16, 0x3f, 0x6b, 0x69, 0x39, 0xaf, 0x39, 0x56, 0x8a, 0x07, 0x6b, 0x83, 0x60, 0x0f, 0x7c, 0x9d,
    0x6a, 0xbe, 0x80, 0x55, 0x24, 0xc8, 0x97, 0x4d, 0x2e, 0xa9, 0xce, 0xe3, 0xb8, 0x9a, 0xa8, 0xb3,
    0x22, 0xb7, 0x2a, 0xf9, 0x3b, 0x69, 0x5e, 0x45, 0x01, 0x4f, 0x2e, 0x07, 0xd7, 0x81, 0x6e, 0xb2,
    0x7b, 0xe1, 0x61, 0x7f, 0xe8, 0x93, 0x04, 0x5f, 0x8c, 0x95, 0x09, 0xa3, 0x5e, 0xc2, 0x27, 0x37,
    0x61, 0x3f, 0xe5, 0x56, 0x48, 0x43, 0xb5, 0x80, 0xc5, 0x47, 0xcd, 0xe0, 0xe1, 0xf5, 0x8d, 0xb8,
    0xdc, 0xd1, 0xb6, 0xec, 0xfa, 0x65, 0x29, 0xed, 0x35, 0xd6, 0xfb, 0x92, 0xb6, 0x26, 0x31, 0x2a,
    0x5a, 0xc6, 0x61, 0xe1, 0x56, 0xbe, 0xc8, 0xcb, 0x66, 0x8a, 0x48, 0x14, 0x1e, 0x61, 0xd4, 0xac,
    0x1b, 0x01, 0x34, 0x88, 0xc1, 0x53, 0xd9, 0x2d, 0xdc, 0x8a, 0x8a, 0xf1, 0xcc, 0x98, 0x35, 0xe4,
    0xfd, 0xd6, 0x2f, 0xe2, 0xe0, 0x81, 0xcc, 0x24, 0xd2, 0xd5, 0x32, 0xcd, 0x9b, 0xe2, 0x09, 0x56,
    0x1c, 0x2d, 0xff, 0x4f, 0x95, 0x6a, 0x5e, 0x45, 0xe2, 0xe9, 0x38, 0xbd, 0xd2, 0x5a, 0x4c, 0x2c,
    0x24, 0x1d, 0x8e, 0x58, 0xdf, 0xf2, 0x7b, 0xd4, 0xdc, 0x6a, 0x8b, 0x00, 0xd4, 0x76, 0x49, 0xdc,
    0xe9, 0xfc, 0x93, 0xfe, 0x47, 0xe3, 0x2c, 0xd4, 0x78, 0xc2, 0x90, 0x68, 0x45, 0xe9, 0xd7, 0x1f,
    0x12, 0x5e, 0x18, 0x42, 0x70, 0x50, 0xeb, 0x53, 0xd5, 0x17, 0x92, 0xc4, 0xe2, 0xdd, 0xcd, 0x4b,
    0x7f, 0x64, 0x93, 0x26, 0xd8, 0x0d, 0xab, 0xec, 0x71, 0x1f, 0x68, 0xd9, 0xb5, 0x0e, 0x26, 0x06,
    0x65, 0xb5, 0x34, 0x35, 0x44, 0x5e, 0xb9, 0x3c, 0x5a, 0xaf, 0x64, 0xac, 0x96, 0x65, 0x11, 0x51,
    0x11, 0x42, 0xc5, 0x84, 0xd2, 0x5d, 0x05, 0xc3, 0x38, 0x3a, 0x04, 0x1b, 0xb3, 0xac, 0xec, 0x3d,
    0x15, 0x1b, 0x54, 0x55, 0x05, 0xbe, 0x37, 0x0d, 0x00, 0xdc, 0x71, 0x53, 0x45, 0x4f, 0x15, 0x81,
    0xb8, 0x1b, 0xc7, 0x43, 0x7f, 0x15, 0xdd, 0x20, 0x4a, 0x42, 0x58, 0x64, 0xaa, 0x66, 0x54, 0xcb,
    0x36, 0x22, 0x6d, 0xbd, 0x1b, 0xaf, 0x83, 0x3c, 0xf6, 0x4a, 0x0f, 0x96, 0xd4, 0xb8, 0x17, 0x2c,
    0x0b, 0x1b, 0x26, 0x1c, 0x8b, 0x18, 0x31, 0x89, 0x03, 0x5c, 0x81, 0xbf, 0xf7, 0x3b, 0x37, 0xc3,
    0x8d, 0x96, 0x6d, 0x83, 0x0d, 0x4e, 0x46, 0x53, 0x04, 0x86, 0xb4, 0xcb, 0xb2, 0x0a, 0x62, 0x7c,
    0x99, 0xb0, 0x79, 0x22, 0x06, 0xf4, 0x78, 0xe3, 0x71, 0xd2, 0xa0, 0x22, 0x42, 0xdb, 0x4e, 0xdd,
    0x0c, 0xe5, 0x08, 0x3f, 0xa5, 0x81, 0x5c, 0x00, 0xf8, 0xa4, 0xcc, 0x45, 0x6b, 0xee, 0x91, 0xbf,
    0x22, 0xf0, 0x58, 0x64, 0x99, 0x36, 0x65, 0xf1, 0xeb, 0xd0, 0x5e, 0x9d, 0x46, 0x37, 0x27, 0xcf,
    0xb4, 0xe2, 0xb4, 0x36, 0xed, 0x34, 0x33, 0xf2, 0xea, 0x75, 0xba, 0x8f, 0xa2, 0xcc, 0x18, 0xa1,
    0x38, 0xcc, 0x10, 0x6f, 0x29, 0x94, 0x5a, 0xaf, 0x30, 0x32, 0x45, 0x92, 0xee, 0xe9, 0x03, 0x96,
    0xee, 0xc3, 0x7c, 0xc8, 0xac, 0x62, 0xf1, 0x07, 0x2d, 0xde, 0xde, 0xb5, 0xa6, 0x6b, 0xb8, 0x22,
    0x9e, 0xc7, 0xa9, 0x3b, 0x20, 0xf4, 0xa7, 0xcc, 0x3d, 0x44, 0x71, 0x84, 0xb9, 0x3b, 0xb1, 0x18,
    0xa1, 0xeb, 0x11, 0xf6, 0x74, 0xca, 0x79, 0xe2, 0x53, 0x5e, 0xaa, 0x54, 0x48, 0xbd, 0x7d, 0xc0,
    0x77, 0xef, 0x78, 0x3b, 0xb3, 0x69, 0x58, 0xa3, 0x03, 0xe8, 0x4b, 0x9a, 0x9f, 0x6d, 0x2e, 0xca,
    0x08, 0xf7, 0x13, 0xb3, 0x11, 0x2c, 0x8d, 0x53, 0xe7, 0xdf, 0xf2, 0x10, 0x85, 0x74, 0x0c, 0x51,
    0xc6, 0x95, 0x4b, 0x25, 0xec, 0xd0, 0x2b, 0xe3, 0x5f, 0xc7, 0xd4, 0xe9, 0xe4, 0xdb, 0x85, 0x45,
    0x32, 0x8c, 0x7b, 0x29, 0xb0, 0x93, 0x99, 0xe2, 0x4a, 0x9e, 0x6e, 0x77, 0x13, 0xf1, 0x11, 0xfd,
    0x7f, 0xa3, 0xee, 0xac, 0x14, 0x66, 0x93, 0xb9, 0x79, 0xe0, 0xfe, 0x45, 0xc7, 0xe1, 0x01, 0xa5,
    0x8a, 0x3a, 0x8c, 0x89, 0x01, 0x37, 0x9a, 0xf8, 0xc9, 0xbe, 0x84, 0x7c, 0xb4, 0x9d, 0xb3, 0x86,
    0xee, 0x9b, 0x5e, 0xa7, 0xa1, 0x5e, 0xfa, 0x9b, 0xb1, 0x2b, 0xce, 0xb1, 0xc7, 0xf7, 0x66, 0xe6,
    0xd4, 0xfa, 0xe1, 0xfc, 0xc9, 0xb1, 0x5d, 0xc6, 0x8d, 0xee, 0x9a, 0xe6, 0x74, 0x58, 0x61, 0xeb,
    0xce, 0x7d, 0x58, 0x77, 0xd0, 0x67, 0xcb, 0xf1, 0x92, 0xf2, 0x2e, 0x75, 0x12, 0x32, 0x0a, 0x88,
    0xf4, 0x1b, 0x19, 0x80, 0x9c, 0x5a, 0x26, 0x2b, 0x26, 0x5d, 0x6c, 0x53, 0x04, 0x0a, 0x6f, 0x76,
    0x56, 0xe6, 0x59, 0x1e, 0x33, 0x56, 0xbd, 0x9f, 0x2d, 0xa3, 0xb1, 0x87, 0xc6, 0x7d, 0xa9, 0x64,
    0xfb, 0x25, 0xea, 0x76, 0x56, 0xb8, 0x54, 0x93, 0x0e, 0xe0, 0xd1, 0xcd, 0x92, 0xcb, 0xfd, 0xd7,
    0x09, 0x3b, 0xc6, 0x65, 0x5c, 0xa6, 0x77, 0xab, 0x3e, 0xd2, 0x24, 0x2f, 0x67, 0x92, 0x7b, 0xf4,
    0x6c, 0x25, 0xb5, 0x1d, 0xc0, 0xa1, 0xc3, 0xec, 0xcc, 0x10, 0x72, 0xba, 0x41, 0xae, 0x09, 0xd0,
    0xe7, 0x5b, 0x1b, 0x5c, 0xa1, 0xb5, 0x48, 0x78, 0x76, 0xb4, 0x73, 0xff, 0x21, 0x64, 0x29, 0xd0,
    0x3f, 0x15, 0xbe, 0x65, 0x11, 0x70, 0xda, 0x10, 0x5a, 0x35, 0xda, 0xf8, 0xe9, 0x1a, 0x23, 0xe5,
    0xe1, 0x94, 0xb5, 0xfa, 0x7f, 0xff, 0x0c, 0xd0, 0x8e, 0xf7, 0x26, 0x57, 0x92, 0x8e, 0x1d, 0xb1,
    0xed, 0x55, 0xaa, 0x68, 0xce, 0x1c, 0xa6, 0x43, 0xd0, 0x7c, 0xeb, 0xaa, 0x08, 0xef, 0xdc, 0x2c,
    0x95, 0x7a, 0x98, 0xbd, 0x27, 0xf2, 0x66, 0xab, 0x0e, 0xc5, 0x0f, 0x1f, 0xa8, 0xa9, 0x29, 0xfd,
    0x6c, 0x94, 0x0e, 0x70, 0x44, 0x67, 0x99, 0xfe, 0x94, 0xc2, 0xb1, 0x73, 0xe9, 0xd8, 0x7b, 0xa6,
    0xb1, 0xeb, 0xa6, 0x25, 0x81, 0x24, 0x3b, 0xcb, 0xf5, 0xfd, 0xf4, 0x7f, 0xd9, 0xb9, 0x70, 0xce,
    0xc2, 0xe0, 0xd6, 0x9e, 0xe4, 0x80, 0x35, 0xad, 0x40, 0x57, 0x34, 0xdf, 0xed, 0x13, 0xf2, 0xb1,
    0xe9, 0xd4, 0x74, 0x14, 0x2b, 0x95, 0x5a, 0x01, 0xe2, 0x69, 0x00, 0x74, 0x12, 0xcc, 0x86, 0x50,
    0xaa, 0x0e, 0x86, 0x46, 0xbf, 0xbd, 0x54, 0x1e, 0x74, 0xfc, 0xb9, 0x75, 0x0d, 0x11, 0xb9, 0x25,
    0x5c, 0x87, 0x4a, 0xa9, 0x09, 0x01, 0x28, 0x65, 0x28, 0xff, 0x7d, 0xd0, 0x8f, 0xcc, 0xe0, 0xeb,
    0xf0, 0xa5, 0x7b, 0xaf, 0xa5, 0x37, 0x89, 0x18, 0x45, 0x3e, 0xee, 0x0b, 0xe0, 0xcd, 0x37, 0x82,
    0x43, 0xd1, 0xd3, 0xc1, 0xf9, 0x54, 0xfe, 0x0f, 0x50, 0x4b, 0x01, 0x02, 0x14, 0x03, 0x14, 0x00,
    0x00, 0x00, 0x08, 0x00, 0x4e, 0x95, 0x6b, 0x5d, 0x39, 0xf2, 0x5b, 0x42, 0x1f, 0x05, 0x00, 0x00,
    0x64, 0x22, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x64, 0x69, 0x72, 0x2f, 0x62, 0x69, 0x67, 0x2e, 0x74, 0x78,
    0x74, 0x50, 0x4b, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x39, 0x00, 0x00,
    0x00, 0x48, 0x05, 0x00, 0x00, 0x00, 0x00,
};

std::string BigText() { // what kBigZip's member holds
    static const char* words[] = {"alpha", "beta", "gamma", "delta", "romfs", "exefs", "patch"};
    unsigned x = 12345;
    std::string out;
    for (int i = 0; i < 1500; ++i) {
        x = (x * 1103515245u + 12345u) & 0x7fffffffu;
        out += words[(x >> 8) % 7];
        out += ((x >> 16) & 7) < 6 ? ' ' : '\n';
    }
    return out;
}

void Le(std::string& s, u32 v, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        s += static_cast<char>((v >> (8 * i)) & 0xFF);
    }
}

// A zip of stored (uncompressed) members.
std::string MakeZip(const std::vector<std::pair<std::string, std::string>>& files, bool bad_crc = false) {
    std::string out, cd;
    for (const auto& [name, data] : files) {
        const u32 crc = Crc32(data) ^ (bad_crc ? 1u : 0u);
        const u32 offset = static_cast<u32>(out.size());
        Le(out, 0x04034b50, 4); Le(out, 20, 2); Le(out, 0, 2); Le(out, 0, 2); Le(out, 0, 4);
        Le(out, crc, 4); Le(out, static_cast<u32>(data.size()), 4); Le(out, static_cast<u32>(data.size()), 4);
        Le(out, static_cast<u32>(name.size()), 2); Le(out, 0, 2);
        out += name; out += data;
        Le(cd, 0x02014b50, 4); Le(cd, 20, 2); Le(cd, 20, 2); Le(cd, 0, 2); Le(cd, 0, 2); Le(cd, 0, 4);
        Le(cd, crc, 4); Le(cd, static_cast<u32>(data.size()), 4); Le(cd, static_cast<u32>(data.size()), 4);
        Le(cd, static_cast<u32>(name.size()), 2); Le(cd, 0, 2); Le(cd, 0, 2); Le(cd, 0, 2); Le(cd, 0, 2);
        Le(cd, 0, 4); Le(cd, offset, 4);
        cd += name;
    }
    const u32 cd_offset = static_cast<u32>(out.size());
    out += cd;
    Le(out, 0x06054b50, 4); Le(out, 0, 2); Le(out, 0, 2); Le(out, static_cast<u32>(files.size()), 2);
    Le(out, static_cast<u32>(files.size()), 2); Le(out, static_cast<u32>(cd.size()), 4);
    Le(out, cd_offset, 4); Le(out, 0, 2);
    return out;
}

std::vector<ArchiveFile> Files(const std::vector<std::pair<std::string, std::string>>& v) {
    std::vector<ArchiveFile> out;
    for (const auto& [p, d] : v) {
        out.push_back({p, d});
    }
    return out;
}

std::vector<std::string> Paths(const MapResult& r) {
    std::vector<std::string> out;
    for (const auto& f : r.files) {
        out.push_back(f.path);
    }
    return out;
}
using V = std::vector<std::string>;

std::string TempDir(const char* name) {
    const std::string dir = std::string("/tmp/azf_mods_test_") + name + "/";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

std::string Read(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void Write(const std::string& path, const std::string& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream(path, std::ios::binary) << data;
}

// Every file under dir, relative, sorted.
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

const char* kCatalog = R"({"schema":1,"mods":[
 {"id":"owner-fix","name":"No Bloom","title":"Game","title_ids":["0004000000033500"],"author":"me",
  "licence":"MIT","verified":true,
  "source":{"type":"owner_repo_path","path":"games/g/patch/0004000000033500/code.ips"},
  "install":{"rules":[{"from":"code.ips","to":"code.ips"}]}},
 {"id":"reframed","name":"Reframed","title_ids":["0004000000033500","0004000000033600"],"author":"A",
  "licence":"GPL-3.0","verified":true,
  "source":{"type":"github_release","repo":"a/reframed","tag":"latest","asset_pattern":"Reframed-*-Mods.zip"},
  "install":{"rules":[{"from":"*/load/mods/{tid}/","to":""}]}},
 {"id":"hdhud","name":"HD HUD","title_ids":["0004000000033500"],"author":"B","licence":"MIT",
  "source":{"type":"github_repo_path","repo":"b/hud","ref":"main","path":"dist/"}},
 {"id":"seven","name":"Seven","title_ids":["0004000000033500"],"author":"C","licence":"GPL-2.0",
  "source":{"type":"github_release","repo":"c/seven","tag":"v1","asset_pattern":"seven_*.7z"},
  "install":{"rules":[{"from":"v100/citra/code.bps","to":"code.bps"}]}}
]})";

// A fake network: path -> reply, with a log of the requests.
struct FakeNet {
    std::map<std::string, HttpReply> replies; // key: host + path
    std::vector<std::string> log;
    Transport Get() {
        return [this](const std::string& host, const std::string& path, std::size_t) {
            log.push_back(host + path);
            const auto it = replies.find(host + path);
            if (it == replies.end()) {
                HttpReply r;
                r.status = 404;
                return r;
            }
            return it->second;
        };
    }
    void Ok(const std::string& key, std::string body) {
        HttpReply r;
        r.status = 200;
        r.body = std::move(body);
        replies[key] = std::move(r);
    }
    void Redirect(const std::string& key, const std::string& to) {
        HttpReply r;
        r.status = 302;
        r.location = to;
        replies[key] = std::move(r);
    }
};

std::string ReleaseJson(const std::string& tag, const std::string& asset, const std::string& url) {
    return "{\"tag_name\":\"" + tag + "\",\"assets\":[{\"name\":\"other.txt\",\"browser_download_url\":"
           "\"https://github.com/x\"},{\"name\":\"" + asset + "\",\"browser_download_url\":\"" + url + "\"}]}";
}

Library MakeLibrary(const std::string& dir, FakeNet& net, const char* catalog = kCatalog) {
    auto parsed = ParseCatalog(catalog);
    REQUIRE(parsed.has_value());
    return Library(dir, *parsed, net.Get());
}

} // namespace

// ---------------------------------------------------------------------------------------------

TEST_CASE("Mods: zip extraction", "[mods]") {
    SECTION("deflate, fixed Huffman") {
        const auto r = ExtractZip({reinterpret_cast<const char*>(kSmallZip), sizeof(kSmallZip)});
        REQUIRE(r.error.empty());
        REQUIRE(r.files.size() == 1);
        REQUIRE(r.files[0].path == "a.txt");
        REQUIRE(r.files[0].data == "hello hello hello hello");
    }
    SECTION("deflate, dynamic Huffman") {
        const auto r = ExtractZip({reinterpret_cast<const char*>(kBigZip), sizeof(kBigZip)});
        REQUIRE(r.error.empty());
        REQUIRE(r.files.size() == 1);
        REQUIRE(r.files[0].path == "dir/big.txt");
        REQUIRE(r.files[0].data == BigText());
    }
    SECTION("stored, folders skipped") {
        auto zip = MakeZip({{"d/", ""}, {"d/f.bin", std::string("\0\x01\x02", 3)}});
        const auto r = ExtractZip(zip);
        REQUIRE(r.error.empty());
        REQUIRE(r.files.size() == 1);
        REQUIRE(r.files[0].data == std::string("\0\x01\x02", 3));
    }
    SECTION("refusals") {
        REQUIRE_FALSE(ExtractZip("nonsense").error.empty());
        REQUIRE_FALSE(ExtractZip(MakeZip({{"a", "b"}}, true)).error.empty());              // CRC
        REQUIRE_FALSE(ExtractZip(MakeZip({{"../evil", "b"}})).error.empty());             // zip slip
        REQUIRE_FALSE(ExtractZip(MakeZip({{"/abs", "b"}})).error.empty());
        REQUIRE_FALSE(ExtractZip(MakeZip({{"a\\..\\b", "b"}})).error.empty());
        REQUIRE_FALSE(ExtractZip(MakeZip({{"a", std::string(100, 'x')}}), 50).error.empty()); // too big
        auto truncated = MakeZip({{"a", "bbbb"}});
        truncated.resize(truncated.size() - 30);
        REQUIRE_FALSE(ExtractZip(truncated).error.empty());
    }
    SECTION("kinds") {
        REQUIRE(DetectArchive("PK\x03\x04....") == ArchiveKind::Zip);
        REQUIRE(DetectArchive(std::string("7z\xBC\xAF\x27\x1C\0\x04", 8)) == ArchiveKind::SevenZip);
        REQUIRE(DetectArchive("Rar!\x1a\x07\x00") == ArchiveKind::Rar);
        REQUIRE(DetectArchive("PATCHEOF") == ArchiveKind::Unknown);
    }
}

TEST_CASE("Mods: catalog parsing", "[mods]") {
    SECTION("the compiled-in catalog") {
        std::vector<std::string> warnings;
        const auto c = ParseCatalog(DefaultCatalogJson(), nullptr, &warnings);
        REQUIRE(c.has_value());
        REQUIRE(warnings.empty());
        const auto* nb = c->Find("stella-glow-usa-no-bloom");
        REQUIRE(nb != nullptr);
        REQUIRE(nb->name == "No Bloom");
        REQUIRE(nb->verified);
        REQUIRE(nb->source.type == Source::Type::OwnerRepoPath);
        REQUIRE(nb->source.repo == "simplysanatori/3ds-game-fixes");
        REQUIRE(nb->ServesTitle(0x0004000000173700ULL));
        const auto* oot = c->Find("ocarina-reframed-single-screen");
        REQUIRE(oot != nullptr);
        REQUIRE_FALSE(oot->verified);
        REQUIRE(oot->title_ids.size() == 5);
        REQUIRE(oot->source.repo == "OTPR26/OOT3DHud");
        const auto* mm = c->Find("mm3d-project-restoration-v100");
        REQUIRE(mm != nullptr);
        REQUIRE_FALSE(mm->Installable()); // a 7z asset
        REQUIRE(nb->Installable());
        REQUIRE(oot->Installable());
    }
    SECTION("verified only for the maintainer's own source") {
        const auto c = ParseCatalog(kCatalog);
        REQUIRE(c.has_value());
        REQUIRE(c->Find("owner-fix")->verified);
        REQUIRE_FALSE(c->Find("reframed")->verified); // says true, source is a community release
    }
    SECTION("bad entries are dropped") {
        std::vector<std::string> warnings;
        const auto c = ParseCatalog(R"({"mods":[
          {"id":"Bad Id","name":"x","title_ids":["0004000000033500"],"source":{"type":"owner_repo_path","path":"a"}},
          {"id":"ok","name":"x","title_ids":["0004000000033500"],"source":{"type":"owner_repo_path","path":"a/b"}},
          {"id":"tid","name":"x","title_ids":["123"],"source":{"type":"owner_repo_path","path":"a"}},
          {"id":"dots","name":"x","title_ids":["0004000000033500"],"source":{"type":"owner_repo_path","path":"../a"}},
          {"id":"repo","name":"x","title_ids":["0004000000033500"],"source":{"type":"github_release","repo":"nope","asset_pattern":"*"}},
          {"id":"kind","name":"x","title_ids":["0004000000033500"],"source":{"type":"gamebanana","path":"a"}},
          {"id":"ok","name":"dup","title_ids":["0004000000033500"],"source":{"type":"owner_repo_path","path":"a"}}
        ]})", nullptr, &warnings);
        REQUIRE(c.has_value());
        REQUIRE(c->mods.size() == 1);
        REQUIRE(c->mods[0].id == "ok");
        REQUIRE(warnings.size() == 6);
    }
    SECTION("not a catalog") {
        std::string error;
        REQUIRE_FALSE(ParseCatalog("nope", &error).has_value());
        REQUIRE_FALSE(error.empty());
        REQUIRE_FALSE(ParseCatalog("[]", &error).has_value());
        REQUIRE_FALSE(ParseCatalog(R"({"mods":5})", &error).has_value());
    }
    SECTION("glob") {
        REQUIRE(GlobMatch("Reframed-*-Mods.zip", "Reframed-2.5.1-Single-Mods.zip"));
        REQUIRE_FALSE(GlobMatch("Reframed-*-Mods.zip", "Reframed-2.5.1-Mods.zip.sig"));
        REQUIRE(GlobMatch("*", "anything"));
        REQUIRE(GlobMatch("a*b*c", "aXXbYYc"));
        REQUIRE_FALSE(GlobMatch("a*b", "a"));
    }
}

TEST_CASE("Mods: archive layouts map to exact paths", "[mods]") {
    const std::string tid = kTidText;
    const std::string other = "0004000000033600";

    SECTION("plain: files at the top") {
        const auto r = MapArchive(Files({{"code.ips", "PATCH"}, {"exheader.bin", "h"}, {"README.txt", "r"},
                                         {"romfs/menu/hud.ctxb", "x"}}), {}, kTid);
        REQUIRE(Paths(r) == V{"code.ips", "exheader.bin", "romfs/menu/hud.ctxb"});
        REQUIRE(r.skipped == 1);
    }
    SECTION("plain in a folder named like the mod") {
        const auto r = MapArchive(Files({{"MyMod/exefs/code.bps", "BPS1"}, {"MyMod/romfs/a.bin", "x"},
                                         {"MyMod/notes.md", "n"}}), {}, kTid);
        REQUIRE(Paths(r) == V{"exefs/code.bps", "romfs/a.bin"});
    }
    SECTION("region subfolders: only the title's own") {
        const std::vector<Rule> rules{{"*/load/mods/{tid}/", ""}};
        const auto files = Files({{"North-America/load/mods/" + tid + "/code.ips", "PATCH"},
                                  {"North-America/load/mods/" + tid + "/romfs/menu/a.ctxb", "a"},
                                  {"Europe/load/mods/" + other + "/code.ips", "PATCH-EU"},
                                  {"Europe/load/mods/" + other + "/exheader.bin", "eu"},
                                  {"LICENSE.txt", "l"}});
        const auto r = MapArchive(files, rules, kTid);
        REQUIRE(Paths(r) == V{"code.ips", "romfs/menu/a.ctxb"});
        REQUIRE(r.files[0].data == "PATCH");
        const auto eu = MapArchive(files, rules, 0x0004000000033600ULL);
        REQUIRE(Paths(eu) == V{"code.ips", "exheader.bin"});
        REQUIRE(eu.files[0].data == "PATCH-EU");
        // the same without rules (detected)
        REQUIRE(Paths(MapArchive(files, {}, kTid)) == V{"code.ips", "romfs/menu/a.ctxb"});
        // lower case title ID in the archive
        std::string lower = tid;
        for (auto& c : lower) c = static_cast<char>(std::tolower(c));
        REQUIRE(MapPath("R/load/mods/" + lower + "/code.ips", rules, kTid) == "code.ips");
    }
    SECTION("Luma layout: luma/titles/<TID>/") {
        const auto files = Files({{"luma/titles/" + tid + "/code.ips", "PATCH"},
                                  {"luma/titles/" + tid + "/exheader.bin", "h"},
                                  {"luma/titles/" + tid + "/romfs/actor/x.zar", "z"},
                                  {"luma/titles/" + other + "/code.ips", "no"}});
        REQUIRE(Paths(MapArchive(files, {}, kTid)) == V{"code.ips", "exheader.bin", "romfs/actor/x.zar"});
        REQUIRE(Paths(MapArchive(files, {{"luma/titles/{tid}/", ""}}, kTid)) ==
                V{"code.ips", "exheader.bin", "romfs/actor/x.zar"});
    }
    SECTION("emulator and 3DS-hardware folders side by side (SSE style)") {
        const std::string base = "Single_3.2.1/USA/";
        const auto files = Files({{base + "Emulator/load/mods/" + tid + "/exefs/code.ips", "PATCH"},
                                  {base + "Emulator/load/mods/" + tid + "/exheader.bin", "h"},
                                  {base + "Emulator/load/mods/" + tid + "/romfs/s.bin", "s"},
                                  {base + "Emulator/load/textures/" + tid + "/tex.png", "t"},
                                  {base + "3DS Hardware/luma/titles/" + tid + "/code.ips", "PATCH-LUMA"}});
        const std::vector<Rule> rules{{"*/*/Emulator/load/mods/{tid}/", ""}};
        REQUIRE(Paths(MapArchive(files, rules, kTid)) == V{"exefs/code.ips", "exheader.bin", "romfs/s.bin"});
    }
    SECTION("exefs + romfs + romfs_ext + exheader") {
        const auto r = MapArchive(Files({{"exefs/code.bin", "c"}, {"exefs/code.ips", "PATCH"}, {"exefs/icon.bin", "i"},
                                         {"exefs/extra.bin", "no"}, {"exheader.bin", "h"},
                                         {"romfs/a/b.bin", "1"}, {"romfs_ext/a/c.bin.ips", "PATCH"},
                                         {"romfs_ext/a/d.bin.stub", ""}, {"stuff/other.txt", "no"}}), {}, kTid);
        REQUIRE(Paths(r) == V{"exefs/code.bin", "exefs/code.ips", "exefs/icon.bin", "exheader.bin", "romfs/a/b.bin",
                              "romfs_ext/a/c.bin.ips", "romfs_ext/a/d.bin.stub"});
        REQUIRE(r.skipped == 2);
    }
    SECTION("rules: versions folder, rename, folder prefix") {
        const std::vector<Rule> rules{{"v100/citra/code.bps", "code.bps"},
                                      {"v100/citra/exheader.bin", "exheader.bin"},
                                      {"extras/", "romfs_ext/"},
                                      {"readme.txt", "exefs/"}};
        const auto r = MapArchive(Files({{"v100/citra/code.bps", "BPS1"}, {"v100/citra/code.ips", "PATCH"},
                                         {"v100/citra/exheader.bin", "h"}, {"v110/citra/code.bps", "other"},
                                         {"extras/m/x.bin.ips", "p"}, {"readme.txt", "r"}}), rules, kTid);
        // readme.txt maps to exefs/readme.txt, which the loader never reads: dropped
        REQUIRE(Paths(r) == V{"code.bps", "exheader.bin", "romfs_ext/m/x.bin.ips"});
    }
    SECTION("nothing escapes the mods folder") {
        REQUIRE(MapPath("../code.ips", {}, kTid).empty());
        REQUIRE(MapPath("a/../../code.ips", {{"a/", ""}}, kTid).empty());
        REQUIRE(MapPath("x/y", {{"x/", "../"}}, kTid).empty());
        REQUIRE_FALSE(IsModPath("../code.ips"));
        REQUIRE_FALSE(IsModPath("/code.ips"));
        REQUIRE_FALSE(IsModPath("romfs"));
        REQUIRE_FALSE(IsModPath("romfs//a"));
        REQUIRE_FALSE(IsModPath("exefs/sub/code.ips"));
        REQUIRE(IsModPath("romfs/a b/c.bin"));
    }
    SECTION("conflict keys") {
        REQUIRE(ConflictKey("code.ips") == ConflictKey("exefs/code.bps"));
        REQUIRE(ConflictKey("code.bps") == ConflictKey("code.ips"));
        REQUIRE(ConflictKey("code.bin") == ConflictKey("exefs/code.bin"));
        REQUIRE(ConflictKey("code.bin") != ConflictKey("code.ips"));
        REQUIRE(ConflictKey("romfs/a/b.bin") == ConflictKey("romfs_ext/a/b.bin.ips"));
        REQUIRE(ConflictKey("romfs/a/b.bin") == ConflictKey("romfs_ext/a/b.bin.stub"));
        REQUIRE(ConflictKey("romfs/a/b.bin") != ConflictKey("romfs/a/c.bin"));
        REQUIRE(ConflictKey("exheader.bin") == "exheader.bin");
    }
}

TEST_CASE("Mods: install, manifest, conflicts, replace, remove", "[mods]") {
    const std::string dir = TempDir("lib");
    const std::string mods = dir + "mods/" + kTidText + "/";
    const std::string lib = dir + "mod-library/" + kTidText + "/";
    FakeNet net;
    auto library = MakeLibrary(dir, net);

    const std::string reframed_zip = MakeZip({{"README.txt", "r"},
                                              {"North-America/load/mods/0004000000033500/code.ips", "PATCH-REF"},
                                              {"North-America/load/mods/0004000000033500/exheader.bin", "hdr"},
                                              {"North-America/load/mods/0004000000033500/romfs/menu/a.ctxb", "ctxb"},
                                              {"Europe/load/mods/0004000000033600/code.ips", "PATCH-EU"}});
    net.Ok("https://api.github.com/repos/a/reframed/releases/latest",
           ReleaseJson("v2.5.1", "Reframed-2.5.1-Mods.zip", "https://github.com/a/reframed/releases/download/v2.5.1/Reframed-2.5.1-Mods.zip"));
    net.Redirect("https://github.com/a/reframed/releases/download/v2.5.1/Reframed-2.5.1-Mods.zip",
                 "https://release-assets.githubusercontent.com/abc/Reframed.zip?sig=1");
    net.Ok("https://release-assets.githubusercontent.com/abc/Reframed.zip?sig=1", reframed_zip);
    net.Ok("https://raw.githubusercontent.com/simplysanatori/3ds-game-fixes/main/games/g/patch/0004000000033500/code.ips",
           "PATCH-OWNER");

    SECTION("release install: exact files, manifest, redirect to GitHub's asset host") {
        const auto out = library.Install(kTid, "reframed");
        REQUIRE(out.result == InstallResult::Ok);
        REQUIRE(Tree(mods) == V{"code.ips", "exheader.bin", "romfs/menu/a.ctxb"});
        REQUIRE(Read(mods + "code.ips") == "PATCH-REF");
        REQUIRE(Read(mods + "romfs/menu/a.ctxb") == "ctxb");
        REQUIRE(Tree(lib) == V{"reframed/files/code.ips", "reframed/files/exheader.bin",
                               "reframed/files/romfs/menu/a.ctxb", "reframed/manifest.json"});
        const std::string manifest = Read(lib + "reframed/manifest.json");
        for (const char* line : {"\"id\": \"reframed\"", "\"name\": \"Reframed\"", "\"version\": \"v2.5.1\"",
                                 "\"source\": \"github.com/a/reframed\"", "\"verified\": false",
                                 "\"active\": true", "\"code.ips\"", "\"exheader.bin\"",
                                 "\"romfs/menu/a.ctxb\""}) {
            INFO(line);
            REQUIRE(manifest.find(line) != std::string::npos);
        }
        // manifest round trip through ListLoaded
        const auto loaded = library.ListLoaded(kTid);
        REQUIRE(loaded.size() == 1);
        REQUIRE(loaded[0].id == "reframed");
        REQUIRE(loaded[0].version == "v2.5.1");
        REQUIRE(loaded[0].managed);
        REQUIRE(loaded[0].state == ModState::Active);
        REQUIRE(loaded[0].files == V{"code.ips", "exheader.bin", "romfs/menu/a.ctxb"});
        // a second install does nothing, no new requests
        const auto requests = net.log.size();
        REQUIRE(library.Install(kTid, "reframed").result == InstallResult::AlreadyInstalled);
        REQUIRE(net.log.size() == requests);
        // the European title gets its own files
        REQUIRE(library.Install(0x0004000000033600ULL, "reframed").result == InstallResult::Ok);
        REQUIRE(Tree(dir + "mods/0004000000033600/") == V{"code.ips"});
        REQUIRE(Read(dir + "mods/0004000000033600/code.ips") == "PATCH-EU");
        // Remove: gone from the mods folder and the library
        REQUIRE(library.Remove(kTid, "reframed").ok);
        REQUIRE(Tree(mods).empty());
        REQUIRE(Tree(lib).empty());
        REQUIRE(library.ListLoaded(kTid).empty());
        REQUIRE(Read(dir + "mods/0004000000033600/code.ips") == "PATCH-EU"); // the other title untouched
        REQUIRE_FALSE(library.Remove(kTid, "reframed").ok);
    }
    SECTION("redirect to a host that is not GitHub's is refused") {
        net.Redirect("https://github.com/a/reframed/releases/download/v2.5.1/Reframed-2.5.1-Mods.zip",
                     "https://evil.example.com/Reframed.zip");
        const auto out = library.Install(kTid, "reframed");
        REQUIRE(out.result == InstallResult::Error);
        REQUIRE(Tree(mods).empty());
        for (const auto& r : net.log) {
            REQUIRE(r.find("evil.example.com") == std::string::npos);
        }
        REQUIRE(library.GetState().phase == State::Phase::Failed);
        // plain http is refused as well
        net.Redirect("https://github.com/a/reframed/releases/download/v2.5.1/Reframed-2.5.1-Mods.zip",
                     "http://github.com/x.zip");
        REQUIRE(library.Install(kTid, "reframed").result == InstallResult::Error);
    }
    SECTION("the owner's fix: one file, verified") {
        REQUIRE(library.Install(kTid, "owner-fix").result == InstallResult::Ok);
        REQUIRE(Tree(mods) == V{"code.ips"});
        const auto loaded = library.ListLoaded(kTid);
        REQUIRE(loaded.size() == 1);
        REQUIRE(loaded[0].verified);
        REQUIRE(loaded[0].name == "No Bloom");
        REQUIRE(loaded[0].source == "github.com/simplysanatori/3ds-game-fixes");
        const auto available = library.ListAvailable(kTid);
        REQUIRE(available.size() == 4); // owner-fix, reframed, hdhud, seven
        REQUIRE(available[0].id == "owner-fix"); // the maintainer's first
        REQUIRE(available[0].active);
        REQUIRE(available[0].verified);
    }
    SECTION("a file that is not an IPS patch is refused") {
        net.Ok("https://raw.githubusercontent.com/simplysanatori/3ds-game-fixes/main/games/g/patch/0004000000033500/code.ips", "<html>");
        REQUIRE(library.Install(kTid, "owner-fix").result == InstallResult::Error);
        REQUIRE(Tree(mods).empty());
        REQUIRE(Tree(lib).empty());
    }
    SECTION("conflict, replace, keep in library, reactivate, remove") {
        REQUIRE(library.Install(kTid, "owner-fix").result == InstallResult::Ok);
        // reframed also has a code.ips: Install says so and changes nothing in the mods folder
        auto out = library.Install(kTid, "reframed");
        REQUIRE(out.result == InstallResult::Conflict);
        REQUIRE(out.conflict_id == "owner-fix");
        REQUIRE(out.conflict_name == "No Bloom");
        REQUIRE(out.conflict_path == "code.ips");
        REQUIRE_FALSE(out.conflict_unmanaged);
        REQUIRE(Tree(mods) == V{"code.ips"});
        REQUIRE(Read(mods + "code.ips") == "PATCH-OWNER");
        // reframed was downloaded and is in the library, not active
        auto loaded = library.ListLoaded(kTid);
        REQUIRE(loaded.size() == 2);
        REQUIRE(loaded[0].id == "owner-fix");
        REQUIRE(loaded[1].id == "reframed");
        REQUIRE(loaded[1].state == ModState::Inactive);
        for (const auto& a : library.ListAvailable(kTid)) {
            REQUIRE(a.in_library == (a.id == "reframed"));
        }
        // replace: the owner's fix is swapped out, stays in the library
        const auto requests = net.log.size();
        out = library.Install(kTid, "reframed", true);
        REQUIRE(out.result == InstallResult::Ok);
        REQUIRE(net.log.size() == requests); // no second download
        REQUIRE(Tree(mods) == V{"code.ips", "exheader.bin", "romfs/menu/a.ctxb"});
        REQUIRE(Read(mods + "code.ips") == "PATCH-REF");
        loaded = library.ListLoaded(kTid);
        REQUIRE(loaded.size() == 2);
        REQUIRE(loaded[0].id == "reframed");
        REQUIRE(loaded[0].state == ModState::Active);
        REQUIRE(loaded[1].id == "owner-fix");
        REQUIRE(loaded[1].state == ModState::Inactive);
        REQUIRE(Tree(lib + "owner-fix/") == V{"files/code.ips", "manifest.json"});
        // remove the active one: the mods folder is empty, the replaced one is still in the library
        REQUIRE(library.Remove(kTid, "reframed").ok);
        REQUIRE(Tree(mods).empty());
        REQUIRE(library.ListLoaded(kTid).size() == 1);
        // reactivate from the library without the network
        const auto before = net.log.size();
        REQUIRE(library.Install(kTid, "owner-fix").result == InstallResult::Ok);
        REQUIRE(net.log.size() == before);
        REQUIRE(Read(mods + "code.ips") == "PATCH-OWNER");
        // Deactivate keeps it in the library
        REQUIRE(library.Deactivate(kTid, "owner-fix").ok);
        REQUIRE(Tree(mods).empty());
        REQUIRE(library.ListLoaded(kTid)[0].state == ModState::Inactive);
    }
    SECTION("a romfs file and a romfs_ext patch of it conflict") {
        // hdhud ships romfs_ext/menu/a.ctxb.ips; reframed ships romfs/menu/a.ctxb
        net.Ok("https://api.github.com/repos/b/hud/git/trees/main?recursive=1",
               R"({"truncated":false,"tree":[{"path":"dist/romfs_ext/menu/a.ctxb.ips","type":"blob"},
                  {"path":"dist/readme.txt","type":"blob"},{"path":"src/x.cpp","type":"blob"},{"path":"dist","type":"tree"}]})");
        net.Ok("https://raw.githubusercontent.com/b/hud/main/dist/romfs_ext/menu/a.ctxb.ips", "PATCH-HUD");
        net.Ok("https://raw.githubusercontent.com/b/hud/main/dist/readme.txt", "readme");
        REQUIRE(library.Install(kTid, "reframed").result == InstallResult::Ok);
        const auto out = library.Install(kTid, "hdhud");
        REQUIRE(out.result == InstallResult::Conflict);
        REQUIRE(out.conflict_id == "reframed");
        REQUIRE(out.conflict_path == "romfs/menu/a.ctxb");
        REQUIRE(Tree(mods) == V{"code.ips", "exheader.bin", "romfs/menu/a.ctxb"});
        // alone it installs fine (a folder source, names relative to it)
        REQUIRE(library.Remove(kTid, "reframed").ok);
        REQUIRE(library.Install(kTid, "hdhud").result == InstallResult::Ok);
        REQUIRE(Tree(mods) == V{"romfs_ext/menu/a.ctxb.ips"});
    }
    SECTION("unmanaged files: listed by name, never replaced, removed only on request") {
        Write(mods + "code.ips", "PATCH-MINE");
        Write(mods + "exheader.bin", "mine");
        Write(mods + "romfs/a/b.bin", "x");
        Write(mods + "romfs/a/c.bin", "y");
        Write(mods + "exefs/code.bin", "z");
        Write(mods + "My Folder/notes.txt", "n");
        auto loaded = library.ListLoaded(kTid);
        std::vector<std::string> names;
        for (const auto& m : loaded) {
            REQUIRE_FALSE(m.managed);
            REQUIRE(m.state == ModState::Active);
            REQUIRE(m.id == std::string(kUnmanagedPrefix) + m.name);
            names.push_back(m.name);
        }
        REQUIRE(names == V{"My Folder", "code.ips", "exefs/code.bin", "exheader.bin", "romfs"});
        // an install that needs the same code patch is a conflict with the unmanaged file, replace or not
        for (const bool replace : {false, true}) {
            const auto out = library.Install(kTid, "owner-fix", replace);
            REQUIRE(out.result == InstallResult::Conflict);
            REQUIRE(out.conflict_unmanaged);
            REQUIRE(out.conflict_id == "unmanaged:code.ips");
            REQUIRE(Read(mods + "code.ips") == "PATCH-MINE");
        }
        // remove the others by id: only that item goes
        REQUIRE(library.Remove(kTid, "unmanaged:romfs").ok);
        REQUIRE(Tree(mods) == V{"My Folder/notes.txt", "code.ips", "exefs/code.bin", "exheader.bin"});
        REQUIRE(library.Remove(kTid, "unmanaged:code.ips").ok);
        REQUIRE_FALSE(library.Remove(kTid, "unmanaged:nothing").ok);
        // now the install goes through and leaves the rest alone
        REQUIRE(library.Install(kTid, "owner-fix").result == InstallResult::Ok);
        REQUIRE(Tree(mods) == V{"My Folder/notes.txt", "code.ips", "exefs/code.bin", "exheader.bin"});
        // removing the managed mod leaves the unmanaged files
        REQUIRE(library.Remove(kTid, "owner-fix").ok);
        REQUIRE(Tree(mods) == V{"My Folder/notes.txt", "exefs/code.bin", "exheader.bin"});
        REQUIRE(Read(mods + "exheader.bin") == "mine");
    }
    SECTION("an earlier version's fix (code.ips + marker) is listed as ours and upgraded in place") {
        Write(mods + "code.ips", "PATCH-OLD");
        Write(mods + "code.ips.developer", "installed by the Developer source\n");
        auto loaded = library.ListLoaded(kTid);
        REQUIRE(loaded.size() == 1);
        REQUIRE(loaded[0].id == kLegacyFixId);
        REQUIRE(loaded[0].name == "No Bloom");
        REQUIRE(loaded[0].verified);
        REQUIRE(library.Install(kTid, "owner-fix").result == InstallResult::Ok);
        REQUIRE(Tree(mods) == V{"code.ips"});
        REQUIRE(Read(mods + "code.ips") == "PATCH-OWNER");
        REQUIRE(library.ListLoaded(kTid)[0].id == "owner-fix");
        // another mod over the legacy fix needs replace
        REQUIRE(library.Remove(kTid, "owner-fix").ok);
        Write(mods + "code.ips", "PATCH-OLD");
        Write(mods + "code.ips.developer", "x");
        REQUIRE(library.Install(kTid, "reframed").result == InstallResult::Conflict);
        REQUIRE(library.Install(kTid, "reframed", true).result == InstallResult::Ok);
        REQUIRE_FALSE(fs::exists(mods + "code.ips.developer"));
        REQUIRE(Read(mods + "code.ips") == "PATCH-REF");
        // removing the legacy fix directly
        REQUIRE(library.Remove(kTid, "reframed").ok);
        Write(mods + "code.ips", "PATCH-OLD");
        Write(mods + "code.ips.developer", "x");
        REQUIRE(library.Remove(kTid, kLegacyFixId).ok);
        REQUIRE(Tree(mods).empty());
    }
    SECTION("7z, wrong title, unknown id") {
        const auto out = library.Install(kTid, "seven");
        REQUIRE(out.result == InstallResult::NotAvailable);
        REQUIRE(net.log.empty());
        REQUIRE(library.Install(0x0004000000111111ULL, "reframed").result == InstallResult::NotAvailable);
        REQUIRE(library.Install(kTid, "nope").result == InstallResult::NotAvailable);
        // an asset that turns out to be 7z, or a damaged zip
        net.Ok("https://release-assets.githubusercontent.com/abc/Reframed.zip?sig=1", std::string("7z\xBC\xAF\x27\x1C\0\x04", 8));
        auto o = library.Install(kTid, "reframed");
        REQUIRE(o.result == InstallResult::Error);
        REQUIRE(o.error.find("7z") != std::string::npos);
        net.Ok("https://release-assets.githubusercontent.com/abc/Reframed.zip?sig=1", MakeZip({{"a", "b"}}, true));
        REQUIRE(library.Install(kTid, "reframed").result == InstallResult::Error);
        // an archive with nothing that fits
        net.Ok("https://release-assets.githubusercontent.com/abc/Reframed.zip?sig=1", MakeZip({{"readme.txt", "b"}}));
        o = library.Install(kTid, "reframed");
        REQUIRE(o.result == InstallResult::Error);
        REQUIRE(Tree(mods).empty());
        REQUIRE(Tree(lib).empty());
    }
    SECTION("a damaged manifest is ignored, never followed outside the mods folder") {
        Write(lib + "evil/manifest.json",
              R"({"id":"evil","name":"Evil","active":true,"files":["../../outside.txt"]})");
        Write(dir + "outside.txt", "keep");
        REQUIRE(library.ListLoaded(kTid).empty());
        REQUIRE_FALSE(library.Remove(kTid, "evil").ok);
        REQUIRE(Read(dir + "outside.txt") == "keep");
    }
}

TEST_CASE("Mods: catalog refresh", "[mods]") {
    FakeNet net;
    const std::string dir = TempDir("refresh");
    auto library = MakeLibrary(dir, net);
    REQUIRE(library.GetCatalog().mods.size() == 4);
    // not reachable: keeps the catalog
    std::string error;
    REQUIRE_FALSE(library.RefreshCatalog(&error));
    REQUIRE(library.GetCatalog().mods.size() == 4);
    // garbage: keeps the catalog
    net.Ok("https://raw.githubusercontent.com/simplysanatori/3ds-game-fixes/main/catalog/mods.json", "<html>");
    REQUIRE_FALSE(library.RefreshCatalog(&error));
    REQUIRE(library.GetCatalog().mods.size() == 4);
    // good
    net.Ok("https://raw.githubusercontent.com/simplysanatori/3ds-game-fixes/main/catalog/mods.json",
           DefaultCatalogJson());
    REQUIRE(library.RefreshCatalog(&error));
    REQUIRE(library.GetCatalog().Find("stella-glow-usa-no-bloom") != nullptr);
    const auto requests = net.log.size();
    library.RefreshCatalogOnce(); // once per run: one request, then nothing
    library.RefreshCatalogOnce();
    REQUIRE(net.log.size() == requests + 1);
}

// The real thing: the owner's "No Bloom" through the catalog, GitHub and a temporary load folder.
TEST_CASE("Mods: real install of No Bloom", "[.network]") {
    const std::string dir = TempDir("real");
    Library library(dir); // the compiled-in catalog, the real transport
    std::string error;
    // catalog/mods.json from the repository when it is there, else the compiled-in copy
    INFO("refresh: " << (library.RefreshCatalog(&error) ? "repository" : error));
    constexpr u64 tid = 0x0004000000173700ULL;
    const std::string mods = dir + "mods/0004000000173700/";
    const auto available = library.ListAvailable(tid);
    REQUIRE(available.size() == 1);
    REQUIRE(available[0].name == "No Bloom");
    REQUIRE(available[0].verified);
    const auto out = library.Install(tid, available[0].id);
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    REQUIRE(Tree(mods) == V{"code.ips"});
    const std::string ips = Read(mods + "code.ips");
    REQUIRE(ips.size() > 8);
    REQUIRE(ips.substr(0, 5) == "PATCH");
    const auto loaded = library.ListLoaded(tid);
    REQUIRE(loaded.size() == 1);
    REQUIRE(loaded[0].verified);
    REQUIRE(library.Install(tid, available[0].id).result == InstallResult::AlreadyInstalled);
    REQUIRE(library.Remove(tid, available[0].id).ok);
    REQUIRE(Tree(dir).empty());
}

// Real release zip (2.9 MB, deflate) of Ocarina ReFramed through the compiled-in catalog.
TEST_CASE("Mods: real install of Ocarina ReFramed", "[.network]") {
    const std::string dir = TempDir("real_oot");
    Library library(dir);
    const std::string mods = dir + "mods/" + kTidText + "/";
    const auto out = library.Install(kTid, "ocarina-reframed-single-screen");
    INFO(out.error);
    REQUIRE(out.result == InstallResult::Ok);
    const auto tree = Tree(mods);
    REQUIRE(std::find(tree.begin(), tree.end(), "code.ips") != tree.end());
    REQUIRE(std::find(tree.begin(), tree.end(), "exheader.bin") != tree.end());
    REQUIRE(std::find(tree.begin(), tree.end(), "romfs/menu/01_US_ENGLISH/hud_all00.ctxb") != tree.end());
    for (const auto& f : tree) { // nothing from other regions or the licence files
        REQUIRE((f == "code.ips" || f == "exheader.bin" || f.rfind("romfs/", 0) == 0));
    }
    REQUIRE(Read(mods + "code.ips").substr(0, 5) == "PATCH");
    REQUIRE(library.Remove(kTid, "ocarina-reframed-single-screen").ok);
    REQUIRE(Tree(dir).empty());
}
