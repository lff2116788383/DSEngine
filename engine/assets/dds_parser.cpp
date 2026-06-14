#include "engine/assets/dds_parser.h"

#include <algorithm>

namespace dse::assets {
namespace {

struct DdsPixelFormat {
    uint32_t size, flags, four_cc, rgb_bit_count;
    uint32_t r_mask, g_mask, b_mask, a_mask;
};

struct DdsHeader {
    uint32_t size, flags, height, width;
    uint32_t pitch_or_linear_size, depth, mip_map_count;
    uint32_t reserved1[11];
    DdsPixelFormat pixel_format;
    uint32_t caps, caps2, caps3, caps4, reserved2;
};

struct DdsHeaderDX10 {
    uint32_t dxgi_format, resource_dimension, misc_flag, array_size, misc_flags2;
};

constexpr uint32_t MakeFourCC(char a, char b, char c, char d) {
    return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
           (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

} // namespace

bool ParseDds(const std::vector<uint8_t>& file_data,
              CompressedTextureFormat& out_format,
              std::vector<CompressedMipLevel>& out_mips,
              int& out_width, int& out_height) {
    if (file_data.size() < 128) return false;
    if (file_data[0] != 'D' || file_data[1] != 'D' || file_data[2] != 'S' || file_data[3] != ' ') return false;

    const auto* header = reinterpret_cast<const DdsHeader*>(file_data.data() + 4);
    out_width = static_cast<int>(header->width);
    out_height = static_cast<int>(header->height);
    uint32_t mip_count = (header->mip_map_count > 0) ? header->mip_map_count : 1;

    size_t data_offset = 4 + sizeof(DdsHeader);
    uint32_t block_bytes = 16;
    bool format_found = false;

    uint32_t fcc = header->pixel_format.four_cc;
    if (fcc == MakeFourCC('D','X','1','0')) {
        if (file_data.size() < data_offset + sizeof(DdsHeaderDX10)) return false;
        const auto* dx10 = reinterpret_cast<const DdsHeaderDX10*>(file_data.data() + data_offset);
        data_offset += sizeof(DdsHeaderDX10);
        switch (dx10->dxgi_format) {
            case 71: out_format = CompressedTextureFormat::BC1_UNORM; block_bytes = 8; format_found = true; break;
            case 72: out_format = CompressedTextureFormat::BC1_SRGB;  block_bytes = 8; format_found = true; break;
            case 74: out_format = CompressedTextureFormat::BC2_UNORM; format_found = true; break;
            case 77: out_format = CompressedTextureFormat::BC3_UNORM; format_found = true; break;
            case 78: out_format = CompressedTextureFormat::BC3_SRGB;  format_found = true; break;
            case 80: out_format = CompressedTextureFormat::BC4_UNORM; block_bytes = 8; format_found = true; break;
            case 83: out_format = CompressedTextureFormat::BC5_UNORM; format_found = true; break;
            case 98: out_format = CompressedTextureFormat::BC7_UNORM; format_found = true; break;
            case 99: out_format = CompressedTextureFormat::BC7_SRGB;  format_found = true; break;
            default: break;
        }
    } else {
        if      (fcc == MakeFourCC('D','X','T','1')) { out_format = CompressedTextureFormat::BC1_UNORM; block_bytes = 8; format_found = true; }
        else if (fcc == MakeFourCC('D','X','T','3')) { out_format = CompressedTextureFormat::BC2_UNORM; format_found = true; }
        else if (fcc == MakeFourCC('D','X','T','5')) { out_format = CompressedTextureFormat::BC3_UNORM; format_found = true; }
        else if (fcc == MakeFourCC('A','T','I','1') || fcc == MakeFourCC('B','C','4','U')) { out_format = CompressedTextureFormat::BC4_UNORM; block_bytes = 8; format_found = true; }
        else if (fcc == MakeFourCC('A','T','I','2') || fcc == MakeFourCC('B','C','5','U')) { out_format = CompressedTextureFormat::BC5_UNORM; format_found = true; }
    }

    if (!format_found) return false;

    out_mips.clear();
    int w = out_width, h = out_height;
    size_t offset = data_offset;
    for (uint32_t i = 0; i < mip_count; ++i) {
        uint32_t blocks_w = std::max<uint32_t>(1u, (static_cast<uint32_t>(w) + 3u) / 4u);
        uint32_t blocks_h = std::max<uint32_t>(1u, (static_cast<uint32_t>(h) + 3u) / 4u);
        size_t mip_size = static_cast<size_t>(blocks_w) * blocks_h * block_bytes;
        if (offset + mip_size > file_data.size()) break;
        out_mips.push_back({file_data.data() + offset, mip_size, w, h});
        offset += mip_size;
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }

    return !out_mips.empty();
}

bool HasDdsExtension(const std::string& path) {
    if (path.size() < 4) return false;
    auto ext = path.substr(path.size() - 4);
    return ext == ".dds" || ext == ".DDS";
}

} // namespace dse::assets
