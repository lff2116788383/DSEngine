/**
 * @file lua_e2e_scene_lifecycle_test.cpp
 * @brief P1: Lua 端到端场景生命周期集成测试
 *
 * 验证完整链路：创建场景 → 放置实体 → 添加物理/脚本组件 → 运行 N 帧 → 检查最终状态。
 * 不依赖 GPU，使用 headless Lua API 路径。
 *
 * 覆盖场景：
 * - 场景初始化 → 多实体创建 → Transform 设置 → 多帧 Tick → 状态一致性
 * - Lua 脚本驱动物理实体：创建带 RigidBody+Collider 的实体，Tick 后验证组件存在
 * - Lua on_update 累积状态：每帧修改 Transform，N 帧后位置偏移正确
 * - 复杂场景：父子层级 + 多实体 + 脚本交互
 * - 场景生命周期：创建 → 运行 → 停止 → 清理，无泄漏无崩溃
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/script.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/physics_2d.h"
#include "engine/core/service_locator.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace dse::runtime;

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

} // namespace

class LuaE2ESceneLifecycleTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
    }

    void BootWithScript(const std::string& path, World& world) {
        SetStartupLuaScriptPath(path);
        LuaApiContext ctx{};
        ctx.world = &world;
        ConfigureLuaApiContext(ctx);
        ASSERT_TRUE(BootstrapLuaRuntime());
    }
};

// ─── 场景初始化 → 多实体创建 → 多帧 Tick → 实体数量一致 ────────────────────────

TEST_F(LuaE2ESceneLifecycleTest, 多实体创建并运行多帧后实体数量正确) {
    TempLuaFile startup("test_e2e_multi_entity.lua", R"(
        function Awake()
            for i = 1, 5 do
                local e = dse.ecs.create_entity()
                dse.ecs.add_transform(e, i * 10.0, 0.0, 0.0)
            end
        end
        function Update(dt)
        end
    )");

    World world;
    BootWithScript(startup.Path(), world);

    // 运行 10 帧
    for (int i = 0; i < 10; ++i) {
        TickLuaRuntime(0.016f);
    }

    // 验证 5 个实体带 Transform
    int count = 0;
    auto view = world.registry().view<TransformComponent>();
    for (auto entity : view) {
        (void)entity;
        ++count;
    }
    EXPECT_EQ(count, 5);

    // 验证位置分布正确
    std::vector<float> x_positions;
    for (auto entity : view) {
        auto& tf = view.get<TransformComponent>(entity);
        x_positions.push_back(tf.position.x);
    }
    std::sort(x_positions.begin(), x_positions.end());
    for (int i = 0; i < 5; ++i) {
        EXPECT_FLOAT_EQ(x_positions[i], (i + 1) * 10.0f);
    }
}

// ─── Lua on_update 累积移动：每帧移动实体，N帧后位置偏移正确 ───────────────────

TEST_F(LuaE2ESceneLifecycleTest, 脚本每帧累积移动实体后位置偏移正确) {
    TempLuaFile startup("test_e2e_movement.lua", R"(
        local entity_id = nil
        function Awake()
            entity_id = dse.ecs.create_entity()
            dse.ecs.add_transform(entity_id, 0.0, 0.0, 0.0)
        end
        function Update(dt)
            if entity_id then
                local x, y, z = dse.ecs.get_transform_position(entity_id)
                if x then
                    dse.ecs.set_transform_position(entity_id, x + 1.0, y, z)
                end
            end
        end
    )");

    World world;
    BootWithScript(startup.Path(), world);

    const int num_frames = 20;
    for (int i = 0; i < num_frames; ++i) {
        TickLuaRuntime(0.016f);
    }

    // 验证实体 x 位置 = num_frames（每帧 +1）
    bool found = false;
    auto view = world.registry().view<TransformComponent>();
    for (auto entity : view) {
        auto& tf = view.get<TransformComponent>(entity);
        EXPECT_NEAR(tf.position.x, static_cast<float>(num_frames), 0.5f);
        found = true;
    }
    EXPECT_TRUE(found);
}

// ─── Lua 创建带物理组件的实体，验证组件存在性 ─────────────────────────────────

TEST_F(LuaE2ESceneLifecycleTest, Lua创建带物理组件的实体后组件存在) {
    TempLuaFile startup("test_e2e_physics.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 10.0, 0.0)
            dse.ecs.add_rigid_body(e, 2)  -- Dynamic
            dse.ecs.add_box_collider(e, 1.0, 1.0)
        end
        function Update(dt) end
    )");

    World world;
    BootWithScript(startup.Path(), world);
    TickLuaRuntime(0.016f);

    // 验证实体有 Transform + RigidBody + BoxCollider
    bool has_transform = false;
    bool has_rigid_body = false;
    bool has_collider = false;

    auto tf_view = world.registry().view<TransformComponent>();
    for (auto entity : tf_view) {
        has_transform = true;
        if (world.registry().all_of<RigidBody2DComponent>(entity)) {
            has_rigid_body = true;
        }
        if (world.registry().all_of<BoxCollider2DComponent>(entity)) {
            has_collider = true;
        }
    }

    EXPECT_TRUE(has_transform);
    EXPECT_TRUE(has_rigid_body);
    EXPECT_TRUE(has_collider);
}

// ─── ScriptComponent 驱动实体：OnAwake 初始化 + OnUpdate 持续修改 ──────────────

TEST_F(LuaE2ESceneLifecycleTest, ScriptComponent驱动实体初始化和持续更新) {
    TempLuaFile startup("test_e2e_scriptcomp_boot.lua", R"(
        function Awake() end
        function Update(dt) end
    )");
    TempLuaFile entity_script("test_e2e_entity_script.lua", R"(
        local M = {}
        M.frame_count = 0
        function M:OnAwake(entity_id)
            dse.ecs.add_transform(entity_id, 0.0, 0.0, 0.0)
        end
        function M:OnUpdate(entity_id, dt)
            M.frame_count = M.frame_count + 1
            local x, y, z = dse.ecs.get_transform_position(entity_id)
            if x then
                dse.ecs.set_transform_position(entity_id, x + 0.5, y, z)
            end
        end
        return M
    )");

    World world;
    Entity e = world.CreateEntity();
    auto& sc = world.registry().emplace<ScriptComponent>(e);
    sc.script_path = entity_script.Path();
    sc.enabled = true;

    BootWithScript(startup.Path(), world);

    // 运行 10 帧
    for (int i = 0; i < 10; ++i) {
        TickLuaRuntime(0.016f);
    }

    // 验证 Transform 已被脚本添加并修改
    ASSERT_TRUE(world.registry().all_of<TransformComponent>(e));
    auto& tf = world.registry().get<TransformComponent>(e);
    // OnUpdate 每帧 +0.5，运行 10 帧（第一帧 OnAwake 后可能还有 OnUpdate）
    EXPECT_GT(tf.position.x, 2.0f);
}

// ─── 场景完整生命周期：创建 → 运行 → 停止 → 再启动 → 无崩溃 ──────────────────

TEST_F(LuaE2ESceneLifecycleTest, 场景完整生命周期创建运行停止再启动无崩溃) {
    TempLuaFile startup("test_e2e_lifecycle.lua", R"(
        local entities = {}
        function Awake()
            for i = 1, 3 do
                local e = dse.ecs.create_entity()
                dse.ecs.add_transform(e, i * 5.0, 0.0, 0.0)
                table.insert(entities, e)
            end
        end
        function Update(dt)
            for _, e in ipairs(entities) do
                local x, y, z = dse.ecs.get_transform_position(e)
                if x then
                    dse.ecs.set_transform_position(e, x + 0.1, y, z)
                end
            end
        end
    )");

    // 第一轮生命周期
    {
        World world;
        BootWithScript(startup.Path(), world);
        for (int i = 0; i < 5; ++i) {
            TickLuaRuntime(0.016f);
        }
        EXPECT_GE(world.EntityCount(), 3u);
        ShutdownLuaRuntime();
    }

    // 第二轮生命周期——不应崩溃
    {
        World world;
        BootWithScript(startup.Path(), world);
        for (int i = 0; i < 5; ++i) {
            TickLuaRuntime(0.016f);
        }
        EXPECT_GE(world.EntityCount(), 3u);
        ShutdownLuaRuntime();
    }
}

// ─── 多脚本实体同场景协作：多个 ScriptComponent 实体同时运行 ──────────────────

TEST_F(LuaE2ESceneLifecycleTest, 多ScriptComponent实体同场景协作不冲突) {
    TempLuaFile startup("test_e2e_multi_script_boot.lua", R"(
        function Awake() end
        function Update(dt) end
    )");
    TempLuaFile script_a("test_e2e_script_a.lua", R"(
        local M = {}
        function M:OnAwake(entity_id)
            dse.ecs.add_transform(entity_id, 100.0, 0.0, 0.0)
        end
        function M:OnUpdate(entity_id, dt)
            local x, y, z = dse.ecs.get_transform_position(entity_id)
            if x then
                dse.ecs.set_transform_position(entity_id, x + 1.0, y, z)
            end
        end
        return M
    )");
    TempLuaFile script_b("test_e2e_script_b.lua", R"(
        local M = {}
        function M:OnAwake(entity_id)
            dse.ecs.add_transform(entity_id, -100.0, 0.0, 0.0)
        end
        function M:OnUpdate(entity_id, dt)
            local x, y, z = dse.ecs.get_transform_position(entity_id)
            if x then
                dse.ecs.set_transform_position(entity_id, x - 1.0, y, z)
            end
        end
        return M
    )");

    World world;
    Entity ea = world.CreateEntity();
    auto& sca = world.registry().emplace<ScriptComponent>(ea);
    sca.script_path = script_a.Path();
    sca.enabled = true;

    Entity eb = world.CreateEntity();
    auto& scb = world.registry().emplace<ScriptComponent>(eb);
    scb.script_path = script_b.Path();
    scb.enabled = true;

    BootWithScript(startup.Path(), world);

    for (int i = 0; i < 10; ++i) {
        TickLuaRuntime(0.016f);
    }

    // ea 向正方向移动，eb 向负方向移动
    ASSERT_TRUE(world.registry().all_of<TransformComponent>(ea));
    ASSERT_TRUE(world.registry().all_of<TransformComponent>(eb));

    auto& tf_a = world.registry().get<TransformComponent>(ea);
    auto& tf_b = world.registry().get<TransformComponent>(eb);

    EXPECT_GT(tf_a.position.x, 100.0f);  // 初始 100 + N*1
    EXPECT_LT(tf_b.position.x, -100.0f); // 初始 -100 - N*1
}
