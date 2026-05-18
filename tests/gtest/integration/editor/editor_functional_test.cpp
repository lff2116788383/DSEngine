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

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"

// Editor modules (headless-safe, no ImGui/GLFW calls in these paths)
#include "apps/editor_cpp/src/editor_shared_components.h"
#include "apps/editor_cpp/src/editor_undo.h"
#include "apps/editor_cpp/src/editor_scene_io.h"
#include "apps/editor_cpp/src/editor_prefab.h"
#include "apps/editor_cpp/src/editor_scene_tabs.h"
#include "apps/editor_cpp/src/editor_test_harness.h"
#include "apps/editor_cpp/src/editor_snapshot.h"

using namespace dse;
using dse::editor::EditorNameComponent;
using dse::editor::CopyRegistry;
using dse::editor::SaveScene;
using dse::editor::LoadScene;

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

// ============================================================
// Test 10: UndoRedo max-history trim
// ============================================================

TEST_F(EditorFunctionalTest, UndoRedoMaxHistoryTrim) {
    dse::editor::UndoRedoManager undo_mgr(3);  // 最多保留 3 步

    int value = 0;
    for (int i = 1; i <= 5; ++i) {
        undo_mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<int>>(
            "Set " + std::to_string(i), value, i,
            [&value](const int& v) { value = v; }));
    }

    // 执行了 5 条，但上限 3，undo 栈只剩最新 3 条
    EXPECT_EQ(undo_mgr.GetUndoCount(), 3);
    EXPECT_EQ(value, 5);

    // 撤销 3 步后应回到第 2 条命令执行后的状态 (value=2)
    EXPECT_TRUE(undo_mgr.Undo());  // 5→4
    EXPECT_TRUE(undo_mgr.Undo());  // 4→3
    EXPECT_TRUE(undo_mgr.Undo());  // 3→2
    EXPECT_EQ(value, 2);
    EXPECT_FALSE(undo_mgr.CanUndo());  // 最旧两条已被丢弃
}

// ============================================================
// Test 11: SceneIO Camera3D + MeshRenderer roundtrip
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_Camera3DAndMeshRendererRoundTrip) {
    // 创建带 Camera3D 和 MeshRenderer 的实体
    Entity cam_e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(cam_e, "MainCamera");
    reg().emplace<TransformComponent>(cam_e).position = glm::vec3(0.f, 10.f, -20.f);
    auto& cam = reg().emplace<Camera3DComponent>(cam_e);
    cam.fov = 75.0f;
    cam.near_clip = 0.1f;
    cam.far_clip = 500.0f;

    Entity mesh_e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(mesh_e, "MeshObj");
    reg().emplace<TransformComponent>(mesh_e);
    auto& mr = reg().emplace<MeshRendererComponent>(mesh_e);
    mr.mesh_path = "assets/cube.dmesh";
    mr.receive_shadow = false;

    const auto path = TempPath("dse_test_3d_components.dscene");
    SaveScene(reg(), path.string());
    ASSERT_TRUE(std::filesystem::exists(path));

    entt::registry loaded;
    LoadScene(loaded, path.string());

    // 统计
    size_t count = dse::editor::test::CountAliveEntities(loaded);
    EXPECT_EQ(count, 2u);

    bool found_cam = false, found_mesh = false;
    for (auto e : loaded.storage<entt::entity>()) {
        if (!loaded.valid(e)) continue;
        if (!loaded.all_of<EditorNameComponent>(e)) continue;
        const auto& name = loaded.get<EditorNameComponent>(e).name;
        if (name == "MainCamera") {
            found_cam = true;
            ASSERT_TRUE(loaded.all_of<Camera3DComponent>(e));
            EXPECT_NEAR(loaded.get<Camera3DComponent>(e).fov, 75.0f, 0.01f);
            EXPECT_NEAR(loaded.get<Camera3DComponent>(e).far_clip, 500.0f, 0.01f);
        } else if (name == "MeshObj") {
            found_mesh = true;
            ASSERT_TRUE(loaded.all_of<MeshRendererComponent>(e));
            EXPECT_EQ(loaded.get<MeshRendererComponent>(e).mesh_path, "assets/cube.dmesh");
            EXPECT_FALSE(loaded.get<MeshRendererComponent>(e).receive_shadow);
        }
    }
    EXPECT_TRUE(found_cam);
    EXPECT_TRUE(found_mesh);

    CleanupFile(path);
}

