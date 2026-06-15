/**
 * @file game_application_test.cpp
 * @brief GameApplication 单元测试
 *
 * 覆盖场景：
 * - 生命周期钩子调用顺序与次数
 * - ECS 基础操作 (CreateEntity/DestroyEntity/Emplace/Get/Has/Remove)
 * - 实体工厂 (CreateEntityAt/CreateCamera3D/CreateDirectionalLight/CreatePointLight/CreateMesh)
 * - Bootstrap 注入 World/AssetManager，Shutdown 清空指针
 *
 * 注意：Run() 需要 GLFW 环境，不在单元测试中覆盖。
 *       通过 TestableGameApplication 直接调用 Bootstrap 绕过窗口依赖。
 */

#include <gtest/gtest.h>
#include "engine/scripting/cpp/game_application.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"

using namespace dse;
using namespace dse::runtime;

// ─── 测试子类：暴露 protected 接口 + 记录生命周期调用 ───

class TestableGameApplication : public GameApplication {
public:
    int init_count = 0;
    int update_count = 0;
    int shutdown_count = 0;
    float last_dt = 0.0f;

    using GameApplication::Bootstrap;
    using GameApplication::Tick;
    using GameApplication::ShutdownInternal;

    using GameApplication::CreateEntity;
    using GameApplication::DestroyEntity;
    using GameApplication::Emplace;
    using GameApplication::Get;
    using GameApplication::Has;
    using GameApplication::Remove;
    using GameApplication::CreateEntityAt;
    using GameApplication::CreateCamera3D;
    using GameApplication::CreateDirectionalLight;
    using GameApplication::CreatePointLight;
    using GameApplication::CreateMesh;
    using GameApplication::GetWorld;
    using GameApplication::GetAssetManager;

protected:
    void OnInit() override { ++init_count; }
    void OnUpdate(float dt) override { ++update_count; last_dt = dt; }
    void OnShutdown() override { ++shutdown_count; }
};

// ─── Fixture ───

class GameApplicationTest : public ::testing::Test {
protected:
    World world;
    AssetManager asset_manager;
    TestableGameApplication app;

    void SetUp() override {
        app.Bootstrap(world, asset_manager);
    }

    void TearDown() override {
        app.ShutdownInternal();
    }
};

// ═══════════════════════════════════════════════════
// 生命周期
// ═══════════════════════════════════════════════════

// 测试 游戏应用：引导调用开启初始化
TEST_F(GameApplicationTest, BootstrapCallsOnInit) {
    EXPECT_EQ(app.init_count, 1);
}

// 测试 游戏应用：滴答调用开启Updateand Passdt
TEST_F(GameApplicationTest, TickCallsOnUpdateandPassdt) {
    app.Tick(world, 0.016f);
    EXPECT_EQ(app.update_count, 1);
    EXPECT_FLOAT_EQ(app.last_dt, 0.016f);
}

// 测试 游戏应用：滴答Accumulate调用计数多个次数
TEST_F(GameApplicationTest, TickAccumulateCallCountMultipleTimes) {
    app.Tick(world, 0.016f);
    app.Tick(world, 0.033f);
    app.Tick(world, 0.008f);
    EXPECT_EQ(app.update_count, 3);
    EXPECT_FLOAT_EQ(app.last_dt, 0.008f);
}

// 测试 游戏应用：关闭调用开启关闭
TEST_F(GameApplicationTest, ShutdownCallsOnShutdown) {
    // TearDown 会调用 ShutdownInternal
    // 这里额外调用一次来检查
    app.ShutdownInternal();
    EXPECT_EQ(app.shutdown_count, 1);
}

// 测试 游戏应用：引导注入世界且资源管理器
TEST_F(GameApplicationTest, BootstrapInjectsWorldAndAssetManager) {
    EXPECT_EQ(&app.GetWorld(), &world);
    EXPECT_EQ(&app.GetAssetManager(), &asset_manager);
}

// ═══════════════════════════════════════════════════
// ECS 基础操作
// ═══════════════════════════════════════════════════

