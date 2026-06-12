/**
 * @file scene_serialization_test.cpp
 * @brief Scene 序列化/反序列化、Prefab 综合单元测试
 */

#include <gtest/gtest.h>
#include "engine/scene/scene.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_foliage.h"
#include "engine/ecs/components_3d_navmesh.h"
#include "engine/ecs/components_3d_terrain_tile.h"
#include "engine/ecs/components_3d_tree.h"
#include <filesystem>
#include <string>
#include <cmath>

using namespace scene;

namespace {

constexpr float kEpsilon = 1e-4f;

bool NearEqual(float a, float b) { return std::fabs(a - b) < kEpsilon; }
bool NearEqual(const glm::vec3& a, const glm::vec3& b) {
    return NearEqual(a.x, b.x) && NearEqual(a.y, b.y) && NearEqual(a.z, b.z);
}
bool NearEqual(const glm::quat& a, const glm::quat& b) {
    return NearEqual(a.x, b.x) && NearEqual(a.y, b.y) &&
           NearEqual(a.z, b.z) && NearEqual(a.w, b.w);
}

const std::string kTestSceneFile  = "__test_scene_serialization_tmp.dscene";
const std::string kTestPrefabFile = "__test_prefab_tmp.dprefab";

struct ScopedFileCleanup {
    std::vector<std::string> paths;
    ~ScopedFileCleanup() {
        for (auto& p : paths)
            std::filesystem::remove(p);
    }
    void Add(const std::string& p) { paths.push_back(p); }
};

} // namespace

// ============================================================
// 1. 序列化/反序列化 往返
// ============================================================

TEST(SceneSerializationTest, TransformSerializationRoundTripConsistency) {
    ScopedFileCleanup cleanup;
    cleanup.Add(kTestSceneFile);

    {
        Scene src("src");
        auto& w = src.GetWorld();
        Entity e = w.CreateEntity();
        auto& t = w.registry().emplace<TransformComponent>(e);
        t.position = {1.5f, -2.5f, 3.5f};
        t.rotation = glm::quat(0.707f, 0.0f, 0.707f, 0.0f);
        t.scale    = {2.0f, 3.0f, 4.0f};
        ASSERT_TRUE(src.Serialize(kTestSceneFile));
    }

    Scene dst("dst");
    ASSERT_TRUE(dst.Deserialize(kTestSceneFile));
    EXPECT_EQ(dst.GetName(), "src");

    auto view = dst.GetWorld().registry().view<TransformComponent>();
    int count = 0;
    for (auto e : view) {
        auto& t = view.get<TransformComponent>(e);
        EXPECT_TRUE(NearEqual(t.position, {1.5f, -2.5f, 3.5f}));
        EXPECT_TRUE(NearEqual(t.scale, {2.0f, 3.0f, 4.0f}));
        ++count;
    }
    EXPECT_EQ(count, 1);
}

TEST(SceneSerializationTest, MultiEntityroundTrip) {
    ScopedFileCleanup cleanup;
    cleanup.Add(kTestSceneFile);

    {
        Scene src("multi");
        auto& w = src.GetWorld();

        for (int i = 0; i < 5; ++i) {
            Entity e = w.CreateEntity();
            auto& t = w.registry().emplace<TransformComponent>(e);
            t.position = {float(i), float(i * 2), float(i * 3)};
        }
        ASSERT_TRUE(src.Serialize(kTestSceneFile));
    }

    Scene dst("dst_multi");
    ASSERT_TRUE(dst.Deserialize(kTestSceneFile));

    auto view = dst.GetWorld().registry().view<TransformComponent>();
    int count = 0;
    for (auto e : view) {
        (void)e;
        ++count;
    }
    EXPECT_EQ(count, 5);
}

TEST(SceneSerializationTest, EmptySceneserializationdeserialization) {
    ScopedFileCleanup cleanup;
    cleanup.Add(kTestSceneFile);

    {
        Scene empty("empty");
        ASSERT_TRUE(empty.Serialize(kTestSceneFile));
    }

    Scene loaded("loaded_empty");
    ASSERT_TRUE(loaded.Deserialize(kTestSceneFile));
    EXPECT_EQ(loaded.GetName(), "empty");

    auto view = loaded.GetWorld().registry().view<TransformComponent>();
    int count = 0;
    for (auto it = view.begin(); it != view.end(); ++it) { ++count; }
    EXPECT_EQ(count, 0);
}

