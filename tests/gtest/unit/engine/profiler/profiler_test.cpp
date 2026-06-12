/**
 * @file profiler_test.cpp
 * @brief CPUProfiler / MemoryProfiler / RenderProfiler 性能分析器单元测试
 *
 * 覆盖场景：
 * - CPUProfiler: BeginSample/EndSample, BeginFrame/EndFrame, Stats, Export, ChromeTrace
 * - MemoryProfiler: RecordAlloc/RecordFree, Snapshot, DetectLeaks, Reset, ChromeTrace
 * - RenderProfiler: RecordDrawCall/SpriteBatch/TextureBind/ShaderSwitch, Accumulated, Reset, ChromeTrace
 * - 性能基线: 各 Profiler 操作开销、Chrome Trace 导出性能
 */

#include <gtest/gtest.h>
#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/memory_profiler.h"
#include "engine/profiler/render_profiler.h"
#include <thread>
#include <chrono>

using namespace dse::profiler;

// ============================================================
// CPUProfiler 测试
// ============================================================

// 测试 CPU性能分析器：状态无
TEST(CPUProfilerTest, StateWithout) {
    CPUProfiler profiler;
    EXPECT_TRUE(profiler.GetStats().empty());
    EXPECT_EQ(profiler.GetFrameStats().frame_count, 0);
}

// 测试 CPU性能分析器：开始结束采样记录统计
TEST(CPUProfilerTest, BeginEndSampleRecordStatistics) {
    CPUProfiler profiler;
    profiler.BeginSample("TestSample");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    profiler.EndSample();

    const auto& stats = profiler.GetStats();
    ASSERT_TRUE(stats.find("TestSample") != stats.end());
    const auto& stat = stats.at("TestSample");
    EXPECT_EQ(stat.call_count, 1);
    EXPECT_GT(stat.total_ms, 0.0);
    EXPECT_GT(stat.avg_ms, 0.0);
}

// 测试 CPU性能分析器：多Timessampling
TEST(CPUProfilerTest, MultiTimessampling) {
    CPUProfiler profiler;
    for (int i = 0; i < 5; ++i) {
        profiler.BeginSample("Loop");
        profiler.EndSample();
    }

    const auto& stat = profiler.GetStats().at("Loop");
    EXPECT_EQ(stat.call_count, 5);
}

// 测试 CPU性能分析器：开始帧结束帧更新帧统计
TEST(CPUProfilerTest, BeginFrameEndFrameUpdateFrameStatistics) {
    CPUProfiler profiler;
    profiler.BeginFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    profiler.EndFrame();

    const auto& fs = profiler.GetFrameStats();
    EXPECT_EQ(fs.frame_count, 1);
    EXPECT_GT(fs.frame_time_ms, 0.0);
    EXPECT_GT(fs.fps, 0.0);
}

// 测试 CPU性能分析器：多帧
TEST(CPUProfilerTest, MultiFrame) {
    CPUProfiler profiler;
    for (int i = 0; i < 3; ++i) {
        profiler.BeginFrame();
        profiler.EndFrame();
    }

    EXPECT_EQ(profiler.GetFrameStats().frame_count, 3);
}

// 测试 CPU性能分析器：重置清空全部统计
TEST(CPUProfilerTest, ResetClearAllStatistics) {
    CPUProfiler profiler;
    profiler.BeginSample("S1");
    profiler.EndSample();
    profiler.BeginFrame();
    profiler.EndFrame();

    profiler.Reset();
    EXPECT_TRUE(profiler.GetStats().empty());
    EXPECT_EQ(profiler.GetFrameStats().frame_count, 0);
}

// 测试 CPU性能分析器：CSV非空
TEST(CPUProfilerTest, CSVNonEmpty) {
    CPUProfiler profiler;
    profiler.BeginSample("CSVTest");
    profiler.EndSample();

    std::string csv = profiler.ExportCSV();
    EXPECT_FALSE(csv.empty());
    EXPECT_NE(csv.find("CSVTest"), std::string::npos);
}

