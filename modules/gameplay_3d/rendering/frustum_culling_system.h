#ifndef DSE_FRUSTUM_CULLING_SYSTEM_H
#define DSE_FRUSTUM_CULLING_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/scene/octree.h"
#include <memory>

namespace dse::gameplay3d {

/**
 * @class FrustumCullingSystem
 * @brief 从 VSEngine2.1 的 VSCuller 提取的视锥体剔除系统。
 * 根据当前主相机的视锥体平面，剔除不在视野内的 MeshRendererComponent，以减少 DrawCall。
 */
class FrustumCullingSystem {
public:
    FrustumCullingSystem() = default;
    ~FrustumCullingSystem() = default;

    /**
     * @brief 执行视锥体剔除更新
     * @param world 当前的世界对象
     */
    void Update(World& world);

private:
    std::unique_ptr<scene::Octree> octree_;
};

} // namespace dse::gameplay3d

#endif // DSE_FRUSTUM_CULLING_SYSTEM_H
