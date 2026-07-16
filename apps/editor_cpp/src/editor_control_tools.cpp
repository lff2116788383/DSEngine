#include "editor_control_server.h"
#include "editor_control_tools_common.h"

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
#include "engine/ecs/light_2d.h"
#include "engine/ecs/trail_renderer_2d.h"
#include "engine/ecs/line_renderer_2d.h"
#include "engine/ecs/parallax_2d.h"
#include "engine/ecs/camera_controller_2d.h"
#include "engine/ecs/audio_spatial_2d.h"
#include "engine/ecs/blueprint_component.h"
#include "engine/ecs/components_3d_impostor.h"
#include "engine/base/time.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/assets/asset_manager.h"
#include "editor_toolbar.h"
#include "editor_scene_io.h"
#include "editor_shared_components.h"
#include "editor_shortcuts.h"
#include "editor_undo.h"
#include "editor_entity_snapshot.h"
#include "editor_shell.h"
#include "editor_scene_tabs.h"
#include "editor_prefab.h"
#include "editor_selection.h"
#include "editor_project.h"
#include "editor_asset_db.h"

#include <vector>
#include <string>

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stb/stb_image_write.h>

namespace dse::editor {

// ─── Helper ─────────────────────────────────────────────────────────────────

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

// ─── Helper: 解析 vec3 数组 ──────────────────────────────────────────────────

// ─── Helper: 按类型名添加组件 ────────────────────────────────────────────────

// ─── Tool: dsengine_entity_create ───────────────────────────────────────────

// ─── Tool: dsengine_entity_delete ───────────────────────────────────────────

// ─── Tool: dsengine_entity_batch_delete ─────────────────────────────────────

// ─── Tool: dsengine_script_create ───────────────────────────────────────────

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

