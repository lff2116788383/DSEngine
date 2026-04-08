#include "catch/catch.hpp"
#include "modules/gameplay_2d/spine/spine_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"

#include <sstream>
#include <string>

using dse::gameplay2d::SpineSystem;

namespace {

struct SpineSmokeSnapshot {
    bool atlas_loaded = false;
    bool skeleton_data_loaded = false;
    bool skeleton_created = false;
    bool animation_state_created = false;
    bool dirty_animation_preserved = false;
    bool visible = false;

    std::string ToDebugString() const {
        std::ostringstream oss;
        oss << "SpineSmokeSnapshot{";
        oss << "atlas_loaded=" << atlas_loaded;
        oss << ", skeleton_data_loaded=" << skeleton_data_loaded;
        oss << ", skeleton_created=" << skeleton_created;
        oss << ", animation_state_created=" << animation_state_created;
        oss << ", dirty_animation_preserved=" << dirty_animation_preserved;
        oss << ", visible=" << visible;
        oss << "}";
        return oss.str();
    }
};

} // namespace

TEST_CASE("Smoke Snapshot - Spine missing assets remain safe and deterministic", "[engine][smoke][snapshot][spine]") {
    World world;
    auto entity = world.CreateEntity();
    auto& spine = world.registry().emplace<SpineRendererComponent>(entity);
    spine.skeleton_data_path = "missing/hero.skel";
    spine.atlas_path = "missing/hero.atlas";
    spine.current_animation = "idle";
    spine.dirty_animation = true;
    spine.visible = true;

    AssetManager asset_manager;
    asset_manager.ConfigureDataRoot("data");

    SpineSystem system;
    system.SetAssetManager(&asset_manager);
    system.Update(world.registry(), 1.0f / 60.0f);

    SpineSmokeSnapshot snapshot;
    snapshot.atlas_loaded = static_cast<bool>(spine.runtime);
    snapshot.skeleton_data_loaded = static_cast<bool>(spine.runtime);
    snapshot.skeleton_created = static_cast<bool>(spine.runtime);
    snapshot.animation_state_created = static_cast<bool>(spine.runtime);
    snapshot.dirty_animation_preserved = spine.dirty_animation;
    snapshot.visible = spine.visible;

    INFO(snapshot.ToDebugString());
    REQUIRE_FALSE(snapshot.atlas_loaded);
    REQUIRE_FALSE(snapshot.skeleton_data_loaded);
    REQUIRE_FALSE(snapshot.skeleton_created);
    REQUIRE_FALSE(snapshot.animation_state_created);
    REQUIRE(snapshot.dirty_animation_preserved);
    REQUIRE(snapshot.visible);
}
