/**
 * @file lua_binding_dssl_integration_test.cpp
 * @brief Lua DSSL 材质绑定集成测试
 *
 * 验证场景：
 * - dssl.load_material 不存在文件返回 nil
 * - dssl.set_float / set_color 错误参数不崩溃
 * - dssl 表存在
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/core/service_locator.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
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

class LuaDSSLBindingTest : public ::testing::Test {
protected:
    void TearDown() override { ShutdownLuaRuntime(); }
};

// 测试 Lua DSSL绑定：DSS Ltable存在
TEST_F(LuaDSSLBindingTest, DSSLtableExists) {
    TempLuaFile script("test_dssl_exists.lua", R"(
        function Awake()
            assert(type(dssl) == "table", "dssl table must exist")
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

// 测试 Lua DSSL绑定：加载材质返回若文件不存在Nil
TEST_F(LuaDSSLBindingTest, LoadMaterialReturnIfFileDoesNotExistNil) {
    TempLuaFile script("test_dssl_nil.lua", R"(
        function Awake()
            local mid = dssl.load_material("not_exist.dssl")
            assert(mid == nil, "non-existent file should return nil")
        end
        function Update(dt) end
    )");
    SetStartupLuaScriptPath(script.Path());
    World world;
    AssetManager asset_mgr;
    LuaApiContext ctx; ctx.world = &world; ctx.asset_manager = &asset_mgr;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);
}

// 测试 Lua DSSL绑定：设置浮点错误ID不崩溃
TEST_F(LuaDSSLBindingTest, SetFloatErrorIDDoesNotCrash) {
    TempLuaFile script("test_dssl_error.lua", R"(
        function Awake()
            -- 不存在的 material_id，应安全返回
            dssl.set_float(999999, "roughness", 0.5)
            dssl.set_color(999999, "albedo_color", 1.0, 0.0, 0.0, 1.0)
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
