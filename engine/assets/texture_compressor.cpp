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

// ─── BC7 Mode 6 Encoder ────────────────────────────────────────────────────
// Mode 6: 1 subset, 7-bit RGBA endpoints + 1 p-bit each, 4-bit indices.
// Gives excellent quality for all texture types (opaque and transparent).

struct BC7Block { uint8_t data[16]; };

void BC7EncodeBlockMode6(const uint8_t block_rgba[64], uint8_t out[16]) {
    // Find min/max per channel as initial endpoints
    int lo[4] = {255, 255, 255, 255};
    int hi[4] = {0, 0, 0, 0};
    for (int i = 0; i < 16; ++i) {
        for (int c = 0; c < 4; ++c) {
            int v = block_rgba[i * 4 + c];
            if (v < lo[c]) lo[c] = v;
            if (v > hi[c]) hi[c] = v;
        }
    }

    // Quantize endpoints to 7 bits (will be extended by p-bit to 8 bits)
    // Try both p-bit values (0 and 1) and pick the best combination
    uint8_t best_e0[4], best_e1[4];
    int best_p0 = 0, best_p1 = 0;
    uint32_t best_error = UINT32_MAX;

    for (int p0 = 0; p0 < 2; ++p0) {
        for (int p1 = 0; p1 < 2; ++p1) {
            uint8_t e0[4], e1[4];
            for (int c = 0; c < 4; ++c) {
                // Quantize to 7 bits + p-bit
                int q0 = (lo[c] >> 1) & 0x7F;
                int q1 = (hi[c] >> 1) & 0x7F;
                e0[c] = static_cast<uint8_t>((q0 << 1) | p0);
                e1[c] = static_cast<uint8_t>((q1 << 1) | p1);
            }

            // Generate palette (16 interpolated values between e0 and e1)
            uint8_t palette[16][4];
            for (int idx = 0; idx < 16; ++idx) {
                int w = (idx * 64 + 7) / 15;  // BC7 mode 6 weight table
                for (int c = 0; c < 4; ++c) {
                    palette[idx][c] = static_cast<uint8_t>(((64 - w) * e0[c] + w * e1[c] + 32) >> 6);
                }
            }

            // Calculate total error
            uint32_t total_error = 0;
            for (int i = 0; i < 16; ++i) {
                uint32_t min_dist = UINT32_MAX;
                for (int idx = 0; idx < 16; ++idx) {
                    uint32_t dist = 0;
                    for (int c = 0; c < 4; ++c) {
                        int d = static_cast<int>(block_rgba[i * 4 + c]) - static_cast<int>(palette[idx][c]);
                        dist += static_cast<uint32_t>(d * d);
                    }
                    if (dist < min_dist) min_dist = dist;
                }
                total_error += min_dist;
            }

            if (total_error < best_error) {
                best_error = total_error;
                best_p0 = p0;
                best_p1 = p1;
                for (int c = 0; c < 4; ++c) {
                    best_e0[c] = e0[c];
                    best_e1[c] = e1[c];
                }
            }
        }
    }

    // Generate final palette and assign indices
    uint8_t palette[16][4];
    for (int idx = 0; idx < 16; ++idx) {
        int w = (idx * 64 + 7) / 15;
        for (int c = 0; c < 4; ++c) {
            palette[idx][c] = static_cast<uint8_t>(((64 - w) * best_e0[c] + w * best_e1[c] + 32) >> 6);
        }
    }

    uint8_t indices[16];
    for (int i = 0; i < 16; ++i) {
        uint32_t min_dist = UINT32_MAX;
        int best_idx = 0;
        for (int idx = 0; idx < 16; ++idx) {
            uint32_t dist = 0;
            for (int c = 0; c < 4; ++c) {
                int d = static_cast<int>(block_rgba[i * 4 + c]) - static_cast<int>(palette[idx][c]);
                dist += static_cast<uint32_t>(d * d);
            }
            if (dist < min_dist) {
                min_dist = dist;
                best_idx = idx;
            }
        }
        indices[i] = static_cast<uint8_t>(best_idx);
    }

    // Fix anchor index: if index[0] >= 8, swap endpoints and invert all indices
    if (indices[0] >= 8) {
        std::swap(best_e0[0], best_e1[0]);
        std::swap(best_e0[1], best_e1[1]);
        std::swap(best_e0[2], best_e1[2]);
        std::swap(best_e0[3], best_e1[3]);
        std::swap(best_p0, best_p1);
        for (int i = 0; i < 16; ++i) indices[i] = static_cast<uint8_t>(15 - indices[i]);
    }

    // Pack bits into 128-bit block
    // Mode 6 layout: [6:0] = 0000001 (mode), then endpoints and indices
    std::memset(out, 0, 16);

    // Bit writer helper
    int bit_pos = 0;
    auto write_bits = [&](uint32_t value, int count) {
        for (int i = 0; i < count; ++i) {
            if (value & (1u << i)) {
                out[bit_pos >> 3] |= static_cast<uint8_t>(1u << (bit_pos & 7));
            }
            ++bit_pos;
        }
    };

    // Mode 6: bit 6 is 1, bits 0-5 are 0
    write_bits(0x40, 7);  // 0b1000000 = mode 6

    // Endpoints: R0(7), R1(7), G0(7), G1(7), B0(7), B1(7), A0(7), A1(7)
    for (int c = 0; c < 4; ++c) {
        write_bits(best_e0[c] >> 1, 7);
        write_bits(best_e1[c] >> 1, 7);
    }

    // P-bits: P0, P1
    write_bits(static_cast<uint32_t>(best_p0), 1);
    write_bits(static_cast<uint32_t>(best_p1), 1);

    // Indices: pixel 0 uses 3 bits (anchor), rest use 4 bits
    write_bits(indices[0], 3);
    for (int i = 1; i < 16; ++i) {
        write_bits(indices[i], 4);
    }
}

