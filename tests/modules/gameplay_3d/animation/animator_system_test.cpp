#include "catch/catch.hpp"
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/importer.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/world.h"
#include "modules/gameplay_3d/animation/animator_system.h"

#include <filesystem>
#include <fstream>

using namespace dse;
using dse::asset::compiler::RawAnimation;
using dse::asset::compiler::RawAnimationChannel;
using dse::asset::compiler::RawBone;
using dse::asset::compiler::RawSceneData;
using dse::asset::compiler::MeshCooker;

namespace {

std::filesystem::path MakeAnimatorTempDir() {
    auto dir = std::filesystem::temp_directory_path() / "dse_animator_system_tests";
    std::filesystem::create_directories(dir);
    return dir;
}

struct ScopedFileCleanup {
    explicit ScopedFileCleanup(std::filesystem::path p) : path(std::move(p)) {}
    ~ScopedFileCleanup() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    std::filesystem::path path;
};

RawSceneData BuildAnimatorScene() {
    RawSceneData scene;

    RawBone root;
    root.name = "root";
    root.parent_index = -1;
    root.local_transform = glm::mat4(1.0f);
    root.inverse_bind_matrix = glm::mat4(1.0f);
    scene.skeleton.push_back(root);

    RawAnimation idle;
    idle.name = "idle";
    idle.duration = 1.0f;
    RawAnimationChannel idle_channel;
    idle_channel.target_node_index = 0;
    idle_channel.time_keys = {0.0f, 1.0f};
    idle_channel.position_keys = {glm::vec3(0.0f), glm::vec3(2.0f, 0.0f, 0.0f)};
    idle_channel.rotation_keys = {
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
    };
    idle_channel.scale_keys = {glm::vec3(1.0f), glm::vec3(1.0f)};
    idle.channels.push_back(idle_channel);
    scene.animations.push_back(idle);

    RawAnimation run;
    run.name = "run";
    run.duration = 1.0f;
    RawAnimationChannel run_channel;
    run_channel.target_node_index = 0;
    run_channel.time_keys = {0.0f, 1.0f};
    run_channel.position_keys = {glm::vec3(10.0f, 0.0f, 0.0f), glm::vec3(14.0f, 0.0f, 0.0f)};
    run_channel.rotation_keys = {
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::quat(1.0f, 0.0f, 0.0f, 0.0f)
    };
    run_channel.scale_keys = {glm::vec3(1.0f), glm::vec3(1.0f)};
    run.channels.push_back(run_channel);
    scene.animations.push_back(run);

    return scene;
}


} // namespace

// 正向测试：默认 Animator3DComponent 状态应为禁用高级树且拥有默认参数。
TEST_CASE("Given_DefaultAnimator3DComponent_When_Created_Then_AnimTreeIsDisabled", "[engine][unit][animator3d]") {
    Animator3DComponent animator;
    REQUIRE(animator.enabled == true);
    REQUIRE(animator.use_anim_tree == false);
    REQUIRE(animator.blend_nodes.empty());
    REQUIRE(animator.speed == 1.0f);
    REQUIRE(animator.blend_parameter == "speed");
    REQUIRE(animator.blend_parameter_value == Approx(0.0f));
}

// 正向测试：配置使用 AnimTree 的实体，状态应被正确记录。
TEST_CASE("Given_AnimTreeEnabled_When_AddingBlendNodes_Then_NodesAreStored", "[engine][unit][animator3d]") {
    Animator3DComponent animator;
    animator.use_anim_tree = true;
    animator.blend_parameter = "locomotion_speed";
    animator.blend_parameter_value = 2.5f;

    AnimBlendNode node1;
    node1.name = "walk";
    node1.danim_path = "walk.danim";
    node1.weight = 0.8f;
    node1.threshold = 1.0f;

    AnimBlendNode node2;
    node2.name = "run";
    node2.danim_path = "run.danim";
    node2.weight = 0.2f;
    node2.threshold = 4.0f;

    animator.blend_nodes.push_back(node1);
    animator.blend_nodes.push_back(node2);

    REQUIRE(animator.use_anim_tree == true);
    REQUIRE(animator.blend_parameter == "locomotion_speed");
    REQUIRE(animator.blend_parameter_value == Approx(2.5f));
    REQUIRE(animator.blend_nodes.size() == 2);
    REQUIRE(animator.blend_nodes[0].name == "walk");
    REQUIRE(animator.blend_nodes[0].weight == 0.8f);
    REQUIRE(animator.blend_nodes[0].threshold == Approx(1.0f));
    REQUIRE(animator.blend_nodes[1].name == "run");
    REQUIRE(animator.blend_nodes[1].weight == 0.2f);
    REQUIRE(animator.blend_nodes[1].threshold == Approx(4.0f));
}

