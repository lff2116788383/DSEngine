/**
 * @file render_profiler_test.cpp
 * @brief RenderProfiler 帧统计/峰值/ChromeTrace 导出测试
 */

#include <gtest/gtest.h>
#include "engine/profiler/render_profiler.h"

using namespace dse::profiler;

class RenderProfilerExtTest : public ::testing::Test {
protected:
    void SetUp() override { prof_.Reset(); }
    RenderProfiler prof_;
};

// 测试 渲染性能分析器扩展：初始状态
TEST_F(RenderProfilerExtTest, InitialState) {
    auto stats = prof_.GetCurrentFrameStats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.triangle_count, 0);
    EXPECT_EQ(stats.vertex_count, 0);

    auto acc = prof_.GetAccumulatedStats();
    EXPECT_EQ(acc.frame_count, 0);
}

// 测试 渲染性能分析器扩展：记录绘制调用
TEST_F(RenderProfilerExtTest, RecordDrawCall) {
    prof_.BeginFrame();
    prof_.RecordDrawCall(300, 100);
    prof_.RecordDrawCall(600, 200);
    auto stats = prof_.GetCurrentFrameStats();
    EXPECT_EQ(stats.draw_calls, 2);
    EXPECT_EQ(stats.vertex_count, 900);
    EXPECT_EQ(stats.triangle_count, 300);
}

// 测试 渲染性能分析器扩展：记录精灵批次
TEST_F(RenderProfilerExtTest, RecordSpriteBatch) {
    prof_.BeginFrame();
    prof_.RecordSpriteBatch(50);
    auto stats = prof_.GetCurrentFrameStats();
    EXPECT_EQ(stats.batch_count, 1);
    EXPECT_EQ(stats.sprite_count, 50);
    EXPECT_EQ(stats.draw_calls, 1);
    EXPECT_EQ(stats.vertex_count, 200);
    EXPECT_EQ(stats.triangle_count, 100);
}

// 测试 渲染性能分析器扩展：纹理绑定且着色器切换
TEST_F(RenderProfilerExtTest, TextureBindAndShaderSwitch) {
    prof_.BeginFrame();
    prof_.RecordTextureBind();
    prof_.RecordTextureBind();
    prof_.RecordShaderSwitch();
    auto stats = prof_.GetCurrentFrameStats();
    EXPECT_EQ(stats.texture_binds, 2);
    EXPECT_EQ(stats.shader_switches, 1);
}

// 测试 渲染性能分析器扩展：设置纹理内存
TEST_F(RenderProfilerExtTest, SetTextureMemory) {
    prof_.BeginFrame();
    prof_.SetTextureMemory(1024 * 1024);
    auto stats = prof_.GetCurrentFrameStats();
    EXPECT_EQ(stats.texture_memory, 1024u * 1024u);
}

// 测试 渲染性能分析器扩展：结束帧Accumulates
TEST_F(RenderProfilerExtTest, EndFrameAccumulates) {
    prof_.BeginFrame();
    prof_.RecordDrawCall(100, 50);
    prof_.EndFrame();

    auto acc = prof_.GetAccumulatedStats();
    EXPECT_EQ(acc.frame_count, 1);
    EXPECT_EQ(acc.total_draw_calls, 1);
    EXPECT_EQ(acc.total_triangles, 50);
    EXPECT_EQ(acc.total_vertices, 100);
    EXPECT_EQ(acc.peak_draw_calls, 1);
}

// 测试 渲染性能分析器扩展：峰值追踪
TEST_F(RenderProfilerExtTest, PeakTracking) {
    prof_.BeginFrame();
    prof_.RecordDrawCall(100, 50);
    prof_.EndFrame();

    prof_.BeginFrame();
    prof_.RecordDrawCall(200, 100);
    prof_.RecordDrawCall(200, 100);
    prof_.EndFrame();

    auto acc = prof_.GetAccumulatedStats();
    EXPECT_EQ(acc.frame_count, 2);
    EXPECT_EQ(acc.peak_draw_calls, 2);
    EXPECT_EQ(acc.peak_triangles, 200);
    EXPECT_EQ(acc.peak_vertices, 400);
}

// 测试 渲染性能分析器扩展：Average Computation
TEST_F(RenderProfilerExtTest, AverageComputation) {
    for (int i = 0; i < 10; ++i) {
        prof_.BeginFrame();
        prof_.RecordDrawCall(100, 50);
        prof_.EndFrame();
    }
    auto acc = prof_.GetAccumulatedStats();
    EXPECT_EQ(acc.frame_count, 10);
    EXPECT_NEAR(acc.avg_draw_calls, 1.0, 0.01);
    EXPECT_NEAR(acc.avg_triangles, 50.0, 0.01);
}

// 测试 渲染性能分析器扩展：更新从RHI
TEST_F(RenderProfilerExtTest, UpdateFromRhi) {
    prof_.BeginFrame();
    prof_.UpdateFromRhi(5, 1000, 500, 10, 3, 2);
    auto stats = prof_.GetCurrentFrameStats();
    EXPECT_EQ(stats.draw_calls, 5);
    EXPECT_EQ(stats.vertex_count, 1000);
    EXPECT_EQ(stats.triangle_count, 500);
    EXPECT_EQ(stats.sprite_count, 10);
    EXPECT_EQ(stats.texture_binds, 3);
    EXPECT_EQ(stats.shader_switches, 2);
}

// 测试 渲染性能分析器扩展：重置
TEST_F(RenderProfilerExtTest, Reset) {
    prof_.BeginFrame();
    prof_.RecordDrawCall(100, 50);
    prof_.EndFrame();
    prof_.Reset();

    auto acc = prof_.GetAccumulatedStats();
    EXPECT_EQ(acc.frame_count, 0);
    EXPECT_EQ(acc.total_draw_calls, 0);
}

// 测试 渲染性能分析器扩展：导出CSV
TEST_F(RenderProfilerExtTest, ExportCSV) {
    prof_.BeginFrame();
    prof_.RecordDrawCall(100, 50);
    prof_.EndFrame();
    std::string csv = prof_.ExportCSV();
    EXPECT_NE(csv.find("Metric,Current,Peak,Average"), std::string::npos);
    EXPECT_NE(csv.find("DrawCalls"), std::string::npos);
}

// 测试 渲染性能分析器扩展：导出Chrome追踪
TEST_F(RenderProfilerExtTest, ExportChromeTrace) {
    prof_.BeginFrame();
    prof_.RecordDrawCall(100, 50);
    prof_.EndFrame();
    std::string trace = prof_.ExportChromeTrace();
    EXPECT_NE(trace.find("render_stats"), std::string::npos);
    EXPECT_NE(trace.find("draw_calls"), std::string::npos);
}

// 测试 渲染性能分析器扩展：开始帧Resets当前统计
TEST_F(RenderProfilerExtTest, BeginFrameResetsCurrentStats) {
    prof_.BeginFrame();
    prof_.RecordDrawCall(500, 200);
    prof_.BeginFrame();
    auto stats = prof_.GetCurrentFrameStats();
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.vertex_count, 0);
}