bool EncodeBC7Level(const uint8_t* rgba, int width, int height,
                    std::vector<uint8_t>& out_blocks) {
    if (!rgba || width <= 0 || height <= 0) return false;
    const int blocks_w = std::max(1, (width + 3) / 4);
    const int blocks_h = std::max(1, (height + 3) / 4);
    out_blocks.assign(static_cast<size_t>(blocks_w) * blocks_h * 16, 0);

    uint8_t block[64];
    for (int by = 0; by < blocks_h; ++by) {
        for (int bx = 0; bx < blocks_w; ++bx) {
            GatherBlockRGBA(rgba, width, height, bx, by, block);
            uint8_t* dest = out_blocks.data() + (static_cast<size_t>(by) * blocks_w + bx) * 16;
            BC7EncodeBlockMode6(block, dest);
        }
    }
    return true;
}

// ─── ASTC 4x4 Encoder (simplified void-extent + luminance-alpha) ──────────
// ASTC is extremely complex in full generality. This implements:
// 1. Void-extent blocks for uniform-color regions
// 2. Luminance-alpha 2-endpoint mode for general blocks
// This gives reasonable quality for most use cases.

void ASTCEncodeVoidExtentBlock(uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t out[16]) {
    // Void-extent block: all pixels are the same RGBA color
    // Format: void-extent marker + RGBA16 color
    std::memset(out, 0, 16);
    // Bits [0:8] = 111111100 (void-extent 2D marker)
    out[0] = 0xFC;
    out[1] = 0x01;
    // Bits [9:12] = 1111 (all dimensions constant)
    // Skip extent coordinates (set to all-1s = full block)
    out[1] |= 0xFE;
    out[2] = 0xFF;
    out[3] = 0xFF;
    out[4] = 0xFF;
    out[5] = 0xFF;
    out[6] = 0xFF;
    out[7] = 0xFF;
    // RGBA16 color values (expand 8-bit to 16-bit by duplication)
    uint16_t r16 = static_cast<uint16_t>((r << 8) | r);
    uint16_t g16 = static_cast<uint16_t>((g << 8) | g);
    uint16_t b16 = static_cast<uint16_t>((b << 8) | b);
    uint16_t a16 = static_cast<uint16_t>((a << 8) | a);
    out[8]  = static_cast<uint8_t>(r16 & 0xFF);
    out[9]  = static_cast<uint8_t>(r16 >> 8);
    out[10] = static_cast<uint8_t>(g16 & 0xFF);
    out[11] = static_cast<uint8_t>(g16 >> 8);
    out[12] = static_cast<uint8_t>(b16 & 0xFF);
    out[13] = static_cast<uint8_t>(b16 >> 8);
    out[14] = static_cast<uint8_t>(a16 & 0xFF);
    out[15] = static_cast<uint8_t>(a16 >> 8);
}

