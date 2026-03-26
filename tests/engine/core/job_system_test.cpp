#include "catch/catch.hpp"
#include "engine/core/job_system.h"
#include <atomic>
#include <future>
#include <chrono>

using core::JobSystem;

// 正向测试：初始化后提交任务应被执行，并在合理时间内完成。
TEST_CASE("Given_InitializedJobSystem_When_ExecuteJob_Then_JobRunsSuccessfully", "[engine][unit][job_system]") {
    JobSystem::Shutdown();
    JobSystem::Init();

    std::promise<void> done;
    auto finished = done.get_future();
    JobSystem::Execute([&done]() mutable {
        done.set_value();
    });

    REQUIRE(finished.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    JobSystem::Shutdown();
}

// 边界测试：提交空任务对象时应安全返回，不影响后续正常任务执行。
TEST_CASE("Given_EmptyJob_When_Execute_Then_NoCrashAndSubsequentJobsStillRun", "[engine][unit][job_system]") {
    JobSystem::Shutdown();
    JobSystem::Init();

    JobSystem::Execute(std::function<void()>{});

    std::atomic<int> value{0};
    std::promise<void> done;
    auto finished = done.get_future();
    JobSystem::Execute([&]() {
        value.fetch_add(1);
        done.set_value();
    });

    REQUIRE(finished.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    REQUIRE(value.load() == 1);
    JobSystem::Shutdown();
}

// 反向测试：系统未初始化或已关闭时提交任务，应回退到同步执行路径。
TEST_CASE("Given_ShutdownJobSystem_When_Execute_Then_JobFallsBackToInlineExecution", "[engine][unit][job_system]") {
    JobSystem::Shutdown();

    int value = 0;
    JobSystem::Execute([&]() {
        value = 99;
    });

    REQUIRE(value == 99);
    JobSystem::Shutdown();
}
