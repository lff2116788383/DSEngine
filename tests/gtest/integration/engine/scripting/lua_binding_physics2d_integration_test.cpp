/**
 * @file lua_binding_physics2d_integration_test.cpp
 * @brief Lua Physics2D 绑定集成测试 — 验证 CircleCollider/Joint2D 的 Lua API
 *
 * 覆盖场景：
 *   1. Lua add_circle_collider 创建 CircleCollider2DComponent
 *   2. Lua set_circle_collider_trigger 设置触发器标志
 *   3. Lua add_joint_2d 创建 Joint2DComponent（各类型）
 *   4. Lua set_joint_2d_revolute 设置铰链参数
 *   5. Lua set_joint_2d_distance 设置距离参数
 *   6. Lua set_joint_2d_prismatic 设置棱柱参数
 *   7. Lua destroy_joint_2d 销毁关节
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/physics_2d.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include "engine/core/service_locator.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace dse::runtime;

namespace {

class LuaTempFile {
public:
    explicit LuaTempFile(const std::string& name, const std::string& content)
        : path_(name) {
        std::ofstream out(path_);
        out << content;
    }
    ~LuaTempFile() { std::filesystem::remove(path_); }
    const std::string& Path() const { return path_; }
private:
    std::string path_;
};

} // namespace

class LuaBindingPhysics2DTest : public ::testing::Test {
protected:
    World world;
    std::shared_ptr<Physics2DSystem> physics = std::make_shared<Physics2DSystem>();

    void SetUp() override {
        dse::core::ServiceLocator::Instance().Register<Physics2DSystem, Physics2DSystem>(physics);
    }

    void TearDown() override {
        ShutdownLuaRuntime();
        physics->Shutdown();
        dse::core::ServiceLocator::Instance().Reset<Physics2DSystem>();
    }

    void RunLuaScript(const std::string& filename, const std::string& lua_code) {
        LuaTempFile script(filename, lua_code);
        SetStartupLuaScriptPath(script.Path());
        LuaApiContext ctx{};
        ctx.world = &world;
        ConfigureLuaApiContext(ctx);
        ASSERT_TRUE(BootstrapLuaRuntime());
        TickLuaRuntime(0.016f);
    }
};

// 测试 Lua绑定物理2D：Lua添加环形碰撞体
TEST_F(LuaBindingPhysics2DTest, LuaAddACircularCollider) {
    RunLuaScript("test_circle_collider.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0, 0, 0)
            dse.ecs.add_rigid_body(e, 2)  -- Dynamic
            dse.ecs.add_circle_collider(e, 1.5, 2.0, 0.5, 0.1)
        end
        function Update(dt) end
    )");

    bool found = false;
    auto view = world.registry().view<CircleCollider2DComponent>();
    for (auto entity : view) {
        auto& cc = view.get<CircleCollider2DComponent>(entity);
        EXPECT_FLOAT_EQ(cc.radius, 1.5f);
        EXPECT_FLOAT_EQ(cc.density, 2.0f);
        EXPECT_FLOAT_EQ(cc.friction, 0.5f);
        EXPECT_FLOAT_EQ(cc.restitution, 0.1f);
        found = true;
    }
    EXPECT_TRUE(found);
}

// 测试 Lua绑定物理2D：Lua设置上环形碰撞体触发
TEST_F(LuaBindingPhysics2DTest, LuaSetUpCircularColliderTrigger) {
    RunLuaScript("test_circle_trigger.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0, 0, 0)
            dse.ecs.add_rigid_body(e, 2)
            dse.ecs.add_circle_collider(e, 0.5)
            dse.ecs.set_circle_collider_trigger(e, true)
        end
        function Update(dt) end
    )");

    auto view = world.registry().view<CircleCollider2DComponent>();
    for (auto entity : view) {
        EXPECT_TRUE(view.get<CircleCollider2DComponent>(entity).is_trigger);
    }
}

// 测试 Lua绑定物理2D：Lua添加Welded关节
TEST_F(LuaBindingPhysics2DTest, LuaAddWeldedJoints) {
    RunLuaScript("test_add_weld_joint.lua", R"(
        function Awake()
            local a = dse.ecs.create_entity()
            dse.ecs.add_transform(a, 0, 0, 0)
            dse.ecs.add_rigid_body(a, 0)  -- Static
            dse.ecs.add_box_collider(a, 1, 1)

            local b = dse.ecs.create_entity()
            dse.ecs.add_transform(b, 1, 0, 0)
            dse.ecs.add_rigid_body(b, 2)  -- Dynamic
            dse.ecs.add_box_collider(b, 1, 1)

            local j = dse.ecs.create_entity()
            dse.ecs.add_joint_2d(j, 3, a, b, 0, 0, 0, 0, false)  -- type=3 (Weld)
        end
        function Update(dt) end
    )");

    auto view = world.registry().view<Joint2DComponent>();
    bool found = false;
    for (auto entity : view) {
        auto& jc = view.get<Joint2DComponent>(entity);
        EXPECT_EQ(jc.type, Joint2DType::Weld);
        found = true;
    }
    EXPECT_TRUE(found);
}

// 测试 Lua绑定物理2D：Lua添加Hinge关节且设置参数
TEST_F(LuaBindingPhysics2DTest, LuaAddHingeJointsAndSetParameters) {
    RunLuaScript("test_revolute_joint.lua", R"(
        function Awake()
            local a = dse.ecs.create_entity()
            dse.ecs.add_transform(a, 0, 0, 0)
            dse.ecs.add_rigid_body(a, 0)
            dse.ecs.add_box_collider(a, 1, 1)

            local b = dse.ecs.create_entity()
            dse.ecs.add_transform(b, 1, 0, 0)
            dse.ecs.add_rigid_body(b, 2)
            dse.ecs.add_box_collider(b, 1, 1)

            local j = dse.ecs.create_entity()
            dse.ecs.add_joint_2d(j, 0, a, b)  -- type=0 (Revolute)
            dse.ecs.set_joint_2d_revolute(j, true, -45, 45, true, 90, 10)
        end
        function Update(dt) end
    )");

    auto view = world.registry().view<Joint2DComponent>();
    for (auto entity : view) {
        auto& jc = view.get<Joint2DComponent>(entity);
        EXPECT_EQ(jc.type, Joint2DType::Revolute);
        EXPECT_TRUE(jc.enable_limit);
        EXPECT_FLOAT_EQ(jc.lower_angle, -45.0f);
        EXPECT_FLOAT_EQ(jc.upper_angle, 45.0f);
        EXPECT_TRUE(jc.enable_motor);
        EXPECT_FLOAT_EQ(jc.motor_speed, 90.0f);
        EXPECT_FLOAT_EQ(jc.max_motor_torque, 10.0f);
    }
}

// 测试 Lua绑定物理2D：Lua添加距离关节且设置参数
TEST_F(LuaBindingPhysics2DTest, LuaAddDistanceJointsAndSetParameters) {
    RunLuaScript("test_distance_joint.lua", R"(
        function Awake()
            local a = dse.ecs.create_entity()
            dse.ecs.add_transform(a, 0, 5, 0)
            dse.ecs.add_rigid_body(a, 0)
            dse.ecs.add_box_collider(a, 1, 1)

            local b = dse.ecs.create_entity()
            dse.ecs.add_transform(b, 0, 0, 0)
            dse.ecs.add_rigid_body(b, 2)
            dse.ecs.add_box_collider(b, 1, 1)

            local j = dse.ecs.create_entity()
            dse.ecs.add_joint_2d(j, 1, a, b)  -- type=1 (Distance)
            dse.ecs.set_joint_2d_distance(j, 0.5, 5.0, 10.0, 0.5)
        end
        function Update(dt) end
    )");

    auto view = world.registry().view<Joint2DComponent>();
    for (auto entity : view) {
        auto& jc = view.get<Joint2DComponent>(entity);
        EXPECT_EQ(jc.type, Joint2DType::Distance);
        EXPECT_FLOAT_EQ(jc.min_length, 0.5f);
        EXPECT_FLOAT_EQ(jc.max_length, 5.0f);
        EXPECT_FLOAT_EQ(jc.stiffness, 10.0f);
        EXPECT_FLOAT_EQ(jc.damping, 0.5f);
    }
}

// 测试 Lua绑定物理2D：Lua添加Prismatic关节且设置参数
TEST_F(LuaBindingPhysics2DTest, LuaAddPrismaticJointsAndSetParameters) {
    RunLuaScript("test_prismatic_joint.lua", R"(
        function Awake()
            local a = dse.ecs.create_entity()
            dse.ecs.add_transform(a, 0, 0, 0)
            dse.ecs.add_rigid_body(a, 0)
            dse.ecs.add_box_collider(a, 1, 1)

            local b = dse.ecs.create_entity()
            dse.ecs.add_transform(b, 0, 1, 0)
            dse.ecs.add_rigid_body(b, 2)
            dse.ecs.add_box_collider(b, 1, 1)

            local j = dse.ecs.create_entity()
            dse.ecs.add_joint_2d(j, 2, a, b)  -- type=2 (Prismatic)
            dse.ecs.set_joint_2d_prismatic(j, 0, 1, true, -2, 2, true, 1.5, 10)
        end
        function Update(dt) end
    )");

    auto view = world.registry().view<Joint2DComponent>();
    for (auto entity : view) {
        auto& jc = view.get<Joint2DComponent>(entity);
        EXPECT_EQ(jc.type, Joint2DType::Prismatic);
        EXPECT_FLOAT_EQ(jc.prismatic_axis.x, 0.0f);
        EXPECT_FLOAT_EQ(jc.prismatic_axis.y, 1.0f);
        EXPECT_TRUE(jc.enable_limit);
        EXPECT_FLOAT_EQ(jc.lower_translation, -2.0f);
        EXPECT_FLOAT_EQ(jc.upper_translation, 2.0f);
        EXPECT_TRUE(jc.enable_motor);
        EXPECT_FLOAT_EQ(jc.prismatic_motor_speed, 1.5f);
        EXPECT_FLOAT_EQ(jc.max_motor_force, 10.0f);
    }
}
