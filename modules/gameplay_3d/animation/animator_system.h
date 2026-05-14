#ifndef DSE_ANIMATOR_SYSTEM_H
#define DSE_ANIMATOR_SYSTEM_H

#include "engine/ecs/world.h"

class AssetManager;

namespace dse {
namespace gameplay3d {

class AnimatorSystem {
public:
    static void SetAssetManager(AssetManager* asset_manager);

    /// Phase 1: Sample base animation → write pose_buffer + fire events.
    /// Builds / refreshes SkeletalCache on first call or dskel_path change.
    static void EvaluateBaseAnim(World& world, float delta_time);

    /// Phase 2: Convert pose_buffer → final_bone_matrices (global * inv(bind)).
    /// Must be called after EvaluateBaseAnim (and optionally after LayerBlend / IK).
    static void ComputeFinalMatrices(World& world);

    /// Convenience: calls EvaluateBaseAnim + ComputeFinalMatrices in sequence.
    /// Backwards-compatible with the old single-call API.
    static void Update(World& world, float delta_time);

private:
    static AssetManager* asset_manager_;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_ANIMATOR_SYSTEM_H