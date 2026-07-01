/**
 * @file binary_patch.h
 * @brief 轻量级二进制增量 patch 生成与应用。
 *
 * 算法：按固定 block（默认 4KB）逐块比较新旧文件，只记录变化块的偏移和数据。
 * patch 格式：[header][block_entry...]
 *   header: magic(4) + version(4) + old_size(8) + new_size(8) + block_size(4) + entry_count(4)
 *   block_entry: offset(8) + compressed_len(4) + raw_data(compressed_len)
 *
 * 无外部压缩依赖；对数据进行简单的 RLE 压缩以减小 patch 体积。
 * 适用于资产文件的增量更新场景。
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dse {
namespace assets {

/// Patch 文件头魔数
inline constexpr uint32_t kPatchMagic = 0x44535050; // "DSPP" (DS Patch Pack)
inline constexpr uint32_t kPatchVersion = 1;
inline constexpr uint32_t kDefaultBlockSize = 4096;

/// 单个变更块
struct PatchBlock {
    uint64_t offset = 0;         ///< 在新文件中的偏移
    std::vector<uint8_t> data;   ///< 块数据
};

/// Patch 数据（内存表示）
struct PatchData {
    uint64_t old_size = 0;
    uint64_t new_size = 0;
    uint32_t block_size = kDefaultBlockSize;
    std::vector<PatchBlock> blocks;

    /// patch 的有效负载大小
    uint64_t PayloadSize() const {
        uint64_t total = 0;
        for (const auto& b : blocks) total += b.data.size();
        return total;
    }
};

/// 二进制 Patch 工具类
class BinaryPatch {
public:
    /// 生成 patch：比较旧文件与新文件，输出增量数据
    /// @return true 成功
    static bool Generate(const std::string& old_file_path,
                         const std::string& new_file_path,
                         const std::string& patch_output_path,
                         uint32_t block_size = kDefaultBlockSize);

    /// 从内存数据生成 patch
    static bool GenerateFromMemory(const uint8_t* old_data, size_t old_size,
                                   const uint8_t* new_data, size_t new_size,
                                   std::vector<uint8_t>& patch_out,
                                   uint32_t block_size = kDefaultBlockSize);

    /// 应用 patch：将 patch 应用到旧文件，生成新文件
    /// @return true 成功
    static bool Apply(const std::string& old_file_path,
                      const std::string& patch_file_path,
                      const std::string& new_output_path);

    /// 从内存数据应用 patch
    static bool ApplyFromMemory(const uint8_t* old_data, size_t old_size,
                                const uint8_t* patch_data, size_t patch_size,
                                std::vector<uint8_t>& new_out);

    /// 序列化 PatchData 到字节流
    static std::vector<uint8_t> Serialize(const PatchData& pd);

    /// 反序列化字节流到 PatchData
    static bool Deserialize(const uint8_t* data, size_t size, PatchData& out);
};

} // namespace assets
} // namespace dse
