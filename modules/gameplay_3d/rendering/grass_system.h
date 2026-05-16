#ifndef DSE_GRASS_SYSTEM_H
#define DSE_GRASS_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace dse {
namespace gameplay3d {

/// 单个草叶的静态布局数据（不含风场，缓存不变）
struct GrassInstanceLayout {
    glm::vec3 position;    ///< 世界坐标（含地形高度）
    float     yaw;         ///< 绕 Y 轴随机旋转角度 (rad)
    float     width;       ///< 最终草叶宽度
    float     height;      ///< 最终草叶高度
    float     wind_phase;  ///< 风相位种子（基于位置 hash）
};

/// GPU-friendly packed instance layout (32 bytes, std430 aligned)
struct GrassGPUInstance {
    glm::vec4 pos_yaw;        ///< xyz = world position, w = yaw (radians)
    glm::vec4 wh_phase_fade;  ///< x = width, y = height, z = wind_phase, w = fade_factor
};

/// 草地 chunk 缓存数据
struct GrassChunkData {
    std::vector<GrassInstanceLayout> layouts;  ///< 静态布局（缓存不变）
    glm::vec3 aabb_min = glm::vec3(0.0f);
    glm::vec3 aabb_max = glm::vec3(0.0f);
    bool valid = false;
};

/**
 * @class GrassSystem
 * @brief 大型植被渲染系统
 *
 * 核心特性:
 * - 程序化草叶 mesh（三角带 + billboard），系统级共享
 * - Chunk 空间缓存，增量更新，避免每帧全量重算
 * - 贴合 TerrainComponent 高度
 * - 通过 MeshDrawItem::instance_transforms 复用现有 GPU Instancing 管线
 */
class GrassSystem {
public:
    void Init(RhiDevice* rhi_device);
    void Shutdown(World& world);

    /// 每帧更新：增量维护 chunk 缓存
    void Update(World& world, float delta_time);

    /// 主场景渲染
    void Render(World& world, CommandBuffer& cmd_buffer);

    /// 阴影 pass 渲染（仅近距离 LOD 0）
    void RenderShadow(World& world, CommandBuffer& cmd_buffer);

private:
    /// 程序化生成草叶三角带 mesh (LOD 0)
    void BuildBladeMesh();

    /// 程序化生成交叉 billboard mesh (LOD 1)
    void BuildBillboardMesh();

    /// 为指定 chunk 生成实例数据
    void GenerateChunkInstances(const GrassComponent& grass,
                                const TerrainComponent* terrain,
                                const TransformComponent* terrain_transform,
                                const TransformComponent& grass_transform,
                                int chunk_x, int chunk_z,
                                GrassChunkData& out);

    /// 编码 chunk 坐标为 hash key
    static uint64_t ChunkKey(int cx, int cz);

    /// 6 平面视锥剔除（AABB vs frustum planes）
    static bool IsAABBInFrustum(const glm::vec4 planes[6],
                                const glm::vec3& aabb_min,
                                const glm::vec3& aabb_max);

    /// 从 VP 矩阵提取 6 个视锥平面
    static void ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 out_planes[6]);

    /// 内部渲染辅助（场景 pass 和阴影 pass 共用）
    void RenderInternal(World& world, CommandBuffer& cmd_buffer,
                        bool shadow_pass);

    RhiDevice* rhi_ = nullptr;

    // 共享草叶 mesh 数据 (LOD 0) — CPU 侧缓存，每帧拷贝到 MeshDrawItem
    std::vector<BatchVertex> blade_vertices_;
    std::vector<unsigned short> blade_indices_;

    // 共享 billboard mesh 数据 (LOD 1)
    std::vector<BatchVertex> billboard_vertices_;
    std::vector<unsigned short> billboard_indices_;

    // Per-entity chunk 缓存: entity_id → { chunk_key → ChunkData }
    struct EntityCache {
        std::unordered_map<uint64_t, GrassChunkData> chunks;
        glm::vec3 last_camera_pos = glm::vec3(0.0f);
    };
    std::unordered_map<uint32_t, EntityCache> entity_caches_;

    double accumulated_time_ = 0.0;  ///< 风场动画累积时间（double 避免长时间精度丢失）

    // GPU Compute 风场（仅 OpenGL 4.3+ 启用，VK/DX11 走 CPU fallback）
    void InitComputeShader();
    void ShutdownComputeResources();
    void EnsureSSBOCapacity(size_t required_count);

    unsigned int wind_compute_shader_ = 0;
    unsigned int input_ssbo_ = 0;
    unsigned int output_ssbo_ = 0;
    size_t ssbo_capacity_ = 0;
    bool gpu_compute_enabled_ = false;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_GRASS_SYSTEM_H
