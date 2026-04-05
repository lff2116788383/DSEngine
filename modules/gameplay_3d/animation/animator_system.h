#ifndef DSE_ANIMATOR_SYSTEM_H
#define DSE_ANIMATOR_SYSTEM_H

#include "engine/ecs/world.h"

class AssetManager;

namespace dse {
namespace gameplay3d {

class AnimatorSystem {
public:
    static void SetAssetManager(AssetManager* asset_manager);
    static void Update(World& world, float delta_time);

private:
    static AssetManager* asset_manager_;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_ANIMATOR_SYSTEM_H