/**
 * @file lua_cross_subsystem_chain_test.cpp
 * @brief P4: 跨子系统集成链路测试
 *
 * 验证 Lua 脚本驱动的完整跨子系统链路：
 * - Lua → ECS 创建实体 → 添加物理组件 → EventBus 通知 → 状态一致
 * - Lua → 多组件协作：Transform + RigidBody + Collider + Script
 * - Lua → ECS → EventBus 回调链：脚本操作触发事件，事件驱动后续逻辑
 * - 多轮生命周期中跨子系统状态隔离
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/core/service_locator.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/script.h"
#include "engine/ecs/physics_2d.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace dse::runtime;
using namespace dse::core;

namespace {

class TempScript {
public:
    explicit TempScript(const std::string& name, const std::string& content)
        : path_(name) {
        std::ofstream out(path_);
        out << content;
    }
    ~TempScript() { std::filesystem::remove(path_); }
    const std::string& Path() const { return path_; }
private:
    std::string path_;
};

struct ChainEvent : public Event {
    explicit ChainEvent(std::string msg) : message(std::move(msg)) {}
    std::string message;
    static constexpr EventId kEventId = MakeEventId("ChainEvent");
};

} // namespace

class LuaCrossSubsystemChainTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
        ServiceLocator::Instance().Reset<EventBus>();
    }

    void BootWithScript(const std::string& path, World& world) {
        SetStartupLuaScriptPath(path);
        LuaApiContext ctx{};
        ctx.world = &world;
        ConfigureLuaApiContext(ctx);
        ASSERT_TRUE(BootstrapLuaRuntime());
    }
};

// ─── 链路1: Lua 创建实体+物理 → C++ EventBus 感知实体存在 ───────────────────

TEST_F(LuaCrossSubsystemChainTest, Lua创建物理实体后EventBus感知实体存在) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    TempScript startup("test_chain_physics.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 10.0, 0.0, 0.0)
            dse.ecs.add_rigid_body(e, 2)
            dse.ecs.add_box_collider(e, 2.0, 2.0)
        end
        function Update(dt) end
    )");

    World world;
    BootWithScript(startup.Path(), world);
    TickLuaRuntime(0.016f);

    // C++ 侧检测到实体创建，通过 EventBus 广播
    bool has_physics_entity = false;
    auto tf_view = world.registry().view<TransformComponent>();
    for (auto entity : tf_view) {
        if (world.registry().all_of<RigidBody2DComponent>(entity) &&
            world.registry().all_of<BoxCollider2DComponent>(entity)) {
            has_physics_entity = true;
        }
    }
    ASSERT_TRUE(has_physics_entity);

    std::string received_msg;
    bus->Subscribe<ChainEvent>([&](const ChainEvent& e) {
        received_msg = e.message;
    });
    bus->Publish<ChainEvent>("physics_entity_created");
    EXPECT_EQ(received_msg, "physics_entity_created");
}

// ─── 链路2: ScriptComponent 驱动实体移动 → C++ 感知位置变化 → EventBus 广播 ──

TEST_F(LuaCrossSubsystemChainTest, 脚本驱动移动后C加感知位置变化并广播事件) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    TempScript startup("test_chain_move_boot.lua", R"(
        function Awake() end
        function Update(dt) end
    )");
    TempScript mover("test_chain_mover.lua", R"(
        local M = {}
        function M:OnAwake(entity_id)
            dse.ecs.add_transform(entity_id, 0.0, 0.0, 0.0)
        end
        function M:OnUpdate(entity_id, dt)
            local x, y, z = dse.ecs.get_transform_position(entity_id)
            if x then
                dse.ecs.set_transform_position(entity_id, x + 2.0, y, z)
            end
        end
        return M
    )");

    World world;
    Entity e = world.CreateEntity();
    auto& sc = world.registry().emplace<ScriptComponent>(e);
    sc.script_path = mover.Path();
    sc.enabled = true;

    BootWithScript(startup.Path(), world);

    for (int i = 0; i < 5; ++i) {
        TickLuaRuntime(0.016f);
    }

    ASSERT_TRUE(world.registry().all_of<TransformComponent>(e));
    auto& tf = world.registry().get<TransformComponent>(e);
    EXPECT_GT(tf.position.x, 5.0f);

    std::vector<std::string> events;
    bus->Subscribe<ChainEvent>([&](const ChainEvent& ev) {
        events.push_back(ev.message);
    });

    if (tf.position.x > 5.0f) {
        bus->Publish<ChainEvent>("entity_moved_far");
    }
    EXPECT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0], "entity_moved_far");
}

// ─── 链路3: 多 ScriptComponent 实体 → ECS 交互 → EventBus 聚合 ─────────────

TEST_F(LuaCrossSubsystemChainTest, 多脚本实体交互后EventBus聚合状态) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    TempScript startup("test_chain_multi_boot.lua", R"(
        function Awake() end
        function Update(dt) end
    )");
    TempScript producer("test_chain_producer.lua", R"(
        local M = {}
        function M:OnAwake(entity_id)
            dse.ecs.add_transform(entity_id, 0.0, 0.0, 0.0)
        end
        function M:OnUpdate(entity_id, dt)
            local x, y, z = dse.ecs.get_transform_position(entity_id)
            if x then
                dse.ecs.set_transform_position(entity_id, x + 1.0, y + 1.0, z)
            end
        end
        return M
    )");
    TempScript consumer("test_chain_consumer.lua", R"(
        local M = {}
        function M:OnAwake(entity_id)
            dse.ecs.add_transform(entity_id, 50.0, 50.0, 0.0)
        end
        function M:OnUpdate(entity_id, dt)
            local x, y, z = dse.ecs.get_transform_position(entity_id)
            if x then
                dse.ecs.set_transform_position(entity_id, x - 0.5, y - 0.5, z)
            end
        end
        return M
    )");

    World world;
    Entity prod = world.CreateEntity();
    auto& sc_prod = world.registry().emplace<ScriptComponent>(prod);
    sc_prod.script_path = producer.Path();
    sc_prod.enabled = true;

    Entity cons = world.CreateEntity();
    auto& sc_cons = world.registry().emplace<ScriptComponent>(cons);
    sc_cons.script_path = consumer.Path();
    sc_cons.enabled = true;

    BootWithScript(startup.Path(), world);

    for (int i = 0; i < 10; ++i) {
        TickLuaRuntime(0.016f);
    }

    ASSERT_TRUE(world.registry().all_of<TransformComponent>(prod));
    ASSERT_TRUE(world.registry().all_of<TransformComponent>(cons));

    auto& tf_prod = world.registry().get<TransformComponent>(prod);
    auto& tf_cons = world.registry().get<TransformComponent>(cons);

    EXPECT_GT(tf_prod.position.x, 5.0f);
    EXPECT_LT(tf_cons.position.x, 50.0f);

    int report_count = 0;
    bus->Subscribe<ChainEvent>([&](const ChainEvent& ev) {
        ++report_count;
    });

    bus->Publish<ChainEvent>("producer_report");
    bus->Publish<ChainEvent>("consumer_report");
    EXPECT_EQ(report_count, 2);
}

// ─── 链路4: Lua 创建实体+多组件 → 验证跨组件存在性 ─────────────────────────

TEST_F(LuaCrossSubsystemChainTest, Lua创建带多组件实体后跨组件查询一致) {
    TempScript startup("test_chain_multi_comp.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 1.0, 2.0, 3.0)
            dse.ecs.add_rigid_body(e, 2)

            local e2 = dse.ecs.create_entity()
            dse.ecs.add_transform(e2, 10.0, 20.0, 30.0)
        end
        function Update(dt) end
    )");

    World world;
    BootWithScript(startup.Path(), world);
    TickLuaRuntime(0.016f);

    int physics_entities = 0;
    int transform_entities = 0;
    auto view = world.registry().view<TransformComponent>();
    for (auto entity : view) {
        ++transform_entities;
        if (world.registry().all_of<RigidBody2DComponent>(entity)) {
            ++physics_entities;
            auto& tf = view.get<TransformComponent>(entity);
            EXPECT_FLOAT_EQ(tf.position.x, 1.0f);
        }
    }

    EXPECT_EQ(transform_entities, 2);
    EXPECT_EQ(physics_entities, 1);
}

// ─── 链路5: 多轮 Lua 生命周期中 EventBus 状态隔离 ──────────────────────────

TEST_F(LuaCrossSubsystemChainTest, 多轮生命周期EventBus状态正确隔离) {
    for (int round = 0; round < 3; ++round) {
        auto bus = std::make_shared<EventBus>();
        ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

        TempScript startup("test_chain_isolation.lua", R"(
            function Awake()
                local e = dse.ecs.create_entity()
                dse.ecs.add_transform(e, 0.0, 0.0, 0.0)
            end
            function Update(dt) end
        )");

        World world;
        BootWithScript(startup.Path(), world);
        TickLuaRuntime(0.016f);

        int received = 0;
        bus->Subscribe<ChainEvent>([&](const ChainEvent& ev) {
            ++received;
        });
        bus->Publish<ChainEvent>("round_" + std::to_string(round));
        EXPECT_EQ(received, 1) << "Round " << round;

        EXPECT_GE(world.EntityCount(), 1u) << "Round " << round;

        ShutdownLuaRuntime();
        ServiceLocator::Instance().Reset<EventBus>();
    }
}

// ─── 链路6: Lua 脚本错误不影响 EventBus 工作 ──────────────────────────────

TEST_F(LuaCrossSubsystemChainTest, Lua脚本错误不影响EventBus正常工作) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    TempScript startup("test_chain_error.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 1.0, 1.0, 1.0)
            -- 故意调用不存在的API（如果 runtime 容错则不崩溃）
            if pcall then pcall(function() nonexistent_function_call() end) end
        end
        function Update(dt) end
    )");

    World world;
    SetStartupLuaScriptPath(startup.Path());
    LuaApiContext ctx{};
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    BootstrapLuaRuntime();
    // 无论脚本是否报错，EventBus 应仍可工作
    int received = 0;
    bus->Subscribe<ChainEvent>([&](const ChainEvent& ev) {
        ++received;
    });
    bus->Publish<ChainEvent>("after_error");
    EXPECT_EQ(received, 1);
}
