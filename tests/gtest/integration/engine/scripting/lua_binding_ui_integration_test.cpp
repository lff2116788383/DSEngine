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

// ============================================================
// 新增 UI 组件 Lua 绑定集成测试
// ============================================================

TEST_F(LuaUIBindingTest, Lua创建TextInput并读写文本) {
    TempLuaFile script("test_ui_text_input.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ui.add_text_input(e, "请输入...", 100, false)
            dse.ui.set_text_input_text(e, "Hello World")
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
    auto view = world.registry().view<UITextInputComponent>();
    for (auto e : view) {
        auto& input = view.get<UITextInputComponent>(e);
        if (input.placeholder == "\xe8\xaf\xb7\xe8\xbe\x93\xe5\x85\xa5...") {
            found = true;
            EXPECT_EQ(input.text, "Hello World");
            EXPECT_EQ(input.max_length, 100);
            EXPECT_FALSE(input.is_password);
            EXPECT_EQ(input.cursor_position, 11); // strlen("Hello World")
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(LuaUIBindingTest, Lua创建ScrollView并设置偏移) {
    TempLuaFile script("test_ui_scroll.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ui.add_scroll_view(e, 800, 1200, false, true)
            dse.ui.set_scroll_offset(e, 0, 300)
            dse.ui.set_scroll_content_size(e, 800, 2000)
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
    auto view = world.registry().view<UIScrollViewComponent>();
    for (auto e : view) {
        auto& sv = view.get<UIScrollViewComponent>(e);
        found = true;
        EXPECT_NEAR(sv.scroll_offset.y, 300.0f, 0.01f);
        EXPECT_NEAR(sv.content_size.x, 800.0f, 0.01f);
        EXPECT_NEAR(sv.content_size.y, 2000.0f, 0.01f);
        EXPECT_FALSE(sv.horizontal);
        EXPECT_TRUE(sv.vertical);
    }
    EXPECT_TRUE(found);
}

TEST_F(LuaUIBindingTest, Lua创建Slider并读写值) {
    TempLuaFile script("test_ui_slider.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ui.add_slider(e, 0.0, 1.0, 0.5, false)
            dse.ui.set_slider_value(e, 0.75)
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
    auto view = world.registry().view<UISliderComponent>();
    for (auto e : view) {
        auto& slider = view.get<UISliderComponent>(e);
        found = true;
        EXPECT_NEAR(slider.value, 0.75f, 0.01f);
        EXPECT_NEAR(slider.min_value, 0.0f, 0.01f);
        EXPECT_NEAR(slider.max_value, 1.0f, 0.01f);
    }
    EXPECT_TRUE(found);
}

TEST_F(LuaUIBindingTest, Lua创建Toggle并切换状态) {
    TempLuaFile script("test_ui_toggle.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ui.add_toggle(e, false, -1)
            dse.ui.set_toggle(e, true)
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
    auto view = world.registry().view<UIToggleComponent>();
    for (auto e : view) {
        auto& toggle = view.get<UIToggleComponent>(e);
        found = true;
        EXPECT_TRUE(toggle.is_on);
        EXPECT_EQ(toggle.group, -1);
    }
    EXPECT_TRUE(found);
}

TEST_F(LuaUIBindingTest, Lua创建ProgressBar并设置进度) {
    TempLuaFile script("test_ui_progress.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ui.add_progress_bar(e, 0.0, 100.0)
            dse.ui.set_progress(e, 65.0)
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
    auto view = world.registry().view<UIProgressBarComponent>();
    for (auto e : view) {
        auto& bar = view.get<UIProgressBarComponent>(e);
        found = true;
        EXPECT_NEAR(bar.value, 65.0f, 0.01f);
        EXPECT_NEAR(bar.max_value, 100.0f, 0.01f);
        EXPECT_NEAR(bar.GetFillAmount(), 0.65f, 0.01f);
    }
    EXPECT_TRUE(found);
}

TEST_F(LuaUIBindingTest, 新UI组件错误参数不崩溃) {
    TempLuaFile script("test_ui_new_error.lua", R"(
        function Awake()
            dse.ui.add_text_input(99999, "bad", 10, false)
            dse.ui.set_text_input_text(99999, "test")
            dse.ui.get_text_input_text(99999)
            dse.ui.add_scroll_view(99999, 100, 100, true, true)
            dse.ui.set_scroll_offset(99999, 10, 10)
            dse.ui.add_slider(99999, 0.5, 0, 1, false)
            dse.ui.set_slider_value(99999, 0.5)
            dse.ui.get_slider_value(99999)
            dse.ui.add_toggle(99999, false, -1)
            dse.ui.set_toggle(99999, true)
            dse.ui.get_toggle(99999)
            dse.ui.add_progress_bar(99999, 0, 1)
            dse.ui.set_progress(99999, 0.5)
            dse.ui.get_progress(99999)
        end
        function Update(dt) end
    )");
    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx; ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
    // 不崩溃即通过
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
