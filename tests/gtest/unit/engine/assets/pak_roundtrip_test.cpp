/**
 * @file pak_roundtrip_test.cpp
 * @brief PakWriter + PakReader round-trip 测试
 */

#include <gtest/gtest.h>
#include "engine/assets/pak_writer.h"
#include "engine/assets/pak_reader.h"
#include "engine/assets/pak_format.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <iterator>

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

    // 写一个最小有效 .dpak（单文件），返回其磁盘字节，供腐化测试改写。
    std::vector<uint8_t> BuildValidPakBytes() {
        WriteFile("seed.bin", std::string(64, 'Z'));
        std::string pak_path = (tmp_dir_ / "seed.dpak").string();
        std::vector<std::string> files = {(tmp_dir_ / "seed.bin").string()};
        EXPECT_TRUE(dse::pak::WriteDpak(pak_path, tmp_dir_.string(), files));
        std::ifstream in(pak_path, std::ios::binary);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    }

    // 把字节写到一个腐化 pak 文件并尝试打开，返回 Open 结果。
    bool OpenTampered(const std::vector<uint8_t>& bytes, const std::string& name) {
        std::string path = (tmp_dir_ / name).string();
        { std::ofstream out(path, std::ios::binary);
          out.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size())); }
        dse::pak::PakReader reader;
        return reader.Open(path);
    }

    fs::path tmp_dir_;
};

// 测试 PAK往返：写入且读取单一文件
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

// 测试 PAK往返：写入且读取多个Files
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

// 测试 PAK往返：空文件列表
TEST_F(PakRoundtripTest, EmptyFileList) {
    std::string pak_path = (tmp_dir_ / "empty.dpak").string();
    EXPECT_FALSE(dse::pak::WriteDpak(pak_path, tmp_dir_.string(), {}));
}

// 测试 PAK往返：读取不存在文件
TEST_F(PakRoundtripTest, ReadNonexistentFile) {
    dse::pak::PakReader reader;
    EXPECT_FALSE(reader.Open("nonexistent.dpak"));
    EXPECT_FALSE(reader.IsOpen());
}

// 测试 PAK往返：读取缺失条目
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

// 测试 PAK往返：关闭且Reopen
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

// ── 损坏样本：畸形 .dpak 必须被安全拒绝（不崩溃、不超大分配） ──────────────

// 截断到比文件头还短：Open 拒绝
TEST_F(PakRoundtripTest, CorruptTruncatedHeaderRejected) {
    auto bytes = BuildValidPakBytes();
    ASSERT_GT(bytes.size(), sizeof(dse::pak::DpakHeader));
    bytes.resize(10); // < 32 字节头
    EXPECT_FALSE(OpenTampered(bytes, "trunc.dpak"));
}

// entry_count 被改成近 4G：旧实现会 resize(~4G) 触发 bad_alloc；现按文件容量拒绝
TEST_F(PakRoundtripTest, CorruptHugeEntryCountRejected) {
    auto bytes = BuildValidPakBytes();
    dse::pak::DpakHeader hdr{};
    std::memcpy(&hdr, bytes.data(), sizeof(hdr));
    hdr.entry_count = 0xFFFFFFFFu;
    std::memcpy(bytes.data(), &hdr, sizeof(hdr));
    EXPECT_FALSE(OpenTampered(bytes, "huge_count.dpak"));
}

// toc_offset 指向文件外：Open 拒绝
TEST_F(PakRoundtripTest, CorruptTocOffsetBeyondFileRejected) {
    auto bytes = BuildValidPakBytes();
    dse::pak::DpakHeader hdr{};
    std::memcpy(&hdr, bytes.data(), sizeof(hdr));
    hdr.toc_offset = bytes.size() + 4096ull;
    std::memcpy(bytes.data(), &hdr, sizeof(hdr));
    EXPECT_FALSE(OpenTampered(bytes, "bad_toc.dpak"));
}

// TOC 条目 data_size 指向文件外：Open 拒绝（防 ReadFile 越界/超大分配）
TEST_F(PakRoundtripTest, CorruptEntryDataSizeBeyondFileRejected) {
    auto bytes = BuildValidPakBytes();
    dse::pak::DpakHeader hdr{};
    std::memcpy(&hdr, bytes.data(), sizeof(hdr));
    ASSERT_GE(hdr.entry_count, 1u);
    // 首条 TOC 项的 data_size 字段偏移 = toc_offset + offsetof(DpakTocEntry, data_size)
    const size_t ds_off = static_cast<size_t>(hdr.toc_offset)
                        + offsetof(dse::pak::DpakTocEntry, data_size);
    ASSERT_LE(ds_off + sizeof(uint64_t), bytes.size());
    uint64_t huge = 0xFFFFFFFFFFull;
    std::memcpy(bytes.data() + ds_off, &huge, sizeof(huge));
    EXPECT_FALSE(OpenTampered(bytes, "bad_entry.dpak"));
}
