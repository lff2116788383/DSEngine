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

    /// 主渲染：depth_only=true 时（PreZ 深度预通道）走 MeshRenderer 实例化深度路径，
    /// false 时（Opaque 彩色通道）走 MeshRenderer 前向路径。
    void Render(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                const glm::vec3& camera_offset = glm::vec3(0.0f),
                bool depth_only = false);

    void RenderShadow(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
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
    void RenderInternal(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                        bool depth_only, bool shadow_pass,
                        const glm::vec3& camera_offset);

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
