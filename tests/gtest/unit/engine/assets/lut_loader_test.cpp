/**
 * @file lut_loader_test.cpp
 * @brief LutLoader identity LUT 生成 + .cube 文件解析测试
 */

#include <gtest/gtest.h>
#include "engine/assets/lut_loader.h"
#include <filesystem>
#include <fstream>

class LutLoaderTest : public ::testing::Test {};

TEST_F(LutLoaderTest, IdentityLutDefaultSize) {
    auto lut = dse::assets::GenerateIdentityLut();
    EXPECT_EQ(lut.size, 32);
    EXPECT_EQ(lut.rgba8.size(), static_cast<size_t>(32 * 32 * 32 * 4));
}

TEST_F(LutLoaderTest, IdentityLutCustomSize) {
    auto lut = dse::assets::GenerateIdentityLut(4);
    EXPECT_EQ(lut.size, 4);
    EXPECT_EQ(lut.rgba8.size(), static_cast<size_t>(4 * 4 * 4 * 4));
}

TEST_F(LutLoaderTest, IdentityLutCornerValues) {
    auto lut = dse::assets::GenerateIdentityLut(4);

    // (r=0,g=0,b=0) → 黑色 (0,0,0,255)
    int idx_000 = (0 * 4 * 4 + 0 * 4 + 0) * 4;
    EXPECT_EQ(lut.rgba8[idx_000 + 0], 0);
    EXPECT_EQ(lut.rgba8[idx_000 + 1], 0);
    EXPECT_EQ(lut.rgba8[idx_000 + 2], 0);
    EXPECT_EQ(lut.rgba8[idx_000 + 3], 255);

    // (r=3,g=3,b=3) → 白色 (255,255,255,255)
    int idx_333 = (3 * 4 * 4 + 3 * 4 + 3) * 4;
    EXPECT_EQ(lut.rgba8[idx_333 + 0], 255);
    EXPECT_EQ(lut.rgba8[idx_333 + 1], 255);
    EXPECT_EQ(lut.rgba8[idx_333 + 2], 255);
    EXPECT_EQ(lut.rgba8[idx_333 + 3], 255);
}

TEST_F(LutLoaderTest, IdentityLutAlphaAlways255) {
    auto lut = dse::assets::GenerateIdentityLut(8);
    for (int i = 0; i < 8 * 8 * 8; ++i) {
        EXPECT_EQ(lut.rgba8[i * 4 + 3], 255) << "Alpha at index " << i;
    }
}

TEST_F(LutLoaderTest, LoadCubeLutFromFile) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "dse_lut_test.cube";

    // 生成 2x2x2 identity .cube 文件
    {
        std::ofstream out(tmp.string());
        out << "# Test LUT\n";
        out << "TITLE \"Test\"\n";
        out << "LUT_3D_SIZE 2\n";
        out << "DOMAIN_MIN 0.0 0.0 0.0\n";
        out << "DOMAIN_MAX 1.0 1.0 1.0\n";
        // 2^3 = 8 entries: iterate B(outer) G(mid) R(inner) in .cube format
        for (int b = 0; b < 2; ++b)
            for (int g = 0; g < 2; ++g)
                for (int r = 0; r < 2; ++r)
                    out << static_cast<float>(r) << " "
                        << static_cast<float>(g) << " "
                        << static_cast<float>(b) << "\n";
    }

    dse::assets::LutData data;
    ASSERT_TRUE(dse::assets::LoadCubeLut(tmp.string(), data));
    EXPECT_EQ(data.size, 2);
    EXPECT_EQ(data.rgba8.size(), static_cast<size_t>(2 * 2 * 2 * 4));

    fs::remove(tmp);
}

TEST_F(LutLoaderTest, LoadCubeLutInvalidPath) {
    dse::assets::LutData data;
    EXPECT_FALSE(dse::assets::LoadCubeLut("nonexistent.cube", data));
}

TEST_F(LutLoaderTest, LoadCubeLutMalformed) {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "dse_lut_bad.cube";
    {
        std::ofstream out(tmp.string());
        out << "LUT_3D_SIZE 2\n";
        out << "0.0 0.0 0.0\n";
        // 缺少 7 个条目
    }

    dse::assets::LutData data;
    EXPECT_FALSE(dse::assets::LoadCubeLut(tmp.string(), data));

    fs::remove(tmp);
}
