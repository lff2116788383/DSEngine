/**
 * @file lua_binding_localization_integration_test.cpp
 * @brief Lua localization 绑定集成测试
 *
 * 验证：
 * - dse.localization.load / set_locale / get_locale / get / has_key / get_locales
 * - ServiceLocator 中的 LocalizationManager 与 Lua 绑定联动
 */

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
#include "engine/core/service_locator.h"
#include "engine/assets/localization_manager.h"
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

class LuaL10nBindingTest : public ::testing::Test {
protected:
    void SetUp() override {
        l10n_ = std::make_shared<assets::LocalizationManager>();
        core::ServiceLocator::Instance().Register<assets::LocalizationManager,
                                                   assets::LocalizationManager>(l10n_);
    }
    void TearDown() override {
        ShutdownLuaRuntime();
        core::ServiceLocator::Instance().Reset<assets::LocalizationManager>();
        l10n_.reset();
    }
    std::shared_ptr<assets::LocalizationManager> l10n_;
};

// 测试 Lua L 10 n绑定：Lua加载且查询Translations
TEST_F(LuaL10nBindingTest, LuaLoadAndQueryTranslations) {
    TempLuaFile script("test_l10n.lua", R"(
        _G.test_results = {}
        function Awake()
            local ok = dse.localization.load("zh-CN",
                '{"ui.ok":"确定","ui.cancel":"取消"}')
            table.insert(test_results, ok and "load_ok" or "load_fail")

            dse.localization.set_locale("zh-CN")
            table.insert(test_results, dse.localization.get_locale())

            table.insert(test_results, dse.localization.get("ui.ok", "fallback"))
            table.insert(test_results, dse.localization.get("missing", "默认值"))
            table.insert(test_results, dse.localization.has_key("ui.ok") and "yes" or "no")
            table.insert(test_results, dse.localization.has_key("missing") and "yes" or "no")

            local locales = dse.localization.get_locales()
            table.insert(test_results, tostring(#locales))
        end
        function Update(dt) end
    )");
    SetStartupLuaScriptPath(script.Path());
    World world;
    LuaApiContext ctx; ctx.world = &world;
    ConfigureLuaApiContext(ctx);
    ASSERT_TRUE(BootstrapLuaRuntime());
    TickLuaRuntime(0.016f);

    // 从 C++ 侧验证 LocalizationManager 状态
    EXPECT_EQ(l10n_->GetCurrentLocale(), "zh-CN");
    EXPECT_EQ(l10n_->Get("ui.ok"), "\xe7\xa1\xae\xe5\xae\x9a"); // "确定" UTF-8
    EXPECT_TRUE(l10n_->HasKey("ui.cancel"));
    EXPECT_FALSE(l10n_->HasKey("missing"));
}

// 测试 Lua L 10 n绑定：不注册管理器不崩溃
TEST_F(LuaL10nBindingTest, NotregisterManagerDoesNotCrash) {
    // Reset the manager so Lua gets nullptr
    core::ServiceLocator::Instance().Reset<assets::LocalizationManager>();
    l10n_.reset();

    TempLuaFile script("test_l10n_null.lua", R"(
        function Awake()
            local ok = dse.localization.load("en", '{"k":"v"}')
            -- should return false but not crash
            dse.localization.set_locale("en")
            dse.localization.get("k")
            dse.localization.has_key("k")
            dse.localization.get_locales()
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
