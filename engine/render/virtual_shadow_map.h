/**
 * @file virtual_shadow_map.h
 * @brief 虚拟阴影贴图 (VSM) — 自适应精度 + 页表 + 缓存失效
 *
 * 功能：
 * - 虚拟页表：将超大阴影贴图虚拟化为页表+物理页池
 * - 自适应精度：屏幕空间反馈驱动页分配（近处高精度，远处低精度）
 * - 缓存失效：仅重渲染标记为脏的页（光源/物体移动时）
 * - 多光源：平行光全覆盖 + 聚光灯/点光源局部页
 * - Clipmap 分级：平行光使用 clipmap 层级覆盖不同距离
 */

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace render {

/// 页状态
enum class PageState : uint8_t {
    Unmapped = 0,   ///< 未分配物理页
    Cached = 1,     ///< 已缓存且有效
    Dirty = 2,      ///< 已缓存但需要重新渲染
    Requested = 3   ///< 被请求但尚未渲染
};

/// 虚拟页描述
struct VirtualPage {
    uint32_t virtual_x = 0;
    uint32_t virtual_y = 0;
    uint32_t mip_level = 0;
    uint32_t light_id = 0;
    uint32_t physical_x = UINT32_MAX;  ///< 物理页池中的位置
    uint32_t physical_y = UINT32_MAX;
    PageState state = PageState::Unmapped;
    uint32_t last_used_frame = 0;
};

/// 物理页池中的一页
struct PhysicalPage {
    bool allocated = false;
    uint32_t owner_hash = 0;           ///< 当前拥有者（虚拟页 hash）
    uint32_t last_rendered_frame = 0;
};

/// VSM 配置
struct VSMConfig {
    uint32_t virtual_resolution = 16384;   ///< 虚拟阴影贴图总分辨率
    uint32_t page_size = 128;              ///< 单页像素尺寸
    uint32_t physical_pool_pages = 1024;   ///< 物理页池总页数（128*128 * 1024 = 16M 像素）
    uint32_t clipmap_levels = 6;           ///< Clipmap 层数（平行光）
    float near_plane = 0.1f;
    float far_plane = 1000.0f;
    uint32_t max_pages_per_frame = 64;     ///< 每帧最多渲染的新页数
};

/// 光源阴影信息
struct ShadowLightInfo {
    uint32_t light_id = 0;
    glm::mat4 view_proj{1.0f};         ///< 光源 VP 矩阵
    glm::vec3 direction{0, -1, 0};
    float radius = 0.0f;               ///< 点光源/聚光灯范围
    bool is_directional = true;
    bool moved = false;                 ///< 本帧是否移动
};

/// VSM 统计信息
struct VSMStats {
    uint32_t total_pages;
    uint32_t mapped_pages;
    uint32_t dirty_pages;
    uint32_t rendered_this_frame;
    uint32_t cache_hit_rate_percent;
    uint32_t physical_pool_usage_percent;
};

/// 虚拟阴影贴图系统
class DSE_EXPORT VirtualShadowMapSystem {
public:
    VirtualShadowMapSystem() = default;
    ~VirtualShadowMapSystem() = default;

    void Init(const VSMConfig& config);
    void Shutdown();

    /// 注册光源
    uint32_t RegisterLight(const ShadowLightInfo& info);

    /// 注销光源
    void UnregisterLight(uint32_t light_id);

    /// 更新光源信息（位置/方向变化时）
    void UpdateLight(uint32_t light_id, const ShadowLightInfo& info);

    /// 提交屏幕空间反馈：哪些虚拟页被采样到
    void SubmitFeedback(const std::vector<glm::uvec3>& sampled_pages);

    /// 标记物体AABB导致的脏页
    void InvalidateRegion(uint32_t light_id, const glm::vec3& aabb_min, const glm::vec3& aabb_max);

    /// 每帧 Begin：收集反馈，决定哪些页需要渲染
    void BeginFrame(uint32_t frame_number, const glm::vec3& camera_pos);

    /// 获取本帧需要渲染的页列表
    const std::vector<VirtualPage>& GetPagesToRender() const { return pages_to_render_; }

    /// 标记页渲染完成
    void MarkPageRendered(uint32_t virtual_x, uint32_t virtual_y, uint32_t mip, uint32_t light_id);

    /// 每帧 End：更新页表
    void EndFrame();

    /// 查询页表：虚拟页 → 物理页映射
    bool LookupPage(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id,
                    uint32_t& out_px, uint32_t& out_py) const;

    /// 获取统计信息
    VSMStats GetStats() const;

    /// 获取配置
    const VSMConfig& GetConfig() const { return config_; }

    /// 获取 Clipmap 层级 VP 矩阵
    glm::mat4 GetClipmapLevelMatrix(uint32_t level) const;

private:
    uint32_t AllocatePhysicalPage();
    void FreePhysicalPage(uint32_t page_index);
    void EvictLRU();
    uint32_t PageHash(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id) const;

    VSMConfig config_;
    std::vector<VirtualPage> page_table_;
    std::vector<PhysicalPage> physical_pool_;
    std::vector<VirtualPage> pages_to_render_;
    std::vector<ShadowLightInfo> lights_;
    std::unordered_set<uint32_t> requested_pages_;  // hash set

    uint32_t current_frame_ = 0;
    uint32_t mapped_count_ = 0;
    uint32_t rendered_this_frame_ = 0;
    glm::vec3 camera_pos_{0.0f};
    bool initialized_ = false;
};

} // namespace render
} // namespace dse
