#include "catch/catch.hpp"
#include "engine/scripting/cpp/cpp_business_runtime.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"

using dse::runtime::CppBusinessHooks;
using dse::runtime::ConfigureCppBusinessHooks;
using dse::runtime::BootstrapCppBusiness;
using dse::runtime::TickCppBusiness;
using dse::runtime::ShutdownCppBusiness;

namespace {
class ScopedLogLevel {
public:
    explicit ScopedLogLevel(dse::debug::LogLevel level)
        : previous_level_(dse::debug::GetLogLevel()) {
        dse::debug::SetLogLevel(level);
    }
    ~ScopedLogLevel() {
        dse::debug::SetLogLevel(previous_level_);
    }

private:
    dse::debug::LogLevel previous_level_;
};
}

// 正向测试：完整配置 hooks 后应按生命周期顺序调用 bootstrap/tick/shutdown。
TEST_CASE("Given_ConfiguredHooks_When_RunLifecycle_Then_AllCallbacksAreInvoked", "[engine][unit][cpp_runtime]") {
    World world;
    int bootstrap_count = 0;
    int tick_count = 0;
    int shutdown_count = 0;
    float observed_dt = 0.0f;

    AssetManager asset_manager;

    ConfigureCppBusinessHooks(CppBusinessHooks{
        [&](World&, AssetManager&) { ++bootstrap_count; },
        [&](World&, float dt) {
            ++tick_count;
            observed_dt = dt;
        },
        [&]() { ++shutdown_count; }
    });

    REQUIRE(BootstrapCppBusiness(world, asset_manager));
    TickCppBusiness(world, 0.016f);
    ShutdownCppBusiness();

    REQUIRE(bootstrap_count == 1);
    REQUIRE(tick_count == 1);
    REQUIRE(observed_dt == Approx(0.016f));
    REQUIRE(shutdown_count == 1);
}

// 边界测试：仅配置 tick 回调时，Tick 仍应可执行，且 Shutdown 可安全跳过空回调。
TEST_CASE("Given_PartialHooks_When_TickAndShutdown_Then_BehaviorRemainsSafe", "[engine][unit][cpp_runtime]") {
    World world;
    int tick_count = 0;

    ConfigureCppBusinessHooks(CppBusinessHooks{
        nullptr,
        [&](World&, float) { ++tick_count; },
        nullptr
    });

    TickCppBusiness(world, 0.01f);
    ShutdownCppBusiness();
    REQUIRE(tick_count == 1);
}

// 反向测试：缺失 bootstrap 或 tick 配置时，Bootstrap 应返回失败。
TEST_CASE("Given_MissingRequiredHooks_When_Bootstrap_Then_ReturnFalse", "[engine][unit][cpp_runtime]") {
    World world;
    ScopedLogLevel mute_errors(dse::debug::LogLevel::Off);

    AssetManager asset_manager;

    ConfigureCppBusinessHooks(CppBusinessHooks{nullptr, nullptr, nullptr});
    REQUIRE_FALSE(BootstrapCppBusiness(world, asset_manager));

    ConfigureCppBusinessHooks(CppBusinessHooks{[](World&, AssetManager&) {}, nullptr, nullptr});
    REQUIRE_FALSE(BootstrapCppBusiness(world, asset_manager));
}