// 测试 CPU性能分析器：JSON非空
TEST(CPUProfilerTest, JSONNonEmpty) {
    CPUProfiler profiler;
    profiler.BeginSample("JSONTest");
    profiler.EndSample();

    std::string json = profiler.ExportJSON();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("JSONTest"), std::string::npos);
}

// 测试 CPU性能分析器：结束采样无匹配开始不崩溃
TEST(CPUProfilerTest, EndSampleNoMatchBeginDoesNotCrash) {
    CPUProfiler profiler;
    EXPECT_NO_THROW(profiler.EndSample());
}

// 测试 CPU性能分析器：Scoped CPU Profile Automatically结束采样
TEST(CPUProfilerTest, ScopedCPUProfileAutomaticallyEndSampling) {
    CPUProfiler profiler;
    {
        ScopedCPUProfile scope(profiler, "ScopedTest");
    }
    // 离开作用域后应自动 EndSample
    EXPECT_TRUE(profiler.GetStats().find("ScopedTest") != profiler.GetStats().end());
}

// ============================================================
// MemoryProfiler 测试
// ============================================================

// 测试 内存性能分析器：记录Allocincrease统计
TEST(MemoryProfilerTest, RecordAllocincreaseStatistics) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Texture", 1024);
    profiler.RecordAlloc("Texture", 2048);

    auto snap = profiler.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 3072u);
    EXPECT_EQ(snap.current_usage, 3072u);
    EXPECT_EQ(snap.active_allocations, 2);
}

// 测试 内存性能分析器：记录释放Reduce用量
TEST(MemoryProfilerTest, RecordFreeReduceUsage) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Buffer", 1024);
    profiler.RecordFree("Buffer", 512);

    auto snap = profiler.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 1024u);
    EXPECT_EQ(snap.total_freed, 512u);
    EXPECT_EQ(snap.current_usage, 512u);
    EXPECT_EQ(snap.active_allocations, 0);
}

// 测试 内存性能分析器：峰值Usagerecord峰值
TEST(MemoryProfilerTest, PeakUsagerecordPeak) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Mesh", 1000);
    profiler.RecordAlloc("Mesh", 2000);
    auto snap1 = profiler.GetSnapshot();
    EXPECT_EQ(snap1.peak_usage, 3000u);

    profiler.RecordFree("Mesh", 2000);
    auto snap2 = profiler.GetSnapshot();
    EXPECT_EQ(snap2.current_usage, 1000u);
    EXPECT_EQ(snap2.peak_usage, 3000u); // 峰值不变
}

// 测试 内存性能分析器：Category统计Classification统计
TEST(MemoryProfilerTest, CategoryStatsClassificationStatistics) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("GPU", 1024);
    profiler.RecordAlloc("CPU", 512);

    const auto& cats = profiler.GetCategoryStats();
    ASSERT_TRUE(cats.find("GPU") != cats.end());
    ASSERT_TRUE(cats.find("CPU") != cats.end());
    EXPECT_EQ(cats.at("GPU").current_bytes, 1024u);
    EXPECT_EQ(cats.at("CPU").current_bytes, 512u);
}

// 测试 内存性能分析器：检测泄漏检测泄漏
TEST(MemoryProfilerTest, DetectLeaksDetectLeaks) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("LeakedTag", 100);
    // 只分配不释放
    auto leaks = profiler.DetectLeaks();
    EXPECT_EQ(leaks.size(), 1u);
    EXPECT_EQ(leaks[0], "LeakedTag");
}

// 测试 内存性能分析器：无泄漏当检测泄漏返回空
TEST(MemoryProfilerTest, WithoutLeakWhenDetectLeaksReturnsEmpty) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Clean", 100);
    profiler.RecordFree("Clean", 100);
    auto leaks = profiler.DetectLeaks();
    EXPECT_TRUE(leaks.empty());
}

