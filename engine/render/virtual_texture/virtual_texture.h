/**
 * @file virtual_texture.h
 * @brief 虚拟纹理（Virtual Texture / Sparse Virtual Texturing）系统
 *
 * 允许使用超大纹理（如 64K×64K 的 terrain splat 或 megatexture），
 * 只将相机可见区域的纹理页加载到 GPU 物理页池中。
 *
 * 架构：
 * - Page Table: 间接映射表（虚拟页 → 物理页），存储为小纹理上传到 GPU
 * - Physical Page Pool: 固定大小的物理纹理 atlas，存储实际像素数据
 * - Feedback Buffer: GPU 渲染时记录使用的虚拟页 + mip level
 * - CPU 端每帧：读取 feedback → 决定加载/驱逐 → 更新 page table
 *
 * 页大小固定为 128×128 像素（含 4 像素 border 用于纹理过滤）
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {

namespace render {
class RhiDevice;
}

namespace vt {

// ─── 常量 ───────────────────────────────────────────────────────────────────

static constexpr uint32_t kPageSize = 128;         ///< 页面像素尺寸（不含 border）
static constexpr uint32_t kPageBorder = 4;         ///< 每页边界像素（用于双线性过滤）
static constexpr uint32_t kPageSizeWithBorder = kPageSize + kPageBorder * 2; // 136
static constexpr uint32_t kMaxMipLevels = 12;      ///< 最大 mip 级别数

// ─── 数据结构 ───────────────────────────────────────────────────────────────

/// 虚拟页坐标
struct PageId {
    uint32_t x = 0;       ///< 页列
    uint32_t y = 0;       ///< 页行
    uint32_t mip = 0;     ///< Mip level
    uint32_t vt_id = 0;   ///< 虚拟纹理 ID（支持多张虚拟纹理）

    bool operator==(const PageId& o) const {
        return x == o.x && y == o.y && mip == o.mip && vt_id == o.vt_id;
    }
};

struct PageIdHash {
    size_t operator()(const PageId& p) const {
        size_t h = std::hash<uint32_t>()(p.x);
        h ^= std::hash<uint32_t>()(p.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint32_t>()(p.mip) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint32_t>()(p.vt_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

/// 物理页池中的槽位
struct PhysicalPage {
    uint32_t pool_x = 0;    ///< 在 atlas 中的列
    uint32_t pool_y = 0;    ///< 在 atlas 中的行
    bool occupied = false;
    PageId resident_page;   ///< 当前驻留的虚拟页
    uint64_t last_used_frame = 0;
};

/// Page Table 条目：GPU 在 shader 中读取此表来做虚拟→物理映射
struct PageTableEntry {
    uint16_t physical_x = 0;  ///< 物理 atlas 中的列
    uint16_t physical_y = 0;  ///< 物理 atlas 中的行
    uint8_t valid = 0;        ///< 是否有效
    uint8_t padding[3] = {};
};

/// Feedback 数据（GPU → CPU 回读）
struct FeedbackEntry {
    uint16_t page_x;
    uint16_t page_y;
    uint8_t mip;
    uint8_t vt_id;
};

// ─── 配置 ───────────────────────────────────────────────────────────────────

struct VirtualTextureConfig {
    uint32_t virtual_size = 16384;      ///< 虚拟纹理总尺寸（像素）
    uint32_t pool_size_pages = 64;      ///< 物理池每边页数（64 → 64×64=4096 页）
    uint32_t feedback_scale = 8;        ///< Feedback 缓冲降采样倍率
    uint32_t max_uploads_per_frame = 8; ///< 每帧最多上传页数
    std::string tile_data_path;         ///< 磁盘瓦片数据目录
};

// ─── ECS 组件 ───────────────────────────────────────────────────────────────

/// 标记使用虚拟纹理的材质
struct VirtualTextureComponent {
    bool enabled = true;
    uint32_t vt_id = 0;                    ///< 对应的虚拟纹理 ID
    std::string tile_data_path;            ///< 瓦片数据路径
    uint32_t virtual_width = 16384;
    uint32_t virtual_height = 16384;
    float mip_bias = 0.0f;                 ///< Mip 采样偏移
};

// ─── 页缓存 ─────────────────────────────────────────────────────────────────

/// 管理物理页池的 LRU 缓存
class DSE_EXPORT VTPageCache {
public:
    VTPageCache() = default;

    void Init(uint32_t pool_size_pages);

    /// 查询虚拟页是否在缓存中
    bool Contains(const PageId& page) const;

    /// 分配物理页（如果缓存满则 LRU 驱逐）
    PhysicalPage* Allocate(const PageId& page, uint64_t frame);

    /// 标记页面使用
    void Touch(const PageId& page, uint64_t frame);

    /// 获取物理页信息
    const PhysicalPage* GetPhysicalPage(const PageId& page) const;

    uint32_t PoolSizePages() const { return pool_size_; }
    size_t OccupiedCount() const { return page_map_.size(); }

    void Clear();

private:
    uint32_t pool_size_ = 0;                                    // 每边页数
    std::vector<PhysicalPage> pages_;                           // 所有物理页
    std::unordered_map<PageId, uint32_t, PageIdHash> page_map_; // 虚拟页 → 物理页索引
    std::vector<uint32_t> free_list_;                           // 空闲页索引
};

// ─── 虚拟纹理系统 ──────────────────────────────────────────────────────────

/// 页加载请求回调
using PageLoadCallback = std::function<bool(const PageId& page, std::vector<uint8_t>& out_pixels)>;

class DSE_EXPORT VirtualTextureSystem {
public:
    VirtualTextureSystem() = default;
    ~VirtualTextureSystem() = default;

    /// 初始化虚拟纹理系统
    bool Init(const VirtualTextureConfig& config, render::RhiDevice* rhi);

    /// 每帧更新：处理 feedback → 加载页 → 更新 page table
    void Update(uint64_t frame_number);

    /// 提交 feedback 数据（从 GPU readback 获得）
    void SubmitFeedback(const std::vector<FeedbackEntry>& entries);

    /// 注册页数据加载回调
    void SetPageLoadCallback(PageLoadCallback cb) { page_load_cb_ = std::move(cb); }

    /// 获取 page table（用于上传到 GPU）
    const std::vector<PageTableEntry>& GetPageTable() const { return page_table_; }

    /// 获取 page table 纹理的维度
    uint32_t PageTableSize() const;

    /// 获取物理 atlas 纹理维度（像素）
    uint32_t PhysicalAtlasSize() const;

    /// 缓存命中率统计
    float CacheHitRate() const;

    void Shutdown();

    const VTPageCache& GetCache() const { return cache_; }

private:
    void ProcessFeedback();
    void UploadPages();
    void RebuildPageTable();

    VirtualTextureConfig config_;
    render::RhiDevice* rhi_ = nullptr;
    VTPageCache cache_;

    std::vector<PageTableEntry> page_table_;
    std::vector<FeedbackEntry> pending_feedback_;
    std::vector<PageId> load_queue_;
    PageLoadCallback page_load_cb_;

    uint64_t current_frame_ = 0;
    uint64_t cache_hits_ = 0;
    uint64_t cache_misses_ = 0;
    bool initialized_ = false;
};

} // namespace vt
} // namespace dse
