/**
 * @file ktx2_parser.cpp
 * @brief KTX2 texture container parser implementation.
 */

#include "engine/assets/ktx2_parser.h"
#include "engine/assets/dtex.h"

#include <cstring>
#include <algorithm>
#include <iostream>

namespace dse::assets {

namespace {

// Vulkan format → CompressedTextureFormat mapping (subset relevant to DSE).
// See https://registry.khronos.org/vulkan/specs/1.3/html/vkspec.html#formats-definition
struct VkFormatMapping {
    uint32_t vk_format;
    CompressedTextureFormat dse_format;
    bool is_srgb;
};

static const VkFormatMapping kVkFormatTable[] = {
    // BC1
    {131, CompressedTextureFormat::BC1_UNORM, false},  // VK_FORMAT_BC1_RGB_UNORM_BLOCK
    {132, CompressedTextureFormat::BC1_SRGB, true},    // VK_FORMAT_BC1_RGB_SRGB_BLOCK
    {133, CompressedTextureFormat::BC1_UNORM, false},  // VK_FORMAT_BC1_RGBA_UNORM_BLOCK
    {134, CompressedTextureFormat::BC1_SRGB, true},    // VK_FORMAT_BC1_RGBA_SRGB_BLOCK
    // BC2
    {135, CompressedTextureFormat::BC2_UNORM, false},  // VK_FORMAT_BC2_UNORM_BLOCK
    {136, CompressedTextureFormat::BC2_UNORM, true},   // VK_FORMAT_BC2_SRGB_BLOCK
    // BC3
    {137, CompressedTextureFormat::BC3_UNORM, false},  // VK_FORMAT_BC3_UNORM_BLOCK
    {138, CompressedTextureFormat::BC3_SRGB, true},    // VK_FORMAT_BC3_SRGB_BLOCK
    // BC4
    {139, CompressedTextureFormat::BC4_UNORM, false},  // VK_FORMAT_BC4_UNORM_BLOCK
    // BC5
    {141, CompressedTextureFormat::BC5_UNORM, false},  // VK_FORMAT_BC5_UNORM_BLOCK
    // BC7
    {145, CompressedTextureFormat::BC7_UNORM, false},  // VK_FORMAT_BC7_UNORM_BLOCK
    {146, CompressedTextureFormat::BC7_SRGB, true},    // VK_FORMAT_BC7_SRGB_BLOCK
    // ASTC 4x4
    {157, CompressedTextureFormat::ASTC_4x4_UNORM, false},  // VK_FORMAT_ASTC_4x4_UNORM_BLOCK
    {158, CompressedTextureFormat::ASTC_4x4_SRGB, true},    // VK_FORMAT_ASTC_4x4_SRGB_BLOCK
    // ASTC 6x6
    {163, CompressedTextureFormat::ASTC_6x6_UNORM, false},  // VK_FORMAT_ASTC_6x6_UNORM_BLOCK
    {164, CompressedTextureFormat::ASTC_6x6_SRGB, true},    // VK_FORMAT_ASTC_6x6_SRGB_BLOCK
    // ASTC 8x8
    {169, CompressedTextureFormat::ASTC_8x8_UNORM, false},  // VK_FORMAT_ASTC_8x8_UNORM_BLOCK
    {170, CompressedTextureFormat::ASTC_8x8_SRGB, true},    // VK_FORMAT_ASTC_8x8_SRGB_BLOCK
};

bool LookupVkFormat(uint32_t vk_format, CompressedTextureFormat& out_format, bool& out_srgb) {
    for (const auto& entry : kVkFormatTable) {
        if (entry.vk_format == vk_format) {
            out_format = entry.dse_format;
            out_srgb = entry.is_srgb;
            return true;
        }
    }
    return false;
}

} // namespace

bool IsKtx2File(const uint8_t* data, size_t size) {
    if (size < 12) return false;
    return std::memcmp(data, kKtx2Identifier, 12) == 0;
}

bool HasKtx2Extension(const std::string& path) {
    if (path.size() < 5) return false;
    std::string ext = path.substr(path.size() - 4);
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".ktx") return true;
    if (path.size() >= 5) {
        ext = path.substr(path.size() - 5);
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".ktx2") return true;
    }
    return false;
}

