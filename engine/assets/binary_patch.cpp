/**
 * @file binary_patch.cpp
 * @brief 轻量级二进制增量 patch 实现
 */

#include "engine/assets/binary_patch.h"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace dse {
namespace assets {

// ── 内部辅助：小端序列化 ─────────────────────────────────────────────────────

namespace {

void WriteU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v));
    buf.push_back(static_cast<uint8_t>(v >> 8));
    buf.push_back(static_cast<uint8_t>(v >> 16));
    buf.push_back(static_cast<uint8_t>(v >> 24));
}

void WriteU64(std::vector<uint8_t>& buf, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<uint8_t>(v >> (i * 8)));
    }
}

uint32_t ReadU32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

uint64_t ReadU64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= uint64_t(p[i]) << (i * 8);
    }
    return v;
}

std::vector<uint8_t> ReadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    if (sz <= 0) return {};
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

bool WriteFile(const std::string& path, const uint8_t* data, size_t size) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    return f.good();
}

} // anonymous namespace

// ── 序列化 / 反序列化 ────────────────────────────────────────────────────────

std::vector<uint8_t> BinaryPatch::Serialize(const PatchData& pd) {
    // header: magic(4) + version(4) + old_size(8) + new_size(8) + block_size(4) + entry_count(4)
    std::vector<uint8_t> out;
    out.reserve(32 + pd.blocks.size() * (12 + pd.block_size));

    WriteU32(out, kPatchMagic);
    WriteU32(out, kPatchVersion);
    WriteU64(out, pd.old_size);
    WriteU64(out, pd.new_size);
    WriteU32(out, pd.block_size);
    WriteU32(out, static_cast<uint32_t>(pd.blocks.size()));

    for (const auto& blk : pd.blocks) {
        WriteU64(out, blk.offset);
        WriteU32(out, static_cast<uint32_t>(blk.data.size()));
        out.insert(out.end(), blk.data.begin(), blk.data.end());
    }
    return out;
}

bool BinaryPatch::Deserialize(const uint8_t* data, size_t size, PatchData& out) {
    if (size < 32) return false;

    uint32_t magic = ReadU32(data);
    if (magic != kPatchMagic) return false;

    uint32_t ver = ReadU32(data + 4);
    if (ver != kPatchVersion) return false;

    out.old_size = ReadU64(data + 8);
    out.new_size = ReadU64(data + 16);
    out.block_size = ReadU32(data + 24);
    uint32_t count = ReadU32(data + 28);

    size_t pos = 32;
    out.blocks.clear();
    out.blocks.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 12 > size) return false;
        PatchBlock blk;
        blk.offset = ReadU64(data + pos); pos += 8;
        uint32_t len = ReadU32(data + pos); pos += 4;
        if (pos + len > size) return false;
        blk.data.assign(data + pos, data + pos + len);
        pos += len;
        out.blocks.push_back(std::move(blk));
    }
    return true;
}

// ── 生成 patch ───────────────────────────────────────────────────────────────

bool BinaryPatch::GenerateFromMemory(const uint8_t* old_data, size_t old_size,
                                     const uint8_t* new_data, size_t new_size,
                                     std::vector<uint8_t>& patch_out,
                                     uint32_t block_size) {
    if (block_size == 0) block_size = kDefaultBlockSize;

    PatchData pd;
    pd.old_size = old_size;
    pd.new_size = new_size;
    pd.block_size = block_size;

    // 逐块比较：只记录有变化的块
    size_t new_blocks = (new_size + block_size - 1) / block_size;

    for (size_t i = 0; i < new_blocks; ++i) {
        uint64_t offset = i * block_size;
        size_t this_block_size = std::min(static_cast<size_t>(block_size),
                                          new_size - static_cast<size_t>(offset));

        bool changed = true;
        if (offset + this_block_size <= old_size) {
            // 对应旧文件同位置的块存在，逐字节比较
            if (std::memcmp(old_data + offset, new_data + offset, this_block_size) == 0) {
                changed = false;
            }
        }

        if (changed) {
            PatchBlock blk;
            blk.offset = offset;
            blk.data.assign(new_data + offset, new_data + offset + this_block_size);
            pd.blocks.push_back(std::move(blk));
        }
    }

    patch_out = Serialize(pd);
    return true;
}

bool BinaryPatch::Generate(const std::string& old_file_path,
                           const std::string& new_file_path,
                           const std::string& patch_output_path,
                           uint32_t block_size) {
    auto old_data = ReadFile(old_file_path);
    auto new_data = ReadFile(new_file_path);
    if (new_data.empty()) return false; // 新文件必须存在

    std::vector<uint8_t> patch_buf;
    if (!GenerateFromMemory(old_data.data(), old_data.size(),
                            new_data.data(), new_data.size(),
                            patch_buf, block_size)) {
        return false;
    }
    return WriteFile(patch_output_path, patch_buf.data(), patch_buf.size());
}

// ── 应用 patch ───────────────────────────────────────────────────────────────

bool BinaryPatch::ApplyFromMemory(const uint8_t* old_data, size_t old_size,
                                  const uint8_t* patch_data, size_t patch_size,
                                  std::vector<uint8_t>& new_out) {
    PatchData pd;
    if (!Deserialize(patch_data, patch_size, pd)) return false;

    if (old_size != pd.old_size) return false;

    // 从旧文件开始，扩展或截断到新大小
    new_out.resize(static_cast<size_t>(pd.new_size), 0);
    size_t copy_size = std::min(old_size, static_cast<size_t>(pd.new_size));
    if (copy_size > 0 && old_data) {
        std::memcpy(new_out.data(), old_data, copy_size);
    }

    // 覆盖变化的块
    for (const auto& blk : pd.blocks) {
        if (blk.offset + blk.data.size() > pd.new_size) return false;
        std::memcpy(new_out.data() + blk.offset, blk.data.data(), blk.data.size());
    }
    return true;
}

bool BinaryPatch::Apply(const std::string& old_file_path,
                        const std::string& patch_file_path,
                        const std::string& new_output_path) {
    auto old_data = ReadFile(old_file_path);
    auto patch_buf = ReadFile(patch_file_path);
    if (patch_buf.empty()) return false;

    std::vector<uint8_t> new_data;
    if (!ApplyFromMemory(old_data.data(), old_data.size(),
                         patch_buf.data(), patch_buf.size(),
                         new_data)) {
        return false;
    }
    return WriteFile(new_output_path, new_data.data(), new_data.size());
}

} // namespace assets
} // namespace dse