// ============================================================
// Test 12: Snapshot entity-count mismatch detection
// ============================================================

TEST_F(EditorFunctionalTest, SnapshotEntityCountMismatch) {
    Entity e1 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e1, "Alpha");
    reg().emplace<TransformComponent>(e1).position = glm::vec3(1.f, 0.f, 0.f);

    std::string snap1 = dse::editor::test::ExportRegistrySnapshot(reg());

    // 添加第二个实体
    Entity e2 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e2, "Beta");
    reg().emplace<TransformComponent>(e2).position = glm::vec3(2.f, 0.f, 0.f);

    std::string snap2 = dse::editor::test::ExportRegistrySnapshot(reg());

    // snap2(2 实体) vs snap1(1 实体) 应检测到差异
    auto diffs = dse::editor::test::CompareSnapshot(snap2, snap1);
    EXPECT_FALSE(diffs.empty()) << "实体数不同应有差异";

    // snap1(1 实体) 与自身比较应无差异
    auto self_diffs = dse::editor::test::CompareSnapshot(snap1, snap1);
    EXPECT_TRUE(self_diffs.empty());
}

// ============================================================
// Test 13: SceneIO 空场景往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_EmptySceneRoundTrip) {
    const auto path = TempPath("dse_test_empty.dscene");
    SaveScene(reg(), path.string());
    ASSERT_TRUE(std::filesystem::exists(path));

    entt::registry loaded;
    LoadScene(loaded, path.string());
    EXPECT_EQ(dse::editor::test::CountAliveEntities(loaded), 0u);

    CleanupFile(path);
}

// ============================================================
// Test 14: SceneIO DirectionalLight3D 往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_DirectionalLight3DRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "Sun");
    reg().emplace<TransformComponent>(e);
    auto& light = reg().emplace<DirectionalLight3DComponent>(e);
    light.intensity = 3.5f;
    light.cast_shadow = true;
    light.color = glm::vec3(1.0f, 0.95f, 0.8f);

    const auto path = TempPath("dse_test_dirlight.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<DirectionalLight3DComponent>(en)) continue;
        found = true;
        const auto& dl = loaded.get<DirectionalLight3DComponent>(en);
        EXPECT_NEAR(dl.intensity, 3.5f, 0.01f);
        EXPECT_TRUE(dl.cast_shadow);
        EXPECT_NEAR(dl.color.r, 1.0f, 0.01f);
        EXPECT_NEAR(dl.color.g, 0.95f, 0.01f);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 15: SceneIO RigidBody2D 往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_RigidBody2DRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "Phys2D");
    reg().emplace<TransformComponent>(e);
    auto& rb = reg().emplace<RigidBody2DComponent>(e);
    rb.type = RigidBody2DType::Static;
    rb.gravity_scale = 0.0f;
    rb.fixed_rotation = true;

    const auto path = TempPath("dse_test_rb2d.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<RigidBody2DComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<RigidBody2DComponent>(en);
        EXPECT_EQ(r.type, RigidBody2DType::Static);
        EXPECT_NEAR(r.gravity_scale, 0.0f, 0.001f);
        EXPECT_TRUE(r.fixed_rotation);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 16: UndoRedo 新命令执行后清空 Redo 栈
// ============================================================

TEST_F(EditorFunctionalTest, UndoRedo_NewCommandClearsRedo) {
    dse::editor::UndoRedoManager mgr;
    int v = 0;

    mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<int>>(
        "A", 0, 1, [&v](const int& x){ v = x; }));
    mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<int>>(
        "B", 1, 2, [&v](const int& x){ v = x; }));
    EXPECT_EQ(v, 2);

    // 撤销一步，Redo 栈应有 1 条
    mgr.Undo();
    EXPECT_EQ(v, 1);
    EXPECT_EQ(mgr.GetRedoCount(), 1);

    // 执行新命令，Redo 栈应被清空
    mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<int>>(
        "C", 1, 99, [&v](const int& x){ v = x; }));
    EXPECT_EQ(v, 99);
    EXPECT_EQ(mgr.GetRedoCount(), 0);
    EXPECT_FALSE(mgr.CanRedo());
}

