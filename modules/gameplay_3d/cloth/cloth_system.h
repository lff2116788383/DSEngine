#ifndef DSE_CLOTH_SYSTEM_H
#define DSE_CLOTH_SYSTEM_H

#include "engine/ecs/world.h"

class AssetManager;

namespace dse {
struct ClothComponent;
} // namespace dse

namespace dse {
namespace gameplay3d {

/**
 * @class ClothSystem
 * @brief XPBD 布料模拟系统
 *
 * 模拟循环（每次 FixedUpdate）：
 *   1. 施加外力（重力、风力）→ 预测位置
 *   2. XPBD 约束投影循环：
 *      a. 距离约束（拉伸）
 *      b. 弯曲约束（二面角）
 *      c. 碰撞约束（球体、胶囊）
 *   3. 从位置差量更新速度
 *   4. 施加阻尼
 *   5. 重计算法线并标记 mesh 脏标志
 */
class ClothSystem {
public:
    ClothSystem() = default;
    ~ClothSystem() = default;

    void SetAssetManager(AssetManager* asset_manager);

    /// 每个物理步调用（如 1/60s）
    void FixedUpdate(World& world, float dt);

    /// 从源 mesh 初始化 ClothComponent
    void InitializeCloth(World& world, entt::entity entity, ClothComponent& cloth);

private:
    AssetManager* asset_manager_ = nullptr;

    void PredictPositions(ClothComponent& cloth, float dt);
    void ProjectDistanceConstraints(ClothComponent& cloth);
    void ProjectBendConstraints(ClothComponent& cloth);
    void ProjectCollisionConstraints(World& world, ClothComponent& cloth);
    void UpdateVelocities(ClothComponent& cloth, float dt);
    void RecomputeNormals(ClothComponent& cloth);

    /// 从三角形网格拓扑构建距离 + 弯曲约束
    void BuildConstraints(ClothComponent& cloth);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_CLOTH_SYSTEM_H
