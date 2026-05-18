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

#include "apps/editor_cpp/src/editor_control_server.h"
#include "apps/editor_cpp/src/editor_shared_components.h"
#include "apps/editor_cpp/src/editor_shortcuts.h"
#include "apps/editor_cpp/src/editor_toolbar.h"
#include "apps/editor_cpp/src/editor_undo.h"
#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/audio.h"
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
};

// ─── DispatchTool: 未知方法 ──────────────────────────────────────────────────

TEST_F(ControlServerTest, 未知方法返回MethodNotFound) {
    auto resp = Dispatch("dsengine_nonexistent_tool");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32601);
}

// ─── dsengine_ping ───────────────────────────────────────────────────────────

TEST_F(ControlServerTest, Ping_返回pong) {
    auto resp = Dispatch("dsengine_ping");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.IsObject());
    ASSERT_TRUE(resp.result.HasMember("pong"));
    EXPECT_TRUE(resp.result["pong"].GetBool());
}

// ─── dsengine_lua_execute ────────────────────────────────────────────────────

TEST_F(ControlServerTest, LuaExecute_缺code参数返回error) {
    auto resp = Dispatch("dsengine_lua_execute", R"({"other": 1})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

// ─── dsengine_script_create ──────────────────────────────────────────────────

TEST_F(ControlServerTest, ScriptCreate_缺path参数返回error) {
    auto resp = Dispatch("dsengine_script_create", R"({"content": "x = 1"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, ScriptCreate_缺content参数返回error) {
    auto resp = Dispatch("dsengine_script_create", R"({"path": "test.lua"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, ScriptCreate_路径含dotdot被拒绝) {
    auto resp = Dispatch("dsengine_script_create",
        R"({"path": "../escape.lua", "content": "x = 1"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, ScriptCreate_正常写入返回written) {
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

TEST_F(ControlServerTest, EditorUndo_空栈返回success_false) {
    auto resp = Dispatch("dsengine_editor_undo");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.HasMember("success"));
    EXPECT_FALSE(resp.result["success"].GetBool());
}

TEST_F(ControlServerTest, EditorRedo_空栈返回success_false) {
    auto resp = Dispatch("dsengine_editor_redo");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.HasMember("success"));
    EXPECT_FALSE(resp.result["success"].GetBool());
}

TEST_F(ControlServerTest, EditorUndoRedo_执行后可撤销再重做) {
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

TEST_F(ControlServerTest, EntityCreate_缺name参数返回error) {
    auto resp = Dispatch("dsengine_entity_create", R"({"position": [0,0,0]})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, EntityCreate_最小参数创建实体) {
    auto resp = Dispatch("dsengine_entity_create", R"({"name": "TestEntity"})");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    ASSERT_TRUE(resp.result.HasMember("entity_id"));
    ASSERT_TRUE(resp.result.HasMember("name"));
    EXPECT_STREQ(resp.result["name"].GetString(), "TestEntity");

    auto entity = static_cast<entt::entity>(resp.result["entity_id"].GetUint());
    EXPECT_TRUE(world_.registry().valid(entity));
}

TEST_F(ControlServerTest, EntityCreate_带position生成正确Transform) {
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

TEST_F(ControlServerTest, EntityCreate_带mesh自动添加MeshRenderer) {
    auto resp = Dispatch("dsengine_entity_create",
        R"({"name": "MeshEntity", "mesh": "assets/cube.dmesh"})");
    ASSERT_FALSE(resp.is_error);

    auto entity = static_cast<entt::entity>(resp.result["entity_id"].GetUint());
    EXPECT_TRUE(world_.registry().all_of<MeshRendererComponent>(entity));
    const auto& mr = world_.registry().get<MeshRendererComponent>(entity);
    EXPECT_EQ(mr.mesh_path, "assets/cube.dmesh");
}

TEST_F(ControlServerTest, EntityCreate_带components数组批量添加) {
    auto resp = Dispatch("dsengine_entity_create",
        R"({"name": "LightEntity", "components": ["DirectionalLight", "SkyLight"]})");
    ASSERT_FALSE(resp.is_error);

    auto entity = static_cast<entt::entity>(resp.result["entity_id"].GetUint());
    EXPECT_TRUE(world_.registry().all_of<DirectionalLight3DComponent>(entity));
    EXPECT_TRUE(world_.registry().all_of<SkyLightComponent>(entity));
}

// ─── dsengine_entity_delete ──────────────────────────────────────────────────

TEST_F(ControlServerTest, EntityDelete_无效id返回error) {
    auto resp = Dispatch("dsengine_entity_delete", R"({"entity_id": 9999999})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, EntityDelete_缺entity_id返回error) {
    auto resp = Dispatch("dsengine_entity_delete", "{}");
    EXPECT_TRUE(resp.is_error);
}

TEST_F(ControlServerTest, EntityDelete_正常删除实体) {
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

TEST_F(ControlServerTest, EntityModify_无效id返回error) {
    auto resp = Dispatch("dsengine_entity_modify", R"({"entity_id": 9999999})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, EntityModify_修改name) {
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

TEST_F(ControlServerTest, EntityModify_修改position更新Transform) {
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

TEST_F(ControlServerTest, EntityAddComponent_缺entity_id返回error) {
    auto resp = Dispatch("dsengine_entity_add_component", R"({"type": "Camera3D"})");
    EXPECT_TRUE(resp.is_error);
}

TEST_F(ControlServerTest, EntityAddComponent_缺type返回error) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "E"})");
    uint32_t eid = create.result["entity_id"].GetUint();
    std::string params = R"({"entity_id": )" + std::to_string(eid) + "}";
    auto resp = Dispatch("dsengine_entity_add_component", params.c_str());
    EXPECT_TRUE(resp.is_error);
}

TEST_F(ControlServerTest, EntityAddComponent_添加Camera3D成功) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "CamEntity"})");
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string params = R"({"entity_id": )" + std::to_string(eid) + R"(, "type": "Camera3D"})";
    auto add = Dispatch("dsengine_entity_add_component", params.c_str());
    ASSERT_FALSE(add.is_error) << add.error_message;
    EXPECT_TRUE(add.result["added"].GetBool());

    auto entity = static_cast<entt::entity>(eid);
    EXPECT_TRUE(world_.registry().all_of<Camera3DComponent>(entity));
}

TEST_F(ControlServerTest, EntityAddComponent_Camera3D带properties) {
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

TEST_F(ControlServerTest, EntityRemoveComponent_缺entity_id返回error) {
    auto resp = Dispatch("dsengine_entity_remove_component", R"({"type": "Camera3D"})");
    EXPECT_TRUE(resp.is_error);
}

TEST_F(ControlServerTest, EntityRemoveComponent_移除已有组件) {
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

TEST_F(ControlServerTest, EntityRemoveComponent_不存在的组件返回removed_false) {
    auto create = Dispatch("dsengine_entity_create", R"({"name": "NoComp"})");
    uint32_t eid = create.result["entity_id"].GetUint();

    std::string params = R"({"entity_id": )" + std::to_string(eid) + R"(, "type": "SkyLight"})";
    auto rem = Dispatch("dsengine_entity_remove_component", params.c_str());
    ASSERT_FALSE(rem.is_error);
    EXPECT_FALSE(rem.result["removed"].GetBool());
}

// ─── dsengine_entity_get_components ─────────────────────────────────────────

TEST_F(ControlServerTest, EntityGetComponents_缺entity_id返回error) {
    auto resp = Dispatch("dsengine_entity_get_components", "{}");
    EXPECT_TRUE(resp.is_error);
}

TEST_F(ControlServerTest, EntityGetComponents_新建实体含Transform) {
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

TEST_F(ControlServerTest, SceneGetState_空场景实体数为0) {
    auto resp = Dispatch("dsengine_scene_get_state");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.HasMember("entity_count"));
    EXPECT_EQ(resp.result["entity_count"].GetInt(), 0);
    ASSERT_TRUE(resp.result.HasMember("editor_state"));
    EXPECT_STREQ(resp.result["editor_state"].GetString(), "edit");
}

TEST_F(ControlServerTest, SceneGetState_创建实体后数量增加) {
    Dispatch("dsengine_entity_create", R"({"name": "A"})");
    Dispatch("dsengine_entity_create", R"({"name": "B"})");

    auto resp = Dispatch("dsengine_scene_get_state");
    ASSERT_FALSE(resp.is_error);
    EXPECT_GE(resp.result["entity_count"].GetInt(), 2);
}

// ─── dsengine_editor_get_state ───────────────────────────────────────────────

TEST_F(ControlServerTest, EditorGetState_初始状态为edit) {
    auto resp = Dispatch("dsengine_editor_get_state");
    ASSERT_FALSE(resp.is_error);
    ASSERT_TRUE(resp.result.HasMember("editor_state"));
    EXPECT_STREQ(resp.result["editor_state"].GetString(), "edit");
}

// ─── dsengine_editor_play / stop ─────────────────────────────────────────────

TEST_F(ControlServerTest, EditorPlay_进入play模式) {
    auto resp = Dispatch("dsengine_editor_play");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_STREQ(resp.result["editor_state"].GetString(), "play");
    EXPECT_EQ(dse::editor::GetEditorState(), dse::editor::EditorState::Play);
}

TEST_F(ControlServerTest, EditorPlay_已在play返回error) {
    Dispatch("dsengine_editor_play");
    auto resp = Dispatch("dsengine_editor_play");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

TEST_F(ControlServerTest, EditorStop_未在play返回error) {
    auto resp = Dispatch("dsengine_editor_stop");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

TEST_F(ControlServerTest, EditorStop_从play退出返回edit) {
    Dispatch("dsengine_editor_play");
    auto resp = Dispatch("dsengine_editor_stop");
    ASSERT_FALSE(resp.is_error) << resp.error_message;
    EXPECT_STREQ(resp.result["editor_state"].GetString(), "edit");
    EXPECT_EQ(dse::editor::GetEditorState(), dse::editor::EditorState::Edit);
}

// ─── dsengine_scene_save ─────────────────────────────────────────────────────

TEST_F(ControlServerTest, SceneSave_无path无当前场景返回error) {
    auto resp = Dispatch("dsengine_scene_save", "{}");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, SceneSave_指定路径保存成功) {
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

TEST_F(ControlServerTest, SceneLoad_缺path返回error) {
    auto resp = Dispatch("dsengine_scene_load", "{}");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, SceneLoad_文件不存在返回error) {
    auto resp = Dispatch("dsengine_scene_load",
        R"({"path": "/nonexistent/scene.dscene"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, SceneLoad_先保存再加载往返) {
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

TEST_F(ControlServerTest, Screenshot_无GPU返回error) {
    auto resp = Dispatch("dsengine_editor_screenshot");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

// ─── dsengine_asset_import ────────────────────────────────────────────────────

TEST_F(ControlServerTest, AssetImport_缺path返回error) {
    auto resp = Dispatch("dsengine_asset_import", "{}");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32602);
}

TEST_F(ControlServerTest, AssetImport_无AssetManager返回error) {
    auto resp = Dispatch("dsengine_asset_import",
        R"({"path": "assets/tex.png", "type": "texture"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

// ─── dsengine_material_create ─────────────────────────────────────────────────

TEST_F(ControlServerTest, MaterialCreate_指定路径写入成功) {
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

TEST_F(ControlServerTest, MaterialCreate_非法路径返回error) {
    auto resp = Dispatch("dsengine_material_create",
        R"({"name":"x","save_path":"Z:\\nonexistent_dir_xyz\\a\\b\\c.dmat"})");
    EXPECT_TRUE(resp.is_error);
    EXPECT_EQ(resp.error_code, -32603);
}

// ─── EntityModify rotation / scale ───────────────────────────────────────────

TEST_F(ControlServerTest, EntityModify_修改rotation欧拉角) {
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

TEST_F(ControlServerTest, EntityModify_修改scale) {
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

TEST_F(ControlServerTest, EntityAddComponent_添加DirectionalLight成功) {
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

TEST_F(ControlServerTest, EntityAddComponent_添加PointLight成功) {
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

TEST_F(ControlServerTest, EntityAddComponent_添加SpotLight成功) {
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

TEST_F(ControlServerTest, EntityAddComponent_添加RigidBody3D成功) {
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

TEST_F(ControlServerTest, EntityAddComponent_添加SkyLight成功) {
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
