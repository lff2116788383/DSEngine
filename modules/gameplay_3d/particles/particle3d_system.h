#ifndef DSE_PARTICLE3D_SYSTEM_H
#define DSE_PARTICLE3D_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/per_in_flight_buffer.h"
#include <memory>
#include <cstdint>
#include <unordered_map>

class AssetManager;

namespace dse {
namespace gameplay3d {

// CPU 发射 + SSBO 渲染路径（ParticleSystem3DComponent）。
// 与 engine/render/particles/gpu_particle_system.h（GpuParticleComponent，
// 全 GPU compute + indirect draw）按组件互斥选择：同一实体二选一。
// 桌面端（有 compute）优先 GPU 路径；WebGL2 / 无 compute 后端走本路径。
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

    // N3：每实例 SSBO 每帧 host 写，改 per-in-flight ring。ring 由系统按 entity 持有
    // （不放进 ECS 组件——组件会被编辑器快照拷贝，ring 拥有 GPU 句柄不可拷贝共享）。
    std::unordered_map<std::uint32_t, dse::render::PerInFlightBuffer> instance_rings_;

    // Internal helper
    void EmitParticle(ParticleSystem3DComponent& ps, const TransformComponent& transform);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_PARTICLE3D_SYSTEM_H