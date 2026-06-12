/**
 * @file app_manifest_test.cpp
 * @brief AppManifest 读写测试 —— 覆盖 game.dsmanifest / project.dseproj 的
 *        window + splash 段解析、相对图片路径解析、颜色解析与读写往返。
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "engine/runtime/app_manifest.h"

using dse::runtime::AppManifest;
using dse::runtime::LoadAppManifest;
using dse::runtime::WriteAppManifest;

namespace {

// 每个用例独立的临时目录，析构时递归清理。
class TempDir {
public:
    TempDir() {
        const auto base = std::filesystem::temp_directory_path();
        dir_ = base / ("dse_manifest_test_" +
                       std::to_string(reinterpret_cast<uintptr_t>(this)) + "_" +
                       std::to_string(::testing::UnitTest::GetInstance()
                                          ->random_seed()));
        std::filesystem::create_directories(dir_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }
    std::filesystem::path file(const std::string& name) const { return dir_ / name; }
    const std::filesystem::path& path() const { return dir_; }

private:
    std::filesystem::path dir_;
};

void WriteText(const std::filesystem::path& p, const std::string& text) {
    std::ofstream ofs(p, std::ios::trunc | std::ios::binary);
    ofs << text;
}

} // namespace

// 测试 应用清单：缺失文件返回false
TEST(AppManifestTest, MissingFileReturnsFalse) {
    AppManifest m;
    EXPECT_FALSE(LoadAppManifest("definitely/not/here.dsmanifest", m));
    EXPECT_FALSE(m.has_window_title);
    EXPECT_FALSE(m.has_splash);
}

// 测试 应用清单：非法JSON返回false
TEST(AppManifestTest, InvalidJsonReturnsFalse) {
    TempDir tmp;
    const auto p = tmp.file("bad.dsmanifest");
    WriteText(p, "{ this is not valid json ");
    AppManifest m;
    EXPECT_FALSE(LoadAppManifest(p.string(), m));
}

// 测试 应用清单：JSON顶层非对象返回false
TEST(AppManifestTest, NonObjectJsonReturnsFalse) {
    TempDir tmp;
    const auto p = tmp.file("arr.dsmanifest");
    WriteText(p, "[1, 2, 3]");
    AppManifest m;
    EXPECT_FALSE(LoadAppManifest(p.string(), m));
}

// 测试 应用清单：空对象加载成功且全部has标志为false
TEST(AppManifestTest, EmptyObjectLoadsWithNoFlags) {
    TempDir tmp;
    const auto p = tmp.file("empty.dsmanifest");
    WriteText(p, "{}");
    AppManifest m;
    ASSERT_TRUE(LoadAppManifest(p.string(), m));
    EXPECT_FALSE(m.has_window_title);
    EXPECT_FALSE(m.has_window_size);
    EXPECT_FALSE(m.has_splash);
}

// 测试 应用清单：解析window段标题与尺寸
TEST(AppManifestTest, ParsesWindowTitleAndSize) {
    TempDir tmp;
    const auto p = tmp.file("win.dsmanifest");
    WriteText(p, R"({ "window": { "title": "My Game", "width": 1280, "height": 720 } })");
    AppManifest m;
    ASSERT_TRUE(LoadAppManifest(p.string(), m));
    EXPECT_TRUE(m.has_window_title);
    EXPECT_EQ(m.window_title, "My Game");
    EXPECT_TRUE(m.has_window_size);
    EXPECT_EQ(m.window_width, 1280);
    EXPECT_EQ(m.window_height, 720);
}

// 测试 应用清单：仅有宽度时尺寸标志置位
TEST(AppManifestTest, PartialWindowSizeSetsFlag) {
    TempDir tmp;
    const auto p = tmp.file("win_partial.dsmanifest");
    WriteText(p, R"({ "window": { "width": 1024 } })");
    AppManifest m;
    ASSERT_TRUE(LoadAppManifest(p.string(), m));
    EXPECT_FALSE(m.has_window_title);
    EXPECT_TRUE(m.has_window_size);
    EXPECT_EQ(m.window_width, 1024);
    EXPECT_EQ(m.window_height, 0); // 未提供则保持默认
}

// 测试 应用清单：splash相对图片路径解析为相对清单目录的绝对路径
TEST(AppManifestTest, SplashRelativeImageResolvedAgainstManifestDir) {
    TempDir tmp;
    const auto p = tmp.file("game.dsmanifest");
    WriteText(p, R"({ "splash": { "image": "assets/logo.png" } })");
    AppManifest m;
    ASSERT_TRUE(LoadAppManifest(p.string(), m));
    ASSERT_TRUE(m.has_splash);
    const auto expected = (tmp.path() / "assets" / "logo.png");
    EXPECT_EQ(std::filesystem::path(m.splash.image_path), expected);
}

// 测试 应用清单：splash绝对图片路径原样保留
TEST(AppManifestTest, SplashAbsoluteImageKeptAsIs) {
    TempDir tmp;
    const auto abs = (tmp.path() / "brand" / "logo.png");
    const auto p = tmp.file("abs.dsmanifest");
    WriteText(p, std::string(R"({ "splash": { "image": ")") +
                     abs.generic_string() + R"(" } })");
    AppManifest m;
    ASSERT_TRUE(LoadAppManifest(p.string(), m));
    ASSERT_TRUE(m.has_splash);
    EXPECT_EQ(std::filesystem::path(m.splash.image_path), abs);
}

// 测试 应用清单：splash颜色支持数字与字符串两种写法
TEST(AppManifestTest, SplashColorParsingNumericAndString) {
    TempDir tmp;
    const auto p = tmp.file("color.dsmanifest");
    WriteText(p, R"({ "splash": {
        "background_argb": 4280163368,
        "title_argb": "0xFF112233",
        "accent_argb": "#445566"
    } })");
    AppManifest m;
    ASSERT_TRUE(LoadAppManifest(p.string(), m));
    ASSERT_TRUE(m.has_splash);
    EXPECT_EQ(m.splash.bg_argb, 4280163368u);            // 0xFF1E1E28
    EXPECT_EQ(m.splash.title_argb, 0xFF112233u);
    EXPECT_EQ(m.splash.accent_argb, 0xFF445566u);        // "#RRGGBB" 补 0xFF alpha
}

// 测试 应用清单：splash数值字段全部解析
TEST(AppManifestTest, SplashNumericFieldsParsed) {
    TempDir tmp;
    const auto p = tmp.file("splashnum.dsmanifest");
    WriteText(p, R"({ "splash": {
        "enabled": false,
        "app_name": "Brand",
        "initial_status": "Loading...",
        "card_width": 500, "card_height": 320, "logo_size": 96,
        "fade_in_ms": 100, "fade_out_ms": 150, "min_display_ms": 800
    } })");
    AppManifest m;
    ASSERT_TRUE(LoadAppManifest(p.string(), m));
    ASSERT_TRUE(m.has_splash);
    EXPECT_FALSE(m.splash.enabled);
    EXPECT_EQ(m.splash.app_name, "Brand");
    EXPECT_EQ(m.splash.initial_status, "Loading...");
    EXPECT_EQ(m.splash.card_width, 500);
    EXPECT_EQ(m.splash.card_height, 320);
    EXPECT_EQ(m.splash.logo_size, 96);
    EXPECT_EQ(m.splash.fade_in_ms, 100);
    EXPECT_EQ(m.splash.fade_out_ms, 150);
    EXPECT_EQ(m.splash.min_display_ms, 800);
}

// 测试 应用清单：写入后再读回保持一致（往返）
TEST(AppManifestTest, WriteThenLoadRoundTrip) {
    TempDir tmp;
    const auto p = tmp.file("roundtrip.dsmanifest");

    AppManifest src;
    src.has_window_title = true;
    src.window_title = "RoundTrip Game";
    src.has_window_size = true;
    src.window_width = 1600;
    src.window_height = 900;
    src.has_splash = true;
    // 用绝对路径，避免读回时按目录再次解析。
    src.splash.image_path = (tmp.path() / "logo.png").string();
    src.splash.app_name = "RT";
    src.splash.initial_status = "init";
    src.splash.card_width = 470;
    src.splash.card_height = 310;
    src.splash.logo_size = 100;
    src.splash.bg_argb = 0xFF010203u;
    src.splash.title_argb = 0xFF0A0B0Cu;
    src.splash.status_argb = 0xFF111213u;
    src.splash.accent_argb = 0xFF202122u;
    src.splash.fade_in_ms = 120;
    src.splash.fade_out_ms = 130;
    src.splash.min_display_ms = 700;
    src.splash.enabled = true;

    ASSERT_TRUE(WriteAppManifest(p.string(), src));

    AppManifest dst;
    ASSERT_TRUE(LoadAppManifest(p.string(), dst));

    EXPECT_TRUE(dst.has_window_title);
    EXPECT_EQ(dst.window_title, src.window_title);
    EXPECT_TRUE(dst.has_window_size);
    EXPECT_EQ(dst.window_width, src.window_width);
    EXPECT_EQ(dst.window_height, src.window_height);

    ASSERT_TRUE(dst.has_splash);
    EXPECT_EQ(std::filesystem::path(dst.splash.image_path),
              std::filesystem::path(src.splash.image_path));
    EXPECT_EQ(dst.splash.app_name, src.splash.app_name);
    EXPECT_EQ(dst.splash.initial_status, src.splash.initial_status);
    EXPECT_EQ(dst.splash.card_width, src.splash.card_width);
    EXPECT_EQ(dst.splash.card_height, src.splash.card_height);
    EXPECT_EQ(dst.splash.logo_size, src.splash.logo_size);
    EXPECT_EQ(dst.splash.bg_argb, src.splash.bg_argb);
    EXPECT_EQ(dst.splash.title_argb, src.splash.title_argb);
    EXPECT_EQ(dst.splash.status_argb, src.splash.status_argb);
    EXPECT_EQ(dst.splash.accent_argb, src.splash.accent_argb);
    EXPECT_EQ(dst.splash.fade_in_ms, src.splash.fade_in_ms);
    EXPECT_EQ(dst.splash.fade_out_ms, src.splash.fade_out_ms);
    EXPECT_EQ(dst.splash.min_display_ms, src.splash.min_display_ms);
    EXPECT_EQ(dst.splash.enabled, src.splash.enabled);
}

// 测试 应用清单：空清单写出空对象且可被读回（无任何has标志）
TEST(AppManifestTest, WriteEmptyManifestProducesLoadableEmptyObject) {
    TempDir tmp;
    const auto p = tmp.file("emptyout.dsmanifest");
    AppManifest src; // 全部 has_* 为 false
    ASSERT_TRUE(WriteAppManifest(p.string(), src));
    AppManifest dst;
    ASSERT_TRUE(LoadAppManifest(p.string(), dst));
    EXPECT_FALSE(dst.has_window_title);
    EXPECT_FALSE(dst.has_window_size);
    EXPECT_FALSE(dst.has_splash);
}
