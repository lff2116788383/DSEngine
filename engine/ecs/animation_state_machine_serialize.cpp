/**
 * @file animation_state_machine_serialize.cpp
 * @brief AnimationStateMachine (.dasm) 共享序列化实现。见头文件说明。
 */

#include "engine/ecs/animation_state_machine_serialize.h"

#include <cstring>
#include <fstream>
#include <sstream>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include "engine/base/debug.h"
#include "engine/core/asset_version_envelope.h"

namespace dse {
namespace gameplay3d {

const char* AnimParamTypeName(AnimParamType type) {
    switch (type) {
        case AnimParamType::Float: return "Float";
        case AnimParamType::Int: return "Int";
        case AnimParamType::Bool: return "Bool";
        case AnimParamType::Trigger: return "Trigger";
    }
    return "Float";
}

AnimParamType AnimParamTypeFromName(const char* name) {
    if (std::strcmp(name, "Int") == 0) return AnimParamType::Int;
    if (std::strcmp(name, "Bool") == 0) return AnimParamType::Bool;
    if (std::strcmp(name, "Trigger") == 0) return AnimParamType::Trigger;
    return AnimParamType::Float;
}

const char* AnimConditionModeName(AnimConditionMode mode) {
    switch (mode) {
        case AnimConditionMode::Greater: return "Greater";
        case AnimConditionMode::Less: return "Less";
        case AnimConditionMode::Equals: return "Equals";
        case AnimConditionMode::NotEqual: return "NotEqual";
        case AnimConditionMode::If: return "If";
        case AnimConditionMode::IfNot: return "IfNot";
    }
    return "If";
}

AnimConditionMode AnimConditionModeFromName(const char* name) {
    if (std::strcmp(name, "Greater") == 0) return AnimConditionMode::Greater;
    if (std::strcmp(name, "Less") == 0) return AnimConditionMode::Less;
    if (std::strcmp(name, "Equals") == 0) return AnimConditionMode::Equals;
    if (std::strcmp(name, "NotEqual") == 0) return AnimConditionMode::NotEqual;
    if (std::strcmp(name, "IfNot") == 0) return AnimConditionMode::IfNot;
    return AnimConditionMode::If;
}

namespace {

using Alloc = rapidjson::Document::AllocatorType;

rapidjson::Value Str(const std::string& s, Alloc& alloc) {
    return rapidjson::Value(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
}

std::string GetStr(const rapidjson::Value& v, const char* key, const std::string& def = "") {
    if (v.HasMember(key) && v[key].IsString()) return v[key].GetString();
    return def;
}

float GetFloat(const rapidjson::Value& v, const char* key, float def = 0.0f) {
    if (v.HasMember(key) && v[key].IsNumber()) return v[key].GetFloat();
    return def;
}

int GetInt(const rapidjson::Value& v, const char* key, int def = 0) {
    if (v.HasMember(key) && v[key].IsInt()) return v[key].GetInt();
    return def;
}

bool GetBool(const rapidjson::Value& v, const char* key, bool def = false) {
    if (v.HasMember(key) && v[key].IsBool()) return v[key].GetBool();
    return def;
}

}  // namespace

void WriteStateMachineJson(const AnimationStateMachine& sm, rapidjson::Value& out, Alloc& alloc) {
    out.SetObject();
    out.AddMember("default_state", Str(sm.GetDefaultState(), alloc), alloc);

    rapidjson::Value params(rapidjson::kArrayType);
    for (const auto& [name, p] : sm.GetParameters()) {
        rapidjson::Value pj(rapidjson::kObjectType);
        pj.AddMember("name", Str(name, alloc), alloc);
        pj.AddMember("type", Str(AnimParamTypeName(p.type), alloc), alloc);
        switch (p.type) {
            case AnimParamType::Float:
                pj.AddMember("value", std::holds_alternative<float>(p.value) ? std::get<float>(p.value) : 0.0f, alloc);
                break;
            case AnimParamType::Int:
                pj.AddMember("value", std::holds_alternative<int>(p.value) ? std::get<int>(p.value) : 0, alloc);
                break;
            case AnimParamType::Bool:
                pj.AddMember("value", std::holds_alternative<bool>(p.value) ? std::get<bool>(p.value) : false, alloc);
                break;
            case AnimParamType::Trigger:
                pj.AddMember("triggered", p.is_triggered, alloc);
                break;
        }
        params.PushBack(pj, alloc);
    }
    out.AddMember("parameters", params, alloc);

    rapidjson::Value states(rapidjson::kArrayType);
    for (const auto& [name, st] : sm.GetStates()) {
        rapidjson::Value sj(rapidjson::kObjectType);
        sj.AddMember("name", Str(st.name, alloc), alloc);
        sj.AddMember("danim_path", Str(st.danim_path, alloc), alloc);
        sj.AddMember("speed", st.speed, alloc);
        sj.AddMember("loop", st.loop, alloc);
        sj.AddMember("is_blend_tree", st.is_blend_tree, alloc);
        sj.AddMember("blend_parameter", Str(st.blend_parameter, alloc), alloc);

        rapidjson::Value blend_nodes(rapidjson::kArrayType);
        for (const auto& bn : st.blend_nodes) {
            rapidjson::Value bj(rapidjson::kObjectType);
            bj.AddMember("danim_path", Str(bn.danim_path, alloc), alloc);
            bj.AddMember("threshold", bn.threshold, alloc);
            blend_nodes.PushBack(bj, alloc);
        }
        sj.AddMember("blend_nodes", blend_nodes, alloc);

        rapidjson::Value transitions(rapidjson::kArrayType);
        for (const auto& tr : st.transitions) {
            rapidjson::Value tj(rapidjson::kObjectType);
            tj.AddMember("target_state", Str(tr.target_state, alloc), alloc);
            tj.AddMember("has_exit_time", tr.has_exit_time, alloc);
            tj.AddMember("exit_time", tr.exit_time, alloc);
            tj.AddMember("transition_duration", tr.transition_duration, alloc);
            rapidjson::Value conds(rapidjson::kArrayType);
            for (const auto& c : tr.conditions) {
                rapidjson::Value cj(rapidjson::kObjectType);
                cj.AddMember("parameter_name", Str(c.parameter_name, alloc), alloc);
                cj.AddMember("mode", Str(AnimConditionModeName(c.mode), alloc), alloc);
                cj.AddMember("threshold", c.threshold, alloc);
                cj.AddMember("int_value", c.int_value, alloc);
                conds.PushBack(cj, alloc);
            }
            tj.AddMember("conditions", conds, alloc);
            transitions.PushBack(tj, alloc);
        }
        sj.AddMember("transitions", transitions, alloc);
        states.PushBack(sj, alloc);
    }
    out.AddMember("states", states, alloc);
}

bool ReadStateMachineJson(const rapidjson::Value& in, AnimationStateMachine& sm, AsmDiagnostics& diag) {
    if (!in.IsObject()) {
        diag.errors.push_back("state machine node is not an object");
        return false;
    }

    if (in.HasMember("parameters") && in["parameters"].IsArray()) {
        for (const auto& pj : in["parameters"].GetArray()) {
            if (!pj.IsObject() || !pj.HasMember("name")) continue;
            std::string name = GetStr(pj, "name");
            if (name.empty()) continue;
            AnimParamType type = AnimParamTypeFromName(GetStr(pj, "type", "Float").c_str());
            switch (type) {
                case AnimParamType::Float:
                    sm.AddParameter(name, type, GetFloat(pj, "value", 0.0f));
                    break;
                case AnimParamType::Int:
                    sm.AddParameter(name, type, GetInt(pj, "value", 0));
                    break;
                case AnimParamType::Bool:
                    sm.AddParameter(name, type, GetBool(pj, "value", false));
                    break;
                case AnimParamType::Trigger:
                    sm.AddTrigger(name);
                    if (GetBool(pj, "triggered", false)) sm.SetTrigger(name);
                    break;
            }
        }
    }

    if (in.HasMember("states") && in["states"].IsArray()) {
        for (const auto& sj : in["states"].GetArray()) {
            if (!sj.IsObject() || !sj.HasMember("name")) continue;
            AnimState st;
            st.name = GetStr(sj, "name");
            if (st.name.empty()) continue;
            st.danim_path = GetStr(sj, "danim_path");
            st.speed = GetFloat(sj, "speed", 1.0f);
            st.loop = GetBool(sj, "loop", true);
            st.is_blend_tree = GetBool(sj, "is_blend_tree", false);
            st.blend_parameter = GetStr(sj, "blend_parameter");

            if (sj.HasMember("blend_nodes") && sj["blend_nodes"].IsArray()) {
                for (const auto& bj : sj["blend_nodes"].GetArray()) {
                    if (!bj.IsObject()) continue;
                    BlendTreeNode bn;
                    bn.danim_path = GetStr(bj, "danim_path");
                    bn.threshold = GetFloat(bj, "threshold", 0.0f);
                    st.blend_nodes.push_back(bn);
                }
            }

            if (sj.HasMember("transitions") && sj["transitions"].IsArray()) {
                for (const auto& tj : sj["transitions"].GetArray()) {
                    if (!tj.IsObject()) continue;
                    AnimTransition tr;
                    tr.target_state = GetStr(tj, "target_state");
                    tr.has_exit_time = GetBool(tj, "has_exit_time", true);
                    tr.exit_time = GetFloat(tj, "exit_time", 1.0f);
                    tr.transition_duration = GetFloat(tj, "transition_duration", 0.25f);
                    if (tj.HasMember("conditions") && tj["conditions"].IsArray()) {
                        for (const auto& cj : tj["conditions"].GetArray()) {
                            if (!cj.IsObject()) continue;
                            AnimTransitionCondition c;
                            c.parameter_name = GetStr(cj, "parameter_name");
                            c.mode = AnimConditionModeFromName(GetStr(cj, "mode", "If").c_str());
                            c.threshold = GetFloat(cj, "threshold", 0.0f);
                            c.int_value = GetInt(cj, "int_value", 0);
                            tr.conditions.push_back(c);
                        }
                    }
                    st.transitions.push_back(tr);
                }
            }
            sm.AddState(st);
        }
    }

    // AddState 会把首个状态设为默认；显式覆盖为文件中声明的默认状态。
    std::string def = GetStr(in, "default_state");
    if (!def.empty()) sm.SetDefaultState(def);

    diag.ok = true;
    return true;
}

std::string SerializeStateMachine(const AnimationStateMachine& sm) {
    rapidjson::Document doc;
    auto& alloc = doc.GetAllocator();
    doc.SetObject();
    dse::assets::WriteVersionEnvelope(doc, kAnimStateMachineSchemaVersion, alloc);
    rapidjson::Value body(rapidjson::kObjectType);
    WriteStateMachineJson(sm, body, alloc);
    doc.AddMember("state_machine", body, alloc);

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);
    return std::string(sb.GetString(), sb.GetSize());
}

bool DeserializeStateMachine(AnimationStateMachine& sm, const std::string& json, AsmDiagnostics& diag) {
    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError()) {
        std::ostringstream os;
        os << "JSON parse error at offset " << doc.GetErrorOffset()
           << " (code " << static_cast<int>(doc.GetParseError()) << ")";
        diag.errors.push_back(os.str());
        return false;
    }
    if (!doc.IsObject()) {
        diag.errors.push_back("root is not an object");
        return false;
    }

