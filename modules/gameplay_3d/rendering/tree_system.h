#ifndef DSE_TREE_SYSTEM_H
#define DSE_TREE_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/mesh_renderer.h"
#include "engine/render/frame_context.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <string>

class AssetManager;

namespace dse {
namespace gameplay3d {

/// 单个树木实例的静态布局数据（缓存不变）
struct TreeInstanceLayout {
    glm::vec3 position;
    float yaw;
    float scale;
};

/// 树木 chunk 缓存数据
struct TreeChunkData {
    std::vector<TreeInstanceLayout> layouts;
    glm::vec3 aabb_min = glm::vec3(0.0f);
    glm::vec3 aabb_max = glm::vec3(0.0f);
    bool valid = false;
};

/// Phase 1：主线程（Prepare）从 ECS 提取的每帧树木渲染快照。
/// 渲染线程 Execute（Render）仅消费此结构，不再访问 World/ECS。
struct TreeFrameRenderData {
    struct EntityDraw {
        dse::render::ExternalShadedMesh tmpl;   ///< 共享局部空间模板 GPU 缓冲（借用 mesh_cache_ 的句柄）
        uint32_t index_count = 0;
        std::vector<glm::mat4> scene_transforms;   ///< cull_distance 视锥+距离剔除后（不含 camera_offset）
        std::vector<glm::mat4> shadow_transforms;  ///< shadow_distance 剔除后（不含 camera_offset）
    };
    std::vector<EntityDraw> entities;
    glm::vec3 light_dir = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color = glm::vec3(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.2f;
    float shadow_strength = 0.35f;
    bool has_camera = false;
};

/**
 * @class TreeSystem
 * @brief 树木/大型植被实例化渲染系统
 *
 * 核心特性:
 * - 使用外部 mesh 模型（TreeComponent::mesh_path），非程序化生成
 * - Chunk 空间缓存，增量更新
 * - 贴合 TerrainComponent 高度
 * - 通过 MeshDrawItem::instance_transforms 复用现有 GPU Instancing 管线
 */
class TreeSystem {
public:
    void Init(RhiDevice* rhi_device);
    void SetAssetManager(AssetManager* asset_manager);
    void Shutdown(World& world);

    void Update(World& world, float delta_time);

    /// Phase 1：主线程（Prepare）提取每帧渲染数据（相机剔除/距离/光照/GPU 模板），供渲染线程消费。
    void ExtractFrameRenderData(World& world, const glm::vec3& camera_offset = glm::vec3(0.0f));

    /// 主渲染：depth_only=true 时（PreZ 深度预通道）走 MeshRenderer 实例化深度路径，
    /// false 时（Opaque 彩色通道）走 MeshRenderer 前向路径。
    /// Phase 1：仅消费 ExtractFrameRenderData 提取的快照，不访问 World。
    void Render(CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                const glm::vec3& camera_offset = glm::vec3(0.0f),
                bool depth_only = false);

    void RenderShadow(CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                      const glm::vec3& camera_offset = glm::vec3(0.0f));

private:
    void GenerateChunkInstances(const TreeComponent& tree,
                                const TerrainComponent* terrain,
                                const TransformComponent* terrain_transform,
                                const TransformComponent& tree_transform,
                                int chunk_x, int chunk_z,
                                TreeChunkData& out);

    static uint64_t ChunkKey(int cx, int cz);
    static bool IsAABBInFrustum(const glm::vec4 planes[6],
                                const glm::vec3& aabb_min,
                                const glm::vec3& aabb_max);
    static void ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 out_planes[6]);

    /// depth_only：当前 pass 绑定无彩色的深度 RT（PreZ/Shadow）→ 走 MeshRenderer 实例化深度路径；
    /// shadow_pass：光源视角阴影 pass（用 shadow_distance + 跳 billboard）。
    void RenderInternal(CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                        bool depth_only, bool shadow_pass,
                        const glm::vec3& camera_offset);

    /// Phase 1：主线程提取的每帧渲染快照（Prepare 写，Execute 读）。
    TreeFrameRenderData frame_data_;

    /// 从 AssetManager 加载 mesh 并缓存为 BatchVertex + indices
    bool EnsureMeshLoaded(const std::string& mesh_path);

    /// mesh_path → 缓存的顶点/索引数据
    struct MeshCacheEntry {
        std::vector<BatchVertex> vertices;
        std::vector<uint32_t> indices;
        /// 前向 pass 用：共享局部空间模板 GPU 缓冲（懒建，DrawSharedTemplateInstanced 消费）
        dse::render::ExternalShadedMesh tmpl;
        uint32_t index_count = 0;
        bool gpu_template_built = false;
    };

    /// 懒建/复用 entry 的共享局部空间模板 GPU 缓冲（前向 pass）。返回是否可用。
    bool EnsureTemplateBuilt(MeshCacheEntry& entry);

    RhiDevice* rhi_ = nullptr;
    AssetManager* asset_manager_ = nullptr;
    dse::render::MeshRenderer mesh_renderer_;  ///< 前向 pass 通用网格渲染器（B2b-6 迁移）

    std::unordered_map<std::string, MeshCacheEntry> mesh_cache_;

    struct EntityCache {
        std::unordered_map<uint64_t, TreeChunkData> chunks;
        glm::vec3 last_camera_pos = glm::vec3(0.0f);
    };
    std::unordered_map<uint32_t, EntityCache> entity_caches_;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_TREE_SYSTEM_H
