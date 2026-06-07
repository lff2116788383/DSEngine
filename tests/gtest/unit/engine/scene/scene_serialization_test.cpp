/**
 * @file scene_serialization_test.cpp
 * @brief Scene 序列化/反序列化、Prefab 综合单元测试
 */

#include <gtest/gtest.h>
#include "engine/scene/scene.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
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

TEST(SceneSerializationTest, Transform序列化往返一致) {
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

TEST(SceneSerializationTest, 多实体序列化往返) {
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

TEST(SceneSerializationTest, 空场景序列化反序列化) {
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

TEST(SceneSerializationTest, 反序列化不存在的文件返回false) {
    Scene s("nonexistent");
    EXPECT_FALSE(s.Deserialize("__nonexistent_file_that_does_not_exist.dscene"));
}

TEST(SceneSerializationTest, 序列化到无效路径返回false) {
    Scene s("invalid_path");
    auto& w = s.GetWorld();
    Entity e = w.CreateEntity();
    w.registry().emplace<TransformComponent>(e);
    EXPECT_FALSE(s.Serialize("/___invalid___/no_such_dir/test.dscene"));
}

// ============================================================
// 3. Prefab 存储/实例化
// ============================================================

TEST(SceneSerializationTest, Prefab保存与加载往返一致) {
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

TEST(SceneSerializationTest, Prefab实例化覆盖Position) {
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

TEST(SceneSerializationTest, Prefab实例化覆盖Rotation和Scale) {
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

TEST(SceneSerializationTest, Prefab加载不存在的文件返回null) {
    World w;
    Entity result = InstantiatePrefab(w, "__nonexistent_prefab_file.dprefab");
    EXPECT_TRUE(result == entt::null);
}

TEST(SceneSerializationTest, SaveEntityAsPrefab无效实体返回false) {
    World w;
    EXPECT_FALSE(SaveEntityAsPrefab(w, entt::null, kTestPrefabFile));
}

// ============================================================
// 4. 父子层级 Prefab
// ============================================================

TEST(SceneSerializationTest, Prefab保存恢复父子层级) {
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

TEST(SceneSerializationTest, RoundTrip回归样例通过) {
    ScopedFileCleanup cleanup;
    const std::string path = "__test_roundtrip_regression.dscene";
    cleanup.Add(path);
    EXPECT_TRUE(RunSceneRoundTripRegressionSample(path));
}

TEST(SceneSerializationTest, 向后兼容回归样例通过) {
    ScopedFileCleanup cleanup;
    const std::string path = "__test_backward_compat_regression.dscene";
    cleanup.Add(path);
    EXPECT_TRUE(RunSceneBackwardCompatibilityRegressionSample(path));
}
