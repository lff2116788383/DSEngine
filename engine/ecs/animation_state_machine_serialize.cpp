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
#include "engine/core/asset_dto.h"

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

struct AnimParamDto {
    std::string name;
    std::string type = "Float";
    bool triggered = false;
};

struct AnimStateDto {
    std::string name;
    std::string danim_path;
    float speed = 1.0f;
    bool loop = true;
    bool is_blend_tree = false;
    std::string blend_parameter;
};

struct BlendTreeNodeDto {
    std::string danim_path;
    float threshold = 0.0f;
};

struct AnimTransitionDto {
    std::string target_state;
    bool has_exit_time = true;
    float exit_time = 1.0f;
    float transition_duration = 0.25f;
};

struct AnimTransitionConditionDto {
    std::string parameter_name;
    std::string mode = "If";
    float threshold = 0.0f;
    int int_value = 0;
};

struct AnimStateMachineDto {
    std::string default_state;
};

constexpr dse::assets::FieldDesc kAnimParamFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(AnimParamDto, name)},
    {"type", dse::assets::FieldType::String, offsetof(AnimParamDto, type)},
    {"triggered", dse::assets::FieldType::Bool, offsetof(AnimParamDto, triggered)},
};

constexpr dse::assets::FieldDesc kAnimStateFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(AnimStateDto, name)},
    {"danim_path", dse::assets::FieldType::String, offsetof(AnimStateDto, danim_path)},
    {"speed", dse::assets::FieldType::Float, offsetof(AnimStateDto, speed)},
    {"loop", dse::assets::FieldType::Bool, offsetof(AnimStateDto, loop)},
    {"is_blend_tree", dse::assets::FieldType::Bool, offsetof(AnimStateDto, is_blend_tree)},
    {"blend_parameter", dse::assets::FieldType::String, offsetof(AnimStateDto, blend_parameter)},
};

constexpr dse::assets::FieldDesc kBlendTreeNodeFields[] = {
    {"danim_path", dse::assets::FieldType::String, offsetof(BlendTreeNodeDto, danim_path)},
    {"threshold", dse::assets::FieldType::Float, offsetof(BlendTreeNodeDto, threshold)},
};

constexpr dse::assets::FieldDesc kAnimTransitionFields[] = {
    {"target_state", dse::assets::FieldType::String, offsetof(AnimTransitionDto, target_state)},
    {"has_exit_time", dse::assets::FieldType::Bool, offsetof(AnimTransitionDto, has_exit_time)},
    {"exit_time", dse::assets::FieldType::Float, offsetof(AnimTransitionDto, exit_time)},
    {"transition_duration", dse::assets::FieldType::Float, offsetof(AnimTransitionDto, transition_duration)},
};

constexpr dse::assets::FieldDesc kAnimTransitionConditionFields[] = {
    {"parameter_name", dse::assets::FieldType::String, offsetof(AnimTransitionConditionDto, parameter_name)},
    {"mode", dse::assets::FieldType::String, offsetof(AnimTransitionConditionDto, mode)},
    {"threshold", dse::assets::FieldType::Float, offsetof(AnimTransitionConditionDto, threshold)},
    {"int_value", dse::assets::FieldType::Int, offsetof(AnimTransitionConditionDto, int_value)},
};

constexpr dse::assets::FieldDesc kAnimStateMachineFields[] = {
    {"default_state", dse::assets::FieldType::String, offsetof(AnimStateMachineDto, default_state)},
};