// ============================================================
// 2. 反序列化异常路径
// ============================================================

TEST(SceneSerializationTest, DoesNotExistReturnsfalse) {
    Scene s("nonexistent");
    EXPECT_FALSE(s.Deserialize("__nonexistent_file_that_does_not_exist.dscene"));
}

TEST(SceneSerializationTest, ToInvalidPathReturnsfalse) {
    Scene s("invalid_path");
    auto& w = s.GetWorld();
    Entity e = w.CreateEntity();
    w.registry().emplace<TransformComponent>(e);
    EXPECT_FALSE(s.Serialize("/___invalid___/no_such_dir/test.dscene"));
}

// ============================================================
// 3. Prefab 存储/实例化
// ============================================================

TEST(SceneSerializationTest, PrefabSaveAndLoadRoundTripsAreConsistent) {
    ScopedFileCleanup cleanup;
    cleanup.Add(kTestPrefabFile);

    World world;
    Entity root = world.CreateEntity();
    auto& t = world.registry().emplace<TransformComponent>(root);
    t.position = {10, 20, 30};
    t.scale = {2, 2, 2};

    ASSERT_TRUE(SaveEntityAsPrefab(world, root, kTestPrefabFile));

    World target_world;
    Entity instance = InstantiatePrefab(target_world, kTestPrefabFile);
    ASSERT_TRUE(instance != entt::null);
    ASSERT_TRUE(target_world.registry().all_of<TransformComponent>(instance));

    auto& inst_t = target_world.registry().get<TransformComponent>(instance);
    EXPECT_TRUE(NearEqual(inst_t.position, {10, 20, 30}));
    EXPECT_TRUE(NearEqual(inst_t.scale, {2, 2, 2}));
}

TEST(SceneSerializationTest, PrefabinstantiationOverridePosition) {
    ScopedFileCleanup cleanup;
    cleanup.Add(kTestPrefabFile);

    World world;
    Entity root = world.CreateEntity();
    auto& t = world.registry().emplace<TransformComponent>(root);
    t.position = {0, 0, 0};
    t.scale = {1, 1, 1};
    ASSERT_TRUE(SaveEntityAsPrefab(world, root, kTestPrefabFile));

    World target;
    PrefabInstantiateOptions opts;
    opts.override_position = true;
    opts.position = {100, 200, 300};
    Entity inst = InstantiatePrefab(target, kTestPrefabFile, opts);
    ASSERT_TRUE(inst != entt::null);

    auto& inst_t = target.registry().get<TransformComponent>(inst);
    EXPECT_TRUE(NearEqual(inst_t.position, {100, 200, 300}));
    EXPECT_TRUE(NearEqual(inst_t.scale, {1, 1, 1}));
}

TEST(SceneSerializationTest, PrefabinstantiationOverrideRotationAndScale) {
    ScopedFileCleanup cleanup;
    cleanup.Add(kTestPrefabFile);

    World world;
    Entity root = world.CreateEntity();
    auto& t = world.registry().emplace<TransformComponent>(root);
    t.position = {5, 5, 5};
    t.rotation = glm::quat(1, 0, 0, 0);
    t.scale = {1, 1, 1};
    ASSERT_TRUE(SaveEntityAsPrefab(world, root, kTestPrefabFile));

    World target;
    PrefabInstantiateOptions opts;
    opts.override_rotation = true;
    opts.rotation = glm::quat(0.707f, 0.707f, 0.0f, 0.0f);
    opts.override_scale = true;
    opts.scale = {3, 3, 3};
    Entity inst = InstantiatePrefab(target, kTestPrefabFile, opts);
    ASSERT_TRUE(inst != entt::null);

    auto& inst_t = target.registry().get<TransformComponent>(inst);
    EXPECT_TRUE(NearEqual(inst_t.position, {5, 5, 5}));
    EXPECT_TRUE(NearEqual(inst_t.rotation, {0.707f, 0.707f, 0.0f, 0.0f}));
    EXPECT_TRUE(NearEqual(inst_t.scale, {3, 3, 3}));
}

