// 纹理 BCn 编码器 + .dtex 容器单元测试。
//
// 验证导入期压缩链路的 CPU 侧核心：BCn 块字节数、mip 链生成、.dtex 容器
// 编码→解析 round-trip（格式/尺寸/mip 视图范围一致），以及 BC1 对纯色块的
// 端点近似正确性（确认编码确实落在目标颜色附近，而非全黑/垃圾数据）。

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "engine/assets/dtex.h"
#include "engine/assets/texture_compressor.h"

namespace {

std::vector<uint8_t> SolidRgba(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < px.size(); i += 4) {
        px[i + 0] = r; px[i + 1] = g; px[i + 2] = b; px[i + 3] = a;
    }
    return px;
}

} // namespace

TEST(TextureCompressor, BlockBytesPerFormat) {
    EXPECT_EQ(dse::assets::DtexBlockBytes(CompressedTextureFormat::BC1_UNORM), 8u);
    EXPECT_EQ(dse::assets::DtexBlockBytes(CompressedTextureFormat::BC1_SRGB), 8u);
    EXPECT_EQ(dse::assets::DtexBlockBytes(CompressedTextureFormat::BC4_UNORM), 8u);
    EXPECT_EQ(dse::assets::DtexBlockBytes(CompressedTextureFormat::BC3_UNORM), 16u);
    EXPECT_EQ(dse::assets::DtexBlockBytes(CompressedTextureFormat::BC5_UNORM), 16u);
    EXPECT_EQ(dse::assets::DtexBlockBytes(CompressedTextureFormat::BC7_UNORM), 16u);
}

TEST(TextureCompressor, SupportMatrix) {
    EXPECT_TRUE(dse::assets::IsBCnEncodeSupported(CompressedTextureFormat::BC1_UNORM));
    EXPECT_TRUE(dse::assets::IsBCnEncodeSupported(CompressedTextureFormat::BC3_SRGB));
    EXPECT_TRUE(dse::assets::IsBCnEncodeSupported(CompressedTextureFormat::BC4_UNORM));
    EXPECT_TRUE(dse::assets::IsBCnEncodeSupported(CompressedTextureFormat::BC5_UNORM));
    // BC2 / BC7 不在 stb_dxt MVP 范围。
    EXPECT_FALSE(dse::assets::IsBCnEncodeSupported(CompressedTextureFormat::BC2_UNORM));
    EXPECT_FALSE(dse::assets::IsBCnEncodeSupported(CompressedTextureFormat::BC7_UNORM));
}

TEST(TextureCompressor, EncodeLevelBlockCount) {
    // 8x8 -> 2x2 blocks. BC1 = 8 bytes/block -> 32 bytes; BC3 = 16 -> 64.
    auto px = SolidRgba(8, 8, 200, 50, 25, 255);
    std::vector<uint8_t> bc1, bc3;
    ASSERT_TRUE(dse::assets::EncodeBCnLevel(px.data(), 8, 8, CompressedTextureFormat::BC1_UNORM, false, bc1));
    ASSERT_TRUE(dse::assets::EncodeBCnLevel(px.data(), 8, 8, CompressedTextureFormat::BC3_UNORM, false, bc3));
    EXPECT_EQ(bc1.size(), 2u * 2u * 8u);
    EXPECT_EQ(bc3.size(), 2u * 2u * 16u);
}

TEST(TextureCompressor, EncodeLevelNonMultipleOf4PadsBlocks) {
    // 5x3 -> ceil(5/4)=2, ceil(3/4)=1 -> 2x1 blocks.
    auto px = SolidRgba(5, 3, 10, 20, 30, 255);
    std::vector<uint8_t> bc5;
    ASSERT_TRUE(dse::assets::EncodeBCnLevel(px.data(), 5, 3, CompressedTextureFormat::BC5_UNORM, false, bc5));
    EXPECT_EQ(bc5.size(), 2u * 1u * 16u);
}

TEST(TextureCompressor, EncodeRejectsUnsupportedAndBadInput) {
    auto px = SolidRgba(4, 4, 0, 0, 0, 255);
    std::vector<uint8_t> out;
    EXPECT_FALSE(dse::assets::EncodeBCnLevel(px.data(), 4, 4, CompressedTextureFormat::BC7_UNORM, false, out));
    EXPECT_FALSE(dse::assets::EncodeBCnLevel(nullptr, 4, 4, CompressedTextureFormat::BC1_UNORM, false, out));
    EXPECT_FALSE(dse::assets::EncodeBCnLevel(px.data(), 0, 4, CompressedTextureFormat::BC1_UNORM, false, out));
    std::vector<uint8_t> dtex;
    EXPECT_FALSE(dse::assets::EncodeTextureToDtex(px.data(), 4, 4, CompressedTextureFormat::BC2_UNORM, true, false, dtex));
}

