#include "engine/assets/texture_compressor.h"

#include <algorithm>
#include <cstring>

#include "engine/assets/dtex.h"

#define STB_DXT_IMPLEMENTATION
#include <stb/stb_dxt.h>

namespace dse::assets {
namespace {

// 从 RGBA8 源中取出以 (bx*4, by*4) 为左上角的 4x4 块，边界按最近像素复制（replicate）。
// 输出 64 字节（16 像素 * RGBA）。
void GatherBlockRGBA(const uint8_t* rgba, int width, int height, int bx, int by,
                     uint8_t out_block[64]) {
    for (int py = 0; py < 4; ++py) {
        int sy = std::min(by * 4 + py, height - 1);
        for (int px = 0; px < 4; ++px) {
            int sx = std::min(bx * 4 + px, width - 1);
            const uint8_t* src = rgba + (static_cast<size_t>(sy) * width + sx) * 4;
            uint8_t* dst = out_block + (py * 4 + px) * 4;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        }
    }
}

// 盒式滤波下采样一级 mip（RGBA8）。尺寸向下取整、最小 1。
void DownsampleBoxRGBA(const std::vector<uint8_t>& src, int sw, int sh,
                       std::vector<uint8_t>& dst, int& dw, int& dh) {
    dw = std::max(1, sw / 2);
    dh = std::max(1, sh / 2);
    dst.resize(static_cast<size_t>(dw) * dh * 4);
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            int x0 = std::min(x * 2, sw - 1);
            int x1 = std::min(x * 2 + 1, sw - 1);
            int y0 = std::min(y * 2, sh - 1);
            int y1 = std::min(y * 2 + 1, sh - 1);
            const uint8_t* p00 = src.data() + (static_cast<size_t>(y0) * sw + x0) * 4;
            const uint8_t* p01 = src.data() + (static_cast<size_t>(y0) * sw + x1) * 4;
            const uint8_t* p10 = src.data() + (static_cast<size_t>(y1) * sw + x0) * 4;
            const uint8_t* p11 = src.data() + (static_cast<size_t>(y1) * sw + x1) * 4;
            uint8_t* d = dst.data() + (static_cast<size_t>(y) * dw + x) * 4;
            for (int c = 0; c < 4; ++c) {
                d[c] = static_cast<uint8_t>((static_cast<int>(p00[c]) + p01[c] + p10[c] + p11[c] + 2) / 4);
            }
        }
    }
}

} // namespace

bool IsBCnEncodeSupported(CompressedTextureFormat format) {
    switch (format) {
        case CompressedTextureFormat::BC1_UNORM:
        case CompressedTextureFormat::BC1_SRGB:
        case CompressedTextureFormat::BC3_UNORM:
        case CompressedTextureFormat::BC3_SRGB:
        case CompressedTextureFormat::BC4_UNORM:
        case CompressedTextureFormat::BC5_UNORM:
            return true;
        default:
            return false;  // BC2 / BC7 由 stb_dxt 之外的编码器负责
    }
}

