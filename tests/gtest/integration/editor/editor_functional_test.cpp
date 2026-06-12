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
#include "apps/editor_cpp/src/editor_shell.h"
#include "apps/editor_cpp/src/editor_test_harness.h"
#include "apps/editor_cpp/src/editor_snapshot.h"
#include "apps/editor_cpp/src/editor_inspector_registry.h"
#include "apps/editor_cpp/src/editor_settings.h"
#include "apps/editor_cpp/src/editor_chat_protocol.h"

using namespace dse;
using dse::editor::EditorNameComponent;
using dse::editor::SiblingIndexComponent;
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

// 测试 编辑器功能：创建实体添加到注册表
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

// 测试 编辑器功能：撤销重做恢复状态
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

// 测试 编辑器功能：撤销重做lambda命令创建销毁
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

// 测试 编辑器功能：撤销重做复合命令
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

// 测试 编辑器功能：撤销重做合并
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

// 测试 编辑器功能：保存加载往返
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

// 测试 编辑器功能：预制体保存加载往返
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

// 测试 编辑器功能：场景标签页切换
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

// 测试 编辑器功能：撤销重做历史且清空
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

// 测试 编辑器功能：注册表快照导出比较
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

// 测试 编辑器功能：CLI Argument Parsing
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

// 测试 编辑器功能：Copy注册表往返
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

// 测试 编辑器功能：撤销重做最大历史修剪
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

// 测试 编辑器功能：场景IO相机3D且网格渲染器往返
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

// 测试 编辑器功能：快照实体计数Mismatch
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

// 测试 编辑器功能：场景IO空场景往返
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

// 测试 编辑器功能：场景IO方向光灯光3D往返
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

// 测试 编辑器功能：场景IO刚体2D往返
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

// 测试 编辑器功能：撤销重做新建命令清空重做
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

// 测试 编辑器功能：场景IO精灵渲染器往返
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

// 测试 编辑器功能：场景IO UI标签往返
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

// 测试 编辑器功能：场景IO粒子发射器往返
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

// 测试 编辑器功能：场景IO Sibling索引Preserved
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

// 测试 编辑器功能：预制体实例为预制体实例
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

// 测试 编辑器功能：预制体多个实例独立
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

// 测试 编辑器功能：场景标签页管理器实体Isolation
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

// 测试 编辑器功能：Copy注册表多组件Integrity
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

// 测试 编辑器功能：场景IO点灯光往返
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

// 测试 编辑器功能：场景IO聚光灯光往返
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

// 测试 编辑器功能：场景IO天空灯光往返
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

// 测试 编辑器功能：场景标签页管理器脏状态追踪
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

// 测试 编辑器功能：预制体多组件往返
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

// 测试 编辑器功能：场景IO动画器3D往返
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

// 测试 编辑器功能：场景IO刚体3D往返
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

// ============================================================
// Test 32: SceneIO UIAnchor 往返
// ============================================================

// 测试 编辑器功能：场景IO UI锚点往返
TEST_F(EditorFunctionalTest, SceneIO_UIAnchorRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "AnchorEnt");
    auto& anchor = reg().emplace<UIAnchorComponent>(e);
    anchor.anchor = 3;
    anchor.offset = glm::vec2(10.0f, -5.0f);

    const auto path = TempPath("dse_test_uianchor.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<UIAnchorComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<UIAnchorComponent>(en);
        EXPECT_EQ(r.anchor, 3);
        EXPECT_NEAR(r.offset.x, 10.0f, 0.01f);
        EXPECT_NEAR(r.offset.y, -5.0f, 0.01f);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 33: SceneIO 50 实体压力测试（无 ID 碰撞/无丢失）
// ============================================================

// 测试 编辑器功能：场景IO多实体压力
TEST_F(EditorFunctionalTest, SceneIO_MultiEntityStressTest) {
    const int kCount = 50;
    for (int i = 0; i < kCount; ++i) {
        Entity e = world.CreateEntity();
        reg().emplace<EditorNameComponent>(e, "StressEnt_" + std::to_string(i));
        auto& t = reg().emplace<TransformComponent>(e);
        t.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
    }

    const auto path = TempPath("dse_test_stress.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    EXPECT_EQ(dse::editor::test::CountAliveEntities(loaded), static_cast<size_t>(kCount));

    int found_25 = 0;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<EditorNameComponent, TransformComponent>(en)) continue;
        const auto& name = loaded.get<EditorNameComponent>(en).name;
        if (name == "StressEnt_25") {
            found_25++;
            EXPECT_NEAR(loaded.get<TransformComponent>(en).position.x, 25.0f, 0.01f);
        }
    }
    EXPECT_EQ(found_25, 1);
    CleanupFile(path);
}

// ============================================================
// Test 34: SceneIO Skybox 往返
// ============================================================

