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

} // namespace

CommandResult CommandBus::dispatch(const EditorCommand& cmd,
                                   dse::runtime::EngineInstance& engine) {
    rapidjson::Document params(rapidjson::kObjectType);
    std::string method;

    std::visit(overloaded{
        [&](const CreateEntityCmd& c) {
            method = "dsengine_entity_create";
            if (!c.name.empty()) AddString(params, "name", c.name);
        },
        [&](const DeleteEntityCmd& c) {
            method = "dsengine_entity_delete";
            AddUint(params, "entity_id", c.entity_id);
        },
        [&](const RenameEntityCmd& c) {
            method = "dsengine_entity_modify";
            AddUint(params, "entity_id", c.entity_id);
            AddString(params, "name", c.name);
        },
        [&](const ReparentEntityCmd& c) {
            method = "dsengine_entity_reparent";
            AddUint(params, "entity_id", c.entity_id);
            if (c.parent.has_value()) AddUint(params, "parent_id", *c.parent);
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
