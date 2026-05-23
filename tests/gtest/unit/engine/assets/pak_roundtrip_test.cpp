/**
 * @file pak_roundtrip_test.cpp
 * @brief PakWriter + PakReader round-trip 测试
 */

#include <gtest/gtest.h>
#include "engine/assets/pak_writer.h"
#include "engine/assets/pak_reader.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class PakRoundtripTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "dse_pak_test";
        fs::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp_dir_, ec);
    }

    void WriteFile(const std::string& name, const std::string& content) {
        std::ofstream out((tmp_dir_ / name).string(), std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    fs::path tmp_dir_;
};

TEST_F(PakRoundtripTest, WriteAndReadSingleFile) {
    const std::string payload = "Hello DSEngine Pak!";
    WriteFile("hello.txt", payload);

    std::string pak_path = (tmp_dir_ / "test.dpak").string();
    std::vector<std::string> files = {(tmp_dir_ / "hello.txt").string()};

    ASSERT_TRUE(dse::pak::WriteDpak(pak_path, tmp_dir_.string(), files));

    dse::pak::PakReader reader;
    ASSERT_TRUE(reader.Open(pak_path));
    EXPECT_TRUE(reader.IsOpen());
    EXPECT_EQ(reader.GetEntries().size(), 1u);
    EXPECT_TRUE(reader.Contains("hello.txt"));
    EXPECT_FALSE(reader.Contains("missing.txt"));

    std::vector<uint8_t> data;
    ASSERT_TRUE(reader.ReadFile("hello.txt", data));
    std::string recovered(data.begin(), data.end());
    EXPECT_EQ(recovered, payload);
}

TEST_F(PakRoundtripTest, WriteAndReadMultipleFiles) {
    WriteFile("a.bin", std::string(256, 'A'));
    WriteFile("b.bin", std::string(1024, 'B'));
    WriteFile("c.bin", "tiny");

    std::string pak_path = (tmp_dir_ / "multi.dpak").string();
    std::vector<std::string> files = {
        (tmp_dir_ / "a.bin").string(),
        (tmp_dir_ / "b.bin").string(),
        (tmp_dir_ / "c.bin").string(),
    };

    ASSERT_TRUE(dse::pak::WriteDpak(pak_path, tmp_dir_.string(), files));

    dse::pak::PakReader reader;
    ASSERT_TRUE(reader.Open(pak_path));
    EXPECT_EQ(reader.GetEntries().size(), 3u);

    std::vector<uint8_t> data;
    ASSERT_TRUE(reader.ReadFile("a.bin", data));
    EXPECT_EQ(data.size(), 256u);
    EXPECT_TRUE(std::all_of(data.begin(), data.end(), [](uint8_t v) { return v == 'A'; }));

    ASSERT_TRUE(reader.ReadFile("b.bin", data));
    EXPECT_EQ(data.size(), 1024u);

    ASSERT_TRUE(reader.ReadFile("c.bin", data));
    EXPECT_EQ(std::string(data.begin(), data.end()), "tiny");
}

TEST_F(PakRoundtripTest, EmptyFileList) {
    std::string pak_path = (tmp_dir_ / "empty.dpak").string();
    EXPECT_FALSE(dse::pak::WriteDpak(pak_path, tmp_dir_.string(), {}));
}

TEST_F(PakRoundtripTest, ReadNonexistentFile) {
    dse::pak::PakReader reader;
    EXPECT_FALSE(reader.Open("nonexistent.dpak"));
    EXPECT_FALSE(reader.IsOpen());
}

TEST_F(PakRoundtripTest, ReadMissingEntry) {
    WriteFile("only.txt", "content");
    std::string pak_path = (tmp_dir_ / "one.dpak").string();
    std::vector<std::string> files = {(tmp_dir_ / "only.txt").string()};
    ASSERT_TRUE(dse::pak::WriteDpak(pak_path, tmp_dir_.string(), files));

    dse::pak::PakReader reader;
    ASSERT_TRUE(reader.Open(pak_path));

    std::vector<uint8_t> data;
    EXPECT_FALSE(reader.ReadFile("no_such_file.txt", data));
}

TEST_F(PakRoundtripTest, CloseAndReopen) {
    WriteFile("data.bin", "reopen_test");
    std::string pak_path = (tmp_dir_ / "reopen.dpak").string();
    std::vector<std::string> files = {(tmp_dir_ / "data.bin").string()};
    ASSERT_TRUE(dse::pak::WriteDpak(pak_path, tmp_dir_.string(), files));

    dse::pak::PakReader reader;
    ASSERT_TRUE(reader.Open(pak_path));
    reader.Close();
    EXPECT_FALSE(reader.IsOpen());
    EXPECT_TRUE(reader.GetEntries().empty());

    ASSERT_TRUE(reader.Open(pak_path));
    EXPECT_TRUE(reader.IsOpen());
    EXPECT_EQ(reader.GetEntries().size(), 1u);
}