// 测试 编辑器功能：场景IO天空盒往返
TEST_F(EditorFunctionalTest, SceneIO_SkyboxRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "SkyboxEnt");
    auto& sb = reg().emplace<dse::SkyboxComponent>(e);
    sb.cubemap_path = "assets/sky_night.dcubemap";
    sb.enabled = true;

    const auto path = TempPath("dse_test_skybox.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<dse::SkyboxComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<dse::SkyboxComponent>(en);
        EXPECT_EQ(r.cubemap_path, "assets/sky_night.dcubemap");
        EXPECT_TRUE(r.enabled);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 35: SceneIO UIGridLayout 往返
// ============================================================

// 测试 编辑器功能：场景IO UI网格布局往返
TEST_F(EditorFunctionalTest, SceneIO_UIGridLayoutRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "GridLayoutEnt");
    auto& grid = reg().emplace<UIGridLayoutComponent>(e);
    grid.columns = 4;
    grid.rows = 2;
    grid.cell_size = glm::vec2(120.0f, 80.0f);
    grid.spacing = glm::vec2(8.0f, 6.0f);
    grid.alignment = 1;

    const auto path = TempPath("dse_test_uigrid.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<UIGridLayoutComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<UIGridLayoutComponent>(en);
        EXPECT_EQ(r.columns, 4);
        EXPECT_EQ(r.rows, 2);
        EXPECT_NEAR(r.cell_size.x, 120.0f, 0.01f);
        EXPECT_NEAR(r.spacing.y, 6.0f, 0.01f);
        EXPECT_EQ(r.alignment, 1);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 36: SceneTabManager CloseTab 行为
// ============================================================

// 测试 编辑器功能：场景标签页管理器关闭标签页
TEST_F(EditorFunctionalTest, SceneTabManager_CloseTab) {
    auto& tab_mgr = dse::editor::SceneTabManager::Get();
    tab_mgr.Init("Untitled");
    EXPECT_EQ(tab_mgr.GetTabCount(), 1);

    int tab1 = tab_mgr.NewScene(reg());
    EXPECT_EQ(tab_mgr.GetTabCount(), 2);
    EXPECT_EQ(tab_mgr.GetActiveIndex(), 1);

    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "Tab1Ent");

    bool closed = tab_mgr.CloseTab(1, reg());
    EXPECT_TRUE(closed);
    EXPECT_EQ(tab_mgr.GetTabCount(), 1);
    EXPECT_EQ(tab_mgr.GetActiveIndex(), 0);

    bool reset = tab_mgr.CloseTab(0, reg());
    EXPECT_FALSE(reset);
    EXPECT_EQ(tab_mgr.GetTabCount(), 1);
    EXPECT_EQ(tab_mgr.GetActiveIndex(), 0);
    EXPECT_EQ(tab_mgr.GetActiveDisplayName(), "Untitled");

    tab_mgr.Init("Untitled");
}

// ============================================================
// Test 37: UndoRedo + SaveScene 非破坏性集成测试
// ============================================================

// 测试 编辑器功能：撤销重做保存场景非Destructive
TEST_F(EditorFunctionalTest, UndoRedo_SaveScene_NonDestructive) {
    Entity e = world.CreateEntity();
    auto& name_comp = reg().emplace<EditorNameComponent>(e, "OriginalName");

    dse::editor::UndoRedoManager undo_mgr;
    auto cmd = std::make_unique<dse::editor::PropertyChangeCommand<std::string>>(
        "Rename",
        std::string("OriginalName"), std::string("ModifiedName"),
        [&name_comp](const std::string& v) { name_comp.name = v; }
    );
    undo_mgr.Execute(std::move(cmd));
    EXPECT_EQ(name_comp.name, "ModifiedName");

    const auto path = TempPath("dse_test_undo_save.dscene");
    SaveScene(reg(), path.string());

    undo_mgr.Undo();
    EXPECT_EQ(name_comp.name, "OriginalName");

    entt::registry saved;
    LoadScene(saved, path.string());
    bool found_modified = false;
    for (auto en : saved.storage<entt::entity>()) {
        if (!saved.valid(en)) continue;
        if (!saved.all_of<EditorNameComponent>(en)) continue;
        if (saved.get<EditorNameComponent>(en).name == "ModifiedName") {
            found_modified = true;
        }
    }
    EXPECT_TRUE(found_modified);
    CleanupFile(path);
}

// ============================================================
// Test 41: SceneIO UICanvasScaler 往返
// ============================================================

// 测试 编辑器功能：场景IO UI画布缩放器往返
TEST_F(EditorFunctionalTest, SceneIO_UICanvasScalerRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "CanvasScalerEnt");
    auto& scaler = reg().emplace<UICanvasScalerComponent>(e);
    scaler.reference_resolution = glm::vec2(1280.0f, 720.0f);
    scaler.scale_factor = 1.5f;
    scaler.match_width_or_height = false;

    const auto path = TempPath("dse_test_uicanvas.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<UICanvasScalerComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<UICanvasScalerComponent>(en);
        EXPECT_NEAR(r.reference_resolution.x, 1280.0f, 0.01f);
        EXPECT_NEAR(r.reference_resolution.y, 720.0f, 0.01f);
        EXPECT_NEAR(r.scale_factor, 1.5f, 0.001f);
        EXPECT_FALSE(r.match_width_or_height);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 42: SceneIO UIAnimation 往返
// ============================================================

// 测试 编辑器功能：场景IO UI动画往返
TEST_F(EditorFunctionalTest, SceneIO_UIAnimationRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "UIAnimEnt");
    auto& anim = reg().emplace<UIAnimationComponent>(e);
    anim.duration = 0.8f;
    anim.target_alpha = 0.5f;
    anim.loop = true;
    anim.ping_pong = true;
    anim.animate_alpha = true;
    anim.easing = 2;
    anim.target_position = glm::vec2(100.0f, -50.0f);

    const auto path = TempPath("dse_test_uianim.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<UIAnimationComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<UIAnimationComponent>(en);
        EXPECT_NEAR(r.duration, 0.8f, 0.001f);
        EXPECT_NEAR(r.target_alpha, 0.5f, 0.001f);
        EXPECT_TRUE(r.loop);
        EXPECT_TRUE(r.ping_pong);
        EXPECT_TRUE(r.animate_alpha);
        EXPECT_EQ(r.easing, 2);
        EXPECT_NEAR(r.target_position.x, 100.0f, 0.01f);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 43: SceneIO Camera3D 往返
// ============================================================

// 测试 编辑器功能：场景IO相机3D往返
TEST_F(EditorFunctionalTest, SceneIO_Camera3DRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "CamEnt");
    auto& cam = reg().emplace<dse::Camera3DComponent>(e);
    cam.fov = 90.0f;
    cam.near_clip = 0.3f;
    cam.far_clip = 800.0f;
    cam.priority = 2;
    cam.enabled = false;

    const auto path = TempPath("dse_test_camera3d.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<dse::Camera3DComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<dse::Camera3DComponent>(en);
        EXPECT_NEAR(r.fov, 90.0f, 0.01f);
        EXPECT_NEAR(r.near_clip, 0.3f, 0.001f);
        EXPECT_NEAR(r.far_clip, 800.0f, 0.1f);
        EXPECT_EQ(r.priority, 2);
        EXPECT_FALSE(r.enabled);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 44: SceneTabManager SwitchTo 恢复 Registry
// ============================================================

// 测试 编辑器功能：场景标签页管理器切换到恢复注册表
TEST_F(EditorFunctionalTest, SceneTabManager_SwitchTo_RestoresRegistry) {
    auto& tab_mgr = dse::editor::SceneTabManager::Get();
    tab_mgr.Init("Tab0");

    Entity e0 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e0, "Tab0Entity");

    // NewScene snapshots tab0 (with Tab0Entity) before switching to empty tab1
    tab_mgr.NewScene(reg());
    EXPECT_EQ(tab_mgr.GetActiveIndex(), 1);

    Entity e1 = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e1, "Tab1Entity");

    // SwitchTo(0) snapshots tab1, then restores tab0's snapshot (Tab0Entity)
    tab_mgr.SwitchTo(0, reg());
    EXPECT_EQ(tab_mgr.GetActiveIndex(), 0);

    bool found_tab0 = false;
    for (auto en : reg().storage<entt::entity>()) {
        if (!reg().valid(en)) continue;
        if (!reg().all_of<EditorNameComponent>(en)) continue;
        if (reg().get<EditorNameComponent>(en).name == "Tab0Entity") {
            found_tab0 = true;
        }
    }
    EXPECT_TRUE(found_tab0);

    tab_mgr.Init("Untitled");
    dse::editor::SetCurrentScenePath("Untitled");
}

// ============================================================
// Test 45: SceneIO UIRenderer 往返
// ============================================================

// 测试 编辑器功能：场景IO UI渲染器往返
TEST_F(EditorFunctionalTest, SceneIO_UIRendererRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "UIRenEnt");
    auto& ui = reg().emplace<UIRendererComponent>(e);
    ui.texture_handle = 42;
    ui.color = glm::vec4(0.8f, 0.6f, 0.4f, 0.9f);
    ui.uv = glm::vec4(0.1f, 0.2f, 0.9f, 0.8f);
    ui.visible = false;

    const auto path = TempPath("dse_test_uirenderer.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<UIRendererComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<UIRendererComponent>(en);
        EXPECT_EQ(r.texture_handle, 42u);
        EXPECT_NEAR(r.color.r, 0.8f, 0.01f);
        EXPECT_NEAR(r.uv.x, 0.1f, 0.01f);
        EXPECT_FALSE(r.visible);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 46: SceneIO 复合多组件实体往返
// ============================================================

// 测试 编辑器功能：场景IO多组件实体往返
TEST_F(EditorFunctionalTest, SceneIO_MultiComponentEntityRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "ComplexEnt");
    reg().emplace<TransformComponent>(e).position = glm::vec3(5, 10, 15);
    auto& cam = reg().emplace<dse::Camera3DComponent>(e);
    cam.fov = 90.0f;
    auto& pl = reg().emplace<dse::PointLightComponent>(e);
    pl.intensity = 4.0f;
    pl.radius = 15.0f;
    auto& mr = reg().emplace<dse::MeshRendererComponent>(e);
    mr.mesh_path = "assets/complex.dmesh";
    mr.metallic = 0.7f;

    const auto path = TempPath("dse_test_multicomp.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<EditorNameComponent>(en)) continue;
        if (loaded.get<EditorNameComponent>(en).name != "ComplexEnt") continue;
        found = true;
        ASSERT_TRUE(loaded.all_of<TransformComponent>(en));
        ASSERT_TRUE(loaded.all_of<dse::Camera3DComponent>(en));
        ASSERT_TRUE(loaded.all_of<dse::PointLightComponent>(en));
        ASSERT_TRUE(loaded.all_of<dse::MeshRendererComponent>(en));
        EXPECT_NEAR(loaded.get<TransformComponent>(en).position.x, 5.0f, 0.01f);
        EXPECT_NEAR(loaded.get<dse::Camera3DComponent>(en).fov, 90.0f, 0.01f);
        EXPECT_NEAR(loaded.get<dse::PointLightComponent>(en).intensity, 4.0f, 0.01f);
        EXPECT_EQ(loaded.get<dse::MeshRendererComponent>(en).mesh_path, "assets/complex.dmesh");
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 47: SceneIO Terrain 往返
// ============================================================

// 测试 编辑器功能：场景IO地形往返
TEST_F(EditorFunctionalTest, SceneIO_TerrainRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "TerrainEnt");
    auto& t = reg().emplace<dse::TerrainComponent>(e);
    t.heightmap_path = "data/terrain/height.png";
    t.width = 256.0f;
    t.depth = 256.0f;
    t.max_height = 50.0f;
    t.resolution_x = 128;
    t.resolution_z = 128;
    t.use_dynamic_lod = true;
    t.enabled = true;

    const auto path = TempPath("dse_test_terrain.dscene");
    SaveScene(reg(), path.string());

    entt::registry loaded;
    LoadScene(loaded, path.string());
    ASSERT_EQ(dse::editor::test::CountAliveEntities(loaded), 1u);

    bool found = false;
    for (auto en : loaded.storage<entt::entity>()) {
        if (!loaded.valid(en)) continue;
        if (!loaded.all_of<dse::TerrainComponent>(en)) continue;
        found = true;
        const auto& r = loaded.get<dse::TerrainComponent>(en);
        EXPECT_EQ(r.heightmap_path, "data/terrain/height.png");
        EXPECT_NEAR(r.width, 256.0f, 0.01f);
        EXPECT_NEAR(r.depth, 256.0f, 0.01f);
        EXPECT_NEAR(r.max_height, 50.0f, 0.01f);
        EXPECT_EQ(r.resolution_x, 128);
        EXPECT_EQ(r.resolution_z, 128);
        EXPECT_TRUE(r.use_dynamic_lod);
    }
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 48: UndoRedo PropertyChangeCommand Execute 和 Undo
// ============================================================

// 测试 编辑器功能：撤销重做Property变更执行且撤销
TEST_F(EditorFunctionalTest, UndoRedo_PropertyChange_ExecuteAndUndo) {
    dse::editor::UndoRedoManager mgr;
    float value = 1.0f;

    mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<float>>(
        "SetValue", 1.0f, 5.0f,
        [&](const float& v) { value = v; }));

    EXPECT_NEAR(value, 5.0f, 0.001f);
    EXPECT_TRUE(mgr.CanUndo());
    EXPECT_FALSE(mgr.CanRedo());

    mgr.Undo();
    EXPECT_NEAR(value, 1.0f, 0.001f);
    EXPECT_FALSE(mgr.CanUndo());
    EXPECT_TRUE(mgr.CanRedo());

    mgr.Redo();
    EXPECT_NEAR(value, 5.0f, 0.001f);
}

// ============================================================
// Test 49: UndoRedo PropertyChangeCommand Merge
// ============================================================

// 测试 编辑器功能：撤销重做Property变更合并相同Description
TEST_F(EditorFunctionalTest, UndoRedo_PropertyChange_MergeSameDescription) {
    dse::editor::UndoRedoManager mgr;
    float value = 0.0f;

    auto setter = [&](const float& v) { value = v; };
    mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<float>>(
        "Drag", 0.0f, 1.0f, setter));
    mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<float>>(
        "Drag", 1.0f, 2.0f, setter), true);
    mgr.Execute(std::make_unique<dse::editor::PropertyChangeCommand<float>>(
        "Drag", 2.0f, 3.0f, setter), true);

    EXPECT_NEAR(value, 3.0f, 0.001f);
    EXPECT_EQ(mgr.GetUndoCount(), 1);

    mgr.Undo();
    EXPECT_NEAR(value, 0.0f, 0.001f);
}

// ============================================================
// Test 50: UndoRedo CompoundCommand 反序 Undo
// ============================================================

// 测试 编辑器功能：撤销重做复合命令Reverse撤销
TEST_F(EditorFunctionalTest, UndoRedo_CompoundCommand_ReverseUndo) {
    dse::editor::UndoRedoManager mgr;
    std::vector<int> log;

    auto compound = std::make_unique<dse::editor::CompoundCommand>("Batch");
    compound->AddCommand(std::make_unique<dse::editor::LambdaCommand>(
        "A", [&] { log.push_back(1); }, [&] { log.push_back(-1); }));
    compound->AddCommand(std::make_unique<dse::editor::LambdaCommand>(
        "B", [&] { log.push_back(2); }, [&] { log.push_back(-2); }));
    compound->AddCommand(std::make_unique<dse::editor::LambdaCommand>(
        "C", [&] { log.push_back(3); }, [&] { log.push_back(-3); }));

    mgr.Execute(std::move(compound));
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], 1);
    EXPECT_EQ(log[2], 3);

    log.clear();
    mgr.Undo();
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0], -3);
    EXPECT_EQ(log[1], -2);
    EXPECT_EQ(log[2], -1);
}

