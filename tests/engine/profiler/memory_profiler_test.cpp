/**
 * @file memory_profiler_test.cpp
 * @brief MemoryProfiler 单元测试
 */

#include "catch/catch.hpp"
#include "engine/profiler/memory_profiler.h"

using namespace dse::profiler;

TEST_CASE("MemoryProfiler - basic alloc tracking", "[profiler][memory]") {
    MemoryProfiler profiler;
    
    profiler.RecordAlloc("Textures", 1024);
    profiler.RecordAlloc("Textures", 2048);
    
    auto snap = profiler.GetSnapshot();
    REQUIRE(snap.current_usage == 3072);
    REQUIRE(snap.total_allocated == 3072);
    REQUIRE(snap.active_allocations == 2);
}

TEST_CASE("MemoryProfiler - free reduces usage", "[profiler][memory]") {
    MemoryProfiler profiler;
    
    profiler.RecordAlloc("Meshes", 4096);
    profiler.RecordFree("Meshes", 4096);
    
    auto snap = profiler.GetSnapshot();
    REQUIRE(snap.current_usage == 0);
    REQUIRE(snap.total_allocated == 4096);
    REQUIRE(snap.total_freed == 4096);
}

TEST_CASE("MemoryProfiler - peak tracking", "[profiler][memory]") {
    MemoryProfiler profiler;
    
    profiler.RecordAlloc("Audio", 8192);
    profiler.RecordAlloc("Audio", 4096);
    profiler.RecordFree("Audio", 8192);
    
    auto snap = profiler.GetSnapshot();
    REQUIRE(snap.peak_usage == 12288);  // 8192 + 4096
    REQUIRE(snap.current_usage == 4096);
}

TEST_CASE("MemoryProfiler - category stats", "[profiler][memory]") {
    MemoryProfiler profiler;
    
    profiler.RecordAlloc("Textures", 1024);
    profiler.RecordAlloc("Meshes", 2048);
    profiler.RecordAlloc("Textures", 512);
    
    auto& cats = profiler.GetCategoryStats();
    REQUIRE(cats.at("Textures").alloc_count == 2);
    REQUIRE(cats.at("Textures").current_bytes == 1536);
    REQUIRE(cats.at("Meshes").alloc_count == 1);
}

TEST_CASE("MemoryProfiler - leak detection", "[profiler][memory]") {
    MemoryProfiler profiler;
    
    profiler.RecordAlloc("Leaked", 1024);
    profiler.RecordAlloc("NotLeaked", 2048);
    profiler.RecordFree("NotLeaked", 2048);
    
    auto leaks = profiler.DetectLeaks();
    REQUIRE(leaks.size() == 1);
    REQUIRE(leaks[0] == "Leaked");
}

TEST_CASE("MemoryProfiler - reset clears all", "[profiler][memory]") {
    MemoryProfiler profiler;
    
    profiler.RecordAlloc("Test", 1024);
    profiler.Reset();
    
    auto snap = profiler.GetSnapshot();
    REQUIRE(snap.current_usage == 0);
    REQUIRE(snap.total_allocated == 0);
    REQUIRE(profiler.GetCategoryStats().empty());
}

TEST_CASE("MemoryProfiler - CSV export", "[profiler][memory]") {
    MemoryProfiler profiler;
    
    profiler.RecordAlloc("Sprites", 512);
    
    std::string csv = profiler.ExportCSV();
    REQUIRE(csv.find("Sprites") != std::string::npos);
    REQUIRE(csv.find("Tag,CurrentBytes") != std::string::npos);
}