// ============================================================
// Test 17: SceneIO SpriteRenderer 往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_SpriteRendererRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "Sprite");
    reg().emplace<TransformComponent>(e);
    auto& s = reg().emplace<SpriteRendererComponent>(e);
    s.color = glm::vec4(0.2f, 0.4f, 0.8f, 0.9f);
    s.sorting_layer = 3;
    s.visible = false;

    const auto path = TempPath("dse_test_sprite.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<SpriteRendererComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<SpriteRendererComponent>(en);
        EXPECT_NEAR(r.color.r, 0.2f, 0.01f);
        EXPECT_NEAR(r.color.g, 0.4f, 0.01f);
        EXPECT_NEAR(r.color.b, 0.8f, 0.01f);
        EXPECT_EQ(r.sorting_layer, 3);
        EXPECT_FALSE(r.visible);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 18: SceneIO UILabel 往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_UILabelRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "Label");
    auto& label = reg().emplace<UILabelComponent>(e);
    label.text = "Hello, DSEngine!";
    label.use_localization = true;
    label.localization_key = "ui.greeting";

    const auto path = TempPath("dse_test_uilabel.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<UILabelComponent>(en)) continue;
        found = true;
        const auto& l = loaded.get<UILabelComponent>(en);
        EXPECT_EQ(l.text, "Hello, DSEngine!");
        EXPECT_TRUE(l.use_localization);
        EXPECT_EQ(l.localization_key, "ui.greeting");
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 19: SceneIO ParticleEmitter 往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_ParticleEmitterRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "Particles");
    auto& emitter = reg().emplace<ParticleEmitterComponent>(e);
    emitter.emit_rate = 50.0f;
    emitter.max_particles = 200;
    emitter.emitting = false;

    const auto path = TempPath("dse_test_particle.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<ParticleEmitterComponent>(en)) continue;
        found = true;
        const auto& pe = loaded.get<ParticleEmitterComponent>(en);
        EXPECT_NEAR(pe.emit_rate, 50.0f, 0.01f);
        EXPECT_EQ(pe.max_particles, 200);
        EXPECT_FALSE(pe.emitting);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 20: SceneIO SiblingIndex 多实体排序往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_SiblingIndexPreserved) {
    Entity e0 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e0, "E0");
    reg().emplace<dse::editor::SiblingIndexComponent>(e0).index = 2;

    Entity e1 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e1, "E1");
    reg().emplace<dse::editor::SiblingIndexComponent>(e1).index = 0;

    Entity e2 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e2, "E2");
    reg().emplace<dse::editor::SiblingIndexComponent>(e2).index = 1;

    const auto path = TempPath("dse_test_sibling.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 3u);

    std::vector<int> indices;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<dse::editor::SiblingIndexComponent>(en)) continue;
        indices.push_back(loaded.get<dse::editor::SiblingIndexComponent>(en).index);
    }
    ASSERT_EQ(indices.size(), 3u);
    std::sort(indices.begin(), indices.end());
    EXPECT_EQ(indices[0], 0);
    EXPECT_EQ(indices[1], 1);
    EXPECT_EQ(indices[2], 2);
    CleanupFile(path);
}

// ============================================================
// Test 21: Prefab 实例化后 IsPrefabInstance 为真
// ============================================================

TEST_F(EditorFunctionalTest, PrefabInstance_IsPrefabInstance) {
    Entity src = world.CreateEntity();
    reg().emplace<EditorNameComponent>(src, "SourceEntity");
    reg().emplace<TransformComponent>(src);

    const auto prefab_path = TempPath("dse_test_isprefab.dprefab");
    ASSERT_TRUE(dse::editor::SaveEntityAsPrefab(reg(), src, prefab_path.string()));

    Entity inst = dse::editor::InstantiatePrefab(world, reg(), prefab_path.string());
    ASSERT_TRUE(reg().valid(inst));

    EXPECT_TRUE(reg().all_of<EditorNameComponent>(inst));
    EXPECT_TRUE(dse::editor::IsPrefabInstance(reg(), inst));
    EXPECT_FALSE(dse::editor::IsPrefabInstance(reg(), src));

    CleanupFile(prefab_path);
}