// ============================================================
// Test 51: UndoRedo MaxHistory 裁剪
// ============================================================

// 测试 编辑器功能：撤销重做最大历史修剪
TEST_F(EditorFunctionalTest, UndoRedo_MaxHistory_Trim) {
    dse::editor::UndoRedoManager mgr(5);
    int dummy = 0;

    for (int i = 0; i < 10; ++i) {
        mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
            "Cmd" + std::to_string(i),
            [&, i] { dummy = i; },
            [&, i] { dummy = i - 1; }));
    }

    EXPECT_EQ(mgr.GetUndoCount(), 5);
    EXPECT_EQ(mgr.GetUndoDescription(), "Cmd9");
}

// ============================================================
// Test 52: UndoRedo LambdaCommand MergeById
// ============================================================

// 测试 编辑器功能：撤销重做lambda命令合并按ID
TEST_F(EditorFunctionalTest, UndoRedo_LambdaCommand_MergeById) {
    dse::editor::UndoRedoManager mgr;
    float pos = 0.0f;

    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "MoveX", [&] { pos = 1.0f; }, [&] { pos = 0.0f; }, "drag_x"));
    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "MoveX", [&] { pos = 5.0f; }, [&] { pos = 1.0f; }, "drag_x"), true);

    EXPECT_NEAR(pos, 5.0f, 0.001f);
    EXPECT_EQ(mgr.GetUndoCount(), 1);

    mgr.Undo();
    EXPECT_NEAR(pos, 0.0f, 0.001f);
}

