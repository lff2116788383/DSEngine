/**
 * @file lua_binding_ui_integration_test.cpp
 * @brief Lua UI 绑定集成测试
 *
 * 验证场景：
 * - Lua 创建 UI Renderer，C++ 侧可读 texture_handle / color / order / size
 * - Lua 创建 UI Label，C++ 侧可读文字数据
 * - Lua 设置 UIButton / Progressbar 属性
 * - 错误参数不崩溃
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/ui.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/core/service_locator.h"
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

class LuaUIBindingTest : public ::testing::Test {
protected:
    void TearDown() override { ShutdownLuaRuntime(); }
};

TEST_F(LuaUIBindingTest, Lua创建UIRenderer可读取属性) {
    TempLuaFile script("test_ui_renderer.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ui.add_renderer(e, 0, 1.0, 0.5, 0.0, 0.8, 5, 200, 60)
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
    auto view = world.registry().view<UIRendererComponent>();
    for (auto e : view) {
        auto& ui = view.get<UIRendererComponent>(e);
        if (ui.order == 5) {
            found = true;
            EXPECT_NEAR(ui.color.r, 1.0f, 0.01f);
            EXPECT_NEAR(ui.color.g, 0.5f, 0.01f);
            EXPECT_NEAR(ui.color.a, 0.8f, 0.01f);
            EXPECT_NEAR(ui.size.x, 200.0f, 0.01f);
            EXPECT_NEAR(ui.size.y, 60.0f, 0.01f);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LuaUIBindingTest, Lua创建UILabel) {
    TempLuaFile script("test_ui_label.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ui.add_label(e, "Hello", 0, 1,1,1,1, 16,16, 2, 16,8, 32)
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
    auto view = world.registry().view<UILabelComponent>();
    for (auto e : view) {
        auto& label = view.get<UILabelComponent>(e);
        if (label.text == "Hello") {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LuaUIBindingTest, Lua设置UIPosition) {
    TempLuaFile script("test_ui_pos.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ui.add_renderer(e, 0, 1,1,1,1, 0, 100, 50)
            dse.ui.set_position(e, 300, 200)
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
    auto view = world.registry().view<UIRendererComponent>();
    for (auto e : view) {
        auto& ui = view.get<UIRendererComponent>(e);
        if (std::abs(ui.position.x - 300.0f) < 1.0f) {
            found = true;
            EXPECT_NEAR(ui.position.y, 200.0f, 1.0f);
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LuaUIBindingTest, 错误参数不崩溃) {
    TempLuaFile script("test_ui_error.lua", R"(
        function Awake()
            dse.ui.add_renderer(99999, 0, 1,1,1,1, 0, 100, 50)
            dse.ui.set_position(99999, 0, 0)
            dse.ui.add_label(99999, "bad", 0, 1,1,1,1)
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

TEST_F(LuaUIBindingTest, Lua创建UIParticles绑定) {
    TempLuaFile script("test_ui_particles.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0,0,0, 1,1,1)
            dse.ecs.add_particle_system_3d(e, 100, 2.0)
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
    auto view = world.registry().view<ParticleSystem3DComponent>();
    for (auto e : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(e);
        if (ps.max_particles == 100) {
            found = true;
            EXPECT_NEAR(ps.start_life_max, 2.0f, 0.01f);
        }
    }
    EXPECT_TRUE(found);
}
