#include "engine/assets/dtex.h"

#include <algorithm>
#include <cstring>

namespace dse::assets {

uint32_t DtexBlockBytes(CompressedTextureFormat format) {
    switch (format) {
        case CompressedTextureFormat::BC1_UNORM:
        case CompressedTextureFormat::BC1_SRGB:
        case CompressedTextureFormat::BC4_UNORM:
            return 8u;
        default:
            return 16u;  // BC2/BC3/BC5/BC7
    }
}

bool ParseDtex(const std::vector<uint8_t>& file_data,
               CompressedTextureFormat& out_format,
               std::vector<CompressedMipLevel>& out_mips,
               int& out_width, int& out_height) {
    out_mips.clear();
    if (file_data.size() < sizeof(DtexHeader)) return false;

    DtexHeader header;
    std::memcpy(&header, file_data.data(), sizeof(DtexHeader));
    if (header.magic != kDtexMagic) return false;
    if (header.version != kDtexVersion) return false;
    if (header.mip_count == 0) return false;
    if (header.format > static_cast<uint32_t>(CompressedTextureFormat::BC7_SRGB)) return false;

    out_format = static_cast<CompressedTextureFormat>(header.format);
    out_width = static_cast<int>(header.width);
    out_height = static_cast<int>(header.height);

    const size_t desc_table_bytes = static_cast<size_t>(header.mip_count) * sizeof(DtexMipDesc);
    if (file_data.size() < sizeof(DtexHeader) + desc_table_bytes) return false;

    const size_t desc_base = sizeof(DtexHeader);
    for (uint32_t i = 0; i < header.mip_count; ++i) {
        DtexMipDesc desc;
        std::memcpy(&desc, file_data.data() + desc_base + i * sizeof(DtexMipDesc), sizeof(DtexMipDesc));
        const size_t end = static_cast<size_t>(desc.offset) + desc.size;
        if (desc.size == 0 || end > file_data.size() || desc.offset < sizeof(DtexHeader) + desc_table_bytes) {
            out_mips.clear();
            return false;
        }
        out_mips.push_back({file_data.data() + desc.offset, desc.size,
                            static_cast<int>(desc.width), static_cast<int>(desc.height)});
    }

    return !out_mips.empty();
}

bool HasDtexExtension(const std::string& path) {
    if (path.size() < 5) return false;
    std::string ext = path.substr(path.size() - 5);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".dtex";
}

} // namespace dse::assets