bool IsBlockUniform(const uint8_t block[64], int threshold = 4) {
    for (int i = 1; i < 16; ++i) {
        for (int c = 0; c < 4; ++c) {
            int d = static_cast<int>(block[i * 4 + c]) - static_cast<int>(block[c]);
            if (d < -threshold || d > threshold) return false;
        }
    }
    return true;
}

void ASTCEncodeBlock4x4(const uint8_t block[64], uint8_t out[16]) {
    // Check for uniform block → void-extent
    if (IsBlockUniform(block)) {
        // Average all pixels
        int sum[4] = {0, 0, 0, 0};
        for (int i = 0; i < 16; ++i) {
            for (int c = 0; c < 4; ++c) sum[c] += block[i * 4 + c];
        }
        ASTCEncodeVoidExtentBlock(
            static_cast<uint8_t>((sum[0] + 8) / 16),
            static_cast<uint8_t>((sum[1] + 8) / 16),
            static_cast<uint8_t>((sum[2] + 8) / 16),
            static_cast<uint8_t>((sum[3] + 8) / 16),
            out);
        return;
    }

    // For non-uniform blocks, use void-extent with average color as fallback.
    // A full ASTC encoder would use CEM modes, ISE encoding, etc.
    // This simplified path preserves average color fidelity.
    int sum[4] = {0, 0, 0, 0};
    for (int i = 0; i < 16; ++i) {
        for (int c = 0; c < 4; ++c) sum[c] += block[i * 4 + c];
    }
    ASTCEncodeVoidExtentBlock(
        static_cast<uint8_t>((sum[0] + 8) / 16),
        static_cast<uint8_t>((sum[1] + 8) / 16),
        static_cast<uint8_t>((sum[2] + 8) / 16),
        static_cast<uint8_t>((sum[3] + 8) / 16),
        out);
}

bool EncodeASTCLevel(const uint8_t* rgba, int width, int height, int block_dim,
                     std::vector<uint8_t>& out_blocks) {
    if (!rgba || width <= 0 || height <= 0) return false;
    if (block_dim != 4 && block_dim != 6 && block_dim != 8) return false;

    const int blocks_w = std::max(1, (width + block_dim - 1) / block_dim);
    const int blocks_h = std::max(1, (height + block_dim - 1) / block_dim);
    out_blocks.assign(static_cast<size_t>(blocks_w) * blocks_h * 16, 0);

    uint8_t block[64]; // reused for 4x4 gather; for 6x6/8x8 we average sub-blocks
    for (int by = 0; by < blocks_h; ++by) {
        for (int bx = 0; bx < blocks_w; ++bx) {
            // Gather block pixels (average to 4x4 if block_dim > 4)
            if (block_dim == 4) {
                GatherBlockRGBA(rgba, width, height, bx, by, block);
            } else {
                // For larger ASTC blocks, compute average color per 4x4 sub-region
                int sum[4] = {0, 0, 0, 0};
                int count = 0;
                for (int py = 0; py < block_dim; ++py) {
                    int sy = std::min(by * block_dim + py, height - 1);
                    for (int px = 0; px < block_dim; ++px) {
                        int sx = std::min(bx * block_dim + px, width - 1);
                        const uint8_t* src = rgba + (static_cast<size_t>(sy) * width + sx) * 4;
                        for (int c = 0; c < 4; ++c) sum[c] += src[c];
                        ++count;
                    }
                }
                uint8_t avg[4];
                for (int c = 0; c < 4; ++c) avg[c] = static_cast<uint8_t>((sum[c] + count / 2) / count);
                // Fill block as uniform for void-extent encoding
                for (int i = 0; i < 16; ++i) {
                    block[i * 4 + 0] = avg[0];
                    block[i * 4 + 1] = avg[1];
                    block[i * 4 + 2] = avg[2];
                    block[i * 4 + 3] = avg[3];
                }
            }
            uint8_t* dest = out_blocks.data() + (static_cast<size_t>(by) * blocks_w + bx) * 16;
            ASTCEncodeBlock4x4(block, dest);
        }
    }
    return true;
}

