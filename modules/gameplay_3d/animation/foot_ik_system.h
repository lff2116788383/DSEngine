#ifndef DSE_FOOT_IK_SYSTEM_H
#define DSE_FOOT_IK_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
namespace gameplay3d {

class FootIKSystem {
public:
    /// Execute FootIK solving on all entities with Animator3DComponent + FootIK3DComponent.
    /// Must be called after EvaluateBaseAnim + LayerBlend and before ComputeFinalMatrices.
    /// Uses Physics Raycast for ground detection.
    /// Modifies pose_buffer in-place.
    static void Update(World& world, float delta_time);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_FOOT_IK_SYSTEM_H
