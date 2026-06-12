/**
 * @file editor_unit_test.cpp
 * @brief 编辑器核心逻辑单元测试（不依赖 ImGui/GLFW/EnTT）
 *
 * 覆盖：
 * - UndoRedoManager: Execute/Undo/Redo/合并/历史/最大栈深
 * - PropertyChangeCommand / LambdaCommand / CompoundCommand
 * - EditorTestConfig: CLI 解析
 * - EditorSettings: 默认值
 *
 * 注：SelectionManager 测试在 integration/editor/ 中（需链接 entt）
 */

#include <gtest/gtest.h>
#include "apps/editor_cpp/src/editor_undo.h"
#include "apps/editor_cpp/src/editor_test_harness.h"
#include "apps/editor_cpp/src/editor_settings.h"

using namespace dse::editor;

// ============================================================
// UndoRedoManager + Commands
// ============================================================

// 测试 撤销重做管理器：状态
TEST(UndoRedoManagerTest, State) {
    UndoRedoManager mgr;
    EXPECT_FALSE(mgr.CanUndo());
    EXPECT_FALSE(mgr.CanRedo());
    EXPECT_EQ(mgr.GetUndoCount(), 0);
    EXPECT_EQ(mgr.GetRedoCount(), 0);
    EXPECT_EQ(mgr.GetUndoDescription(), "");
    EXPECT_EQ(mgr.GetRedoDescription(), "");
}

// 测试 撤销重做管理器：执行撤销重做
TEST(UndoRedoManagerTest, Execute_Undo_Redo) {
    UndoRedoManager mgr;
    int value = 0;
    auto cmd = std::make_unique<PropertyChangeCommand<int>>(
        "set value", 0, 42,
        [&value](const int& v) { value = v; });

    mgr.Execute(std::move(cmd));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(mgr.CanUndo());
    EXPECT_FALSE(mgr.CanRedo());

    mgr.Undo();
    EXPECT_EQ(value, 0);
    EXPECT_FALSE(mgr.CanUndo());
    EXPECT_TRUE(mgr.CanRedo());

    mgr.Redo();
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(mgr.CanUndo());
    EXPECT_FALSE(mgr.CanRedo());
}

// 测试 撤销重做管理器：Executeclear之后重做
TEST(UndoRedoManagerTest, ExecuteclearAfterRedo) {
    UndoRedoManager mgr;
    int value = 0;
    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "cmd1", 0, 1, [&](const int& v) { value = v; }));
    mgr.Undo();
    EXPECT_TRUE(mgr.CanRedo());

    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "cmd2", 0, 2, [&](const int& v) { value = v; }));
    EXPECT_FALSE(mgr.CanRedo()); // redo 被清空
    EXPECT_EQ(value, 2);
}

// 测试 撤销重做管理器：最大
TEST(UndoRedoManagerTest, Max) {
    UndoRedoManager mgr(3);
    int value = 0;
    for (int i = 1; i <= 5; ++i) {
        mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
            "cmd" + std::to_string(i), 0, i, [&](const int& v) { value = v; }));
    }
    EXPECT_EQ(mgr.GetUndoCount(), 3); // max_history=3, 只保留最近 3 个
}

// 测试 撤销重做管理器：Mergemerge
TEST(UndoRedoManagerTest, Mergemerge) {
    UndoRedoManager mgr;
    int value = 0;
    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "drag", 0, 10, [&](const int& v) { value = v; }));
    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "drag", 10, 20, [&](const int& v) { value = v; }), true);

    EXPECT_EQ(value, 20);
    EXPECT_EQ(mgr.GetUndoCount(), 1); // 合并后只有 1 条
    mgr.Undo();
    EXPECT_EQ(value, 0); // 撤销到初始值
}

// 测试 撤销重做管理器：获取历史
TEST(UndoRedoManagerTest, GetHistory) {
    UndoRedoManager mgr;
    int v = 0;
    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "A", 0, 1, [&](const int& x) { v = x; }));
    mgr.Execute(std::make_unique<PropertyChangeCommand<int>>(
        "B", 1, 2, [&](const int& x) { v = x; }));

    auto undo_hist = mgr.GetUndoHistory();
    EXPECT_EQ(undo_hist.size(), 2u);
    EXPECT_EQ(undo_hist[0], "B"); // 最新在前
    EXPECT_EQ(undo_hist[1], "A");

    mgr.Undo();
    auto redo_hist = mgr.GetRedoHistory();
    EXPECT_EQ(redo_hist.size(), 1u);
    EXPECT_EQ(redo_hist[0], "B");
}

// 测试 lambda命令：执行撤销
TEST(LambdaCommandTest, Execute_Undo) {
    int counter = 0;
    LambdaCommand cmd("inc", [&]() { counter++; }, [&]() { counter--; });
    cmd.Execute();
    EXPECT_EQ(counter, 1);
    cmd.Undo();
    EXPECT_EQ(counter, 0);
    EXPECT_EQ(cmd.GetDescription(), "inc");
}

