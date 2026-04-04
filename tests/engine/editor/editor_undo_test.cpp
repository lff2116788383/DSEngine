/**
 * @file editor_undo_test.cpp
 * @brief Undo/Redo 系统单元测试
 */

#include "catch/catch.hpp"
#include "apps/editor_cpp/src/editor_undo.h"
#include <glm/glm.hpp>

using namespace dse::editor;

// ─── Basic Undo/Redo ────────────────────────────────────────────────────
TEST_CASE("UndoRedo - basic property change undo", "[editor][undo]") {
    UndoRedoManager mgr;
    int value = 10;

    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "Change value", 10, 20,
        [&](const int& v) { value = v; }
    ));

    REQUIRE(value == 20);
    REQUIRE(mgr.CanUndo());

    mgr.Undo();
    REQUIRE(value == 10);
    REQUIRE(mgr.CanRedo());
}

TEST_CASE("UndoRedo - redo restores value", "[editor][undo]") {
    UndoRedoManager mgr;
    float value = 1.0f;

    mgr.Execute(std::make_unique<PropertyChangeCommand<float>>(
        "Change float", 1.0f, 5.0f,
        [&](const float& v) { value = v; }
    ));

    mgr.Undo();
    REQUIRE(value == Approx(1.0f));

    mgr.Redo();
    REQUIRE(value == Approx(5.0f));
}

// ─── Multiple operations ────────────────────────────────────────────────
TEST_CASE("UndoRedo - multiple undo steps", "[editor][undo]") {
    UndoRedoManager mgr;
    int value = 0;

    for (int i = 1; i <= 5; ++i) {
        int old_val = value;
        mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
            "Step " + std::to_string(i), old_val, i * 10,
            [&](const int& v) { value = v; }
        ));
    }

    REQUIRE(value == 50);
    REQUIRE(mgr.GetUndoCount() == 5);

    mgr.Undo();
    REQUIRE(value == 40);
    mgr.Undo();
    REQUIRE(value == 30);
    mgr.Undo();
    REQUIRE(value == 20);
}

// ─── New action clears redo ─────────────────────────────────────────────
TEST_CASE("UndoRedo - new action clears redo stack", "[editor][undo]") {
    UndoRedoManager mgr;
    int value = 0;

    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "Set 10", 0, 10, [&](const int& v) { value = v; }
    ));
    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "Set 20", 10, 20, [&](const int& v) { value = v; }
    ));

    mgr.Undo();  // back to 10
    REQUIRE(mgr.CanRedo());

    // New action should clear redo
    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "Set 30", 10, 30, [&](const int& v) { value = v; }
    ));
    REQUIRE_FALSE(mgr.CanRedo());
    REQUIRE(value == 30);
}

// ─── Merge ──────────────────────────────────────────────────────────────
TEST_CASE("UndoRedo - merge consecutive same-description commands", "[editor][undo]") {
    UndoRedoManager mgr;
    float value = 0.0f;

    mgr.Execute(std::make_unique<PropertyChangeCommand<float>>(
        "Drag position.x", 0.0f, 1.0f,
        [&](const float& v) { value = v; }
    ));

    // Simulate continuous drag
    mgr.Execute(std::make_unique<PropertyChangeCommand<float>>(
        "Drag position.x", 1.0f, 2.0f,
        [&](const float& v) { value = v; }
    ), true);

    mgr.Execute(std::make_unique<PropertyChangeCommand<float>>(
        "Drag position.x", 2.0f, 3.0f,
        [&](const float& v) { value = v; }
    ), true);

    REQUIRE(value == Approx(3.0f));
    REQUIRE(mgr.GetUndoCount() == 1);  // All merged into one

    mgr.Undo();
    REQUIRE(value == Approx(0.0f));  // Back to original
}

