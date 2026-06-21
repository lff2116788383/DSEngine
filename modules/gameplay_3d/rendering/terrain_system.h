#ifndef DSE_TERRAIN_SYSTEM_H
#define DSE_TERRAIN_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/mesh_renderer.h"
#include "engine/ecs/components_3d_terrain_tile.h"
#include <glm/glm.hpp>

namespace dse {
namespace gameplay3d {

class TerrainSystem {
public:
    void Init(RhiDevice* rhi_device);
    void Shutdown(World& world);
    /// depth_only=true（PreZ/Shadow 深度 RT）走 MeshRenderer::DrawDepthOnlySharedTemplateInstanced，false（Opaque 彩色）走 MeshRenderer::DrawSharedTemplateInstanced。
    void Render(World& world, CommandBuffer& cmd_buffer, const glm::vec3& camera_offset = glm::vec3(0.0f),
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
    void RenderTiles(World& world, CommandBuffer& cmd_buffer, const glm::vec3& camera_offset,
                     bool depth_only);
    void BuildTileMesh(TerrainTileData& tile, const TerrainTileManagerComponent& mgr, int tile_x, int tile_z);
    void DestroyTileMeshGPU(TerrainTileData& tile);
    void GenerateProceduralTile(TerrainTileData& tile, const TerrainTileManagerComponent& mgr, int tx, int tz);
    void ShutdownTiles(World& world);

    RhiDevice* rhi_ = nullptr;
    dse::render::MeshRenderer mesh_renderer_;  ///< 前向 pass 通用网格渲染器（B2b-6 迁移）
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_TERRAIN_SYSTEM_H