// 测试 游戏应用：创建实体返回有效实体
TEST_F(GameApplicationTest, CreateEntityReturnsAValidEntity) {
    Entity e = app.CreateEntity();
    EXPECT_TRUE(world.registry().valid(e));
}

// 测试 游戏应用：销毁实体为无效之后Destruction
TEST_F(GameApplicationTest, DestroyEntityEntityIsInvalidAfterDestruction) {
    Entity e = app.CreateEntity();
    app.DestroyEntity(e);
    EXPECT_FALSE(world.registry().valid(e));
}

// 测试 游戏应用：就地构造添加组件
TEST_F(GameApplicationTest, EmplaceAddComponent) {
    Entity e = app.CreateEntity();
    auto& t = app.Emplace<TransformComponent>(e);
    t.position = glm::vec3(1, 2, 3);
    EXPECT_TRUE(world.registry().all_of<TransformComponent>(e));
    auto& got = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(got.position.x, 1.0f);
    EXPECT_FLOAT_EQ(got.position.y, 2.0f);
    EXPECT_FLOAT_EQ(got.position.z, 3.0f);
}

// 测试 游戏应用：就地构造Replace已存在组件
TEST_F(GameApplicationTest, EmplaceReplaceExistingComponents) {
    Entity e = app.CreateEntity();
    app.Emplace<TransformComponent>(e).position = glm::vec3(1, 0, 0);
    app.Emplace<TransformComponent>(e).position = glm::vec3(9, 8, 7);
    auto& t = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(t.position.x, 9.0f);
}

// 测试 游戏应用：获取返回组件指针
TEST_F(GameApplicationTest, GetReturnComponentPointer) {
    Entity e = app.CreateEntity();
    app.Emplace<TransformComponent>(e).position = glm::vec3(5, 6, 7);
    auto* ptr = app.Get<TransformComponent>(e);
    ASSERT_NE(ptr, nullptr);
    EXPECT_FLOAT_EQ(ptr->position.y, 6.0f);
}

// 测试 游戏应用：获取返回无组件Entitiesnullptr
TEST_F(GameApplicationTest, GetReturnsForComponentlessEntitiesnullptr) {
    Entity e = app.CreateEntity();
    auto* ptr = app.Get<TransformComponent>(e);
    EXPECT_EQ(ptr, nullptr);
}

// 测试 游戏应用：获取返回无效Entitiesnullptr
TEST_F(GameApplicationTest, GetReturnsForInvalidEntitiesnullptr) {
    Entity e = app.CreateEntity();
    app.DestroyEntity(e);
    auto* ptr = app.Get<TransformComponent>(e);
    EXPECT_EQ(ptr, nullptr);
}

// 测试 游戏应用：拥有正确Determine组件存在
TEST_F(GameApplicationTest, HasCorrectlyDetermineComponentExistence) {
    Entity e = app.CreateEntity();
    EXPECT_FALSE(app.Has<TransformComponent>(e));
    app.Emplace<TransformComponent>(e);
    EXPECT_TRUE(app.Has<TransformComponent>(e));
}

// 测试 游戏应用：拥有返回无效Entitiesfalse
TEST_F(GameApplicationTest, HasReturnsForInvalidEntitiesfalse) {
    Entity e = app.CreateEntity();
    app.DestroyEntity(e);
    EXPECT_FALSE(app.Has<TransformComponent>(e));
}

// 测试 游戏应用：移除组件
TEST_F(GameApplicationTest, RemoveRemoveComponent) {
    Entity e = app.CreateEntity();
    app.Emplace<TransformComponent>(e);
    EXPECT_TRUE(app.Has<TransformComponent>(e));
    app.Remove<TransformComponent>(e);
    EXPECT_FALSE(app.Has<TransformComponent>(e));
}

// 测试 游戏应用：移除无崩溃无组件实体
TEST_F(GameApplicationTest, RemoveNoCrashForComponentlessEntities) {
    Entity e = app.CreateEntity();
    EXPECT_NO_THROW(app.Remove<TransformComponent>(e));
}

