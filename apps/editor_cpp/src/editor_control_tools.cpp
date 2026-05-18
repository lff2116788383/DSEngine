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
#include "editor_shortcuts.h"
#include "editor_undo.h"
#include "editor_shell.h"
#include "editor_scene_tabs.h"

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

// ─── Tool: dsengine_editor_play ─────────────────────────────────────────────

static JsonRpcResponse HandleEditorPlay(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    if (GetEditorState() != EditorState::Edit) {
        return MakeToolError(-32603, "Already in play/pause mode");
    }

    auto& registry = engine.pipeline()->world().registry();
    EnterPlayMode(registry);

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

    if (GetEditorState() == EditorState::Edit) {
        return MakeToolError(-32603, "Not in play mode");
    }

    auto& registry = engine.pipeline()->world().registry();
    entt::entity dummy = entt::null;
    ExitPlayMode(registry, dummy);

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

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("modified", rapidjson::Value(true), alloc);
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

// ─── 注册表 ─────────────────────────────────────────────────────────────────

void RegisterBuiltinTools(ControlServer& server) {
    server.RegisterTool("dsengine_ping",                HandlePing);
    server.RegisterTool("dsengine_lua_execute",         HandleLuaExecute);
    server.RegisterTool("dsengine_scene_get_state",     HandleSceneGetState);
    server.RegisterTool("dsengine_entity_create",       HandleEntityCreate);
    server.RegisterTool("dsengine_entity_delete",       HandleEntityDelete);
    server.RegisterTool("dsengine_entity_modify",       HandleEntityModify);
    server.RegisterTool("dsengine_script_create",       HandleScriptCreate);
    server.RegisterTool("dsengine_editor_get_state",    HandleEditorGetState);
    server.RegisterTool("dsengine_editor_play",         HandleEditorPlay);
    server.RegisterTool("dsengine_editor_stop",         HandleEditorStop);
    server.RegisterTool("dsengine_editor_undo",         HandleEditorUndo);
    server.RegisterTool("dsengine_editor_redo",         HandleEditorRedo);
    server.RegisterTool("dsengine_editor_screenshot",   HandleEditorScreenshot);
    server.RegisterTool("dsengine_scene_save",          HandleSceneSave);
    server.RegisterTool("dsengine_scene_load",          HandleSceneLoad);
}

} // namespace dse::editor