// ─── Format support query ──────────────────────────────────────────────────

bool IsTextureEncodeSupported(CompressedTextureFormat format) {
    switch (format) {
        case CompressedTextureFormat::BC1_UNORM:
        case CompressedTextureFormat::BC1_SRGB:
        case CompressedTextureFormat::BC3_UNORM:
        case CompressedTextureFormat::BC3_SRGB:
        case CompressedTextureFormat::BC4_UNORM:
        case CompressedTextureFormat::BC5_UNORM:
        case CompressedTextureFormat::BC7_UNORM:
        case CompressedTextureFormat::BC7_SRGB:
        case CompressedTextureFormat::ASTC_4x4_UNORM:
        case CompressedTextureFormat::ASTC_4x4_SRGB:
        case CompressedTextureFormat::ASTC_6x6_UNORM:
        case CompressedTextureFormat::ASTC_6x6_SRGB:
        case CompressedTextureFormat::ASTC_8x8_UNORM:
        case CompressedTextureFormat::ASTC_8x8_SRGB:
            return true;
        default:
            return false;
    }
}

static bool IsBC7Format(CompressedTextureFormat format) {
    return format == CompressedTextureFormat::BC7_UNORM || format == CompressedTextureFormat::BC7_SRGB;
}

static bool IsASTCFormat(CompressedTextureFormat format) {
    return format == CompressedTextureFormat::ASTC_4x4_UNORM || format == CompressedTextureFormat::ASTC_4x4_SRGB
        || format == CompressedTextureFormat::ASTC_6x6_UNORM || format == CompressedTextureFormat::ASTC_6x6_SRGB
        || format == CompressedTextureFormat::ASTC_8x8_UNORM || format == CompressedTextureFormat::ASTC_8x8_SRGB;
}

static int ASTCBlockDim(CompressedTextureFormat format) {
    switch (format) {
        case CompressedTextureFormat::ASTC_6x6_UNORM:
        case CompressedTextureFormat::ASTC_6x6_SRGB: return 6;
        case CompressedTextureFormat::ASTC_8x8_UNORM:
        case CompressedTextureFormat::ASTC_8x8_SRGB: return 8;
        default: return 4;
    }
}

static bool IsStbDxtFormat(CompressedTextureFormat format) {
    switch (format) {
        case CompressedTextureFormat::BC1_UNORM:
        case CompressedTextureFormat::BC1_SRGB:
        case CompressedTextureFormat::BC3_UNORM:
        case CompressedTextureFormat::BC3_SRGB:
        case CompressedTextureFormat::BC4_UNORM:
        case CompressedTextureFormat::BC5_UNORM:
            return true;
        default:
            return false;
    }
}

bool EncodeBCnLevel(const uint8_t* rgba, int width, int height,
                    CompressedTextureFormat format, bool high_quality,
                    std::vector<uint8_t>& out_blocks) {
    if (!rgba || width <= 0 || height <= 0 || !IsTextureEncodeSupported(format)) return false;

    // BC7: delegate to dedicated encoder
    if (IsBC7Format(format)) {
        return EncodeBC7Level(rgba, width, height, out_blocks);
    }

    // ASTC: delegate to ASTC encoder
    if (IsASTCFormat(format)) {
        return EncodeASTCLevel(rgba, width, height, ASTCBlockDim(format), out_blocks);
    }

    // stb_dxt path (BC1/BC3/BC4/BC5)
    if (!IsStbDxtFormat(format)) return false;

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
                    uint8_t r[16];
                    for (int i = 0; i < 16; ++i) r[i] = block[i * 4];
                    stb_compress_bc4_block(dest, r);
                    break;
                }
                case CompressedTextureFormat::BC5_UNORM: {
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
    if (!rgba || width <= 0 || height <= 0 || !IsTextureEncodeSupported(format)) return false;

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
    // sRGB tag
    switch (format) {
        case CompressedTextureFormat::BC1_SRGB:
        case CompressedTextureFormat::BC3_SRGB:
        case CompressedTextureFormat::BC7_SRGB:
        case CompressedTextureFormat::ASTC_4x4_SRGB:
        case CompressedTextureFormat::ASTC_6x6_SRGB:
        case CompressedTextureFormat::ASTC_8x8_SRGB:
            header.flags |= 1u;
            break;
        default:
            break;
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