// 测试 内存性能分析器：重置清空全部统计
TEST(MemoryProfilerTest, ResetClearAllStatistics) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Test", 100);
    profiler.Reset();

    auto snap = profiler.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 0u);
    EXPECT_EQ(snap.current_usage, 0u);
    EXPECT_EQ(snap.active_allocations, 0);
}

// 测试 内存性能分析器：CSV非空
TEST(MemoryProfilerTest, CSVNonEmpty) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("CSV", 256);
    std::string csv = profiler.ExportCSV();
    EXPECT_FALSE(csv.empty());
    EXPECT_NE(csv.find("CSV"), std::string::npos);
}

// ============================================================
// RenderProfiler 测试
// ============================================================

// 测试 渲染性能分析器：初始状态为零
TEST(RenderProfilerTest, TheInitialStateIsZero) {
    RenderProfiler profiler;
    auto& frame = profiler.GetCurrentFrameStats();
    EXPECT_EQ(frame.draw_calls, 0);
    EXPECT_EQ(frame.triangle_count, 0);
    EXPECT_EQ(frame.vertex_count, 0);
}

// 测试 渲染性能分析器：调用
TEST(RenderProfilerTest, Calls) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.RecordDrawCall(200, 100);

    auto& frame = profiler.GetCurrentFrameStats();
    EXPECT_EQ(frame.draw_calls, 2);
    EXPECT_EQ(frame.vertex_count, 300);
    EXPECT_EQ(frame.triangle_count, 150);
}

// 测试 渲染性能分析器：次数Statistically正确
TEST(RenderProfilerTest, TimesStatisticallyCorrect) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordSpriteBatch(10);

    auto& frame = profiler.GetCurrentFrameStats();
    EXPECT_EQ(frame.sprite_count, 10);
    EXPECT_EQ(frame.batch_count, 1);
    EXPECT_EQ(frame.draw_calls, 1);
    EXPECT_EQ(frame.vertex_count, 40);   // 10 * 4
    EXPECT_EQ(frame.triangle_count, 20); // 10 * 2
}

// 测试 渲染性能分析器：绑定且
TEST(RenderProfilerTest, BindingAnd) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordTextureBind();
    profiler.RecordTextureBind();
    profiler.RecordShaderSwitch();

    auto& frame = profiler.GetCurrentFrameStats();
    EXPECT_EQ(frame.texture_binds, 2);
    EXPECT_EQ(frame.shader_switches, 1);
}

// 测试 渲染性能分析器：设置上内部
TEST(RenderProfilerTest, SetUpInside) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.SetTextureMemory(1024 * 1024);

    EXPECT_EQ(profiler.GetCurrentFrameStats().texture_memory, 1024u * 1024);
}

// 测试 渲染性能分析器：帧
TEST(RenderProfilerTest, Frame) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();

    auto& acc = profiler.GetAccumulatedStats();
    EXPECT_EQ(acc.frame_count, 1);
    EXPECT_EQ(acc.total_draw_calls, 1);
    EXPECT_EQ(acc.total_triangles, 50);
    EXPECT_EQ(acc.total_vertices, 100);
}

// 测试 渲染性能分析器：多帧峰值
TEST(RenderProfilerTest, MultiFramePeak) {
    RenderProfiler profiler;

    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();

    profiler.BeginFrame();
    profiler.RecordDrawCall(200, 100);
    profiler.RecordDrawCall(150, 75);
    profiler.EndFrame();

    auto& acc = profiler.GetAccumulatedStats();
    EXPECT_EQ(acc.frame_count, 2);
    EXPECT_EQ(acc.peak_draw_calls, 2);
    EXPECT_EQ(acc.peak_triangles, 175);
    EXPECT_EQ(acc.peak_vertices, 350);
}

// 测试 渲染性能分析器：重置清空全部统计
TEST(RenderProfilerTest, ResetClearAllStatistics) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();
    profiler.Reset();

    EXPECT_EQ(profiler.GetAccumulatedStats().frame_count, 0);
    EXPECT_EQ(profiler.GetCurrentFrameStats().draw_calls, 0);
}

