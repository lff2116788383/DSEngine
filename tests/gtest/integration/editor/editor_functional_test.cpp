/**
 * @file editor_functional_test.cpp
 * @brief 编辑器无头功能测试套件 (Headless Functional Tests)
 *
 * 直接调用编辑器 API（不依赖 ImGui/GLFW），验证：
 * - Create Entity 添加到 Registry
 * - Undo/Redo 恢复状态
 * - SaveScene / LoadScene 往返一致
 * - Prefab 保存/加载
 * - SceneTabManager 多标签切换
 * - BuildGame 对话框默认值
 * - Registry 快照导出/对比
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"

// Editor modules (headless-safe, no ImGui/GLFW calls in these paths)
#include "apps/editor_cpp/src/editor_shared_components.h"
#include "apps/editor_cpp/src/editor_undo.h"
#include "apps/editor_cpp/src/editor_scene_io.h"
#include "apps/editor_cpp/src/editor_prefab.h"
#include "apps/editor_cpp/src/editor_scene_tabs.h"
#include "apps/editor_cpp/src/editor_test_harness.h"
#include "apps/editor_cpp/src/editor_snapshot.h"

// ============================================================
// Test Fixture
// ============================================================

class EditorFunctionalTest : public ::testing::Test {
protected:
    World world;

    entt::registry& reg() { return world.registry(); }

    void TearDown() override {
        world.Clear();
    }

    static std::filesystem::path TempPath(const std::string& filename) {
        return std::filesystem::temp_directory_path() / filename;
    }

    static void CleanupFile(const std::filesystem::path& path) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
};

// ============================================================
// Test 1: CreateEntityAddsToRegistry
// ============================================================

TEST_F(EditorFunctionalTest, CreateEntityAddsToRegistry) {
    EXPECT_EQ(world.EntityCount(), 0u);

    Entity e1 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e1, "TestEntity1");
    reg().emplace<TransformComponent>(e1);

    EXPECT_EQ(world.EntityCount(), 1u);
    EXPECT_TRUE(world.IsAlive(e1));
    EXPECT_TRUE(reg().all_of<EditorNameComponent>(e1));
    EXPECT_TRUE(reg().all_of<TransformComponent>(e1));
    EXPECT_EQ(reg().get<EditorNameComponent>(e1).name, "TestEntity1");

    Entity e2 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e2, "TestEntity2");
    auto& t2 = reg().emplace<TransformComponent>(e2);
    t2.position = glm::vec3(1.0f, 2.0f, 3.0f);

    EXPECT_EQ(world.EntityCount(), 2u);
    EXPECT_FLOAT_EQ(reg().get<TransformComponent>(e2).position.x, 1.0f);
    EXPECT_FLOAT_EQ(reg().get<TransformComponent>(e2).position.y, 2.0f);
    EXPECT_FLOAT_EQ(reg().get<TransformComponent>(e2).position.z, 3.0f);

    world.DestroyEntity(e1);
    EXPECT_EQ(world.EntityCount(), 1u);
    EXPECT_FALSE(world.IsAlive(e1));
    EXPECT_TRUE(world.IsAlive(e2));
}

// ============================================================
// Test 2: UndoRedoRestoresState
// ============================================================

TEST_F(EditorFunctionalTest, UndoRedoRestoresState) {
    dse::editor::UndoRedoManager undo_mgr;

    // Initial state: position at origin
    Entity e = world.CreateEntity();
    auto& t = reg().emplace<TransformComponent>(e);
    t.position = glm::vec3(0.0f, 0.0f, 0.0f);

    EXPECT_FALSE(undo_mgr.CanUndo());
    EXPECT_FALSE(undo_mgr.CanRedo());

    // Execute: move to (10, 20, 30)
    glm::vec3 old_pos = t.position;
    glm::vec3 new_pos = glm::vec3(10.0f, 20.0f, 30.0f);
    auto cmd = std::make_unique<dse::editor::PropertyChangeCommand<glm::vec3>>(
        "Move Entity",
        old_pos, new_pos,
        [&t](const glm::vec3& v) { t.position = v; }
    );
    undo_mgr.Execute(std::move(cmd));

    EXPECT_FLOAT_EQ(t.position.x, 10.0f);
    EXPECT_FLOAT_EQ(t.position.y, 20.0f);
    EXPECT_FLOAT_EQ(t.position.z, 30.0f);
    EXPECT_TRUE(undo_mgr.CanUndo());
    EXPECT_EQ(undo_mgr.GetUndoDescription(), "Move Entity");

    // Undo: back to origin
    EXPECT_TRUE(undo_mgr.Undo());
    EXPECT_FLOAT_EQ(t.position.x, 0.0f);
    EXPECT_FLOAT_EQ(t.position.y, 0.0f);
    EXPECT_FLOAT_EQ(t.position.z, 0.0f);
    EXPECT_FALSE(undo_mgr.CanUndo());
    EXPECT_TRUE(undo_mgr.CanRedo());

    // Redo: back to (10, 20, 30)
    EXPECT_TRUE(undo_mgr.Redo());
    EXPECT_FLOAT_EQ(t.position.x, 10.0f);
    EXPECT_FLOAT_EQ(t.position.y, 20.0f);
    EXPECT_FLOAT_EQ(t.position.z, 30.0f);
    EXPECT_TRUE(undo_mgr.CanUndo());
    EXPECT_FALSE(undo_mgr.CanRedo());
}

TEST_F(EditorFunctionalTest, UndoRedoLambdaCommandCreateDestroy) {
    dse::editor::UndoRedoManager undo_mgr;

    Entity created = entt::null;

    // Execute: create entity via LambdaCommand
    auto cmd = std::make_unique<dse::editor::LambdaCommand>(
        "Create Entity",
        [&]() {
            created = world.CreateEntity();
            reg().emplace<EditorNameComponent>(created, "LambdaEntity");
            reg().emplace<TransformComponent>(created);
        },
        [&]() {
            if (world.IsAlive(created)) {
                world.DestroyEntity(created);
                created = entt::null;
            }
        }
    );
    undo_mgr.Execute(std::move(cmd));

    EXPECT_TRUE(created != entt::null);
    EXPECT_EQ(world.EntityCount(), 1u);
    EXPECT_TRUE(reg().all_of<EditorNameComponent>(created));

    // Undo: entity destroyed
    undo_mgr.Undo();
    EXPECT_EQ(world.EntityCount(), 0u);

    // Redo: entity re-created
    undo_mgr.Redo();
    EXPECT_EQ(world.EntityCount(), 1u);
}

TEST_F(EditorFunctionalTest, UndoRedoCompoundCommand) {
    dse::editor::UndoRedoManager undo_mgr;

    Entity e = world.CreateEntity();
    auto& t = reg().emplace<TransformComponent>(e);
    t.position = glm::vec3(0.0f);
    t.scale = glm::vec3(1.0f);

    // Compound: move + scale
    auto compound = std::make_unique<dse::editor::CompoundCommand>("Move+Scale");
    compound->AddCommand(std::make_unique<dse::editor::PropertyChangeCommand<glm::vec3>>(
        "Move", t.position, glm::vec3(5.0f, 5.0f, 5.0f),
        [&t](const glm::vec3& v) { t.position = v; }));
    compound->AddCommand(std::make_unique<dse::editor::PropertyChangeCommand<glm::vec3>>(
        "Scale", t.scale, glm::vec3(2.0f, 2.0f, 2.0f),
        [&t](const glm::vec3& v) { t.scale = v; }));

    undo_mgr.Execute(std::move(compound));
    EXPECT_FLOAT_EQ(t.position.x, 5.0f);
    EXPECT_FLOAT_EQ(t.scale.x, 2.0f);

    undo_mgr.Undo();
    EXPECT_FLOAT_EQ(t.position.x, 0.0f);
    EXPECT_FLOAT_EQ(t.scale.x, 1.0f);

    undo_mgr.Redo();
    EXPECT_FLOAT_EQ(t.position.x, 5.0f);
    EXPECT_FLOAT_EQ(t.scale.x, 2.0f);
}

TEST_F(EditorFunctionalTest, UndoRedoMerge) {
    dse::editor::UndoRedoManager undo_mgr;

    int value = 0;

    // Execute first command
    undo_mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<int>>(
        "Change Value", 0, 1,
        [&value](const int& v) { value = v; }));
    EXPECT_EQ(value, 1);

    // Execute second with merge
    undo_mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<int>>(
        "Change Value", 1, 5,
        [&value](const int& v) { value = v; }), true);
    EXPECT_EQ(value, 5);

    // Should have merged: single undo goes back to 0
    EXPECT_EQ(undo_mgr.GetUndoCount(), 1);
    undo_mgr.Undo();
    EXPECT_EQ(value, 0);
}

// ============================================================
// Test 3: SaveLoadRoundTrip
// ============================================================

TEST_F(EditorFunctionalTest, SaveLoadRoundTrip) {
    // Create entities with various components
    Entity e1 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e1, "Hero");
    auto& t1 = reg().emplace<TransformComponent>(e1);
    t1.position = glm::vec3(100.0f, 200.0f, 0.0f);
    t1.scale = glm::vec3(2.0f, 2.0f, 1.0f);

    Entity e2 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e2, "Ground");
    auto& t2 = reg().emplace<TransformComponent>(e2);
    t2.position = glm::vec3(0.0f, -50.0f, 0.0f);

    EXPECT_EQ(world.EntityCount(), 2u);

    // Save
    const auto scene_path = TempPath("dse_editor_test_scene.dscene");
    SaveScene(reg(), scene_path.string());
    EXPECT_TRUE(std::filesystem::exists(scene_path));

    // Load into fresh registry
    entt::registry loaded_reg;
    LoadScene(loaded_reg, scene_path.string());

    // Verify entity count
    size_t loaded_count = dse::editor::test::CountAliveEntities(loaded_reg);
    EXPECT_EQ(loaded_count, 2u);

    // Verify by scanning loaded entities
    bool found_hero = false;
    bool found_ground = false;
    for (auto entity : loaded_reg.storage<entt::entity>()) {
        if (!loaded_reg.valid(entity)) continue;
        if (loaded_reg.all_of<EditorNameComponent>(entity)) {
            const auto& name = loaded_reg.get<EditorNameComponent>(entity).name;
            if (name == "Hero") {
                found_hero = true;
                ASSERT_TRUE(loaded_reg.all_of<TransformComponent>(entity));
                const auto& t = loaded_reg.get<TransformComponent>(entity);
                EXPECT_FLOAT_EQ(t.position.x, 100.0f);
                EXPECT_FLOAT_EQ(t.position.y, 200.0f);
                EXPECT_FLOAT_EQ(t.scale.x, 2.0f);
            } else if (name == "Ground") {
                found_ground = true;
                ASSERT_TRUE(loaded_reg.all_of<TransformComponent>(entity));
                const auto& t = loaded_reg.get<TransformComponent>(entity);
                EXPECT_FLOAT_EQ(t.position.y, -50.0f);
            }
        }
    }
    EXPECT_TRUE(found_hero);
    EXPECT_TRUE(found_ground);

    CleanupFile(scene_path);
}

// ============================================================
// Test 4: PrefabSaveLoadRoundTrip
// ============================================================

TEST_F(EditorFunctionalTest, PrefabSaveLoadRoundTrip) {
    Entity source = world.CreateEntity();
    reg().emplace<EditorNameComponent>(source, "PrefabSource");
    auto& t = reg().emplace<TransformComponent>(source);
    t.position = glm::vec3(10.0f, 20.0f, 30.0f);
    t.scale = glm::vec3(3.0f, 3.0f, 3.0f);

    const auto prefab_path = TempPath("dse_editor_test.dprefab");

    // Save prefab
    bool saved = dse::editor::SaveEntityAsPrefab(reg(), source, prefab_path.string());
    ASSERT_TRUE(saved);
    ASSERT_TRUE(std::filesystem::exists(prefab_path));

    // Verify file is valid JSON with dprefab type
    {
        std::ifstream ifs(prefab_path);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        EXPECT_NE(content.find("dprefab"), std::string::npos);
        EXPECT_NE(content.find("PrefabSource"), std::string::npos);
    }

    // Instantiate into same world
    Entity instance = dse::editor::InstantiatePrefab(world, reg(), prefab_path.string());
    ASSERT_TRUE(instance != entt::null);
    ASSERT_TRUE(world.IsAlive(instance));
    ASSERT_TRUE(reg().all_of<EditorNameComponent>(instance));
    ASSERT_TRUE(reg().all_of<TransformComponent>(instance));

    // Verify component values survived round-trip
    const auto& inst_name = reg().get<EditorNameComponent>(instance).name;
    EXPECT_EQ(inst_name, "PrefabSource");

    const auto& inst_t = reg().get<TransformComponent>(instance);
    EXPECT_NEAR(inst_t.position.x, 10.0f, 0.01f);
    EXPECT_NEAR(inst_t.position.y, 20.0f, 0.01f);
    EXPECT_NEAR(inst_t.position.z, 30.0f, 0.01f);
    EXPECT_NEAR(inst_t.scale.x, 3.0f, 0.01f);

    // Verify it's marked as prefab instance
    EXPECT_TRUE(dse::editor::IsPrefabInstance(reg(), instance));
    EXPECT_FALSE(dse::editor::IsPrefabInstance(reg(), source));

    // Now we have 2 entities (source + instance)
    EXPECT_EQ(world.EntityCount(), 2u);

    CleanupFile(prefab_path);
}

// ============================================================
// Test 5: SceneTabSwitching
// ============================================================

TEST_F(EditorFunctionalTest, SceneTabSwitching) {
    // SceneTabManager is a singleton that touches ImGui in DrawTabBar.
    // Here we test the data-layer API only (Init/NewScene/SwitchTo/Close).
    auto& tab_mgr = dse::editor::SceneTabManager::Get();

    tab_mgr.Init("Untitled");
    EXPECT_EQ(tab_mgr.GetTabCount(), 1);
    EXPECT_EQ(tab_mgr.GetActiveIndex(), 0);
    EXPECT_EQ(tab_mgr.GetActiveDisplayName(), "Untitled");

    // Create a scene entity in tab 0
    Entity e0 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e0, "Tab0Entity");
    reg().emplace<TransformComponent>(e0);
    EXPECT_EQ(world.EntityCount(), 1u);

    // Open a new tab (this snapshots current tab and clears registry)
    int tab1 = tab_mgr.NewScene(reg());
    EXPECT_EQ(tab_mgr.GetTabCount(), 2);
    EXPECT_EQ(tab_mgr.GetActiveIndex(), tab1);

    // Mark dirty
    tab_mgr.MarkDirty();
    EXPECT_TRUE(tab_mgr.GetActiveTab().dirty);

    // Mark clean
    tab_mgr.MarkClean();
    EXPECT_FALSE(tab_mgr.GetActiveTab().dirty);

    // Set path
    tab_mgr.SetCurrentPath("test/scene2.dscene");
    EXPECT_EQ(tab_mgr.GetActiveFilePath(), "test/scene2.dscene");

    // Find tab by path
    int found = tab_mgr.FindTabByPath("test/scene2.dscene");
    EXPECT_EQ(found, tab1);
    EXPECT_EQ(tab_mgr.FindTabByPath("nonexistent.dscene"), -1);

    // Switch back to tab 0
    tab_mgr.SwitchTo(0, reg());
    EXPECT_EQ(tab_mgr.GetActiveIndex(), 0);

    // Close tab
    tab_mgr.CloseTab(tab1, reg());
    EXPECT_EQ(tab_mgr.GetTabCount(), 1);

    // Reset to a clean state for other tests
    tab_mgr.Init("Untitled");
}

// ============================================================
// Test 6: UndoRedoHistoryAndClear
// ============================================================

TEST_F(EditorFunctionalTest, UndoRedoHistoryAndClear) {
    dse::editor::UndoRedoManager undo_mgr;

    int value = 0;

    // Execute multiple commands
    for (int i = 1; i <= 5; ++i) {
        undo_mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<int>>(
            "Set to " + std::to_string(i), value, i,
            [&value](const int& v) { value = v; }));
    }
    EXPECT_EQ(value, 5);
    EXPECT_EQ(undo_mgr.GetUndoCount(), 5);
    EXPECT_EQ(undo_mgr.GetRedoCount(), 0);

    // Check undo history (newest first)
    auto undo_history = undo_mgr.GetUndoHistory();
    ASSERT_EQ(undo_history.size(), 5u);
    EXPECT_EQ(undo_history[0], "Set to 5");
    EXPECT_EQ(undo_history[4], "Set to 1");

    // Undo 2 steps
    undo_mgr.Undo();
    undo_mgr.Undo();
    EXPECT_EQ(value, 3);
    EXPECT_EQ(undo_mgr.GetUndoCount(), 3);
    EXPECT_EQ(undo_mgr.GetRedoCount(), 2);

    // Check redo history
    auto redo_history = undo_mgr.GetRedoHistory();
    ASSERT_EQ(redo_history.size(), 2u);
    EXPECT_EQ(redo_history[0], "Set to 4");
    EXPECT_EQ(redo_history[1], "Set to 5");

    // Clear all history
    undo_mgr.Clear();
    EXPECT_EQ(undo_mgr.GetUndoCount(), 0);
    EXPECT_EQ(undo_mgr.GetRedoCount(), 0);
    EXPECT_FALSE(undo_mgr.CanUndo());
    EXPECT_FALSE(undo_mgr.CanRedo());
    // Value stays at last applied state
    EXPECT_EQ(value, 3);
}

// ============================================================
// Test 7: RegistrySnapshotExportCompare
// ============================================================

TEST_F(EditorFunctionalTest, RegistrySnapshotExportCompare) {
    // Build a scene
    Entity e1 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e1, "Snapshot_A");
    auto& t1 = reg().emplace<TransformComponent>(e1);
    t1.position = glm::vec3(1.0f, 2.0f, 3.0f);
    t1.scale = glm::vec3(1.0f, 1.0f, 1.0f);

    Entity e2 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e2, "Snapshot_B");
    auto& t2 = reg().emplace<TransformComponent>(e2);
    t2.position = glm::vec3(4.0f, 5.0f, 6.0f);

    // Export snapshot
    std::string snapshot = dse::editor::test::ExportRegistrySnapshot(reg());
    EXPECT_FALSE(snapshot.empty());
    EXPECT_NE(snapshot.find("Snapshot_A"), std::string::npos);
    EXPECT_NE(snapshot.find("Snapshot_B"), std::string::npos);
    EXPECT_NE(snapshot.find("\"entity_count\""), std::string::npos);

    // Compare with self — should be identical
    auto diffs = dse::editor::test::CompareSnapshot(snapshot, snapshot);
    EXPECT_TRUE(diffs.empty()) << "Self-compare should produce no diffs";

    // Modify scene and re-export
    t1.position = glm::vec3(99.0f, 2.0f, 3.0f);
    std::string snapshot2 = dse::editor::test::ExportRegistrySnapshot(reg());

    auto diffs2 = dse::editor::test::CompareSnapshot(snapshot2, snapshot);
    EXPECT_FALSE(diffs2.empty()) << "Modified scene should produce diffs";

    // Check CountAliveEntities
    size_t alive = dse::editor::test::CountAliveEntities(reg());
    EXPECT_EQ(alive, 2u);
}

// ============================================================
// Test 8: CLI Argument Parsing
// ============================================================

TEST_F(EditorFunctionalTest, CLIArgumentParsing) {
    // Test default config
    {
        char* argv[] = {(char*)"editor"};
        auto config = dse::editor::test::ParseEditorTestArgs(1, argv);
        EXPECT_FALSE(config.headless);
        EXPECT_TRUE(config.replay_path.empty());
        EXPECT_TRUE(config.verify_path.empty());
        EXPECT_TRUE(config.scene_path.empty());
        EXPECT_EQ(config.max_frames, 300);
        EXPECT_FALSE(dse::editor::test::HasTestArgs(config));
    }

    // Test full args
    {
        char* argv[] = {
            (char*)"editor",
            (char*)"--headless",
            (char*)"--replay=input.json",
            (char*)"--verify=expected.json",
            (char*)"--scene=test.dscene",
            (char*)"--max-frames=100"
        };
        auto config = dse::editor::test::ParseEditorTestArgs(6, argv);
        EXPECT_TRUE(config.headless);
        EXPECT_EQ(config.replay_path, "input.json");
        EXPECT_EQ(config.verify_path, "expected.json");
        EXPECT_EQ(config.scene_path, "test.dscene");
        EXPECT_EQ(config.max_frames, 100);
        EXPECT_TRUE(dse::editor::test::HasTestArgs(config));
    }
}

// ============================================================
// Test 9: CopyRegistry round-trip
// ============================================================

TEST_F(EditorFunctionalTest, CopyRegistryRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "CopyTest");
    auto& t = reg().emplace<TransformComponent>(e);
    t.position = glm::vec3(7.0f, 8.0f, 9.0f);

    entt::registry dst;
    CopyRegistry(dst, reg());

    size_t dst_count = dse::editor::test::CountAliveEntities(dst);
    EXPECT_EQ(dst_count, 1u);

    bool found = false;
    for (auto entity : dst.storage<entt::entity>()) {
        if (!dst.valid(entity)) continue;
        if (dst.all_of<EditorNameComponent>(entity)) {
            EXPECT_EQ(dst.get<EditorNameComponent>(entity).name, "CopyTest");
            ASSERT_TRUE(dst.all_of<TransformComponent>(entity));
            EXPECT_FLOAT_EQ(dst.get<TransformComponent>(entity).position.x, 7.0f);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}
