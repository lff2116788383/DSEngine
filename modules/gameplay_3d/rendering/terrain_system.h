#ifndef DSE_TERRAIN_SYSTEM_H
#define DSE_TERRAIN_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/mesh_renderer.h"
#include "engine/render/frame_context.h"
#include "engine/ecs/components_3d_terrain_tile.h"
#include <glm/glm.hpp>

namespace dse {
namespace gameplay3d {

/// Phase 1：主线程（Prepare）从 ECS 提取的每帧地形渲染快照。
/// 渲染线程 Execute（Render）仅消费此结构，不再访问 World/ECS。
struct TerrainFrameRenderData {
    struct DrawItem {
        dse::render::BufferHandle shaded_vbo;
        dse::render::BufferHandle index_buffer;
        uint32_t index_count = 0;
        glm::mat4 model = glm::mat4(1.0f);  ///< local_to_world（camera_offset 在 Execute 应用）
        // 材质（仅彩色 pass 使用）
        bool splat_enabled = false;
        dse::render::TextureHandle splat_weight_map;
        dse::render::TextureHandle splat_layers[4];
        glm::vec4 splat_tiling = glm::vec4(10.0f);
        dse::render::TextureHandle albedo_tex;
        float shadow_strength = 0.35f;
        bool has_snow = false;
        float snow_coverage = 0.0f;
        glm::vec3 snow_albedo = glm::vec3(0.92f, 0.93f, 0.96f);
        float snow_roughness = 0.75f;
        float snow_normal_threshold = 0.4f;
        float snow_edge_sharpness = 3.0f;
    };
    std::vector<DrawItem> patch_items;
    std::vector<DrawItem> tile_items;
    dse::render::DirectionalLight patch_light;
    dse::render::DirectionalLight tile_light;
    bool valid = false;
};

class TerrainSystem {
public:
    void Init(RhiDevice* rhi_device);
    void Shutdown(World& world);

    /// Phase 1：主线程（Prepare）提取每帧渲染数据（含 tile 生命周期/脏重建/LOD/光照）。
    void ExtractFrameRenderData(World& world);

    /// depth_only=true（PreZ/Shadow 深度 RT）走 MeshRenderer::DrawDepthOnlySharedTemplateInstanced，false（Opaque 彩色）走 MeshRenderer::DrawSharedTemplateInstanced。
    /// Phase 1：仅消费 ExtractFrameRenderData 提取的快照，不访问 World。
    void Render(CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                const glm::vec3& camera_offset = glm::vec3(0.0f),
                bool depth_only = false);

    /// CPU 侧双线性插值高度查询（世界空间 xz → 高度 y）
    static float SampleHeight(const TerrainComponent& terrain,
                               const TransformComponent& transform,
                               float world_x, float world_z);

    /// 脏时把逐顶点 splat_data 上传为 RGBA8 权重图纹理，供 splat 混合采样。
    /// Render() 内部按需调用；亦公开供单测直接驱动其分支（脏标志/尺寸回退/钳制）。
    void UploadSplatWeightMap(TerrainComponent& terrain);

private:
    void RebuildTerrain(TerrainComponent& terrain);
    void DestroyTerrainGPU(TerrainComponent& terrain);

    // Tiled terrain
    void UpdateTiles(World& world);
    void ExtractTiles(World& world);    ///< Prepare：把 tile 绘制项提取进 frame_data_.tile_items
    void ExtractPatches(World& world);  ///< Prepare：把单 patch 绘制项提取进 frame_data_.patch_items
    void BuildTileMesh(TerrainTileData& tile, const TerrainTileManagerComponent& mgr, int tile_x, int tile_z);
    void DestroyTileMeshGPU(TerrainTileData& tile);
    void GenerateProceduralTile(TerrainTileData& tile, const TerrainTileManagerComponent& mgr, int tx, int tz);
    void ShutdownTiles(World& world);

    RhiDevice* rhi_ = nullptr;
    dse::render::MeshRenderer mesh_renderer_;  ///< 前向 pass 通用网格渲染器（B2b-6 迁移）
    TerrainFrameRenderData frame_data_;        ///< Phase 1：主线程提取的每帧渲染快照
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_TERRAIN_SYSTEM_H
