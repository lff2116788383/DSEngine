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
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/audio.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "editor_toolbar.h"
#include "editor_scene_io.h"
#include "editor_shared_components.h"
#include "editor_shortcuts.h"
#include "editor_undo.h"
#include "editor_shell.h"
#include "editor_scene_tabs.h"
#include "editor_prefab.h"
#include "editor_selection.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stb/stb_image_write.h>

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

static void CollectEntityComponents(entt::registry& registry, entt::entity entity,
                                     rapidjson::Value& arr, rapidjson::Document::AllocatorType& alloc,
                                     bool include_properties);
static bool RemoveComponentByType(entt::registry& registry, entt::entity entity,
                                   const std::string& type);

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
            dse::editor::GetEditorState() == dse::editor::EditorState::Play ? "play" :
            dse::editor::GetEditorState() == dse::editor::EditorState::Pause ? "pause" : "edit",
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
            CollectEntityComponents(registry, entity, components, alloc, true);
            entity_obj.AddMember("components", components, alloc);
        }

        entities_arr.PushBack(entity_obj, alloc);
    }

    result.AddMember("entities", entities_arr, alloc);
    result.AddMember("entity_count",
        static_cast<int>(registry.storage<entt::entity>().size()), alloc);
    return MakeOk(std::move(result));
}

// ─── Helper: 解析 vec3 数组 ──────────────────────────────────────────────────

static glm::vec3 ParseVec3(const rapidjson::Value& arr, glm::vec3 def = glm::vec3(0.0f)) {
    if (!arr.IsArray() || arr.Size() < 3) return def;
    return glm::vec3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat());
}

static glm::vec4 ParseVec4(const rapidjson::Value& arr, glm::vec4 def = glm::vec4(1.0f)) {
    if (!arr.IsArray() || arr.Size() < 4) return def;
    return glm::vec4(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat());
}

// ─── Helper: 按类型名添加组件 ────────────────────────────────────────────────

static void AddComponentByType(entt::registry& registry, entt::entity entity,
                               const std::string& type,
                               const rapidjson::Value* props) {
    auto getF = [&](const char* key, float def) -> float {
        if (props && props->HasMember(key) && (*props)[key].IsNumber())
            return (*props)[key].GetFloat();
        return def;
    };
    auto getB = [&](const char* key, bool def) -> bool {
        if (props && props->HasMember(key) && (*props)[key].IsBool())
            return (*props)[key].GetBool();
        return def;
    };
    auto getS = [&](const char* key, const char* def) -> std::string {
        if (props && props->HasMember(key) && (*props)[key].IsString())
            return (*props)[key].GetString();
        return def;
    };

    if (type == "MeshRenderer" || type == "MeshRendererComponent") {
        if (!registry.all_of<MeshRendererComponent>(entity)) {
            auto& mr = registry.emplace<MeshRendererComponent>(entity);
            mr.mesh_path = getS("mesh_path", "");
            mr.shader_variant = getS("shader_variant", "MESH_PBR");
            mr.metallic = getF("metallic", 0.0f);
            mr.roughness = getF("roughness", 0.5f);
            if (props && props->HasMember("color"))
                mr.color = ParseVec4((*props)["color"]);
        }
    } else if (type == "Camera3D" || type == "Camera3DComponent") {
        if (!registry.all_of<Camera3DComponent>(entity)) {
            auto& cam = registry.emplace<Camera3DComponent>(entity);
            cam.fov = getF("fov", 60.0f);
            cam.near_clip = getF("near", 0.1f);
            cam.far_clip = getF("far", 1000.0f);
        }
    } else if (type == "DirectionalLight" || type == "DirectionalLight3DComponent") {
        if (!registry.all_of<DirectionalLight3DComponent>(entity)) {
            auto& dl = registry.emplace<DirectionalLight3DComponent>(entity);
            dl.intensity = getF("intensity", 1.0f);
            if (props && props->HasMember("color"))
                dl.color = ParseVec3((*props)["color"], glm::vec3(1.0f));
            if (props && props->HasMember("direction"))
                dl.direction = ParseVec3((*props)["direction"], dl.direction);
        }
    } else if (type == "PointLight" || type == "PointLightComponent") {
        if (!registry.all_of<PointLightComponent>(entity)) {
            auto& pl = registry.emplace<PointLightComponent>(entity);
            pl.intensity = getF("intensity", 1.0f);
            pl.radius = getF("range", 10.0f);
            if (props && props->HasMember("color"))
                pl.color = ParseVec3((*props)["color"], glm::vec3(1.0f));
        }
    } else if (type == "SpotLight" || type == "SpotLightComponent") {
        if (!registry.all_of<SpotLightComponent>(entity)) {
            auto& sl = registry.emplace<SpotLightComponent>(entity);
            sl.intensity = getF("intensity", 1.0f);
            sl.radius = getF("range", 10.0f);
            sl.inner_cone_angle = getF("inner_cone", 12.5f);
            sl.outer_cone_angle = getF("outer_cone", 17.5f);
        }
    } else if (type == "RigidBody3D" || type == "RigidBody3DComponent") {
        if (!registry.all_of<RigidBody3DComponent>(entity)) {
            auto& rb = registry.emplace<RigidBody3DComponent>(entity);
            rb.mass = getF("mass", 1.0f);
            std::string body_type = getS("body_type", "dynamic");
            if (body_type == "static") rb.type = RigidBody3DType::Static;
            else if (body_type == "kinematic") rb.type = RigidBody3DType::Kinematic;
            else rb.type = RigidBody3DType::Dynamic;
        }
    } else if (type == "BoxCollider3D" || type == "BoxCollider3DComponent") {
        if (!registry.all_of<BoxCollider3DComponent>(entity)) {
            auto& bc = registry.emplace<BoxCollider3DComponent>(entity);
            if (props && props->HasMember("size"))
                bc.size = ParseVec3((*props)["size"], glm::vec3(1.0f));
            bc.is_trigger = getB("is_trigger", false);
        }
    } else if (type == "SphereCollider3D" || type == "SphereCollider3DComponent") {
        if (!registry.all_of<SphereCollider3DComponent>(entity)) {
            auto& sc = registry.emplace<SphereCollider3DComponent>(entity);
            sc.radius = getF("radius", 0.5f);
            sc.is_trigger = getB("is_trigger", false);
        }
    } else if (type == "AudioSource" || type == "AudioSourceComponent") {
        if (!registry.all_of<AudioSourceComponent>(entity)) {
            auto& as = registry.emplace<AudioSourceComponent>(entity);
            as.loop = getB("loop", false);
            as.play_on_awake = getB("play_on_awake", true);
            as.volume = getF("volume", 1.0f);
        }
    } else if (type == "AudioListener" || type == "AudioListenerComponent") {
        if (!registry.all_of<AudioListenerComponent>(entity))
            registry.emplace<AudioListenerComponent>(entity);
    } else if (type == "SkyLight" || type == "SkyLightComponent") {
        if (!registry.all_of<SkyLightComponent>(entity))
            registry.emplace<SkyLightComponent>(entity);
    } else if (type == "Skybox" || type == "SkyboxComponent") {
        if (!registry.all_of<SkyboxComponent>(entity)) {
            auto& sb = registry.emplace<SkyboxComponent>(entity);
            sb.cubemap_path = getS("cubemap_path", "");
        }
    } else if (type == "PostProcess" || type == "PostProcessComponent") {
        if (!registry.all_of<PostProcessComponent>(entity))
            registry.emplace<PostProcessComponent>(entity);
    }
    // 其他未知类型静默忽略
}