TEST(SceneSerializationTest, PrefabLoadingAFileThatDoesNotExistReturnsnull) {
    World w;
    Entity result = InstantiatePrefab(w, "__nonexistent_prefab_file.dprefab");
    EXPECT_TRUE(result == entt::null);
}

TEST(SceneSerializationTest, SaveEntityAsPrefabInvalidEntityReturnedfalse) {
    World w;
    EXPECT_FALSE(SaveEntityAsPrefab(w, entt::null, kTestPrefabFile));
}

// ============================================================
// 4. 父子层级 Prefab
// ============================================================

TEST(SceneSerializationTest, PrefabSaveAndRestoreParentChildHierarchy) {
    ScopedFileCleanup cleanup;
    cleanup.Add(kTestPrefabFile);

    World world;
    Entity parent = world.CreateEntity();
    auto& pt = world.registry().emplace<TransformComponent>(parent);
    pt.position = {0, 0, 0};

    Entity child = world.CreateEntity();
    auto& ct = world.registry().emplace<TransformComponent>(child);
    ct.position = {1, 2, 3};
    world.registry().emplace<ParentComponent>(child, ParentComponent{parent});

    ASSERT_TRUE(SaveEntityAsPrefab(world, parent, kTestPrefabFile));

    World target;
    Entity inst = InstantiatePrefab(target, kTestPrefabFile);
    ASSERT_TRUE(inst != entt::null);

    auto parent_view = target.registry().view<ParentComponent>();
    int child_count = 0;
    for (auto e : parent_view) {
        auto& pc = parent_view.get<ParentComponent>(e);
        EXPECT_TRUE(pc.parent != entt::null);
        ++child_count;
    }
    EXPECT_EQ(child_count, 1);
}

// ============================================================
// 5. 内置回归样例
// ============================================================

TEST(SceneSerializationTest, RoundTripRegressionExamplePassed) {
    ScopedFileCleanup cleanup;
    const std::string path = "__test_roundtrip_regression.dscene";
    cleanup.Add(path);
    EXPECT_TRUE(RunSceneRoundTripRegressionSample(path));
}