Ktx2ParseResult ParseKtx2(const std::vector<uint8_t>& file_data) {
    Ktx2ParseResult result;

    constexpr size_t kIdentifierSize = 12;
    constexpr size_t kHeaderSize = sizeof(Ktx2Header);
    constexpr size_t kMinFileSize = kIdentifierSize + kHeaderSize;

    if (file_data.size() < kMinFileSize) {
        result.error = "File too small for KTX2";
        return result;
    }

    if (!IsKtx2File(file_data.data(), file_data.size())) {
        result.error = "Not a KTX2 file (bad identifier)";
        return result;
    }

    Ktx2Header header;
    std::memcpy(&header, file_data.data() + kIdentifierSize, kHeaderSize);

    // Validate basic fields
    if (header.pixel_width == 0 || header.pixel_height == 0) {
        result.error = "Invalid KTX2 dimensions";
        return result;
    }

    if (header.level_count == 0) header.level_count = 1;

    auto sc = static_cast<Ktx2Supercompression>(header.supercompression_scheme);
    if (sc == Ktx2Supercompression::BasisLZ) {
        result.error = "Basis Universal (ETC1S) supercompression not supported — use basisu CLI to transcode first";
        return result;
    }

    // Map Vulkan format to DSE format
    if (!LookupVkFormat(header.vk_format, result.format, result.is_srgb)) {
        // Check if it's a UASTC format (vk_format == 0 with BasisLZ/Zstd supercompression)
        if (header.vk_format == 0) {
            result.error = "Basis Universal (UASTC) format requires transcoding — use basisu CLI to transcode first";
            return result;
        }
        result.error = "Unsupported VkFormat: " + std::to_string(header.vk_format);
        return result;
    }

    result.width = header.pixel_width;
    result.height = header.pixel_height;
    result.level_count = header.level_count;

    // Read level index table (immediately after header)
    size_t level_index_offset = kIdentifierSize + kHeaderSize;
    size_t level_index_size = header.level_count * sizeof(Ktx2LevelIndex);

    if (file_data.size() < level_index_offset + level_index_size) {
        result.error = "File truncated at level index";
        return result;
    }

    std::vector<Ktx2LevelIndex> levels(header.level_count);
    std::memcpy(levels.data(), file_data.data() + level_index_offset, level_index_size);

    // Extract level data
    result.level_data.resize(header.level_count);
    for (uint32_t i = 0; i < header.level_count; ++i) {
        const auto& lvl = levels[i];
        if (lvl.byte_offset + lvl.byte_length > file_data.size()) {
            result.error = "Level " + std::to_string(i) + " data out of bounds";
            return result;
        }

        if (sc == Ktx2Supercompression::None || sc == Ktx2Supercompression::ZLIB) {
            // No supercompression or unsupported ZLIB: copy raw data
            // (ZLIB decompression would need zlib; for now treat as raw if not compressed)
            size_t data_size = static_cast<size_t>(lvl.byte_length);
            result.level_data[i].resize(data_size);
            std::memcpy(result.level_data[i].data(),
                        file_data.data() + lvl.byte_offset, data_size);
        } else if (sc == Ktx2Supercompression::Zstandard) {
            // Zstandard decompression would need zstd library.
            // For now, store raw and note that decompression is needed.
            size_t data_size = static_cast<size_t>(lvl.byte_length);
            result.level_data[i].resize(data_size);
            std::memcpy(result.level_data[i].data(),
                        file_data.data() + lvl.byte_offset, data_size);
            // If uncompressed_byte_length differs, this data needs zstd decompression.
            if (lvl.uncompressed_byte_length != lvl.byte_length && lvl.uncompressed_byte_length > 0) {
                result.error = "Zstandard supercompression detected — zstd library required for decompression";
                return result;
            }
        }
    }

    result.success = true;
    return result;
}

bool ConvertKtx2ToDtex(const Ktx2ParseResult& ktx2, std::vector<uint8_t>& out_dtex) {
    if (!ktx2.success || ktx2.level_data.empty()) return false;

    // Build DtexHeader
    DtexHeader header;
    header.format = static_cast<uint32_t>(ktx2.format);
    header.width = ktx2.width;
    header.height = ktx2.height;
    header.mip_count = static_cast<uint32_t>(ktx2.level_data.size());
    header.flags = ktx2.is_srgb ? 1u : 0u;

    // Build mip descriptors
    std::vector<DtexMipDesc> mip_descs(header.mip_count);
    uint32_t data_start = static_cast<uint32_t>(sizeof(DtexHeader) + header.mip_count * sizeof(DtexMipDesc));
    uint32_t current_offset = data_start;

    int bw = DtexBlockWidth(ktx2.format);
    int bh = DtexBlockHeight(ktx2.format);

    for (uint32_t i = 0; i < header.mip_count; ++i) {
        uint32_t mip_w = std::max(1u, ktx2.width >> i);
        uint32_t mip_h = std::max(1u, ktx2.height >> i);
        mip_descs[i].width = mip_w;
        mip_descs[i].height = mip_h;
        mip_descs[i].offset = current_offset;
        mip_descs[i].size = static_cast<uint32_t>(ktx2.level_data[i].size());
        current_offset += mip_descs[i].size;
    }

    // Assemble output
    out_dtex.resize(current_offset);
    std::memcpy(out_dtex.data(), &header, sizeof(DtexHeader));
    std::memcpy(out_dtex.data() + sizeof(DtexHeader), mip_descs.data(),
                header.mip_count * sizeof(DtexMipDesc));
    for (uint32_t i = 0; i < header.mip_count; ++i) {
        std::memcpy(out_dtex.data() + mip_descs[i].offset,
                    ktx2.level_data[i].data(), ktx2.level_data[i].size());
    }

    return true;
}

} // namespace dse::assets