// ============================================================
// Test 53: InspectorRegistry Register 和排序
// ============================================================

// 测试 编辑器功能：检视器注册表排序顺序
TEST_F(EditorFunctionalTest, InspectorRegistry_SortOrder) {
    std::vector<dse::editor::InspectorEntry> entries = {
        {"CompC", "3D", nullptr, nullptr, nullptr, 300},
        {"CompA", "2D", nullptr, nullptr, nullptr, 100},
        {"CompB", "Physics", nullptr, nullptr, nullptr, 200},
    };
    std::sort(entries.begin(), entries.end(),
        [](const dse::editor::InspectorEntry& a, const dse::editor::InspectorEntry& b) {
            return a.sort_order < b.sort_order;
        });

    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].component_name, "CompA");
    EXPECT_EQ(entries[1].component_name, "CompB");
    EXPECT_EQ(entries[2].component_name, "CompC");
}

// ============================================================
// Test 54: InspectorRegistry Has 回调
// ============================================================

// 测试 编辑器功能：检视器注册表拥有且添加回调
TEST_F(EditorFunctionalTest, InspectorRegistry_HasAndAddCallback) {
    dse::editor::InspectorEntry entry{
        "TransformComponent", "Core", nullptr,
        [](entt::registry& r, entt::entity e) -> bool {
            return r.all_of<TransformComponent>(e);
        },
        [](entt::registry& r, entt::entity e) {
            if (!r.all_of<TransformComponent>(e))
                r.emplace<TransformComponent>(e);
        },
        10
    };

    Entity e = world.CreateEntity();
    EXPECT_FALSE(entry.has(reg(), e));

    entry.add(reg(), e);
    EXPECT_TRUE(entry.has(reg(), e));
}

