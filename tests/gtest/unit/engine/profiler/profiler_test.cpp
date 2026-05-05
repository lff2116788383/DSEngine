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

TEST(CPUProfilerTest, 初始状态无统计) {
    CPUProfiler profiler;
    EXPECT_TRUE(profiler.GetStats().empty());
    EXPECT_EQ(profiler.GetFrameStats().frame_count, 0);
}

TEST(CPUProfilerTest, BeginEndSample记录统计) {
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

TEST(CPUProfilerTest, 多次采样累积统计) {
    CPUProfiler profiler;
    for (int i = 0; i < 5; ++i) {
        profiler.BeginSample("Loop");
        profiler.EndSample();
    }

    const auto& stat = profiler.GetStats().at("Loop");
    EXPECT_EQ(stat.call_count, 5);
}

TEST(CPUProfilerTest, BeginFrameEndFrame更新帧统计) {
    CPUProfiler profiler;
    profiler.BeginFrame();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    profiler.EndFrame();

    const auto& fs = profiler.GetFrameStats();
    EXPECT_EQ(fs.frame_count, 1);
    EXPECT_GT(fs.frame_time_ms, 0.0);
    EXPECT_GT(fs.fps, 0.0);
}

TEST(CPUProfilerTest, 多帧累积) {
    CPUProfiler profiler;
    for (int i = 0; i < 3; ++i) {
        profiler.BeginFrame();
        profiler.EndFrame();
    }

    EXPECT_EQ(profiler.GetFrameStats().frame_count, 3);
}

TEST(CPUProfilerTest, Reset清除所有统计) {
    CPUProfiler profiler;
    profiler.BeginSample("S1");
    profiler.EndSample();
    profiler.BeginFrame();
    profiler.EndFrame();

    profiler.Reset();
    EXPECT_TRUE(profiler.GetStats().empty());
    EXPECT_EQ(profiler.GetFrameStats().frame_count, 0);
}

TEST(CPUProfilerTest, 导出CSV非空) {
    CPUProfiler profiler;
    profiler.BeginSample("CSVTest");
    profiler.EndSample();

    std::string csv = profiler.ExportCSV();
    EXPECT_FALSE(csv.empty());
    EXPECT_NE(csv.find("CSVTest"), std::string::npos);
}

TEST(CPUProfilerTest, 导出JSON非空) {
    CPUProfiler profiler;
    profiler.BeginSample("JSONTest");
    profiler.EndSample();

    std::string json = profiler.ExportJSON();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("JSONTest"), std::string::npos);
}

TEST(CPUProfilerTest, EndSample无匹配Begin不崩溃) {
    CPUProfiler profiler;
    EXPECT_NO_THROW(profiler.EndSample());
}

TEST(CPUProfilerTest, ScopedCPUProfile自动结束采样) {
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

TEST(MemoryProfilerTest, RecordAlloc增加统计) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Texture", 1024);
    profiler.RecordAlloc("Texture", 2048);

    auto snap = profiler.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 3072u);
    EXPECT_EQ(snap.current_usage, 3072u);
    EXPECT_EQ(snap.active_allocations, 2);
}

TEST(MemoryProfilerTest, RecordFree减少使用量) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Buffer", 1024);
    profiler.RecordFree("Buffer", 512);

    auto snap = profiler.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 1024u);
    EXPECT_EQ(snap.total_freed, 512u);
    EXPECT_EQ(snap.current_usage, 512u);
    EXPECT_EQ(snap.active_allocations, 0);
}

TEST(MemoryProfilerTest, PeakUsage记录峰值) {
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

TEST(MemoryProfilerTest, CategoryStats分类统计) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("GPU", 1024);
    profiler.RecordAlloc("CPU", 512);

    const auto& cats = profiler.GetCategoryStats();
    ASSERT_TRUE(cats.find("GPU") != cats.end());
    ASSERT_TRUE(cats.find("CPU") != cats.end());
    EXPECT_EQ(cats.at("GPU").current_bytes, 1024u);
    EXPECT_EQ(cats.at("CPU").current_bytes, 512u);
}

TEST(MemoryProfilerTest, DetectLeaks检测泄漏) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("LeakedTag", 100);
    // 只分配不释放
    auto leaks = profiler.DetectLeaks();
    EXPECT_EQ(leaks.size(), 1u);
    EXPECT_EQ(leaks[0], "LeakedTag");
}

TEST(MemoryProfilerTest, 无泄漏时DetectLeaks返回空) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Clean", 100);
    profiler.RecordFree("Clean", 100);
    auto leaks = profiler.DetectLeaks();
    EXPECT_TRUE(leaks.empty());
}

TEST(MemoryProfilerTest, Reset清除所有统计) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Test", 100);
    profiler.Reset();

    auto snap = profiler.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 0u);
    EXPECT_EQ(snap.current_usage, 0u);
    EXPECT_EQ(snap.active_allocations, 0);
}

TEST(MemoryProfilerTest, 导出CSV非空) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("CSV", 256);
    std::string csv = profiler.ExportCSV();
    EXPECT_FALSE(csv.empty());
    EXPECT_NE(csv.find("CSV"), std::string::npos);
}

