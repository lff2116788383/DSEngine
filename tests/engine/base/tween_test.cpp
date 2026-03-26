#include "catch/catch.hpp"
#include "engine/base/tween.h"

using dse::utils::EaseType;
using dse::utils::Tween;

// 正向测试：Evaluate 在关键采样点返回符合数学定义的缓动权重。
TEST_CASE("Given_KeySamplePoints_When_Evaluate_Then_ReturnExpectedEasingWeights", "[engine][unit][tween]") {
    REQUIRE(Tween::Evaluate(EaseType::Linear, 0.0f) == Approx(0.0f));
    REQUIRE(Tween::Evaluate(EaseType::Linear, 1.0f) == Approx(1.0f));

    REQUIRE(Tween::Evaluate(EaseType::EaseInQuad, 0.5f) == Approx(0.25f));
    REQUIRE(Tween::Evaluate(EaseType::EaseOutQuad, 0.5f) == Approx(0.75f));
    REQUIRE(Tween::Evaluate(EaseType::EaseInOutQuad, 0.5f) == Approx(0.5f));
}

// 正向测试：Lerp 在起点、中点、终点按缓动权重输出正确插值结果。
TEST_CASE("Given_StartEndAndEaseType_When_Lerp_Then_ReturnExpectedInterpolatedValue", "[engine][unit][tween]") {
    REQUIRE(Tween::Lerp(10.0f, 20.0f, 0.0f, EaseType::Linear) == Approx(10.0f));
    REQUIRE(Tween::Lerp(10.0f, 20.0f, 1.0f, EaseType::Linear) == Approx(20.0f));
    REQUIRE(Tween::Lerp(0.0f, 100.0f, 0.5f, EaseType::EaseInQuad) == Approx(25.0f));
    REQUIRE(Tween::Lerp(0.0f, 100.0f, 0.5f, EaseType::EaseOutQuad) == Approx(75.0f));
}

// 正向测试：各缓动函数在端点应与线性一致，确保插值起终点稳定。
TEST_CASE("Given_EaseTypeEndpoints_When_Evaluate_Then_AllCurvesMatchBoundaryValues", "[engine][unit][tween]") {
    REQUIRE(Tween::Evaluate(EaseType::EaseInQuad, 0.0f) == Approx(0.0f));
    REQUIRE(Tween::Evaluate(EaseType::EaseInQuad, 1.0f) == Approx(1.0f));
    REQUIRE(Tween::Evaluate(EaseType::EaseOutQuad, 0.0f) == Approx(0.0f));
    REQUIRE(Tween::Evaluate(EaseType::EaseOutQuad, 1.0f) == Approx(1.0f));
    REQUIRE(Tween::Evaluate(EaseType::EaseInOutQuad, 0.0f) == Approx(0.0f));
    REQUIRE(Tween::Evaluate(EaseType::EaseInOutQuad, 1.0f) == Approx(1.0f));
}

// 正向测试：在 [0,1] 区间采样时，缓动函数输出应保持单调不减。
TEST_CASE("Given_OrderedTimeSamples_When_Evaluate_Then_EasingValuesAreMonotonic", "[engine][unit][tween]") {
    const float samples[] = {0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f};
    auto assert_monotonic = [&](EaseType type) {
        float prev = Tween::Evaluate(type, samples[0]);
        for (size_t i = 1; i < sizeof(samples) / sizeof(samples[0]); ++i) {
            const float current = Tween::Evaluate(type, samples[i]);
            REQUIRE(current >= prev);
            prev = current;
        }
    };

    assert_monotonic(EaseType::Linear);
    assert_monotonic(EaseType::EaseInQuad);
    assert_monotonic(EaseType::EaseOutQuad);
    assert_monotonic(EaseType::EaseInOutQuad);
}

// 边界测试：超出 [0,1] 的时间参数会被夹紧，避免插值越界。
TEST_CASE("Given_OutOfRangeT_When_Evaluate_Then_ClampToUnitInterval", "[engine][unit][tween]") {
    REQUIRE(Tween::Evaluate(EaseType::Linear, -0.5f) == Approx(0.0f));
    REQUIRE(Tween::Evaluate(EaseType::Linear, 1.5f) == Approx(1.0f));
}

// 边界测试：Lerp 对越界时间参数同样应执行夹紧，返回边界插值结果。
TEST_CASE("Given_OutOfRangeT_When_Lerp_Then_ResultIsClampedToStartOrEnd", "[engine][unit][tween]") {
    REQUIRE(Tween::Lerp(3.0f, 9.0f, -1.0f, EaseType::EaseInQuad) == Approx(3.0f));
    REQUIRE(Tween::Lerp(3.0f, 9.0f, 2.0f, EaseType::EaseOutQuad) == Approx(9.0f));
}

// 反向测试：非法缓动类型应走兜底分支，行为与线性结果一致。
TEST_CASE("Given_InvalidEaseType_When_Evaluate_Then_FallbackToLinear", "[engine][unit][tween]") {
    const auto invalid_type = static_cast<EaseType>(999);
    REQUIRE(Tween::Evaluate(invalid_type, 0.25f) == Approx(0.25f));
    REQUIRE(Tween::Evaluate(invalid_type, 2.0f) == Approx(1.0f));
}

// 反向测试：非法缓动类型用于 Lerp 时应退化为线性插值并保持夹紧行为。
TEST_CASE("Given_InvalidEaseType_When_Lerp_Then_UseLinearInterpolationFallback", "[engine][unit][tween]") {
    const auto invalid_type = static_cast<EaseType>(999);
    REQUIRE(Tween::Lerp(0.0f, 10.0f, 0.3f, invalid_type) == Approx(3.0f));
    REQUIRE(Tween::Lerp(0.0f, 10.0f, 5.0f, invalid_type) == Approx(10.0f));
}
