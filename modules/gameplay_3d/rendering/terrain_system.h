#ifndef DSE_TERRAIN_SYSTEM_H
#define DSE_TERRAIN_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include <glm/glm.hpp>

namespace dse {
namespace gameplay3d {

class TerrainSystem {
public:
    void Init(RhiDevice* rhi_device);
    void Shutdown(World& world);
    void Render(World& world, CommandBuffer& cmd_buffer);

    /// CPU 侧双线性插值高度查询（世界空间 xz → 高度 y）
    static float SampleHeight(const TerrainComponent& terrain,
                               const TransformComponent& transform,
                               float world_x, float world_z);

private:
    void RebuildTerrain(TerrainComponent& terrain);
    void DestroyTerrainGPU(TerrainComponent& terrain);

    RhiDevice* rhi_ = nullptr;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_TERRAIN_SYSTEM_H
