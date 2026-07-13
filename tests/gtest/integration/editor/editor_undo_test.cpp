/**
 * @file editor_undo_test.cpp
 * @brief P2-1 integration tests for the unified Undo/Redo system.
 *
 * Validates ICommand, PropertyChangeCommand, LambdaCommand, CompoundCommand,
 * and UndoRedoManager including merge, trim, clear, and history queries.
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "editor_undo.h"

using namespace dse::editor;

// ── ICommand / PropertyChangeCommand ──────────────────────────────

TEST(UndoSystemTest, PropertyChangeExecuteAndUndo) {
    int value = 10;
    PropertyChangeCommand<int> cmd("set value", 10, 20,
        [&value](const int& v) { value = v; });

    cmd.Execute();
    EXPECT_EQ(value, 20);

    cmd.Undo();
    EXPECT_EQ(value, 10);
}

TEST(UndoSystemTest, PropertyChangeMergeSameDescription) {
    int value = 0;
    auto cmd1 = std::make_unique<PropertyChangeCommand<int>>(
        "drag", 0, 5, [&value](const int& v) { value = v; });
    auto cmd2 = std::make_unique<PropertyChangeCommand<int>>(
        "drag", 5, 10, [&value](const int& v) { value = v; });

    EXPECT_TRUE(cmd1->MergeWith(cmd2.get()));
    // After merge, cmd1 should have cmd2's new_value
    cmd1->Execute();
    EXPECT_EQ(value, 10);
    cmd1->Undo();
    EXPECT_EQ(value, 0);  // original old_value preserved
}

TEST(UndoSystemTest, PropertyChangeNoMergeDifferentDescription) {
    auto cmd1 = std::make_unique<PropertyChangeCommand<int>>(
        "set A", 0, 5, [](const int&) {});
    auto cmd2 = std::make_unique<PropertyChangeCommand<int>>(
        "set B", 5, 10, [](const int&) {});

    EXPECT_FALSE(cmd1->MergeWith(cmd2.get()));
}

// ── LambdaCommand ────────────────────────────────────────────────

TEST(UndoSystemTest, LambdaCommandExecuteUndo) {
    int counter = 0;
    LambdaCommand cmd("increment",
        [&counter]() { counter++; },
        [&counter]() { counter--; });

    cmd.Execute();
    EXPECT_EQ(counter, 1);

    cmd.Undo();
    EXPECT_EQ(counter, 0);
}

TEST(UndoSystemTest, LambdaCommandMergeWithSameId) {
    int value = 0;
    auto cmd1 = std::make_unique<LambdaCommand>(
        "drag", [&]() { value = 5; }, [&]() { value = 0; }, "drag_x");
    auto cmd2 = std::make_unique<LambdaCommand>(
        "drag", [&]() { value = 10; }, [&]() { value = 0; }, "drag_x");

    EXPECT_TRUE(cmd1->MergeWith(cmd2.get()));
    cmd1->Execute();
    EXPECT_EQ(value, 10);
}

TEST(UndoSystemTest, LambdaCommandNoMergeWithoutId) {
    auto cmd1 = std::make_unique<LambdaCommand>(
        "action1", []() {}, []() {});
    auto cmd2 = std::make_unique<LambdaCommand>(
        "action2", []() {}, []() {});

    EXPECT_FALSE(cmd1->MergeWith(cmd2.get()));
}

// ── CompoundCommand ─────────────────────────────────────────────

TEST(UndoSystemTest, CompoundCommandAtomicExecuteUndo) {
    int a = 0, b = 0, c = 0;

    CompoundCommand compound("multi-edit");
    compound.AddCommand(std::make_unique<LambdaCommand>(
        "set a", [&]() { a = 1; }, [&]() { a = 0; }));
    compound.AddCommand(std::make_unique<LambdaCommand>(
        "set b", [&]() { b = 2; }, [&]() { b = 0; }));
    compound.AddCommand(std::make_unique<LambdaCommand>(
        "set c", [&]() { c = 3; }, [&]() { c = 0; }));

    compound.Execute();
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);

    compound.Undo();
    // Undo in reverse order
    EXPECT_EQ(c, 0);
    EXPECT_EQ(b, 0);
    EXPECT_EQ(a, 0);
}

TEST(UndoSystemTest, CompoundCommandIsEmpty) {
    CompoundCommand compound("empty");
    EXPECT_TRUE(compound.IsEmpty());

    compound.AddCommand(std::make_unique<LambdaCommand>(
        "action", []() {}, []() {}));
    EXPECT_FALSE(compound.IsEmpty());
}

// ── UndoRedoManager ─────────────────────────────────────────────

TEST(UndoRedoManagerTest, ExecuteAndUndo) {
    UndoRedoManager mgr;
    int value = 0;

    mgr.Execute(std::make_unique<LambdaCommand>(
        "set 5", [&]() { value = 5; }, [&]() { value = 0; }));

    EXPECT_EQ(value, 5);
    EXPECT_TRUE(mgr.CanUndo());
    EXPECT_FALSE(mgr.CanRedo());

    EXPECT_TRUE(mgr.Undo());
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(mgr.CanUndo());
    EXPECT_TRUE(mgr.CanRedo());
}

TEST(UndoRedoManagerTest, UndoAndRedo) {
    UndoRedoManager mgr;
    int value = 0;

    mgr.Execute(std::make_unique<LambdaCommand>(
        "set 10", [&]() { value = 10; }, [&]() { value = 0; }));

    mgr.Undo();
    EXPECT_EQ(value, 0);

    EXPECT_TRUE(mgr.Redo());
    EXPECT_EQ(value, 10);
    EXPECT_TRUE(mgr.CanUndo());
    EXPECT_FALSE(mgr.CanRedo());
}

TEST(UndoRedoManagerTest, MultipleCommandsUndoRedoChain) {
    UndoRedoManager mgr;
    int value = 0;

    for (int i = 1; i <= 5; ++i) {
        mgr.Execute(std::make_unique<LambdaCommand>(
            "step " + std::to_string(i),
            [&value, i]() { value = i; },
            [&value, i]() { value = i - 1; }));
    }

    EXPECT_EQ(value, 5);
    EXPECT_EQ(mgr.GetUndoCount(), 5);

    // Undo all
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(mgr.Undo());
    }
    EXPECT_EQ(value, 0);
    EXPECT_EQ(mgr.GetRedoCount(), 5);

    // Redo all
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(mgr.Redo());
    }
    EXPECT_EQ(value, 5);
}

TEST(UndoRedoManagerTest, NewCommandClearsRedoStack) {
    UndoRedoManager mgr;
    int value = 0;

    mgr.Execute(std::make_unique<LambdaCommand>(
        "cmd1", [&]() { value = 1; }, [&]() { value = 0; }));
    mgr.Execute(std::make_unique<LambdaCommand>(
        "cmd2", [&]() { value = 2; }, [&]() { value = 1; }));

    EXPECT_TRUE(mgr.Undo());  // value=1, redo has cmd2
    EXPECT_EQ(mgr.GetRedoCount(), 1);

    // New command should clear redo stack
    mgr.Execute(std::make_unique<LambdaCommand>(
        "cmd3", [&]() { value = 3; }, [&]() { value = 1; }));

    EXPECT_EQ(mgr.GetRedoCount(), 0);
    EXPECT_EQ(mgr.GetUndoCount(), 2);
}

TEST(UndoRedoManagerTest, MergeConsecutiveCommands) {
    UndoRedoManager mgr;
    int value = 0;

    // First command
    mgr.Execute(std::make_unique<LambdaCommand>(
        "drag", [&]() { value = 5; }, [&]() { value = 0; }, "drag_x"));

    // Second command with same merge_id — should merge
    mgr.Execute(std::make_unique<LambdaCommand>(
        "drag", [&]() { value = 10; }, [&]() { value = 0; }, "drag_x"),
        true);

    EXPECT_EQ(mgr.GetUndoCount(), 1);  // merged into one
    EXPECT_EQ(value, 10);

    // Undo should restore to original value
    mgr.Undo();
    EXPECT_EQ(value, 0);
}

TEST(UndoRedoManagerTest, HistoryTrim) {
    UndoRedoManager mgr(3);  // max 3 items
    int value = 0;

    for (int i = 1; i <= 5; ++i) {
        mgr.Execute(std::make_unique<LambdaCommand>(
            "cmd" + std::to_string(i),
            [&value, i]() { value = i; },
            [&value, i]() { value = i - 1; }));
    }

    EXPECT_EQ(mgr.GetUndoCount(), 3);  // trimmed to max
}

TEST(UndoRedoManagerTest, ClearHistory) {
    UndoRedoManager mgr;
    mgr.Execute(std::make_unique<LambdaCommand>(
        "cmd", []() {}, []() {}));

    EXPECT_EQ(mgr.GetUndoCount(), 1);
    mgr.Clear();
    EXPECT_EQ(mgr.GetUndoCount(), 0);
    EXPECT_EQ(mgr.GetRedoCount(), 0);
}

TEST(UndoRedoManagerTest, GetUndoHistory) {
    UndoRedoManager mgr;
    mgr.Execute(std::make_unique<LambdaCommand>(
        "first", []() {}, []() {}));
    mgr.Execute(std::make_unique<LambdaCommand>(
        "second", []() {}, []() {}));
    mgr.Execute(std::make_unique<LambdaCommand>(
        "third", []() {}, []() {}));

    auto history = mgr.GetUndoHistory();
    ASSERT_EQ(history.size(), 3u);
    // Most recent first
    EXPECT_EQ(history[0], "third");
    EXPECT_EQ(history[1], "second");
    EXPECT_EQ(history[2], "first");
}

TEST(UndoRedoManagerTest, GetRedoHistory) {
    UndoRedoManager mgr;
    mgr.Execute(std::make_unique<LambdaCommand>(
        "first", []() {}, []() {}));
    mgr.Execute(std::make_unique<LambdaCommand>(
        "second", []() {}, []() {}));

    mgr.Undo();
    mgr.Undo();

    auto history = mgr.GetRedoHistory();
    ASSERT_EQ(history.size(), 2u);
    // GetRedoHistory iterates rbegin->rend: last-undone (first) appears first
    EXPECT_EQ(history[0], "first");
    EXPECT_EQ(history[1], "second");
}

TEST(UndoRedoManagerTest, UndoOnEmptyStackReturnsFalse) {
    UndoRedoManager mgr;
    EXPECT_FALSE(mgr.Undo());
}

TEST(UndoRedoManagerTest, RedoOnEmptyStackReturnsFalse) {
    UndoRedoManager mgr;
    EXPECT_FALSE(mgr.Redo());
}

TEST(UndoRedoManagerTest, CompoundCommandInManager) {
    UndoRedoManager mgr;
    int a = 0, b = 0;

    auto compound = std::make_unique<CompoundCommand>("multi");
    compound->AddCommand(std::make_unique<LambdaCommand>(
        "set a", [&]() { a = 1; }, [&]() { a = 0; }));
    compound->AddCommand(std::make_unique<LambdaCommand>(
        "set b", [&]() { b = 2; }, [&]() { b = 0; }));

    mgr.Execute(std::move(compound));
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(mgr.GetUndoCount(), 1);

    mgr.Undo();
    EXPECT_EQ(a, 0);
    EXPECT_EQ(b, 0);

    mgr.Redo();
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
}

TEST(UndoRedoManagerTest, GetDescriptions) {
    UndoRedoManager mgr;
    mgr.Execute(std::make_unique<LambdaCommand>(
        "my command", []() {}, []() {}));

    EXPECT_EQ(mgr.GetUndoDescription(), "my command");
    EXPECT_TRUE(mgr.GetRedoDescription().empty());

    mgr.Undo();
    EXPECT_TRUE(mgr.GetUndoDescription().empty());
    EXPECT_EQ(mgr.GetRedoDescription(), "my command");
}
