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

TEST_F(LuaBinding3DIntegrationTest, Lua创建3D相机CPlusPlus侧可读取FOV) {
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

TEST_F(LuaBinding3DIntegrationTest, Lua创建3D方向光CPlusPlus侧可读取参数) {
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

TEST_F(LuaBinding3DIntegrationTest, Lua创建3D刚体和碰撞体CPlusPlus侧可读取) {
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

TEST_F(LuaBinding3DIntegrationTest, Lua创建3D粒子系统CPlusPlus侧可读取参数) {
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

TEST_F(LuaBinding3DIntegrationTest, Lua创建后处理CPlusPlus侧可读取Bloom参数) {
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

TEST_F(LuaBinding3DIntegrationTest, Lua3D球体碰撞体创建CPlusPlus侧可验证) {
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

TEST_F(LuaBinding3DIntegrationTest, Lua3D错误参数不崩溃) {
    LuaTempScript startup("test_3d_error.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            -- 传无效参数给 3D API，不应崩溃
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
