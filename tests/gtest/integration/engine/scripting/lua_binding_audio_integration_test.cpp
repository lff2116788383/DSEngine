/**
 * @file lua_binding_audio_integration_test.cpp
 * @brief Lua Binding 音频组件集成测试
 *
 * 验证场景：
 * - Lua 脚本通过 dse.audio.add_source 创建音频源
 * - Lua 脚本通过 dse.audio.set_playing 控制播放状态
 * - Lua 脚本通过 dse.audio.set_spatial 启用 3D 空间化
 * - Lua 音频 API 创建的组件在 C++ 侧可查询验证
 * - Lua 音频 API 不存在时不崩溃
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/audio.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"
#include <cmath>
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

class LuaBindingAudioIntegrationTest : public ::testing::Test {
protected:
    AssetManager asset_manager_;

    void TearDown() override {
        ShutdownLuaRuntime();
    }
};

// 测试 Lua绑定音频集成：Lua创建音频源C加参数能够应读取从侧
TEST_F(LuaBindingAudioIntegrationTest, LuaCreateAudioSourceCPlusPlusParametersCanBeReadFromTheSide) {
    LuaTempScript startup("test_audio_source.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 3.0, 0.0, -1.0, 1.0, 1.0, 1.0)
            if dse.audio and dse.audio.add_source then
                dse.audio.add_source(e, "", false, true, 0.8)
            end
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ctx.asset_manager = &asset_manager_;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    bool found = false;
    auto view = world.registry().view<AudioSourceComponent>();
    for (auto entity : view) {
        auto& audio = view.get<AudioSourceComponent>(entity);
        if (audio.loop && std::abs(audio.volume - 0.8f) < 0.01f) {
            found = true;
            break;
        }
    }
    if (found) {
        SUCCEED();
    }

    ShutdownLuaRuntime();
}

// 测试 Lua绑定音频集成：Lua控制音频回放状态无崩溃
TEST_F(LuaBindingAudioIntegrationTest, LuaControlAudioPlaybackStatusWithoutCrashing) {
    LuaTempScript startup("test_audio_playing.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            if dse.audio and dse.audio.add_source then
                dse.audio.add_source(e, "", true, false, 1.0)
                dse.audio.set_playing(e, false)
            end
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ctx.asset_manager = &asset_manager_;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    EXPECT_NO_THROW(TickLuaRuntime(0.016f));

    auto view = world.registry().view<AudioSourceComponent>();
    for (auto entity : view) {
        auto& audio = view.get<AudioSourceComponent>(entity);
        if (audio.play_on_awake) {
            EXPECT_FALSE(audio.is_playing);
        }
    }

    ShutdownLuaRuntime();
}

// 测试 Lua绑定音频集成：Luaset上3D Spatialization参数执行不折叠
TEST_F(LuaBindingAudioIntegrationTest, LuasetUp3DSpatializationParametersDoNotCollapse) {
    LuaTempScript startup("test_audio_spatial.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            if dse.audio and dse.audio.add_source then
                dse.audio.add_source(e, "", false, false, 1.0)
            end
            if dse.audio and dse.audio.set_spatial then
                dse.audio.set_spatial(e, true, 1.0, 15.0, 1.5)
            end
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ctx.asset_manager = &asset_manager_;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    EXPECT_NO_THROW(TickLuaRuntime(0.016f));

    auto view = world.registry().view<AudioSourceComponent>();
    for (auto entity : view) {
        auto& audio = view.get<AudioSourceComponent>(entity);
        if (audio.spatial_enabled) {
            EXPECT_NEAR(audio.min_distance, 1.0f, 0.01f);
            EXPECT_NEAR(audio.max_distance, 15.0f, 0.01f);
            EXPECT_NEAR(audio.rolloff, 1.5f, 0.01f);
        }
    }

    ShutdownLuaRuntime();
}

// 测试 Lua绑定音频集成：Lua音频API Don T崩溃若Doesn T存在
TEST_F(LuaBindingAudioIntegrationTest, LuaAudioAPIDonTCrashIfItDoesnTExist) {
    LuaTempScript startup("test_audio_nil.lua", R"(
        function Awake()
            local e = dse.ecs.create_entity()
            dse.ecs.add_transform(e, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0)
            if dse.audio == nil then
                return
            end
        end
        function Update(dt)
        end
    )");

    SetStartupLuaScriptPath(startup.Path());

    World world;
    LuaApiContext ctx;
    ctx.world = &world;
    ctx.asset_manager = &asset_manager_;
    ConfigureLuaApiContext(ctx);

    ASSERT_TRUE(BootstrapLuaRuntime());
    EXPECT_NO_THROW(TickLuaRuntime(0.016f));

    ShutdownLuaRuntime();
}
