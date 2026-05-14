#ifndef DSE_ANIM_LAYER_BLEND_SYSTEM_H
#define DSE_ANIM_LAYER_BLEND_SYSTEM_H

#include "engine/ecs/world.h"

class AssetManager;

namespace dse {
namespace gameplay3d {

class AnimLayerBlendSystem {
public:
    static void SetAssetManager(AssetManager* asset_manager);

    /// Evaluate all AnimLayerComponent layers and blend their results
    /// onto the base pose_buffer in Animator3DComponent.
    /// Must be called after AnimatorSystem::EvaluateBaseAnim and before
    /// AnimatorSystem::ComputeFinalMatrices.
    static void Update(World& world, float delta_time);

private:
    static AssetManager* asset_manager_;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_ANIM_LAYER_BLEND_SYSTEM_H
