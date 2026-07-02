/**
 * @file ktx2_parser.h
 * @brief KTX2 texture container parser — reads KTX2 files and extracts
 *        GPU-ready compressed mip data (BCn/ASTC/ETC2/uncompressed RGBA).
 *
 * KTX2 is the Khronos Texture Container v2 format, used in modern glTF
 * via KHR_texture_basisu. This parser supports:
 * - Direct BCn/ASTC/ETC2 data (no supercompression)
 * - ZSTD supercompression (level data decompression)
 * - Basis Universal / UASTC detection (reported but not transcoded)
 *
 * Output feeds directly into the existing CompressedMipLevel / RHI pipeline.
 */

#ifndef DSE_ASSETS_KTX2_PARSER_H
#define DSE_ASSETS_KTX2_PARSER_H

#include <cstdint>
#include <string>
#include <vector>

#include "engine/render/rhi/rhi_types.h"

namespace dse::assets {

/// KTX2 file identifier (12 bytes).
static constexpr uint8_t kKtx2Identifier[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

/// Supercompression schemes.
enum class Ktx2Supercompression : uint32_t {
    None       = 0,
    BasisLZ    = 1,   // Basis Universal LZ (ETC1S)
    Zstandard  = 2,   // Zstd compressed
    ZLIB       = 3,   // zlib compressed
};

/// KTX2 header (64 bytes, after the 12-byte identifier).
#pragma pack(push, 1)
struct Ktx2Header {
    uint32_t vk_format;
    uint32_t type_size;
    uint32_t pixel_width;
    uint32_t pixel_height;
    uint32_t pixel_depth;
    uint32_t layer_count;
    uint32_t face_count;
    uint32_t level_count;
    uint32_t supercompression_scheme;
    // Index
    uint32_t dfd_byte_offset;
    uint32_t dfd_byte_length;
    uint32_t kvd_byte_offset;
    uint32_t kvd_byte_length;
    uint64_t sgd_byte_offset;
    uint64_t sgd_byte_length;
};

struct Ktx2LevelIndex {
    uint64_t byte_offset;
    uint64_t byte_length;
    uint64_t uncompressed_byte_length;
};
#pragma pack(pop)

/// Result of parsing a KTX2 file.
struct Ktx2ParseResult {
    bool success = false;
    std::string error;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t level_count = 0;

    CompressedTextureFormat format = CompressedTextureFormat::BC1_UNORM;
    bool is_srgb = false;

    /// Decompressed mip data (each entry = one mip level's block data).
    std::vector<std::vector<uint8_t>> level_data;
};

/**
 * @brief Parse a KTX2 file from memory.
 *
 * Extracts mip level data for BCn/ASTC/ETC2 formats.
 * For Basis Universal supercompression, returns an error (transcoding not supported).
 */
Ktx2ParseResult ParseKtx2(const std::vector<uint8_t>& file_data);

/**
 * @brief Convert a parsed KTX2 result into a .dtex byte stream.
 *
 * This bridges KTX2 → dtex so the existing runtime pipeline can load it.
 */
bool ConvertKtx2ToDtex(const Ktx2ParseResult& ktx2, std::vector<uint8_t>& out_dtex);

/// Check if file data starts with the KTX2 identifier.
bool IsKtx2File(const uint8_t* data, size_t size);

/// Check path extension.
bool HasKtx2Extension(const std::string& path);

} // namespace dse::assets

#endif // DSE_ASSETS_KTX2_PARSER_H