// ============================================================
// Test 55: InspectorRegistry Category 分类
// ============================================================

// 测试 编辑器功能：检视器注册表Singleton Accessible
TEST_F(EditorFunctionalTest, InspectorRegistry_SingletonAccessible) {
    auto& registry = dse::editor::InspectorRegistry::Get();
    const auto& entries = registry.GetEntries();
    // 测试环境无 ImGui panel .cpp 链入，entries 可能为空但不崩溃
    (void)entries;
    SUCCEED();
}

// ============================================================
// Test 56: EditorSettings AddRecentFile 去重和裁剪
// ============================================================

// 测试 编辑器功能：编辑器设置添加Recent文件Dedup且修剪
TEST_F(EditorFunctionalTest, EditorSettings_AddRecentFile_DedupAndTrim) {
    dse::editor::EditorSettings settings;
    settings.max_recent_files = 3;

    dse::editor::AddRecentFile(settings, "a.dscene");
    dse::editor::AddRecentFile(settings, "b.dscene");
    dse::editor::AddRecentFile(settings, "c.dscene");
    dse::editor::AddRecentFile(settings, "a.dscene");

    EXPECT_EQ(settings.recent_files.size(), 3u);
    EXPECT_EQ(settings.recent_files[0], "a.dscene");

    dse::editor::AddRecentFile(settings, "d.dscene");
    EXPECT_EQ(settings.recent_files.size(), 3u);
    EXPECT_EQ(settings.recent_files[0], "d.dscene");
}

