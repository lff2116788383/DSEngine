#include "editor_control_server.h"

#include <iostream>
#include <fstream>
#include <filesystem>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "editor_toolbar.h"
#include "editor_scene_io.h"
#include "editor_shared_components.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace dse::editor {

// ─── Helper ─────────────────────────────────────────────────────────────────

static JsonRpcResponse MakeOk(rapidjson::Document result = {}) {
    JsonRpcResponse resp;
    resp.is_error = false;
    resp.result = std::move(result);
    return resp;
}

static JsonRpcResponse MakeToolError(int code, const std::string& msg) {
    JsonRpcResponse resp;
    resp.is_error = true;
    resp.error_code = code;
    resp.error_message = msg;
    return resp;
}

// ─── Tool: dsengine_lua_execute ─────────────────────────────────────────────

static JsonRpcResponse HandleLuaExecute(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    if (!params.HasMember("code") || !params["code"].IsString()) {
        return MakeToolError(-32602, "Missing required param: code");
    }

    const char* code = params["code"].GetString();
    std::string out;
    bool ok = dse::runtime::ExecuteLuaString(code, &out);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("success", ok, alloc);
    result.AddMember("output",
        rapidjson::Value(out.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_get_state ─────────────────────────────────────────

static JsonRpcResponse HandleSceneGetState(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    bool include_components = true;
    if (params.HasMember("include_components") && params["include_components"].IsBool()) {
        include_components = params["include_components"].GetBool();
    }

    World& world = engine.pipeline()->world();
    auto& registry = world.registry();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    result.AddMember("editor_state",
        rapidjson::Value(
            GetEditorState() == EditorState::Play ? "play" :
            GetEditorState() == EditorState::Pause ? "pause" : "edit",
            alloc),
        alloc);

    rapidjson::Value entities_arr(rapidjson::kArrayType);

    auto& entity_view = registry.storage<entt::entity>();
    for (auto entity : entity_view) {
        if (!registry.valid(entity)) continue;

        rapidjson::Value entity_obj(rapidjson::kObjectType);
        entity_obj.AddMember("id", static_cast<uint32_t>(entity), alloc);

        // Name
        if (registry.all_of<EditorNameComponent>(entity)) {
            const auto& name = registry.get<EditorNameComponent>(entity);
            entity_obj.AddMember("name",
                rapidjson::Value(name.name.c_str(), alloc), alloc);
        }

        if (include_components) {
            rapidjson::Value components(rapidjson::kArrayType);

            if (registry.all_of<TransformComponent>(entity)) {
                const auto& t = registry.get<TransformComponent>(entity);
                rapidjson::Value comp(rapidjson::kObjectType);
                comp.AddMember("type", "Transform", alloc);
                rapidjson::Value pos(rapidjson::kArrayType);
                pos.PushBack(t.position.x, alloc).PushBack(t.position.y, alloc).PushBack(t.position.z, alloc);
                comp.AddMember("position", pos, alloc);
                rapidjson::Value scl(rapidjson::kArrayType);
                scl.PushBack(t.scale.x, alloc).PushBack(t.scale.y, alloc).PushBack(t.scale.z, alloc);
                comp.AddMember("scale", scl, alloc);
                components.PushBack(comp, alloc);
            }

            if (registry.all_of<dse::Camera3DComponent>(entity)) {
                rapidjson::Value comp(rapidjson::kObjectType);
                comp.AddMember("type", "Camera3D", alloc);
                const auto& cam = registry.get<dse::Camera3DComponent>(entity);
                comp.AddMember("fov", cam.fov, alloc);
                comp.AddMember("near_clip", cam.near_clip, alloc);
                comp.AddMember("far_clip", cam.far_clip, alloc);
                components.PushBack(comp, alloc);
            }

            if (registry.all_of<dse::MeshRendererComponent>(entity)) {
                rapidjson::Value comp(rapidjson::kObjectType);
                comp.AddMember("type", "MeshRenderer", alloc);
                components.PushBack(comp, alloc);
            }

            if (registry.all_of<dse::DirectionalLight3DComponent>(entity)) {
                rapidjson::Value comp(rapidjson::kObjectType);
                comp.AddMember("type", "DirectionalLight", alloc);
                components.PushBack(comp, alloc);
            }

            if (registry.all_of<dse::PointLightComponent>(entity)) {
                rapidjson::Value comp(rapidjson::kObjectType);
                comp.AddMember("type", "PointLight", alloc);
                components.PushBack(comp, alloc);
            }

            if (registry.all_of<SpriteRendererComponent>(entity)) {
                rapidjson::Value comp(rapidjson::kObjectType);
                comp.AddMember("type", "SpriteRenderer", alloc);
                components.PushBack(comp, alloc);
            }

            entity_obj.AddMember("components", components, alloc);
        }

        entities_arr.PushBack(entity_obj, alloc);
    }

    result.AddMember("entities", entities_arr, alloc);
    result.AddMember("entity_count",
        static_cast<int>(registry.storage<entt::entity>().size()), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_create ───────────────────────────────────────────

static JsonRpcResponse HandleEntityCreate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("name") || !params["name"].IsString()) {
        return MakeToolError(-32602, "Missing required param: name");
    }

    World& world = engine.pipeline()->world();
    auto& registry = world.registry();
    auto entity = registry.create();

    // Name
    auto& name_comp = registry.emplace<EditorNameComponent>(entity);
    name_comp.name = params["name"].GetString();

    // Transform
    TransformComponent transform;
    if (params.HasMember("position") && params["position"].IsArray() && params["position"].Size() >= 3) {
        const auto& p = params["position"];
        transform.position = glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat());
    }
    if (params.HasMember("scale") && params["scale"].IsArray() && params["scale"].Size() >= 3) {
        const auto& s = params["scale"];
        transform.scale = glm::vec3(s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat());
    }
    if (params.HasMember("rotation") && params["rotation"].IsArray() && params["rotation"].Size() >= 3) {
        const auto& r = params["rotation"];
        transform.rotation = glm::quat(glm::vec3(
            glm::radians(r[0].GetFloat()),
            glm::radians(r[1].GetFloat()),
            glm::radians(r[2].GetFloat())));
    }
    registry.emplace<TransformComponent>(entity, transform);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("name", rapidjson::Value(name_comp.name.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_delete ───────────────────────────────────────────

static JsonRpcResponse HandleEntityDelete(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    registry.destroy(entity);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("deleted", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_script_create ───────────────────────────────────────────

static JsonRpcResponse HandleScriptCreate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    if (!params.HasMember("path") || !params["path"].IsString() ||
        !params.HasMember("content") || !params["content"].IsString()) {
        return MakeToolError(-32602, "Missing required params: path, content");
    }

    std::filesystem::path file_path = params["path"].GetString();
    // 安全检查：不允许 .. 路径逃逸
    if (file_path.string().find("..") != std::string::npos) {
        return MakeToolError(-32602, "Path must not contain '..'");
    }

    std::filesystem::create_directories(file_path.parent_path());
    std::ofstream ofs(file_path, std::ios::trunc);
    if (!ofs.is_open()) {
        return MakeToolError(-32603, "Failed to write file: " + file_path.string());
    }
    ofs << params["content"].GetString();
    ofs.close();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(file_path.string().c_str(), alloc), alloc);
    result.AddMember("written", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_get_state ────────────────────────────────────────

static JsonRpcResponse HandleEditorGetState(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    auto& registry = engine.pipeline()->world().registry();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    const char* state_str =
        GetEditorState() == EditorState::Play ? "play" :
        GetEditorState() == EditorState::Pause ? "pause" : "edit";
    result.AddMember("editor_state", rapidjson::Value(state_str, alloc), alloc);
    result.AddMember("entity_count",
        static_cast<int>(registry.storage<entt::entity>().size()), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_ping ────────────────────────────────────────────────────

static JsonRpcResponse HandlePing(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("pong", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── 注册表 ─────────────────────────────────────────────────────────────────

void RegisterBuiltinTools(ControlServer& server) {
    server.RegisterTool("dsengine_ping",             HandlePing);
    server.RegisterTool("dsengine_lua_execute",      HandleLuaExecute);
    server.RegisterTool("dsengine_scene_get_state",  HandleSceneGetState);
    server.RegisterTool("dsengine_entity_create",    HandleEntityCreate);
    server.RegisterTool("dsengine_entity_delete",    HandleEntityDelete);
    server.RegisterTool("dsengine_script_create",    HandleScriptCreate);
    server.RegisterTool("dsengine_editor_get_state", HandleEditorGetState);
}

} // namespace dse::editor
