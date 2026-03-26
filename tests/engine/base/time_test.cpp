#include "catch/catch.hpp"
#include "engine/base/time.h"
#include <thread>
#include <chrono>

// 正向测试：初始化后时间应单调递增，固定步长可被正确设置与读取。
TEST_CASE("Given_TimeInitialized_When_QueryAndSetFixedStep_Then_TimeMonotonicAndStepApplied", "[engine][unit][time]") {
    Time::Init();
    const float startup_a = Time::TimeSinceStartup();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const float startup_b = Time::TimeSinceStartup();
    REQUIRE(startup_b >= startup_a);

    Time::set_fixed_update_time(0.02f);
    REQUIRE(Time::fixed_update_time() == Approx(0.02f));
}

// 边界测试：首次 Update 不应产生负增量时间，后续帧增量应为非负值。
TEST_CASE("Given_FirstAndSecondUpdate_When_QueryDeltaTime_Then_DeltaTimeIsNonNegative", "[engine][unit][time]") {
    Time::Init();
    Time::Update();
    REQUIRE(Time::delta_time() >= 0.0f);

    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    Time::Update();
    REQUIRE(Time::delta_time() >= 0.0f);
}

// 反向测试：将固定步长设置为负值时，接口应保持赋值结果以便上层自行防御。
TEST_CASE("Given_NegativeFixedStep_When_SetFixedUpdateTime_Then_ValueIsAssignedAsProvided", "[engine][unit][time]") {
    Time::set_fixed_update_time(-0.01f);
    REQUIRE(Time::fixed_update_time() == Approx(-0.01f));
}
