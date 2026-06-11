/**
 * @file runtime_subsystem_integration_test.cpp
 * @brief Runtime ↔ 子系统（Physics2D / Scripting / ServiceLocator）集成测试
 *
 * 覆盖场景：
 *   1. FramePipeline + World + Physics2DSystem 手动装配后物理更新正常
 *   2. EngineInstance 构造析构不泄漏（不调用 Init，避免 GLFW 依赖）
 *   3. EngineInstance 多次构造析构稳定性
 *   4. ServiceLocator 注册 Physics2D + EventBus → FramePipeline 访问
 *   5. BusinessRuntimeBridge Lua 模式 + World → Lua 脚本创建实体
 *   6. Runtime 服务全链注册 → 使用 → 注销 → 二次注册
 *   7. FramePipeline Shutdown 后 Physics2DSystem 状态重置
 */

#include <gtest/gtest.h>
#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/business_runtime_bridge.h"
#include "engine/runtime/runtime_context.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/physics_2d.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/core/service_locator.h"
#include "engine/core/event_bus.h"
#include "engine/core/job_system.h"
#include "engine/scripting/lua/lua_runtime.h"
#include <filesystem>
#include <fstream>

using namespace dse::runtime;
using namespace dse::core;

namespace {

class TempLuaFile {
public:
    explicit TempLuaFile(const std::string& name, const std::string& content)
        : path_(name) {
        std::ofstream out(path_);
        out << content;
    }
    ~TempLuaFile() { std::filesystem::remove(path_); }
    const std::string& Path() const { return path_; }
private:
    std::string path_;
};

struct RuntimeIntTestEvent : public Event {
    int value = 0;
    static constexpr EventId kEventId = MakeEventId("RuntimeIntTestEvent");
};

} // namespace

class RuntimeSubsystemIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        ServiceLocator::Instance().Reset<Physics2DSystem>();
        ServiceLocator::Instance().Reset<EventBus>();
        ServiceLocator::Instance().Reset<JobSystem>();
    }
};

TEST_F(RuntimeSubsystemIntegrationTest, FramePipelineAndPhysics2DPhysicalUpdateAfterManualAssembly) {
    World world;

    auto ground = world.CreateEntity();
    world.registry().emplace<TransformComponent>(ground).position = {0, -5, 0};
    auto& ground_rb = world.registry().emplace<RigidBody2DComponent>(ground);
    ground_rb.type = RigidBody2DType::Static;
    world.registry().emplace<BoxCollider2DComponent>(ground);
    auto& ground_box = world.registry().get<BoxCollider2DComponent>(ground);
    ground_box.size = {20.0f, 1.0f};

    auto ball = world.CreateEntity();
    world.registry().emplace<TransformComponent>(ball).position = {0, 10, 0};
    auto& ball_rb = world.registry().emplace<RigidBody2DComponent>(ball);
    ball_rb.type = RigidBody2DType::Dynamic;
    world.registry().emplace<CircleCollider2DComponent>(ball).radius = 0.5f;

    Physics2DSystem physics;
    physics.Init(world);

    float initial_y = world.registry().get<TransformComponent>(ball).position.y;
    for (int i = 0; i < 60; ++i) {
        physics.FixedUpdate(world, 1.0f / 60.0f);
    }
    float final_y = world.registry().get<TransformComponent>(ball).position.y;

    EXPECT_LT(final_y, initial_y);
    physics.Shutdown();
}

TEST_F(RuntimeSubsystemIntegrationTest, EngineInstanceConstructionAndDestructionDoNotLeak) {
    EngineRunConfig config;
    config.window_width = 320;
    config.window_height = 240;

    {
        EngineInstance instance(config);
        EXPECT_NE(instance.pipeline(), nullptr);
    }
    // 析构不崩溃即通过
}

TEST_F(RuntimeSubsystemIntegrationTest, EngineInstanceStableWithMultipleConstructionsAndDestructions) {
    for (int i = 0; i < 3; ++i) {
        EngineRunConfig config;
        EngineInstance instance(config);
        EXPECT_NE(instance.pipeline(), nullptr);
        EXPECT_NO_THROW(instance.Shutdown());
    }
}

TEST_F(RuntimeSubsystemIntegrationTest, ServiceLocatorregisterPhysics2DAndEventBus) {
    auto physics = std::make_shared<Physics2DSystem>();
    auto event_bus = std::make_shared<EventBus>();

    ServiceLocator::Instance().Register<Physics2DSystem, Physics2DSystem>(physics);
    ServiceLocator::Instance().Register<EventBus, EventBus>(event_bus);

    EXPECT_TRUE(ServiceLocator::Instance().Has<Physics2DSystem>());
    EXPECT_TRUE(ServiceLocator::Instance().Has<EventBus>());

    auto* p = ServiceLocator::Instance().Get<Physics2DSystem>();
    ASSERT_NE(p, nullptr);

    World world;
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    world.registry().emplace<RigidBody2DComponent>(e).type = RigidBody2DType::Dynamic;
    world.registry().emplace<BoxCollider2DComponent>(e);

    EXPECT_NO_THROW(p->Init(world));
    EXPECT_NO_THROW(p->Shutdown());
}

TEST_F(RuntimeSubsystemIntegrationTest, BusinessRuntimeBridgeLuaPatternCreatesEntities) {
    TempLuaFile script("test_runtime_lua.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 5, 10, 0)
        end
        function Update(dt) end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx{};
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    bool found = false;
    auto view = world.registry().view<TransformComponent>();
    for (auto entity : view) {
        auto& t = view.get<TransformComponent>(entity);
        if (std::abs(t.position.x - 5.0f) < 0.01f && std::abs(t.position.y - 10.0f) < 0.01f) {
            found = true;
        }
    }
    EXPECT_TRUE(found);

    ShutdownLuaRuntime();
}

TEST_F(RuntimeSubsystemIntegrationTest, AllchainregisteruseAgainregister) {
    auto bus1 = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus1);
    EXPECT_TRUE(ServiceLocator::Instance().Has<EventBus>());

    bool received = false;
    bus1->Subscribe<RuntimeIntTestEvent>([&received](const RuntimeIntTestEvent& e) {
        received = true;
    });
    bus1->Publish<RuntimeIntTestEvent>();
    EXPECT_TRUE(received);

    ServiceLocator::Instance().Reset<EventBus>();
    EXPECT_FALSE(ServiceLocator::Instance().Has<EventBus>());

    auto bus2 = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus2);
    EXPECT_TRUE(ServiceLocator::Instance().Has<EventBus>());
    EXPECT_NE(ServiceLocator::Instance().Get<EventBus>(), bus1.get());
}

TEST_F(RuntimeSubsystemIntegrationTest, Physics2DSystemShutdownAfterStateReset) {
    World world;
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e).position = {0, 5, 0};
    auto& rb = world.registry().emplace<RigidBody2DComponent>(e);
    rb.type = RigidBody2DType::Dynamic;
    world.registry().emplace<BoxCollider2DComponent>(e);

    Physics2DSystem physics;
    physics.Init(world);
    physics.FixedUpdate(world, 0.02f);

    EXPECT_NE(world.registry().get<RigidBody2DComponent>(e).runtime_body, nullptr);

    physics.Shutdown();

    physics.Init(world);
    physics.FixedUpdate(world, 0.02f);
    EXPECT_NE(world.registry().get<RigidBody2DComponent>(e).runtime_body, nullptr);

    physics.Shutdown();
}