// 测试 渲染性能分析器：CSV非空
TEST(RenderProfilerTest, CSVNonEmpty) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();

    std::string csv = profiler.ExportCSV();
    EXPECT_FALSE(csv.empty());
    EXPECT_NE(csv.find("DrawCalls"), std::string::npos);
}

// ============================================================
// Chrome Trace 导出测试 — CPUProfiler
// ============================================================

// 测试 CPU性能分析器：导出Chrome追踪正确格式
TEST(CPUProfilerTest, ExportChromeTraceCorrectFormat) {
    CPUProfiler profiler;
    profiler.BeginFrame();
    profiler.BeginSample("TraceTest");
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    profiler.EndSample();
    profiler.EndFrame();

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_FALSE(trace.empty());
    EXPECT_EQ(trace.front(), '[');
    EXPECT_EQ(trace.back(), ']');
    EXPECT_NE(trace.find("TraceTest"), std::string::npos);
    EXPECT_NE(trace.find("\"ph\":\"X\""), std::string::npos);
    EXPECT_NE(trace.find("\"cat\":\"cpu\""), std::string::npos);
    EXPECT_NE(trace.find("\"ts\":"), std::string::npos);
    EXPECT_NE(trace.find("\"dur\":"), std::string::npos);
    EXPECT_NE(trace.find("\"pid\":"), std::string::npos);
    EXPECT_NE(trace.find("\"tid\":"), std::string::npos);
}

// 测试 CPU性能分析器：导出Chrome Tracemulti采样
TEST(CPUProfilerTest, ExportChromeTracemultiSampling) {
    CPUProfiler profiler;
    profiler.BeginSample("A");
    profiler.EndSample();
    profiler.BeginSample("B");
    profiler.EndSample();

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_NE(trace.find("\"name\":\"A\""), std::string::npos);
    EXPECT_NE(trace.find("\"name\":\"B\""), std::string::npos);
}

// 测试 CPU性能分析器：导出Chrome追踪空数据返回空数组
TEST(CPUProfilerTest, ExportChromeTraceEmptyDataReturnsAnEmptyArray) {
    CPUProfiler profiler;
    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace, "[\n\n]");
}

// 测试 CPU性能分析器：重置清空Chrome追踪数据
TEST(CPUProfilerTest, ResetClearChromeTracedata) {
    CPUProfiler profiler;
    profiler.BeginSample("X");
    profiler.EndSample();
    profiler.Reset();

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace.find("\"name\":\"X\""), std::string::npos);
}

// ============================================================
// Chrome Trace 导出测试 — MemoryProfiler
// ============================================================

// 测试 内存性能分析器：导出Chrome追踪正确格式
TEST(MemoryProfilerTest, ExportChromeTraceCorrectFormat) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("TraceTag", 1024);

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_FALSE(trace.empty());
    EXPECT_EQ(trace.front(), '[');
    EXPECT_EQ(trace.back(), ']');
    EXPECT_NE(trace.find("alloc:TraceTag"), std::string::npos);
    EXPECT_NE(trace.find("\"ph\":\"i\""), std::string::npos);
    EXPECT_NE(trace.find("memory_usage"), std::string::npos);
    EXPECT_NE(trace.find("\"ph\":\"C\""), std::string::npos);
}

// 测试 内存性能分析器：导出Chrome追踪记录分配且Deallocation
TEST(MemoryProfilerTest, ExportChromeTraceRecordAllocationAndDeallocation) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Buf", 100);
    profiler.RecordFree("Buf", 100);

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_NE(trace.find("alloc:Buf"), std::string::npos);
    EXPECT_NE(trace.find("free:Buf"), std::string::npos);
}

