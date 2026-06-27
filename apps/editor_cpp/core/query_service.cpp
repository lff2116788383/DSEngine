#include "apps/editor_cpp/core/query_service.h"

#include <algorithm>
#include <optional>
#include <unordered_map>

namespace dse::editor::core {

namespace {

/// 组件数组在不同工具里可能是纯字符串数组，也可能是 {"type":...} 对象数组。
/// 统一抽出类型名列表。
void ExtractComponentNames(const rapidjson::Value& arr,
                           std::vector<std::string>& out) {
    if (!arr.IsArray()) return;
    for (const auto& c : arr.GetArray()) {
        if (c.IsString()) {
            out.emplace_back(c.GetString());
        } else if (c.IsObject() && c.HasMember("type") && c["type"].IsString()) {
            out.emplace_back(c["type"].GetString());
        }
    }
}

} // namespace

SceneSummaryVM QueryService::sceneSummary(dse::runtime::EngineInstance& engine) {
    SceneSummaryVM vm;
    if (!sink_) return vm;

    rapidjson::Document params(rapidjson::kObjectType);
    // 概览不需要逐组件明细，关掉以减负。
    params.AddMember("include_components", false, params.GetAllocator());

    dse::editor::JsonRpcResponse resp =
        sink_("dsengine_scene_get_state", params, engine);
    if (resp.is_error || !resp.result.IsObject()) return vm;

    const auto& r = resp.result;
    if (r.HasMember("editor_state") && r["editor_state"].IsString()) {
        vm.editor_state = r["editor_state"].GetString();
    }
    if (r.HasMember("entity_count") && r["entity_count"].IsInt()) {
        vm.entity_count = r["entity_count"].GetInt();
    }
    vm.valid = true;
    return vm;
}

EntityComponentsVM QueryService::entityComponents(
    std::uint32_t entity_id, dse::runtime::EngineInstance& engine) {
    EntityComponentsVM vm;
    vm.entity_id = entity_id;
    if (!sink_) return vm;

    rapidjson::Document params(rapidjson::kObjectType);
    auto& a = params.GetAllocator();
    params.AddMember("entity_id", rapidjson::Value(entity_id), a);
    // detailed=false → components 为纯字符串数组，便于映射为 VM 字段。
    params.AddMember("detailed", false, a);

    dse::editor::JsonRpcResponse resp =
        sink_("dsengine_entity_get_components", params, engine);
    if (resp.is_error || !resp.result.IsObject()) return vm;

    const auto& r = resp.result;
    if (r.HasMember("components") && r["components"].IsArray()) {
        for (const auto& c : r["components"].GetArray()) {
            if (c.IsString()) vm.components.emplace_back(c.GetString());
        }
    }
    vm.valid = true;
    return vm;
}

InspectorVM QueryService::inspector(std::uint32_t entity_id,
                                    dse::runtime::EngineInstance& engine) {
    InspectorVM vm;
    vm.entity_id = entity_id;
    if (!sink_) return vm;

    rapidjson::Document params(rapidjson::kObjectType);
    params.AddMember("entity_id", rapidjson::Value(entity_id), params.GetAllocator());

    dse::editor::JsonRpcResponse resp =
        sink_("dsengine_entity_get_state", params, engine);
    if (resp.is_error || !resp.result.IsObject()) return vm;

    const auto& r = resp.result;
    if (r.HasMember("name") && r["name"].IsString()) vm.name = r["name"].GetString();

    if (r.HasMember("transform") && r["transform"].IsObject()) {
        const auto& tf = r["transform"];
        vm.transform.present = true;
        auto readArr = [](const rapidjson::Value& src, float* dst, std::size_t n) {
            if (!src.IsArray()) return;
            for (rapidjson::SizeType i = 0; i < src.Size() && i < n; ++i)
                if (src[i].IsNumber()) dst[i] = src[i].GetFloat();
        };
        if (tf.HasMember("position")) readArr(tf["position"], vm.transform.position.data(), 3);
        if (tf.HasMember("rotation")) readArr(tf["rotation"], vm.transform.rotation.data(), 4);
        if (tf.HasMember("scale"))    readArr(tf["scale"],    vm.transform.scale.data(), 3);
    }

    if (r.HasMember("components")) ExtractComponentNames(r["components"], vm.components);
    vm.valid = true;
    return vm;
}

EditorStateVM QueryService::editorState(dse::runtime::EngineInstance& engine) {
    EditorStateVM vm;
    if (!sink_) return vm;

    rapidjson::Document params(rapidjson::kObjectType);
    dse::editor::JsonRpcResponse resp =
        sink_("dsengine_editor_get_state", params, engine);
    if (resp.is_error || !resp.result.IsObject()) return vm;

    const auto& r = resp.result;
    if (r.HasMember("editor_state") && r["editor_state"].IsString())
        vm.editor_state = r["editor_state"].GetString();
    if (r.HasMember("entity_count") && r["entity_count"].IsInt())
        vm.entity_count = r["entity_count"].GetInt();
    if (r.HasMember("data_root") && r["data_root"].IsString())
        vm.data_root = r["data_root"].GetString();
    vm.valid = true;
    return vm;
}

SelectionVM QueryService::selection(dse::runtime::EngineInstance& engine) {
    SelectionVM vm;
    if (!sink_) return vm;

    rapidjson::Document params(rapidjson::kObjectType);
    dse::editor::JsonRpcResponse resp =
        sink_("dsengine_selection_get", params, engine);
    if (resp.is_error || !resp.result.IsObject()) return vm;

    const auto& r = resp.result;
    if (r.HasMember("entity_ids") && r["entity_ids"].IsArray()) {
        for (const auto& v : r["entity_ids"].GetArray())
            if (v.IsUint()) vm.entity_ids.push_back(v.GetUint());
    }
    if (r.HasMember("count") && r["count"].IsInt()) vm.count = r["count"].GetInt();
    if (r.HasMember("primary_id") && r["primary_id"].IsUint()) {
        vm.has_primary = true;
        vm.primary_id = r["primary_id"].GetUint();
    }
    vm.valid = true;
    return vm;
}

UndoHistoryVM QueryService::undoHistory(dse::runtime::EngineInstance& engine) {
    UndoHistoryVM vm;
    if (!sink_) return vm;

    rapidjson::Document params(rapidjson::kObjectType);
    dse::editor::JsonRpcResponse resp =
        sink_("dsengine_undo_history", params, engine);
    if (resp.is_error || !resp.result.IsObject()) return vm;

    const auto& r = resp.result;
    if (r.HasMember("can_undo") && r["can_undo"].IsBool()) vm.can_undo = r["can_undo"].GetBool();
    if (r.HasMember("can_redo") && r["can_redo"].IsBool()) vm.can_redo = r["can_redo"].GetBool();
    if (r.HasMember("undo_count") && r["undo_count"].IsInt()) vm.undo_count = r["undo_count"].GetInt();
    if (r.HasMember("redo_count") && r["redo_count"].IsInt()) vm.redo_count = r["redo_count"].GetInt();
    if (r.HasMember("undo_description") && r["undo_description"].IsString())
        vm.undo_description = r["undo_description"].GetString();
    if (r.HasMember("redo_description") && r["redo_description"].IsString())
        vm.redo_description = r["redo_description"].GetString();
    auto readList = [](const rapidjson::Value& src, std::vector<std::string>& out) {
        if (!src.IsArray()) return;
        for (const auto& s : src.GetArray())
            if (s.IsString()) out.emplace_back(s.GetString());
    };
    if (r.HasMember("undo_history")) readList(r["undo_history"], vm.undo_history);
    if (r.HasMember("redo_history")) readList(r["redo_history"], vm.redo_history);
    vm.valid = true;
    return vm;
}

FindResultVM QueryService::findByName(const std::string& name, bool partial,
                                      dse::runtime::EngineInstance& engine) {
    FindResultVM vm;
    if (!sink_) return vm;

    rapidjson::Document params(rapidjson::kObjectType);
    auto& a = params.GetAllocator();
    params.AddMember("name",
        rapidjson::Value(name.c_str(),
                         static_cast<rapidjson::SizeType>(name.size()), a), a);
    params.AddMember("partial", rapidjson::Value(partial), a);

    dse::editor::JsonRpcResponse resp =
        sink_("dsengine_entity_find_by_name", params, engine);
    if (resp.is_error || !resp.result.IsObject()) return vm;

    const auto& r = resp.result;
    if (r.HasMember("count") && r["count"].IsInt()) vm.count = r["count"].GetInt();
    if (r.HasMember("entity_id") && r["entity_id"].IsUint()) {
        vm.has_first = true;
        vm.first_id = r["entity_id"].GetUint();
    }
    if (r.HasMember("matches") && r["matches"].IsArray()) {
        for (const auto& m : r["matches"].GetArray()) {
            if (!m.IsObject()) continue;
            EntityMatchVM em;
            if (m.HasMember("entity_id") && m["entity_id"].IsUint())
                em.entity_id = m["entity_id"].GetUint();
            if (m.HasMember("name") && m["name"].IsString())
                em.name = m["name"].GetString();
            vm.matches.push_back(std::move(em));
        }
    }
    vm.valid = true;
    return vm;
}

MetricsVM QueryService::metrics(dse::runtime::EngineInstance& engine) {
    MetricsVM vm;
    if (!sink_) return vm;

    rapidjson::Document params(rapidjson::kObjectType);
    dse::editor::JsonRpcResponse resp =
        sink_("dsengine_editor_get_metrics", params, engine);
    if (resp.is_error || !resp.result.IsObject()) return vm;

    const auto& r = resp.result;
    if (r.HasMember("fps") && r["fps"].IsNumber()) vm.fps = r["fps"].GetFloat();
    if (r.HasMember("delta_ms") && r["delta_ms"].IsNumber()) vm.delta_ms = r["delta_ms"].GetFloat();
    if (r.HasMember("draw_calls") && r["draw_calls"].IsInt()) vm.draw_calls = r["draw_calls"].GetInt();
    if (r.HasMember("entity_count") && r["entity_count"].IsInt()) vm.entity_count = r["entity_count"].GetInt();
    if (r.HasMember("time_since_startup") && r["time_since_startup"].IsNumber())
        vm.time_since_startup = r["time_since_startup"].GetFloat();
    if (r.HasMember("editor_state") && r["editor_state"].IsString())
        vm.editor_state = r["editor_state"].GetString();
    vm.valid = true;
    return vm;
}

HierarchyVM QueryService::hierarchy(dse::runtime::EngineInstance& engine) {
    HierarchyVM vm;
    if (!sink_) return vm;

    rapidjson::Document params(rapidjson::kObjectType);
    params.AddMember("include_components", true, params.GetAllocator());

    dse::editor::JsonRpcResponse resp =
        sink_("dsengine_scene_get_state", params, engine);
    if (resp.is_error || !resp.result.IsObject()) return vm;

    const auto& r = resp.result;
    if (!r.HasMember("entities") || !r["entities"].IsArray()) {
        vm.valid = true;  // 空场景也是合法结果
        return vm;
    }

    // 1) 扁平建节点 + id→index 映射
    std::unordered_map<std::uint32_t, std::size_t> index_of;
    std::vector<std::optional<std::uint32_t>> parent_of;
    for (const auto& e : r["entities"].GetArray()) {
        if (!e.IsObject() || !e.HasMember("id") || !e["id"].IsUint()) continue;
        HierarchyNodeVM node;
        node.entity_id = e["id"].GetUint();
        if (e.HasMember("name") && e["name"].IsString()) node.name = e["name"].GetString();
        if (e.HasMember("sibling_index") && e["sibling_index"].IsInt())
            node.sibling_index = e["sibling_index"].GetInt();
        if (e.HasMember("parent_id") && e["parent_id"].IsUint()) {
            node.has_parent = true;
            node.parent_id = e["parent_id"].GetUint();
        }
        if (e.HasMember("components")) ExtractComponentNames(e["components"], node.components);

        index_of[node.entity_id] = vm.nodes.size();
        parent_of.push_back(node.has_parent ? std::optional<std::uint32_t>(node.parent_id)
                                            : std::nullopt);
        vm.nodes.push_back(std::move(node));
    }

    // 2) 连边：父存在则挂到父的 children，否则视为根
    for (std::size_t i = 0; i < vm.nodes.size(); ++i) {
        if (parent_of[i].has_value()) {
            auto it = index_of.find(*parent_of[i]);
            if (it != index_of.end()) {
                vm.nodes[it->second].children.push_back(i);
                continue;
            }
            // 父不存在（异常）→ 退化为根
        }
        vm.roots.push_back(i);
    }

    // 3) 按 sibling_index 排序（与 hierarchy 面板一致）
    auto by_sibling = [&](std::size_t a, std::size_t b) {
        return vm.nodes[a].sibling_index < vm.nodes[b].sibling_index;
    };
    std::sort(vm.roots.begin(), vm.roots.end(), by_sibling);
    for (auto& n : vm.nodes) std::sort(n.children.begin(), n.children.end(), by_sibling);

    vm.valid = true;
    return vm;
}

} // namespace dse::editor::core
