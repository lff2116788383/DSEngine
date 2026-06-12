/**
 * @file profiler_jobsystem_integration_test.cpp
 * @brief Profiler ↔ JobSystem / RenderStats / MemoryProfiler 集成测试
 *
 * 覆盖场景：
 *   1. CPUProfiler + JobSystem 异步任务性能采样
 *   2. CPUProfiler 多线程并发采样安全性
 *   3. RenderProfiler + CPUProfiler 联合帧模拟
 *   4. MemoryProfiler 记录资产分配释放后泄漏检测
 *   5. Chrome Trace 导出包含 JobSystem 任务和 CPU 采样
 *   6. Profiler 通过 ServiceLocator 跨模块访问
 *   7. 多帧累积统计准确性
 */

#include <gtest/gtest.h>
#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/memory_profiler.h"
#include "engine/profiler/render_profiler.h"
#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include <atomic>
#include <thread>
#include <chrono>

using namespace dse::profiler;
using namespace dse::core;

class ProfilerIntegrationTest : public ::testing::Test {
protected:
    CPUProfiler cpu_profiler;
    RenderProfiler render_profiler;
    MemoryProfiler memory_profiler;
    std::shared_ptr<JobSystem> job_system = std::make_shared<JobSystem>();

    void SetUp() override {
        job_system->Init();
    }
    void TearDown() override {
        job_system->Shutdown();
        ServiceLocator::Instance().Reset<CPUProfiler>();
        ServiceLocator::Instance().Reset<RenderProfiler>();
        ServiceLocator::Instance().Reset<MemoryProfiler>();
    }
};

TEST_F(ProfilerIntegrationTest, CPUProfilersamplingJobSystemAsynchronousTasksTakeTime) {
    cpu_profiler.BeginFrame();

    cpu_profiler.BeginSample("AsyncTask");
    std::atomic<int> result{0};
    auto handle = job_system->Submit([&result]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        result.store(42);
    }, JobPriority::Normal);
    job_system->Wait(handle);
    cpu_profiler.EndSample();

    cpu_profiler.EndFrame();

    EXPECT_EQ(result.load(), 42);
    const auto& stats = cpu_profiler.GetStats();
    ASSERT_TRUE(stats.count("AsyncTask"));
    EXPECT_GT(stats.at("AsyncTask").total_ms, 1.0);
    EXPECT_EQ(stats.at("AsyncTask").call_count, 1);
}

TEST_F(ProfilerIntegrationTest, CPUProfilerMultiThreadedConcurrentSamplingSafety) {
    constexpr int kThreadCount = 4;
    constexpr int kSamplesPerThread = 50;

    std::vector<JobHandle> handles;
    for (int t = 0; t < kThreadCount; ++t) {
        auto h = job_system->Submit([this, t]() {
            for (int i = 0; i < kSamplesPerThread; ++i) {
                cpu_profiler.BeginSample("Thread" + std::to_string(t));
                cpu_profiler.EndSample();
            }
        }, JobPriority::Normal);
        handles.push_back(h);
    }
    for (auto& h : handles) {
        job_system->Wait(h);
    }

    const auto& stats = cpu_profiler.GetStats();
    int total_calls = 0;
    for (const auto& [name, s] : stats) {
        total_calls += s.call_count;
    }
    EXPECT_EQ(total_calls, kThreadCount * kSamplesPerThread);
}

TEST_F(ProfilerIntegrationTest, RenderProfilerAndCPUProfilerjointFrameSimulation) {
    for (int frame = 0; frame < 5; ++frame) {
        cpu_profiler.BeginFrame();
        render_profiler.BeginFrame();

        cpu_profiler.BeginSample("Update");
        cpu_profiler.EndSample();

        cpu_profiler.BeginSample("Render");
        render_profiler.RecordDrawCall(300, 100);
        render_profiler.RecordDrawCall(600, 200);
        render_profiler.RecordSpriteBatch(32);
        render_profiler.RecordTextureBind();
        render_profiler.RecordShaderSwitch();
        cpu_profiler.EndSample();

        render_profiler.EndFrame();
        cpu_profiler.EndFrame();
    }

    const auto& cpu_stats = cpu_profiler.GetStats();
    EXPECT_TRUE(cpu_stats.count("Update"));
    EXPECT_TRUE(cpu_stats.count("Render"));
    EXPECT_EQ(cpu_stats.at("Render").call_count, 5);
    EXPECT_EQ(cpu_profiler.GetFrameStats().frame_count, 5);

    const auto& render_acc = render_profiler.GetAccumulatedStats();
    EXPECT_EQ(render_acc.frame_count, 5);
    // RecordSpriteBatch 内部也贡献 draw call + 顶点/三角形
    EXPECT_GE(render_acc.total_draw_calls, 10);
    EXPECT_GE(render_acc.total_triangles, 1500);
    EXPECT_GE(render_acc.total_vertices, 4500);
    EXPECT_GE(render_acc.peak_draw_calls, 2);
}