// 测试 内存性能分析器：导出Chrome追踪空数据返回空数组
TEST(MemoryProfilerTest, ExportChromeTraceEmptyDataReturnsAnEmptyArray) {
    MemoryProfiler profiler;
    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace, "[\n\n]");
}

// 测试 内存性能分析器：重置清空Chrome追踪数据
TEST(MemoryProfilerTest, ResetClearChromeTracedata) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("X", 100);
    profiler.Reset();
    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace.find("alloc:X"), std::string::npos);
}

// ============================================================
// Chrome Trace 导出测试 — RenderProfiler
// ============================================================

// 测试 渲染性能分析器：导出Chrome追踪正确格式
TEST(RenderProfilerTest, ExportChromeTraceCorrectFormat) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_FALSE(trace.empty());
    EXPECT_EQ(trace.front(), '[');
    EXPECT_EQ(trace.back(), ']');
    EXPECT_NE(trace.find("render_stats"), std::string::npos);
    EXPECT_NE(trace.find("\"ph\":\"C\""), std::string::npos);
    EXPECT_NE(trace.find("\"draw_calls\":"), std::string::npos);
}

// 测试 渲染性能分析器：导出Chrome追踪多个帧录制
TEST(RenderProfilerTest, ExportChromeTraceMultipleFrameRecording) {
    RenderProfiler profiler;
    for (int i = 0; i < 3; ++i) {
        profiler.BeginFrame();
        profiler.RecordDrawCall(100, 50);
        profiler.EndFrame();
    }

    std::string trace = profiler.ExportChromeTrace();
    size_t count = 0;
    size_t pos = 0;
    while ((pos = trace.find("render_stats", pos)) != std::string::npos) {
        ++count;
        pos += 12;
    }
    EXPECT_EQ(count, 3u);
}

// 测试 渲染性能分析器：导出Chrome追踪空数据包含GPU线程元数据
TEST(RenderProfilerTest, ExportChromeTraceEmptyDataContainsGPUThreadMetadata) {
    RenderProfiler profiler;
    std::string trace = profiler.ExportChromeTrace();
    EXPECT_NE(trace.find("\"name\":\"thread_name\""), std::string::npos);
    EXPECT_NE(trace.find("CPU Render"), std::string::npos);
    EXPECT_NE(trace.find("GPU"), std::string::npos);
    EXPECT_EQ(trace.find("render_stats"), std::string::npos);
}

// 测试 渲染性能分析器：重置清空Chrome追踪数据
TEST(RenderProfilerTest, ResetClearChromeTracedata) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();
    profiler.Reset();

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace.find("render_stats"), std::string::npos);
}

// ============================================================
// GPU Timer 集成测试 — RenderProfiler
// ============================================================

// 测试 渲染性能分析器：更新GPU Timers写入手柄Udata
TEST(RenderProfilerTest, UpdateGpuTimersWriteGPUdata) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(10, 5);

    std::vector<GpuPassTiming> timings = {
        {"ShadowMap", 1.5f},
        {"GBuffer", 2.3f},
        {"Lighting", 0.8f},
    };
    profiler.UpdateGpuTimers(timings);
    profiler.EndFrame();

    const auto& result = profiler.GetGpuPassTimings();
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0].name, "ShadowMap");
    EXPECT_FLOAT_EQ(result[0].duration_ms, 1.5f);
    EXPECT_EQ(result[1].name, "GBuffer");
    EXPECT_EQ(result[2].name, "Lighting");
}

// 测试 渲染性能分析器：GPU总计时间Spent写入帧统计
TEST(RenderProfilerTest, GPUTotalTimeSpentWritingFrameStats) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.UpdateGpuTimers({{"A", 1.0f}, {"B", 2.0f}});
    profiler.EndFrame();

    EXPECT_FLOAT_EQ(profiler.GetCurrentFrameStats().total_gpu_time_ms, 3.0f);
}