// ============================================================
// Test 22: Prefab 多实例独立性
// ============================================================

TEST_F(EditorFunctionalTest, PrefabMultipleInstances_Independent) {
    Entity src = world.CreateEntity();
    reg().emplace<EditorNameComponent>(src, "PrefabSrc");
    reg().emplace<TransformComponent>(src);

    const auto prefab_path = TempPath("dse_test_multi_prefab.dprefab");
    ASSERT_TRUE(dse::editor::SaveEntityAsPrefab(reg(), src, prefab_path.string()));

    Entity inst1 = dse::editor::InstantiatePrefab(world, reg(), prefab_path.string());
    Entity inst2 = dse::editor::InstantiatePrefab(world, reg(), prefab_path.string());
    ASSERT_NE(inst1, inst2);
    ASSERT_TRUE(reg().valid(inst1));
    ASSERT_TRUE(reg().valid(inst2));

    ASSERT_TRUE(reg().all_of<EditorNameComponent>(inst1));
    reg().get<EditorNameComponent>(inst1).name = "ModifiedInst1";

    ASSERT_TRUE(reg().all_of<EditorNameComponent>(inst2));
    EXPECT_NE(reg().get<EditorNameComponent>(inst2).name, "ModifiedInst1");

    CleanupFile(prefab_path);
}

// ============================================================
// Test 23: SceneTabManager 实体隔离
// ============================================================

TEST_F(EditorFunctionalTest, SceneTabManager_EntityIsolation) {
    auto& tab_mgr = dse::editor::SceneTabManager::Get();
    tab_mgr.Init("Untitled");

    Entity e_a = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e_a, "Tab0EntityA");
    ASSERT_EQ(dse::editor::test::CountAliveEntities(reg()), 1u);

    int tab1 = tab_mgr.NewScene(reg());
    EXPECT_EQ(tab1, 1);
    EXPECT_EQ(tab_mgr.GetActiveIndex(), 1);

    Entity e_b = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e_b, "Tab1EntityB");
    ASSERT_EQ(dse::editor::test::CountAliveEntities(reg()), 1u);

    tab_mgr.SwitchTo(0, reg());
    EXPECT_EQ(tab_mgr.GetActiveIndex(), 0);

    bool found_a = false, found_b = false;
    for (auto en : reg().storage<entt::entity>()) {
        if (!reg().valid(en)) continue;
        if (!reg().all_of<EditorNameComponent>(en)) continue;
        const auto& name = reg().get<EditorNameComponent>(en).name;
        if (name == "Tab0EntityA") found_a = true;
        if (name == "Tab1EntityB") found_b = true;
    }
    EXPECT_TRUE(found_a);
    EXPECT_FALSE(found_b);
}

// ============================================================
// Test 24: CopyRegistry 多组件完整性
// ============================================================

TEST_F(EditorFunctionalTest, CopyRegistry_MultiComponent_Integrity) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "MultiComp");
    reg().emplace<TransformComponent>(e);
    auto& dl = reg().emplace<dse::DirectionalLight3DComponent>(e);
    dl.intensity = 2.5f;
    dl.cast_shadow = true;
    auto& rb = reg().emplace<RigidBody2DComponent>(e);
    rb.type = RigidBody2DType::Dynamic;
    rb.gravity_scale = 2.0f;

    entt::registry copy;
    dse::editor::CopyRegistry(copy, reg());

    ASSERT_EQ(dse::editor::test::CountAliveEntities(copy), 1u);

    bool found = false;
    for (auto en : copy.storage<entt::entity>()) {
        if (!copy.valid(en)) continue;
        if (!copy.all_of<dse::DirectionalLight3DComponent, RigidBody2DComponent>(en)) continue;
        found = true;
        const auto& d = copy.get<dse::DirectionalLight3DComponent>(en);
        const auto& r = copy.get<RigidBody2DComponent>(en);
        EXPECT_NEAR(d.intensity, 2.5f, 0.01f);
        EXPECT_TRUE(d.cast_shadow);
        EXPECT_EQ(r.type, RigidBody2DType::Dynamic);
        EXPECT_NEAR(r.gravity_scale, 2.0f, 0.01f);
    }
    EXPECT_TRUE(found);
}

