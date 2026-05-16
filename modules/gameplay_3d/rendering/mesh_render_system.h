/**
 * @file mesh_render_system.h
 * @brief 3D 静态网格渲染系统，负责提取 MeshRendererComponent 并提交给 RHI
 */

#ifndef DSE_MESH_RENDER_SYSTEM_H
#define DSE_MESH_RENDER_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include <glm/glm.hpp>
class AssetManager;

namespace dse { namespace render { struct RenderPassContext; } }

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

    /// 释放 GPU Driven 资源（Mega VAO/VBO/IBO），必须在 RHI Shutdown 之前调用
    void CleanupGPUResources(RhiDevice* rhi);

    /// 场景切换时调用：清空 mega buffer 注册表，下帧重建
    void InvalidateMegaBuffer() {
        mesh_registry_.clear();
        mega_vbo_data_.clear();
        mega_ibo_data_.clear();
        mega_vbo_vertex_count_ = 0;
        mega_ibo_index_count_ = 0;
        mega_buffer_dirty_ = true;
    }

    /**
     * @brief GPU Driven: 准备每帧 GPU 场景数据
     * 收集不透明非蒙皮 mesh，填充 DrawElementsIndirectCommand / GPUInstanceData / AABB，
     * 上传到 RHI 缓冲区，更新 RenderPassContext 中的计数器。
     * @return 本帧 indirect draw command 数量（0 表示无可用数据或不支持）
     */
    int PrepareGPUScene(World& world, dse::render::RenderPassContext& ctx);

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

    /// GPU Driven: 每帧缓存
    std::vector<DrawElementsIndirectCommand> gpu_draw_cmds_;
    std::vector<dse::render::GPUInstanceData> gpu_instances_;
    std::vector<HiZAABB> gpu_aabbs_;
    size_t gpu_draw_cmd_capacity_ = 0;
    size_t gpu_instance_capacity_ = 0;

    /// Mega buffer registry: mesh_path → 在 mega buffer 中的位置
    std::unordered_map<std::string, dse::render::MeshBatchEntry> mesh_registry_;
    std::vector<float> mega_vbo_data_;       ///< 累积的顶点数据
    std::vector<uint32_t> mega_ibo_data_;    ///< 累积的索引数据（32-bit）
    uint32_t mega_vbo_vertex_count_ = 0;     ///< mega VBO 中已有的顶点总数
    uint32_t mega_ibo_index_count_ = 0;      ///< mega IBO 中已有的 index 总数
    unsigned int mega_vao_ = 0;              ///< mega buffer 的 VAO
    unsigned int mega_vbo_ = 0;              ///< mega VBO GL handle
    unsigned int mega_ibo_ = 0;              ///< mega IBO GL handle
    bool mega_buffer_dirty_ = false;         ///< 需要重新上传
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_MESH_RENDER_SYSTEM_H
