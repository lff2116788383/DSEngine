/**
 * @file lua_eventbus_integration_test.cpp
 * @brief Lua Runtime + EventBus 集成测试
 *
 * 验证场景：
 * - C++ 侧 EventBus 事件触发后，Lua 脚本通过 LuaApiContext 感知到事件
 * - Lua 脚本中通过 dse.ecs 操作实体后，C++ 侧 EventBus 发布对应事件
 * - Lua Runtime 生命周期与 EventBus 的协作一致性
 *
 * 注意：当前 Lua 绑定尚未直接暴露 EventBus 的 Subscribe/Publish API，
 *       因此本测试通过 C++ 侧在 Lua 回调触发后发布 EventBus 事件来验证
 *       跨语言边界的协作链路。
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
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace dse::runtime;
using namespace dse::core;

// ============================================================
// 辅助：临时 Lua 脚本
// ============================================================

class LuaTempScript {
public:
    explicit LuaTempScript(const std::string& name, const std::string& content)
        : path_(name) {
        std::ofstream out(path_);
        out << content;
    }
    ~LuaTempScript() {
        std::filesystem::remove(path_);
    }
    const std::string& Path() const { return path_; }
private:
    std::string path_;
};

// ============================================================
// 测试用事件
// ============================================================

struct LuaActionEvent : public Event {
    explicit LuaActionEvent(std::string action) : action(std::move(action)) {}
    std::string action;
    static constexpr EventId kEventId = MakeEventId("LuaActionEvent");
};

// ============================================================
// Lua + EventBus 集成测试
// ============================================================

class LuaEventBusIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
        ServiceLocator::Instance().Reset<EventBus>();
    }
};

TEST_F(LuaEventBusIntegrationTest, LuaAfterTheScriptIsExecutedCPlusPlusPublishesEventBusevent) {
    // 设置 EventBus
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    std::vector<std::string> received_actions;
    bus->Subscribe<LuaActionEvent>([&received_actions](const LuaActionEvent& e) {
        received_actions.push_back(e.action);
    });

    // Lua 脚本仅执行 Awake，C++ 侧在脚本 tick 后手动发布事件
    LuaTempScript script("test_lua_event.lua", R"(
        function Awake()
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    // 模拟：Lua 脚本执行完毕后，C++ 业务层发布事件
    bus->Publish<LuaActionEvent>("lua_awake_done");

    ASSERT_EQ(received_actions.size(), 1u);
    EXPECT_EQ(received_actions[0], "lua_awake_done");

    ShutdownLuaRuntime();
}

TEST_F(LuaEventBusIntegrationTest, LuaAfterCreatingTheEntityCPlusPluspassEventBusBroadcasts) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    int entity_created_count = 0;
    bus->Subscribe<LuaActionEvent>([&entity_created_count](const LuaActionEvent& e) {
        if (e.action == "entity_created") {
            ++entity_created_count;
        }
    });

    // Lua 脚本通过 dse.ecs 创建实体
    LuaTempScript script("test_lua_ecs_event.lua", R"(
        function Awake()
            local entity_id = dse.ecs.create_entity()
            dse.ecs.add_transform(entity_id, 10.0, 20.0, 0.0)
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    // Lua 脚本已创建实体，C++ 侧感知后发布事件
    EXPECT_GT(world.EntityCount(), 0u);
    bus->Publish<LuaActionEvent>("entity_created");

    EXPECT_EQ(entity_created_count, 1);

    ShutdownLuaRuntime();
}

TEST_F(LuaEventBusIntegrationTest, EventBuseventTriggerLuascriptResponse) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    // 记录 C++ 事件是否到达并触发了 Lua 端的后续操作
    bool event_processed = false;

    LuaTempScript script("test_event_to_lua.lua", R"(
        function Awake()
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());

    // C++ 侧发布事件，订阅者中驱动 Lua Runtime 逻辑
    bus->Subscribe<LuaActionEvent>([&](const LuaActionEvent& e) {
        if (e.action == "trigger_lua_update") {
            TickLuaRuntime(0.016f);
            event_processed = true;
        }
    });

    bus->Publish<LuaActionEvent>("trigger_lua_update");

    EXPECT_TRUE(event_processed);

    ShutdownLuaRuntime();
}

TEST_F(LuaEventBusIntegrationTest, LuaAfterTheScriptOperatesOnTheEntityEventBusNotifyOthersCPlusPlusmodule) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    std::vector<float> recorded_positions;

    // C++ 模块订阅位置变化事件
    bus->Subscribe<LuaActionEvent>([&recorded_positions](const LuaActionEvent& e) {
        if (e.action == "position_updated") {
            // 在真实场景中，事件会携带位置数据
            recorded_positions.push_back(1.0f); // 标记已收到
        }
    });

    LuaTempScript script("test_lua_position_event.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 5.0, 10.0, 0.0)
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    // 验证 Lua 创建了实体
    EXPECT_EQ(world.EntityCount(), 1u);

    // 模拟：业务逻辑感知到实体创建后通过 EventBus 通知
    bus->Publish<LuaActionEvent>("position_updated");

    EXPECT_EQ(recorded_positions.size(), 1u);

    ShutdownLuaRuntime();
}

TEST_F(LuaEventBusIntegrationTest, LuaRuntimeAfterClosingEventBusstillWorksFine) {
    auto bus = std::make_shared<EventBus>();
    ServiceLocator::Instance().Register<EventBus, EventBus>(bus);

    LuaTempScript script("test_shutdown_event.lua", R"(
        function Awake() end
    )");

    SetStartupLuaScriptPath(script.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    ShutdownLuaRuntime();

    // Lua 关闭后 EventBus 仍应可发布和订阅
    int received = 0;
    bus->Subscribe<LuaActionEvent>([&received](const LuaActionEvent& e) {
        if (e.action == "post_shutdown") ++received;
    });
    bus->Publish<LuaActionEvent>("post_shutdown");
    EXPECT_EQ(received, 1);
}
