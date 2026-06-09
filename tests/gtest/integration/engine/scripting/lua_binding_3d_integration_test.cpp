/**
 * @file lua_binding_3d_integration_test.cpp
 * @brief Lua Binding 3D 组件集成测试
 *
 * 验证场景：
 * - Lua 脚本通过 dse.ecs.add_camera_3d 创建 3D 相机
 * - Lua 脚本通过 dse.ecs.add_directional_light_3d 创建方向光
 * - Lua 脚本通过 dse.ecs.add_rigidbody_3d / add_box_collider_3d 创建物理体
 * - Lua 脚本通过 dse.ecs.add_particle_system_3d 创建粒子系统
 * - Lua 脚本通过 dse.ecs.add_post_process 创建后处理
 * - Lua 3D API 创建的组件在 C++ 侧可查询验证
 * - Lua 错误参数不崩溃引擎
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/core/service_locator.h"
#include "engine/ecs/components_3d_cloth.h"
#include "engine/ecs/components_3d_fluid.h"
#include "engine/ecs/components_3d_fracture.h"
#include "engine/ecs/components_3d_weather.h"
#include "engine/ecs/components_3d_snow.h"
#include "engine/ecs/components_3d_sky.h"
#include "engine/ecs/components_3d_animation.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/animation_state_machine.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

using namespace dse::runtime;
using namespace dse;  // 3D 组件在 dse 命名空间

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

class LuaBinding3DIntegrationTest : public ::testing::Test {
protected:
    void TearDown() override {
        ShutdownLuaRuntime();
    }
};

TEST_F(LuaBinding3DIntegrationTest, LuaCreate3DCameraCppCanReadFov) {
    LuaTempScript startup("test_3d_camera.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 5.0, 10.0, 1.0, 1.0, 1.0)
            dse.ecs.set_transform_rotation(e, -25.0, 0.0, 0.0)
            dse.ecs.add_camera_3d(e, 75.0, 1)
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

    bool found = false;
    auto view = world.registry().view<Camera3DComponent>();
    for (auto entity : view) {
        auto& cam = view.get<Camera3DComponent>(entity);
        if (std::abs(cam.fov - 75.0f) < 1.0f) {
            found = true;
            EXPECT_EQ(cam.priority, 1);
            break;
        }
    }
    EXPECT_TRUE(found);

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, LuaCreate3DDirectionalLightCppCanReadParams) {
    LuaTempScript startup("test_3d_light.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_directional_light_3d(e, -0.5, -1.0, -0.3, 1.0, 0.9, 0.8, 1.2, 0.15, 0.4)
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

    bool found = false;
    auto view = world.registry().view<DirectionalLight3DComponent>();
    for (auto entity : view) {
        auto& light = view.get<DirectionalLight3DComponent>(entity);
        if (light.intensity > 1.0f && light.shadow_strength > 0.3f) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    ShutdownLuaRuntime();
}

// S1.8：set_point_light_3d / set_spot_light_3d / set_spot_light_shadow 写入委托 C ABI 后，
// 部分更新（nil 通道沿用当前值）+ 方向归一化 经 Lua→C ABI→registry 实链路验证
TEST_F(LuaBinding3DIntegrationTest, LuaSetPointAndSpotLightDelegatedWritesCppCanRead) {
    LuaTempScript startup("test_3d_light_setters.lua", R"(
        function Awake()
            local p = dse.ecs.create_entity()
            dse.ecs.add_point_light_3d(p, 1.0, 1.0, 1.0, 1.0, 10.0)
            -- 只改 r/b 与 intensity，g 与 radius 传 nil 应保持 add 时的值
            dse.ecs.set_point_light_3d(p, 0.2, nil, 0.8, 5.0)

            local s = dse.ecs.create_entity()
            dse.ecs.add_spot_light_3d(s, 0.0, -1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 20.0, 12.5, 17.5)
            dse.ecs.set_spot_light_3d(s, 2.0, 0.0, 0.0, nil, nil, nil, 3.0, nil, nil, 30.0)
            dse.ecs.set_spot_light_shadow(s, true)
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

    bool found_point = false;
    for (auto entity : world.registry().view<PointLightComponent>()) {
        auto& pl = world.registry().get<PointLightComponent>(entity);
        EXPECT_NEAR(pl.color.r, 0.2f, 0.001f);
        EXPECT_NEAR(pl.color.g, 1.0f, 0.001f);  // nil → 保持 add 值
        EXPECT_NEAR(pl.color.b, 0.8f, 0.001f);
        EXPECT_NEAR(pl.intensity, 5.0f, 0.001f);
        EXPECT_NEAR(pl.radius, 10.0f, 0.001f);  // nil → 保持 add 值
        found_point = true;
    }
    EXPECT_TRUE(found_point);

    bool found_spot = false;
    for (auto entity : world.registry().view<SpotLightComponent>()) {
        auto& sl = world.registry().get<SpotLightComponent>(entity);
        EXPECT_NEAR(sl.direction.x, 1.0f, 0.001f);  // (2,0,0) 归一化 → (1,0,0)
        EXPECT_NEAR(sl.direction.y, 0.0f, 0.001f);
        EXPECT_NEAR(sl.direction.z, 0.0f, 0.001f);
        EXPECT_NEAR(sl.color.r, 1.0f, 0.001f);      // nil → 保持 add 值
        EXPECT_NEAR(sl.intensity, 3.0f, 0.001f);
        EXPECT_NEAR(sl.radius, 20.0f, 0.001f);      // nil → 保持 add 值
        EXPECT_NEAR(sl.outer_cone_angle, 30.0f, 0.001f);
        EXPECT_TRUE(sl.cast_shadow);
        found_spot = true;
    }
    EXPECT_TRUE(found_spot);

    ShutdownLuaRuntime();
}

// S1.8 Tier B/C：set_directional_light_3d / set_point_light_shadow 写入委托 per-field C ABI，
// set_directional_light_shadow 委托手写复合 dse_dir_light_set_shadow_params；经 Lua→C ABI→registry
// 验证部分更新（nil 沿用现值）、方向归一化、enabled，以及 cascade 级联钳制 + clamp。
TEST_F(LuaBinding3DIntegrationTest, LuaSetDirAndPointLightDelegatedWritesCppCanRead) {
    LuaTempScript startup("test_3d_light_tierbc.lua", R"(
        function Awake()
            local d = dse.ecs.create_entity()
            dse.ecs.add_directional_light_3d(d, -0.5, -1.0, -0.3, 1.0, 0.9, 0.8, 1.2, 0.15, 0.4)
            -- enabled=false；方向 (2,0,0) 归一化→(1,0,0)；color/ambient/shadow_strength 传 nil 保持；intensity=2.0
            dse.ecs.set_directional_light_3d(d, false, 2.0, 0.0, 0.0, nil, nil, nil, 2.0, nil, nil)
            -- cascade 钳制：strength 1.5→1.0；c0 0.05→0.1；c1 0.05→0.2；c2 0.05→0.3；lambda 2.0→1.0
            dse.ecs.set_directional_light_shadow(d, true, 1.5, 0.05, 0.05, 0.05, 2.0)

            local p = dse.ecs.create_entity()
            dse.ecs.add_point_light_3d(p, 1.0, 1.0, 1.0, 1.0, 10.0)
            dse.ecs.set_point_light_shadow(p, true)
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

    bool found_dir = false;
    for (auto entity : world.registry().view<DirectionalLight3DComponent>()) {
        auto& dl = world.registry().get<DirectionalLight3DComponent>(entity);
        EXPECT_FALSE(dl.enabled);                          // set_directional_light_3d(false, ...)
        EXPECT_NEAR(dl.direction.x, 1.0f, 0.001f);         // (2,0,0) 归一化
        EXPECT_NEAR(dl.direction.y, 0.0f, 0.001f);
        EXPECT_NEAR(dl.direction.z, 0.0f, 0.001f);
        EXPECT_NEAR(dl.color.r, 1.0f, 0.001f);             // nil → 保持 add 值
        EXPECT_NEAR(dl.color.g, 0.9f, 0.001f);
        EXPECT_NEAR(dl.color.b, 0.8f, 0.001f);
        EXPECT_NEAR(dl.intensity, 2.0f, 0.001f);
        EXPECT_NEAR(dl.ambient_intensity, 0.15f, 0.001f);  // nil → 保持 add 值
        EXPECT_TRUE(dl.cast_shadow);
        EXPECT_NEAR(dl.shadow_strength, 1.0f, 0.001f);     // 1.5 → clamp 1.0
        EXPECT_NEAR(dl.cascade_splits[0], 0.1f, 0.001f);   // max(0.1, 0.05)
        EXPECT_NEAR(dl.cascade_splits[1], 0.2f, 0.001f);   // max(0.1+0.1, 0.05)
        EXPECT_NEAR(dl.cascade_splits[2], 0.3f, 0.001f);   // max(0.2+0.1, 0.05)
        EXPECT_NEAR(dl.cascade_split_lambda, 1.0f, 0.001f);// 2.0 → clamp 1.0
        found_dir = true;
    }
    EXPECT_TRUE(found_dir);

    bool found_point = false;
    for (auto entity : world.registry().view<PointLightComponent>()) {
        auto& pl = world.registry().get<PointLightComponent>(entity);
        EXPECT_TRUE(pl.cast_shadow);                       // set_point_light_shadow(true)
        found_point = true;
    }
    EXPECT_TRUE(found_point);

    ShutdownLuaRuntime();
}

// S1.9：Animator3DComponent 纯字段经 codegen 入 defs，验证 Lua→C ABI→registry 实链路。
// 含 danim_path 纯字符串写入（set 后系统按值比较自动重载，无副作用）。
TEST_F(LuaBinding3DIntegrationTest, LuaSetAnimator3DFieldsCppCanRead) {
    LuaTempScript startup("test_3d_animator.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_animator_3d(e, "anims/idle.danim", "skeletons/hero.dskel")
            dse.ecs.set_animator3d_danim_path(e, "anims/run.danim")
            dse.ecs.set_animator3d_speed(e, 2.0)
            dse.ecs.set_animator3d_loop(e, false)
            dse.ecs.set_animator3d_use_anim_tree(e, true)
            dse.ecs.set_animator3d_blend_parameter(e, "velocity")
            dse.ecs.set_animator3d_blend_parameter_value(e, 0.5)
            dse.ecs.set_animator3d_enabled(e, false)
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

    bool found = false;
    for (auto entity : world.registry().view<Animator3DComponent>()) {
        auto& a = world.registry().get<Animator3DComponent>(entity);
        EXPECT_FALSE(a.enabled);
        EXPECT_EQ(a.danim_path, "anims/run.danim");        // 被 set 覆盖 add 初值
        EXPECT_EQ(a.dskel_path, "skeletons/hero.dskel");
        EXPECT_NEAR(a.speed, 2.0f, 0.001f);
        EXPECT_FALSE(a.loop);
        EXPECT_TRUE(a.use_anim_tree);
        EXPECT_EQ(a.blend_parameter, "velocity");
        EXPECT_NEAR(a.blend_parameter_value, 0.5f, 0.001f);
        found = true;
    }
    EXPECT_TRUE(found);

    ShutdownLuaRuntime();
}

// 动画 L4/L5：FSM / 动画层 / IK / Morph 经 C ABI 委托后，Lua→C ABI→组件 完整链路。
TEST_F(LuaBinding3DIntegrationTest, LuaAnimationDelegatedRoundTrip) {
    LuaTempScript startup("test_3d_animation_deleg.lua", R"(
        function Awake()
            -- FSM
            local e = dse.ecs.create_entity()
            dse.ecs.add_animator_3d(e, "anims/idle.danim", "skeletons/hero.dskel")
            dse.ecs.init_animator_3d_fsm(e)
            dse.ecs.add_animator_3d_state(e, "idle", "anims/idle.danim", true, 1.0)
            dse.ecs.add_animator_3d_state(e, "run", "anims/run.danim", true, 1.5)
            dse.ecs.add_animator_3d_transition(e, "idle", "run", 0.2, false, 1.0,
                { {"speed", 0, 0.5} })
            dse.ecs.set_animator_3d_param_float(e, "speed", 0.9)
            dse.ecs.set_animator_3d_param_trigger(e, "jump")
            dse.ecs.set_animator_3d_state(e, "run", 2.0, false)
            dse.ecs.add_animator_3d_event(e, "hit", 0.4)

            -- 动画层
            local la = dse.ecs.create_entity()
            dse.ecs.add_anim_layer_component(la)
            local idx = dse.ecs.add_anim_layer(la, "upper", 0.5, 0)
            dse.ecs.set_anim_layer_clip(la, idx, "anims/aim.danim", 1.0, true)
            dse.ecs.set_anim_layer_weight(la, idx, 0.8)
            dse.ecs.set_anim_layer_bone_mask(la, idx, {"spine", "head"})

            -- IK
            local ik = dse.ecs.create_entity()
            dse.ecs.add_ik_component(ik)
            local cidx = dse.ecs.add_ik_chain(ik, "leg", 1, "hip", "foot", 0.9)
            dse.ecs.set_ik_target(ik, cidx, 1.0, 2.0, 3.0)
            dse.ecs.set_ik_iterations(ik, cidx, 15)

            -- Morph
            local mo = dse.ecs.create_entity()
            dse.ecs.add_morph_target_component(mo)
            dse.ecs.morph_add_target(mo, "smile", {1,0,0, 0,1,0,  2,0,0, 0,1,0})
            dse.ecs.morph_set_weight(mo, "smile", 0.6)

            _G.fsm_e = e
            _G.layer_e = la
            _G.ik_e = ik
            _G.morph_e = mo
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

    // FSM 校验
    bool fsm_found = false;
    for (auto entity : world.registry().view<Animator3DComponent>()) {
        auto& a = world.registry().get<Animator3DComponent>(entity);
        ASSERT_TRUE(static_cast<bool>(a.state_machine));
        auto& sm = *a.state_machine;
        EXPECT_EQ(sm.GetStatesMutable().size(), 2u);
        ASSERT_TRUE(sm.GetStatesMutable().count("idle"));
        ASSERT_EQ(sm.GetStatesMutable().at("idle").transitions.size(), 1u);
        EXPECT_EQ(sm.GetStatesMutable().at("idle").transitions[0].target_state, "run");
        EXPECT_NEAR(sm.GetFloat("speed"), 0.9f, 0.001f);
        EXPECT_TRUE(sm.GetParameters().count("jump"));
        EXPECT_EQ(a.current_state_name, "run");
        EXPECT_NEAR(a.speed, 2.0f, 0.001f);
        EXPECT_FALSE(a.loop);
        ASSERT_EQ(a.events.size(), 1u);
        EXPECT_EQ(a.events[0].name, "hit");
        fsm_found = true;
    }
    EXPECT_TRUE(fsm_found);

    // 动画层校验
    bool layer_found = false;
    for (auto entity : world.registry().view<AnimLayerComponent>()) {
        auto& comp = world.registry().get<AnimLayerComponent>(entity);
        ASSERT_EQ(comp.layers.size(), 1u);
        EXPECT_EQ(comp.layers[0].name, "upper");
        EXPECT_NEAR(comp.layers[0].weight, 0.8f, 0.001f);
        EXPECT_EQ(comp.layers[0].danim_path, "anims/aim.danim");
        ASSERT_EQ(comp.layers[0].bone_mask_include.size(), 2u);
        EXPECT_EQ(comp.layers[0].bone_mask_include[0], "spine");
        layer_found = true;
    }
    EXPECT_TRUE(layer_found);

    // IK 校验
    bool ik_found = false;
    for (auto entity : world.registry().view<IKChain3DComponent>()) {
        auto& comp = world.registry().get<IKChain3DComponent>(entity);
        ASSERT_EQ(comp.chains.size(), 1u);
        EXPECT_EQ(comp.chains[0].name, "leg");
        EXPECT_NEAR(comp.chains[0].target_position.y, 2.0f, 0.001f);
        EXPECT_EQ(comp.chains[0].iterations, 15);
        ik_found = true;
    }
    EXPECT_TRUE(ik_found);

    // Morph 校验
    bool morph_found = false;
    for (auto entity : world.registry().view<MorphTargetComponent>()) {
        auto& comp = world.registry().get<MorphTargetComponent>(entity);
        ASSERT_EQ(comp.targets.size(), 1u);
        EXPECT_EQ(comp.targets[0].name, "smile");
        EXPECT_NEAR(comp.GetWeight("smile"), 0.6f, 0.001f);
        EXPECT_EQ(comp.vertex_count, 2);
        morph_found = true;
    }
    EXPECT_TRUE(morph_found);

    ShutdownLuaRuntime();
}

// L5：physics_3d_raycast 经 C ABI 委托后，验证 Lua→C ABI→Lua 完整返回链路。
// 脚本把 raycast 命中点/法线/距离写回 marker 实体的 transform，C++ 读回断言。
TEST_F(LuaBinding3DIntegrationTest, LuaPhysics3DRaycastDelegatedReturnsHit) {
    LuaTempScript startup("test_3d_raycast.lua", R"(
        function Awake()
            local box = dse.ecs.create_entity()
            dse.ecs.add_transform(box, 5.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_box_collider_3d(box, 2.0, 2.0, 2.0)  -- half-size 1 → AABB x∈[4,6]

            local marker = dse.ecs.create_entity()
            dse.ecs.add_transform(marker, -100.0, -100.0, -100.0, 1.0, 1.0, 1.0)

            local hit, ent, hx, hy, hz, nx, ny, nz, dist =
                dse.ecs.physics_3d_raycast(0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 100.0)
            if hit then
                dse.ecs.set_transform_position(marker, hx, hy, hz)
                dse.ecs.set_transform_scale(marker, dist, nx, 1.0)  -- 编码 dist + normal.x
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

    bool marker_found = false;
    for (auto entity : world.registry().view<TransformComponent>()) {
        if (world.registry().all_of<BoxCollider3DComponent>(entity)) continue;  // 跳过盒子
        const auto& tf = world.registry().get<TransformComponent>(entity);
        EXPECT_NEAR(tf.position.x, 4.0f, 1e-3f);   // 命中进入面 x=4
        EXPECT_NEAR(tf.position.y, 0.0f, 1e-3f);
        EXPECT_NEAR(tf.position.z, 0.0f, 1e-3f);
        EXPECT_NEAR(tf.scale.x, 4.0f, 1e-3f);      // distance
        EXPECT_NEAR(tf.scale.y, -1.0f, 1e-3f);     // normal.x
        marker_found = true;
    }
    EXPECT_TRUE(marker_found);

    ShutdownLuaRuntime();
}

// L5：rigidbody_3d_set/get_velocity 经 C ABI 委托 — 无物理服务时走组件缓存。
// 脚本 set→get→写回 marker，C++ 同时校验组件缓存与 Lua 返回链路。
TEST_F(LuaBinding3DIntegrationTest, LuaRigidBody3DVelocityDelegatedRoundTrip) {
    LuaTempScript startup("test_3d_rb_velocity.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_rigidbody_3d(e, 2, 1.0)
            dse.ecs.rigidbody_3d_set_velocity(e, 1.5, 2.5, 3.5)
            local vx, vy, vz = dse.ecs.rigidbody_3d_get_velocity(e)

            local marker = dse.ecs.create_entity()
            dse.ecs.add_transform(marker, vx, vy, vz, 1.0, 1.0, 1.0)
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

    bool rb_found = false;
    for (auto entity : world.registry().view<RigidBody3DComponent>()) {
        const auto& rb = world.registry().get<RigidBody3DComponent>(entity);
        EXPECT_NEAR(rb.velocity.x, 1.5f, 1e-3f);
        EXPECT_NEAR(rb.velocity.y, 2.5f, 1e-3f);
        EXPECT_NEAR(rb.velocity.z, 3.5f, 1e-3f);
        rb_found = true;
    }
    EXPECT_TRUE(rb_found);

    bool marker_found = false;
    for (auto entity : world.registry().view<TransformComponent>()) {
        if (world.registry().all_of<RigidBody3DComponent>(entity)) continue;
        const auto& tf = world.registry().get<TransformComponent>(entity);
        EXPECT_NEAR(tf.position.x, 1.5f, 1e-3f);   // get_velocity 返回链路
        EXPECT_NEAR(tf.position.y, 2.5f, 1e-3f);
        EXPECT_NEAR(tf.position.z, 3.5f, 1e-3f);
        marker_found = true;
    }
    EXPECT_TRUE(marker_found);

    ShutdownLuaRuntime();
}

// L5：character_controller_3d_move 经 C ABI 委托 — 无物理服务时走 ECS 回退。
// 空场景下按位移更新 transform 且不着地，move 把状态写入 CCT 组件，C++ 读回断言。
TEST_F(LuaBinding3DIntegrationTest, LuaCharacterController3DMoveDelegatedEcsFallback) {
    LuaTempScript startup("test_3d_cct_move.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 5.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_character_controller_3d(e, 0.3, 1.0)
            dse.ecs.character_controller_3d_move(e, 2.0, 0.0, 0.0, 0.0, 0.5)
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

    bool cct_found = false;
    for (auto entity : world.registry().view<CharacterController3DComponent>()) {
        const auto& cc = world.registry().get<CharacterController3DComponent>(entity);
        const auto& tf = world.registry().get<TransformComponent>(entity);
        EXPECT_FALSE(cc.is_grounded);                 // 空场景不着地
        EXPECT_NEAR(cc.velocity.x, 4.0f, 1e-3f);      // dx/dt = 2/0.5
        EXPECT_NEAR(tf.position.x, 2.0f, 1e-3f);
        EXPECT_NEAR(tf.position.y, 5.0f, 1e-3f);
        cct_found = true;
    }
    EXPECT_TRUE(cct_found);

    ShutdownLuaRuntime();
}

// L5：world_to_screen 经 C ABI 委托 — 主相机投影可见性（前方可见 / 背后不可见）。
// 脚本把两次可见性编码进 marker.position.x / .y，C++ 读回断言。
TEST_F(LuaBinding3DIntegrationTest, LuaWorldToScreenDelegatedVisibility) {
    LuaTempScript startup("test_3d_w2s.lua", R"(
        function Awake()
            local cam = dse.ecs.create_entity()
            dse.ecs.add_transform(cam, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_camera_3d(cam, 60.0, 0)

            local _, _, vis_front = dse.ecs.world_to_screen(0.0, 0.0, -5.0)
            local _, _, vis_behind = dse.ecs.world_to_screen(0.0, 0.0, 5.0)

            local marker = dse.ecs.create_entity()
            dse.ecs.add_transform(marker,
                (vis_front and 1.0 or 0.0),
                (vis_behind and 1.0 or 0.0),
                0.0, 1.0, 1.0, 1.0)
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

    bool marker_found = false;
    for (auto entity : world.registry().view<TransformComponent>()) {
        if (world.registry().all_of<Camera3DComponent>(entity)) continue;  // 跳过相机
        const auto& tf = world.registry().get<TransformComponent>(entity);
        EXPECT_NEAR(tf.position.x, 1.0f, 1e-3f);   // 前方可见
        EXPECT_NEAR(tf.position.y, 0.0f, 1e-3f);   // 背后不可见
        marker_found = true;
    }
    EXPECT_TRUE(marker_found);

    ShutdownLuaRuntime();
}

// S1.9：Cloth/Fluid 经 C ABI 委托 — Lua→dse_*→组件状态 完整链路。
// 脚本配置布料(钉点/风)与流体发射器，并把粒子数写回 marker，C++ 读回断言。
TEST_F(LuaBinding3DIntegrationTest, LuaClothFluidDelegatedRoundTrip) {
    LuaTempScript startup("test_3d_cloth_fluid.lua", R"(
        function Awake()
            local cloth = dse.ecs.create_entity()
            dse.ecs.add_transform(cloth, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_cloth(cloth, 12, 0.8, 0.02, 0.4)
            dse.ecs.cloth_pin_vertices(cloth, {3, 7, 11})
            dse.ecs.set_cloth_wind(cloth, 1.0, 0.0, -2.0)   -- turbulence 省略 → 保持

            local fluid = dse.ecs.create_entity()
            dse.ecs.add_transform(fluid, 10.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_fluid_emitter(fluid, 1, 250.0, 4.0, 3.0)
            local count = dse.ecs.get_fluid_particle_count(fluid)

            local marker = dse.ecs.create_entity()
            dse.ecs.add_transform(marker, count, 0.0, 0.0, 1.0, 1.0, 1.0)
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

    bool cloth_found = false;
    for (auto entity : world.registry().view<ClothComponent>()) {
        const auto& c = world.registry().get<ClothComponent>(entity);
        EXPECT_EQ(c.solver_iterations, 12u);
        ASSERT_EQ(c.pinned_vertices.size(), 3u);
        EXPECT_EQ(c.pinned_vertices[0], 3u);
        EXPECT_EQ(c.pinned_vertices[2], 11u);
        EXPECT_NEAR(c.wind.x, 1.0f, 1e-3f);
        EXPECT_NEAR(c.wind.z, -2.0f, 1e-3f);
        cloth_found = true;
    }
    EXPECT_TRUE(cloth_found);

    bool fluid_found = false;
    for (auto entity : world.registry().view<FluidEmitterComponent>()) {
        const auto& f = world.registry().get<FluidEmitterComponent>(entity);
        EXPECT_EQ(f.shape, FluidEmitterShape::Sphere);
        EXPECT_NEAR(f.emission_rate, 250.0f, 1e-3f);
        fluid_found = true;
    }
    EXPECT_TRUE(fluid_found);

    bool marker_found = false;
    for (auto entity : world.registry().view<TransformComponent>()) {
        if (world.registry().all_of<ClothComponent>(entity) ||
            world.registry().all_of<FluidEmitterComponent>(entity)) continue;
        const auto& tf = world.registry().get<TransformComponent>(entity);
        EXPECT_NEAR(tf.position.x, 0.0f, 1e-3f);   // get_fluid_particle_count 初始 0
        marker_found = true;
    }
    EXPECT_TRUE(marker_found);

    ShutdownLuaRuntime();
}

// SoftBody + Rope 委托回环：Lua → dse_* C ABI → 组件状态（无条件编译子系统）。
TEST_F(LuaBinding3DIntegrationTest, LuaSoftBodyRopeDelegatedRoundTrip) {
    LuaTempScript startup("test_3d_softbody_rope.lua", R"(
        function Awake()
            local sb = dse.ecs.create_entity()
            dse.ecs.add_transform(sb, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_softbody(sb, 0.7, 8, 0.97, 0.6)
            dse.ecs.softbody_set_gravity(sb, false)   -- gravity_scale 省略 → 保持

            local rope = dse.ecs.create_entity()
            dse.ecs.add_transform(rope, 5.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_rope(rope, 20, 0.25, 0.98, 10)
            dse.ecs.rope_set_anchors(rope, 3, 4, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6)
            dse.ecs.rope_set_gravity(rope, true, 1.5)
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

    bool sb_found = false;
    for (auto entity : world.registry().view<SoftBodyComponent>()) {
        const auto& sb = world.registry().get<SoftBodyComponent>(entity);
        EXPECT_EQ(sb.solver_iterations, 8);
        EXPECT_NEAR(sb.stiffness, 0.7f, 1e-3f);
        EXPECT_FALSE(sb.use_gravity);
        EXPECT_NEAR(sb.gravity_scale, 1.0f, 1e-3f);   // 省略 → 默认保持
        sb_found = true;
    }
    EXPECT_TRUE(sb_found);

    bool rope_found = false;
    for (auto entity : world.registry().view<RopeComponent>()) {
        const auto& r = world.registry().get<RopeComponent>(entity);
        EXPECT_EQ(r.segment_count, 20);
        EXPECT_EQ(r.anchor_entity_a, 3u);
        EXPECT_EQ(r.anchor_entity_b, 4u);
        EXPECT_NEAR(r.anchor_offset_a.y, 0.2f, 1e-3f);
        EXPECT_NEAR(r.anchor_offset_b.z, 0.6f, 1e-3f);
        EXPECT_TRUE(r.use_gravity);
        EXPECT_NEAR(r.gravity_scale, 1.5f, 1e-3f);
        rope_found = true;
    }
    EXPECT_TRUE(rope_found);

    ShutdownLuaRuntime();
}

// Batch 3：Lua → C ABI → 组件状态 的环境子系统回环（Weather/SnowCover/DayNight/Cloud）。
TEST_F(LuaBinding3DIntegrationTest, LuaEnvironmentDelegatedRoundTrip) {
    LuaTempScript startup("test_3d_environment.lua", R"(
        function Awake()
            local w = dse.ecs.create_entity()
            dse.ecs.add_transform(w, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_weather(w, "rain", 0.8)
            dse.ecs.set_weather(w, nil, nil, 4.0, nil)   -- type/intensity 保持，仅写 wind_x
            dse.ecs.set_weather_spawn(w, 50.0, nil, 1234)

            local s = dse.ecs.create_entity()
            dse.ecs.add_transform(s, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_snow_cover(s)
            dse.ecs.set_snow_cover(s, 0.9, nil, 0.07)
            dse.ecs.set_snow_texture(s, "tex/snow.png", 12.0)
            dse.ecs.set_snow_cover_enabled(s, false)

            local d = dse.ecs.create_entity()
            dse.ecs.add_transform(d, 2.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_day_night_cycle(d, 9.0, true, 30.0)
            dse.ecs.set_day_night_time(d, 18.25)
            dse.ecs.set_day_night_location(d, 51.0, nil, 100)

            local c = dse.ecs.create_entity()
            dse.ecs.add_transform(c, 3.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_volumetric_cloud(c)
            dse.ecs.set_cloud_layer(c, 2500.0, nil, 0.66, nil)
            dse.ecs.set_cloud_wind(c, 0.5, nil, 33.0)
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

    bool w_found = false;
    for (auto entity : world.registry().view<WeatherComponent>()) {
        const auto& wc = world.registry().get<WeatherComponent>(entity);
        EXPECT_EQ(wc.type, WeatherType::Rain);          // set_weather type=nil 保持
        EXPECT_NEAR(wc.intensity, 0.8f, 1e-3f);         // intensity=nil 保持
        EXPECT_NEAR(wc.wind_x, 4.0f, 1e-3f);
        EXPECT_NEAR(wc.spawn_radius, 50.0f, 1e-3f);
        EXPECT_EQ(wc.max_particles, 1234);
        w_found = true;
    }
    EXPECT_TRUE(w_found);

    bool s_found = false;
    for (auto entity : world.registry().view<SnowCoverComponent>()) {
        const auto& sc = world.registry().get<SnowCoverComponent>(entity);
        EXPECT_NEAR(sc.target_coverage, 0.9f, 1e-3f);
        EXPECT_NEAR(sc.melt_rate, 0.07f, 1e-3f);
        EXPECT_EQ(sc.snow_texture_path, "tex/snow.png");
        EXPECT_NEAR(sc.snow_tiling, 12.0f, 1e-3f);
        EXPECT_FALSE(sc.enabled);
        s_found = true;
    }
    EXPECT_TRUE(s_found);

    bool d_found = false;
    for (auto entity : world.registry().view<DayNightCycleComponent>()) {
        const auto& dnc = world.registry().get<DayNightCycleComponent>(entity);
        EXPECT_NEAR(dnc.time_of_day, 18.25f, 1e-3f);
        EXPECT_TRUE(dnc.auto_advance);
        EXPECT_NEAR(dnc.time_speed, 30.0f, 1e-3f);
        EXPECT_NEAR(dnc.latitude, 51.0f, 1e-3f);
        EXPECT_EQ(dnc.day_of_year, 100);
        d_found = true;
    }
    EXPECT_TRUE(d_found);

    bool c_found = false;
    for (auto entity : world.registry().view<VolumetricCloudComponent>()) {
        const auto& vc = world.registry().get<VolumetricCloudComponent>(entity);
        EXPECT_NEAR(vc.cloud_bottom, 2500.0f, 1e-3f);
        EXPECT_NEAR(vc.coverage, 0.66f, 1e-3f);
        EXPECT_NEAR(vc.wind_direction.x, 0.5f, 1e-3f);
        EXPECT_NEAR(vc.wind_speed, 33.0f, 1e-3f);
        c_found = true;
    }
    EXPECT_TRUE(c_found);

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, LuaCreate3DRigidBodyAndColliderCppCanRead) {
    LuaTempScript startup("test_3d_physics.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 2.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_rigidbody_3d(e, 2, 5.0)
            dse.ecs.add_box_collider_3d(e, 1.0, 1.0, 1.0)
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

    bool found_rb = false;
    auto rb_view = world.registry().view<RigidBody3DComponent>();
    for (auto entity : rb_view) {
        auto& rb = rb_view.get<RigidBody3DComponent>(entity);
        if (rb.mass > 4.0f) {
            found_rb = true;
            EXPECT_EQ(static_cast<int>(rb.type), 2);
            break;
        }
    }
    EXPECT_TRUE(found_rb);

    bool found_collider = false;
    auto col_view = world.registry().view<BoxCollider3DComponent>();
    for (auto entity : col_view) {
        auto& col = col_view.get<BoxCollider3DComponent>(entity);
        if (col.size.x > 0.5f && col.size.y > 0.5f && col.size.z > 0.5f) {
            found_collider = true;
            break;
        }
    }
    EXPECT_TRUE(found_collider);

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, LuaCreate3DParticleSystemCppCanReadParams) {
    LuaTempScript startup("test_3d_particles.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_particle_system_3d(e, 500, 80.0)
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

    bool found = false;
    auto view = world.registry().view<ParticleSystem3DComponent>();
    for (auto entity : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(entity);
        if (ps.max_particles == 500 && ps.emission_rate > 70.0f) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, LuaCreatePostProcessCppCanReadBloomParams) {
    LuaTempScript startup("test_3d_postprocess.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_post_process(e, true, 0.8, 1.5, 1.0)
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

    bool found = false;
    auto view = world.registry().view<PostProcessComponent>();
    for (auto entity : view) {
        auto& pp = view.get<PostProcessComponent>(entity);
        if (pp.enabled && pp.bloom_threshold > 0.5f && pp.bloom_intensity > 1.0f) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, Lua3DSphereColliderCreateCppCanVerify) {
    LuaTempScript startup("test_3d_sphere_collider.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 1.0, 3.0, -2.0, 1.0, 1.0, 1.0)
            dse.ecs.add_sphere_collider_3d(e, 0.5)
            dse.ecs.add_rigidbody_3d(e, 2, 2.0)
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

    bool found = false;
    auto view = world.registry().view<SphereCollider3DComponent>();
    for (auto entity : view) {
        auto& col = view.get<SphereCollider3DComponent>(entity);
        if (std::abs(col.radius - 0.5f) < 0.01f) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, LuaConfigureMeshMaterialAndDepthStateCppCanRead) {
    LuaTempScript startup("test_3d_mesh_material.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_mesh_renderer(e, 0.25, 0.5, 0.75, 0.9)
            dse.ecs.set_mesh_path(e, "data/meshes/test.dmesh")
            dse.ecs.set_mesh_shader_variant(e, "MESH_UNLIT")
            dse.ecs.set_mesh_depth_state(e, false, true)
            dse.ecs.set_mesh_material(e, 0.2, 0.7, 0.8, 1.0, 0.5, 0.25, 1.2, false, true, 0.9, 0.8, 0.7, 0.6)
            dse.ecs.set_mesh_material_scalar(e, "material_alpha_cutoff", 0.33)
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

    auto view = world.registry().view<MeshRendererComponent>();
    ASSERT_EQ(view.size(), 1u);
    for (auto entity : view) {
        const auto& mesh = view.get<MeshRendererComponent>(entity);
        EXPECT_EQ(mesh.mesh_path, "data/meshes/test.dmesh");
        EXPECT_EQ(mesh.shader_variant, "MESH_UNLIT");
        EXPECT_FALSE(mesh.depth_test_enabled);
        EXPECT_TRUE(mesh.depth_write_enabled);
        EXPECT_NEAR(mesh.roughness, 0.7f, 0.001f);
        EXPECT_NEAR(mesh.metallic, 0.2f, 0.001f);
        EXPECT_NEAR(mesh.ao, 0.8f, 0.001f);
        EXPECT_NEAR(mesh.material_alpha_cutoff, 0.33f, 0.001f);
        EXPECT_NEAR(mesh.normal_strength, 1.2f, 0.001f);
        EXPECT_FALSE(mesh.receive_shadow);
        EXPECT_TRUE(mesh.material_double_sided);
        EXPECT_NEAR(mesh.color.r, 0.9f, 0.001f);
        EXPECT_NEAR(mesh.color.g, 0.8f, 0.001f);
        EXPECT_NEAR(mesh.color.b, 0.7f, 0.001f);
        EXPECT_NEAR(mesh.color.a, 0.6f, 0.001f);
        EXPECT_NEAR(mesh.emissive.x, 1.0f, 0.001f);
        EXPECT_NEAR(mesh.emissive.y, 0.5f, 0.001f);
        EXPECT_NEAR(mesh.emissive.z, 0.25f, 0.001f);
    }

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, LuaConfigureTerrainAndHeightmapCppCanRead) {
    LuaTempScript startup("test_3d_terrain.lua", R"(
        function Awake()
            terrain = dse.ecs.create_entity()
            dse.ecs.add_transform(terrain, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_terrain(terrain, "", 256.0, 128.0, 42.0)
            dse.ecs.set_terrain_params(terrain, 9, 7, 3, 18.0, true)
            dse.ecs.set_terrain_height(terrain, 3, 4, 12.5)
            dse.ecs.add_terrain_heightmap(terrain, 0.0, 0.0, 2.0, 3, 3, 1.0, false)
            dse.ecs.terrain_heightmap_set_data(terrain, { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0 })
            sampled_height = dse.ecs.terrain_get_height(2.0, -2.0)
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

    auto terrain_view = world.registry().view<TerrainComponent>();
    ASSERT_EQ(terrain_view.size(), 1u);
    for (auto entity : terrain_view) {
        const auto& terrain = terrain_view.get<TerrainComponent>(entity);
        EXPECT_NEAR(terrain.width, 256.0f, 0.001f);
        EXPECT_NEAR(terrain.depth, 128.0f, 0.001f);
        EXPECT_NEAR(terrain.max_height, 42.0f, 0.001f);
        EXPECT_EQ(terrain.resolution_x, 9);
        EXPECT_EQ(terrain.resolution_z, 7);
        EXPECT_EQ(terrain.max_lod_levels, 3);
        EXPECT_NEAR(terrain.lod_distance_factor, 18.0f, 0.001f);
        EXPECT_TRUE(terrain.is_dirty);
        ASSERT_GE(terrain.height_data.size(), static_cast<size_t>(9 * 7));
        EXPECT_NEAR(terrain.height_data[4 * 9 + 3], 12.5f, 0.001f);
    }

    auto hm_view = world.registry().view<TerrainHeightmapComponent>();
    ASSERT_EQ(hm_view.size(), 1u);
    for (auto entity : hm_view) {
        const auto& hm = hm_view.get<TerrainHeightmapComponent>(entity);
        EXPECT_EQ(hm.cols, 3);
        EXPECT_EQ(hm.rows, 3);
        ASSERT_EQ(hm.heights.size(), 9u);
        EXPECT_NEAR(hm.GetHeight(2.0f, -2.0f), 4.0f, 0.001f);
    }

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, LuaConfigure3DPhysicsExtendedComponentsCppCanRead) {
    LuaTempScript startup("test_3d_physics_extended.lua", R"(
        function Awake()
            local a = dse.ecs.create_entity()
            local b = dse.ecs.create_entity()
            dse.ecs.add_transform(a, 0.0, 1.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_transform(b, 0.0, 3.0, 0.0, 1.0, 1.0, 1.0)
            dse.ecs.add_rigidbody_3d(a, 2, 3.0)
            dse.ecs.add_capsule_collider_3d(a, 0.35, 1.25, 1)
            dse.ecs.add_mesh_collider_3d(a, true)
            dse.ecs.set_collider_trigger(a, true)
            dse.ecs.set_collider_material(a, 0.4, 0.6)
            dse.ecs.set_collision_layer(a, 4, 12)
            dse.ecs.add_joint_3d(a, b, 1, 0.0, 0.5, 0.0)
            dse.ecs.set_joint_3d_hinge_limits(a, -20.0, 35.0)
            dse.ecs.set_joint_3d_spring(a, 77.0, 8.0)
            dse.ecs.set_joint_3d_distance(a, 0.5, 3.5)
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

    auto rb_view = world.registry().view<RigidBody3DComponent>();
    ASSERT_EQ(rb_view.size(), 1u);
    for (auto entity : rb_view) {
        const auto& rb = rb_view.get<RigidBody3DComponent>(entity);
        EXPECT_EQ(rb.collision_layer, 4u);
        EXPECT_EQ(rb.collision_mask, 12u);
    }

    auto capsule_view = world.registry().view<CapsuleCollider3DComponent>();
    ASSERT_EQ(capsule_view.size(), 1u);
    for (auto entity : capsule_view) {
        const auto& capsule = capsule_view.get<CapsuleCollider3DComponent>(entity);
        EXPECT_NEAR(capsule.radius, 0.35f, 0.001f);
        EXPECT_NEAR(capsule.height, 1.25f, 0.001f);
        EXPECT_TRUE(capsule.is_trigger);
        EXPECT_NEAR(capsule.bounciness, 0.6f, 0.001f);
        EXPECT_NEAR(capsule.friction, 0.4f, 0.001f);
    }

    auto mesh_col_view = world.registry().view<MeshCollider3DComponent>();
    ASSERT_EQ(mesh_col_view.size(), 1u);
    for (auto entity : mesh_col_view) {
        const auto& mesh_col = mesh_col_view.get<MeshCollider3DComponent>(entity);
        EXPECT_TRUE(mesh_col.convex);
        EXPECT_TRUE(mesh_col.is_trigger);
        EXPECT_NEAR(mesh_col.bounciness, 0.6f, 0.001f);
        EXPECT_NEAR(mesh_col.friction, 0.4f, 0.001f);
    }

    auto joint_view = world.registry().view<Joint3DComponent>();
    ASSERT_EQ(joint_view.size(), 1u);
    for (auto entity : joint_view) {
        const auto& joint = joint_view.get<Joint3DComponent>(entity);
        EXPECT_EQ(joint.type, Joint3DType::Hinge);
        EXPECT_TRUE(joint.use_limits);
        EXPECT_NEAR(joint.lower_limit, -20.0f, 0.001f);
        EXPECT_NEAR(joint.upper_limit, 35.0f, 0.001f);
        EXPECT_NEAR(joint.spring_stiffness, 77.0f, 0.001f);
        EXPECT_NEAR(joint.spring_damping, 8.0f, 0.001f);
        EXPECT_NEAR(joint.min_distance, 0.5f, 0.001f);
        EXPECT_NEAR(joint.max_distance, 3.5f, 0.001f);
    }

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, LuaConfigureGameplay3DAdvancedComponentsCppCanRead) {
    LuaTempScript startup("test_3d_gameplay_components.lua", R"(
        function Awake()
            local collider = dse.ecs.create_entity()
            dse.ecs.add_transform(collider, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)

            local cloth = dse.ecs.create_entity()
            dse.ecs.add_cloth(cloth, 12, 0.65, 0.03, 0.45)
            dse.ecs.set_cloth_wind(cloth, 1.0, 2.0, 3.0, 0.7)
            dse.ecs.set_cloth_gravity(cloth, 0.0, -5.0, 0.0)
            dse.ecs.cloth_pin_vertices(cloth, { 0, 2, 4 })
            dse.ecs.cloth_add_sphere_collider(cloth, collider, 1.25)

            local fluid = dse.ecs.create_entity()
            dse.ecs.add_fluid_emitter(fluid, 2, 640.0, 4.5, 3.25)
            dse.ecs.set_fluid_physics(fluid, 0.2, 0.3, 900.0, 60.0)
            dse.ecs.set_fluid_rendering(fluid, 0.1, 0.2, 0.3, 0.4, 0.5, 2.5, 0.9)
            dse.ecs.set_fluid_emit_direction(fluid, 0.0, 1.0, 0.0, 0.12)
            dse.ecs.set_fluid_floor(fluid, -2.0, 0.55)

            local fracture = dse.ecs.create_entity()
            dse.ecs.add_fracture(fracture, 1, 16, 700.0, 10.0)
            dse.ecs.set_fracture_params(fracture, 88.0, 6.0, 2.0, 1.5)
            dse.ecs.fracture_apply_damage(fracture, 11.0, 1.0, 2.0, 3.0)

            local ragdoll = dse.ecs.create_entity()
            dse.ecs.add_ragdoll(ragdoll, 21.0, false, 3.0, 4.0)
            dse.ecs.ragdoll_activate(ragdoll)
            dse.ecs.set_ragdoll_collision_layer(ragdoll, 8, 15)

            local softbody = dse.ecs.create_entity()
            dse.ecs.add_softbody(softbody, 0.8, 9, 0.91, 0.7)
            dse.ecs.softbody_set_gravity(softbody, false, 0.25)

            local vehicle = dse.ecs.create_entity()
            dse.ecs.add_vehicle(vehicle, 6000.0, 4000.0, 40.0)
            dse.ecs.vehicle_add_wheel(vehicle, 1.0, -0.5, 1.2, 0.42, true, true, 123.0, 45.0)
            dse.ecs.vehicle_set_input(vehicle, 2.0, -1.0, -2.0)

            local rope = dse.ecs.create_entity()
            dse.ecs.add_rope(rope, 6, 0.33, 0.95, 5)
            dse.ecs.rope_set_gravity(rope, false, 0.2)
            dse.ecs.rope_set_anchors(rope, 0, collider, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6)

            local buoyancy = dse.ecs.create_entity()
            dse.ecs.add_buoyancy(buoyancy, 2.5, 15.0, 4.0, 1.5, 0.8)
            dse.ecs.buoyancy_add_sample_point(buoyancy, 0.0, -0.5, 0.0, 2.0)
            dse.ecs.buoyancy_set_water_level(buoyancy, 3.5)
            dse.ecs.buoyancy_set_use_fluid(buoyancy, false)
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

    auto cloth_view = world.registry().view<ClothComponent>();
    ASSERT_EQ(cloth_view.size(), 1u);
    for (auto entity : cloth_view) {
        const auto& cloth = cloth_view.get<ClothComponent>(entity);
        EXPECT_EQ(cloth.solver_iterations, 12u);
        EXPECT_NEAR(cloth.stiffness, 0.65f, 0.001f);
        EXPECT_NEAR(cloth.wind_turbulence, 0.7f, 0.001f);
        EXPECT_NEAR(cloth.gravity.y, -5.0f, 0.001f);
        ASSERT_EQ(cloth.pinned_vertices.size(), 3u);
        ASSERT_EQ(cloth.sphere_colliders.size(), 1u);
        EXPECT_NEAR(cloth.sphere_colliders[0].radius, 1.25f, 0.001f);
    }

    auto fluid_view = world.registry().view<FluidEmitterComponent>();
    ASSERT_EQ(fluid_view.size(), 1u);
    for (auto entity : fluid_view) {
        const auto& fluid = fluid_view.get<FluidEmitterComponent>(entity);
        EXPECT_EQ(fluid.shape, FluidEmitterShape::Box);
        EXPECT_NEAR(fluid.emission_rate, 640.0f, 0.001f);
        EXPECT_NEAR(fluid.particle_lifetime, 4.5f, 0.001f);
        EXPECT_NEAR(fluid.emit_direction.y, 1.0f, 0.001f);
        EXPECT_NEAR(fluid.floor_y, -2.0f, 0.001f);
        EXPECT_NEAR(fluid.collision_restitution, 0.55f, 0.001f);
        EXPECT_NEAR(fluid.color.w, 0.4f, 0.001f);
    }

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    auto fracture_view = world.registry().view<FractureComponent>();
    ASSERT_EQ(fracture_view.size(), 1u);
    for (auto entity : fracture_view) {
        const auto& fracture = fracture_view.get<FractureComponent>(entity);
        EXPECT_EQ(fracture.source, FractureSource::RuntimeVoronoi);
        EXPECT_EQ(fracture.runtime_fragment_count, 16u);
        EXPECT_TRUE(fracture.fracture_requested);
        EXPECT_NEAR(fracture.health, -1.0f, 0.001f);
        EXPECT_NEAR(fracture.explosion_force, 88.0f, 0.001f);
        EXPECT_NEAR(fracture.impact_point.z, 3.0f, 0.001f);
    }
#endif

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    auto ragdoll_view = world.registry().view<RagdollComponent>();
    ASSERT_EQ(ragdoll_view.size(), 1u);
    for (auto entity : ragdoll_view) {
        const auto& ragdoll = ragdoll_view.get<RagdollComponent>(entity);
        EXPECT_TRUE(ragdoll.active);
        EXPECT_FALSE(ragdoll.auto_setup);
        EXPECT_NEAR(ragdoll.total_mass, 21.0f, 0.001f);
        EXPECT_EQ(ragdoll.collision_layer, 8u);
        EXPECT_EQ(ragdoll.collision_mask, 15u);
    }
#endif

    auto softbody_view = world.registry().view<SoftBodyComponent>();
    ASSERT_EQ(softbody_view.size(), 1u);
    for (auto entity : softbody_view) {
        const auto& softbody = softbody_view.get<SoftBodyComponent>(entity);
        EXPECT_NEAR(softbody.stiffness, 0.8f, 0.001f);
        EXPECT_EQ(softbody.solver_iterations, 9);
        EXPECT_FALSE(softbody.use_gravity);
        EXPECT_NEAR(softbody.gravity_scale, 0.25f, 0.001f);
    }

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    auto vehicle_view = world.registry().view<VehicleComponent>();
    ASSERT_EQ(vehicle_view.size(), 1u);
    for (auto entity : vehicle_view) {
        const auto& vehicle = vehicle_view.get<VehicleComponent>(entity);
        EXPECT_NEAR(vehicle.max_engine_force, 6000.0f, 0.001f);
        EXPECT_NEAR(vehicle.max_brake_force, 4000.0f, 0.001f);
        EXPECT_NEAR(vehicle.max_steer_angle, 40.0f, 0.001f);
        ASSERT_EQ(vehicle.wheels.size(), 1u);
        EXPECT_TRUE(vehicle.wheels[0].is_drive_wheel);
        EXPECT_TRUE(vehicle.wheels[0].is_steer_wheel);
        EXPECT_NEAR(vehicle.throttle, 1.0f, 0.001f);
        EXPECT_NEAR(vehicle.brake, 0.0f, 0.001f);
        EXPECT_NEAR(vehicle.steering, -1.0f, 0.001f);
    }
#endif

    auto rope_view = world.registry().view<RopeComponent>();
    ASSERT_EQ(rope_view.size(), 1u);
    for (auto entity : rope_view) {
        const auto& rope = rope_view.get<RopeComponent>(entity);
        EXPECT_EQ(rope.segment_count, 6);
        EXPECT_NEAR(rope.segment_length, 0.33f, 0.001f);
        EXPECT_FALSE(rope.use_gravity);
        EXPECT_NEAR(rope.gravity_scale, 0.2f, 0.001f);
        EXPECT_NEAR(rope.anchor_offset_a.x, 0.1f, 0.001f);
        EXPECT_NEAR(rope.anchor_offset_b.z, 0.6f, 0.001f);
    }

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    auto buoyancy_view = world.registry().view<BuoyancyComponent>();
    ASSERT_EQ(buoyancy_view.size(), 1u);
    for (auto entity : buoyancy_view) {
        const auto& buoyancy = buoyancy_view.get<BuoyancyComponent>(entity);
        EXPECT_NEAR(buoyancy.water_level, 3.5f, 0.001f);
        EXPECT_NEAR(buoyancy.buoyancy_force, 15.0f, 0.001f);
        EXPECT_FALSE(buoyancy.use_fluid_system);
        ASSERT_EQ(buoyancy.sample_points.size(), 1u);
        EXPECT_NEAR(buoyancy.sample_points[0].force_scale, 2.0f, 0.001f);
    }
#endif

    ShutdownLuaRuntime();
}

TEST_F(LuaBinding3DIntegrationTest, Lua3DErrorArgsNoCrash) {
    LuaTempScript startup("test_3d_error.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            -- invalid 3D API arguments should not crash
            pcall(dse.ecs.add_camera_3d, e, -1.0, 0)
            pcall(dse.ecs.add_rigidbody_3d, e, -999, -1.0)
            pcall(dse.ecs.add_box_collider_3d, e, 0.0, 0.0, 0.0)
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ConfigureLuaApiContext(ctx);

    bool result = BootstrapLuaRuntime();
    EXPECT_NO_THROW(TickLuaRuntime(0.016f));

    ShutdownLuaRuntime();
}
