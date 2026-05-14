#ifndef DSE_IK_SOLVER_SYSTEM_H
#define DSE_IK_SOLVER_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
namespace gameplay3d {

class IKSolverSystem {
public:
    /// Execute IK solving on all entities with Animator3DComponent + IKChain3DComponent.
    /// Must be called after EvaluateBaseAnim + LayerBlend and before ComputeFinalMatrices.
    /// Modifies pose_buffer in-place.
    static void Update(World& world, float delta_time);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_IK_SOLVER_SYSTEM_H
