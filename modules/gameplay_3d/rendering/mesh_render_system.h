/**
 * @file mesh_render_system.h
 * @brief 3D 静态网格渲染系统，负责提取 MeshRendererComponent 并提交给 RHI
 */

#ifndef DSE_MESH_RENDER_SYSTEM_H
#define DSE_MESH_RENDER_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include <glm/glm.hpp>
class AssetManager;

namespace dse {
namespace gameplay3d {

/// Hi-Z AABB 数据（std430 layout: 2 x vec4 per object）
struct HiZAABB {
    glm::vec4 min_point;  // xyz = world min, w = 0
    glm::vec4 max_point;  // xyz = world max, w = 0
};

/**
 * @class MeshRenderSystem
 * @brief 3D 渲染系统，负责遍历场景中的 3D 网格并提交渲染
 */
class MeshRenderSystem {
public:
    MeshRenderSystem() = default;
    ~MeshRenderSystem() = default;

    /**
     * @brief 提取 ECS 中的 MeshRendererComponent 并提交到 CommandBuffer
     * @param world 实体世界
     * @param cmd_buffer 渲染命令缓冲
     */
    void Render(World& world, CommandBuffer& cmd_buffer);

    /**
     * @brief 渲染透明物体 (WBOIT)
     * @param wboit_mode 1=accumulation, 2=revealage
     */
    void RenderTransparent(World& world, CommandBuffer& cmd_buffer, int wboit_mode);

    void SetAssetManager(AssetManager* asset_manager);

    /// Hi-Z Occlusion Culling: 获取上一帧收集的 AABB 列表
    const std::vector<HiZAABB>& cached_aabbs() const { return cached_aabbs_; }
    int cached_aabb_count() const { return static_cast<int>(cached_aabbs_.size()); }

    /// Hi-Z Occlusion Culling: 设置当前帧可见性数据（从 GPU SSBO 读回）
    /// 当 AABB 数量与可见性数据大小不一致时自动失效（entity 增删导致索引错位）
    void SetHiZVisibility(const std::vector<uint32_t>& visibility) {
        if (visibility.size() != cached_aabbs_.size()) {
            hiz_visibility_.clear();
        } else {
            hiz_visibility_ = visibility;
        }
    }

private:
    AssetManager* asset_manager_ = nullptr;
    std::vector<MeshDrawItem> transparent_items_;  ///< 每帧缓存的透明绘制项

    /// Hi-Z: 上一帧收集的不透明 mesh AABB（供下一帧 GPU 剔除使用）
    std::vector<HiZAABB> cached_aabbs_;
    /// Hi-Z: 当前帧使用的可见性数据（从上一帧 GPU 计算结果读回）
    std::vector<uint32_t> hiz_visibility_;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_MESH_RENDER_SYSTEM_H