static bool ModifyComponentProperties(entt::registry& registry, entt::entity entity,
                                       const std::string& type, const rapidjson::Value& props) {
    auto getF = [&](const char* key, float def) -> float {
        if (props.HasMember(key) && props[key].IsNumber()) return props[key].GetFloat();
        return def;
    };

    if (type == "MeshRenderer" || type == "MeshRendererComponent") {
        if (!registry.all_of<MeshRendererComponent>(entity)) return false;
        auto& mr = registry.get<MeshRendererComponent>(entity);
        if (props.HasMember("mesh_path") && props["mesh_path"].IsString())
            mr.mesh_path = props["mesh_path"].GetString();
        if (props.HasMember("shader_variant") && props["shader_variant"].IsString())
            mr.shader_variant = props["shader_variant"].GetString();
        if (props.HasMember("color") && props["color"].IsArray())
            mr.color = ParseVec4(props["color"]);
        if (props.HasMember("metallic")) mr.metallic = getF("metallic", mr.metallic);
        if (props.HasMember("roughness")) mr.roughness = getF("roughness", mr.roughness);
        return true;
    } else if (type == "Camera3D" || type == "Camera3DComponent") {
        if (!registry.all_of<Camera3DComponent>(entity)) return false;
        auto& c = registry.get<Camera3DComponent>(entity);
        if (props.HasMember("fov")) c.fov = getF("fov", c.fov);
        if (props.HasMember("near_clip")) c.near_clip = getF("near_clip", c.near_clip);
        if (props.HasMember("far_clip")) c.far_clip = getF("far_clip", c.far_clip);
        return true;
    } else if (type == "DirectionalLight" || type == "DirectionalLight3DComponent") {
        if (!registry.all_of<DirectionalLight3DComponent>(entity)) return false;
        auto& dl = registry.get<DirectionalLight3DComponent>(entity);
        if (props.HasMember("intensity")) dl.intensity = getF("intensity", dl.intensity);
        if (props.HasMember("color") && props["color"].IsArray())
            dl.color = ParseVec3(props["color"], dl.color);
        if (props.HasMember("direction") && props["direction"].IsArray())
            dl.direction = ParseVec3(props["direction"], dl.direction);
        return true;
    } else if (type == "PointLight" || type == "PointLightComponent") {
        if (!registry.all_of<PointLightComponent>(entity)) return false;
        auto& pl = registry.get<PointLightComponent>(entity);
        if (props.HasMember("intensity")) pl.intensity = getF("intensity", pl.intensity);
        if (props.HasMember("range")) pl.radius = getF("range", pl.radius);
        if (props.HasMember("color") && props["color"].IsArray())
            pl.color = ParseVec3(props["color"], pl.color);
        return true;
    } else if (type == "SpotLight" || type == "SpotLightComponent") {
        if (!registry.all_of<SpotLightComponent>(entity)) return false;
        auto& sl = registry.get<SpotLightComponent>(entity);
        if (props.HasMember("intensity")) sl.intensity = getF("intensity", sl.intensity);
        if (props.HasMember("range")) sl.radius = getF("range", sl.radius);
        if (props.HasMember("inner_cone")) sl.inner_cone_angle = getF("inner_cone", sl.inner_cone_angle);
        if (props.HasMember("outer_cone")) sl.outer_cone_angle = getF("outer_cone", sl.outer_cone_angle);
        return true;
    } else if (type == "RigidBody3D" || type == "RigidBody3DComponent") {
        if (!registry.all_of<RigidBody3DComponent>(entity)) return false;
        auto& rb = registry.get<RigidBody3DComponent>(entity);
        if (props.HasMember("mass")) rb.mass = getF("mass", rb.mass);
        if (props.HasMember("body_type") && props["body_type"].IsString()) {
            std::string bt = props["body_type"].GetString();
            if (bt == "static") rb.type = RigidBody3DType::Static;
            else if (bt == "kinematic") rb.type = RigidBody3DType::Kinematic;
            else rb.type = RigidBody3DType::Dynamic;
        }
        return true;
    } else if (type == "AudioSource" || type == "AudioSourceComponent") {
        if (!registry.all_of<AudioSourceComponent>(entity)) return false;
        auto& as = registry.get<AudioSourceComponent>(entity);
        if (props.HasMember("volume")) as.volume = getF("volume", as.volume);
        if (props.HasMember("pitch")) as.pitch = getF("pitch", as.pitch);
        if (props.HasMember("loop") && props["loop"].IsBool()) as.loop = props["loop"].GetBool();
        return true;
    }
    return false;
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
        transform.position = ParseVec3(params["position"]);
    }
    if (params.HasMember("scale") && params["scale"].IsArray() && params["scale"].Size() >= 3) {
        transform.scale = ParseVec3(params["scale"], glm::vec3(1.0f));
    }
    if (params.HasMember("rotation") && params["rotation"].IsArray() && params["rotation"].Size() >= 3) {
        const auto& r = params["rotation"];
        transform.rotation = glm::quat(glm::vec3(
            glm::radians(r[0].GetFloat()),
            glm::radians(r[1].GetFloat()),
            glm::radians(r[2].GetFloat())));
    }
    registry.emplace<TransformComponent>(entity, transform);

    // mesh 快捷参数 → 自动添加 MeshRendererComponent
    if (params.HasMember("mesh") && params["mesh"].IsString()) {
        if (!registry.all_of<MeshRendererComponent>(entity)) {
            auto& mr = registry.emplace<MeshRendererComponent>(entity);
            mr.mesh_path = params["mesh"].GetString();
            mr.shader_variant = "MESH_PBR";
            if (params.HasMember("color") && params["color"].IsArray())
                mr.color = ParseVec4(params["color"]);
        }
    }

    // components 数组 → 批量添加
    if (params.HasMember("components") && params["components"].IsArray()) {
        for (auto& comp : params["components"].GetArray()) {
            std::string comp_type;
            const rapidjson::Value* comp_props = nullptr;

            if (comp.IsString()) {
                comp_type = comp.GetString();
            } else if (comp.IsObject()) {
                if (comp.HasMember("type") && comp["type"].IsString()) {
                    comp_type = comp["type"].GetString();
                }
                if (comp.HasMember("properties") && comp["properties"].IsObject()) {
                    comp_props = &comp["properties"];
                }
            }

            if (!comp_type.empty()) {
                AddComponentByType(registry, entity, comp_type, comp_props);
            }
        }
    }

    // 构建返回值：列出实际添加的组件
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("name", rapidjson::Value(name_comp.name.c_str(), alloc), alloc);

    rapidjson::Value added_comps(rapidjson::kArrayType);
    added_comps.PushBack("Transform", alloc);
    if (registry.all_of<MeshRendererComponent>(entity))     added_comps.PushBack("MeshRenderer", alloc);
    if (registry.all_of<Camera3DComponent>(entity))          added_comps.PushBack("Camera3D", alloc);
    if (registry.all_of<DirectionalLight3DComponent>(entity)) added_comps.PushBack("DirectionalLight", alloc);
    if (registry.all_of<PointLightComponent>(entity))        added_comps.PushBack("PointLight", alloc);
    if (registry.all_of<SpotLightComponent>(entity))         added_comps.PushBack("SpotLight", alloc);
    if (registry.all_of<RigidBody3DComponent>(entity))       added_comps.PushBack("RigidBody3D", alloc);
    if (registry.all_of<BoxCollider3DComponent>(entity))     added_comps.PushBack("BoxCollider3D", alloc);
    if (registry.all_of<SphereCollider3DComponent>(entity))  added_comps.PushBack("SphereCollider3D", alloc);
    if (registry.all_of<AudioSourceComponent>(entity))       added_comps.PushBack("AudioSource", alloc);
    if (registry.all_of<AudioListenerComponent>(entity))     added_comps.PushBack("AudioListener", alloc);
    if (registry.all_of<SkyLightComponent>(entity))          added_comps.PushBack("SkyLight", alloc);
    if (registry.all_of<SkyboxComponent>(entity))            added_comps.PushBack("Skybox", alloc);
    if (registry.all_of<PostProcessComponent>(entity))       added_comps.PushBack("PostProcess", alloc);
    result.AddMember("components", added_comps, alloc);

    // Undo: destroy the created entity
    {
        entt::entity created = entity;
        auto& reg = registry;
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Create Entity (RPC)",
            []() {},
            [created, &reg]() { if (reg.valid(created)) reg.destroy(created); }
        ));
    }

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

    // Snapshot name + transform for undo reconstruction
    std::string saved_name;
    TransformComponent saved_tf;
    bool has_tf = false;
    if (registry.all_of<EditorNameComponent>(entity))
        saved_name = registry.get<EditorNameComponent>(entity).name;
    if (registry.all_of<TransformComponent>(entity)) {
        saved_tf = registry.get<TransformComponent>(entity);
        has_tf = true;
    }

    registry.destroy(entity);

    // Undo: recreate entity with name + transform
    {
        auto& w = engine.pipeline()->world();
        auto& reg = registry;
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Delete Entity (RPC)",
            []() {},
            [saved_name, saved_tf, has_tf, &w, &reg]() {
                auto e = w.CreateEntity();
                if (!saved_name.empty()) reg.emplace<EditorNameComponent>(e, saved_name);
                if (has_tf) reg.emplace<TransformComponent>(e, saved_tf);
            }
        ));
    }

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
        dse::editor::GetEditorState() == dse::editor::EditorState::Play ? "play" :
        dse::editor::GetEditorState() == dse::editor::EditorState::Pause ? "pause" : "edit";
    result.AddMember("editor_state", rapidjson::Value(state_str, alloc), alloc);
    result.AddMember("entity_count",
        static_cast<int>(registry.storage<entt::entity>().size()), alloc);

    auto* am = engine.asset_manager();
    if (am) {
        std::string dr = am->GetDataRoot();
        result.AddMember("data_root", rapidjson::Value(dr.c_str(), alloc), alloc);
    }
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

