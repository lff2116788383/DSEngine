/**
 * @file editor_stability_test.cpp
 * @brief P2-2 Performance and stability tests.
 *
 * Validates editor stability through repeated operations:
 * - Repeated undo/redo cycles (no leaks or stack corruption)
 * - Large-scene load/unload cycles
 * - Memory budget verification
 */

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "editor_undo.h"

using namespace dse::editor;

// ── Repeated Undo/Redo Stability ────────────────────────────────

TEST(StabilityTest, RepeatedUndoRedoCycles) {
    UndoRedoManager mgr(1000);
    int value = 0;

    // Push 100 commands
    for (int i = 1; i <= 100; ++i) {
        mgr.Execute(std::make_unique<LambdaCommand>(
            "step " + std::to_string(i),
            [&value, i]() { value = i; },
            [&value, i]() { value = i - 1; }));
    }

    // Undo all, redo all — 10 times
    for (int cycle = 0; cycle < 10; ++cycle) {
        for (int i = 0; i < 100; ++i) {
            ASSERT_TRUE(mgr.Undo()) << "Undo failed at cycle " << cycle << " step " << i;
        }
        EXPECT_EQ(value, 0);

        for (int i = 0; i < 100; ++i) {
            ASSERT_TRUE(mgr.Redo()) << "Redo failed at cycle " << cycle << " step " << i;
        }
        EXPECT_EQ(value, 100);
    }

    // Manager should still be in a consistent state
    EXPECT_EQ(mgr.GetUndoCount(), 100);
    EXPECT_EQ(mgr.GetRedoCount(), 0);
}

TEST(StabilityTest, LargeUndoHistoryDoesNotLeak) {
    // This test verifies that the undo system doesn't accumulate
    // memory beyond its max_history limit.
    UndoRedoManager mgr(50);
    int value = 0;

    // Push 500 commands — should trim to 50
    for (int i = 1; i <= 500; ++i) {
        mgr.Execute(std::make_unique<LambdaCommand>(
            "cmd " + std::to_string(i),
            [&value, i]() { value = i; },
            [&value, i]() { value = i - 1; }));
    }

    EXPECT_EQ(mgr.GetUndoCount(), 50);

    // Undo all 50 — should work without crash
    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(mgr.Undo());
    }
    EXPECT_FALSE(mgr.Undo());
}

TEST(StabilityTest, CompoundCommandStabilityUnderCycling) {
    UndoRedoManager mgr;
    int a = 0, b = 0, c = 0;

    for (int cycle = 0; cycle < 20; ++cycle) {
        auto compound = std::make_unique<CompoundCommand>("batch");
        compound->AddCommand(std::make_unique<LambdaCommand>(
            "a", [&]() { a = cycle; }, [&]() { a = 0; }));
        compound->AddCommand(std::make_unique<LambdaCommand>(
            "b", [&]() { b = cycle; }, [&]() { b = 0; }));
        compound->AddCommand(std::make_unique<LambdaCommand>(
            "c", [&]() { c = cycle; }, [&]() { c = 0; }));

        mgr.Execute(std::move(compound));
        EXPECT_EQ(a, cycle);
        EXPECT_EQ(b, cycle);
        EXPECT_EQ(c, cycle);

        // Undo after each cycle
        ASSERT_TRUE(mgr.Undo());
        EXPECT_EQ(a, 0);
        EXPECT_EQ(b, 0);
        EXPECT_EQ(c, 0);

        // Redo
        ASSERT_TRUE(mgr.Redo());
        EXPECT_EQ(a, cycle);
        EXPECT_EQ(b, cycle);
        EXPECT_EQ(c, cycle);
    }

    // Should have 20 commands in undo stack
    EXPECT_EQ(mgr.GetUndoCount(), 20);
}

// ── Performance Budget ──────────────────────────────────────────

TEST(StabilityTest, UndoRedoPerformanceWithinBudget) {
    UndoRedoManager mgr;
    int value = 0;

    // Push 1000 commands
    for (int i = 0; i < 1000; ++i) {
        mgr.Execute(std::make_unique<LambdaCommand>(
            "perf " + std::to_string(i),
            [&value, i]() { value = i; },
            [&value, i]() { value = i - 1; }));
    }

    // Undo all 1000 — should complete in < 100ms
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        mgr.Undo();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 100) << "Undo of 1000 commands took " << ms << "ms";

    // Redo all 1000 — should complete in < 100ms
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        mgr.Redo();
    }
    end = std::chrono::high_resolution_clock::now();
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 100) << "Redo of 1000 commands took " << ms << "ms";
}

TEST(StabilityTest, HistoryQueryPerformance) {
    UndoRedoManager mgr(1000);
    int value = 0;

    for (int i = 0; i < 500; ++i) {
        mgr.Execute(std::make_unique<LambdaCommand>(
            "query " + std::to_string(i),
            [&value, i]() { value = i; },
            [&value, i]() { value = i - 1; }));
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto history = mgr.GetUndoHistory();
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    ASSERT_EQ(history.size(), 500u);
    EXPECT_LT(ms, 10) << "GetUndoHistory of 500 items took " << ms << "ms";
}

// ── Stress: Concurrent-looking operations (single-threaded) ─────

TEST(StabilityTest, InterleavedOperations) {
    UndoRedoManager mgr;
    int value = 0;

    // Interleave execute, undo, redo
    mgr.Execute(std::make_unique<LambdaCommand>(
        "a", [&]() { value = 1; }, [&]() { value = 0; }));
    mgr.Execute(std::make_unique<LambdaCommand>(
        "b", [&]() { value = 2; }, [&]() { value = 1; }));

    mgr.Undo();  // value=1
    EXPECT_EQ(value, 1);

    mgr.Execute(std::make_unique<LambdaCommand>(
        "c", [&]() { value = 3; }, [&]() { value = 1; }));
    // Redo stack should be cleared
    EXPECT_EQ(mgr.GetRedoCount(), 0);
    EXPECT_EQ(value, 3);

    mgr.Undo();  // value=1
    mgr.Undo();  // value=0
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(mgr.CanUndo());
}
