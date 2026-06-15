// DDS / BCn 解析器单元测试。
//
// 验证 .dds 直传链路的 CPU 侧核心：FourCC + DX10 头识别、压缩格式映射、
// mip 链尺寸与块字节数计算（这正是「不解压、直传 GPU」省显存的前提）。

#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "engine/assets/dds_parser.h"

namespace {

constexpr uint32_t kDdsHeaderSize = 124;   // DDS_HEADER（不含 4 字节魔数）
constexpr uint32_t kMipCountOffset = 24;   // dwMipMapCount 在 header 内偏移
constexpr uint32_t kDdsPfOffset = 72;      // ddspf(DDS_PIXELFORMAT) 在 header 内偏移
constexpr uint32_t kFourCcOffset = kDdsPfOffset + 8; // dwFourCC 字段偏移 = 80

void WriteU32(std::vector<uint8_t>& buf, size_t at, uint32_t v) {
    std::memcpy(buf.data() + at, &v, sizeof(v));
}

// 构造一个最小合法 DDS：4 字节魔数 + 124 字节 header（+ 可选 DX10 头）+ 像素数据。
std::vector<uint8_t> MakeDds(uint32_t width, uint32_t height, uint32_t mip_count,
                             uint32_t four_cc, uint32_t dx10_dxgi_format,
                             size_t pixel_bytes) {
    const bool has_dx10 = (four_cc == (('D') | ('X' << 8) | ('1' << 16) | ('0' << 24)));
    size_t header_total = 4 + kDdsHeaderSize + (has_dx10 ? 20u : 0u);
    std::vector<uint8_t> buf(header_total + pixel_bytes, 0);

    buf[0] = 'D'; buf[1] = 'D'; buf[2] = 'S'; buf[3] = ' ';
    WriteU32(buf, 4 + 0, kDdsHeaderSize);             // size
    WriteU32(buf, 4 + 8, height);                     // height
    WriteU32(buf, 4 + 12, width);                     // width
    WriteU32(buf, 4 + kMipCountOffset, mip_count);    // mip_map_count
    WriteU32(buf, 4 + kFourCcOffset, four_cc);        // pixel_format.four_cc

    if (has_dx10) {
        WriteU32(buf, 4 + kDdsHeaderSize + 0, dx10_dxgi_format); // dxgi_format
    }
    return buf;
}

uint32_t FourCC(char a, char b, char c, char d) {
    return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) |
           (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24);
}

} // namespace

TEST(DdsParser, RejectsNonDds) {
    std::vector<uint8_t> junk(200, 0xAB);
    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;
    EXPECT_FALSE(dse::assets::ParseDds(junk, fmt, mips, w, h));
}

TEST(DdsParser, RejectsTooSmall) {
    std::vector<uint8_t> tiny(64, 0);
    tiny[0] = 'D'; tiny[1] = 'D'; tiny[2] = 'S'; tiny[3] = ' ';
    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;
    EXPECT_FALSE(dse::assets::ParseDds(tiny, fmt, mips, w, h));
}

TEST(DdsParser, ParsesDxt1SingleMip) {
    // 8x8 BC1：blocks 2x2, block_bytes 8 -> 4 blocks * 8 = 32 bytes
    auto buf = MakeDds(8, 8, 1, FourCC('D','X','T','1'), 0, 32);
    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;
    ASSERT_TRUE(dse::assets::ParseDds(buf, fmt, mips, w, h));
    EXPECT_EQ(fmt, CompressedTextureFormat::BC1_UNORM);
    EXPECT_EQ(w, 8);
    EXPECT_EQ(h, 8);
    ASSERT_EQ(mips.size(), 1u);
    EXPECT_EQ(mips[0].width, 8);
    EXPECT_EQ(mips[0].height, 8);
    EXPECT_EQ(mips[0].size, 32u);
}

TEST(DdsParser, ParsesDxt5MipChain) {
    // 8x8 BC3 (block_bytes 16) 三级 mip：8x8(4blk*16=64) + 4x4(1blk*16=16) + 2x2(1*16=16)
    auto buf = MakeDds(8, 8, 3, FourCC('D','X','T','5'), 0, 64 + 16 + 16);
    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;
    ASSERT_TRUE(dse::assets::ParseDds(buf, fmt, mips, w, h));
    EXPECT_EQ(fmt, CompressedTextureFormat::BC3_UNORM);
    ASSERT_EQ(mips.size(), 3u);
    EXPECT_EQ(mips[0].size, 64u);
    EXPECT_EQ(mips[1].width, 4);
    EXPECT_EQ(mips[1].size, 16u);
    EXPECT_EQ(mips[2].width, 2);
    EXPECT_EQ(mips[2].size, 16u);
}

TEST(DdsParser, ParsesDx10Bc7) {
    // DX10 头，DXGI_FORMAT_BC7_UNORM = 98。4x4 BC7 (block_bytes 16) -> 1 block * 16
    auto buf = MakeDds(4, 4, 1, FourCC('D','X','1','0'), 98, 16);
    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;
    ASSERT_TRUE(dse::assets::ParseDds(buf, fmt, mips, w, h));
    EXPECT_EQ(fmt, CompressedTextureFormat::BC7_UNORM);
    ASSERT_EQ(mips.size(), 1u);
    EXPECT_EQ(mips[0].size, 16u);
}

TEST(DdsParser, RejectsUnknownFourCC) {
    auto buf = MakeDds(8, 8, 1, FourCC('Q','Q','Q','Q'), 0, 64);
    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;
    EXPECT_FALSE(dse::assets::ParseDds(buf, fmt, mips, w, h));
}

TEST(DdsParser, MipChainTruncatedAtDataBoundary) {
    // 声明 3 级 mip，但只提供顶层数据 -> 解析应只产出能容纳的 mip。
    auto buf = MakeDds(8, 8, 3, FourCC('D','X','T','1'), 0, 32 /*仅顶层 8x8*/);
    CompressedTextureFormat fmt;
    std::vector<CompressedMipLevel> mips;
    int w = 0, h = 0;
    ASSERT_TRUE(dse::assets::ParseDds(buf, fmt, mips, w, h));
    EXPECT_EQ(mips.size(), 1u);
}

TEST(DdsParser, HasDdsExtension) {
    EXPECT_TRUE(dse::assets::HasDdsExtension("foo.dds"));
    EXPECT_TRUE(dse::assets::HasDdsExtension("BAR.DDS"));
    EXPECT_FALSE(dse::assets::HasDdsExtension("foo.png"));
    EXPECT_FALSE(dse::assets::HasDdsExtension("dds"));
}
