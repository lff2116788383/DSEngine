/**
 * @file memory_profiler_test.cpp
 * @brief MemoryProfiler 分配追踪/泄漏检测/CSV 导出测试
 */

#include <gtest/gtest.h>
#include "engine/profiler/memory_profiler.h"
#include <algorithm>

using namespace dse::profiler;

class MemoryProfilerExtTest : public ::testing::Test {
protected:
    void SetUp() override { prof_.Reset(); }
    MemoryProfiler prof_;
};

// 测试 内存性能分析器扩展：初始快照
TEST_F(MemoryProfilerExtTest, InitialSnapshot) {
    auto snap = prof_.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 0u);
    EXPECT_EQ(snap.total_freed, 0u);
    EXPECT_EQ(snap.current_usage, 0u);
    EXPECT_EQ(snap.peak_usage, 0u);
    EXPECT_EQ(snap.active_allocations, 0);
}

// 测试 内存性能分析器扩展：单一分配
TEST_F(MemoryProfilerExtTest, SingleAlloc) {
    prof_.RecordAlloc("gpu", 1024);
    auto snap = prof_.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 1024u);
    EXPECT_EQ(snap.current_usage, 1024u);
    EXPECT_EQ(snap.peak_usage, 1024u);
    EXPECT_EQ(snap.active_allocations, 1);
}

// 测试 内存性能分析器扩展：分配且释放
TEST_F(MemoryProfilerExtTest, AllocAndFree) {
    prof_.RecordAlloc("gpu", 1024);
    prof_.RecordFree("gpu", 1024);
    auto snap = prof_.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 1024u);
    EXPECT_EQ(snap.total_freed, 1024u);
    EXPECT_EQ(snap.current_usage, 0u);
    EXPECT_EQ(snap.peak_usage, 1024u);
    EXPECT_EQ(snap.active_allocations, 0);
}

// 测试 内存性能分析器扩展：峰值追踪
TEST_F(MemoryProfilerExtTest, PeakTracking) {
    prof_.RecordAlloc("a", 500);
    prof_.RecordAlloc("b", 800);
    prof_.RecordFree("a", 500);
    auto snap = prof_.GetSnapshot();
    EXPECT_EQ(snap.peak_usage, 1300u);
    EXPECT_EQ(snap.current_usage, 800u);
}

// 测试 内存性能分析器扩展：Category统计
TEST_F(MemoryProfilerExtTest, CategoryStats) {
    prof_.RecordAlloc("texture", 2048);
    prof_.RecordAlloc("texture", 1024);
    prof_.RecordFree("texture", 2048);

    const auto& cats = prof_.GetCategoryStats();
    auto it = cats.find("texture");
    ASSERT_NE(it, cats.end());
    EXPECT_EQ(it->second.alloc_count, 2);
    EXPECT_EQ(it->second.free_count, 1);
    EXPECT_EQ(it->second.total_allocated, 3072u);
    EXPECT_EQ(it->second.total_freed, 2048u);
    EXPECT_EQ(it->second.current_bytes, 1024u);
    EXPECT_EQ(it->second.peak_bytes, 3072u);
}

// 测试 内存性能分析器扩展：检测泄漏
TEST_F(MemoryProfilerExtTest, DetectLeaks) {
    prof_.RecordAlloc("leaky", 512);
    prof_.RecordAlloc("clean", 256);
    prof_.RecordFree("clean", 256);

    auto leaks = prof_.DetectLeaks();
    EXPECT_EQ(leaks.size(), 1u);
    EXPECT_EQ(leaks[0], "leaky");
}

// 测试 内存性能分析器扩展：检测无泄漏
TEST_F(MemoryProfilerExtTest, DetectNoLeaks) {
    prof_.RecordAlloc("a", 100);
    prof_.RecordFree("a", 100);
    auto leaks = prof_.DetectLeaks();
    EXPECT_TRUE(leaks.empty());
}

// 测试 内存性能分析器扩展：重置
TEST_F(MemoryProfilerExtTest, Reset) {
    prof_.RecordAlloc("x", 999);
    prof_.Reset();
    auto snap = prof_.GetSnapshot();
    EXPECT_EQ(snap.total_allocated, 0u);
    EXPECT_EQ(snap.current_usage, 0u);
    EXPECT_TRUE(prof_.GetCategoryStats().empty());
}

// 测试 内存性能分析器扩展：导出CSV
TEST_F(MemoryProfilerExtTest, ExportCSV) {
    prof_.RecordAlloc("gpu", 4096);
    std::string csv = prof_.ExportCSV();
    EXPECT_NE(csv.find("Tag,CurrentBytes,PeakBytes"), std::string::npos);
    EXPECT_NE(csv.find("gpu"), std::string::npos);
    EXPECT_NE(csv.find("4096"), std::string::npos);
}

// 测试 内存性能分析器扩展：导出Chrome追踪
TEST_F(MemoryProfilerExtTest, ExportChromeTrace) {
    prof_.RecordAlloc("mesh", 2048);
    prof_.RecordFree("mesh", 2048);
    std::string trace = prof_.ExportChromeTrace();
    EXPECT_NE(trace.find("alloc:mesh"), std::string::npos);
    EXPECT_NE(trace.find("free:mesh"), std::string::npos);
    EXPECT_NE(trace.find("memory_usage"), std::string::npos);
}

// 测试 内存性能分析器扩展：释放More比Allocated
TEST_F(MemoryProfilerExtTest, FreeMoreThanAllocated) {
    prof_.RecordAlloc("buf", 100);
    prof_.RecordFree("buf", 200);
    auto snap = prof_.GetSnapshot();
    EXPECT_EQ(snap.current_usage, 0u);
}