// ============================================================
// Test 57: EditorSettings 默认值
// ============================================================

// 测试 编辑器功能：编辑器设置默认值
TEST_F(EditorFunctionalTest, EditorSettings_DefaultValues) {
    dse::editor::EditorSettings settings;
    EXPECT_TRUE(settings.recent_files.empty());
    EXPECT_EQ(settings.default_gizmo_operation, 0);
    EXPECT_EQ(settings.default_gizmo_mode, 0);
    EXPECT_EQ(settings.max_recent_files, 10);
}

// ============================================================
// Test 58: EditorSettings Save 和 Load 往返
// ============================================================

// 测试 编辑器功能：编辑器设置保存加载往返
TEST_F(EditorFunctionalTest, EditorSettings_SaveLoadRoundTrip) {
    dse::editor::EditorSettings original;
    original.recent_files = {"scene1.dscene", "scene2.dscene"};
    original.last_scene_path = "scenes/main.dscene";
    original.default_gizmo_operation = 2;
    original.default_gizmo_mode = 1;

    dse::editor::SaveEditorSettings(original);
    auto loaded = dse::editor::LoadEditorSettings();

    EXPECT_EQ(loaded.recent_files.size(), 2u);
    if (!loaded.recent_files.empty())
        EXPECT_EQ(loaded.recent_files[0], "scene1.dscene");
    EXPECT_EQ(loaded.last_scene_path, "scenes/main.dscene");
    EXPECT_EQ(loaded.default_gizmo_operation, 2);
    EXPECT_EQ(loaded.default_gizmo_mode, 1);
}

// ============================================================
// Test 59: Snapshot ExportRegistrySnapshot 基本输出
// ============================================================

// 测试 编辑器功能：快照导出注册表快照基础输出
TEST_F(EditorFunctionalTest, Snapshot_ExportRegistrySnapshot_BasicOutput) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "SnapEntity");
    reg().emplace<TransformComponent>(e).position = glm::vec3(1, 2, 3);

    std::string json = dse::editor::test::ExportRegistrySnapshot(reg());
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("SnapEntity"), std::string::npos);
}

// ============================================================
// Test 60: Snapshot CompareSnapshot 检测差异
// ============================================================

// 测试 编辑器功能：快照比较快照Detects Difference
TEST_F(EditorFunctionalTest, Snapshot_CompareSnapshot_DetectsDifference) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "Original");
    reg().emplace<TransformComponent>(e).position = glm::vec3(0, 0, 0);

    std::string snap1 = dse::editor::test::ExportRegistrySnapshot(reg());

    reg().get<TransformComponent>(e).position = glm::vec3(99, 0, 0);
    std::string snap2 = dse::editor::test::ExportRegistrySnapshot(reg());

    auto diffs = dse::editor::test::CompareSnapshot(snap2, snap1);
    EXPECT_FALSE(diffs.empty());
}

// ============================================================
// Test 61: UndoRedo 新 Execute 清空 Redo 栈
// ============================================================

// 测试 编辑器功能：撤销重做执行清空重做栈
TEST_F(EditorFunctionalTest, UndoRedo_ExecuteClearsRedoStack) {
    dse::editor::UndoRedoManager mgr;
    int val = 0;

    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "A", [&] { val = 1; }, [&] { val = 0; }));
    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "B", [&] { val = 2; }, [&] { val = 1; }));

    mgr.Undo();
    EXPECT_TRUE(mgr.CanRedo());

    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "C", [&] { val = 3; }, [&] { val = 1; }));
    EXPECT_FALSE(mgr.CanRedo());
}