    result.AddMember("entity_count", CountValidEntities(registry), alloc);

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

// ─── Tool: dsengine_entity_add_component ────────────────────────────────────

// ─── Tool: dsengine_entity_remove_component ─────────────────────────────────

// ─── Helper: 收集实体上的组件列表 ───────────────────────────────────────────

// ─── Tool: dsengine_entity_get_components ───────────────────────────────────

// ─── Tool: dsengine_scene_save ──────────────────────────────────────────────

// ─── Tool: dsengine_scene_load ──────────────────────────────────────────────

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

// ─── Tool: dsengine_material_create ─────────────────────────────────────────

// ─── Tool: dsengine_entity_get_state ────────────────────────────────────────

// ─── Tool: dsengine_entity_duplicate ────────────────────────────────────────

// ─── Tool: dsengine_prefab_save ──────────────────────────────────────────────

// ─── Tool: dsengine_prefab_instantiate ───────────────────────────────────────

// ─── Tool: dsengine_scene_new ────────────────────────────────────────────────

// ─── Tool: dsengine_entity_reparent ──────────────────────────────────────────

// ─── Tool: dsengine_entity_find_by_name ─────────────────────────────────────

// ─── Tool: dsengine_undo_history ────────────────────────────────────────────

static JsonRpcResponse HandleUndoHistory(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& mgr = GetUndoRedoManager();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    result.AddMember("can_undo", rapidjson::Value(mgr.CanUndo()), alloc);
    result.AddMember("can_redo", rapidjson::Value(mgr.CanRedo()), alloc);
    result.AddMember("undo_count", rapidjson::Value(mgr.GetUndoCount()), alloc);
    result.AddMember("redo_count", rapidjson::Value(mgr.GetRedoCount()), alloc);
    result.AddMember("undo_description",
        rapidjson::Value(mgr.GetUndoDescription().c_str(), alloc), alloc);
    result.AddMember("redo_description",
        rapidjson::Value(mgr.GetRedoDescription().c_str(), alloc), alloc);

    rapidjson::Value undo_list(rapidjson::kArrayType);
    for (const auto& s : mgr.GetUndoHistory())
        undo_list.PushBack(rapidjson::Value(s.c_str(), alloc), alloc);
    result.AddMember("undo_history", undo_list, alloc);

    rapidjson::Value redo_list(rapidjson::kArrayType);
    for (const auto& s : mgr.GetRedoHistory())
        redo_list.PushBack(rapidjson::Value(s.c_str(), alloc), alloc);
    result.AddMember("redo_history", redo_list, alloc);

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

// ─── Tool: dsengine_project_open ────────────────────────────────────────────
// 打开工程级（.dseproj）：加载项目描述符 → 同步 data root → 刷新资产库 →
// 加载项目默认场景。现有 dsengine_scene_load 仅 scene 级，本工具补齐工程级语义。

// ─── Tool: dsengine_editor_quit ─────────────────────────────────────────────
// 置退出标志，主循环下一帧返回 false。供自动化会话结束时优雅退出编辑器。

static JsonRpcResponse HandleEditorQuit(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    dse::editor::RequestExit();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("ok", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_idle ─────────────────────────────────────────────
// 返回当前帧的状态快照（同步点）。编辑器主循环每帧持续渲染，调用间隔内帧已自然推进，
// 因此本工具用于"等待渲染稳定后读取状态"，frames 参数记录期望推进帧数（供报告参考）。

static JsonRpcResponse HandleEditorIdle(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    int frames = 1;
    if (params.HasMember("frames") && params["frames"].IsInt()) {
        frames = params["frames"].GetInt();
    }

    auto& registry = engine.pipeline()->world().registry();
    const float dt = Time::delta_time();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("frames_requested", frames, alloc);
    result.AddMember("delta_ms", dt * 1000.0f, alloc);
    result.AddMember("fps", dt > 0.0f ? (1.0f / dt) : 0.0f, alloc);
    result.AddMember("time_since_startup", Time::TimeSinceStartup(), alloc);
    result.AddMember("entity_count", CountValidEntities(registry), alloc);
    const char* state_str =
        dse::editor::GetEditorState() == dse::editor::EditorState::Play ? "play" :
        dse::editor::GetEditorState() == dse::editor::EditorState::Pause ? "pause" : "edit";
    result.AddMember("editor_state", rapidjson::Value(state_str, alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_get_metrics ──────────────────────────────────────
// 返回 FPS / DrawCall / 实体数等运行时指标，供 soak/性能监控使用。

static JsonRpcResponse HandleEditorGetMetrics(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    auto& registry = engine.pipeline()->world().registry();
    const float dt = Time::delta_time();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("fps", dt > 0.0f ? (1.0f / dt) : 0.0f, alloc);
    result.AddMember("delta_ms", dt * 1000.0f, alloc);
    result.AddMember("draw_calls", engine.pipeline()->LastDrawCalls(), alloc);
    result.AddMember("entity_count", CountValidEntities(registry), alloc);
    result.AddMember("time_since_startup", Time::TimeSinceStartup(), alloc);
    const char* state_str =
        dse::editor::GetEditorState() == dse::editor::EditorState::Play ? "play" :
        dse::editor::GetEditorState() == dse::editor::EditorState::Pause ? "pause" : "edit";
    result.AddMember("editor_state", rapidjson::Value(state_str, alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_list_tools ──────────────────────────────────────────────
// 自省：返回已注册的全部工具名，供用例 schema 校验、杜绝命名漂移。

static std::vector<std::string> g_registered_tool_names;

static JsonRpcResponse HandleListTools(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    rapidjson::Value arr(rapidjson::kArrayType);
    for (const auto& name : g_registered_tool_names) {
        arr.PushBack(rapidjson::Value(name.c_str(), alloc), alloc);
    }
    result.AddMember("tools", arr, alloc);
    result.AddMember("count", static_cast<int>(g_registered_tool_names.size()), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_set_active ────────────────────────────────────────

// ─── Tool: dsengine_entity_get_children ─────────────────────────────────────

// ─── Tool: dsengine_script_attach ───────────────────────────────────────────

// ─── Tool: dsengine_editor_pause ────────────────────────────────────────────

static JsonRpcResponse HandleEditorPause(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto state = dse::editor::GetEditorState();
    if (state == dse::editor::EditorState::Edit)
        return MakeToolError(-32600, "Cannot pause: editor is in edit mode, not play mode");

    const char* prev = state == dse::editor::EditorState::Play ? "play" : "pause";
    dse::editor::ToggleEditorPause();
    auto new_state = dse::editor::GetEditorState();
    const char* curr = new_state == dse::editor::EditorState::Play ? "play" :
                       new_state == dse::editor::EditorState::Pause ? "pause" : "edit";

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("previous_state", rapidjson::Value(prev, alloc), alloc);
    result.AddMember("editor_state", rapidjson::Value(curr, alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_list ──────────────────────────────────────────────

// ─── Tool: dsengine_project_get_info ────────────────────────────────────────

// ─── Tool: dsengine_gizmo_set_mode ──────────────────────────────────────────

static int* g_gizmo_operation_ptr = nullptr;
static int* g_gizmo_mode_ptr = nullptr;

void SetGizmoPointers(int* op, int* mode) {
    g_gizmo_operation_ptr = op;
    g_gizmo_mode_ptr = mode;
}

static JsonRpcResponse HandleGizmoSetMode(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (params.HasMember("operation") && params["operation"].IsString() && g_gizmo_operation_ptr) {
        std::string op = params["operation"].GetString();
        if (op == "translate" || op == "move")   *g_gizmo_operation_ptr = 7;   // ImGuizmo::TRANSLATE
        else if (op == "rotate")                 *g_gizmo_operation_ptr = 120; // ImGuizmo::ROTATE
        else if (op == "scale")                  *g_gizmo_operation_ptr = 896; // ImGuizmo::SCALE
        result.AddMember("operation", rapidjson::Value(op.c_str(), alloc), alloc);
    }

    if (params.HasMember("space") && params["space"].IsString() && g_gizmo_mode_ptr) {
        std::string mode = params["space"].GetString();
        if (mode == "local")       *g_gizmo_mode_ptr = 0; // ImGuizmo::LOCAL
        else if (mode == "world")  *g_gizmo_mode_ptr = 1; // ImGuizmo::WORLD
        result.AddMember("space", rapidjson::Value(mode.c_str(), alloc), alloc);
    }

    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_physics_raycast ─────────────────────────────────────────

static JsonRpcResponse HandlePhysicsRaycast(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("origin") || !params["origin"].IsArray() || params["origin"].Size() < 3)
        return MakeToolError(-32602, "Missing required param: origin ([x,y,z])");
    if (!params.HasMember("direction") || !params["direction"].IsArray() || params["direction"].Size() < 3)
        return MakeToolError(-32602, "Missing required param: direction ([x,y,z])");

    glm::vec3 origin = ParseVec3(params["origin"]);
    glm::vec3 direction = ParseVec3(params["direction"]);
    float max_dist = 1000.0f;
    if (params.HasMember("max_distance") && params["max_distance"].IsNumber())
        max_dist = params["max_distance"].GetFloat();

    std::string lua_code = "local r = DSE.Physics3D.Raycast("
        + std::to_string(origin.x) + "," + std::to_string(origin.y) + "," + std::to_string(origin.z) + ","
        + std::to_string(direction.x) + "," + std::to_string(direction.y) + "," + std::to_string(direction.z) + ","
        + std::to_string(max_dist) + ") "
        "return r";

    std::string output;
    bool ok = dse::runtime::ExecuteLuaString(lua_code.c_str(), &output);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("executed", ok, alloc);
    result.AddMember("output", rapidjson::Value(output.c_str(), alloc), alloc);

    rapidjson::Value orig(rapidjson::kArrayType);
    orig.PushBack(origin.x, alloc).PushBack(origin.y, alloc).PushBack(origin.z, alloc);
    result.AddMember("origin", orig, alloc);
    rapidjson::Value dir(rapidjson::kArrayType);
    dir.PushBack(direction.x, alloc).PushBack(direction.y, alloc).PushBack(direction.z, alloc);
    result.AddMember("direction", dir, alloc);
    result.AddMember("max_distance", max_dist, alloc);

    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_physics_set_gravity ─────────────────────────────────────

static JsonRpcResponse HandlePhysicsSetGravity(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    float gx = 0.0f, gy = -9.81f, gz = 0.0f;
    if (params.HasMember("gravity") && params["gravity"].IsArray() && params["gravity"].Size() >= 3 &&
        params["gravity"][0].IsNumber() && params["gravity"][1].IsNumber() && params["gravity"][2].IsNumber()) {
        gx = params["gravity"][0].GetFloat();
        gy = params["gravity"][1].GetFloat();
        gz = params["gravity"][2].GetFloat();
    } else if (params.HasMember("y") && params["y"].IsNumber()) {
        gy = params["y"].GetFloat();
    }

    std::string lua_code = "DSE.Physics3D.SetGravity(" +
        std::to_string(gx) + "," + std::to_string(gy) + "," + std::to_string(gz) + ")";

    std::string output;
    bool ok = dse::runtime::ExecuteLuaString(lua_code.c_str(), &output);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("executed", ok, alloc);
    if (!output.empty()) result.AddMember("output", rapidjson::Value(output.c_str(), alloc), alloc);
    rapidjson::Value grav(rapidjson::kArrayType);
    grav.PushBack(gx, alloc).PushBack(gy, alloc).PushBack(gz, alloc);
    result.AddMember("gravity", grav, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_audio_play ──────────────────────────────────────────────

static JsonRpcResponse HandleAudioPlay(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("clip_path") || !params["clip_path"].IsString())
        return MakeToolError(-32602, "Missing required param: clip_path (string)");

    std::string clip = params["clip_path"].GetString();
    float volume = 1.0f;
    if (params.HasMember("volume") && params["volume"].IsNumber())
        volume = params["volume"].GetFloat();
    bool loop = false;
    if (params.HasMember("loop") && params["loop"].IsBool())
        loop = params["loop"].GetBool();

    std::string lua_code = "DSE.Audio.PlaySfx(\"" + clip + "\", " +
        std::to_string(volume) + ", " + (loop ? "true" : "false") + ")";

    std::string output;
    bool ok = dse::runtime::ExecuteLuaString(lua_code.c_str(), &output);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("executed", ok, alloc);
    result.AddMember("clip_path", rapidjson::Value(clip.c_str(), alloc), alloc);
    result.AddMember("volume", volume, alloc);
    result.AddMember("loop", loop, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_audio_stop ──────────────────────────────────────────────

static JsonRpcResponse HandleAudioStop(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    std::string lua_code = "DSE.Audio.StopAllSfx() DSE.Audio.StopBgm()";
    std::string output;
    bool ok = dse::runtime::ExecuteLuaString(lua_code.c_str(), &output);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("executed", ok, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_camera_set_view ─────────────────────────────────────────

static JsonRpcResponse HandleCameraSetView(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    auto& registry = engine.pipeline()->world().registry();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (params.HasMember("entity_id") && params["entity_id"].IsUint()) {
        auto target = static_cast<entt::entity>(params["entity_id"].GetUint());
        if (!registry.valid(target))
            return MakeToolError(-32602, "Invalid entity_id");

        if (registry.all_of<TransformComponent>(target)) {
            const auto& tt = registry.get<TransformComponent>(target);
            // Find the first Camera3D and move it to look at the target
            auto cam_view = registry.view<Camera3DComponent, TransformComponent>();
            for (auto cam_entity : cam_view) {
                auto& cam_transform = cam_view.get<TransformComponent>(cam_entity);
                glm::vec3 offset(0.0f, 2.0f, 5.0f);
                cam_transform.position = tt.position + offset;
                cam_transform.dirty = true;
                result.AddMember("camera_entity", static_cast<uint32_t>(cam_entity), alloc);
                break;
            }
            rapidjson::Value pos(rapidjson::kArrayType);
            pos.PushBack(tt.position.x, alloc).PushBack(tt.position.y, alloc).PushBack(tt.position.z, alloc);
            result.AddMember("target_position", pos, alloc);
        }
    } else if (params.HasMember("position") && params["position"].IsArray()) {
        glm::vec3 pos = ParseVec3(params["position"]);
        auto cam_view = registry.view<Camera3DComponent, TransformComponent>();
        for (auto cam_entity : cam_view) {
            auto& cam_transform = cam_view.get<TransformComponent>(cam_entity);
            cam_transform.position = pos;
            cam_transform.dirty = true;
            result.AddMember("camera_entity", static_cast<uint32_t>(cam_entity), alloc);
            break;
        }
        rapidjson::Value p(rapidjson::kArrayType);
        p.PushBack(pos.x, alloc).PushBack(pos.y, alloc).PushBack(pos.z, alloc);
        result.AddMember("position", p, alloc);
    }

    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_asset_list ──────────────────────────────────────────────

// ─── Tool: dsengine_asset_delete ────────────────────────────────────────────

// ─── 注册表 ─────────────────────────────────────────────────────────────────

struct ToolEntry {
    const char* method;
    ToolHandler handler;
};

static const ToolEntry kBuiltinTools[] = {
    { "dsengine_ping",                      HandlePing },
    { "dsengine_lua_execute",               HandleLuaExecute },
    { "dsengine_editor_get_state",          HandleEditorGetState },
    { "dsengine_editor_play",               HandleEditorPlay },
    { "dsengine_editor_stop",               HandleEditorStop },
    { "dsengine_editor_undo",               HandleEditorUndo },
    { "dsengine_editor_redo",               HandleEditorRedo },
    { "dsengine_editor_screenshot",         HandleEditorScreenshot },
    { "dsengine_undo_history",              HandleUndoHistory },
    { "dsengine_selection_get",             HandleSelectionGet },
    { "dsengine_selection_set",             HandleSelectionSet },
    { "dsengine_selection_clear",           HandleSelectionClear },
    { "dsengine_editor_quit",               HandleEditorQuit },
    { "dsengine_editor_idle",               HandleEditorIdle },
    { "dsengine_editor_get_metrics",        HandleEditorGetMetrics },
    { "dsengine_list_tools",                HandleListTools },
    { "dsengine_editor_pause",              HandleEditorPause },
    { "dsengine_gizmo_set_mode",            HandleGizmoSetMode },
    { "dsengine_physics_raycast",           HandlePhysicsRaycast },
    { "dsengine_physics_set_gravity",       HandlePhysicsSetGravity },
    { "dsengine_audio_play",                HandleAudioPlay },
    { "dsengine_audio_stop",                HandleAudioStop },
    { "dsengine_camera_set_view",           HandleCameraSetView },
};

void RegisterBuiltinTools(ControlServer& server) {
    // Register entity and scene tools from sub-files
    RegisterEntityTools(server, g_registered_tool_names);
    RegisterSceneTools(server, g_registered_tool_names);

    g_registered_tool_names.clear();
    for (const auto& tool : kBuiltinTools) {
        server.RegisterTool(tool.method, tool.handler);
        g_registered_tool_names.emplace_back(tool.method);
    }
}

} // namespace dse::editor