// 边界测试：空 AnimTree 节点列表
TEST_CASE("Given_EmptyAnimTree_When_Configured_Then_HandledSafely", "[engine][unit][animator3d]") {
    Animator3DComponent animator;
    animator.use_anim_tree = true;
    animator.blend_nodes.clear();

    REQUIRE(animator.blend_nodes.empty());
}

TEST_CASE("Given_InvalidSkeletonAsset_When_AnimatorSystemUpdates_Then_FinalBoneMatricesStayEmpty", "[engine][unit][animator3d]") {
    World world;
    AssetManager asset_manager;
    gameplay3d::AnimatorSystem::SetAssetManager(&asset_manager);

    auto entity = world.CreateEntity();
    auto& animator = world.registry().emplace<Animator3DComponent>(entity);
    animator.dskel_path = "missing/test.dskel";
    animator.danim_path = "missing/test.danim";

    gameplay3d::AnimatorSystem::Update(world, 1.0f / 60.0f);

    REQUIRE(animator.final_bone_matrices.empty());
    gameplay3d::AnimatorSystem::SetAssetManager(nullptr);
}

TEST_CASE("Given_LegacyBlendTreeNodes_When_BlendValueBetweenThresholds_Then_RuntimeOutputsInterpolatedBonePalette", "[engine][unit][animator3d]") {
    const auto dir = MakeAnimatorTempDir();
    const auto dskel_path = dir / "animator_runtime.dskel";
    const auto idle_path = dir / "animator_runtime_idle.danim";
    const auto run_path = dir / "animator_runtime_run.danim";
    ScopedFileCleanup cleanup_skel(dskel_path);
    ScopedFileCleanup cleanup_idle(idle_path);
    ScopedFileCleanup cleanup_run(run_path);

    MeshCooker cooker;
    RawSceneData scene = BuildAnimatorScene();
    REQUIRE(cooker.CookToDskel(scene, dir.string(), "animator_runtime"));
    REQUIRE(cooker.CookToDanim(scene, dir.string(), "animator_runtime"));

    AssetManager asset_manager;
    asset_manager.ConfigureDataRoot(dir.generic_string());
    gameplay3d::AnimatorSystem::SetAssetManager(&asset_manager);

    World world;
    auto entity = world.CreateEntity();
    auto& animator = world.registry().emplace<Animator3DComponent>(entity);
    animator.dskel_path = dskel_path.filename().generic_string();
    animator.use_anim_tree = true;
    animator.blend_parameter_value = 0.5f;
    animator.blend_nodes.push_back({"idle", idle_path.filename().generic_string(), 1.0f, 0.0f});
    animator.blend_nodes.push_back({"run", run_path.filename().generic_string(), 1.0f, 1.0f});

    gameplay3d::AnimatorSystem::Update(world, 0.5f);

    REQUIRE(animator.final_bone_matrices.size() == 1);
    const glm::vec3 blended_position = glm::vec3(animator.final_bone_matrices[0][3]);
    REQUIRE(blended_position.x == Approx(0.0f).margin(0.01f));
    REQUIRE(animator.blend_nodes[0].current_time == Approx(1.0f));
    REQUIRE(animator.blend_nodes[1].current_time == Approx(1.0f));
    gameplay3d::AnimatorSystem::SetAssetManager(nullptr);
}

TEST_CASE("Given_LegacyBlendTreeNodesWithoutUsableAnimations_When_AnimatorSystemUpdates_Then_BindPoseIsPreserved", "[engine][unit][animator3d]") {
    const auto dir = MakeAnimatorTempDir();
    const auto dskel_path = dir / "animator_missing_blend.dskel";
    ScopedFileCleanup cleanup_skel(dskel_path);

    MeshCooker cooker;
    RawSceneData scene = BuildAnimatorScene();
    REQUIRE(cooker.CookToDskel(scene, dir.string(), "animator_missing_blend"));

    AssetManager asset_manager;
    asset_manager.ConfigureDataRoot(dir.generic_string());
    gameplay3d::AnimatorSystem::SetAssetManager(&asset_manager);

    World world;
    auto entity = world.CreateEntity();
    auto& animator = world.registry().emplace<Animator3DComponent>(entity);
    animator.dskel_path = dskel_path.filename().generic_string();
    animator.use_anim_tree = true;
    animator.blend_parameter_value = 0.5f;
    animator.blend_nodes.push_back({"missing_idle", "missing_idle.danim", 1.0f, 0.0f});
    animator.blend_nodes.push_back({"missing_run", "missing_run.danim", 1.0f, 1.0f});

    gameplay3d::AnimatorSystem::Update(world, 0.5f);

    REQUIRE(animator.final_bone_matrices.size() == 1);
    const glm::vec3 bind_pose_position = glm::vec3(animator.final_bone_matrices[0][3]);
    REQUIRE(bind_pose_position.x == Approx(0.0f));
    REQUIRE(bind_pose_position.y == Approx(0.0f));
    REQUIRE(bind_pose_position.z == Approx(0.0f));
    gameplay3d::AnimatorSystem::SetAssetManager(nullptr);
}
