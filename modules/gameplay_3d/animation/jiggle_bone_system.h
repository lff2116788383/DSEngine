/**
 * @file jiggle_bone_system.h
 * @brief 次级骨骼动力学系统（弹簧骨 / 乳摇）。
 *
 * 在 EvaluateBaseAnim 之后、ComputeFinalMatrices 之前运行：读取 Animator3DComponent
 * 的 pose_buffer（局部 pose）与 skel_cache，对声明为弹簧骨的骨骼做模型空间 Verlet
 * 弹簧-阻尼积分，并把结果写回局部 pose 的旋转分量——这样后续的全局层级传播会自然
 * 带动子骨骼，且不破坏 final = global * inv_bind 的计算路径。
 */

#ifndef DSE_JIGGLE_BONE_SYSTEM_H
#define DSE_JIGGLE_BONE_SYSTEM_H

#include "engine/ecs/world.h"
#include <glm/glm.hpp>

namespace dse {
namespace gameplay3d {

/// 弹簧末端一步积分的输入（全部为模型空间）。抽成纯数据便于单元测试。
struct JiggleStepInput {
    glm::vec3 cur_tip;      ///< 当前末端位置
    glm::vec3 prev_tip;     ///< 上一帧末端位置
    glm::vec3 rest_tip;     ///< 动画驱动的静止末端位置
    glm::vec3 head_pos;     ///< 骨骼头位置（旋转支点）
    glm::vec3 gravity;      ///< 重力向量（单位/秒^2，模型空间）
    float stiffness = 0.08f;///< 回弹刚度 [0,1]
    float damping = 0.35f;  ///< 速度阻尼 [0,1]
    float length = 0.1f;    ///< 末端长度约束
};

/// Verlet 弹簧-阻尼积分一步，返回新的末端位置（已施加长度约束）。
/// 纯函数、无副作用，供 JiggleBoneSystem 与单元测试共用。
glm::vec3 JiggleIntegrate(const JiggleStepInput& in, float dt);

/// 次级骨骼动力学系统。见文件头说明。
class JiggleBoneSystem {
public:
    /// 对所有同时拥有 Animator3DComponent + JiggleBoneComponent 的实体做一步模拟。
    static void Update(World& world, float delta_time);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_JIGGLE_BONE_SYSTEM_H
