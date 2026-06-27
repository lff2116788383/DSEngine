#include "apps/editor_cpp/core/command_bus.h"

#include <utility>

namespace dse::editor::core {

namespace {

// std::visit 辅助
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void AddString(rapidjson::Document& doc, const char* key, const std::string& v) {
    auto& a = doc.GetAllocator();
    doc.AddMember(rapidjson::Value(key, a),
                  rapidjson::Value(v.c_str(),
                                   static_cast<rapidjson::SizeType>(v.size()), a),
                  a);
}

void AddUint(rapidjson::Document& doc, const char* key, std::uint32_t v) {
    auto& a = doc.GetAllocator();
    doc.AddMember(rapidjson::Value(key, a), rapidjson::Value(v), a);
}

void AddInt(rapidjson::Document& doc, const char* key, int v) {
    auto& a = doc.GetAllocator();
    doc.AddMember(rapidjson::Value(key, a), rapidjson::Value(v), a);
}

void AddVec3(rapidjson::Document& doc, const char* key, const Vec3& v) {
    auto& a = doc.GetAllocator();
    rapidjson::Value arr(rapidjson::kArrayType);
    arr.PushBack(v[0], a).PushBack(v[1], a).PushBack(v[2], a);
    doc.AddMember(rapidjson::Value(key, a), arr, a);
}

void AddUintArray(rapidjson::Document& doc, const char* key,
                  const std::vector<EntityId>& ids) {
    auto& a = doc.GetAllocator();
    rapidjson::Value arr(rapidjson::kArrayType);
    for (EntityId id : ids) arr.PushBack(rapidjson::Value(id), a);
    doc.AddMember(rapidjson::Value(key, a), arr, a);
}

} // namespace

CommandResult CommandBus::dispatch(const EditorCommand& cmd,
                                   dse::runtime::EngineInstance& engine) {
    rapidjson::Document params(rapidjson::kObjectType);
    std::string method;

    std::visit(overloaded{
        // ── 实体 ──
        [&](const CreateEntityCmd& c) {
            method = "dsengine_entity_create";
            if (!c.name.empty()) AddString(params, "name", c.name);
            if (!c.components.empty()) {
                auto& a = params.GetAllocator();
                rapidjson::Value arr(rapidjson::kArrayType);
                for (const auto& t : c.components)
                    arr.PushBack(rapidjson::Value(
                        t.c_str(), static_cast<rapidjson::SizeType>(t.size()), a), a);
                params.AddMember("components", arr, a);
            }
        },
        [&](const DeleteEntityCmd& c) {
            method = "dsengine_entity_delete";
            AddUint(params, "entity_id", c.entity_id);
        },
        [&](const BatchDeleteEntitiesCmd& c) {
            method = "dsengine_entity_batch_delete";
            AddUintArray(params, "entity_ids", c.entity_ids);
        },
        [&](const RenameEntityCmd& c) {
            method = "dsengine_entity_modify";
            AddUint(params, "entity_id", c.entity_id);
            AddString(params, "name", c.name);
        },
        [&](const TransformEntityCmd& c) {
            method = "dsengine_entity_modify";
            AddUint(params, "entity_id", c.entity_id);
            if (c.position) AddVec3(params, "position", *c.position);
            if (c.rotation) AddVec3(params, "rotation", *c.rotation);
            if (c.scale)    AddVec3(params, "scale", *c.scale);
        },
        [&](const ReparentEntityCmd& c) {
            method = "dsengine_entity_reparent";
            AddUint(params, "entity_id", c.entity_id);
            if (c.parent.has_value()) AddUint(params, "parent_id", *c.parent);
            if (c.sibling_index.has_value())
                AddInt(params, "sibling_index", *c.sibling_index);
        },
        [&](const DuplicateEntityCmd& c) {
            method = "dsengine_entity_duplicate";
            AddUint(params, "entity_id", c.entity_id);
        },
        [&](const AddComponentCmd& c) {
            method = "dsengine_entity_add_component";
            AddUint(params, "entity_id", c.entity_id);
            AddString(params, "type", c.type);
        },
        [&](const RemoveComponentCmd& c) {
            method = "dsengine_entity_remove_component";
            AddUint(params, "entity_id", c.entity_id);
            AddString(params, "type", c.type);
        },
        // ── 选择 ──
        [&](const SetSelectionCmd& c) {
            method = "dsengine_selection_set";
            AddUintArray(params, "entity_ids", c.entity_ids);
        },
        [&](const ClearSelectionCmd&) {
            method = "dsengine_selection_clear";
        },
        // ── 场景 ──
        [&](const NewSceneCmd&) {
            method = "dsengine_scene_new";
        },
        [&](const SaveSceneCmd& c) {
            method = "dsengine_scene_save";
            if (c.path.has_value()) AddString(params, "path", *c.path);
        },
        [&](const LoadSceneCmd& c) {
            method = "dsengine_scene_load";
            AddString(params, "path", c.path);
        },
        // ── 资产 / 材质 / 脚本 ──
        [&](const ImportAssetCmd& c) {
            method = "dsengine_asset_import";
            AddString(params, "path", c.path);
            if (c.type.has_value()) AddString(params, "type", *c.type);
        },
        [&](const CreateMaterialCmd& c) {
            method = "dsengine_material_create";
            if (!c.name.empty()) AddString(params, "name", c.name);
            if (c.shader_variant.has_value())
                AddString(params, "shader_variant", *c.shader_variant);
            if (c.save_path.has_value())
                AddString(params, "save_path", *c.save_path);
        },
        [&](const CreateScriptCmd& c) {
            method = "dsengine_script_create";
            AddString(params, "path", c.path);
            AddString(params, "content", c.content);
        },
        // ── 预制体 ──
        [&](const SavePrefabCmd& c) {
            method = "dsengine_prefab_save";
            AddUint(params, "entity_id", c.entity_id);
            AddString(params, "path", c.path);
        },
        [&](const InstantiatePrefabCmd& c) {
            method = "dsengine_prefab_instantiate";
            AddString(params, "path", c.path);
        },
        // ── 编辑器生命周期 / 动作 ──
        [&](const PlayCmd&) { method = "dsengine_editor_play"; },
        [&](const StopCmd&) { method = "dsengine_editor_stop"; },
        [&](const UndoCmd&) { method = "dsengine_editor_undo"; },
        [&](const RedoCmd&) { method = "dsengine_editor_redo"; },
        [&](const OpenProjectCmd& c) {
            method = "dsengine_project_open";
            AddString(params, "path", c.path);
        },
        [&](const ExecuteLuaCmd& c) {
            method = "dsengine_lua_execute";
            AddString(params, "code", c.code);
        },
    }, cmd);

    CommandResult out;
    if (!sink_) {
        out.ok = false;
        out.error_code = -32603;
        out.error_message = "CommandBus has no ToolSink bound";
        return out;
    }

    dse::editor::JsonRpcResponse resp = sink_(method, params, engine);
    out.ok = !resp.is_error;
    out.error_code = resp.error_code;
    out.error_message = resp.error_message;
    if (out.ok && resp.result.IsObject()) {
        out.data.CopyFrom(resp.result, out.data.GetAllocator());
    }
    return out;
}

} // namespace dse::editor::core