// ============================================================
// Test 62: UndoRedo Clear 后栈为空
// ============================================================

// 测试 编辑器功能：撤销重做清空Empties栈
TEST_F(EditorFunctionalTest, UndoRedo_ClearEmptiesStack) {
    dse::editor::UndoRedoManager mgr;
    int val = 0;

    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "X", [&] { val = 1; }, [&] { val = 0; }));
    mgr.Undo();
    EXPECT_TRUE(mgr.CanRedo());

    mgr.Clear();
    EXPECT_FALSE(mgr.CanUndo());
    EXPECT_FALSE(mgr.CanRedo());
    EXPECT_EQ(mgr.GetUndoCount(), 0);
    EXPECT_EQ(mgr.GetRedoCount(), 0);
}

// ============================================================
// Test 63: UndoRedo GetHistory 返回正确顺序
// ============================================================

// 测试 编辑器功能：撤销重做获取历史顺序
TEST_F(EditorFunctionalTest, UndoRedo_GetHistoryOrder) {
    dse::editor::UndoRedoManager mgr;
    int dummy = 0;

    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "First", [&] { dummy = 1; }, [&] { dummy = 0; }));
    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "Second", [&] { dummy = 2; }, [&] { dummy = 1; }));
    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "Third", [&] { dummy = 3; }, [&] { dummy = 2; }));

    auto history = mgr.GetUndoHistory();
    ASSERT_EQ(history.size(), 3u);
    EXPECT_EQ(history[0], "Third");
    EXPECT_EQ(history[2], "First");
}

// ============================================================
// Test 64: Prefab IsPrefabInstance 标记
// ============================================================

// 测试 编辑器功能：预制体为预制体实例之后Instantiate
TEST_F(EditorFunctionalTest, Prefab_IsPrefabInstance_AfterInstantiate) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "PrefabSrc");
    reg().emplace<TransformComponent>(e).position = glm::vec3(5, 0, 0);

    auto path = TempPath("dse_test_prefab_flag.dprefab");
    bool saved = dse::editor::SaveEntityAsPrefab(reg(), e, path.string());
    ASSERT_TRUE(saved);

    auto inst = dse::editor::InstantiatePrefab(world, reg(), path.string());
    EXPECT_TRUE(dse::editor::IsPrefabInstance(reg(), inst));
    EXPECT_FALSE(dse::editor::IsPrefabInstance(reg(), e));

    CleanupFile(path);
}

// ============================================================
// Test 65: SceneIO SiblingIndex 往返
// ============================================================

// 测试 编辑器功能：场景IO Sibling索引往返
TEST_F(EditorFunctionalTest, SceneIO_SiblingIndexRoundTrip) {
    Entity e = world.CreateEntity();
    reg().emplace<EditorNameComponent>(e, "SiblingEnt");
    reg().emplace<TransformComponent>(e);
    reg().emplace<SiblingIndexComponent>(e).index = 42;

    auto path = TempPath("dse_test_sibling.dscene");
    SaveScene(reg(), path.string());

    World w2;
    LoadScene(w2.registry(), path.string());

    bool found = false;
    w2.registry().view<SiblingIndexComponent>().each(
        [&](auto, const SiblingIndexComponent& r) {
            found = true;
            EXPECT_EQ(r.index, 42);
        });
    EXPECT_TRUE(found);
    CleanupFile(path);
}

// ============================================================
// Test 66: EditorSettings AddRecentFile 忽略 Untitled 和空路径
// ============================================================

// 测试 编辑器功能：编辑器设置添加Recent文件忽略Untitled且空
TEST_F(EditorFunctionalTest, EditorSettings_AddRecentFile_IgnoresUntitledAndEmpty) {
    dse::editor::EditorSettings settings;
    dse::editor::AddRecentFile(settings, "valid.dscene");
    dse::editor::AddRecentFile(settings, "Untitled");
    dse::editor::AddRecentFile(settings, "");

    EXPECT_EQ(settings.recent_files.size(), 1u);
    EXPECT_EQ(settings.recent_files[0], "valid.dscene");
}

// ============================================================
// Test 67: ChatProtocol ParseBridgeMessage - assistant_message
// ============================================================

// 测试 编辑器功能：聊天协议解析Assistant消息
TEST_F(EditorFunctionalTest, ChatProtocol_Parse_AssistantMessage) {
    auto msg = dse::editor::ParseBridgeMessage(
        R"({"type":"assistant_message","content":"Hello world"})");
    EXPECT_TRUE(msg.valid);
    EXPECT_EQ(msg.type, dse::editor::BridgeMessageType::AssistantMessage);
    EXPECT_EQ(msg.content, "Hello world");
}

// ============================================================
// Test 68: ChatProtocol ParseBridgeMessage - tool_call
// ============================================================