// ─── Lambda command ─────────────────────────────────────────────────────
TEST_CASE("UndoRedo - lambda command", "[editor][undo]") {
    UndoRedoManager mgr;
    std::vector<int> items;

    mgr.Execute(std::make_unique<LambdaCommand>(
        "Add item",
        [&]() { items.push_back(42); },
        [&]() { items.pop_back(); }
    ));

    REQUIRE(items.size() == 1);
    REQUIRE(items[0] == 42);

    mgr.Undo();
    REQUIRE(items.empty());

    mgr.Redo();
    REQUIRE(items.size() == 1);
}

// ─── Compound command ───────────────────────────────────────────────────
TEST_CASE("UndoRedo - compound command undoes all sub-commands", "[editor][undo]") {
    UndoRedoManager mgr;
    int a = 0, b = 0;

    auto compound = std::make_unique<CompoundCommand>("Move entity");
    compound->AddCommand(std::make_unique<PropertyChangeCommand<int>>(
        "Set a", 0, 10, [&](const int& v) { a = v; }
    ));
    compound->AddCommand(std::make_unique<PropertyChangeCommand<int>>(
        "Set b", 0, 20, [&](const int& v) { b = v; }
    ));

    mgr.Execute(std::move(compound));
    REQUIRE(a == 10);
    REQUIRE(b == 20);

    mgr.Undo();
    REQUIRE(a == 0);
    REQUIRE(b == 0);
}

// ─── History limit ──────────────────────────────────────────────────────
TEST_CASE("UndoRedo - history limit trims old commands", "[editor][undo]") {
    UndoRedoManager mgr(3);  // max 3 history
    int value = 0;

    for (int i = 1; i <= 5; ++i) {
        mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
            "Step", value, i * 10,
            [&](const int& v) { value = v; }
        ));
        value = i * 10;
    }

    REQUIRE(mgr.GetUndoCount() == 3);  // Oldest 2 trimmed
}

// ─── Empty stack behavior ───────────────────────────────────────────────
TEST_CASE("UndoRedo - undo on empty stack returns false", "[editor][undo]") {
    UndoRedoManager mgr;
    REQUIRE_FALSE(mgr.CanUndo());
    REQUIRE_FALSE(mgr.Undo());
    REQUIRE_FALSE(mgr.CanRedo());
    REQUIRE_FALSE(mgr.Redo());
}

// ─── Description ────────────────────────────────────────────────────────
TEST_CASE("UndoRedo - descriptions are correct", "[editor][undo]") {
    UndoRedoManager mgr;
    int v = 0;

    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "Change X", 0, 1, [&](const int& val) { v = val; }
    ));
    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "Change Y", 1, 2, [&](const int& val) { v = val; }
    ));

    REQUIRE(mgr.GetUndoDescription() == "Change Y");
    mgr.Undo();
    REQUIRE(mgr.GetUndoDescription() == "Change X");
    REQUIRE(mgr.GetRedoDescription() == "Change Y");
}

// ─── Clear ──────────────────────────────────────────────────────────────
TEST_CASE("UndoRedo - clear empties both stacks", "[editor][undo]") {
    UndoRedoManager mgr;
    int v = 0;

    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "Test", 0, 1, [&](const int& val) { v = val; }
    ));
    mgr.Undo();

    REQUIRE(mgr.CanRedo());
    mgr.Clear();
    REQUIRE_FALSE(mgr.CanUndo());
    REQUIRE_FALSE(mgr.CanRedo());
}

// ─── glm::vec3 property ─────────────────────────────────────────────────
TEST_CASE("UndoRedo - glm::vec3 property change", "[editor][undo]") {
    UndoRedoManager mgr;
    glm::vec3 position(0.0f, 0.0f, 0.0f);

    mgr.Execute(std::make_unique<PropertyChangeCommand<glm::vec3>>(
        "Move entity",
        glm::vec3(0.0f),
        glm::vec3(10.0f, 20.0f, 0.0f),
        [&](const glm::vec3& v) { position = v; }
    ));

    REQUIRE(position.x == Approx(10.0f));
    REQUIRE(position.y == Approx(20.0f));

    mgr.Undo();
    REQUIRE(position.x == Approx(0.0f));
    REQUIRE(position.y == Approx(0.0f));
}
