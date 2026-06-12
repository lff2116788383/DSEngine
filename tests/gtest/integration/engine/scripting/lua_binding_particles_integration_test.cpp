/**
 * @file lua_binding_particles_integration_test.cpp
 * @brief Lua Particles 绑定集成测试（3D 粒子 + 2D 发射器 + GameplayTuning）
 *
 * 覆盖：
 * - add_particle_system_3d: 创建 3D 粒子组件 + 参数
 * - set_particle_system_3d_params: 修改粒子参数
 * - get_particle_system_3d_state: 查询粒子状态
 * - add_particle_emitter: 创建 2D 发射器
 * - particle_burst: 触发瞬发
 * - add_gameplay_tuning / set_gameplay_tuning: 调参组件
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/components_2d.h"
#include "engine/core/service_locator.h"
#include <filesystem>
#include <fstream>

using namespace dse::runtime;
using namespace dse;

namespace {

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

} // namespace

class LuaParticleBindingTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
    }
};

// ============================================================
// 3D 粒子系统
// ============================================================

// 测试 Lua粒子绑定：添加粒子系统3D创建组件
TEST_F(LuaParticleBindingTest, AddParticleSystem3DCreateComponent) {
    LuaTempScript script("test_ps3d_add.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
            dse.ecs.add_particle_system_3d(e, 500, 50.0)
        end
        function Update(dt) end
    )");

    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    int found = 0;
    world.registry().view<ParticleSystem3DComponent>().each(
        [&](auto entity, const ParticleSystem3DComponent& ps) {
            EXPECT_EQ(ps.max_particles, 500);
            EXPECT_FLOAT_EQ(ps.emission_rate, 50.0f);
            found++;
        });
    EXPECT_EQ(found, 1);
}

// 测试 Lua粒子绑定：设置参数修改参数
TEST_F(LuaParticleBindingTest, SetParamsModifyParameters) {
    LuaTempScript script("test_ps3d_params.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
            dse.ecs.add_particle_system_3d(e, 1000, 100.0)
            dse.ecs.set_particle_system_3d_params(e,
                0.5, 2.0,   -- life min/max
                0.1, 0.5,   -- size min/max
                1.0, 5.0,   -- speed min/max
                1.0, 0.5, 0.0, 1.0,  -- color RGBA
                0.0, -9.8, 0.0,      -- gravity
                "particle.png"        -- texture
            )
        end
        function Update(dt) end
    )");

    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    int found = 0;
    world.registry().view<ParticleSystem3DComponent>().each(
        [&](auto entity, const ParticleSystem3DComponent& ps) {
            EXPECT_FLOAT_EQ(ps.start_life_min, 0.5f);
            EXPECT_FLOAT_EQ(ps.start_life_max, 2.0f);
            EXPECT_FLOAT_EQ(ps.start_size_min, 0.1f);
            EXPECT_FLOAT_EQ(ps.start_size_max, 0.5f);
            EXPECT_FLOAT_EQ(ps.start_speed_min, 1.0f);
            EXPECT_FLOAT_EQ(ps.start_speed_max, 5.0f);
            EXPECT_NEAR(ps.gravity.y, -9.8f, 1e-4f);
            EXPECT_EQ(ps.texture_path, "particle.png");
            found++;
        });
    EXPECT_EQ(found, 1);
}

// ============================================================
// 2D 粒子发射器
// ============================================================

// 测试 Lua粒子绑定：添加粒子发射器创建组件
TEST_F(LuaParticleBindingTest, AddParticleEmitterCreateComponent) {
    LuaTempScript script("test_emitter.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
            dse.ecs.add_particle_emitter(e, 0, 200, 30.0)
        end
        function Update(dt) end
    )");

    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    int found = 0;
    world.registry().view<ParticleEmitterComponent>().each(
        [&](auto entity, const ParticleEmitterComponent& pe) {
            EXPECT_EQ(pe.max_particles, 200);
            EXPECT_FLOAT_EQ(pe.emit_rate, 30.0f);
            found++;
        });
    EXPECT_EQ(found, 1);
}

// 测试 Lua粒子绑定：粒子Bursttrigger即时
TEST_F(LuaParticleBindingTest, ParticleBursttriggerInstant) {
    LuaTempScript script("test_burst.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
            dse.ecs.add_particle_emitter(e, 0, 100, 10.0)
            dse.ecs.particle_burst(e, 50)
        end
        function Update(dt) end
    )");

    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    int found = 0;
    world.registry().view<ParticleEmitterComponent>().each(
        [&](auto entity, const ParticleEmitterComponent& pe) {
            EXPECT_EQ(pe.pending_burst, 50);
            found++;
        });
    EXPECT_EQ(found, 1);
}

// ============================================================
// GameplayTuning
// ============================================================

// 测试 Lua粒子绑定：玩法调参创建且设置上
TEST_F(LuaParticleBindingTest, GameplayTuningCreateAndSetUp) {
    LuaTempScript script("test_tuning.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0, 0, 0, 1, 1, 1)
            dse.ecs.add_gameplay_tuning(e)
            dse.ecs.set_gameplay_tuning(e, 5.0, 2.0, 3.0, 1.5, 10.0, 0.8)
        end
        function Update(dt) end
    )");

    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    int found = 0;
    world.registry().view<GameplayTuningComponent>().each(
        [&](auto entity, const GameplayTuningComponent& gt) {
            EXPECT_FLOAT_EQ(gt.leaf_min_distance, 5.0f);
            EXPECT_FLOAT_EQ(gt.leaf_move_left, 2.0f);
            EXPECT_FLOAT_EQ(gt.leaf_move_right, 3.0f);
            EXPECT_FLOAT_EQ(gt.jump_speed_scale, 1.5f);
            EXPECT_FLOAT_EQ(gt.jump_speed_max, 10.0f);
            EXPECT_FLOAT_EQ(gt.camera_follow_damping, 0.8f);
            found++;
        });
    EXPECT_EQ(found, 1);
}

// ============================================================
// 错误参数安全性
// ============================================================

// 测试 Lua粒子绑定：若实体不存在Will不崩溃
TEST_F(LuaParticleBindingTest, IfTheEntityDoesNotExistItWillNotCrash) {
    LuaTempScript script("test_safe.lua", R"(
        function Awake()
            dse.ecs.set_particle_system_3d_params(99999)
            dse.ecs.particle_burst(99999, 10)
            dse.ecs.set_gameplay_tuning(99999)
        end
        function Update(dt) end
    )");

    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    EXPECT_NO_THROW(TickLuaRuntime(0.016f));
}
