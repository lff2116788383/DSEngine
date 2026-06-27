/**
 * @file editor_control_server_test.cpp
 * @brief ControlServer::DispatchTool 与 20 个内建 Tool Handler 的无头集成测试
 *
 * 测试策略：
 * - 不启动 WebSocket 服务器（不调用 ControlServer::Start）
 * - 通过 DispatchTool 直接调用 handler，不走网络
 * - 注入 World 到 FramePipeline（不调用 EngineInstance::Init，无 GPU 依赖）
 * - 需要 GPU / AssetManager 的路径只测 error path
 *
 * 覆盖：
 *   dsengine_ping, dsengine_script_create,
 *   dsengine_editor_undo, dsengine_editor_redo,
 *   dsengine_entity_create, dsengine_entity_delete,
 *   dsengine_entity_modify, dsengine_entity_add_component,
 *   dsengine_entity_remove_component, dsengine_entity_get_components,
 *   dsengine_scene_get_state, dsengine_editor_get_state,
 *   dsengine_editor_play, dsengine_editor_stop,
 *   dsengine_scene_save, dsengine_scene_load,
 *   dsengine_editor_screenshot, dsengine_asset_import,
 *   dsengine_material_create, dsengine_lua_execute
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <cmath>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <set>

#include "apps/editor_cpp/src/editor_control_server.h"
#include "apps/editor_cpp/src/editor_shared_components.h"
#include "apps/editor_cpp/src/editor_prefab.h"
#include "apps/editor_cpp/src/editor_shortcuts.h"
#include "apps/editor_cpp/src/editor_toolbar.h"
#include "apps/editor_cpp/src/editor_undo.h"
#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/transform.h"
#include <glm/gtc/quaternion.hpp>

namespace fs = std::filesystem;

// forward-declared in editor_test_stubs.cpp
namespace dse::editor { void ResetEditorStateStub(); }

using namespace dse;
using dse::editor::EditorNameComponent;
using dse::editor::EditorState;
using dse::editor::GetEditorState;
using dse::editor::ResetEditorStateStub;

// ─── 辅助：解析 JSON 字符串到 rapidjson::Document ───────────────────────────

static rapidjson::Document ParseParams(const char* json) {
    rapidjson::Document doc;
    doc.Parse(json);
    return doc;
}

// ─── 测试夹具 ────────────────────────────────────────────────────────────────

class ControlServerTest : public ::testing::Test {
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

    dse::editor::JsonRpcResponse Dispatch(const std::string& method, const char* params_json = "{}") {
        auto params = ParseParams(params_json);
        return server_.DispatchTool(method, params, *engine_);
    }

    int CountAliveEntities() {
        auto& reg = world_.registry();
        int alive = 0;
        for (auto e : reg.storage<entt::entity>()) {
            if (reg.valid(e)) ++alive;
        }
        return alive;
    }
};

// ─── DispatchTool: 未知方法 ──────────────────────────────────────────────────

// 测试 控制服务器：未知Method返回Method不Found
TEST_F(ControlServerTest, UnknownMethodReturnsMethodNotFound) {
    auto resp = Dispatch("dsengine_nonexistent_tool");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32601);
}

// ─── dsengine_ping ───────────────────────────────────────────────────────────

// 测试 控制服务器：Ping返回Pong
TEST_F(ControlServerTest, Ping_ReturnsPong) {
    auto resp = Dispatch("dsengine_ping");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.IsObject());
    ASSERT_TRUE(resp.result.HasMember("pong"));
    EXPECT_TRUE(resp.result["pong"].GetBool());
}

// ─── dsengine_lua_execute ────────────────────────────────────────────────────

// 测试 控制服务器：Lua执行Missingcode参数返回错误
TEST_F(ControlServerTest, LuaExecute_MissingcodeParameterReturnerror) {
    auto resp = Dispatch("dsengine_lua_execute", R"({"other": 1})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// ─── dsengine_script_create ──────────────────────────────────────────────────

// 测试 控制服务器：脚本创建缺失路径参数返回错误
TEST_F(ControlServerTest, ScriptCreate_MissingpathParameterReturnerror) {
    auto resp = Dispatch("dsengine_script_create", R"({"content": "x = 1"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：脚本创建Missingcontent参数返回错误
TEST_F(ControlServerTest, ScriptCreate_MissingcontentParameterReturnerror) {
    auto resp = Dispatch("dsengine_script_create", R"({"path": "test.lua"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：脚本创建路径Containsdotdotrejected
TEST_F(ControlServerTest, ScriptCreate_ThePathContainsdotdotrejected) {
    auto resp = Dispatch("dsengine_script_create",
        R"({"path": "../escape.lua", "content": "x = 1"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：脚本创建法线写入Returnwritten
TEST_F(ControlServerTest, ScriptCreate_NormalWriteReturnwritten) {
    fs::path tmp = fs::temp_directory_path() / "dse_test_script_create.lua";
    std::string params = R"({"path": ")" + tmp.string() + R"(", "content": "return 42"})";
    // 转义反斜杠（Windows 路径）
    for (char& c : params) if (c == '\\') c = '/';

    auto resp = Dispatch("dsengine_script_create", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    ASSERT_TRUE(resp.result.HasMember("written"));
    EXPECT_TRUE(resp.result["written"].GetBool());

    fs::remove(tmp);
}

// ─── dsengine_editor_undo / redo ─────────────────────────────────────────────

// 测试 控制服务器：编辑器撤销返回空Stacksuccess假
TEST_F(ControlServerTest, EditorUndo_ReturnEmptyStacksuccess_false) {
    auto resp = Dispatch("dsengine_editor_undo");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.HasMember("success"));
    EXPECT_FALSE(resp.result["success"].GetBool());
}

// 测试 控制服务器：编辑器重做返回空Stacksuccess假
TEST_F(ControlServerTest, EditorRedo_ReturnEmptyStacksuccess_false) {
    auto resp = Dispatch("dsengine_editor_redo");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.HasMember("success"));
    EXPECT_FALSE(resp.result["success"].GetBool());
}

// 测试 控制服务器：编辑器撤销重做撤销然后重做之后执行
TEST_F(ControlServerTest, EditorUndoRedo_UndoThenRedoAfterExecute) {
    auto& mgr = dse::editor::GetUndoRedoManager();
    mgr.Clear();

    int counter = 0;
    mgr.Execute(std::make_unique<dse::editor::LambdaCommand>(
        "increment",
        [&]{ counter++; },
        [&]{ counter--; }));

    EXPECT_EQ(counter, 1);

    auto undo_resp = Dispatch("dsengine_editor_undo");
    ASSERT_FALSE(undo_resp.is_error);
    EXPECT_TRUE(undo_resp.result["success"].GetBool());
    EXPECT_EQ(counter, 0);

    auto redo_resp = Dispatch("dsengine_editor_redo");
    ASSERT_FALSE(redo_resp.is_error);
    EXPECT_TRUE(redo_resp.result["success"].GetBool());
    EXPECT_EQ(counter, 1);

    mgr.Clear();
}

// ─── dsengine_entity_create ──────────────────────────────────────────────────

// 测试 控制服务器：实体创建Missingname参数返回错误
TEST_F(ControlServerTest, EntityCreate_MissingnameParameterReturnerror) {
    auto resp = Dispatch("dsengine_entity_create", R"({"position": [0,0,0]})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：实体创建最小参数创建实体
TEST_F(ControlServerTest, EntityCreate_MinimalParametersCreateEntity) {
    auto resp = Dispatch("dsengine_entity_create", R"({"name": "TestEntity"})");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    ASSERT_TRUE(resp.result.HasMember("entity_id"));
    ASSERT_TRUE(resp.result.HasMember("name"));
    EXPECT_STREQ(resp.result["name"].GetString(), "TestEntity");

    auto entity = static_cast<entt::entity>(resp.result["entity_id"].GetUint());
    EXPECT_TRUE(world_.registry().valid(entity));
}

// 测试 控制服务器：实体创建Bringpositiongenerated正确变换
TEST_F(ControlServerTest, EntityCreate_BringpositiongeneratedCorrectlyTransform) {
    auto resp = Dispatch("dsengine_entity_create",
        R"({"name": "PosEntity", "position": [1.0, 2.0, 3.0]})");
    ASSERT_FALSE(resp.is_error);

    auto entity = static_cast<entt::entity>(resp.result["entity_id"].GetUint());
    ASSERT_TRUE(world_.registry().all_of<TransformComponent>(entity));
    const auto& t = world_.registry().get<TransformComponent>(entity);
    EXPECT_FLOAT_EQ(t.position.x, 1.0f);
    EXPECT_FLOAT_EQ(t.position.y, 2.0f);
    EXPECT_FLOAT_EQ(t.position.z, 3.0f);
}

// 测试 控制服务器：实体创建Bringmesh Automatically已添加网格渲染器
TEST_F(ControlServerTest, EntityCreate_BringmeshAutomaticallyAddedMeshRenderer) {
    auto resp = Dispatch("dsengine_entity_create",
        R"({"name": "MeshEntity", "mesh": "assets/cube.dmesh"})");
    ASSERT_FALSE(resp.is_error);

    auto entity = static_cast<entt::entity>(resp.result["entity_id"].GetUint());
    EXPECT_TRUE(world_.registry().all_of<MeshRendererComponent>(entity));
    const auto& mr = world_.registry().get<MeshRendererComponent>(entity);
    EXPECT_EQ(mr.mesh_path, "assets/cube.dmesh");
}

// 测试 控制服务器：实体创建Bringcomponents数组批次Addition
TEST_F(ControlServerTest, EntityCreate_BringcomponentsArrayBatchAddition) {
    auto resp = Dispatch("dsengine_entity_create",
        R"({"name": "LightEntity", "components": ["DirectionalLight", "SkyLight"]})");
    ASSERT_FALSE(resp.is_error);

    auto entity = static_cast<entt::entity>(resp.result["entity_id"].GetUint());
    EXPECT_TRUE(world_.registry().all_of<DirectionalLight3DComponent>(entity));
    EXPECT_TRUE(world_.registry().all_of<SkyLightComponent>(entity));
}

// ─── dsengine_entity_delete ──────────────────────────────────────────────────

// 测试 控制服务器：实体删除Invalidid返回错误
TEST_F(ControlServerTest, EntityDelete_InvalididReturnserror) {
    auto resp = Dispatch("dsengine_entity_delete", R"({"entity_id": 9999999})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：实体删除缺失实体ID返回错误
TEST_F(ControlServerTest, EntityDelete_Missingentity_IdReturnserror) {
    auto resp = Dispatch("dsengine_entity_delete", "{}");
    EXPECT_TRUE(resp.is_error);
}

// 测试 控制服务器：实体删除实体Normally
TEST_F(ControlServerTest, EntityDelete_DeleteEntitiesNormally) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "ToDelete"})");
    ASSERT_FALSE(create.is_error);
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string params = R"({"entity_id": )" + std::to_string(eid) + "}";
    auto del = Dispatch("dsengine_entity_delete", params.c_str());
    ASSERT_FALSE(del.is_error) << del.error_message;
    EXPECT_TRUE(del.result["deleted"].GetBool());

    auto entity = static_cast<entt::entity>(eid);
    EXPECT_FALSE(world_.registry().valid(entity));
}

// ─── dsengine_entity_modify ──────────────────────────────────────────────────

// 测试 控制服务器：实体修改Invalidid返回错误
TEST_F(ControlServerTest, EntityModify_InvalididReturnserror) {
    auto resp = Dispatch("dsengine_entity_modify", R"({"entity_id": 9999999})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：实体修改Revisename
TEST_F(ControlServerTest, EntityModify_Revisename) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "OldName"})");
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string params = R"({"entity_id": )" + std::to_string(eid) + R"(, "name": "NewName"})";
    auto mod = Dispatch("dsengine_entity_modify", params.c_str());
    ASSERT_FALSE(mod.is_error);
    EXPECT_TRUE(mod.result["modified"].GetBool());

    auto entity = static_cast<entt::entity>(eid);
    const auto& name_comp = world_.registry().get<EditorNameComponent>(entity);
    EXPECT_EQ(name_comp.name, "NewName");
}

// 测试 控制服务器：实体修改Reviseposition更新变换
TEST_F(ControlServerTest, EntityModify_RevisepositionUpdateTransform) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "MoveMe"})");
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string params = R"({"entity_id": )" + std::to_string(eid) +
        R"(, "position": [10.0, 20.0, 30.0]})";
    auto mod = Dispatch("dsengine_entity_modify", params.c_str());
    ASSERT_FALSE(mod.is_error);

    auto entity = static_cast<entt::entity>(eid);
    const auto& t = world_.registry().get<TransformComponent>(entity);
    EXPECT_FLOAT_EQ(t.position.x, 10.0f);
    EXPECT_FLOAT_EQ(t.position.y, 20.0f);
    EXPECT_FLOAT_EQ(t.position.z, 30.0f);
}

// ─── dsengine_entity_add_component ──────────────────────────────────────────

// 测试 控制服务器：实体添加组件缺失实体ID返回错误
TEST_F(ControlServerTest, EntityAddComponent_Missingentity_IdReturnserror) {
    auto resp = Dispatch("dsengine_entity_add_component", R"({"type": "Camera3D"})");
    EXPECT_TRUE(resp.is_error);
}

// 测试 控制服务器：实体添加组件Missingtype返回错误
TEST_F(ControlServerTest, EntityAddComponent_MissingtypeReturnserror) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "E"})");
    uint32_t eid = create.result["entity_id"].GetUint();
    std::string params = R"({"entity_id": )" + std::to_string(eid) + "}";
    auto resp = Dispatch("dsengine_entity_add_component", params.c_str());
    EXPECT_TRUE(resp.is_error);
}

// 测试 控制服务器：实体添加组件添加到相机3D成功
TEST_F(ControlServerTest, EntityAddComponent_AddToCamera3DSucceeds) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "CamEntity"})");
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string params = R"({"entity_id": )" + std::to_string(eid) + R"(, "type": "Camera3D"})";
    auto add = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(add.is_error) << add.error_message;
    EXPECT_TRUE(add.result["added"].GetBool());

    auto entity = static_cast<entt::entity>(eid);
    EXPECT_TRUE(world_.registry().all_of<Camera3DComponent>(entity));
}

// 测试 控制服务器：实体添加组件相机3 Dbringproperties
TEST_F(ControlServerTest, EntityAddComponent_Camera3Dbringproperties) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "CamFov"})");
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string params = R"({"entity_id": )" + std::to_string(eid) +
        R"(, "type": "Camera3D", "properties": {"fov": 90.0}})";
    auto add = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(add.is_error);

    auto entity = static_cast<entt::entity>(eid);
    const auto& cam = world_.registry().get<Camera3DComponent>(entity);
    EXPECT_FLOAT_EQ(cam.fov, 90.0f);
}

// ─── dsengine_entity_remove_component ────────────────────────────────────────

// 测试 控制服务器：实体移除组件缺失实体ID返回错误
TEST_F(ControlServerTest, EntityRemoveComponent_Missingentity_IdReturnserror) {
    auto resp = Dispatch("dsengine_entity_remove_component", R"({"type": "Camera3D"})");
    EXPECT_TRUE(resp.is_error);
}

// 测试 控制服务器：实体移除组件移除已存在组件
TEST_F(ControlServerTest, EntityRemoveComponent_RemoveExistingComponents) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "RemoveTest"})");
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string add_params = R"({"entity_id": )" + std::to_string(eid) + R"(, "type": "SkyLight"})";
    Dispatch("dsengine_entity_add_component", add_params.c_str());

    std::string rem_params = R"({"entity_id": )" + std::to_string(eid) + R"(, "type": "SkyLight"})";
    auto rem = Dispatch("dsengine_entity_remove_component", rem_params.c_str());
    ASSERT_FALSE(rem.is_error);
    EXPECT_TRUE(rem.result["removed"].GetBool());

    auto entity = static_cast<entt::entity>(eid);
    EXPECT_FALSE(world_.registry().all_of<SkyLightComponent>(entity));
}

// 测试 控制服务器：实体移除组件非存在组件Returnsremoved假
TEST_F(ControlServerTest, EntityRemoveComponent_NonExistentComponentReturnsremoved_false) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "NoComp"})");
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string params = R"({"entity_id": )" + std::to_string(eid) + R"(, "type": "SkyLight"})";
    auto rem = Dispatch("dsengine_entity_remove_component", params.c_str());
    ASSERT_FALSE(rem.is_error);
    EXPECT_FALSE(rem.result["removed"].GetBool());
}

// ─── dsengine_entity_get_components ─────────────────────────────────────────

// 测试 控制服务器：实体获取组件缺失实体ID返回错误
TEST_F(ControlServerTest, EntityGetComponents_Missingentity_IdReturnserror) {
    auto resp = Dispatch("dsengine_entity_get_components", "{}");
    EXPECT_TRUE(resp.is_error);
}

// 测试 控制服务器：实体获取组件新建实体包含变换
TEST_F(ControlServerTest, EntityGetComponents_TheNewEntityContainsTransform) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "CompTest"})");
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string params = R"({"entity_id": )" + std::to_string(eid) + "}";
    auto resp = Dispatch("dsengine_entity_get_components", params.c_str());
    ASSERT_FALSE(resp.is_error);

    ASSERT_TRUE(resp.result.HasMember("components"));
    const auto& comps = resp.result["components"];
    ASSERT_TRUE(comps.IsArray());

    bool found_transform = false;
    for (const auto& c : comps.GetArray()) {
        if (c.IsObject() && c.HasMember("type") &&
            std::string(c["type"].GetString()) == "Transform") {
            found_transform = true;
            break;
        }
    }
    EXPECT_TRUE(found_transform);
}

// ─── dsengine_scene_get_state ────────────────────────────────────────────────

// 测试 控制服务器：场景获取状态数量的空场景实体为0
TEST_F(ControlServerTest, SceneGetState_TheNumberOfEmptySceneEntitiesIs0) {
    auto resp = Dispatch("dsengine_scene_get_state");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.HasMember("entity_count"));
    EXPECT_EQ(resp.result["entity_count"].GetInt(), 0);
    ASSERT_TRUE(resp.result.HasMember("editor_state"));
    EXPECT_STREQ(resp.result["editor_state"].GetString(), "edit");
}

// 测试 控制服务器：场景获取状态Quantity Increases之后创建实体
TEST_F(ControlServerTest, SceneGetState_TheQuantityIncreasesAfterCreatingTheEntity) {
    Dispatch("dsengine_entity_create", R"({"name": "A"})");
    Dispatch("dsengine_entity_create", R"({"name": "B"})");

    auto resp = Dispatch("dsengine_scene_get_state");
    ASSERT_FALSE(resp.is_error);
    EXPECT_GE(resp.result["entity_count"].GetInt(), 2);
}

// ─── dsengine_editor_get_state ───────────────────────────────────────────────

// 测试 控制服务器：编辑器获取状态初始状态Isedit
TEST_F(ControlServerTest, EditorGetState_TheInitialStateIsedit) {
    auto resp = Dispatch("dsengine_editor_get_state");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.HasMember("editor_state"));
    EXPECT_STREQ(resp.result["editor_state"].GetString(), "edit");
}

// ─── dsengine_editor_play / stop ─────────────────────────────────────────────

// 测试 控制服务器：编辑器播放Enterplaymodel
TEST_F(ControlServerTest, EditorPlay_Enterplaymodel) {
    auto resp = Dispatch("dsengine_editor_play");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_STREQ(resp.result["editor_state"].GetString(), "play");
    EXPECT_EQ(dse::editor::GetEditorState(), dse::editor::EditorState::Play);
}

// 测试 控制服务器：编辑器播放已经Inplay返回错误
TEST_F(ControlServerTest, EditorPlay_AlreadyInplayReturnserror) {
    Dispatch("dsengine_editor_play");
    auto resp = Dispatch("dsengine_editor_play");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

// 测试 控制服务器：编辑器停止不Hereplay返回错误
TEST_F(ControlServerTest, EditorStop_NotHereplayReturnserror) {
    auto resp = Dispatch("dsengine_editor_stop");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

// 测试 控制服务器：编辑器停止Fromplay Exit Returnedit
TEST_F(ControlServerTest, EditorStop_FromplayExitReturnedit) {
    Dispatch("dsengine_editor_play");
    auto resp = Dispatch("dsengine_editor_stop");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_STREQ(resp.result["editor_state"].GetString(), "edit");
    EXPECT_EQ(dse::editor::GetEditorState(), dse::editor::EditorState::Edit);
}

// ─── dsengine_scene_save ─────────────────────────────────────────────────────

// 测试 控制服务器：场景保存Withoutpath无当前场景Returnederror
TEST_F(ControlServerTest, SceneSave_WithoutpathNoCurrentSceneReturnederror) {
    auto resp = Dispatch("dsengine_scene_save", "{}");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：场景保存Specified路径为Saved成功
TEST_F(ControlServerTest, SceneSave_TheSpecifiedPathIsSavedSuccessfully) {
    fs::path tmp = fs::temp_directory_path() / "dse_test_scene_save.dscene";
    std::string path_fwd = tmp.string();
    for (char& c : path_fwd) if (c == '\\') c = '/';

    std::string params = R"({"path": ")" + path_fwd + R"("})";
    auto resp = Dispatch("dsengine_scene_save", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["saved"].GetBool());

    EXPECT_TRUE(fs::exists(tmp));
    fs::remove(tmp);
}

// ─── dsengine_scene_load ─────────────────────────────────────────────────────

// 测试 控制服务器：场景加载缺失路径返回错误
TEST_F(ControlServerTest, SceneLoad_MissingpathReturnserror) {
    auto resp = Dispatch("dsengine_scene_load", "{}");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：场景加载文件不存在返回错误
TEST_F(ControlServerTest, SceneLoad_FileDoesNotExistReturnerror) {
    auto resp = Dispatch("dsengine_scene_load",
        R"({"path": "/nonexistent/scene.dscene"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：场景加载保存然后加载往返
TEST_F(ControlServerTest, SceneLoad_SaveThenLoadRoundTrip) {
    Dispatch("dsengine_entity_create", R"({"name": "RoundTripEntity"})");

    fs::path tmp = fs::temp_directory_path() / "dse_test_roundtrip.dscene";
    std::string path_fwd = tmp.string();
    for (char& c : path_fwd) if (c == '\\') c = '/';

    std::string save_params = R"({"path": ")" + path_fwd + R"("})";
    auto save = Dispatch("dsengine_scene_save", save_params.c_str());
    ASSERT_FALSE(save.is_error) << save.error_message;

    world_.registry().clear();

    std::string load_params = R"({"path": ")" + path_fwd + R"("})";
    auto load = Dispatch("dsengine_scene_load", load_params.c_str());
    ASSERT_FALSE(load.is_error) << load.error_message;
    EXPECT_TRUE(load.result["loaded"].GetBool());
    EXPECT_GE(load.result["entity_count"].GetInt(), 1);

    fs::remove(tmp);
}

// ─── dsengine_editor_screenshot ──────────────────────────────────────────────

// 测试 控制服务器：Screenshot无GPU返回错误
TEST_F(ControlServerTest, Screenshot_WithoutGPUReturnserror) {
    auto resp = Dispatch("dsengine_editor_screenshot");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

// ─── dsengine_asset_import ────────────────────────────────────────────────────

// 测试 控制服务器：资源导入缺失路径返回错误
TEST_F(ControlServerTest, AssetImport_MissingpathReturnserror) {
    auto resp = Dispatch("dsengine_asset_import", "{}");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// 测试 控制服务器：资源导入无资源管理器返回错误
TEST_F(ControlServerTest, AssetImport_WithoutAssetManagerReturnserror) {
    auto resp = Dispatch("dsengine_asset_import",
        R"({"path": "assets/tex.png", "type": "texture"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

// ─── dsengine_material_create ─────────────────────────────────────────────────

// 测试 控制服务器：材质创建Specified路径为Written成功
TEST_F(ControlServerTest, MaterialCreate_TheSpecifiedPathIsWrittenSuccessfully) {
    auto tmp_path = fs::temp_directory_path() / "dse_test_mat.dmat";
    std::string tmp = tmp_path.generic_string();
    std::string params = R"({"name":"test_mat","save_path":")" + tmp + R"("})";
    auto resp = Dispatch("dsengine_material_create", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result.HasMember("success"));
    EXPECT_TRUE(resp.result["success"].GetBool());
    EXPECT_TRUE(fs::exists(tmp));
    fs::remove(tmp);
}

// 测试 控制服务器：材质创建Illegal路径返回错误
TEST_F(ControlServerTest, MaterialCreate_IllegalPathReturnerror) {
    auto resp = Dispatch("dsengine_material_create",
        R"({"name":"x","save_path":"Z:\\nonexistent_dir_xyz\\a\\b\\c.dmat"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

// ─── EntityModify rotation / scale ───────────────────────────────────────────

// 测试 控制服务器：实体修改Reviserotation Euler Angles
TEST_F(ControlServerTest, EntityModify_ReviserotationEulerAngles) {
    auto create_resp = Dispatch("dsengine_entity_create", R"({"name":"RotTest"})");
    ASSERT_FALSE(create_resp.is_error);
    uint32_t eid = create_resp.result["entity_id"].GetUint();

    std::string params =
        R"({"entity_id":)" + std::to_string(eid) +
        R"(,"rotation":[30.0,0.0,0.0]})";
    auto resp = Dispatch("dsengine_entity_modify", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["modified"].GetBool());

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(registry.all_of<TransformComponent>(entity));
    const auto& t = registry.get<TransformComponent>(entity);
    glm::vec3 euler = glm::degrees(glm::eulerAngles(t.rotation));
    EXPECT_NEAR(std::abs(euler.x), 30.0f, 1.0f);
}

// 测试 控制服务器：实体修改Revisescale
TEST_F(ControlServerTest, EntityModify_Revisescale) {
    auto create_resp = Dispatch("dsengine_entity_create", R"({"name":"ScaleTest"})");
    ASSERT_FALSE(create_resp.is_error);
    uint32_t eid = create_resp.result["entity_id"].GetUint();

    std::string params =
        R"({"entity_id":)" + std::to_string(eid) +
        R"(,"scale":[2.0,3.0,4.0]})";
    auto resp = Dispatch("dsengine_entity_modify", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(registry.all_of<TransformComponent>(entity));
    const auto& t = registry.get<TransformComponent>(entity);
    EXPECT_NEAR(t.scale.x, 2.0f, 0.01f);
    EXPECT_NEAR(t.scale.y, 3.0f, 0.01f);
    EXPECT_NEAR(t.scale.z, 4.0f, 0.01f);
}

// ─── EntityAddComponent DirectionalLight / PointLight ────────────────────────

// 测试 控制服务器：实体添加组件添加到方向光灯光成功
TEST_F(ControlServerTest, EntityAddComponent_AddToDirectionalLightSucceeds) {
    auto create_resp = Dispatch("dsengine_entity_create", R"({"name":"DirLightEnt"})");
    ASSERT_FALSE(create_resp.is_error);
    uint32_t eid = create_resp.result["entity_id"].GetUint();

    std::string params =
        R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"DirectionalLight","properties":{"intensity":2.5}})";
    auto resp = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["added"].GetBool());

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(registry.all_of<DirectionalLight3DComponent>(entity));
    EXPECT_NEAR(registry.get<DirectionalLight3DComponent>(entity).intensity, 2.5f, 0.01f);
}

// 测试 控制服务器：实体添加组件添加到点灯光成功
TEST_F(ControlServerTest, EntityAddComponent_AddToPointLightSucceeds) {
    auto create_resp = Dispatch("dsengine_entity_create", R"({"name":"PointLightEnt"})");
    ASSERT_FALSE(create_resp.is_error);
    uint32_t eid = create_resp.result["entity_id"].GetUint();

    std::string params =
        R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"PointLight","properties":{"intensity":1.8,"range":15.0}})";
    auto resp = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["added"].GetBool());

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(registry.all_of<PointLightComponent>(entity));
    const auto& pl = registry.get<PointLightComponent>(entity);
    EXPECT_NEAR(pl.intensity, 1.8f, 0.01f);
    EXPECT_NEAR(pl.radius, 15.0f, 0.01f);
}

// 测试 控制服务器：实体添加组件添加到聚光灯光成功
TEST_F(ControlServerTest, EntityAddComponent_AddToSpotLightSucceeds) {
    auto create_resp = Dispatch("dsengine_entity_create", R"({"name":"SpotLightEnt"})");
    ASSERT_FALSE(create_resp.is_error);
    uint32_t eid = create_resp.result["entity_id"].GetUint();

    std::string params =
        R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"SpotLight","properties":{"intensity":3.0,"inner_cone":20.0,"outer_cone":35.0}})";
    auto resp = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["added"].GetBool());

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(registry.all_of<dse::SpotLightComponent>(entity));
    const auto& sl = registry.get<dse::SpotLightComponent>(entity);
    EXPECT_NEAR(sl.intensity, 3.0f, 0.01f);
    EXPECT_NEAR(sl.inner_cone_angle, 20.0f, 0.01f);
    EXPECT_NEAR(sl.outer_cone_angle, 35.0f, 0.01f);
}

// 测试 控制服务器：实体添加组件添加到刚体3D成功
TEST_F(ControlServerTest, EntityAddComponent_AddToRigidBody3DSucceeds) {
    auto create_resp = Dispatch("dsengine_entity_create", R"({"name":"RB3DEnt"})");
    ASSERT_FALSE(create_resp.is_error);
    uint32_t eid = create_resp.result["entity_id"].GetUint();

    std::string params =
        R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"RigidBody3D","properties":{"mass":2.5,"body_type":"dynamic"}})";
    auto resp = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["added"].GetBool());

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(registry.all_of<dse::RigidBody3DComponent>(entity));
    const auto& rb = registry.get<dse::RigidBody3DComponent>(entity);
    EXPECT_NEAR(rb.mass, 2.5f, 0.01f);
    EXPECT_EQ(rb.type, dse::RigidBody3DType::Dynamic);
}

// 测试 控制服务器：实体添加组件添加到天空灯光成功
TEST_F(ControlServerTest, EntityAddComponent_AddToSkyLightSucceeds) {
    auto create_resp = Dispatch("dsengine_entity_create", R"({"name":"SkyLightEnt"})");
    ASSERT_FALSE(create_resp.is_error);
    uint32_t eid = create_resp.result["entity_id"].GetUint();

    std::string params =
        R"({"entity_id":)" + std::to_string(eid) + R"(,"type":"SkyLight"})";
    auto resp = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["added"].GetBool());

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    EXPECT_TRUE(registry.all_of<dse::SkyLightComponent>(entity));
}

// 测试 控制服务器：实体添加组件相机3D Near远剪辑
TEST_F(ControlServerTest, EntityAddComponent_Camera3D_NearFarClip) {
    auto create_resp = Dispatch("dsengine_entity_create", R"({"name":"CamNearFar"})");
    ASSERT_FALSE(create_resp.is_error);
    uint32_t eid = create_resp.result["entity_id"].GetUint();

    std::string params =
        R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"Camera3D","properties":{"fov":45.0,"near":0.2,"far":2000.0}})";
    auto resp = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["added"].GetBool());

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(registry.all_of<dse::Camera3DComponent>(entity));
    const auto& cam = registry.get<dse::Camera3DComponent>(entity);
    EXPECT_NEAR(cam.fov, 45.0f, 0.01f);
    EXPECT_FLOAT_EQ(cam.near_clip, 0.2f);
    EXPECT_FLOAT_EQ(cam.far_clip, 2000.0f);
}

// 测试 控制服务器：实体添加组件盒碰撞体3D带Props
TEST_F(ControlServerTest, EntityAddComponent_BoxCollider3D_WithProps) {
    auto create_resp = Dispatch("dsengine_entity_create", R"({"name":"ColliderEnt"})");
    ASSERT_FALSE(create_resp.is_error);
    uint32_t eid = create_resp.result["entity_id"].GetUint();

    std::string params =
        R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"BoxCollider3D","properties":{"size":[2.0,3.0,1.5],"is_trigger":true}})";
    auto resp = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["added"].GetBool());

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(registry.all_of<dse::BoxCollider3DComponent>(entity));
    const auto& bc = registry.get<dse::BoxCollider3DComponent>(entity);
    EXPECT_NEAR(bc.size.x, 2.0f, 0.01f);
    EXPECT_NEAR(bc.size.y, 3.0f, 0.01f);
    EXPECT_NEAR(bc.size.z, 1.5f, 0.01f);
    EXPECT_TRUE(bc.is_trigger);
}

// ─── Test 57: modify_component 修改 PointLight 属性 ─────────────────────────

// 测试 控制服务器：实体修改组件点灯光
TEST_F(ControlServerTest, EntityModify_ModifyComponent_PointLight) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"PLEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string add = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"PointLight","properties":{"intensity":1.0}})";
    ASSERT_FALSE(Dispatch("dsengine_entity_add_component", add.c_str()).is_error);

    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"modify_component":{"type":"PointLight","properties":{"intensity":5.5,"range":20.0}}})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    auto& registry = world_.registry();
    const auto& pl = registry.get<dse::PointLightComponent>(
        static_cast<entt::entity>(eid));
    EXPECT_NEAR(pl.intensity, 5.5f, 0.01f);
    EXPECT_NEAR(pl.radius, 20.0f, 0.01f);
}

// ─── Test 58: get_components 返回正确组件列表 ────────────────────────────────

// 测试 控制服务器：实体获取组件返回正确列表
TEST_F(ControlServerTest, EntityGetComponents_ReturnsCorrectList) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"MultiCompEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    auto add = [&](const char* type) {
        std::string p = R"({"entity_id":)" + std::to_string(eid) +
            R"(,"type":")" + type + R"(","properties":{}})";
        ASSERT_FALSE(Dispatch("dsengine_entity_add_component", p.c_str()).is_error);
    };
    add("Camera3D");
    add("PointLight");

    std::string gp = R"({"entity_id":)" + std::to_string(eid) + R"(})";
    auto resp = Dispatch("dsengine_entity_get_components", gp.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    ASSERT_TRUE(resp.result.HasMember("components"));
    ASSERT_TRUE(resp.result["components"].IsArray());

    std::set<std::string> names;
    for (const auto& c : resp.result["components"].GetArray()) {
        if (c.IsString()) {
            names.insert(c.GetString());
        } else if (c.IsObject() && c.HasMember("type") && c["type"].IsString()) {
            names.insert(c["type"].GetString());
        }
    }
    EXPECT_TRUE(names.count("Camera3D") || names.count("Camera3DComponent"));
    EXPECT_TRUE(names.count("PointLight") || names.count("PointLightComponent"));
}

// ─── Test 59: remove_component 移除不存在组件不崩溃 ─────────────────────────

// 测试 控制服务器：实体移除组件非存在返回假
TEST_F(ControlServerTest, EntityRemoveComponent_NonExistent_ReturnsFalse) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"NoCompEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string params = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"MeshRenderer"})";
    auto resp = Dispatch("dsengine_entity_remove_component", params.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_FALSE(resp.result["removed"].GetBool());
}

// ─── Test 60: modify_components 数组批量 patch ─────────────────────────────

// 测试 控制服务器：实体修改组件批次数组
TEST_F(ControlServerTest, EntityModify_ModifyComponents_BatchArray) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"BatchEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    // 添加 Camera3D 和 DirectionalLight
    auto addComp = [&](const char* type) {
        std::string p = R"({"entity_id":)" + std::to_string(eid) +
            R"(,"type":")" + type + R"(","properties":{}})";
        ASSERT_FALSE(Dispatch("dsengine_entity_add_component", p.c_str()).is_error);
    };
    addComp("Camera3D");
    addComp("DirectionalLight");

    // 批量 patch
    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"modify_components":[)"
        R"({"type":"Camera3D","properties":{"fov":75.0}},)"
        R"({"type":"DirectionalLight","properties":{"intensity":4.2}})"
        R"(]})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    auto& registry = world_.registry();
    const auto entity = static_cast<entt::entity>(eid);
    EXPECT_NEAR(registry.get<Camera3DComponent>(entity).fov, 75.0f, 0.01f);
    EXPECT_NEAR(registry.get<dse::DirectionalLight3DComponent>(entity).intensity, 4.2f, 0.01f);
}

// ─── Test 61: modify_component 修改 AudioSource ──────────────────────────────

// 测试 控制服务器：实体修改组件音频源
TEST_F(ControlServerTest, EntityModify_ModifyComponent_AudioSource) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"AudioEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string add = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"AudioSource","properties":{}})";
    ASSERT_FALSE(Dispatch("dsengine_entity_add_component", add.c_str()).is_error);

    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"modify_component":{"type":"AudioSource","properties":{"volume":0.6,"pitch":1.25,"loop":true}}})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    const auto& as = world_.registry().get<AudioSourceComponent>(
        static_cast<entt::entity>(eid));
    EXPECT_NEAR(as.volume, 0.6f, 0.01f);
    EXPECT_NEAR(as.pitch, 1.25f, 0.01f);
    EXPECT_TRUE(as.loop);
}

// ─── Test 62: modify_component 修改 SpotLight ────────────────────────────────

// 测试 控制服务器：实体修改组件聚光灯光
TEST_F(ControlServerTest, EntityModify_ModifyComponent_SpotLight) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"SpotEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string add = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"SpotLight","properties":{}})";
    ASSERT_FALSE(Dispatch("dsengine_entity_add_component", add.c_str()).is_error);

    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"modify_component":{"type":"SpotLight","properties":{"intensity":6.0,"range":18.0,"inner_cone":20.0,"outer_cone":40.0}}})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    const auto& sl = world_.registry().get<dse::SpotLightComponent>(
        static_cast<entt::entity>(eid));
    EXPECT_NEAR(sl.intensity, 6.0f, 0.01f);
    EXPECT_NEAR(sl.radius, 18.0f, 0.01f);
    EXPECT_NEAR(sl.inner_cone_angle, 20.0f, 0.01f);
    EXPECT_NEAR(sl.outer_cone_angle, 40.0f, 0.01f);
}

// ─── Test 63: scene_get_state include_components=false ───────────────────────

// 测试 控制服务器：场景获取状态无组件无组件字段
TEST_F(ControlServerTest, SceneGetState_WithoutComponents_NoComponentsField) {
    Dispatch("dsengine_entity_create", R"({"name":"StateTestEnt"})");

    auto resp = Dispatch("dsengine_scene_get_state",
        R"({"include_components": false})");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    ASSERT_TRUE(resp.result.HasMember("entities"));
    ASSERT_TRUE(resp.result["entities"].IsArray());
    ASSERT_GT(resp.result["entities"].Size(), 0u);

    const auto& first = resp.result["entities"][0];
    EXPECT_FALSE(first.HasMember("components"));
}

// ─── Test 64: entity_create 带 rotation + scale ──────────────────────────────

// 测试 控制服务器：实体创建带Rotation且缩放
TEST_F(ControlServerTest, EntityCreate_WithRotationAndScale) {
    auto resp = Dispatch("dsengine_entity_create",
        R"({"name":"FullTransform","position":[1,2,3],"rotation":[0,90,0],"scale":[2,2,2]})");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    uint32_t eid = resp.result["entity_id"].GetUint();

    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(world_.registry().all_of<TransformComponent>(entity));
    const auto& t = world_.registry().get<TransformComponent>(entity);
    EXPECT_NEAR(t.position.x, 1.0f, 0.01f);
    EXPECT_NEAR(t.scale.x, 2.0f, 0.01f);
    EXPECT_NEAR(t.scale.y, 2.0f, 0.01f);
}

// ─── Test 65: delete 后 get_components 返回 invalid entity ──────────────────

// 测试 控制服务器：实体删除然后获取组件返回无效
TEST_F(ControlServerTest, EntityDelete_ThenGetComponents_ReturnsInvalid) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"WillDie"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    auto del = Dispatch("dsengine_entity_delete",
        (R"({"entity_id":)" + std::to_string(eid) + "}").c_str());
    ASSERT_FALSE(del.is_error);

    auto gc = Dispatch("dsengine_entity_get_components",
        (R"({"entity_id":)" + std::to_string(eid) + "}").c_str());
    EXPECT_TRUE(gc.is_error);
    EXPECT_EQ(gc.error_code, -32602);
}

// ─── Test 66: modify_component RigidBody3D mass + body_type ──────────────────

// 测试 控制服务器：实体修改组件刚体3D
TEST_F(ControlServerTest, EntityModify_ModifyComponent_RigidBody3D) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"RBEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string add = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"RigidBody3D","properties":{}})";
    ASSERT_FALSE(Dispatch("dsengine_entity_add_component", add.c_str()).is_error);

    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"modify_component":{"type":"RigidBody3D","properties":{"mass":10.0,"body_type":"kinematic"}}})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    const auto& rb = world_.registry().get<dse::RigidBody3DComponent>(
        static_cast<entt::entity>(eid));
    EXPECT_NEAR(rb.mass, 10.0f, 0.01f);
    EXPECT_EQ(rb.type, dse::RigidBody3DType::Kinematic);
}

// ─── Test 67: add_components 批量带 properties 对象 ──────────────────────────

// 测试 控制服务器：实体修改添加组件带属性
TEST_F(ControlServerTest, EntityModify_AddComponents_WithProperties) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"AddBatchEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"add_components":[)"
        R"({"type":"PointLight","properties":{"intensity":5.0,"range":20.0}},)"
        R"("AudioListener")"
        R"(]})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    auto entity = static_cast<entt::entity>(eid);
    ASSERT_TRUE(world_.registry().all_of<dse::PointLightComponent>(entity));
    ASSERT_TRUE(world_.registry().all_of<AudioListenerComponent>(entity));
    EXPECT_NEAR(world_.registry().get<dse::PointLightComponent>(entity).intensity, 5.0f, 0.01f);
    EXPECT_NEAR(world_.registry().get<dse::PointLightComponent>(entity).radius, 20.0f, 0.01f);
}

// ─── Test 68: asset_import 未知扩展名 auto-detect 失败 ───────────────────────

// 测试 控制服务器：资源导入未知Extension返回错误
TEST_F(ControlServerTest, AssetImport_UnknownExtension_ReturnsError) {
    auto resp = Dispatch("dsengine_asset_import",
        R"({"path":"test_file.xyz"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// ─── Test 69: modify_component 无效组件类型 ─────────────────────────────────

// 测试 控制服务器：实体修改组件无效类型无特效
TEST_F(ControlServerTest, EntityModify_ModifyComponent_InvalidType_NoEffect) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"NoComp"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"modify_component":{"type":"NonExistentComponent","properties":{"x":1}}})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error);
    // 没有匹配组件，modified_components 为空
    if (resp.result.HasMember("modified_components")) {
        EXPECT_EQ(resp.result["modified_components"].Size(), 0u);
    }
}

// ─── Test 70: scene_load 不存在路径 ─────────────────────────────────────────

// 测试 控制服务器：场景加载非存在路径返回错误
TEST_F(ControlServerTest, SceneLoad_NonExistentPath_ReturnsError) {
    auto resp = Dispatch("dsengine_scene_load",
        R"({"path":"__non_existent_path_12345__.dscene"})");
    EXPECT_TRUE(resp.is_error);
}

// ─── Test 71: add_component 重复添加同类型 ──────────────────────────────────

// 测试 控制服务器：实体添加组件重复无错误
TEST_F(ControlServerTest, EntityAddComponent_Duplicate_NoError) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"DupTest"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string add = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"PointLight","properties":{}})";
    ASSERT_FALSE(Dispatch("dsengine_entity_add_component", add.c_str()).is_error);

    auto resp2 = Dispatch("dsengine_entity_add_component", add.c_str());
    // 重复添加不应崩溃，可能返回成功或错误取决于实现
    (void)resp2;
    SUCCEED();
}

// ─── Test 72: entity_modify 无效 entity_id ──────────────────────────────────

// 测试 控制服务器：实体修改无效实体ID返回错误
TEST_F(ControlServerTest, EntityModify_InvalidEntityId_ReturnsError) {
    auto resp = Dispatch("dsengine_entity_modify",
        R"({"entity_id":99999,"name":"Ghost"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// ─── Test 73: material_create 基本调用 ──────────────────────────────────────

// 测试 控制服务器：材质创建基础
TEST_F(ControlServerTest, MaterialCreate_Basic) {
    auto resp = Dispatch("dsengine_material_create",
        R"({"name":"test_mat_new","shader_variant":"MESH_PBR","base_color":[1,1,1,1]})");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result.IsObject());
}

// ─── Test 74: scene_get_state include_components=true 有 components 字段 ───

// 测试 控制服务器：场景获取状态带组件拥有组件字段
TEST_F(ControlServerTest, SceneGetState_WithComponents_HasComponentsField) {
    Dispatch("dsengine_entity_create", R"({"name":"CompEnt"})");

    auto resp = Dispatch("dsengine_scene_get_state",
        R"({"include_components": true})");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    ASSERT_TRUE(resp.result.HasMember("entities"));
    ASSERT_GT(resp.result["entities"].Size(), 0u);

    const auto& first = resp.result["entities"][0];
    EXPECT_TRUE(first.HasMember("components"));
}

// ─── Test 75: modify_components 三组件批量（PointLight + SpotLight + AudioSource）────

// 测试 控制服务器：实体修改组件三类型
TEST_F(ControlServerTest, EntityModify_ModifyComponents_ThreeTypes) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"ThreeCompEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    auto addComp = [&](const char* type) {
        std::string p = R"({"entity_id":)" + std::to_string(eid) +
            R"(,"type":")" + type + R"(","properties":{}})";
        ASSERT_FALSE(Dispatch("dsengine_entity_add_component", p.c_str()).is_error);
    };
    addComp("PointLight");
    addComp("SpotLight");
    addComp("AudioSource");

    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"modify_components":[)"
        R"({"type":"PointLight","properties":{"intensity":3.0,"range":15.0}},)"
        R"({"type":"SpotLight","properties":{"intensity":4.0,"inner_cone":25.0,"outer_cone":45.0}},)"
        R"({"type":"AudioSource","properties":{"volume":0.8,"loop":true}})"
        R"(]})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    EXPECT_NEAR(registry.get<dse::PointLightComponent>(entity).intensity, 3.0f, 0.01f);
    EXPECT_NEAR(registry.get<dse::PointLightComponent>(entity).radius, 15.0f, 0.01f);
    EXPECT_NEAR(registry.get<dse::SpotLightComponent>(entity).intensity, 4.0f, 0.01f);
    EXPECT_NEAR(registry.get<dse::SpotLightComponent>(entity).inner_cone_angle, 25.0f, 0.01f);
    EXPECT_NEAR(registry.get<dse::SpotLightComponent>(entity).outer_cone_angle, 45.0f, 0.01f);
    EXPECT_NEAR(registry.get<AudioSourceComponent>(entity).volume, 0.8f, 0.01f);
    EXPECT_TRUE(registry.get<AudioSourceComponent>(entity).loop);
}

// ─── Test 76: modify_components 部分命中（数组中含未挂载类型 → 只更新存在的，不报错）────

// 测试 控制服务器：实体修改组件Partial命中
TEST_F(ControlServerTest, EntityModify_ModifyComponents_PartialHit) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"PartialHitEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string add = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"Camera3D","properties":{}})";
    ASSERT_FALSE(Dispatch("dsengine_entity_add_component", add.c_str()).is_error);

    // DirectionalLight 未添加到实体，应被忽略；Camera3D 应被更新
    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"modify_components":[)"
        R"({"type":"Camera3D","properties":{"fov":90.0}},)"
        R"({"type":"DirectionalLight","properties":{"intensity":5.0}})"
        R"(]})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    EXPECT_NEAR(registry.get<Camera3DComponent>(entity).fov, 90.0f, 0.01f);
    EXPECT_FALSE(registry.all_of<dse::DirectionalLight3DComponent>(entity));
}

// ─── Test 77: modify_components 空数组 → no-op，不报错 ─────────────────────────

// 测试 控制服务器：实体修改组件空数组无操作
TEST_F(ControlServerTest, EntityModify_ModifyComponents_EmptyArray_NoOp) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"EmptyBatchEnt"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string mod = R"({"entity_id":)" + std::to_string(eid) + R"(,"modify_components":[]})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
}

// ─── Test 78: name + modify_components 并存 ────────────────────────────────────

// 测试 控制服务器：实体修改组件带顶部级别名称
TEST_F(ControlServerTest, EntityModify_ModifyComponents_WithTopLevelName) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"OrigName"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string add = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"type":"PointLight","properties":{}})";
    ASSERT_FALSE(Dispatch("dsengine_entity_add_component", add.c_str()).is_error);

    std::string mod = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"name":"RenamedEnt","modify_components":[)"
        R"({"type":"PointLight","properties":{"intensity":7.0,"range":25.0}})"
        R"(]})";
    auto resp = Dispatch("dsengine_entity_modify", mod.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    auto& registry = world_.registry();
    auto entity = static_cast<entt::entity>(eid);
    EXPECT_EQ(registry.get<dse::editor::EditorNameComponent>(entity).name, "RenamedEnt");
    EXPECT_NEAR(registry.get<dse::PointLightComponent>(entity).intensity, 7.0f, 0.01f);
    EXPECT_NEAR(registry.get<dse::PointLightComponent>(entity).radius, 25.0f, 0.01f);
}

// ─── Test 79: entity_get_state 返回 transform + 组件详情 ─────────────────────

// 测试 控制服务器：实体获取状态返回变换且组件
TEST_F(ControlServerTest, EntityGetState_ReturnsTransformAndComponents) {
    auto cr = Dispatch("dsengine_entity_create",
        R"({"name":"StateEnt","position":[1.0,2.0,3.0],"components":["PointLight"]})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string q = R"({"entity_id":)" + std::to_string(eid) + R"(})";
    auto resp = Dispatch("dsengine_entity_get_state", q.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;

    EXPECT_EQ(std::string(resp.result["name"].GetString()), "StateEnt");
    ASSERT_TRUE(resp.result.HasMember("transform"));
    const auto& tf = resp.result["transform"];
    EXPECT_NEAR(tf["position"][0].GetFloat(), 1.0f, 0.01f);
    EXPECT_NEAR(tf["position"][1].GetFloat(), 2.0f, 0.01f);
    EXPECT_NEAR(tf["position"][2].GetFloat(), 3.0f, 0.01f);
    ASSERT_TRUE(resp.result.HasMember("components"));
    EXPECT_TRUE(resp.result["components"].IsArray());
}

// ─── Test 80: entity_get_state 对无效 entity_id 报错 ──────────────────────────

// 测试 控制服务器：实体获取状态无效ID返回错误
TEST_F(ControlServerTest, EntityGetState_InvalidId_ReturnsError) {
    auto resp = Dispatch("dsengine_entity_get_state", R"({"entity_id":99999})");
    EXPECT_TRUE(resp.is_error);
}

// ─── Test 81: entity_duplicate 复制实体含组件 ────────────────────────────────

// 测试 控制服务器：实体重复Copies名称且组件
TEST_F(ControlServerTest, EntityDuplicate_CopiesNameAndComponents) {
    auto cr = Dispatch("dsengine_entity_create",
        R"({"name":"Original","components":["PointLight"]})");
    ASSERT_FALSE(cr.is_error);
    uint32_t src_id = cr.result["entity_id"].GetUint();

    std::string dup_p = R"({"entity_id":)" + std::to_string(src_id) + R"(})";
    auto dr = Dispatch("dsengine_entity_duplicate", dup_p.c_str());
    ASSERT_FALSE(dr.is_error) << dr.error_message;

    uint32_t dst_id = dr.result["entity_id"].GetUint();
    EXPECT_NE(dst_id, src_id);
    EXPECT_EQ(std::string(dr.result["name"].GetString()), "Original (Copy)");
    EXPECT_EQ(dr.result["source_entity_id"].GetUint(), src_id);

    auto& registry = world_.registry();
    auto dst = static_cast<entt::entity>(dst_id);
    EXPECT_TRUE(registry.all_of<dse::PointLightComponent>(dst));
}

// ─── Test 82: entity_duplicate 对无效 id 报错 ────────────────────────────────

// 测试 控制服务器：实体重复无效ID返回错误
TEST_F(ControlServerTest, EntityDuplicate_InvalidId_ReturnsError) {
    auto resp = Dispatch("dsengine_entity_duplicate", R"({"entity_id":99999})");
    EXPECT_TRUE(resp.is_error);
}

// ─── Test 83: prefab_save + prefab_instantiate 往返 ──────────────────────────

// 测试 控制服务器：预制体保存且Instantiate往返
TEST_F(ControlServerTest, PrefabSaveAndInstantiate_RoundTrip) {
    auto cr = Dispatch("dsengine_entity_create",
        R"({"name":"PrefabSrc","mesh":"cube.obj"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t src_id = cr.result["entity_id"].GetUint();

    // 保存到临时路径（generic_string 用正斜杠，避免 Windows 反斜杠破坏 JSON 字符串）
    std::string tmp = (std::filesystem::temp_directory_path() / "test_rpc.dprefab").generic_string();
    std::string save_p = R"({"entity_id":)" + std::to_string(src_id) +
        R"(,"path":")" + tmp + R"("})";
    auto sr = Dispatch("dsengine_prefab_save", save_p.c_str());
    ASSERT_FALSE(sr.is_error) << sr.error_message;
    EXPECT_TRUE(sr.result["saved"].GetBool());

    // 实例化
    std::string inst_p = R"({"path":")" + tmp + R"("})";
    auto ir = Dispatch("dsengine_prefab_instantiate", inst_p.c_str());
    ASSERT_FALSE(ir.is_error) << ir.error_message;

    uint32_t inst_id = ir.result["entity_id"].GetUint();
    EXPECT_NE(inst_id, src_id);
    EXPECT_EQ(std::string(ir.result["name"].GetString()), "PrefabSrc");

    auto& registry = world_.registry();
    auto inst = static_cast<entt::entity>(inst_id);
    EXPECT_TRUE(registry.valid(inst));
    EXPECT_TRUE(registry.all_of<dse::MeshRendererComponent>(inst));
    EXPECT_TRUE(dse::editor::IsPrefabInstance(registry, inst));
}

// ─── Test 84: scene_new 清空所有实体 ─────────────────────────────────────────

// 测试 控制服务器：场景新建清空全部实体
TEST_F(ControlServerTest, SceneNew_ClearsAllEntities) {
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"Ent1"})");
    ASSERT_FALSE(cr.is_error);
    Dispatch("dsengine_entity_create", R"({"name":"Ent2"})");

    auto& registry = world_.registry();
    EXPECT_GT(static_cast<int>(registry.storage<entt::entity>().size()), 0);

    auto resp = Dispatch("dsengine_scene_new", "{}");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["cleared"].GetBool());

    int alive = 0;
    for (auto e : registry.storage<entt::entity>()) {
        if (registry.valid(e)) ++alive;
    }
    EXPECT_EQ(alive, 0);
}

// ─── Test 85: entity_reparent 设置父节点 ─────────────────────────────────────

// 测试 控制服务器：实体重设父级设置父组件
TEST_F(ControlServerTest, EntityReparent_SetsParentComponent) {
    auto pr = Dispatch("dsengine_entity_create", R"({"name":"Parent"})");
    ASSERT_FALSE(pr.is_error);
    uint32_t parent_id = pr.result["entity_id"].GetUint();

    auto cr = Dispatch("dsengine_entity_create", R"({"name":"Child"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t child_id = cr.result["entity_id"].GetUint();

    std::string p = R"({"entity_id":)" + std::to_string(child_id) +
        R"(,"parent_id":)" + std::to_string(parent_id) + R"(})";
    auto resp = Dispatch("dsengine_entity_reparent", p.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_EQ(resp.result["entity_id"].GetUint(), child_id);

    auto& registry = world_.registry();
    auto child = static_cast<entt::entity>(child_id);
    ASSERT_TRUE(registry.all_of<ParentComponent>(child));
    EXPECT_EQ(static_cast<uint32_t>(registry.get<ParentComponent>(child).parent), parent_id);
}

// ─── Test 86: entity_reparent 用 0xFFFFFFFF 解除父节点 ───────────────────────

// 测试 控制服务器：实体重设父级Detach带最大Uint
TEST_F(ControlServerTest, EntityReparent_DetachWithMaxUint) {
    auto pr = Dispatch("dsengine_entity_create", R"({"name":"Parent"})");
    uint32_t parent_id = pr.result["entity_id"].GetUint();
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"Child"})");
    uint32_t child_id = cr.result["entity_id"].GetUint();

    // 先绑定
    std::string attach = R"({"entity_id":)" + std::to_string(child_id) +
        R"(,"parent_id":)" + std::to_string(parent_id) + R"(})";
    Dispatch("dsengine_entity_reparent", attach.c_str());

    // 用 0xFFFFFFFF 解绑
    std::string detach = R"({"entity_id":)" + std::to_string(child_id) +
        R"(,"parent_id":4294967295})";
    auto resp = Dispatch("dsengine_entity_reparent", detach.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["parent_id"].IsNull());

    auto& registry = world_.registry();
    auto child = static_cast<entt::entity>(child_id);
    EXPECT_FALSE(registry.all_of<ParentComponent>(child));
}

// ─── Test 87: entity_reparent 循环检测报错 ───────────────────────────────────

// 测试 控制服务器：实体重设父级环形父返回错误
TEST_F(ControlServerTest, EntityReparent_CircularParent_ReturnsError) {
    auto ar = Dispatch("dsengine_entity_create", R"({"name":"A"})");
    uint32_t a_id = ar.result["entity_id"].GetUint();
    auto br = Dispatch("dsengine_entity_create", R"({"name":"B"})");
    uint32_t b_id = br.result["entity_id"].GetUint();

    // A → parent=B
    std::string p1 = R"({"entity_id":)" + std::to_string(a_id) +
        R"(,"parent_id":)" + std::to_string(b_id) + R"(})";
    Dispatch("dsengine_entity_reparent", p1.c_str());

    // B → parent=A (circular)
    std::string p2 = R"({"entity_id":)" + std::to_string(b_id) +
        R"(,"parent_id":)" + std::to_string(a_id) + R"(})";
    auto resp = Dispatch("dsengine_entity_reparent", p2.c_str());
    EXPECT_TRUE(resp.is_error);
}

// ─── Test 87b: entity_reparent undo 解绑后 redo 重新挂回（redo 保真） ─────────

// 测试 控制服务器：实体重设父级撤销后重做恢复父级
TEST_F(ControlServerTest, EntityReparent_UndoThenRedoRestoresParent) {
    auto pr = Dispatch("dsengine_entity_create", R"({"name":"Parent"})");
    uint32_t parent_id = pr.result["entity_id"].GetUint();
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"Child"})");
    uint32_t child_id = cr.result["entity_id"].GetUint();

    std::string p = R"({"entity_id":)" + std::to_string(child_id) +
        R"(,"parent_id":)" + std::to_string(parent_id) + R"(})";
    ASSERT_FALSE(Dispatch("dsengine_entity_reparent", p.c_str()).is_error);

    auto& registry = world_.registry();
    auto child = static_cast<entt::entity>(child_id);
    ASSERT_TRUE(registry.all_of<ParentComponent>(child));

    // Undo → child 回到根（原 old_parent 为 null）
    ASSERT_FALSE(Dispatch("dsengine_editor_undo", "{}").is_error);
    EXPECT_FALSE(registry.all_of<ParentComponent>(child));

    // Redo → 重新挂回 parent（验证非空 redo lambda）
    ASSERT_FALSE(Dispatch("dsengine_editor_redo", "{}").is_error);
    ASSERT_TRUE(registry.all_of<ParentComponent>(child));
    EXPECT_EQ(static_cast<uint32_t>(registry.get<ParentComponent>(child).parent), parent_id);
}

// ─── Test 88: selection_get 空选择返回 count=0 ───────────────────────────────

// 测试 控制服务器：选择获取空返回零
TEST_F(ControlServerTest, SelectionGet_EmptyReturnsZero) {
    Dispatch("dsengine_selection_clear", "{}");
    auto resp = Dispatch("dsengine_selection_get", "{}");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_EQ(resp.result["count"].GetInt(), 0);
    EXPECT_TRUE(resp.result["entity_ids"].IsArray());
    EXPECT_EQ(resp.result["entity_ids"].Size(), 0u);
    EXPECT_TRUE(resp.result["primary_id"].IsNull());
}

// ─── Test 89: selection_set 批量设置选择 ─────────────────────────────────────

// 测试 控制服务器：选择设置多个实体
TEST_F(ControlServerTest, SelectionSet_SetsMultipleEntities) {
    auto r1 = Dispatch("dsengine_entity_create", R"({"name":"Sel1"})");
    auto r2 = Dispatch("dsengine_entity_create", R"({"name":"Sel2"})");
    uint32_t id1 = r1.result["entity_id"].GetUint();
    uint32_t id2 = r2.result["entity_id"].GetUint();

    std::string p = R"({"entity_ids":[)" + std::to_string(id1) +
        "," + std::to_string(id2) + "]}";
    auto resp = Dispatch("dsengine_selection_set", p.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_EQ(resp.result["count"].GetInt(), 2);
    EXPECT_EQ(resp.result["primary_id"].GetUint(), id2);

    // Verify via selection_get
    auto gr = Dispatch("dsengine_selection_get", "{}");
    ASSERT_FALSE(gr.is_error);
    EXPECT_EQ(gr.result["count"].GetInt(), 2);
    EXPECT_EQ(gr.result["entity_ids"].Size(), 2u);
}

// ─── Test 90: selection_set 过滤无效 entity_id ───────────────────────────────

// 测试 控制服务器：选择设置Filters无效ID
TEST_F(ControlServerTest, SelectionSet_FiltersInvalidIds) {
    auto r = Dispatch("dsengine_entity_create", R"({"name":"Valid"})");
    uint32_t valid_id = r.result["entity_id"].GetUint();

    std::string p = R"({"entity_ids":[)" + std::to_string(valid_id) +
        R"(,9999999]})";
    auto resp = Dispatch("dsengine_selection_set", p.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_EQ(resp.result["count"].GetInt(), 1);
}

// ─── Test 91: selection_clear 清空选择 ───────────────────────────────────────

// 测试 控制服务器：选择清空选择
TEST_F(ControlServerTest, SelectionClear_ClearsSelection) {
    auto r = Dispatch("dsengine_entity_create", R"({"name":"ToDeselect"})");
    uint32_t id = r.result["entity_id"].GetUint();
    Dispatch("dsengine_selection_set",
        (R"({"entity_ids":[)" + std::to_string(id) + "]}").c_str());

    auto cr = Dispatch("dsengine_selection_clear", "{}");
    ASSERT_FALSE(cr.is_error) << cr.error_message;
    EXPECT_TRUE(cr.result["cleared"].GetBool());

    auto gr = Dispatch("dsengine_selection_get", "{}");
    EXPECT_EQ(gr.result["count"].GetInt(), 0);
}

// ─── Test 92: entity_batch_delete 批量删除多个实体 ────────────────────────────

// 测试 控制服务器：实体批次删除Deletes多个
TEST_F(ControlServerTest, EntityBatchDelete_DeletesMultiple) {
    auto r1 = Dispatch("dsengine_entity_create", R"({"name":"BatchA"})");
    auto r2 = Dispatch("dsengine_entity_create", R"({"name":"BatchB"})");
    auto r3 = Dispatch("dsengine_entity_create", R"({"name":"BatchC"})");
    uint32_t id1 = r1.result["entity_id"].GetUint();
    uint32_t id2 = r2.result["entity_id"].GetUint();
    uint32_t id3 = r3.result["entity_id"].GetUint();

    int count_before = CountAliveEntities(); (void)id1; (void)id2; (void)id3;

    std::string p = R"({"entity_ids":[)" +
        std::to_string(id1) + "," +
        std::to_string(id2) + "," +
        std::to_string(id3) + "]}";
    auto resp = Dispatch("dsengine_entity_batch_delete", p.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_EQ(resp.result["deleted_count"].GetInt(), 3);
    EXPECT_EQ(resp.result["deleted_ids"].Size(), 3u);
    EXPECT_EQ(CountAliveEntities(), count_before - 3);
}

// ─── Test 93: entity_batch_delete 过滤无效 id ────────────────────────────────

// 测试 控制服务器：实体批次删除Filters无效ID
TEST_F(ControlServerTest, EntityBatchDelete_FiltersInvalidIds) {
    auto r = Dispatch("dsengine_entity_create", R"({"name":"OnlyOne"})");
    uint32_t valid_id = r.result["entity_id"].GetUint();
    int count_before = CountAliveEntities();

    std::string p = R"({"entity_ids":[)" + std::to_string(valid_id) + R"(,9999999]})";
    auto resp = Dispatch("dsengine_entity_batch_delete", p.c_str());
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_EQ(resp.result["deleted_count"].GetInt(), 1);
    EXPECT_EQ(CountAliveEntities(), count_before - 1);
}

// ─── Test 94: entity_batch_delete Undo 恢复所有实体 ──────────────────────────

// 测试 控制服务器：实体批次删除撤销恢复实体
TEST_F(ControlServerTest, EntityBatchDelete_UndoRestoresEntities) {
    auto r1 = Dispatch("dsengine_entity_create", R"({"name":"UndoA"})");
    auto r2 = Dispatch("dsengine_entity_create", R"({"name":"UndoB"})");
    uint32_t id1 = r1.result["entity_id"].GetUint();
    uint32_t id2 = r2.result["entity_id"].GetUint();
    int count_before = CountAliveEntities();

    std::string p = R"({"entity_ids":[)" + std::to_string(id1) + "," +
        std::to_string(id2) + "]}";
    Dispatch("dsengine_entity_batch_delete", p.c_str());
    EXPECT_EQ(CountAliveEntities(), count_before - 2);

    Dispatch("dsengine_editor_undo", "{}");
    EXPECT_EQ(CountAliveEntities(), count_before);
}

// ─── Test 95: undo_history 空栈时 can_undo=false ──────────────────────────────

// 测试 控制服务器：撤销历史空栈
TEST_F(ControlServerTest, UndoHistory_EmptyStack) {
    Dispatch("dsengine_scene_new", "{}");  // clears undo stack
    auto resp = Dispatch("dsengine_undo_history", "{}");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_FALSE(resp.result["can_undo"].GetBool());
    EXPECT_FALSE(resp.result["can_redo"].GetBool());
    EXPECT_EQ(resp.result["undo_count"].GetInt(), 0);
    EXPECT_EQ(resp.result["redo_count"].GetInt(), 0);
    EXPECT_TRUE(resp.result["undo_history"].IsArray());
    EXPECT_EQ(resp.result["undo_history"].Size(), 0u);
}

// ─── Test 96: undo_history 创建实体后 can_undo=true ──────────────────────────

// 测试 控制服务器：撤销历史之后创建能够撤销
TEST_F(ControlServerTest, UndoHistory_AfterCreate_CanUndo) {
    Dispatch("dsengine_scene_new", "{}");
    Dispatch("dsengine_entity_create", R"({"name":"H1"})");
    Dispatch("dsengine_entity_create", R"({"name":"H2"})");

    auto resp = Dispatch("dsengine_undo_history", "{}");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["can_undo"].GetBool());
    EXPECT_GE(resp.result["undo_count"].GetInt(), 1);
    EXPECT_FALSE(std::string(resp.result["undo_description"].GetString()).empty());
    EXPECT_GE(resp.result["undo_history"].Size(), 1u);
}

// ─── Test 97: undo 后 can_redo=true，redo_description 非空 ──────────────────

// 测试 控制服务器：撤销历史之后撤销能够重做
TEST_F(ControlServerTest, UndoHistory_AfterUndo_CanRedo) {
    Dispatch("dsengine_scene_new", "{}");
    Dispatch("dsengine_entity_create", R"({"name":"RedoMe"})");
    Dispatch("dsengine_editor_undo", "{}");

    auto resp = Dispatch("dsengine_undo_history", "{}");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_TRUE(resp.result["can_redo"].GetBool());
    EXPECT_GE(resp.result["redo_count"].GetInt(), 1);
    EXPECT_FALSE(std::string(resp.result["redo_description"].GetString()).empty());
}

// ─── Test 98: entity_find_by_name 精确匹配 ────────────────────────────────────

// 测试 控制服务器：实体查找按名称Exact匹配
TEST_F(ControlServerTest, EntityFindByName_ExactMatch) {
    Dispatch("dsengine_scene_new", "{}");
    Dispatch("dsengine_entity_create", R"({"name":"UniqueAlpha"})");
    Dispatch("dsengine_entity_create", R"({"name":"BetaEntity"})");

    auto resp = Dispatch("dsengine_entity_find_by_name", R"({"name":"UniqueAlpha"})");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_EQ(resp.result["count"].GetInt(), 1);
    EXPECT_FALSE(resp.result["entity_id"].IsNull());
}

// ─── Test 99: entity_find_by_name 部分匹配 ────────────────────────────────────

// 测试 控制服务器：实体查找按名称Partial匹配
TEST_F(ControlServerTest, EntityFindByName_PartialMatch) {
    Dispatch("dsengine_scene_new", "{}");
    Dispatch("dsengine_entity_create", R"({"name":"TreeA"})");
    Dispatch("dsengine_entity_create", R"({"name":"TreeB"})");
    Dispatch("dsengine_entity_create", R"({"name":"Rock"})");

    auto resp = Dispatch("dsengine_entity_find_by_name",
        R"({"name":"Tree","partial":true})");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_EQ(resp.result["count"].GetInt(), 2);
    EXPECT_EQ(resp.result["matches"].Size(), 2u);
}

// ─── Test 100: entity_find_by_name 未找到时 count=0 entity_id=null ───────────

// 测试 控制服务器：实体查找按名称无匹配
TEST_F(ControlServerTest, EntityFindByName_NoMatch) {
    auto resp = Dispatch("dsengine_entity_find_by_name",
        R"({"name":"__nonexistent_xyz__"})");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_EQ(resp.result["count"].GetInt(), 0);
    EXPECT_TRUE(resp.result["entity_id"].IsNull());
}

// ─── Test 101: entity_modify rename 支持 Undo ─────────────────────────────────

// 测试 控制服务器：实体修改Rename撤销
TEST_F(ControlServerTest, EntityModify_RenameUndo) {
    Dispatch("dsengine_scene_new", "{}");
    auto cr = Dispatch("dsengine_entity_create", R"({"name":"OrigName"})");
    ASSERT_FALSE(cr.is_error);
    uint32_t eid = cr.result["entity_id"].GetUint();

    std::string mp = R"({"entity_id":)" + std::to_string(eid) +
        R"(,"name":"NewName"})";
    Dispatch("dsengine_entity_modify", mp.c_str());

    auto& registry = world_.registry();
    auto ent = static_cast<entt::entity>(eid);
    EXPECT_EQ(registry.get<dse::editor::EditorNameComponent>(ent).name, "NewName");

    Dispatch("dsengine_editor_undo", "{}");
    EXPECT_EQ(registry.get<dse::editor::EditorNameComponent>(ent).name, "OrigName");
}
