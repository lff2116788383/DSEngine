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
