#include "catch/catch.hpp"
#include "engine/scene/scene.h"
#include "engine/ecs/components_2d.h"
#include <filesystem>
#include <fstream>

namespace {
std::string MakeTempPath(const char* name) {
    auto base = std::filesystem::temp_directory_path() / "dse_prefab_tests";
    std::filesystem::create_directories(base);
    return (base / name).string();
}

struct ScopedTempFile {
    explicit ScopedTempFile(std::string p) : path(std::move(p)) {}
    ~ScopedTempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    std::string path;
};
}

// 正向用例：保存嵌套 Prefab 后再实例化，应恢复父子层级和关键组件数据。
TEST_CASE("Given_NestedPrefab_When_SaveAndInstantiate_Then_HierarchyAndComponentsAreRestored", "[engine][unit][scene][prefab]") {
    World source_world;
    Entity root = source_world.CreateEntity();
    Entity child = source_world.CreateEntity();

    auto& root_tf = source_world.registry().emplace<TransformComponent>(root);
    root_tf.position = glm::vec3(1.0f, 2.0f, 0.0f);
    auto& root_sprite = source_world.registry().emplace<SpriteRendererComponent>(root);
    root_sprite.sorting_layer = 3;
    root_sprite.order_in_layer = 4;
    root_sprite.shader_variant = "SPRITE_TINT";

    auto& child_tf = source_world.registry().emplace<TransformComponent>(child);
    child_tf.position = glm::vec3(5.0f, 6.0f, 0.0f);
    source_world.registry().emplace<ParentComponent>(child, ParentComponent{root});
    auto& child_script = source_world.registry().emplace<ScriptComponent>(child);
    child_script.script_path = "script/ai/enemy.lua";
    auto& child_spine = source_world.registry().emplace<SpineRendererComponent>(child);
    child_spine.skeleton_data_path = "spine/hero.skel";
    child_spine.atlas_path = "spine/hero.atlas";
    child_spine.current_animation = "idle";
    child_spine.loop = true;

    const std::string prefab_path = MakeTempPath("nested_prefab.json");
    ScopedTempFile scoped_prefab_file(prefab_path);
    REQUIRE(scene::SaveEntityAsPrefab(source_world, root, prefab_path));

    World instance_world;
    scene::PrefabInstantiateOptions options;
    options.override_position = true;
    options.position = glm::vec3(100.0f, 200.0f, 0.0f);
    Entity instance_root = scene::InstantiatePrefab(instance_world, prefab_path, options);
    REQUIRE(static_cast<entt::id_type>(instance_root) != static_cast<entt::id_type>(entt::null));
    REQUIRE(instance_world.registry().all_of<TransformComponent>(instance_root));
    const auto& instance_root_tf = instance_world.registry().get<TransformComponent>(instance_root);
    REQUIRE(instance_root_tf.position.x == Approx(100.0f));
    REQUIRE(instance_root_tf.position.y == Approx(200.0f));
    REQUIRE(instance_world.registry().all_of<SpriteRendererComponent>(instance_root));

    Entity instance_child = entt::null;
    auto parent_view = instance_world.registry().view<ParentComponent>();
    for (auto entity : parent_view) {
        if (parent_view.get<ParentComponent>(entity).parent == instance_root) {
            instance_child = entity;
            break;
        }
    }
    REQUIRE(static_cast<entt::id_type>(instance_child) != static_cast<entt::id_type>(entt::null));
    REQUIRE(instance_world.registry().all_of<ScriptComponent>(instance_child));
    REQUIRE(instance_world.registry().all_of<SpineRendererComponent>(instance_child));
    const auto& child_script_inst = instance_world.registry().get<ScriptComponent>(instance_child);
    const auto& child_spine_inst = instance_world.registry().get<SpineRendererComponent>(instance_child);
    REQUIRE(child_script_inst.script_path == "script/ai/enemy.lua");
    REQUIRE(child_spine_inst.skeleton_data_path == "spine/hero.skel");
    REQUIRE(child_spine_inst.atlas_path == "spine/hero.atlas");
    REQUIRE(child_spine_inst.current_animation == "idle");
}

// 回归用例：旧版单实体 Prefab 格式仍可实例化，确保向后兼容。
TEST_CASE("Given_LegacySingleEntityPrefab_When_Instantiate_Then_BackwardCompatibilityWorks", "[engine][unit][scene][prefab]") {
    const std::string legacy_path = MakeTempPath("legacy_prefab.json");
    ScopedTempFile scoped_legacy_file(legacy_path);
    const char* json = R"({
        "type":"prefab",
        "prefab_schema_version":1,
        "components":{
            "TransformComponent":{
                "position":[7.0,8.0,0.0],
                "rotation":[0.0,0.0,0.0,1.0],
                "scale":[1.0,1.0,1.0]
            },
            "ScriptComponent":{
                "script_path":"script/player.lua",
                "enabled":true
            }
        }
    })";
    {
        std::ofstream out(legacy_path);
        REQUIRE(out.is_open());
        out << json;
    }

    World world;
    Entity e = scene::InstantiatePrefab(world, legacy_path);
    REQUIRE(static_cast<entt::id_type>(e) != static_cast<entt::id_type>(entt::null));
    REQUIRE(world.registry().all_of<TransformComponent>(e));
    REQUIRE(world.registry().all_of<ScriptComponent>(e));
    const auto& tf = world.registry().get<TransformComponent>(e);
    const auto& sc = world.registry().get<ScriptComponent>(e);
    REQUIRE(tf.position.x == Approx(7.0f));
    REQUIRE(tf.position.y == Approx(8.0f));
    REQUIRE(sc.script_path == "script/player.lua");
}