// ═══════════════════════════════════════════════════
// 实体工厂
// ═══════════════════════════════════════════════════

// 测试 游戏应用：创建实体Atset上位置且缩放
TEST_F(GameApplicationTest, CreateEntityAtsetUpPositionAndScale) {
    Entity e = app.CreateEntityAt(glm::vec3(10, 20, 30), glm::vec3(2, 3, 4));
    ASSERT_TRUE(world.registry().all_of<TransformComponent>(e));
    auto& t = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(t.position.x, 10.0f);
    EXPECT_FLOAT_EQ(t.position.y, 20.0f);
    EXPECT_FLOAT_EQ(t.position.z, 30.0f);
    EXPECT_FLOAT_EQ(t.scale.x, 2.0f);
    EXPECT_FLOAT_EQ(t.scale.y, 3.0f);
    EXPECT_FLOAT_EQ(t.scale.z, 4.0f);
    EXPECT_TRUE(t.dirty);
}

// 测试 游戏应用：创建实体Atdefault缩放为1
TEST_F(GameApplicationTest, CreateEntityAtdefaultScaleIs1) {
    Entity e = app.CreateEntityAt(glm::vec3(0));
    auto& t = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(t.scale.x, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.y, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.z, 1.0f);
}

// 测试 游戏应用：创建相机3D创建相机且释放Cam
TEST_F(GameApplicationTest, CreateCamera3DCreateCameraAndFreeCam) {
    Entity e = app.CreateCamera3D(glm::vec3(0, 5, 15), 90.0f, 0.5f, 500.0f);
    ASSERT_TRUE(world.registry().all_of<TransformComponent>(e));
    ASSERT_TRUE(world.registry().all_of<Camera3DComponent>(e));
    ASSERT_TRUE(world.registry().all_of<FreeCameraControllerComponent>(e));

    auto& t = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(t.position.y, 5.0f);
    EXPECT_FLOAT_EQ(t.position.z, 15.0f);

    auto& cam = world.registry().get<Camera3DComponent>(e);
    EXPECT_FLOAT_EQ(cam.fov, 90.0f);
    EXPECT_FLOAT_EQ(cam.near_clip, 0.5f);
    EXPECT_FLOAT_EQ(cam.far_clip, 500.0f);
    EXPECT_TRUE(cam.enabled);
}

// 测试 游戏应用：创建相机3D默认参数
TEST_F(GameApplicationTest, CreateCamera3DDefaultParameters) {
    Entity e = app.CreateCamera3D(glm::vec3(0));
    auto& cam = world.registry().get<Camera3DComponent>(e);
    EXPECT_FLOAT_EQ(cam.fov, 60.0f);
    EXPECT_FLOAT_EQ(cam.near_clip, 0.1f);
    EXPECT_FLOAT_EQ(cam.far_clip, 1000.0f);
}

// 测试 游戏应用：创建方向光灯光创建方向光灯光
TEST_F(GameApplicationTest, CreateDirectionalLightCreateDirectionalLights) {
    Entity e = app.CreateDirectionalLight(
        glm::vec3(-1, -1, 0), glm::vec3(1, 0.9f, 0.8f), 2.5f, false);
    ASSERT_TRUE(world.registry().all_of<DirectionalLight3DComponent>(e));
    auto& light = world.registry().get<DirectionalLight3DComponent>(e);
    EXPECT_FLOAT_EQ(light.intensity, 2.5f);
    EXPECT_FALSE(light.cast_shadow);
    EXPECT_FLOAT_EQ(light.color.r, 1.0f);
    EXPECT_FLOAT_EQ(light.color.g, 0.9f);
    // direction 已归一化
    EXPECT_NEAR(glm::length(light.direction), 1.0f, 1e-5f);
}

