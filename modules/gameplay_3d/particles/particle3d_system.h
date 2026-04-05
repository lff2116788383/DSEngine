#ifndef DSE_PARTICLE3D_SYSTEM_H
#define DSE_PARTICLE3D_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include <memory>

class AssetManager;

namespace dse {
namespace gameplay3d {

class Particle3DSystem {
public:
    Particle3DSystem() = default;
    ~Particle3DSystem() = default;

    void Init(World& world, RhiDevice* rhi);
    void SetAssetManager(AssetManager* asset_manager);
    void Update(World& world, float delta_time);
    void Shutdown(World& world);

private:
    RhiDevice* rhi_ = nullptr;
    AssetManager* asset_manager_ = nullptr;
    
    // Internal helper
    void EmitParticle(struct ParticleSystem3DComponent& ps, const struct TransformComponent& transform);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_PARTICLE3D_SYSTEM_H