// 测试 编辑器功能：聊天协议解析Tool调用
TEST_F(EditorFunctionalTest, ChatProtocol_Parse_ToolCall) {
    auto msg = dse::editor::ParseBridgeMessage(
        R"({"type":"tool_call","name":"dsengine_entity_create","arguments":"{\"name\":\"Box\"}","call_id":"abc123"})");
    EXPECT_TRUE(msg.valid);
    EXPECT_EQ(msg.type, dse::editor::BridgeMessageType::ToolCall);
    EXPECT_EQ(msg.tool_name, "dsengine_entity_create");
    EXPECT_EQ(msg.call_id, "abc123");
    EXPECT_FALSE(msg.tool_args.empty());
}

// ============================================================
// Test 69: ChatProtocol ParseBridgeMessage - error
// ============================================================

// 测试 编辑器功能：聊天协议解析错误
TEST_F(EditorFunctionalTest, ChatProtocol_Parse_Error) {
    auto msg = dse::editor::ParseBridgeMessage(
        R"({"type":"error","message":"API key missing"})");
    EXPECT_TRUE(msg.valid);
    EXPECT_EQ(msg.type, dse::editor::BridgeMessageType::Error);
    EXPECT_EQ(msg.content, "API key missing");
}

// ============================================================
// Test 70: ChatProtocol ParseBridgeMessage - status
// ============================================================

// 测试 编辑器功能：聊天协议解析状态
TEST_F(EditorFunctionalTest, ChatProtocol_Parse_Status) {
    auto msg = dse::editor::ParseBridgeMessage(
        R"({"type":"status","message":"Connected"})");
    EXPECT_TRUE(msg.valid);
    EXPECT_EQ(msg.type, dse::editor::BridgeMessageType::Status);
    EXPECT_EQ(msg.content, "Connected");
}

// ============================================================
// Test 71: ChatProtocol ParseBridgeMessage - invalid JSON
// ============================================================

// 测试 编辑器功能：聊天协议解析无效JSON
TEST_F(EditorFunctionalTest, ChatProtocol_Parse_InvalidJSON) {
    auto msg = dse::editor::ParseBridgeMessage("not json at all");
    EXPECT_FALSE(msg.valid);
    EXPECT_EQ(msg.type, dse::editor::BridgeMessageType::Unknown);
    EXPECT_EQ(msg.content, "not json at all");
}

// ============================================================
// Test 72: ChatProtocol ParseBridgeMessage - unknown type
// ============================================================

// 测试 编辑器功能：聊天协议解析未知类型
TEST_F(EditorFunctionalTest, ChatProtocol_Parse_UnknownType) {
    auto msg = dse::editor::ParseBridgeMessage(
        R"({"type":"custom_event","data":123})");
    EXPECT_TRUE(msg.valid);
    EXPECT_EQ(msg.type, dse::editor::BridgeMessageType::Unknown);
}

// ============================================================
// Test 73: ChatProtocol BuildUserMessage 格式正确
// ============================================================

// 测试 编辑器功能：聊天协议构建User消息
TEST_F(EditorFunctionalTest, ChatProtocol_BuildUserMessage) {
    std::string line = dse::editor::BuildUserMessage("Create a cube");
    EXPECT_TRUE(line.back() == '\n');

    rapidjson::Document doc;
    doc.Parse(line.c_str());
    ASSERT_FALSE(doc.HasParseError());
    EXPECT_STREQ(doc["type"].GetString(), "user_message");
    EXPECT_STREQ(doc["content"].GetString(), "Create a cube");
}

// ============================================================
// Test 74: ChatProtocol BuildToolResult 格式正确
// ============================================================

// 测试 编辑器功能：聊天协议构建Tool结果
TEST_F(EditorFunctionalTest, ChatProtocol_BuildToolResult) {
    std::string line = dse::editor::BuildToolResult("call_42", R"({"ok":true})");
    EXPECT_TRUE(line.back() == '\n');

    rapidjson::Document doc;
    doc.Parse(line.c_str());
    ASSERT_FALSE(doc.HasParseError());
    EXPECT_STREQ(doc["type"].GetString(), "tool_result");
    EXPECT_STREQ(doc["call_id"].GetString(), "call_42");
    EXPECT_STREQ(doc["result"].GetString(), R"({"ok":true})");
}

// ============================================================
// Test 75: ChatProtocol ParseBridgeMessage - 空 content 字段
// ============================================================

// 测试 编辑器功能：聊天协议解析空内容
TEST_F(EditorFunctionalTest, ChatProtocol_Parse_EmptyContent) {
    auto msg = dse::editor::ParseBridgeMessage(
        R"({"type":"assistant_message","content":""})");
    EXPECT_TRUE(msg.valid);
    EXPECT_EQ(msg.type, dse::editor::BridgeMessageType::AssistantMessage);
    EXPECT_TRUE(msg.content.empty());
}
