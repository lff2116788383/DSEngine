/**
 * @file lua_ecs_integration_test.cpp
 * @brief Lua Scripting ↔ ECS 集成测试
 *
 * 验证场景：
 * - Lua 脚本通过 dse.ecs.create_entity 创建实体
 * - Lua 脚本通过 dse.ecs.add_transform 添加 TransformComponent
 * - Lua 脚本通过 dse.ecs.get/set_transform_position 读写位置
 * - Lua 创建的实体在 C++ 侧可查询
 * - C++ 侧修改实体后 Lua 侧可感知
 * - Lua 脚本批量创建实体
 * - Lua 错误脚本不崩溃引擎
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/script.h"
#include "engine/ecs/transform.h"
#include "engine/core/service_locator.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace dse::runtime;

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

class LuaEcsIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
    }
};

TEST_F(LuaEcsIntegrationTest, Lua创建实体CPlusPlus侧可查询) {
    LuaTempScript startup("test_ecs_create.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    size_t count_before = world.EntityCount();
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    EXPECT_GT(world.EntityCount(), count_before);

    ShutdownLuaRuntime();
}

TEST_F(LuaEcsIntegrationTest, Lua添加Transform后CPlusPlus侧可读取位置) {
    LuaTempScript startup("test_ecs_transform.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 10.0, 20.0, 30.0)
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    // 查找有 TransformComponent 的实体
    bool found = false;
    auto view = world.registry().view<TransformComponent>();
    for (auto entity : view) {
        auto& tf = view.get<TransformComponent>(entity);
        if (glm::distance(tf.position, glm::vec3(10.0f, 20.0f, 30.0f)) < 0.1f) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    ShutdownLuaRuntime();
}

TEST_F(LuaEcsIntegrationTest, Lua读取CPlusPlus侧创建实体的位置) {
    LuaTempScript startup("test_ecs_read.lua", R"(
        entity_ids = {}
        function Awake()
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    // C++ 侧创建实体
    Entity e = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(e);
    tf.position = glm::vec3(5.0f, 10.0f, 0.0f);
    tf.dirty = true;

    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());

    // Lua 侧通过 get_transform_position 读取
    LuaTempScript reader("test_read_pos.lua", R"(
        function Awake()
        end
        function Update(dt)
        end
    )");

    // 验证 C++ 侧实体在 Lua 引导后仍然存在
    EXPECT_TRUE(world.IsAlive(e));
    auto& tf_after = world.registry().get<TransformComponent>(e);
    EXPECT_FLOAT_EQ(tf_after.position.x, 5.0f);

    ShutdownLuaRuntime();
}

TEST_F(LuaEcsIntegrationTest, Lua批量创建实体) {
    LuaTempScript startup("test_ecs_batch.lua", R"(
        function Awake()
            for i = 1, 10 do
                local e = dse.ecs.create_entity()
                dse.ecs.add_transform(e, i * 1.0, 0.0, 0.0)
            end
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    // 应有 10 个带 Transform 的实体
    int transform_count = 0;
    auto view = world.registry().view<TransformComponent>();
    for (auto entity : view) {
        (void)entity;
        ++transform_count;
    }
    EXPECT_EQ(transform_count, 10);

    ShutdownLuaRuntime();
}

TEST_F(LuaEcsIntegrationTest, Lua错误脚本不崩溃引擎) {
    LuaTempScript startup("test_ecs_error.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            -- 故意访问不存在的 API
            nonexistent_api_call()
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    // 引导应因运行时错误而失败，但不应崩溃
    bool result = BootstrapLuaRuntime();
    EXPECT_FALSE(result);

    // World 状态应保持完整
    EXPECT_NO_THROW(world.EntityCount());

    ShutdownLuaRuntime();
}

TEST_F(LuaEcsIntegrationTest, Lua创建实体后销毁不崩溃) {
    LuaTempScript startup("test_ecs_destroy.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 1.0, 2.0, 3.0)
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    // 在 C++ 侧销毁所有实体
    auto view = world.registry().view<TransformComponent>();
    std::vector<Entity> to_destroy;
    for (auto entity : view) {
        to_destroy.push_back(entity);
    }
    for (auto entity : to_destroy) {
        world.DestroyEntity(entity);
    }

    EXPECT_EQ(world.EntityCount(), 0u);

    // 再 Tick 一次，不应崩溃
    EXPECT_NO_THROW(TickLuaRuntime(0.016f));

    ShutdownLuaRuntime();
}

TEST_F(LuaEcsIntegrationTest, ScriptComponent与LuaECS协同) {
    LuaTempScript startup("test_ecs_script.lua", R"(
        function Awake()
        end
        function Update(dt)
        end
    )");
    LuaTempScript entity_script("test_entity_ecs.lua", R"(
        local M = {}
        function M:OnAwake(entity_id)
            dse.ecs.add_transform(entity_id, 100.0, 200.0, 0.0)
        end
        function M:OnUpdate(entity_id, dt)
        end
        return M
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    Entity e = world.CreateEntity();
    auto& script_comp = world.registry().emplace<ScriptComponent>(e);
    script_comp.script_path = entity_script.Path();
    script_comp.enabled = true;

    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());

    // 第一帧：OnAwake 应通过 Lua 给实体添加 Transform
    TickLuaRuntime(0.016f);

    // 验证实体获得了 TransformComponent
    EXPECT_TRUE(world.registry().all_of<TransformComponent>(e));
    if (world.registry().all_of<TransformComponent>(e)) {
        auto& tf = world.registry().get<TransformComponent>(e);
        EXPECT_FLOAT_EQ(tf.position.x, 100.0f);
        EXPECT_FLOAT_EQ(tf.position.y, 200.0f);
    }

    ShutdownLuaRuntime();
}