bool EncodeBCnLevel(const uint8_t* rgba, int width, int height,
                    CompressedTextureFormat format, bool high_quality,
                    std::vector<uint8_t>& out_blocks) {
    if (!rgba || width <= 0 || height <= 0 || !IsBCnEncodeSupported(format)) return false;

    const int blocks_w = std::max(1, (width + 3) / 4);
    const int blocks_h = std::max(1, (height + 3) / 4);
    const uint32_t block_bytes = DtexBlockBytes(format);
    const int mode = high_quality ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;

    out_blocks.assign(static_cast<size_t>(blocks_w) * blocks_h * block_bytes, 0);

    uint8_t block[64];
    for (int by = 0; by < blocks_h; ++by) {
        for (int bx = 0; bx < blocks_w; ++bx) {
            GatherBlockRGBA(rgba, width, height, bx, by, block);
            uint8_t* dest = out_blocks.data() +
                            (static_cast<size_t>(by) * blocks_w + bx) * block_bytes;
            switch (format) {
                case CompressedTextureFormat::BC1_UNORM:
                case CompressedTextureFormat::BC1_SRGB:
                    stb_compress_dxt_block(dest, block, 0, mode);
                    break;
                case CompressedTextureFormat::BC3_UNORM:
                case CompressedTextureFormat::BC3_SRGB:
                    stb_compress_dxt_block(dest, block, 1, mode);
                    break;
                case CompressedTextureFormat::BC4_UNORM: {
                    // 单通道（R）：从 RGBA 块抽出 16 个 R 字节。
                    uint8_t r[16];
                    for (int i = 0; i < 16; ++i) r[i] = block[i * 4];
                    stb_compress_bc4_block(dest, r);
                    break;
                }
                case CompressedTextureFormat::BC5_UNORM: {
                    // 双通道（RG）：交错的 16 个 RG 对。
                    uint8_t rg[32];
                    for (int i = 0; i < 16; ++i) {
                        rg[i * 2 + 0] = block[i * 4 + 0];
                        rg[i * 2 + 1] = block[i * 4 + 1];
                    }
                    stb_compress_bc5_block(dest, rg);
                    break;
                }
                default:
                    return false;
            }
        }
    }
    return true;
}

bool EncodeTextureToDtex(const uint8_t* rgba, int width, int height,
                         CompressedTextureFormat format, bool generate_mips,
                         bool high_quality, std::vector<uint8_t>& out_bytes) {
    out_bytes.clear();
    if (!rgba || width <= 0 || height <= 0 || !IsBCnEncodeSupported(format)) return false;

    // 逐级编码：先把 mip0 复制为可下采样的工作缓冲。
    std::vector<std::vector<uint8_t>> level_blocks;
    std::vector<int> level_w, level_h;

    std::vector<uint8_t> cur(rgba, rgba + static_cast<size_t>(width) * height * 4);
    int cw = width, ch = height;
    while (true) {
        std::vector<uint8_t> blocks;
        if (!EncodeBCnLevel(cur.data(), cw, ch, format, high_quality, blocks)) return false;
        level_blocks.push_back(std::move(blocks));
        level_w.push_back(cw);
        level_h.push_back(ch);
        if (!generate_mips || (cw <= 1 && ch <= 1)) break;
        std::vector<uint8_t> down;
        int dw, dh;
        DownsampleBoxRGBA(cur, cw, ch, down, dw, dh);
        cur = std::move(down);
        cw = dw;
        ch = dh;
    }

    const uint32_t mip_count = static_cast<uint32_t>(level_blocks.size());
    const size_t desc_table_bytes = static_cast<size_t>(mip_count) * sizeof(DtexMipDesc);
    const size_t data_base = sizeof(DtexHeader) + desc_table_bytes;

    DtexHeader header;
    header.format = static_cast<uint32_t>(format);
    header.width = static_cast<uint32_t>(width);
    header.height = static_cast<uint32_t>(height);
    header.mip_count = mip_count;
    if (format == CompressedTextureFormat::BC1_SRGB ||
        format == CompressedTextureFormat::BC3_SRGB) {
        header.flags |= 1u;  // sRGB tag（信息性）
    }

    size_t total = data_base;
    for (auto& b : level_blocks) total += b.size();
    out_bytes.resize(total);

    std::memcpy(out_bytes.data(), &header, sizeof(DtexHeader));

    size_t offset = data_base;
    for (uint32_t i = 0; i < mip_count; ++i) {
        DtexMipDesc desc;
        desc.width = static_cast<uint32_t>(level_w[i]);
        desc.height = static_cast<uint32_t>(level_h[i]);
        desc.offset = static_cast<uint32_t>(offset);
        desc.size = static_cast<uint32_t>(level_blocks[i].size());
        std::memcpy(out_bytes.data() + sizeof(DtexHeader) + static_cast<size_t>(i) * sizeof(DtexMipDesc),
                    &desc, sizeof(DtexMipDesc));
        std::memcpy(out_bytes.data() + offset, level_blocks[i].data(), level_blocks[i].size());
        offset += level_blocks[i].size();
    }

    return true;
}

} // namespace dse::assets