// ============================================================
// Test 25: SceneIO PointLight 往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_PointLightRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "PointLightEnt");
    auto& pl = reg().emplace<dse::PointLightComponent>(e);
    pl.intensity = 3.5f;
    pl.radius = 20.0f;
    pl.falloff = 1.5f;
    pl.cast_shadow = true;
    pl.color = glm::vec3(0.9f, 0.7f, 0.3f);

    const auto path = TempPath("dse_test_pointlight.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<dse::PointLightComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<dse::PointLightComponent>(en);
        EXPECT_NEAR(r.intensity, 3.5f, 0.01f);
        EXPECT_NEAR(r.radius, 20.0f, 0.01f);
        EXPECT_NEAR(r.falloff, 1.5f, 0.01f);
        EXPECT_TRUE(r.cast_shadow);
        EXPECT_NEAR(r.color.r, 0.9f, 0.01f);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 26: SceneIO SpotLight 往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_SpotLightRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "SpotLightEnt");
    auto& sl = reg().emplace<dse::SpotLightComponent>(e);
    sl.intensity = 2.0f;
    sl.radius = 15.0f;
    sl.inner_cone_angle = 15.0f;
    sl.outer_cone_angle = 30.0f;
    sl.cast_shadow = false;
    sl.color = glm::vec3(0.5f, 0.5f, 1.0f);

    const auto path = TempPath("dse_test_spotlight.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<dse::SpotLightComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<dse::SpotLightComponent>(en);
        EXPECT_NEAR(r.intensity, 2.0f, 0.01f);
        EXPECT_NEAR(r.inner_cone_angle, 15.0f, 0.01f);
        EXPECT_NEAR(r.outer_cone_angle, 30.0f, 0.01f);
        EXPECT_FALSE(r.cast_shadow);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 27: SceneIO SkyLight 往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_SkyLightRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "SkyLightEnt");
    auto& sky = reg().emplace<dse::SkyLightComponent>(e);
    sky.intensity = 0.8f;
    sky.up_color = glm::vec3(0.4f, 0.6f, 1.0f);
    sky.down_color = glm::vec3(0.2f, 0.15f, 0.1f);

    const auto path = TempPath("dse_test_skylight.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<dse::SkyLightComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<dse::SkyLightComponent>(en);
        EXPECT_NEAR(r.intensity, 0.8f, 0.01f);
        EXPECT_NEAR(r.up_color.r, 0.4f, 0.01f);
        EXPECT_NEAR(r.down_color.g, 0.15f, 0.01f);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 28: SceneTabManager dirty 状态追踪
// ============================================================

TEST_F(EditorFunctionalTest, SceneTabManager_DirtyStateTracking) {
    auto& tab_mgr = dse::editor::SceneTabManager::Get();
    tab_mgr.Init("Untitled");

    EXPECT_FALSE(tab_mgr.GetActiveTab().dirty);
    EXPECT_FALSE(tab_mgr.IsAnyTabDirty());

    tab_mgr.MarkDirty();
    EXPECT_TRUE(tab_mgr.GetActiveTab().dirty);
    EXPECT_TRUE(tab_mgr.IsAnyTabDirty());

    tab_mgr.MarkClean();
    EXPECT_FALSE(tab_mgr.GetActiveTab().dirty);
    EXPECT_FALSE(tab_mgr.IsAnyTabDirty());

    tab_mgr.NewScene(reg());
    tab_mgr.MarkDirty();
    EXPECT_TRUE(tab_mgr.IsAnyTabDirty());

    tab_mgr.SwitchTo(0, reg());
    EXPECT_FALSE(tab_mgr.GetActiveTab().dirty);
    EXPECT_TRUE(tab_mgr.IsAnyTabDirty());

    tab_mgr.Init("Untitled");
}

// ============================================================
// Test 29: Prefab 多组件完整性
// ============================================================

TEST_F(EditorFunctionalTest, Prefab_MultiComponent_RoundTrip) {
    Entity src = world.CreateEntity();
    reg().emplace<EditorNameComponent>(src, "MultiCompPrefab");
    auto& t = reg().emplace<TransformComponent>(src);
    t.position = glm::vec3(5.0f, 0.0f, 0.0f);
    t.scale = glm::vec3(2.0f, 2.0f, 2.0f);
    auto& mr = reg().emplace<dse::MeshRendererComponent>(src);
    mr.mesh_path = "assets/sphere.dmesh";
    mr.metallic = 0.8f;
    auto& anim = reg().emplace<dse::Animator3DComponent>(src);
    anim.dskel_path = "assets/hero.dskel";
    anim.speed = 0.5f;

    const auto prefab_path = TempPath("dse_test_multicomp.dprefab");
    ASSERT_TRUE(dse::editor::SaveEntityAsPrefab(reg(), src, prefab_path.string()));

    Entity inst = dse::editor::InstantiatePrefab(world, reg(), prefab_path.string());
    ASSERT_TRUE(reg().valid(inst));

    ASSERT_TRUE(reg().all_of<TransformComponent>(inst));
    ASSERT_TRUE(reg().all_of<dse::MeshRendererComponent>(inst));
    ASSERT_TRUE(reg().all_of<dse::Animator3DComponent>(inst));

    EXPECT_NEAR(reg().get<TransformComponent>(inst).position.x, 5.0f, 0.01f);
    EXPECT_EQ(reg().get<dse::MeshRendererComponent>(inst).mesh_path, "assets/sphere.dmesh");
    EXPECT_NEAR(reg().get<dse::MeshRendererComponent>(inst).metallic, 0.8f, 0.01f);
    EXPECT_EQ(reg().get<dse::Animator3DComponent>(inst).dskel_path, "assets/hero.dskel");
    EXPECT_NEAR(reg().get<dse::Animator3DComponent>(inst).speed, 0.5f, 0.01f);

    CleanupFile(prefab_path);
}

// ============================================================
// Test 30: SceneIO Animator3D 往返
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_Animator3DRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "AnimatorEnt");
    auto& anim = reg().emplace<dse::Animator3DComponent>(e);
    anim.dskel_path = "assets/hero.dskel";
    anim.danim_path = "assets/walk.danim";
    anim.speed = 1.5f;
    anim.loop = false;

    const auto path = TempPath("dse_test_animator3d.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<dse::Animator3DComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<dse::Animator3DComponent>(en);
        EXPECT_EQ(r.dskel_path, "assets/hero.dskel");
        EXPECT_EQ(r.danim_path, "assets/walk.danim");
        EXPECT_NEAR(r.speed, 1.5f, 0.01f);
        EXPECT_FALSE(r.loop);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 31: SceneIO RigidBody3D 往返 + CopyRegistry bug 验证
// ============================================================

TEST_F(EditorFunctionalTest, SceneIO_RigidBody3DRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "RB3DEnt");
    auto& rb = reg().emplace<dse::RigidBody3DComponent>(e);
    rb.type = dse::RigidBody3DType::Dynamic;
    rb.mass = 5.0f;
    rb.gravity_scale = 0.5f;
    rb.use_gravity = true;

    const auto path = TempPath("dse_test_rigidbody3d.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<dse::RigidBody3DComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<dse::RigidBody3DComponent>(en);
        EXPECT_EQ(r.type, dse::RigidBody3DType::Dynamic);
        EXPECT_NEAR(r.mass, 5.0f, 0.01f);
        EXPECT_NEAR(r.gravity_scale, 0.5f, 0.01f);
        EXPECT_TRUE(r.use_gravity);
        EXPECT_EQ(r.runtime_body, static_cast<void*>(nullptr));
    }
    EXPECT_TRUE(found);
    CleanupFile(path);

    // CopyRegistry 不再丢失 RigidBody3D（bug fix 验证）
    entt::registry copy;
    dse::editor::CopyRegistry(copy, reg());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(copy), 1u);
    bool copy_found = false;
    for (auto en : copy.storage<entt::entity>()) {
        if (!copy.valid(en)) continue;
        if (!copy.all_of<dse::RigidBody3DComponent>(en)) continue;
        copy_found = true;
        EXPECT_NEAR(copy.get<dse::RigidBody3DComponent>(en).mass, 5.0f, 0.01f);
        EXPECT_EQ(copy.get<dse::RigidBody3DComponent>(en).runtime_body, static_cast<void*>(nullptr));
    }
    EXPECT_TRUE(copy_found);
}
