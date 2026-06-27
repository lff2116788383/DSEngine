#include "apps/editor_cpp/core/query_service.h"

namespace dse::editor::core {

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

} // namespace dse::editor::core