TEST(SceneSerializationTest, AndAfterComponentroundTrip) {
    ScopedFileCleanup cleanup;
    cleanup.Add(kTestSceneFile);

    dse::TreeComponent expected_tree;
    expected_tree.mesh_path = "trees/pine.dmesh";
    expected_tree.density = 0.05f;
    expected_tree.seed = 42;
    expected_tree.billboard_distance = 180.0f;

    dse::TerrainTileManagerComponent expected_ttm;
    expected_ttm.tile_world_size = 64.0f;
    expected_ttm.load_radius = 220.0f;
    expected_ttm.max_lod_levels = 5;
    expected_ttm.heightmap_pattern = "terrain/tile_{x}_{z}.raw";

    dse::DynamicObstacleComponent expected_obstacle;
    expected_obstacle.shape = dse::DynamicObstacleComponent::Shape::Cylinder;
    expected_obstacle.cylinder_radius = 1.5f;
    expected_obstacle.cylinder_height = 3.0f;

    dse::NavMeshAutoRebakeComponent expected_nav;
    expected_nav.tile_size = 48.0f;
    expected_nav.agent_radius = 0.7f;
    expected_nav.collect_mesh_renderers = false;

    dse::FoliageComponent expected_foliage;
    expected_foliage.wind_strength = 1.2f;
    expected_foliage.stiffness = 0.4f;

    dse::SubSceneComponent expected_subscene;
    expected_subscene.scene_path = "levels/forest_chunk.dscene";

    dse::PostProcessComponent expected_pp;
    expected_pp.ssao_enabled = true;
    expected_pp.ssao_sample_count = 24;
    expected_pp.bloom_knee = 0.2f;
    expected_pp.ssr_enabled = true;
    expected_pp.ssr_fade_distance = 0.35f;

    {
        Scene src("extended_components");
        auto& w = src.GetWorld();
        Entity e = w.CreateEntity();
        w.registry().emplace<TransformComponent>(e);
        w.registry().emplace<dse::TreeComponent>(e, expected_tree);
        w.registry().emplace<dse::TerrainTileManagerComponent>(e, expected_ttm);
        w.registry().emplace<dse::DynamicObstacleComponent>(e, expected_obstacle);
        w.registry().emplace<dse::NavMeshAutoRebakeComponent>(e, expected_nav);
        w.registry().emplace<dse::FoliageComponent>(e, expected_foliage);
        w.registry().emplace<dse::SubSceneComponent>(e, expected_subscene);
        w.registry().emplace<dse::PostProcessComponent>(e, expected_pp);
        ASSERT_TRUE(src.Serialize(kTestSceneFile));
    }

    Scene dst("extended_loaded");
    ASSERT_TRUE(dst.Deserialize(kTestSceneFile));

    auto view = dst.GetWorld().registry().view<
        dse::TreeComponent,
        dse::TerrainTileManagerComponent,
        dse::DynamicObstacleComponent,
        dse::NavMeshAutoRebakeComponent,
        dse::FoliageComponent,
        dse::SubSceneComponent,
        dse::PostProcessComponent>();
    ASSERT_EQ(view.size_hint(), 1u);

    Entity loaded = *view.begin();
    const auto& tree = view.get<dse::TreeComponent>(loaded);
    const auto& ttm = view.get<dse::TerrainTileManagerComponent>(loaded);
    const auto& obstacle = view.get<dse::DynamicObstacleComponent>(loaded);
    const auto& nav = view.get<dse::NavMeshAutoRebakeComponent>(loaded);
    const auto& foliage = view.get<dse::FoliageComponent>(loaded);
    const auto& subscene = view.get<dse::SubSceneComponent>(loaded);
    const auto& pp = view.get<dse::PostProcessComponent>(loaded);

    EXPECT_EQ(tree.mesh_path, expected_tree.mesh_path);
    EXPECT_NEAR(tree.density, expected_tree.density, kEpsilon);
    EXPECT_EQ(tree.seed, expected_tree.seed);
    EXPECT_NEAR(tree.billboard_distance, expected_tree.billboard_distance, kEpsilon);

    EXPECT_NEAR(ttm.tile_world_size, expected_ttm.tile_world_size, kEpsilon);
    EXPECT_NEAR(ttm.load_radius, expected_ttm.load_radius, kEpsilon);
    EXPECT_EQ(ttm.max_lod_levels, expected_ttm.max_lod_levels);
    EXPECT_EQ(ttm.heightmap_pattern, expected_ttm.heightmap_pattern);
    EXPECT_TRUE(ttm.tiles.empty());

    EXPECT_EQ(obstacle.shape, expected_obstacle.shape);
    EXPECT_NEAR(obstacle.cylinder_radius, expected_obstacle.cylinder_radius, kEpsilon);
    EXPECT_NEAR(obstacle.cylinder_height, expected_obstacle.cylinder_height, kEpsilon);
    EXPECT_EQ(obstacle.obstacle_ref_, 0u);
    EXPECT_TRUE(obstacle.dirty_);

    EXPECT_NEAR(nav.tile_size, expected_nav.tile_size, kEpsilon);
    EXPECT_NEAR(nav.agent_radius, expected_nav.agent_radius, kEpsilon);
    EXPECT_EQ(nav.collect_mesh_renderers, expected_nav.collect_mesh_renderers);
    EXPECT_TRUE(nav.needs_full_rebake_);

    EXPECT_NEAR(foliage.wind_strength, expected_foliage.wind_strength, kEpsilon);
    EXPECT_NEAR(foliage.stiffness, expected_foliage.stiffness, kEpsilon);

    EXPECT_EQ(subscene.scene_path, expected_subscene.scene_path);

    EXPECT_TRUE(pp.ssao_enabled);
    EXPECT_EQ(pp.ssao_sample_count, expected_pp.ssao_sample_count);
    EXPECT_NEAR(pp.bloom_knee, expected_pp.bloom_knee, kEpsilon);
    EXPECT_TRUE(pp.ssr_enabled);
    EXPECT_NEAR(pp.ssr_fade_distance, expected_pp.ssr_fade_distance, kEpsilon);
}

TEST(SceneSerializationTest, TowardAfterRegressionExamplePassed) {
    ScopedFileCleanup cleanup;
    const std::string path = "__test_backward_compat_regression.dscene";
    cleanup.Add(path);
    EXPECT_TRUE(RunSceneBackwardCompatibilityRegressionSample(path));
}