// 测试 游戏应用：创建方向光灯光默认参数
TEST_F(GameApplicationTest, CreateDirectionalLightDefaultParameters) {
    Entity e = app.CreateDirectionalLight(glm::vec3(0, -1, 0));
    auto& light = world.registry().get<DirectionalLight3DComponent>(e);
    EXPECT_FLOAT_EQ(light.intensity, 1.0f);
    EXPECT_TRUE(light.cast_shadow);
    EXPECT_FLOAT_EQ(light.color.r, 1.0f);
    EXPECT_FLOAT_EQ(light.color.g, 1.0f);
    EXPECT_FLOAT_EQ(light.color.b, 1.0f);
}

// 测试 游戏应用：创建点灯光创建点灯光
TEST_F(GameApplicationTest, CreatePointLightCreateAPointLight) {
    Entity e = app.CreatePointLight(
        glm::vec3(5, 10, 15), glm::vec3(1, 0, 0), 3.0f, 25.0f);
    ASSERT_TRUE(world.registry().all_of<TransformComponent>(e));
    ASSERT_TRUE(world.registry().all_of<PointLightComponent>(e));
    auto& t = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(t.position.x, 5.0f);
    auto& light = world.registry().get<PointLightComponent>(e);
    EXPECT_FLOAT_EQ(light.intensity, 3.0f);
    EXPECT_FLOAT_EQ(light.radius, 25.0f);
    EXPECT_FLOAT_EQ(light.color.r, 1.0f);
    EXPECT_FLOAT_EQ(light.color.g, 0.0f);
}

// 测试 游戏应用：创建点灯光默认参数
TEST_F(GameApplicationTest, CreatePointLightDefaultParameters) {
    Entity e = app.CreatePointLight(glm::vec3(0));
    auto& light = world.registry().get<PointLightComponent>(e);
    EXPECT_FLOAT_EQ(light.intensity, 1.0f);
    EXPECT_FLOAT_EQ(light.radius, 10.0f);
}

// 测试 游戏应用：创建网格创建网格实体
TEST_F(GameApplicationTest, CreateMeshCreateMeshEntities) {
    Entity e = app.CreateMesh(
        glm::vec3(1, 2, 3), "models/test.dmesh", glm::vec3(2));
    ASSERT_TRUE(world.registry().all_of<TransformComponent>(e));
    ASSERT_TRUE(world.registry().all_of<MeshRendererComponent>(e));
    auto& t = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(t.position.x, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.x, 2.0f);
    auto& mesh = world.registry().get<MeshRendererComponent>(e);
    EXPECT_EQ(mesh.mesh_path, "models/test.dmesh");
    EXPECT_EQ(mesh.shader_variant, "MESH_PBR");
    EXPECT_TRUE(mesh.visible);
    EXPECT_TRUE(mesh.receive_shadow);
}

// 测试 游戏应用：创建Meshdefault缩放为1
TEST_F(GameApplicationTest, CreateMeshdefaultScaleIs1) {
    Entity e = app.CreateMesh(glm::vec3(0), "models/cube.dmesh");
    auto& t = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(t.scale.x, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.y, 1.0f);
    EXPECT_FLOAT_EQ(t.scale.z, 1.0f);
}

// ═══════════════════════════════════════════════════
// 多实体综合场景
// ═══════════════════════════════════════════════════

// 测试 游戏应用：场景创建销毁
TEST_F(GameApplicationTest, Scene_CreateDestroy) {
    Entity cam = app.CreateCamera3D(glm::vec3(0, 5, 15));
    Entity sun = app.CreateDirectionalLight(glm::vec3(-1, -1, -1));
    Entity box1 = app.CreateMesh(glm::vec3(0), "models/cube.dmesh");
    Entity box2 = app.CreateMesh(glm::vec3(5, 0, 0), "models/sphere.dmesh");

    EXPECT_TRUE(world.registry().valid(cam));
    EXPECT_TRUE(world.registry().valid(sun));
    EXPECT_TRUE(world.registry().valid(box1));
    EXPECT_TRUE(world.registry().valid(box2));

    app.DestroyEntity(box1);
    EXPECT_FALSE(world.registry().valid(box1));
    EXPECT_TRUE(world.registry().valid(box2));
}