    dse::assets::ReadVersionEnvelope(doc, kAnimStateMachineSchemaVersion, ".dasm", diag);

    // 版本 0（legacy，无 version 字段）视为直接内嵌状态机对象，无 state_machine 包裹。
    const rapidjson::Value* body = nullptr;
    if (doc.HasMember("state_machine") && doc["state_machine"].IsObject()) {
        body = &doc["state_machine"];
    } else {
        body = &doc;
        if (diag.source_version == 0) diag.migrated = true;
    }

    return ReadStateMachineJson(*body, sm, diag);
}

bool SaveStateMachineToFile(const AnimationStateMachine& sm, const std::string& path, AsmDiagnostics& diag) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) {
        diag.errors.push_back("cannot open file for writing: " + path);
        return false;
    }
    std::string text = SerializeStateMachine(sm);
    ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!ofs) {
        diag.errors.push_back("write failed: " + path);
        return false;
    }
    diag.ok = true;
    return true;
}

bool LoadStateMachineFromFile(AnimationStateMachine& sm, const std::string& path, AsmDiagnostics& diag) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        diag.errors.push_back("cannot open file for reading: " + path);
        return false;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return DeserializeStateMachine(sm, ss.str(), diag);
}

}  // namespace gameplay3d
}  // namespace dse
