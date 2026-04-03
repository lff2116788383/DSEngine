/**
 * @file cpu_profiler_test.cpp
 * @brief CPUProfiler 单元测试
 */

#include "catch/catch.hpp"
#include "engine/profiler/cpu_profiler.h"
#include <thread>
#include <chrono>

using namespace dse::profiler;

TEST_CASE("CPUProfiler - basic sample recording", "[profiler][cpu]") {
    CPUProfiler profiler;
    
    profiler.BeginFrame();
    profiler.BeginSample("TestFunction");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    profiler.EndSample();
    profiler.EndFrame();
    
    auto& stats = profiler.GetStats();
    REQUIRE(stats.count("TestFunction") == 1);
    REQUIRE(stats.at("TestFunction").call_count == 1);
    REQUIRE(stats.at("TestFunction").total_ms >= 5.0);  // at least 5ms
}

TEST_CASE("CPUProfiler - nested samples", "[profiler][cpu]") {
    CPUProfiler profiler;
    
    profiler.BeginFrame();
    profiler.BeginSample("Outer");
    profiler.BeginSample("Inner");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    profiler.EndSample();
    profiler.EndSample();
    profiler.EndFrame();
    
    auto& samples = profiler.GetCurrentFrameSamples();
    REQUIRE(samples.size() == 2);
    // Inner should have depth 1
    REQUIRE(samples[0].depth == 1);
    // Outer should have depth 0
    REQUIRE(samples[1].depth == 0);
}

TEST_CASE("CPUProfiler - frame stats tracking", "[profiler][cpu]") {
    CPUProfiler profiler;
    
    for (int i = 0; i < 5; ++i) {
        profiler.BeginFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        profiler.EndFrame();
    }
    
    auto& frame_stats = profiler.GetFrameStats();
    REQUIRE(frame_stats.frame_count == 5);
    REQUIRE(frame_stats.avg_frame_time_ms > 0.0);
    REQUIRE(frame_stats.fps > 0.0);
}

TEST_CASE("CPUProfiler - multiple calls accumulate", "[profiler][cpu]") {
    CPUProfiler profiler;
    
    for (int i = 0; i < 3; ++i) {
        profiler.BeginFrame();
        profiler.BeginSample("Repeated");
        profiler.EndSample();
        profiler.EndFrame();
    }
    
    REQUIRE(profiler.GetStats().at("Repeated").call_count == 3);
}

TEST_CASE("CPUProfiler - reset clears everything", "[profiler][cpu]") {
    CPUProfiler profiler;
    
    profiler.BeginFrame();
    profiler.BeginSample("Test");
    profiler.EndSample();
    profiler.EndFrame();
    
    profiler.Reset();
    
    REQUIRE(profiler.GetStats().empty());
    REQUIRE(profiler.GetFrameStats().frame_count == 0);
}

TEST_CASE("CPUProfiler - CSV export", "[profiler][cpu]") {
    CPUProfiler profiler;
    
    profiler.BeginFrame();
    profiler.BeginSample("ExportTest");
    profiler.EndSample();
    profiler.EndFrame();
    
    std::string csv = profiler.ExportCSV();
    REQUIRE(csv.find("ExportTest") != std::string::npos);
    REQUIRE(csv.find("Name,TotalMs") != std::string::npos);
}

TEST_CASE("CPUProfiler - JSON export", "[profiler][cpu]") {
    CPUProfiler profiler;
    
    profiler.BeginFrame();
    profiler.BeginSample("JsonTest");
    profiler.EndSample();
    profiler.EndFrame();
    
    std::string json = profiler.ExportJSON();
    REQUIRE(json.find("JsonTest") != std::string::npos);
    REQUIRE(json.find("frame_stats") != std::string::npos);
}

TEST_CASE("CPUProfiler - scoped profile helper", "[profiler][cpu]") {
    CPUProfiler profiler;
    
    profiler.BeginFrame();
    {
        ScopedCPUProfile scope(profiler, "ScopedTest");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    profiler.EndFrame();
    
    REQUIRE(profiler.GetStats().count("ScopedTest") == 1);
}
