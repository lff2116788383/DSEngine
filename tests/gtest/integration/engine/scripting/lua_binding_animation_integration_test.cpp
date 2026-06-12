/**
 * @file lua_binding_animation_integration_test.cpp
 * @brief Lua Animation 绑定集成测试
 *
 * 验证场景：
 * - Lua 创建 2D Animator + AnimationState，C++ 侧可读
 * - Lua 创建 3D Animator + FSM，C++ 侧可读状态
 * - Lua add_anim_layer 并设置 weight/clip，C++ 侧可读
 * - Lua IK 组件绑定
 * - 错误参数不崩溃
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/animation/animation_state_machine.h"
#include "engine/core/service_locator.h"
#include "engine/base/debug.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

using namespace dse;
using namespace dse::runtime;

namespace {
class TempLuaFile {
public:
    explicit TempLuaFile(const std::string& name, const std::string& content)
        : path_(name) { std::ofstream(path_) << content; }
    ~TempLuaFile() { std::filesystem::remove(path_); }
    const std::string& Path() const { return path_; }
private:
    std::string path_;
};
} // namespace

class LuaAnimBindingTest : public ::testing::Test {
protected:
    void SetUp() override { Debug::Init(); }
    void TearDown() override { ShutdownLuaRuntime(); }
};

// 测试 Lua动画绑定：Lua创建2D Animatorand添加状态
TEST_F(LuaAnimBindingTest, LuaCreate2DAnimatorandAddState) {
    TempLuaFile script("test_anim_2d.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_animator(e)
            dse.ecs.add_animation_state(e, "idle", 10.0, true, {})
        end
        function Update(dt) end
    )");
    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx; ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    bool found = false;
    auto view = world.registry().view<AnimatorComponent>();
    for (auto e : view) {
        auto& anim = view.get<AnimatorComponent>(e);
        if (anim.states.count("idle")) {
            found = true;
            EXPECT_FLOAT_EQ(anim.states["idle"].frame_rate, 10.0f);
            EXPECT_TRUE(anim.states["idle"].loop);
        }
    }
    EXPECT_TRUE(found);
}

// 测试 Lua动画绑定：Lua创建3D动画器FSM
TEST_F(LuaAnimBindingTest, LuaCreate3DAnimatorFSM) {
    TempLuaFile script("test_anim_3d_fsm.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0,0,0, 1,1,1)
            dse.ecs.add_animator_3d(e)
            dse.ecs.init_animator_3d_fsm(e, "idle")
            dse.ecs.add_animator_3d_state(e, "idle", "idle.glb")
            dse.ecs.add_animator_3d_state(e, "run", "run.glb")
            dse.ecs.add_animator_3d_transition(e, "idle", "run", 0.25, true, 1.0)
        end
        function Update(dt) end
    )");
    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx; ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    bool found = false;
    auto view = world.registry().view<Animator3DComponent>();
    for (auto e : view) {
        auto& anim = view.get<Animator3DComponent>(e);
        if (anim.state_machine) {
            found = true;
            EXPECT_EQ(anim.state_machine->GetDefaultState(), "idle");
        }
    }
    EXPECT_TRUE(found);
}

// 测试 Lua动画绑定：Lua添加到动画Layerand设置Weight
TEST_F(LuaAnimBindingTest, LuaAddToAnimLayerandSetWeight) {
    TempLuaFile script("test_anim_layer.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0,0,0, 1,1,1)
            dse.ecs.add_animator_3d(e)
            dse.ecs.add_anim_layer_component(e)
            dse.ecs.add_anim_layer(e, "upper_body")
            dse.ecs.set_anim_layer_weight(e, 0, 0.75)
        end
        function Update(dt) end
    )");
    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx; ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    bool found = false;
    auto view = world.registry().view<AnimLayerComponent>();
    for (auto e : view) {
        auto& lc = view.get<AnimLayerComponent>(e);
        for (auto& layer : lc.layers) {
            if (layer.name == "upper_body") {
                found = true;
                EXPECT_NEAR(layer.weight, 0.75f, 0.01f);
            }
        }
    }
    EXPECT_TRUE(found);
}

// 测试 Lua动画绑定：Lua添加到IK组件
TEST_F(LuaAnimBindingTest, LuaAddToIKComponent) {
    TempLuaFile script("test_ik.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0,0,0, 1,1,1)
            dse.ecs.add_animator_3d(e)
            dse.ecs.add_ik_component(e)
            dse.ecs.add_ik_chain(e, "left_hand", 0, "LeftHand", "LeftUpperArm", 1.0)
            dse.ecs.set_ik_target(e, 0, 1.0, 1.5, 0.5)
            dse.ecs.set_ik_weight(e, 0, 0.8)
            dse.ecs.set_ik_iterations(e, 0, 2)
        end
        function Update(dt) end
    )");
    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx; ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    bool found = false;
    auto view = world.registry().view<IKChain3DComponent>();
    for (auto e : view) {
        auto& ik = view.get<IKChain3DComponent>(e);
        for (auto& chain : ik.chains) {
            if (chain.name == "left_hand") {
                found = true;
                EXPECT_NEAR(chain.weight, 0.8f, 0.01f);
                EXPECT_EQ(chain.iterations, 2);
            }
        }
    }
    EXPECT_TRUE(found);
}

// 测试 Lua动画绑定：错误参数不崩溃
TEST_F(LuaAnimBindingTest, ErrorParametersDoesNotCrash) {
    TempLuaFile script("test_anim_error.lua", R"(
        function Awake()
            -- 不存在的实体（不应崩溃）
            pcall(dse.ecs.add_animator, 99999)
            pcall(dse.ecs.play_animation, 99999, "idle")
            -- nil 参数（luaL_checkinteger 会报错，用 pcall 捕获）
            pcall(dse.ecs.set_animator_3d_state, nil, "run")
        end
        function Update(dt) end
    )");
    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx; ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
}
