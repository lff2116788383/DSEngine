#ifndef DSE_TERRAIN_SYSTEM_H
#define DSE_TERRAIN_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"

namespace dse {
namespace gameplay3d {

class TerrainSystem {
public:
    void Render(World& world, CommandBuffer& cmd_buffer);
private:
    void RebuildTerrain(class TerrainComponent& terrain);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_TERRAIN_SYSTEM_H
