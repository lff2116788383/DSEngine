/**
 * @file editor_core_command_bus_test.cpp
 * @brief EditorCore 门面层（CommandBus + QueryService）无头集成测试
 *
 * 验证目标（Phase 0）：
 * - CommandBus 把类型化命令翻译为 dsengine_* 工具调用，确实复用现有逻辑（不重复实现）。
 * - QueryService 把只读工具结果映射为类型化 ViewModel。
 * - 写路径经 CommandBus 后，读路径（QueryService）能观察到一致的状态变化。
 *
 * 策略与 ControlServerTest 一致：注入 World 到 FramePipeline，不启 WebSocket、不依赖 GPU；
 * ToolSink 绑定到 ControlServer::DispatchTool（同进程直调）。
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>

#include <rapidjson/document.h>

#include "apps/editor_cpp/src/editor_control_server.h"
#include "apps/editor_cpp/core/command_bus.h"
#include "apps/editor_cpp/core/query_service.h"

#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"

namespace dse::editor { void ResetEditorStateStub(); }

namespace core = dse::editor::core;

class EditorCoreTest : public ::testing::Test {
protected:
    World world_;
    dse::runtime::EngineRunConfig config_;
    std::unique_ptr<dse::runtime::EngineInstance> engine_;
    dse::editor::ControlServer server_;

    void SetUp() override {
        engine_ = std::make_unique<dse::runtime::EngineInstance>(config_);
        engine_->pipeline()->SetWorld(&world_);
        dse::editor::RegisterBuiltinTools(server_);
        dse::editor::ResetEditorStateStub();
    }

    void TearDown() override {
        dse::editor::ResetEditorStateStub();
        engine_->Shutdown();
    }

    core::ToolSink sink() {
        return [this](const std::string& method, const rapidjson::Document& params,
                      dse::runtime::EngineInstance& engine) {
            return server_.DispatchTool(method, params, engine);
        };
    }
};

// ─── CommandBus：无 sink 时返回内部错误，不崩溃 ──────────────────────────────

TEST_F(EditorCoreTest, DispatchWithoutSinkReturnsError) {
    core::CommandBus bus{core::ToolSink{}};
    EXPECT_FALSE(bus.has_sink());

    auto r = bus.dispatch(core::CreateEntityCmd{"X"}, *engine_);
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.error_code, -32603);
}

// ─── CommandBus：CreateEntity 复用 dsengine_entity_create 并使存活数 +1 ───────

TEST_F(EditorCoreTest, CreateEntityIncrementsCountAndReturnsId) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto before = query.sceneSummary(*engine_);
    ASSERT_TRUE(before.valid);
    const int baseline = before.entity_count;

    auto r = bus.dispatch(core::CreateEntityCmd{"FromBus"}, *engine_);
    ASSERT_TRUE(r.ok) << r.error_message;
    ASSERT_TRUE(r.data.HasMember("entity_id"));
    ASSERT_TRUE(r.data["entity_id"].IsUint());

    auto after = query.sceneSummary(*engine_);
    ASSERT_TRUE(after.valid);
    EXPECT_EQ(after.entity_count, baseline + 1);
    EXPECT_EQ(after.editor_state, "edit");
}

// ─── QueryService：entityComponents 映射新建实体的组件清单 ────────────────────

TEST_F(EditorCoreTest, EntityComponentsReflectsCreatedEntity) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto r = bus.dispatch(core::CreateEntityCmd{"Inspectee"}, *engine_);
    ASSERT_TRUE(r.ok) << r.error_message;
    const std::uint32_t id = r.data["entity_id"].GetUint();

    auto vm = query.entityComponents(id, *engine_);
    ASSERT_TRUE(vm.valid);
    EXPECT_EQ(vm.entity_id, id);
    // 新建实体至少带 Transform（见 HandleEntityCreate）。
    EXPECT_NE(std::find(vm.components.begin(), vm.components.end(), "Transform"),
              vm.components.end());
}

// ─── CommandBus：Delete 使存活数回落 ─────────────────────────────────────────

TEST_F(EditorCoreTest, DeleteEntityDecrementsCount) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto created = bus.dispatch(core::CreateEntityCmd{"Doomed"}, *engine_);
    ASSERT_TRUE(created.ok) << created.error_message;
    const std::uint32_t id = created.data["entity_id"].GetUint();
    const int after_create = query.sceneSummary(*engine_).entity_count;

    core::DeleteEntityCmd del;
    del.entity_id = id;
    auto r = bus.dispatch(del, *engine_);
    ASSERT_TRUE(r.ok) << r.error_message;

    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, after_create - 1);
}

// ─── CommandBus：Rename 经 entity_modify 生效（不报错即复用成功） ─────────────

TEST_F(EditorCoreTest, RenameEntitySucceeds) {
    core::CommandBus bus{sink()};

    auto created = bus.dispatch(core::CreateEntityCmd{"OldName"}, *engine_);
    ASSERT_TRUE(created.ok) << created.error_message;
    const std::uint32_t id = created.data["entity_id"].GetUint();

    core::RenameEntityCmd rn;
    rn.entity_id = id;
    rn.name = "NewName";
    auto r = bus.dispatch(rn, *engine_);
    EXPECT_TRUE(r.ok) << r.error_message;
}

// ─── CommandBus：Transform 经 entity_modify 生效，Inspector 读回一致 ──────────

TEST_F(EditorCoreTest, TransformEntityReflectedInInspector) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto created = bus.dispatch(core::CreateEntityCmd{"Movable"}, *engine_);
    ASSERT_TRUE(created.ok) << created.error_message;
    const std::uint32_t id = created.data["entity_id"].GetUint();

    core::TransformEntityCmd tf;
    tf.entity_id = id;
    tf.position = core::Vec3{{1.0f, 2.0f, 3.0f}};
    tf.scale = core::Vec3{{2.0f, 2.0f, 2.0f}};
    ASSERT_TRUE(bus.dispatch(tf, *engine_).ok);

    auto vm = query.inspector(id, *engine_);
    ASSERT_TRUE(vm.valid);
    ASSERT_TRUE(vm.transform.present);
    EXPECT_FLOAT_EQ(vm.transform.position[0], 1.0f);
    EXPECT_FLOAT_EQ(vm.transform.position[1], 2.0f);
    EXPECT_FLOAT_EQ(vm.transform.position[2], 3.0f);
    EXPECT_FLOAT_EQ(vm.transform.scale[0], 2.0f);
}

// ─── CommandBus：AddComponent / RemoveComponent 经工具改变组件清单 ────────────

TEST_F(EditorCoreTest, AddRemoveComponentChangesInspector) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto created = bus.dispatch(core::CreateEntityCmd{"Compy"}, *engine_);
    ASSERT_TRUE(created.ok) << created.error_message;
    const std::uint32_t id = created.data["entity_id"].GetUint();

    const std::size_t base = query.inspector(id, *engine_).components.size();

    core::AddComponentCmd add;
    add.entity_id = id;
    add.type = "MeshRenderer";
    ASSERT_TRUE(bus.dispatch(add, *engine_).ok);
    EXPECT_GT(query.inspector(id, *engine_).components.size(), base);

    core::RemoveComponentCmd rem;
    rem.entity_id = id;
    rem.type = "MeshRenderer";
    ASSERT_TRUE(bus.dispatch(rem, *engine_).ok);
    EXPECT_EQ(query.inspector(id, *engine_).components.size(), base);
}

// ─── CommandBus：Duplicate +1，BatchDelete 批量回落 ──────────────────────────

TEST_F(EditorCoreTest, DuplicateAndBatchDelete) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto a = bus.dispatch(core::CreateEntityCmd{"A"}, *engine_);
    auto b = bus.dispatch(core::CreateEntityCmd{"B"}, *engine_);
    ASSERT_TRUE(a.ok && b.ok);
    const std::uint32_t ida = a.data["entity_id"].GetUint();
    const std::uint32_t idb = b.data["entity_id"].GetUint();
    const int after_two = query.sceneSummary(*engine_).entity_count;

    core::DuplicateEntityCmd dup;
    dup.entity_id = ida;
    auto dr = bus.dispatch(dup, *engine_);
    ASSERT_TRUE(dr.ok) << dr.error_message;
    const std::uint32_t iddup = dr.data["entity_id"].GetUint();
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, after_two + 1);

    core::BatchDeleteEntitiesCmd bd;
    bd.entity_ids = {ida, idb, iddup};
    ASSERT_TRUE(bus.dispatch(bd, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, after_two - 2);
}

// ─── CommandBus + QueryService：选择集 set/get/clear 往返 ─────────────────────

TEST_F(EditorCoreTest, SelectionRoundTrip) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto e = bus.dispatch(core::CreateEntityCmd{"Sel"}, *engine_);
    ASSERT_TRUE(e.ok);
    const std::uint32_t id = e.data["entity_id"].GetUint();

    core::SetSelectionCmd set;
    set.entity_ids = {id};
    ASSERT_TRUE(bus.dispatch(set, *engine_).ok);

    auto sel = query.selection(*engine_);
    ASSERT_TRUE(sel.valid);
    EXPECT_EQ(sel.count, 1);
    ASSERT_TRUE(sel.has_primary);
    EXPECT_EQ(sel.primary_id, id);

    ASSERT_TRUE(bus.dispatch(core::ClearSelectionCmd{}, *engine_).ok);
    EXPECT_EQ(query.selection(*engine_).count, 0);
}

// ─── CommandBus：Reparent 经工具生效，HierarchyVM 重建出父子树 ────────────────

TEST_F(EditorCoreTest, ReparentReflectedInHierarchyVM) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto pa = bus.dispatch(core::CreateEntityCmd{"Parent"}, *engine_);
    auto ch = bus.dispatch(core::CreateEntityCmd{"Child"}, *engine_);
    ASSERT_TRUE(pa.ok && ch.ok);
    const std::uint32_t parent = pa.data["entity_id"].GetUint();
    const std::uint32_t child = ch.data["entity_id"].GetUint();

    core::ReparentEntityCmd rp;
    rp.entity_id = child;
    rp.parent = parent;
    ASSERT_TRUE(bus.dispatch(rp, *engine_).ok);

    auto h = query.hierarchy(*engine_);
    ASSERT_TRUE(h.valid);
    // 找到父节点，确认 child 在其 children 中。
    bool found_parent = false, child_under_parent = false;
    for (const auto& n : h.nodes) {
        if (n.entity_id == parent) {
            found_parent = true;
            for (auto ci : n.children)
                if (h.nodes[ci].entity_id == child) child_under_parent = true;
        }
        if (n.entity_id == child) {
            EXPECT_TRUE(n.has_parent);
            EXPECT_EQ(n.parent_id, parent);
        }
    }
    EXPECT_TRUE(found_parent);
    EXPECT_TRUE(child_under_parent);
    // child 不应再是根。
    for (auto ri : h.roots) EXPECT_NE(h.nodes[ri].entity_id, child);
}

// ─── QueryService：editorState / undoHistory / findByName / metrics ──────────

TEST_F(EditorCoreTest, ReadOnlyViewModelsReflectState) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    auto e = bus.dispatch(core::CreateEntityCmd{"Beacon"}, *engine_);
    ASSERT_TRUE(e.ok);
    const std::uint32_t id = e.data["entity_id"].GetUint();

    auto es = query.editorState(*engine_);
    ASSERT_TRUE(es.valid);
    EXPECT_EQ(es.editor_state, "edit");
    EXPECT_GE(es.entity_count, 1);

    auto uh = query.undoHistory(*engine_);
    ASSERT_TRUE(uh.valid);
    EXPECT_TRUE(uh.can_undo);
    EXPECT_GE(uh.undo_count, 1);

    auto fr = query.findByName("Beacon", false, *engine_);
    ASSERT_TRUE(fr.valid);
    EXPECT_GE(fr.count, 1);
    ASSERT_TRUE(fr.has_first);
    EXPECT_EQ(fr.first_id, id);

    auto m = query.metrics(*engine_);
    ASSERT_TRUE(m.valid);
    EXPECT_EQ(m.entity_count, es.entity_count);
}

// ─── CommandBus：NewScene 清空场景，Undo 后 redo 经命令亦可达 ──────────────────

TEST_F(EditorCoreTest, NewSceneClearsEntities) {
    core::CommandBus bus{sink()};
    core::QueryService query{sink()};

    ASSERT_TRUE(bus.dispatch(core::CreateEntityCmd{"Transient"}, *engine_).ok);
    ASSERT_GE(query.sceneSummary(*engine_).entity_count, 1);

    ASSERT_TRUE(bus.dispatch(core::NewSceneCmd{}, *engine_).ok);
    EXPECT_EQ(query.sceneSummary(*engine_).entity_count, 0);
}