TEST_F(ProfilerIntegrationTest, MemoryProfilerAssetAllocationReleaseAndLeakDetection) {
    memory_profiler.RecordAlloc("Texture", 1024 * 1024);
    memory_profiler.RecordAlloc("Mesh", 512 * 1024);
    memory_profiler.RecordAlloc("Audio", 256 * 1024);

    auto snapshot = memory_profiler.GetSnapshot();
    EXPECT_EQ(snapshot.active_allocations, 3);
    EXPECT_EQ(snapshot.current_usage, (1024 + 512 + 256) * 1024);

    memory_profiler.RecordFree("Texture", 1024 * 1024);
    memory_profiler.RecordFree("Audio", 256 * 1024);

    auto leaks = memory_profiler.DetectLeaks();
    bool found_mesh_leak = false;
    for (const auto& tag : leaks) {
        if (tag == "Mesh") found_mesh_leak = true;
    }
    EXPECT_TRUE(found_mesh_leak);

    memory_profiler.RecordFree("Mesh", 512 * 1024);
    leaks = memory_profiler.DetectLeaks();
    EXPECT_TRUE(leaks.empty());
}

TEST_F(ProfilerIntegrationTest, ChromeTraceExportContainsFullFrameData) {
    cpu_profiler.BeginFrame();
    cpu_profiler.BeginSample("Physics");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    cpu_profiler.EndSample();
    cpu_profiler.EndFrame();

    std::string trace = cpu_profiler.ExportChromeTrace();
    ASSERT_FALSE(trace.empty());
    EXPECT_NE(trace.find("Physics"), std::string::npos);
    EXPECT_NE(trace.find("\"ph\""), std::string::npos);
    EXPECT_NE(trace.find("\"dur\""), std::string::npos);

    memory_profiler.RecordAlloc("TestBuffer", 4096);
    std::string mem_trace = memory_profiler.ExportChromeTrace();
    ASSERT_FALSE(mem_trace.empty());
    EXPECT_NE(mem_trace.find("TestBuffer"), std::string::npos);
}

TEST_F(ProfilerIntegrationTest, ProfilerpassServiceLocatorCrossModuleAccess) {
    auto shared_cpu = std::make_shared<CPUProfiler>();
    auto shared_render = std::make_shared<RenderProfiler>();
    auto shared_memory = std::make_shared<MemoryProfiler>();

    ServiceLocator::Instance().Register<CPUProfiler, CPUProfiler>(shared_cpu);
    ServiceLocator::Instance().Register<RenderProfiler, RenderProfiler>(shared_render);
    ServiceLocator::Instance().Register<MemoryProfiler, MemoryProfiler>(shared_memory);

    EXPECT_TRUE(ServiceLocator::Instance().Has<CPUProfiler>());
    EXPECT_TRUE(ServiceLocator::Instance().Has<RenderProfiler>());
    EXPECT_TRUE(ServiceLocator::Instance().Has<MemoryProfiler>());

    auto* cpu = ServiceLocator::Instance().Get<CPUProfiler>();
    ASSERT_NE(cpu, nullptr);
    cpu->BeginFrame();
    cpu->BeginSample("CrossModule");
    cpu->EndSample();
    cpu->EndFrame();

    EXPECT_EQ(cpu->GetStats().at("CrossModule").call_count, 1);

    auto* mem = ServiceLocator::Instance().Get<MemoryProfiler>();
    ASSERT_NE(mem, nullptr);
    mem->RecordAlloc("CrossModuleBuffer", 2048);
    EXPECT_EQ(mem->GetSnapshot().active_allocations, 1);
}

TEST_F(ProfilerIntegrationTest, MultiFrame) {
    for (int frame = 0; frame < 10; ++frame) {
        cpu_profiler.BeginFrame();
        cpu_profiler.BeginSample("Tick");
        cpu_profiler.EndSample();
        cpu_profiler.EndFrame();
    }

    const auto& fs = cpu_profiler.GetFrameStats();
    EXPECT_EQ(fs.frame_count, 10);
    EXPECT_GT(fs.avg_fps, 0.0);
    EXPECT_GT(fs.avg_frame_time_ms, 0.0);

    const auto& tick_stats = cpu_profiler.GetStats().at("Tick");
    EXPECT_EQ(tick_stats.call_count, 10);
    EXPECT_LE(tick_stats.min_ms, tick_stats.max_ms);
    EXPECT_GE(tick_stats.avg_ms, 0.0);
}