using Alloc = rapidjson::Document::AllocatorType;

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
    AnimStateMachineDto smdto;
    smdto.default_state = sm.GetDefaultState();
    dse::assets::WriteFields(out, alloc, kAnimStateMachineFields,
                             sizeof(kAnimStateMachineFields) / sizeof(kAnimStateMachineFields[0]), &smdto);

    rapidjson::Value params(rapidjson::kArrayType);
    for (const auto& [name, p] : sm.GetParameters()) {
        AnimParamDto pdto;
        pdto.name = name;
        pdto.type = AnimParamTypeName(p.type);
        pdto.triggered = p.is_triggered;
        rapidjson::Value pj(rapidjson::kObjectType);
        dse::assets::WriteFields(pj, alloc, kAnimParamFields,
                                 sizeof(kAnimParamFields) / sizeof(kAnimParamFields[0]), &pdto);
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
                break;
        }
        params.PushBack(pj, alloc);
    }
    out.AddMember("parameters", params, alloc);

    rapidjson::Value states(rapidjson::kArrayType);
    for (const auto& [name, st] : sm.GetStates()) {
        (void)name;
        AnimStateDto sdto;
        sdto.name = st.name;
        sdto.danim_path = st.danim_path;
        sdto.speed = st.speed;
        sdto.loop = st.loop;
        sdto.is_blend_tree = st.is_blend_tree;
        sdto.blend_parameter = st.blend_parameter;
        rapidjson::Value sj(rapidjson::kObjectType);
        dse::assets::WriteFields(sj, alloc, kAnimStateFields,
                                 sizeof(kAnimStateFields) / sizeof(kAnimStateFields[0]), &sdto);

        rapidjson::Value blend_nodes(rapidjson::kArrayType);
        for (const auto& bn : st.blend_nodes) {
            BlendTreeNodeDto bdto;
            bdto.danim_path = bn.danim_path;
            bdto.threshold = bn.threshold;
            rapidjson::Value bj(rapidjson::kObjectType);
            dse::assets::WriteFields(bj, alloc, kBlendTreeNodeFields,
                                     sizeof(kBlendTreeNodeFields) / sizeof(kBlendTreeNodeFields[0]), &bdto);
            blend_nodes.PushBack(bj, alloc);
        }
        sj.AddMember("blend_nodes", blend_nodes, alloc);

        rapidjson::Value transitions(rapidjson::kArrayType);
        for (const auto& tr : st.transitions) {
            AnimTransitionDto tdto;
            tdto.target_state = tr.target_state;
            tdto.has_exit_time = tr.has_exit_time;
            tdto.exit_time = tr.exit_time;
            tdto.transition_duration = tr.transition_duration;
            rapidjson::Value tj(rapidjson::kObjectType);
            dse::assets::WriteFields(tj, alloc, kAnimTransitionFields,
                                     sizeof(kAnimTransitionFields) / sizeof(kAnimTransitionFields[0]), &tdto);

            rapidjson::Value conds(rapidjson::kArrayType);
            for (const auto& c : tr.conditions) {
                AnimTransitionConditionDto cdto;
                cdto.parameter_name = c.parameter_name;
                cdto.mode = AnimConditionModeName(c.mode);
                cdto.threshold = c.threshold;
                cdto.int_value = c.int_value;
                rapidjson::Value cj(rapidjson::kObjectType);
                dse::assets::WriteFields(cj, alloc, kAnimTransitionConditionFields,
                                         sizeof(kAnimTransitionConditionFields) / sizeof(kAnimTransitionConditionFields[0]), &cdto);
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

    // ADR-3: .dasm read path uses unified DTO/field table.
    AnimStateMachineDto smdto;
    dse::assets::ReadFields(in, kAnimStateMachineFields,
                            sizeof(kAnimStateMachineFields) / sizeof(kAnimStateMachineFields[0]), &smdto);

    if (in.HasMember("parameters") && in["parameters"].IsArray()) {
        for (const auto& pj : in["parameters"].GetArray()) {
            if (!pj.IsObject()) continue;
            AnimParamDto pdto;
            dse::assets::ReadFields(pj, kAnimParamFields,
                                    sizeof(kAnimParamFields) / sizeof(kAnimParamFields[0]), &pdto);
            if (pdto.name.empty()) continue;
            AnimParamType type = AnimParamTypeFromName(pdto.type.c_str());
            switch (type) {
                case AnimParamType::Float:
                    sm.AddParameter(pdto.name, type, GetFloat(pj, "value", 0.0f));
                    break;
                case AnimParamType::Int:
                    sm.AddParameter(pdto.name, type, GetInt(pj, "value", 0));
                    break;
                case AnimParamType::Bool:
                    sm.AddParameter(pdto.name, type, GetBool(pj, "value", false));
                    break;
                case AnimParamType::Trigger:
                    sm.AddTrigger(pdto.name);
                    if (pdto.triggered) sm.SetTrigger(pdto.name);
                    break;
            }
        }
    }

    if (in.HasMember("states") && in["states"].IsArray()) {
        for (const auto& sj : in["states"].GetArray()) {
            if (!sj.IsObject()) continue;
            AnimStateDto sdto;
            dse::assets::ReadFields(sj, kAnimStateFields,
                                    sizeof(kAnimStateFields) / sizeof(kAnimStateFields[0]), &sdto);
            if (sdto.name.empty()) continue;

            AnimState st;
            st.name = std::move(sdto.name);
            st.danim_path = std::move(sdto.danim_path);
            st.speed = sdto.speed;
            st.loop = sdto.loop;
            st.is_blend_tree = sdto.is_blend_tree;
            st.blend_parameter = std::move(sdto.blend_parameter);

            if (sj.HasMember("blend_nodes") && sj["blend_nodes"].IsArray()) {
                for (const auto& bj : sj["blend_nodes"].GetArray()) {
                    if (!bj.IsObject()) continue;
                    BlendTreeNodeDto bdto;
                    dse::assets::ReadFields(bj, kBlendTreeNodeFields,
                                            sizeof(kBlendTreeNodeFields) / sizeof(kBlendTreeNodeFields[0]), &bdto);
                    BlendTreeNode bn;
                    bn.danim_path = std::move(bdto.danim_path);
                    bn.threshold = bdto.threshold;
                    st.blend_nodes.push_back(std::move(bn));
                }
            }

            if (sj.HasMember("transitions") && sj["transitions"].IsArray()) {
                for (const auto& tj : sj["transitions"].GetArray()) {
                    if (!tj.IsObject()) continue;
                    AnimTransitionDto tdto;
                    dse::assets::ReadFields(tj, kAnimTransitionFields,
                                            sizeof(kAnimTransitionFields) / sizeof(kAnimTransitionFields[0]), &tdto);
                    AnimTransition tr;
                    tr.target_state = std::move(tdto.target_state);
                    tr.has_exit_time = tdto.has_exit_time;
                    tr.exit_time = tdto.exit_time;
                    tr.transition_duration = tdto.transition_duration;

                    if (tj.HasMember("conditions") && tj["conditions"].IsArray()) {
                        for (const auto& cj : tj["conditions"].GetArray()) {
                            if (!cj.IsObject()) continue;
                            AnimTransitionConditionDto cdto;
                            dse::assets::ReadFields(cj, kAnimTransitionConditionFields,
                                                    sizeof(kAnimTransitionConditionFields) / sizeof(kAnimTransitionConditionFields[0]), &cdto);
                            AnimTransitionCondition c;
                            c.parameter_name = std::move(cdto.parameter_name);
                            c.mode = AnimConditionModeFromName(cdto.mode.c_str());
                            c.threshold = cdto.threshold;
                            c.int_value = cdto.int_value;
                            tr.conditions.push_back(std::move(c));
                        }
                    }
                    st.transitions.push_back(std::move(tr));
                }
            }
            sm.AddState(st);
        }
    }

    // AddState 浼氭妸棣栦釜鐘舵佽缃负榛樿锛涙樉寮忚鐩栦负鏂囦欢涓０鏄庣殑榛樿鐘舵併?
    if (!smdto.default_state.empty()) sm.SetDefaultState(smdto.default_state);

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
