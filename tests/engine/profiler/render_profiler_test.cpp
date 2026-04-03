/**
 * @file render_profiler_test.cpp
 * @brief RenderProfiler 单元测试
 */

#include "catch/catch.hpp"
#include "engine/profiler/render_profiler.h"

using namespace dse::profiler;

TEST_CASE("RenderProfiler - draw call counting", "[profiler][render]") {
    RenderProfiler profiler;
    
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.RecordDrawCall(200, 100);
    profiler.EndFrame();
    
    auto& stats = profiler.GetCurrentFrameStats();
    REQUIRE(stats.draw_calls == 2);
    REQUIRE(stats.vertex_count == 300);
    REQUIRE(stats.triangle_count == 150);
}

TEST_CASE("RenderProfiler - sprite batch recording", "[profiler][render]") {
    RenderProfiler profiler;
    
    profiler.BeginFrame();
    profiler.RecordSpriteBatch(50);
    profiler.EndFrame();
    
    auto& stats = profiler.GetCurrentFrameStats();
    REQUIRE(stats.sprite_count == 50);
    REQUIRE(stats.batch_count == 1);
    REQUIRE(stats.draw_calls == 1);
    REQUIRE(stats.vertex_count == 200);   // 50 * 4
    REQUIRE(stats.triangle_count == 100); // 50 * 2
}

TEST_CASE("RenderProfiler - frame reset", "[profiler][render]") {
    RenderProfiler profiler;
    
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();
    
    profiler.BeginFrame();
    // New frame should start at zero
    auto& stats = profiler.GetCurrentFrameStats();
    REQUIRE(stats.draw_calls == 0);
}

TEST_CASE("RenderProfiler - accumulated stats", "[profiler][render]") {
    RenderProfiler profiler;
    
    for (int i = 0; i < 3; ++i) {
        profiler.BeginFrame();
        profiler.RecordDrawCall(100 * (i + 1), 50 * (i + 1));
        profiler.EndFrame();
    }
    
    auto& acc = profiler.GetAccumulatedStats();
    REQUIRE(acc.frame_count == 3);
    REQUIRE(acc.peak_draw_calls == 1);
    REQUIRE(acc.total_draw_calls == 3);
    REQUIRE(acc.peak_vertices == 300);
}

TEST_CASE("RenderProfiler - texture and shader tracking", "[profiler][render]") {
    RenderProfiler profiler;
    
    profiler.BeginFrame();
    profiler.RecordTextureBind();
    profiler.RecordTextureBind();
    profiler.RecordShaderSwitch();
    profiler.SetTextureMemory(1024 * 1024);
    profiler.EndFrame();
    
    auto& stats = profiler.GetCurrentFrameStats();
    REQUIRE(stats.texture_binds == 2);
    REQUIRE(stats.shader_switches == 1);
    REQUIRE(stats.texture_memory == 1024 * 1024);
}

TEST_CASE("RenderProfiler - reset clears all", "[profiler][render]") {
    RenderProfiler profiler;
    
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();
    
    profiler.Reset();
    
    auto& acc = profiler.GetAccumulatedStats();
    REQUIRE(acc.frame_count == 0);
    REQUIRE(acc.total_draw_calls == 0);
}

TEST_CASE("RenderProfiler - CSV export", "[profiler][render]") {
    RenderProfiler profiler;
    
    profiler.BeginFrame();
    profiler.RecordDrawCall(100, 50);
    profiler.EndFrame();
    
    std::string csv = profiler.ExportCSV();
    REQUIRE(csv.find("DrawCalls") != std::string::npos);
    REQUIRE(csv.find("Metric,Current") != std::string::npos);
}
