#include "catch/catch.hpp"
#include "engine/runtime/frame_pipeline.h"
#include "engine/assets/asset_manager.h"
#include <type_traits>

// 回归用例：确保 Runtime 核心类型的构造/析构访问级别可被测试与业务代码正常使用。
TEST_CASE("Given_RuntimeCoreTypes_When_CheckAccessTraits_Then_ConstructAndDestructArePubliclyUsable", "[engine][unit][runtime]") {
    REQUIRE(std::is_default_constructible<FramePipeline>::value);
    REQUIRE(std::is_destructible<FramePipeline>::value);
    REQUIRE(std::is_destructible<AssetManager>::value);
}