// 测试 lambda命令：合并ID
TEST(LambdaCommandTest, MergeId) {
    int v = 0;
    LambdaCommand cmd1("drag", [&]() { v = 10; }, [&]() { v = 0; }, "drag_x");
    LambdaCommand cmd2("drag", [&]() { v = 20; }, [&]() { v = 10; }, "drag_x");
    EXPECT_TRUE(cmd1.MergeWith(&cmd2));
    cmd1.Execute();
    EXPECT_EQ(v, 20); // 合并后 execute 走最新
    cmd1.Undo();
    EXPECT_EQ(v, 0);  // undo 走最早
}

// 测试 复合命令：情形9
TEST(CompoundCommandTest, TestCase9) {
    int a = 0, b = 0;
    auto compound = std::make_unique<CompoundCommand>("batch");
    compound->AddCommand(std::make_unique<LambdaCommand>(
        "a", [&]() { a = 1; }, [&]() { a = 0; }));
    compound->AddCommand(std::make_unique<LambdaCommand>(
        "b", [&]() { b = 2; }, [&]() { b = 0; }));

    EXPECT_FALSE(compound->IsEmpty());
    compound->Execute();
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);

    compound->Undo(); // 逆序撤销
    EXPECT_EQ(a, 0);
    EXPECT_EQ(b, 0);
}

// 测试 撤销重做管理器：清空
TEST(UndoRedoManagerTest, Clear) {
    UndoRedoManager mgr;
    int v = 0;
    mgr.Execute(std::make_unique<LambdaCommand>(
        "x", [&]() { v = 1; }, [&]() { v = 0; }));
    mgr.Clear();
    EXPECT_FALSE(mgr.CanUndo());
    EXPECT_FALSE(mgr.CanRedo());
    EXPECT_EQ(mgr.GetUndoCount(), 0);
}

// ============================================================
// EditorTestConfig CLI 解析
// ============================================================

// 测试 编辑器配置：默认值
TEST(EditorTestConfigTest, DefaultValues) {
    test::EditorTestConfig config;
    EXPECT_FALSE(config.headless);
    EXPECT_TRUE(config.replay_path.empty());
    EXPECT_TRUE(config.verify_path.empty());
    EXPECT_TRUE(config.scene_path.empty());
    EXPECT_EQ(config.max_frames, 300);
}

// 测试 编辑器配置：Headless
TEST(EditorTestConfigTest, Headless) {
    char arg0[] = "editor";
    char arg1[] = "--headless";
    char* argv[] = {arg0, arg1};
    auto config = test::ParseEditorTestArgs(2, argv);
    EXPECT_TRUE(config.headless);
    EXPECT_TRUE(test::HasTestArgs(config));
}

// 测试 编辑器配置：Replay verify
TEST(EditorTestConfigTest, Replay_verify) {
    char arg0[] = "editor";
    char arg1[] = "--replay=test.json";
    char arg2[] = "--verify=expected.json";
    char arg3[] = "--scene=level1.dscene";
    char arg4[] = "--max-frames=100";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4};
    auto config = test::ParseEditorTestArgs(5, argv);
    EXPECT_EQ(config.replay_path, "test.json");
    EXPECT_EQ(config.verify_path, "expected.json");
    EXPECT_EQ(config.scene_path, "level1.dscene");
    EXPECT_EQ(config.max_frames, 100);
    EXPECT_TRUE(test::HasTestArgs(config));
}

// 测试 编辑器配置：无参数当拥有Args为false
TEST(EditorTestConfigTest, WithoutParametersWhenHasTestArgsIsfalse) {
    char arg0[] = "editor";
    char* argv[] = {arg0};
    auto config = test::ParseEditorTestArgs(1, argv);
    EXPECT_FALSE(test::HasTestArgs(config));
}

// 测试 编辑器配置：最大帧负数值回退返回到300
TEST(EditorTestConfigTest, max_FramesNegativeNumbersFallBackTo300) {
    char arg0[] = "editor";
    char arg1[] = "--max-frames=-5";
    char* argv[] = {arg0, arg1};
    auto config = test::ParseEditorTestArgs(2, argv);
    EXPECT_EQ(config.max_frames, 300);
}

// ============================================================
// EditorSettings 默认值
// ============================================================

// 测试 编辑器设置：默认值
TEST(EditorSettingsTest, DefaultValues) {
    EditorSettings settings;
    EXPECT_TRUE(settings.recent_files.empty());
    EXPECT_TRUE(settings.last_scene_path.empty());
    EXPECT_EQ(settings.default_gizmo_operation, 0);
    EXPECT_EQ(settings.default_gizmo_mode, 0);
    EXPECT_EQ(settings.max_recent_files, 10);
}