// ─── Tool: dsengine_editor_play ─────────────────────────────────────────────

static JsonRpcResponse HandleEditorPlay(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    if (dse::editor::GetEditorState() != dse::editor::EditorState::Edit) {
        return MakeToolError(-32603, "Already in play/pause mode");
    }

    auto& registry = engine.pipeline()->world().registry();
    dse::editor::EnterPlayMode(registry);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("editor_state", "play", alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_stop ─────────────────────────────────────────────

static JsonRpcResponse HandleEditorStop(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    if (dse::editor::GetEditorState() == dse::editor::EditorState::Edit) {
        return MakeToolError(-32603, "Not in play mode");
    }

    auto& registry = engine.pipeline()->world().registry();
    entt::entity dummy = entt::null;
    dse::editor::ExitPlayMode(registry, dummy);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("editor_state", "edit", alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_undo ─────────────────────────────────────────────

static JsonRpcResponse HandleEditorUndo(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& mgr = GetUndoRedoManager();
    bool ok = mgr.CanUndo() ? mgr.Undo() : false;

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("success", ok, alloc);
    if (mgr.CanUndo()) {
        result.AddMember("next_undo",
            rapidjson::Value(mgr.GetUndoDescription().c_str(), alloc), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_redo ─────────────────────────────────────────────

static JsonRpcResponse HandleEditorRedo(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& mgr = GetUndoRedoManager();
    bool ok = mgr.CanRedo() ? mgr.Redo() : false;

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("success", ok, alloc);
    if (mgr.CanRedo()) {
        result.AddMember("next_redo",
            rapidjson::Value(mgr.GetRedoDescription().c_str(), alloc), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_modify ───────────────────────────────────────────

static JsonRpcResponse HandleEntityModify(
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

    // name
    if (params.HasMember("name") && params["name"].IsString()) {
        if (registry.all_of<EditorNameComponent>(entity)) {
            registry.get<EditorNameComponent>(entity).name = params["name"].GetString();
        } else {
            registry.emplace<EditorNameComponent>(entity).name = params["name"].GetString();
        }
    }

    // position / rotation / scale
    if (registry.all_of<TransformComponent>(entity)) {
        auto& t = registry.get<TransformComponent>(entity);
        if (params.HasMember("position") && params["position"].IsArray() && params["position"].Size() >= 3) {
            const auto& p = params["position"];
            t.position = glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat());
            t.dirty = true;
        }
        if (params.HasMember("rotation") && params["rotation"].IsArray() && params["rotation"].Size() >= 3) {
            const auto& r = params["rotation"];
            t.rotation = glm::quat(glm::vec3(
                glm::radians(r[0].GetFloat()),
                glm::radians(r[1].GetFloat()),
                glm::radians(r[2].GetFloat())));
            t.dirty = true;
        }
        if (params.HasMember("scale") && params["scale"].IsArray() && params["scale"].Size() >= 3) {
            const auto& s = params["scale"];
            t.scale = glm::vec3(s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat());
            t.dirty = true;
        }
    } else if (params.HasMember("position") || params.HasMember("rotation") || params.HasMember("scale")) {
        TransformComponent t;
        if (params.HasMember("position") && params["position"].IsArray() && params["position"].Size() >= 3) {
            const auto& p = params["position"];
            t.position = glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat());
        }
        if (params.HasMember("rotation") && params["rotation"].IsArray() && params["rotation"].Size() >= 3) {
            const auto& r = params["rotation"];
            t.rotation = glm::quat(glm::vec3(
                glm::radians(r[0].GetFloat()),
                glm::radians(r[1].GetFloat()),
                glm::radians(r[2].GetFloat())));
        }
        if (params.HasMember("scale") && params["scale"].IsArray() && params["scale"].Size() >= 3) {
            const auto& s = params["scale"];
            t.scale = glm::vec3(s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat());
        }
        registry.emplace<TransformComponent>(entity, t);
    }

    // modify_component: 修改已存在组件的属性
    rapidjson::Value modified_comps(rapidjson::kArrayType);
    rapidjson::Document::AllocatorType temp_alloc;  // 临时分配器

    if (params.HasMember("modify_component") && params["modify_component"].IsObject()) {
        const auto& mc = params["modify_component"];
        if (mc.HasMember("type") && mc["type"].IsString() &&
            mc.HasMember("properties") && mc["properties"].IsObject()) {
            std::string ctype = mc["type"].GetString();
            if (ModifyComponentProperties(registry, entity, ctype, mc["properties"])) {
                modified_comps.PushBack(rapidjson::Value(ctype.c_str(), temp_alloc), temp_alloc);
            }
        }
    }
    if (params.HasMember("modify_components") && params["modify_components"].IsArray()) {
        for (auto& item : params["modify_components"].GetArray()) {
            if (item.IsObject() && item.HasMember("type") && item["type"].IsString() &&
                item.HasMember("properties") && item["properties"].IsObject()) {
                std::string ctype = item["type"].GetString();
                if (ModifyComponentProperties(registry, entity, ctype, item["properties"])) {
                    modified_comps.PushBack(rapidjson::Value(ctype.c_str(), temp_alloc), temp_alloc);
                }
            }
        }
    }

    // add_components: 批量添加组件（复用 AddComponentByType）
    std::vector<std::string> added_list;
    if (params.HasMember("add_components") && params["add_components"].IsArray()) {
        for (auto& item : params["add_components"].GetArray()) {
            std::string ctype;
            const rapidjson::Value* props = nullptr;
            if (item.IsString()) {
                ctype = item.GetString();
            } else if (item.IsObject() && item.HasMember("type") && item["type"].IsString()) {
                ctype = item["type"].GetString();
                if (item.HasMember("properties") && item["properties"].IsObject())
                    props = &item["properties"];
            }
            if (!ctype.empty()) {
                AddComponentByType(registry, entity, ctype, props);
                added_list.push_back(ctype);
            }
        }
    }

    // remove_components: 批量移除组件（复用 RemoveComponentByType）
    std::vector<std::string> removed_list;
    if (params.HasMember("remove_components") && params["remove_components"].IsArray()) {
        for (auto& item : params["remove_components"].GetArray()) {
            if (item.IsString()) {
                std::string ctype = item.GetString();
                if (RemoveComponentByType(registry, entity, ctype)) {
                    removed_list.push_back(ctype);
                }
            }
        }
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("modified", rapidjson::Value(true), alloc);
    if (modified_comps.Size() > 0) {
        rapidjson::Value comps_copy(rapidjson::kArrayType);
        for (auto& v : modified_comps.GetArray()) {
            comps_copy.PushBack(rapidjson::Value(v.GetString(), alloc), alloc);
        }
        result.AddMember("modified_components", comps_copy, alloc);
    }
    if (!added_list.empty()) {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (auto& s : added_list) arr.PushBack(rapidjson::Value(s.c_str(), alloc), alloc);
        result.AddMember("added_components", arr, alloc);
    }
    if (!removed_list.empty()) {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (auto& s : removed_list) arr.PushBack(rapidjson::Value(s.c_str(), alloc), alloc);
        result.AddMember("removed_components", arr, alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_add_component ────────────────────────────────────

static JsonRpcResponse HandleEntityAddComponent(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }
    if (!params.HasMember("type") || !params["type"].IsString()) {
        return MakeToolError(-32602, "Missing required param: type (string)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    std::string comp_type = params["type"].GetString();
    const rapidjson::Value* props = nullptr;
    if (params.HasMember("properties") && params["properties"].IsObject()) {
        props = &params["properties"];
    }

    AddComponentByType(registry, entity, comp_type, props);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("component", rapidjson::Value(comp_type.c_str(), alloc), alloc);
    result.AddMember("added", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_remove_component ─────────────────────────────────

static bool RemoveComponentByType(entt::registry& registry, entt::entity entity,
                                   const std::string& type) {
    if (type == "MeshRenderer" || type == "MeshRendererComponent") {
        if (registry.all_of<MeshRendererComponent>(entity)) { registry.remove<MeshRendererComponent>(entity); return true; }
    } else if (type == "Camera3D" || type == "Camera3DComponent") {
        if (registry.all_of<Camera3DComponent>(entity)) { registry.remove<Camera3DComponent>(entity); return true; }
    } else if (type == "DirectionalLight" || type == "DirectionalLight3DComponent") {
        if (registry.all_of<DirectionalLight3DComponent>(entity)) { registry.remove<DirectionalLight3DComponent>(entity); return true; }
    } else if (type == "PointLight" || type == "PointLightComponent") {
        if (registry.all_of<PointLightComponent>(entity)) { registry.remove<PointLightComponent>(entity); return true; }
    } else if (type == "SpotLight" || type == "SpotLightComponent") {
        if (registry.all_of<SpotLightComponent>(entity)) { registry.remove<SpotLightComponent>(entity); return true; }
    } else if (type == "RigidBody3D" || type == "RigidBody3DComponent") {
        if (registry.all_of<RigidBody3DComponent>(entity)) { registry.remove<RigidBody3DComponent>(entity); return true; }
    } else if (type == "BoxCollider3D" || type == "BoxCollider3DComponent") {
        if (registry.all_of<BoxCollider3DComponent>(entity)) { registry.remove<BoxCollider3DComponent>(entity); return true; }
    } else if (type == "SphereCollider3D" || type == "SphereCollider3DComponent") {
        if (registry.all_of<SphereCollider3DComponent>(entity)) { registry.remove<SphereCollider3DComponent>(entity); return true; }
    } else if (type == "AudioSource" || type == "AudioSourceComponent") {
        if (registry.all_of<AudioSourceComponent>(entity)) { registry.remove<AudioSourceComponent>(entity); return true; }
    } else if (type == "AudioListener" || type == "AudioListenerComponent") {
        if (registry.all_of<AudioListenerComponent>(entity)) { registry.remove<AudioListenerComponent>(entity); return true; }
    } else if (type == "SkyLight" || type == "SkyLightComponent") {
        if (registry.all_of<SkyLightComponent>(entity)) { registry.remove<SkyLightComponent>(entity); return true; }
    } else if (type == "Skybox" || type == "SkyboxComponent") {
        if (registry.all_of<SkyboxComponent>(entity)) { registry.remove<SkyboxComponent>(entity); return true; }
    } else if (type == "PostProcess" || type == "PostProcessComponent") {
        if (registry.all_of<PostProcessComponent>(entity)) { registry.remove<PostProcessComponent>(entity); return true; }
    }
    return false;
}

static JsonRpcResponse HandleEntityRemoveComponent(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }
    if (!params.HasMember("type") || !params["type"].IsString()) {
        return MakeToolError(-32602, "Missing required param: type (string)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    std::string comp_type = params["type"].GetString();
    bool removed = RemoveComponentByType(registry, entity, comp_type);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("component", rapidjson::Value(comp_type.c_str(), alloc), alloc);
    result.AddMember("removed", rapidjson::Value(removed), alloc);
    if (!removed) {
        result.AddMember("reason", "Component not found on entity", alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Helper: 收集实体上的组件列表 ───────────────────────────────────────────

static void CollectEntityComponents(entt::registry& registry, entt::entity entity,
                                     rapidjson::Value& arr, rapidjson::Document::AllocatorType& alloc,
                                     bool include_properties) {
    auto addComp = [&](const char* type_name) {
        if (include_properties) {
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", rapidjson::Value(type_name, alloc), alloc);
            arr.PushBack(comp, alloc);
        } else {
            arr.PushBack(rapidjson::Value(type_name, alloc), alloc);
        }
    };

    if (registry.all_of<TransformComponent>(entity)) {
        if (include_properties) {
            const auto& t = registry.get<TransformComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Transform", alloc);
            rapidjson::Value pos(rapidjson::kArrayType);
            pos.PushBack(t.position.x, alloc).PushBack(t.position.y, alloc).PushBack(t.position.z, alloc);
            comp.AddMember("position", pos, alloc);
            rapidjson::Value rot(rapidjson::kArrayType);
            glm::vec3 euler = glm::degrees(glm::eulerAngles(t.rotation));
            rot.PushBack(euler.x, alloc).PushBack(euler.y, alloc).PushBack(euler.z, alloc);
            comp.AddMember("rotation", rot, alloc);
            rapidjson::Value scl(rapidjson::kArrayType);
            scl.PushBack(t.scale.x, alloc).PushBack(t.scale.y, alloc).PushBack(t.scale.z, alloc);
            comp.AddMember("scale", scl, alloc);
            arr.PushBack(comp, alloc);
        } else {
            arr.PushBack(rapidjson::Value("Transform", alloc), alloc);
        }
    }
    if (registry.all_of<MeshRendererComponent>(entity)) {
        if (include_properties) {
            const auto& mr = registry.get<MeshRendererComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "MeshRenderer", alloc);
            comp.AddMember("mesh_path", rapidjson::Value(mr.mesh_path.c_str(), alloc), alloc);
            comp.AddMember("shader_variant", rapidjson::Value(mr.shader_variant.c_str(), alloc), alloc);
            comp.AddMember("metallic", mr.metallic, alloc);
            comp.AddMember("roughness", mr.roughness, alloc);
            rapidjson::Value col(rapidjson::kArrayType);
            col.PushBack(mr.color.r, alloc).PushBack(mr.color.g, alloc).PushBack(mr.color.b, alloc).PushBack(mr.color.a, alloc);
            comp.AddMember("color", col, alloc);
            arr.PushBack(comp, alloc);
        } else {
            arr.PushBack(rapidjson::Value("MeshRenderer", alloc), alloc);
        }
    }
    if (registry.all_of<Camera3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<Camera3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Camera3D", alloc);
            comp.AddMember("fov", c.fov, alloc);
            comp.AddMember("near_clip", c.near_clip, alloc);
            comp.AddMember("far_clip", c.far_clip, alloc);
            arr.PushBack(comp, alloc);
        } else {
            arr.PushBack(rapidjson::Value("Camera3D", alloc), alloc);
        }
    }
    if (registry.all_of<DirectionalLight3DComponent>(entity))  addComp("DirectionalLight");
    if (registry.all_of<PointLightComponent>(entity))          addComp("PointLight");
    if (registry.all_of<SpotLightComponent>(entity))           addComp("SpotLight");
    if (registry.all_of<RigidBody3DComponent>(entity))         addComp("RigidBody3D");
    if (registry.all_of<BoxCollider3DComponent>(entity))       addComp("BoxCollider3D");
    if (registry.all_of<SphereCollider3DComponent>(entity))    addComp("SphereCollider3D");
    if (registry.all_of<AudioSourceComponent>(entity))         addComp("AudioSource");
    if (registry.all_of<AudioListenerComponent>(entity))       addComp("AudioListener");
    if (registry.all_of<SkyLightComponent>(entity))            addComp("SkyLight");
    if (registry.all_of<SkyboxComponent>(entity))              addComp("Skybox");
    if (registry.all_of<PostProcessComponent>(entity))         addComp("PostProcess");
    if (registry.all_of<SpriteRendererComponent>(entity))      addComp("SpriteRenderer");
    if (registry.all_of<Animator3DComponent>(entity))          addComp("Animator3D");
    if (registry.all_of<WaterComponent>(entity))               addComp("Water");
    if (registry.all_of<TerrainComponent>(entity))             addComp("Terrain");
    if (registry.all_of<DecalComponent>(entity))               addComp("Decal");
}

// ─── Tool: dsengine_entity_get_components ───────────────────────────────────

static JsonRpcResponse HandleEntityGetComponents(
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

    bool detailed = true;
    if (params.HasMember("detailed") && params["detailed"].IsBool()) {
        detailed = params["detailed"].GetBool();
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);

    if (registry.all_of<EditorNameComponent>(entity)) {
        result.AddMember("name",
            rapidjson::Value(registry.get<EditorNameComponent>(entity).name.c_str(), alloc), alloc);
    }

    rapidjson::Value comps(rapidjson::kArrayType);
    CollectEntityComponents(registry, entity, comps, alloc, detailed);
    result.AddMember("components", comps, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_save ──────────────────────────────────────────────

static JsonRpcResponse HandleSceneSave(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    std::string path;
    if (params.HasMember("path") && params["path"].IsString()) {
        path = params["path"].GetString();
    } else {
        path = GetCurrentScenePath();
        if (path.empty() || path == "Untitled") {
            return MakeToolError(-32602, "No path specified and no current scene file");
        }
    }

    auto& registry = engine.pipeline()->world().registry();
    SaveScene(registry, path);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("saved", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_load ──────────────────────────────────────────────

static JsonRpcResponse HandleSceneLoad(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    std::string path = params["path"].GetString();
    if (!std::filesystem::exists(path)) {
        return MakeToolError(-32602, "Scene file not found: " + path);
    }

    auto& registry = engine.pipeline()->world().registry();
    LoadScene(registry, path);
    SetCurrentScenePath(path);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("loaded", rapidjson::Value(true), alloc);
    result.AddMember("entity_count",
        static_cast<int>(registry.storage<entt::entity>().size()), alloc);
    return MakeOk(std::move(result));
}

// ─── Helper: Base64 编码 ────────────────────────────────────────────────────

static const char kBase64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<unsigned int>(data[i + 2]);
        out.push_back(kBase64Table[(n >> 18) & 0x3F]);
        out.push_back(kBase64Table[(n >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kBase64Table[(n >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? kBase64Table[n & 0x3F] : '=');
    }
    return out;
}

// stb_image_write 回调：将 PNG 数据追加到 vector
static void StbWriteCallback(void* context, void* data, int size) {
    auto* buf = static_cast<std::vector<unsigned char>*>(context);
    auto* bytes = static_cast<unsigned char*>(data);
    buf->insert(buf->end(), bytes, bytes + size);
}

// ─── Tool: dsengine_editor_screenshot ───────────────────────────────────────

static JsonRpcResponse HandleEditorScreenshot(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    std::string target = "scene";
    if (params.HasMember("target") && params["target"].IsString()) {
        target = params["target"].GetString();
    }

    RenderTargetReadback readback;
    if (target == "game") {
        readback = engine.pipeline()->ReadMainColorRgba8WithSize();
    } else {
        readback = engine.pipeline()->ReadSceneColorRgba8WithSize();
    }

    if (readback.pixels.empty() || readback.width <= 0 || readback.height <= 0) {
        return MakeToolError(-32603, "Failed to read framebuffer (no active render target)");
    }

    // Y-flip if needed (OpenGL)
    if (engine.pipeline()->NeedsReadbackYFlip()) {
        const int stride = readback.width * 4;
        std::vector<unsigned char> row(stride);
        for (int y = 0; y < readback.height / 2; ++y) {
            unsigned char* top = readback.pixels.data() + y * stride;
            unsigned char* bot = readback.pixels.data() + (readback.height - 1 - y) * stride;
            std::memcpy(row.data(), top, stride);
            std::memcpy(top, bot, stride);
            std::memcpy(bot, row.data(), stride);
        }
    }

    // Encode to PNG in memory
    std::vector<unsigned char> png_buf;
    png_buf.reserve(readback.width * readback.height); // rough estimate
    int ok = stbi_write_png_to_func(
        StbWriteCallback, &png_buf,
        readback.width, readback.height, 4,
        readback.pixels.data(), readback.width * 4);

    if (!ok || png_buf.empty()) {
        return MakeToolError(-32603, "PNG encoding failed");
    }

    std::string b64 = Base64Encode(png_buf.data(), png_buf.size());

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("width", readback.width, alloc);
    result.AddMember("height", readback.height, alloc);
    result.AddMember("format", "png", alloc);
    result.AddMember("encoding", "base64", alloc);
    result.AddMember("data", rapidjson::Value(b64.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_asset_import ─────────────────────────────────────────────

static JsonRpcResponse HandleAssetImport(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }
    std::string path = params["path"].GetString();

    std::string type = "auto";
    if (params.HasMember("type") && params["type"].IsString()) {
        type = params["type"].GetString();
    }

    auto* am = engine.asset_manager();
    if (!am) {
        return MakeToolError(-32603, "AssetManager not available");
    }

    // auto-detect type from extension
    if (type == "auto") {
        auto ext_pos = path.rfind('.');
        if (ext_pos != std::string::npos) {
            std::string ext = path.substr(ext_pos);
            for (auto& c : ext) c = static_cast<char>(std::tolower(c));
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
                ext == ".tga" || ext == ".dds" || ext == ".hdr" || ext == ".ppm")
                type = "texture";
            else if (ext == ".dmesh")
                type = "mesh";
            else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac")
                type = "audio";
            else if (ext == ".dmat")
                type = "material";
        }
        if (type == "auto") {
            return MakeToolError(-32602, "Cannot auto-detect asset type for: " + path);
        }
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (type == "texture") {
        auto tex = am->LoadTexture(path);
        if (!tex) return MakeToolError(-32603, "Failed to load texture: " + path);
        result.AddMember("type", "texture", alloc);
        result.AddMember("handle", tex->GetHandle(), alloc);
        result.AddMember("width", tex->GetWidth(), alloc);
        result.AddMember("height", tex->GetHeight(), alloc);
        result.AddMember("channels", tex->GetChannels(), alloc);
    } else if (type == "mesh") {
        auto mesh = am->LoadDmesh(path);
        if (!mesh) return MakeToolError(-32603, "Failed to load mesh: " + path);
        result.AddMember("type", "mesh", alloc);
        result.AddMember("path", rapidjson::Value(mesh->GetPath().c_str(), alloc), alloc);
        result.AddMember("size_bytes", static_cast<uint64_t>(mesh->GetData().size()), alloc);
    } else if (type == "audio") {
        auto clip = am->LoadAudioClip(path);
        if (!clip) return MakeToolError(-32603, "Failed to load audio: " + path);
        result.AddMember("type", "audio", alloc);
        result.AddMember("path", rapidjson::Value(clip->GetPath().c_str(), alloc), alloc);
        result.AddMember("size_bytes", static_cast<uint64_t>(clip->GetData().size()), alloc);
    } else if (type == "material") {
        std::size_t idx = 0;
        if (params.HasMember("material_index") && params["material_index"].IsUint())
            idx = params["material_index"].GetUint();
        auto mat = am->LoadMaterialInstanceFromDmat(path, idx);
        if (!mat) return MakeToolError(-32603, "Failed to load material: " + path);
        result.AddMember("type", "material", alloc);
        result.AddMember("material_id", mat->GetId(), alloc);
        result.AddMember("name", rapidjson::Value(mat->GetName().c_str(), alloc), alloc);
    } else {
        return MakeToolError(-32602, "Unknown asset type: " + type);
    }

    result.AddMember("success", true, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_material_create ─────────────────────────────────────────

static JsonRpcResponse HandleMaterialCreate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    std::string name = "untitled_material";
    if (params.HasMember("name") && params["name"].IsString()) {
        name = params["name"].GetString();
    }

    auto* am = engine.asset_manager();
    if (!am) {
        return MakeToolError(-32603, "AssetManager not available");
    }

    // Build .dmat JSON
    rapidjson::Document dmat;
    dmat.SetObject();
    auto& da = dmat.GetAllocator();

    rapidjson::Value mat_obj(rapidjson::kObjectType);
    mat_obj.AddMember("name", rapidjson::Value(name.c_str(), da), da);

    // shader_variant
    std::string shader_variant = "MESH_PBR";
    if (params.HasMember("shader_variant") && params["shader_variant"].IsString())
        shader_variant = params["shader_variant"].GetString();
    mat_obj.AddMember("shader_variant", rapidjson::Value(shader_variant.c_str(), da), da);

    // base_color [r,g,b,a]
    if (params.HasMember("base_color") && params["base_color"].IsArray()) {
        rapidjson::Value bc(rapidjson::kArrayType);
        for (auto& v : params["base_color"].GetArray()) bc.PushBack(v.GetFloat(), da);
        mat_obj.AddMember("base_color", bc, da);
    } else {
        rapidjson::Value bc(rapidjson::kArrayType);
        bc.PushBack(1.0f, da).PushBack(1.0f, da).PushBack(1.0f, da).PushBack(1.0f, da);
        mat_obj.AddMember("base_color", bc, da);
    }

    // emissive [r,g,b]
    if (params.HasMember("emissive") && params["emissive"].IsArray()) {
        rapidjson::Value em(rapidjson::kArrayType);
        for (auto& v : params["emissive"].GetArray()) em.PushBack(v.GetFloat(), da);
        mat_obj.AddMember("emissive", em, da);
    }

    // scalars
    auto addFloat = [&](const char* key) {
        if (params.HasMember(key) && params[key].IsNumber())
            mat_obj.AddMember(rapidjson::Value(key, da),
                              rapidjson::Value(params[key].GetFloat()), da);
    };
    addFloat("metallic");
    addFloat("roughness");
    addFloat("occlusion_strength");
    addFloat("normal_scale");
    addFloat("alpha_cutoff");

    if (params.HasMember("alpha_test") && params["alpha_test"].IsBool())
        mat_obj.AddMember("alpha_test", params["alpha_test"].GetBool(), da);
    if (params.HasMember("double_sided") && params["double_sided"].IsBool())
        mat_obj.AddMember("double_sided", params["double_sided"].GetBool(), da);

    // texture paths
    auto addTex = [&](const char* key) {
        if (params.HasMember(key) && params[key].IsString())
            mat_obj.AddMember(rapidjson::Value(key, da),
                              rapidjson::Value(params[key].GetString(), da), da);
    };
    addTex("base_color_texture");
    addTex("normal_texture");
    addTex("metallic_roughness_texture");
    addTex("emissive_texture");
    addTex("occlusion_texture");

    rapidjson::Value materials_arr(rapidjson::kArrayType);
    materials_arr.PushBack(mat_obj, da);
    dmat.AddMember("materials", materials_arr, da);

    // Serialize to JSON string
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    dmat.Accept(writer);
    std::string json_str = sb.GetString();

    // Save to file
    std::string save_path;
    if (params.HasMember("save_path") && params["save_path"].IsString()) {
        save_path = params["save_path"].GetString();
    } else {
        std::string data_root = am->GetDataRoot();
        save_path = data_root + "/materials/" + name + ".dmat";
    }

    // Ensure directory exists
    std::filesystem::path file_path(save_path);
    if (file_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(file_path.parent_path(), ec);
    }

    {
        std::ofstream f(save_path, std::ios::binary);
        if (!f.is_open()) {
            return MakeToolError(-32603, "Failed to write material file: " + save_path);
        }
        f.write(json_str.data(), static_cast<std::streamsize>(json_str.size()));
    }

    // Load into engine
    auto loaded = am->LoadMaterialInstanceFromDmat(save_path, 0);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("success", true, alloc);
    result.AddMember("file_path", rapidjson::Value(save_path.c_str(), alloc), alloc);
    if (loaded) {
        result.AddMember("material_id", loaded->GetId(), alloc);
        result.AddMember("material_name", rapidjson::Value(loaded->GetName().c_str(), alloc), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_get_state ────────────────────────────────────────

static JsonRpcResponse HandleEntityGetState(
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

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);

    if (registry.all_of<EditorNameComponent>(entity)) {
        result.AddMember("name",
            rapidjson::Value(registry.get<EditorNameComponent>(entity).name.c_str(), alloc), alloc);
    }

    if (registry.all_of<TransformComponent>(entity)) {
        const auto& t = registry.get<TransformComponent>(entity);
        rapidjson::Value tf(rapidjson::kObjectType);
        rapidjson::Value pos(rapidjson::kArrayType);
        pos.PushBack(t.position.x, alloc); pos.PushBack(t.position.y, alloc); pos.PushBack(t.position.z, alloc);
        tf.AddMember("position", pos, alloc);
        rapidjson::Value rot(rapidjson::kArrayType);
        rot.PushBack(t.rotation.x, alloc); rot.PushBack(t.rotation.y, alloc);
        rot.PushBack(t.rotation.z, alloc); rot.PushBack(t.rotation.w, alloc);
        tf.AddMember("rotation", rot, alloc);
        rapidjson::Value scl(rapidjson::kArrayType);
        scl.PushBack(t.scale.x, alloc); scl.PushBack(t.scale.y, alloc); scl.PushBack(t.scale.z, alloc);
        tf.AddMember("scale", scl, alloc);
        result.AddMember("transform", tf, alloc);
    }

    rapidjson::Value comps(rapidjson::kArrayType);
    CollectEntityComponents(registry, entity, comps, alloc, true);
    result.AddMember("components", comps, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_duplicate ────────────────────────────────────────

static JsonRpcResponse HandleEntityDuplicate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }

    auto src = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& world = engine.pipeline()->world();
    auto& registry = world.registry();

    if (!registry.valid(src)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    auto dst = world.CreateEntity();

    std::string new_name = "Copy";
    if (registry.all_of<EditorNameComponent>(src))
        new_name = registry.get<EditorNameComponent>(src).name + " (Copy)";
    registry.emplace<EditorNameComponent>(dst, new_name);

    if (registry.all_of<TransformComponent>(src)) {
        auto tf = registry.get<TransformComponent>(src);
        tf.position += glm::vec3(0.5f, 0.0f, 0.5f);
        tf.dirty = true;
        registry.emplace<TransformComponent>(dst, tf);
    } else {
        registry.emplace<TransformComponent>(dst);
    }

    if (registry.all_of<MeshRendererComponent>(src))
        registry.emplace<MeshRendererComponent>(dst, registry.get<MeshRendererComponent>(src));
    if (registry.all_of<Camera3DComponent>(src))
        registry.emplace<Camera3DComponent>(dst, registry.get<Camera3DComponent>(src));
    if (registry.all_of<DirectionalLight3DComponent>(src))
        registry.emplace<DirectionalLight3DComponent>(dst, registry.get<DirectionalLight3DComponent>(src));
    if (registry.all_of<PointLightComponent>(src))
        registry.emplace<PointLightComponent>(dst, registry.get<PointLightComponent>(src));
    if (registry.all_of<SpotLightComponent>(src))
        registry.emplace<SpotLightComponent>(dst, registry.get<SpotLightComponent>(src));
    if (registry.all_of<RigidBody3DComponent>(src))
        registry.emplace<RigidBody3DComponent>(dst, registry.get<RigidBody3DComponent>(src));
    if (registry.all_of<BoxCollider3DComponent>(src))
        registry.emplace<BoxCollider3DComponent>(dst, registry.get<BoxCollider3DComponent>(src));
    if (registry.all_of<SphereCollider3DComponent>(src))
        registry.emplace<SphereCollider3DComponent>(dst, registry.get<SphereCollider3DComponent>(src));
    if (registry.all_of<AudioSourceComponent>(src))
        registry.emplace<AudioSourceComponent>(dst, registry.get<AudioSourceComponent>(src));
    if (registry.all_of<SkyLightComponent>(src))
        registry.emplace<SkyLightComponent>(dst, registry.get<SkyLightComponent>(src));
    if (registry.all_of<PostProcessComponent>(src))
        registry.emplace<PostProcessComponent>(dst, registry.get<PostProcessComponent>(src));
    if (registry.all_of<dse::Animator3DComponent>(src))
        registry.emplace<dse::Animator3DComponent>(dst, registry.get<dse::Animator3DComponent>(src));

    // Undo: destroy the duplicate
    {
        entt::entity dup = dst;
        auto& reg = registry;
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Duplicate Entity (RPC)",
            []() {},
            [dup, &reg]() { if (reg.valid(dup)) reg.destroy(dup); }
        ));
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(dst), alloc);
    result.AddMember("name", rapidjson::Value(new_name.c_str(), alloc), alloc);
    result.AddMember("source_entity_id", static_cast<uint32_t>(src), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_prefab_save ──────────────────────────────────────────────

static JsonRpcResponse HandlePrefabSave(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }
    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    std::string path = params["path"].GetString();
    if (path.find("..") != std::string::npos) {
        return MakeToolError(-32602, "Path must not contain '..'");
    }

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    if (!SaveEntityAsPrefab(registry, entity, path)) {
        return MakeToolError(-32603, "Failed to save prefab: " + path);
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("saved", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_prefab_instantiate ───────────────────────────────────────

static JsonRpcResponse HandlePrefabInstantiate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    std::string path = params["path"].GetString();
    if (!std::filesystem::exists(path)) {
        return MakeToolError(-32602, "Prefab file not found: " + path);
    }

    auto& world = engine.pipeline()->world();
    auto& registry = world.registry();
    auto entity = InstantiatePrefab(world, registry, path);

    if (entity == entt::null) {
        return MakeToolError(-32603, "Failed to instantiate prefab: " + path);
    }

    // Undo: destroy the instantiated entity
    {
        entt::entity inst = entity;
        auto& reg = registry;
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Instantiate Prefab (RPC)",
            []() {},
            [inst, &reg]() { if (reg.valid(inst)) reg.destroy(inst); }
        ));
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    std::string inst_name = "Prefab Instance";
    if (registry.all_of<EditorNameComponent>(entity))
        inst_name = registry.get<EditorNameComponent>(entity).name;
    result.AddMember("name", rapidjson::Value(inst_name.c_str(), alloc), alloc);
    result.AddMember("source_path", rapidjson::Value(path.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_new ────────────────────────────────────────────────

static JsonRpcResponse HandleSceneNew(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    auto& registry = engine.pipeline()->world().registry();
    registry.clear();
    GetUndoRedoManager().Clear();
    SetCurrentScenePath("Untitled");

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("cleared", rapidjson::Value(true), alloc);
    result.AddMember("path", rapidjson::Value("Untitled", alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_reparent ──────────────────────────────────────────

static JsonRpcResponse HandleEntityReparent(
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

    // parent_id: optional uint. Omit or 0xFFFFFFFF to detach (set root).
    entt::entity new_parent = entt::null;
    if (params.HasMember("parent_id") && params["parent_id"].IsUint()) {
        uint32_t pid = params["parent_id"].GetUint();
        if (pid != 0xFFFFFFFFu) {
            new_parent = static_cast<entt::entity>(pid);
            if (!registry.valid(new_parent)) {
                return MakeToolError(-32602, "Invalid parent_id");
            }
            // Cycle detection: new_parent must not be a descendant of entity
            entt::entity cur = new_parent;
            while (cur != entt::null && registry.valid(cur)) {
                if (cur == entity) return MakeToolError(-32602, "Circular parenting detected");
                if (!registry.all_of<ParentComponent>(cur)) break;
                cur = registry.get<ParentComponent>(cur).parent;
            }
        }
    }

    entt::entity old_parent = entt::null;
    int old_sibling = 0;
    if (registry.all_of<ParentComponent>(entity))
        old_parent = registry.get<ParentComponent>(entity).parent;
    if (registry.all_of<SiblingIndexComponent>(entity))
        old_sibling = registry.get<SiblingIndexComponent>(entity).index;

    // Apply new parent
    if (new_parent == entt::null) {
        registry.remove<ParentComponent>(entity);
    } else if (registry.all_of<ParentComponent>(entity)) {
        registry.get<ParentComponent>(entity).parent = new_parent;
    } else {
        registry.emplace<ParentComponent>(entity, new_parent);
    }

    // Apply sibling_index if provided
    if (params.HasMember("sibling_index") && params["sibling_index"].IsInt()) {
        int si = params["sibling_index"].GetInt();
        if (registry.all_of<SiblingIndexComponent>(entity))
            registry.get<SiblingIndexComponent>(entity).index = si;
        else
            registry.emplace<SiblingIndexComponent>(entity, si);
    }

    // Dirty transform
    if (registry.all_of<TransformComponent>(entity))
        registry.get<TransformComponent>(entity).dirty = true;

    // Undo
    {
        auto& reg = registry;
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Reparent Entity (RPC)",
            []() {},
            [&reg, entity, old_parent, old_sibling]() {
                if (!reg.valid(entity)) return;
                if (old_parent == entt::null) {
                    reg.remove<ParentComponent>(entity);
                } else if (reg.all_of<ParentComponent>(entity)) {
                    reg.get<ParentComponent>(entity).parent = old_parent;
                } else {
                    reg.emplace<ParentComponent>(entity, old_parent);
                }
                if (reg.all_of<SiblingIndexComponent>(entity))
                    reg.get<SiblingIndexComponent>(entity).index = old_sibling;
                if (reg.all_of<TransformComponent>(entity))
                    reg.get<TransformComponent>(entity).dirty = true;
            }
        ));
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("parent_id",
        new_parent == entt::null ? rapidjson::Value(rapidjson::kNullType) :
        rapidjson::Value(static_cast<uint32_t>(new_parent)),
        alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_selection_get ───────────────────────────────────────────

static JsonRpcResponse HandleSelectionGet(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& sel = SelectionManager::Get();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    rapidjson::Value ids(rapidjson::kArrayType);
    for (auto ent : sel.GetAll()) {
        ids.PushBack(static_cast<uint32_t>(ent), alloc);
    }
    result.AddMember("entity_ids", ids, alloc);
    result.AddMember("count", rapidjson::Value(sel.Count()), alloc);
    if (sel.GetPrimary() == entt::null) {
        result.AddMember("primary_id", rapidjson::Value(rapidjson::kNullType), alloc);
    } else {
        result.AddMember("primary_id",
            rapidjson::Value(static_cast<uint32_t>(sel.GetPrimary())), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_selection_set ───────────────────────────────────────────

static JsonRpcResponse HandleSelectionSet(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_ids") || !params["entity_ids"].IsArray()) {
        return MakeToolError(-32602, "Missing required param: entity_ids (array of uint)");
    }

    auto& registry = engine.pipeline()->world().registry();
    auto& sel = SelectionManager::Get();
    sel.Clear();

    int added = 0;
    for (const auto& v : params["entity_ids"].GetArray()) {
        if (!v.IsUint()) continue;
        auto ent = static_cast<entt::entity>(v.GetUint());
        if (!registry.valid(ent)) continue;
        sel.Add(ent);
        ++added;
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("count", rapidjson::Value(added), alloc);
    if (sel.GetPrimary() == entt::null) {
        result.AddMember("primary_id", rapidjson::Value(rapidjson::kNullType), alloc);
    } else {
        result.AddMember("primary_id",
            rapidjson::Value(static_cast<uint32_t>(sel.GetPrimary())), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_selection_clear ─────────────────────────────────────────

static JsonRpcResponse HandleSelectionClear(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    SelectionManager::Get().Clear();

    rapidjson::Document result;
    result.SetObject();
    result.AddMember("cleared", rapidjson::Value(true), result.GetAllocator());
    return MakeOk(std::move(result));
}

// ─── 注册表 ─────────────────────────────────────────────────────────────────

struct ToolEntry {
    const char* method;
    ToolHandler handler;
};

static const ToolEntry kBuiltinTools[] = {
    { "dsengine_ping",                      HandlePing },
    { "dsengine_lua_execute",               HandleLuaExecute },
    { "dsengine_scene_get_state",           HandleSceneGetState },
    { "dsengine_entity_create",             HandleEntityCreate },
    { "dsengine_entity_delete",             HandleEntityDelete },
    { "dsengine_entity_modify",             HandleEntityModify },
    { "dsengine_entity_add_component",      HandleEntityAddComponent },
    { "dsengine_entity_remove_component",   HandleEntityRemoveComponent },
    { "dsengine_entity_get_components",     HandleEntityGetComponents },
    { "dsengine_script_create",             HandleScriptCreate },
    { "dsengine_editor_get_state",          HandleEditorGetState },
    { "dsengine_editor_play",               HandleEditorPlay },
    { "dsengine_editor_stop",               HandleEditorStop },
    { "dsengine_editor_undo",               HandleEditorUndo },
    { "dsengine_editor_redo",               HandleEditorRedo },
    { "dsengine_editor_screenshot",         HandleEditorScreenshot },
    { "dsengine_scene_save",                HandleSceneSave },
    { "dsengine_scene_load",                HandleSceneLoad },
    { "dsengine_asset_import",              HandleAssetImport },
    { "dsengine_material_create",           HandleMaterialCreate },
    { "dsengine_entity_get_state",          HandleEntityGetState },
    { "dsengine_entity_duplicate",          HandleEntityDuplicate },
    { "dsengine_prefab_save",               HandlePrefabSave },
    { "dsengine_prefab_instantiate",        HandlePrefabInstantiate },
    { "dsengine_scene_new",                 HandleSceneNew },
    { "dsengine_entity_reparent",           HandleEntityReparent },
    { "dsengine_selection_get",             HandleSelectionGet },
    { "dsengine_selection_set",             HandleSelectionSet },
    { "dsengine_selection_clear",           HandleSelectionClear },
};

void RegisterBuiltinTools(ControlServer& server) {
    for (const auto& tool : kBuiltinTools) {
        server.RegisterTool(tool.method, tool.handler);
    }
}

} // namespace dse::editor
