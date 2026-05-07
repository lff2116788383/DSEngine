#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dse::pak {

/// .dpak file format:
///
///   [DpakHeader]          (32 bytes)
///   [DpakTocEntry] * N    (N * 80 bytes)
///   [file data blocks]    (concatenated raw data)
///
/// All multi-byte integers are little-endian.

constexpr uint32_t kDpakMagic   = 0x4B415044; // "DPAK"
constexpr uint32_t kDpakVersion = 1;

#pragma pack(push, 1)

struct DpakHeader {
    uint32_t magic        = kDpakMagic;
    uint32_t version      = kDpakVersion;
    uint32_t entry_count  = 0;       // number of TOC entries
    uint32_t flags        = 0;       // reserved
    uint64_t toc_offset   = 0;       // byte offset to first TOC entry
    uint64_t data_offset  = 0;       // byte offset to first data block
};

static_assert(sizeof(DpakHeader) == 32, "DpakHeader must be 32 bytes");

struct DpakTocEntry {
    char     path[64]     = {};      // relative path (null-terminated)
    uint64_t data_offset  = 0;       // offset from file start
    uint64_t data_size    = 0;       // uncompressed size in bytes
};

static_assert(sizeof(DpakTocEntry) == 80, "DpakTocEntry must be 80 bytes");

#pragma pack(pop)

/// In-memory TOC entry with std::string path (used by reader)
struct PakEntry {
    std::string path;
    uint64_t    data_offset = 0;
    uint64_t    data_size   = 0;
};

} // namespace dse::pak