// ============================================================
// RenderProfiler 测试
// ============================================================

TEST(RenderProfilerTest, 初始状态为零) {
    RenderProfiler profiler;
    auto& frame = profiler.GetCurrentFrameStats();
    EXPECT_EQ(frame.draw_calls, 0);
    EXPECT_EQ(frame.triangle_count, 0);
    EXPECT_EQ(frame.vertex_count, 0);
}

TEST(RenderProfilerTest, 记录绘制调用增加计数) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.RecordDrawCall(200, 100);

    auto& frame = profiler.GetCurrentFrameStats();
    EXPECT_EQ(frame.draw_calls, 2);
    EXPECT_EQ(frame.vertex_count, 300);
    EXPECT_EQ(frame.triangle_count, 150);
}

TEST(RenderProfilerTest, 记录精灵批次统计正确) {
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

TEST(RenderProfilerTest, 记录纹理绑定和着色器切换) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordTextureBind();
    profiler.RecordTextureBind();
    profiler.RecordShaderSwitch();

    auto& frame = profiler.GetCurrentFrameStats();
    EXPECT_EQ(frame.texture_binds, 2);
    EXPECT_EQ(frame.shader_switches, 1);
}

TEST(RenderProfilerTest, 设置纹理内存) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.SetTextureMemory(1024 * 1024);

    EXPECT_EQ(profiler.GetCurrentFrameStats().texture_memory, 1024u * 1024);
}

TEST(RenderProfilerTest, 结束帧累积统计) {
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

TEST(RenderProfilerTest, 多帧Peak统计) {
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

TEST(RenderProfilerTest, 重置清除所有统计) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();
    profiler.Reset();

    EXPECT_EQ(profiler.GetAccumulatedStats().frame_count, 0);
    EXPECT_EQ(profiler.GetCurrentFrameStats().draw_calls, 0);
}

TEST(RenderProfilerTest, 导出CSV非空) {
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

TEST(CPUProfilerTest, ExportChromeTrace格式正确) {
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

TEST(CPUProfilerTest, ExportChromeTrace多采样) {
    CPUProfiler profiler;
    profiler.BeginSample("A");
    profiler.EndSample();
    profiler.BeginSample("B");
    profiler.EndSample();

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_NE(trace.find("\"name\":\"A\""), std::string::npos);
    EXPECT_NE(trace.find("\"name\":\"B\""), std::string::npos);
}

TEST(CPUProfilerTest, ExportChromeTrace空数据返回空数组) {
    CPUProfiler profiler;
    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace, "[\n\n]");
}

TEST(CPUProfilerTest, Reset清除ChromeTrace数据) {
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

TEST(MemoryProfilerTest, ExportChromeTrace格式正确) {
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

TEST(MemoryProfilerTest, ExportChromeTrace记录分配和释放) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("Buf", 100);
    profiler.RecordFree("Buf", 100);

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_NE(trace.find("alloc:Buf"), std::string::npos);
    EXPECT_NE(trace.find("free:Buf"), std::string::npos);
}

TEST(MemoryProfilerTest, ExportChromeTrace空数据返回空数组) {
    MemoryProfiler profiler;
    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace, "[\n\n]");
}

TEST(MemoryProfilerTest, Reset清除ChromeTrace数据) {
    MemoryProfiler profiler;
    profiler.RecordAlloc("X", 100);
    profiler.Reset();
    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace.find("alloc:X"), std::string::npos);
}

// ============================================================
// Chrome Trace 导出测试 — RenderProfiler
// ============================================================

TEST(RenderProfilerTest, ExportChromeTrace格式正确) {
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

TEST(RenderProfilerTest, ExportChromeTrace多帧记录) {
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

TEST(RenderProfilerTest, ExportChromeTrace空数据返回空数组) {
    RenderProfiler profiler;
    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace, "[\n\n]");
}

TEST(RenderProfilerTest, Reset清除ChromeTrace数据) {
    RenderProfiler profiler;
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();
    profiler.Reset();

    std::string trace = profiler.ExportChromeTrace();
    EXPECT_EQ(trace.find("render_stats"), std::string::npos);
}

// ============================================================
// 性能基线测试
// ============================================================

TEST(ProfilerBenchmark, CPU采样开销低于100微秒) {
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

TEST(ProfilerBenchmark, Memory记录开销低于100微秒) {
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

TEST(ProfilerBenchmark, Render记录开销低于100微秒) {
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

TEST(ProfilerBenchmark, ChromeTrace导出万条数据低于100毫秒) {
    CPUProfiler profiler;
    for (int i = 0; i < 10000; ++i) {
        profiler.BeginSample("BenchExport");
        profiler.EndSample();
    }
    auto start = std::chrono::high_resolution_clock::now();
    std::string trace = profiler.ExportChromeTrace();
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    EXPECT_LT(elapsed_ms, 100.0) << "export 10000 samples: " << elapsed_ms << " ms";
    EXPECT_FALSE(trace.empty());
}