// 测试 渲染性能分析器：GPU计时Appears在Chrome追踪Exporting
TEST(RenderProfilerTest, GPUTimingAppearsAtChromeTraceExporting) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.UpdateGpuTimers({{"ShadowPass", 1.23f}});
    profiler.EndFrame();

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_NE(trace.find("ShadowPass"), std::string::npos);
    EXPECT_NE(trace.find("\"cat\":\"gpu\""), std::string::npos);
    EXPECT_NE(trace.find("\"ph\":\"X\""), std::string::npos);
    EXPECT_NE(trace.find("\"tid\":2"), std::string::npos);
}

// 测试 渲染性能分析器：GPU计时Appears在CSV Exporting
TEST(RenderProfilerTest, GPUTimingAppearsAtCSVExporting) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.UpdateGpuTimers({{"ForwardPass", 0.75f}});
    profiler.EndFrame();

    std::string csv = profiler.ExportCSV();
    EXPECT_NE(csv.find("GpuTimeMs"), std::string::npos);
    EXPECT_NE(csv.find("GPU:ForwardPass"), std::string::npos);
}

// 测试 渲染性能分析器：重置清空手柄Utiming数据
TEST(RenderProfilerTest, ResetClearGPUtimingData) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.UpdateGpuTimers({{"TestPass", 1.0f}});
    profiler.EndFrame();
    profiler.Reset();

    EXPECT_TRUE(profiler.GetGpuPassTimings().empty());
    EXPECT_FLOAT_EQ(profiler.GetCurrentFrameStats().total_gpu_time_ms, 0.0f);
}

// 测试 渲染性能分析器：无手柄Udata当不Chrome追踪
TEST(RenderProfilerTest, WithoutGPUdataWhenNotChromeTrace) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(10, 5);
    profiler.EndFrame();

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_NE(trace.find("render_stats"), std::string::npos);
    EXPECT_EQ(trace.find("\"cat\":\"gpu\""), std::string::npos);
}

// ============================================================
// 性能基线测试
// ============================================================

// 测试 性能分析器基准：CPU采样开销为Lower比100微秒
TEST(ProfilerBenchmark, CPUSamplingOverheadIsLowerThan100microseconds) {
    CPUProfiler profiler;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        profiler.BeginSample("BenchmarkSample");
        profiler.EndSample();
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double per_op_us = (elapsed_ms * 1000.0) / 10000.0;
    EXPECT_LT(per_op_us, 100.0) << "per-op: " << per_op_us << " us";
}

// 测试 性能分析器基准：内存Logging开销为小于比100微秒
TEST(ProfilerBenchmark, MemoryLoggingOverheadIsLessThan100microseconds) {
    MemoryProfiler profiler;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        profiler.RecordAlloc("Bench", 1024);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double per_op_us = (elapsed_ms * 1000.0) / 10000.0;
    EXPECT_LT(per_op_us, 100.0) << "per-op: " << per_op_us << " us";
}

// 测试 性能分析器基准：渲染Logging开销为小于比100微秒
TEST(ProfilerBenchmark, RenderLoggingOverheadIsLessThan100microseconds) {
    RenderProfiler profiler;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        profiler.BeginFrame();
        profiler.RecordDrawCall(100, 50);
        profiler.EndFrame();
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double per_op_us = (elapsed_ms * 1000.0) / 10000.0;
    EXPECT_LT(per_op_us, 100.0) << "per-op: " << per_op_us << " us";
}

// 测试 性能分析器基准：Chrome追踪导出10000 Pieces的数据Below 200 millisecond
TEST(ProfilerBenchmark, ChromeTraceExport10000PiecesOfDataBelow200millisecond) {
    CPUProfiler profiler;
    for (int i = 0; i < 10000; ++i) {
        profiler.BeginSample("BenchExport");
        profiler.EndSample();
    }
    auto start = std::chrono::high_resolution_clock::now();
    std::string trace = profiler.ExportChromeTrace();
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(elapsed_ms, 200.0) << "export 10000 samples: " << elapsed_ms << " ms";
    EXPECT_FALSE(trace.empty());
}
