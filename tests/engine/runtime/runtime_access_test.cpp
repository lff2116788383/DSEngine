#include "catch/catch.hpp"
#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/engine_app.h"
#include "engine/assets/asset_manager.h"
#include <type_traits>

// 回归用例：确保 Runtime 核心类型的构造/析构访问级别可被测试与业务代码正常使用。
TEST_CASE("Given_RuntimeCoreTypes_When_CheckAccessTraits_Then_ConstructAndDestructArePubliclyUsable", "[engine][unit][runtime]") {
    REQUIRE(std::is_default_constructible<FramePipeline>::value);
    REQUIRE(std::is_destructible<FramePipeline>::value);
    REQUIRE(std::is_destructible<AssetManager>::value);
}

TEST_CASE("Given_RuntimeServices_When_BuildingEngineRunConfig_Then_NewAndLegacyInjectionPathsRemainUsable", "[engine][unit][runtime]") {
    World explicit_world;
    AssetManager explicit_asset_manager;

    dse::runtime::EngineRunConfig config;
    config.WithServices({&explicit_world, &explicit_asset_manager});

    REQUIRE(config.services.world == &explicit_world);
    REQUIRE(config.services.asset_manager == &explicit_asset_manager);

    config.world = &explicit_world;
    config.asset_manager = &explicit_asset_manager;

    REQUIRE(config.world == &explicit_world);
    REQUIRE(config.asset_manager == &explicit_asset_manager);
}