TEST(TextureCompressor, DtexRoundTripSingleMip) {
    auto px = SolidRgba(16, 16, 120, 180, 60, 255);
    std::vector<uint8_t> dtex;
    ASSERT_TRUE(dse::assets::EncodeTextureToDtex(px.data(), 16, 16, CompressedTextureFormat::BC3_UNORM,
                                                 /*generate_mips=*/false, /*hq=*/false, dtex));

    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;
    ASSERT_TRUE(dse::assets::ParseDtex(dtex, fmt, mips, w, h));
    EXPECT_EQ(fmt, CompressedTextureFormat::BC3_UNORM);
    EXPECT_EQ(w, 16);
    EXPECT_EQ(h, 16);
    ASSERT_EQ(mips.size(), 1u);
    EXPECT_EQ(mips[0].width, 16);
    EXPECT_EQ(mips[0].height, 16);
    // 16x16 -> 4x4 blocks * 16 bytes = 256 bytes.
    EXPECT_EQ(mips[0].size, 4u * 4u * 16u);
}

TEST(TextureCompressor, DtexRoundTripFullMipChain) {
    auto px = SolidRgba(16, 16, 200, 30, 30, 255);
    std::vector<uint8_t> dtex;
    ASSERT_TRUE(dse::assets::EncodeTextureToDtex(px.data(), 16, 16, CompressedTextureFormat::BC1_UNORM,
                                                 /*generate_mips=*/true, /*hq=*/false, dtex));

    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;
    ASSERT_TRUE(dse::assets::ParseDtex(dtex, fmt, mips, w, h));
    EXPECT_EQ(fmt, CompressedTextureFormat::BC1_UNORM);
    // 16 -> 8 -> 4 -> 2 -> 1 : 5 levels.
    ASSERT_EQ(mips.size(), 5u);
    EXPECT_EQ(mips[0].width, 16);
    EXPECT_EQ(mips[4].width, 1);
    EXPECT_EQ(mips[4].height, 1);
    // 每个 mip 视图范围都应落在文件内。
    for (const auto& m : mips) {
        const auto* base = dtex.data();
        EXPECT_GE(m.data, base);
        EXPECT_LE(m.data + m.size, base + dtex.size());
        EXPECT_GT(m.size, 0u);
    }
}

TEST(TextureCompressor, Bc1SolidColorEndpointNearTarget) {
    // 纯红：BC1 块的 color0(565) 应解出 R 高、G/B 低，证明编码非垃圾数据。
    auto px = SolidRgba(4, 4, 255, 0, 0, 255);
    std::vector<uint8_t> blocks;
    ASSERT_TRUE(dse::assets::EncodeBCnLevel(px.data(), 4, 4, CompressedTextureFormat::BC1_UNORM, true, blocks));
    ASSERT_EQ(blocks.size(), 8u);
    uint16_t color0 = static_cast<uint16_t>(blocks[0] | (blocks[1] << 8));
    int r5 = (color0 >> 11) & 0x1F;
    int g6 = (color0 >> 5) & 0x3F;
    int b5 = color0 & 0x1F;
    int r8 = (r5 * 255) / 31;
    int g8 = (g6 * 255) / 63;
    int b8 = (b5 * 255) / 31;
    EXPECT_GT(r8, 200);
    EXPECT_LT(g8, 40);
    EXPECT_LT(b8, 40);
}

TEST(TextureCompressor, ParseRejectsCorruptData) {
    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;

    std::vector<uint8_t> too_short(4, 0);
    EXPECT_FALSE(dse::assets::ParseDtex(too_short, fmt, mips, w, h));

    // 合法编码后篡改魔数。
    auto px = SolidRgba(8, 8, 1, 2, 3, 255);
    std::vector<uint8_t> dtex;
    ASSERT_TRUE(dse::assets::EncodeTextureToDtex(px.data(), 8, 8, CompressedTextureFormat::BC1_UNORM, false, false, dtex));
    dtex[0] = 0xFF;
    EXPECT_FALSE(dse::assets::ParseDtex(dtex, fmt, mips, w, h));
}

TEST(TextureCompressor, ExtensionDetection) {
    EXPECT_TRUE(dse::assets::HasDtexExtension("a/b/c.dtex"));
    EXPECT_TRUE(dse::assets::HasDtexExtension("UPPER.DTEX"));
    EXPECT_FALSE(dse::assets::HasDtexExtension("x.dds"));
    EXPECT_FALSE(dse::assets::HasDtexExtension("noext"));
}
