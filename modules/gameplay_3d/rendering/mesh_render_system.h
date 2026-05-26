/**
 * @file mesh_render_system.h
 * @brief 3D 静态网格渲染系统，负责提取 MeshRendererComponent 并提交给 RHI
 */

#ifndef DSE_MESH_RENDER_SYSTEM_H
#define DSE_MESH_RENDER_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/render/hiz_types.h"
#include "engine/render/static_batch/static_batch_builder.h"
#include <glm/glm.hpp>
class AssetManager;


namespace dse { namespace render { struct RenderPassContext; } }

namespace dse {
namespace gameplay3d {

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
        file_mesh_registry_.clear();
        inline_mesh_registry_.clear();
        mega_vbo_data_.clear();
        mega_ibo_data_.clear();
        mega_vbo_vertex_count_ = 0;
        mega_ibo_index_count_ = 0;
        mega_buffer_dirty_ = true;
        vertex_cache_.clear();
        mesh_path_templates_.clear();
        batch_cache_dirty_ = true;
        cached_opaque_items_.clear();
    }

    /// 场景切换或静态物体变化时调用：下帧重建静态合批
    void InvalidateStaticBatches() {
        static_batches_built_ = false;
        static_batch_items_.clear();
        vertex_cache_.clear();
        mesh_path_templates_.clear();
        batch_cache_dirty_ = true;
        cached_opaque_items_.clear();
    }

    /// 静态合批统计：上帧合并后的批次数
    int static_batch_count() const { return static_cast<int>(static_batch_items_.size()); }

    /**
     * @brief GPU Driven: 准备每帧 GPU 场景数据
     * 收集不透明非蒙皮 mesh，填充 DrawElementsIndirectCommand / GPUInstanceData / AABB，
     * 上传到 RHI 缓冲区，更新 RenderPassContext 中的计数器。
     * @return 本帧 indirect draw command 数量（0 表示无可用数据或不支持）
     */
    int PrepareGPUScene(World& world, dse::render::RenderPassContext& ctx);


    /// Vertex Cache: 场景切换时清除顶点缓存
    void InvalidateVertexCache() { vertex_cache_.clear(); }

    /// 每帧 Update 后标记 batch_items 需要重建（下一次 Render() 首调构建，后续 pass 复用）
    void MarkBatchDirty() { batch_cache_dirty_ = true; }

    /// Hi-Z Occlusion Culling: 获取上一帧收集的 AABB 列表
    const std::vector<HiZAABB>& cached_aabbs() const { return cached_aabbs_; }
    int cached_aabb_count() const { return static_cast<int>(cached_aabbs_.size()); }

    /// Hi-Z Occlusion Culling: 设置当前帧可见性数据（从 GPU SSBO 读回）
    /// 当 AABB 数量与可见性数据大小不一致时自动失效（entity 增删导致索引错位）
    void SetHiZVisibility(const std::vector<uint32_t>& visibility) {
        if (visibility.size() != cached_aabbs_.size()) {
            hiz_visibility_.clear();
            return;
        }
        // 防止死循环：全部 occluded → 清空 visibility（保守策略，下帧全部可见）
        bool any_visible = false;
        for (auto v : visibility) { if (v != 0) { any_visible = true; break; } }
        if (!any_visible) {
            hiz_visibility_.clear();
            return;
        }
        hiz_visibility_ = visibility;
    }

    /// GPU Driven: 判断实体是否走 GPU-driven 路径（不透明、非蒙皮、有 bounds）
    static bool IsGPUDrivenEligible(World& world, entt::entity entity,
                                     const MeshRendererComponent& mr);

private:
    AssetManager* asset_manager_ = nullptr;
    std::vector<MeshDrawItem> transparent_items_;  ///< 每帧缓存的透明绘制项

    /// 静态合批：首帧构建后缓存，后续帧直接复用
    dse::render::StaticBatchBuilder static_batch_builder_;
    std::vector<MeshDrawItem> static_batch_items_;   ///< 合批结果（缓存至场景切换）
    bool static_batches_built_ = false;              ///< 是否已完成首帧静态合批

