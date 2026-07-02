/**
 * @file editor_command_undo_redo_test.cpp
 * @brief P2: 编辑器 CommandBus 各命令的 execute/undo 粒度验证
 *
 * 针对 editor_command.h 中每个命令类型，验证通过 CommandBus dispatch 后：
 * - 状态正确变更（写后读一致）
 * - Undo 后状态回退到执行前
 * - Redo 后状态恢复到执行后
 *
 * 使用与 editor_core_command_bus_test.cpp 相同的 fixture（同进程直调 ToolSink）。
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <rapidjson/document.h>

#include "apps/editor_cpp/src/editor_control_server.h"
#include "apps/editor_cpp/src/editor_shortcuts.h"
#include "apps/editor_cpp/core/command_bus.h"
#include "apps/editor_cpp/core/query_service.h"
#include "apps/editor_cpp/src/editor_undo.h"

#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"

namespace dse::editor { void ResetEditorStateStub(); }

namespace core = dse::editor::core;

class EditorCommandUndoRedoTest : public ::testing::Test {
protected:
    World world_;
    dse::runtime::EngineRunConfig config_;
    std::unique_ptr<dse::runtime::EngineInstance> engine_;
    dse::editor::ControlServer server_;

    void SetUp() override {
        dse::editor::GetUndoRedoManager().Clear();
        engine_ = std::make_unique<dse::runtime::EngineInstance>(config_);
        engine_->pipeline()->SetWorld(&world_);
        dse::editor::RegisterBuiltinTools(server_);
        dse::editor::ResetEditorStateStub();
    }

    void TearDown() override {
        dse::editor::GetUndoRedoManager().Clear();
        dse::editor::ResetEditorStateStub();
        engine_->Shutdown();
    }

    core::ToolSink sink() {
        return [this](const std::string& method, const rapidjson::Document& params,
                      dse::runtime::EngineInstance& engine) {
            return server_.DispatchTool(method, params, engine);
        };
    }

    std::uint32_t CreateEntity(core::CommandBus& bus, const std::string& name) {
        auto r = bus.dispatch(core::CreateEntityCmd{name}, *engine_);
        EXPECT_TRUE(r.ok) << r.error_message;
        return r.data["entity_id"].GetUint();
    }
};

// ─── CreateEntity + Undo + Redo ────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, CreateEntityUndoRedo) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    const int baseline = query.sceneSummary(*engine_).entity_count;

    auto r = bus.dispatch(core::CreateEntityCmd{"UndoMe"}, *engine_);
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, baseline + 1);

    // Undo
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, baseline);

    // Redo
    ASSERT_TRUE(bus.dispatch(core::RedoCmd{}, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, baseline + 1);
}

// ─── DeleteEntity + Undo + Redo ────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, DeleteEntityUndoRedo) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto id = CreateEntity(bus, "Victim");
    const int after_create = query.sceneSummary(*engine_).entity_count;

    core::DeleteEntityCmd del;
    del.entity_id = id;
    ASSERT_TRUE(bus.dispatch(del, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, after_create - 1);

    // Undo restores entity
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, after_create);

    // Redo deletes again
    ASSERT_TRUE(bus.dispatch(core::RedoCmd{}, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, after_create - 1);
}

// ─── RenameEntity + Undo ───────────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, RenameEntityUndoRestoresName) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto id = CreateEntity(bus, "OrigName");

    core::RenameEntityCmd rn;
    rn.entity_id = id;
    rn.name = "Changed";
    ASSERT_TRUE(bus.dispatch(rn, *engine_).ok);

    auto vm = query.inspector(id, *engine_);
    ASSERT_TRUE(vm.valid);
    EXPECT_EQ(vm.name, "Changed");

    // Undo
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    vm = query.inspector(id, *engine_);
    ASSERT_TRUE(vm.valid);
    EXPECT_EQ(vm.name, "OrigName");
}

// ─── Reparent + Undo ──────────────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, ReparentUndoRestoresHierarchy) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto parent_id = CreateEntity(bus, "Parent");
    auto child_id = CreateEntity(bus, "Child");

    // Reparent child under parent
    core::ReparentEntityCmd rp;
    rp.entity_id = child_id;
    rp.parent = parent_id;
    ASSERT_TRUE(bus.dispatch(rp, *engine_).ok);

    auto h = query.hierarchy(*engine_);
    ASSERT_TRUE(h.valid);
    bool child_has_parent = false;
    for (const auto& n : h.nodes) {
        if (n.entity_id == child_id) {
            EXPECT_TRUE(n.has_parent);
            EXPECT_EQ(n.parent_id, parent_id);
            child_has_parent = true;
        }
    }
    EXPECT_TRUE(child_has_parent);

    // Undo
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    h = query.hierarchy(*engine_);
    ASSERT_TRUE(h.valid);
    for (const auto& n : h.nodes) {
        if (n.entity_id == child_id) {
            EXPECT_FALSE(n.has_parent);
        }
    }
}

// ─── Duplicate + Undo ─────────────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, DuplicateUndoRemovesDuplicate) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto original = CreateEntity(bus, "Original");
    const int before_dup = query.sceneSummary(*engine_).entity_count;

    core::DuplicateEntityCmd dup;
    dup.entity_id = original;
    auto dr = bus.dispatch(dup, *engine_);
    ASSERT_TRUE(dr.ok) << dr.error_message;
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, before_dup + 1);

    // Undo
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, before_dup);
}

// ─── BatchDelete + Undo ──────────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, BatchDeleteUndoRestoresAll) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto a = CreateEntity(bus, "A");
    auto b = CreateEntity(bus, "B");
    auto c = CreateEntity(bus, "C");
    const int before = query.sceneSummary(*engine_).entity_count;

    core::BatchDeleteEntitiesCmd bd;
    bd.entity_ids = {a, b, c};
    ASSERT_TRUE(bus.dispatch(bd, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, before - 3);

    // Undo
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, before);
}

// ─── 多步 Undo/Redo 链 ─────────────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, MultiStepUndoRedoChainConsistent) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    const int baseline = query.sceneSummary(*engine_).entity_count;

    CreateEntity(bus, "Step1");
    CreateEntity(bus, "Step2");
    CreateEntity(bus, "Step3");
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, baseline + 3);

    // Undo 三次
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, baseline);

    // Redo 两次
    ASSERT_TRUE(bus.dispatch(core::RedoCmd{}, *engine_).ok);
    ASSERT_TRUE(bus.dispatch(core::RedoCmd{}, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, baseline + 2);

    // 新操作清空 redo 栈
    CreateEntity(bus, "Step4");
    auto uh = query.undoHistory(*engine_);
    EXPECT_FALSE(uh.can_redo);
}

// ─── Transform Execute 验证 ──────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, TransformEntityPositionExecute) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto id = CreateEntity(bus, "Movable");

    core::TransformEntityCmd tf;
    tf.entity_id = id;
    tf.position = core::Vec3{{10.0f, 20.0f, 30.0f}};
    ASSERT_TRUE(bus.dispatch(tf, *engine_).ok);

    auto vm = query.inspector(id, *engine_);
    ASSERT_TRUE(vm.valid);
    EXPECT_FLOAT_EQ(vm.transform.position[0], 10.0f);
    EXPECT_FLOAT_EQ(vm.transform.position[1], 20.0f);
    EXPECT_FLOAT_EQ(vm.transform.position[2], 30.0f);
}

TEST_F(EditorCommandUndoRedoTest, TransformEntityScaleExecute) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto id = CreateEntity(bus, "Scalable");

    core::TransformEntityCmd tf;
    tf.entity_id = id;
    tf.scale = core::Vec3{{3.0f, 3.0f, 3.0f}};
    ASSERT_TRUE(bus.dispatch(tf, *engine_).ok);

    auto vm = query.inspector(id, *engine_);
    ASSERT_TRUE(vm.valid);
    EXPECT_FLOAT_EQ(vm.transform.scale[0], 3.0f);
    EXPECT_FLOAT_EQ(vm.transform.scale[1], 3.0f);
    EXPECT_FLOAT_EQ(vm.transform.scale[2], 3.0f);
}

// ─── AddComponent Execute 验证 ──────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, AddComponentExecuteAddsComponent) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto id = CreateEntity(bus, "CompHost");
    const auto base_count = query.inspector(id, *engine_).components.size();

    core::AddComponentCmd add;
    add.entity_id = id;
    add.type = "MeshRenderer";
    ASSERT_TRUE(bus.dispatch(add, *engine_).ok);
    EXPECT_GT(query.inspector(id, *engine_).components.size(), base_count);
}

// ─── Selection Execute 验证 ─────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, SelectionSetAndClear) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto e1 = CreateEntity(bus, "E1");
    auto e2 = CreateEntity(bus, "E2");

    // Select e1
    core::SetSelectionCmd sel1;
    sel1.entity_ids = {e1};
    ASSERT_TRUE(bus.dispatch(sel1, *engine_).ok);
    auto s = query.selection(*engine_);
    EXPECT_EQ(s.count, 1);

    // Select e2
    core::SetSelectionCmd sel2;
    sel2.entity_ids = {e2};
    ASSERT_TRUE(bus.dispatch(sel2, *engine_).ok);
    s = query.selection(*engine_);
    EXPECT_EQ(s.count, 1);

    // Clear
    ASSERT_TRUE(bus.dispatch(core::ClearSelectionCmd{}, *engine_).ok);
    EXPECT_EQ(query.selection(*engine_).count, 0);
}

// ─── UndoHistory 查询验证 ──────────────────────────────────────────────

TEST_F(EditorCommandUndoRedoTest, UndoHistoryReflectsState) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto uh = query.undoHistory(*engine_);
    EXPECT_FALSE(uh.can_undo);
    EXPECT_FALSE(uh.can_redo);

    CreateEntity(bus, "Tracked");
    uh = query.undoHistory(*engine_);
    EXPECT_TRUE(uh.can_undo);
    EXPECT_GT(uh.undo_count, 0);

    // Undo
    ASSERT_TRUE(bus.dispatch(core::UndoCmd{}, *engine_).ok);
    uh = query.undoHistory(*engine_);
    EXPECT_TRUE(uh.can_redo);
}
