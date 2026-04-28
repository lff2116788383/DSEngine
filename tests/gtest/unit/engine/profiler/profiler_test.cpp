/**
 * @file profiler_test.cpp
 * @brief CPUProfiler / MemoryProfiler / RenderProfiler 性能分析器单元测试
 *
 * 覆盖场景：
 * - CPUProfiler: BeginSample/EndSample, BeginFrame/EndFrame, Stats, Export
 * - MemoryProfiler: RecordAlloc/RecordFree, Snapshot, DetectLeaks, Reset
 * - RenderProfiler: RecordDrawCall/SpriteBatch/TextureBind/ShaderSwitch, Accumulated, Reset
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