    /// Hi-Z: 上一帧收集的不透明 mesh AABB（供下一帧 GPU 剔除使用）
    std::vector<HiZAABB> cached_aabbs_;
    /// Hi-Z: 当前帧使用的可见性数据（从上一帧 GPU 计算结果读回）
    std::vector<uint32_t> hiz_visibility_;

    /// GPU Driven: 本帧 GPU Driven 路径是否已处理不透明 mesh（避免 Render 中双重绘制）
    bool gpu_driven_active_ = false;

    /// Vertex rebuild cache: 避免每帧/每 pass 重复构建 BatchVertex（per-entity）
    struct VertexCacheEntry {
        std::vector<BatchVertex> vertices;
        std::vector<uint32_t> indices;
        glm::vec3 bounds_min{0.0f};
        glm::vec3 bounds_max{0.0f};
        const float* data_ptr = nullptr;  ///< temp_vertices.data() 指针，用于失效检测
    };
    std::unordered_map<uint32_t, VertexCacheEntry> vertex_cache_;

    /// Mesh path template: 按 mesh_path 缓存 local-space BatchVertex，
    /// 同 mesh 的所有蒙皮实例共享同一份数据，执行器跳过重复 VBO 上传。
    struct MeshPathTemplate {
        std::vector<BatchVertex> vertices;
        std::vector<uint32_t> indices;
        glm::vec3 bounds_min{0.0f};
        glm::vec3 bounds_max{0.0f};
        const float* data_ptr = nullptr;  ///< 失效检测
    };
    std::unordered_map<std::string, MeshPathTemplate> mesh_path_templates_;

    /// Per-frame batch_items cache: Render() 首次调用构建，后续 pass 直接复用
    std::vector<MeshDrawItem> cached_opaque_items_;
    std::vector<MeshDrawItem> cached_transparent_items_;
    bool batch_cache_dirty_ = true;  ///< OnUpdate() 设 true，首次 Render() 消费后 false

    /// GPU Driven: 每帧缓存
    std::vector<DrawElementsIndirectCommand> gpu_draw_cmds_;
    std::vector<dse::render::GPUInstanceData> gpu_instances_;
    std::vector<HiZAABB> gpu_aabbs_;
    std::vector<dse::render::GPUMaterialData> gpu_materials_;
    std::unordered_map<uint64_t, uint32_t> material_dedup_;
    std::vector<dse::render::GPUDrawTextures> gpu_tex_keys_;
    std::vector<dse::render::TextureBucket> gpu_texture_buckets_;
    size_t gpu_draw_cmd_capacity_ = 0;
    size_t gpu_instance_capacity_ = 0;
    size_t gpu_material_capacity_ = 0;

    /// Mega buffer registry: file mesh 用 mesh_path 做 key（去重）；inline mesh 用 entity id 做 key（独立）
    std::unordered_map<std::string, dse::render::MeshBatchEntry> file_mesh_registry_;
    std::unordered_map<uint32_t, dse::render::MeshBatchEntry> inline_mesh_registry_;
    std::vector<float> mega_vbo_data_;       ///< 累积的顶点数据
    std::vector<uint32_t> mega_ibo_data_;    ///< 累积的索引数据（32-bit）
    uint32_t mega_vbo_vertex_count_ = 0;     ///< mega VBO 中已有的顶点总数
    uint32_t mega_ibo_index_count_ = 0;      ///< mega IBO 中已有的 index 总数
    dse::render::VertexArrayHandle mega_vao_;              ///< mega buffer 的 VAO
    dse::render::BufferHandle mega_vbo_;              ///< mega VBO handle
    dse::render::BufferHandle mega_ibo_;              ///< mega IBO handle
    bool mega_buffer_dirty_ = false;         ///< 需要重新上传
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_MESH_RENDER_SYSTEM_H
