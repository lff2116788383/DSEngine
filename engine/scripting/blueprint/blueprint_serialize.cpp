/**
 * @file blueprint_serialize.cpp
 * @brief Blueprint (.dbp) 共享序列化契约层实现（引擎侧）。
 */

#include "engine/scripting/blueprint/blueprint_serialize.h"

#include <fstream>
#include <sstream>
#include <set>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include "engine/base/debug.h"
#include "engine/core/asset_version_envelope.h"
#include "engine/core/asset_dto.h"

namespace dse::bp {

const char* BpVarTypeName(BpVarType type) {
    switch (type) {
        case BpVarType::Bool:   return "Bool";
        case BpVarType::Int:    return "Int";
        case BpVarType::Float:  return "Float";
        case BpVarType::String: return "String";
        case BpVarType::Vec2:   return "Vec2";
        case BpVarType::Vec3:   return "Vec3";
        case BpVarType::Vec4:   return "Vec4";
        case BpVarType::Entity: return "Entity";
        case BpVarType::Array:  return "Array";
    }
    return "Float";
}

const char* BpPinTypeName(BpPinType type) {
    switch (type) {
        case BpPinType::Flow:     return "Flow";
        case BpPinType::Bool:     return "Bool";
        case BpPinType::Int:      return "Int";
        case BpPinType::Float:    return "Float";
        case BpPinType::String:   return "String";
        case BpPinType::Vec2:     return "Vec2";
        case BpPinType::Vec3:     return "Vec3";
        case BpPinType::Vec4:     return "Vec4";
        case BpPinType::Entity:   return "Entity";
        case BpPinType::Array:    return "Array";
        case BpPinType::Any:      return "Any";
        case BpPinType::Wildcard: return "Wildcard";
    }
    return "Any";
}

BpPinType BpPinTypeFromName(const char* name) {
    if (!name) return BpPinType::Any;
    std::string s = name;
    if (s == "Flow")     return BpPinType::Flow;
    if (s == "Bool")     return BpPinType::Bool;
    if (s == "Int")      return BpPinType::Int;
    if (s == "Float")    return BpPinType::Float;
    if (s == "String")   return BpPinType::String;
    if (s == "Vec2")     return BpPinType::Vec2;
    if (s == "Vec3")     return BpPinType::Vec3;
    if (s == "Vec4")     return BpPinType::Vec4;
    if (s == "Entity")   return BpPinType::Entity;
    if (s == "Array")    return BpPinType::Array;
    if (s == "Wildcard") return BpPinType::Wildcard;
    return BpPinType::Any;
}

namespace {


struct BlueprintDto {
    std::string name;
    std::string description;
    std::string author;
};

struct BpVariableDto {
    std::string name;
    std::string type = "Float";
    std::string array_element_type = "Float";
    bool default_bool = false;
    int default_int = 0;
    float default_float = 0.0f;
    std::string default_string;
    float default_vec[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    bool is_exposed = false;
};

struct BpGraphDto {
    std::string name;
    int next_id = 1;
    bool is_pure = false;
};

struct BpNodeDto {
    int id = 0;
    std::string name;
    std::string category;
    std::string comment;
    float pos_x = 0.0f;
    float pos_y = 0.0f;
};

struct BpPinDto {
    int id = 0;
    std::string name;
    std::string type = "Any";
    float default_float = 0.0f;
    int default_int = 0;
    bool default_bool = false;
    std::string default_string;
    float default_vec[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct BpLinkDto {
    int id = 0;
    int from_pin = 0;
    int to_pin = 0;
};

struct BpParamDto {
    int id = 0;
    std::string name;
    std::string type = "Any";
};

constexpr dse::assets::FieldDesc kBlueprintFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(BlueprintDto, name)},
    {"description", dse::assets::FieldType::String, offsetof(BlueprintDto, description)},
    {"author", dse::assets::FieldType::String, offsetof(BlueprintDto, author)},
};

constexpr dse::assets::FieldDesc kBpVariableFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(BpVariableDto, name)},
    {"type", dse::assets::FieldType::String, offsetof(BpVariableDto, type)},
    {"array_element_type", dse::assets::FieldType::String, offsetof(BpVariableDto, array_element_type)},
    {"default_bool", dse::assets::FieldType::Bool, offsetof(BpVariableDto, default_bool)},
    {"default_int", dse::assets::FieldType::Int, offsetof(BpVariableDto, default_int)},
    {"default_float", dse::assets::FieldType::Float, offsetof(BpVariableDto, default_float)},
    {"default_string", dse::assets::FieldType::String, offsetof(BpVariableDto, default_string)},
    {"default_vec", dse::assets::FieldType::FloatArray4, offsetof(BpVariableDto, default_vec)},
    {"is_exposed", dse::assets::FieldType::Bool, offsetof(BpVariableDto, is_exposed)},
};

constexpr dse::assets::FieldDesc kBpGraphFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(BpGraphDto, name)},
    {"next_id", dse::assets::FieldType::Int, offsetof(BpGraphDto, next_id)},
    {"is_pure", dse::assets::FieldType::Bool, offsetof(BpGraphDto, is_pure)},
};

constexpr dse::assets::FieldDesc kBpNodeFields[] = {
    {"id", dse::assets::FieldType::Int, offsetof(BpNodeDto, id)},
    {"name", dse::assets::FieldType::String, offsetof(BpNodeDto, name)},
    {"category", dse::assets::FieldType::String, offsetof(BpNodeDto, category)},
    {"comment", dse::assets::FieldType::String, offsetof(BpNodeDto, comment)},
    {"pos_x", dse::assets::FieldType::Float, offsetof(BpNodeDto, pos_x)},
    {"pos_y", dse::assets::FieldType::Float, offsetof(BpNodeDto, pos_y)},
};

constexpr dse::assets::FieldDesc kBpPinFields[] = {
    {"id", dse::assets::FieldType::Int, offsetof(BpPinDto, id)},
    {"name", dse::assets::FieldType::String, offsetof(BpPinDto, name)},
    {"type", dse::assets::FieldType::String, offsetof(BpPinDto, type)},
    {"default_float", dse::assets::FieldType::Float, offsetof(BpPinDto, default_float)},
    {"default_int", dse::assets::FieldType::Int, offsetof(BpPinDto, default_int)},
    {"default_bool", dse::assets::FieldType::Bool, offsetof(BpPinDto, default_bool)},
    {"default_string", dse::assets::FieldType::String, offsetof(BpPinDto, default_string)},
    {"default_vec", dse::assets::FieldType::FloatArray4, offsetof(BpPinDto, default_vec)},
};

constexpr dse::assets::FieldDesc kBpLinkFields[] = {
    {"id", dse::assets::FieldType::Int, offsetof(BpLinkDto, id)},
    {"from_pin", dse::assets::FieldType::Int, offsetof(BpLinkDto, from_pin)},
    {"to_pin", dse::assets::FieldType::Int, offsetof(BpLinkDto, to_pin)},
};

constexpr dse::assets::FieldDesc kBpParamFields[] = {
    {"id", dse::assets::FieldType::Int, offsetof(BpParamDto, id)},
    {"name", dse::assets::FieldType::String, offsetof(BpParamDto, name)},
    {"type", dse::assets::FieldType::String, offsetof(BpParamDto, type)},
};


int MaxIdInGraph(const BpFunctionGraph& g) {
    int m = 0;
    for (const auto& n : g.nodes) {
        if (n.id > m) m = n.id;
        for (const auto& p : n.inputs)  if (p.id > m) m = p.id;
        for (const auto& p : n.outputs) if (p.id > m) m = p.id;
    }
    for (const auto& l : g.links) if (l.id > m) m = l.id;
    for (const auto& p : g.input_params)  if (p.id > m) m = p.id;
    for (const auto& p : g.output_params) if (p.id > m) m = p.id;
    return m;
}

// 版本迁移：把 source_version 的资产就地升级到 kBlueprintSchemaVersion。
void MigrateAsset(BlueprintAsset& asset, int source_version, BlueprintDiagnostics& diag) {
    if (source_version >= kBlueprintSchemaVersion) {
        asset.version = kBlueprintSchemaVersion;
        return;
    }
    // v0(legacy，无 version 字段) -> v1：回填缺失/不一致的 next_id，避免后续
    // 在编辑器中新增节点时 id 复用导致连线错乱。
    if (source_version < 1) {
        for (auto& g : asset.graphs) {
            int need = MaxIdInGraph(g) + 1;
            if (g.next_id < need) {
                g.next_id = need;
                diag.migrated = true;
            }
        }
        if (diag.migrated) {
            diag.warnings.push_back(
                "migrated .dbp from legacy(v0) to v1: back-filled graph next_id");
        }
    }
    asset.version = kBlueprintSchemaVersion;
}

const std::set<std::string>& KnownTopLevelKeys() {
    static const std::set<std::string> keys = {
        "name", "version", "description", "author", "variables", "graphs", "interfaces"};
    return keys;
}

void ReadPin(const rapidjson::Value& p, BpPinKind kind, BpPin& pin) {
    pin.kind = kind;
    BpPinDto dto;
    dse::assets::ReadFields(p, kBpPinFields,
                            sizeof(kBpPinFields) / sizeof(kBpPinFields[0]), &dto);
    pin.id = dto.id;
    pin.name = std::move(dto.name);
    pin.type = BpPinTypeFromName(dto.type.c_str());
    pin.default_float = dto.default_float;
    pin.default_int = dto.default_int;
    pin.default_bool = dto.default_bool;
    pin.default_string = std::move(dto.default_string);
    for (int i = 0; i < 4; ++i) pin.default_vec[i] = dto.default_vec[i];
}

}  // namespace

std::string SerializeBlueprintAsset(const BlueprintAsset& asset) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& a = doc.GetAllocator();
    dse::assets::WriteVersionEnvelope(doc, kBlueprintSchemaVersion, a);

    BlueprintDto bdto;
    bdto.name = asset.name;
    bdto.description = asset.description;
    bdto.author = asset.author;
    dse::assets::WriteFields(doc, a, kBlueprintFields,
                             sizeof(kBlueprintFields) / sizeof(kBlueprintFields[0]), &bdto);

    rapidjson::Value variables(rapidjson::kArrayType);
    for (const auto& var : asset.variables) {
        BpVariableDto vdto;
        vdto.name = var.name;
        vdto.type = BpVarTypeName(var.type);
        vdto.array_element_type = BpVarTypeName(var.array_element_type);
        vdto.default_bool = var.default_bool;
        vdto.default_int = var.default_int;
        vdto.default_float = var.default_float;
        vdto.default_string = var.default_string;
        for (int i = 0; i < 4; ++i) vdto.default_vec[i] = var.default_vec[i];
        vdto.is_exposed = var.is_exposed;
        rapidjson::Value vj(rapidjson::kObjectType);
        dse::assets::WriteFields(vj, a, kBpVariableFields,
                                 sizeof(kBpVariableFields) / sizeof(kBpVariableFields[0]), &vdto);
        variables.PushBack(vj, a);
    }
    doc.AddMember("variables", variables, a);

    rapidjson::Value graphs(rapidjson::kArrayType);
    for (const auto& g : asset.graphs) {
        BpGraphDto gdto;
        gdto.name = g.name;
        gdto.next_id = g.next_id;
        gdto.is_pure = g.is_pure;
        rapidjson::Value gj(rapidjson::kObjectType);
        dse::assets::WriteFields(gj, a, kBpGraphFields,
                                 sizeof(kBpGraphFields) / sizeof(kBpGraphFields[0]), &gdto);

        rapidjson::Value nodes(rapidjson::kArrayType);
        for (const auto& n : g.nodes) {
            BpNodeDto ndto;
            ndto.id = n.id;
            ndto.name = n.name;
            ndto.category = n.category;
            ndto.comment = n.comment;
            ndto.pos_x = n.pos_x;
            ndto.pos_y = n.pos_y;
            rapidjson::Value nj(rapidjson::kObjectType);
            dse::assets::WriteFields(nj, a, kBpNodeFields,
                                     sizeof(kBpNodeFields) / sizeof(kBpNodeFields[0]), &ndto);

            auto write_pins = [&](const std::vector<BpPin>& pins) {
                rapidjson::Value arr(rapidjson::kArrayType);
                for (const auto& p : pins) {
                    BpPinDto pdto;
                    pdto.id = p.id;
                    pdto.name = p.name;
                    pdto.type = BpPinTypeName(p.type);
                    pdto.default_float = p.default_float;
                    pdto.default_int = p.default_int;
                    pdto.default_bool = p.default_bool;
                    pdto.default_string = p.default_string;
                    for (int i = 0; i < 4; ++i) pdto.default_vec[i] = p.default_vec[i];
                    rapidjson::Value pj(rapidjson::kObjectType);
                    dse::assets::WriteFields(pj, a, kBpPinFields,
                                             sizeof(kBpPinFields) / sizeof(kBpPinFields[0]), &pdto);
                    arr.PushBack(pj, a);
                }
                return arr;
            };
            nj.AddMember("inputs", write_pins(n.inputs), a);
            nj.AddMember("outputs", write_pins(n.outputs), a);
            nodes.PushBack(nj, a);
        }
        gj.AddMember("nodes", nodes, a);

        rapidjson::Value links(rapidjson::kArrayType);
        for (const auto& l : g.links) {
            BpLinkDto ldto;
            ldto.id = l.id;
            ldto.from_pin = l.from_pin;
            ldto.to_pin = l.to_pin;
            rapidjson::Value lj(rapidjson::kObjectType);
            dse::assets::WriteFields(lj, a, kBpLinkFields,
                                     sizeof(kBpLinkFields) / sizeof(kBpLinkFields[0]), &ldto);
            links.PushBack(lj, a);
        }
        gj.AddMember("links", links, a);

        auto write_params = [&](const std::vector<BpPin>& params) {
            rapidjson::Value arr(rapidjson::kArrayType);
            for (const auto& p : params) {
                BpParamDto pdto;
                pdto.id = p.id;
                pdto.name = p.name;
                pdto.type = BpPinTypeName(p.type);
                rapidjson::Value pj(rapidjson::kObjectType);
                dse::assets::WriteFields(pj, a, kBpParamFields,
                                         sizeof(kBpParamFields) / sizeof(kBpParamFields[0]), &pdto);
                arr.PushBack(pj, a);
            }
            return arr;
        };
        gj.AddMember("input_params", write_params(g.input_params), a);
        gj.AddMember("output_params", write_params(g.output_params), a);
        graphs.PushBack(gj, a);
    }
    doc.AddMember("graphs", graphs, a);

    rapidjson::Value interfaces(rapidjson::kArrayType);
    for (const auto& iface : asset.implemented_interfaces) {
        rapidjson::Value s(iface.c_str(), static_cast<rapidjson::SizeType>(iface.size()), a);
        interfaces.PushBack(s, a);
    }
    doc.AddMember("interfaces", interfaces, a);

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);
    return std::string(sb.GetString(), sb.GetSize());
}

bool DeserializeBlueprintAsset(BlueprintAsset& asset, const std::string& json,
                               BlueprintDiagnostics& diag) {
    diag = BlueprintDiagnostics{};

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
        diag.errors.push_back("root is not a JSON object");
        return false;
    }

    asset = BlueprintAsset{};

    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        std::string key = it->name.GetString();
        if (KnownTopLevelKeys().count(key) == 0) {
            diag.warnings.push_back("unknown top-level field ignored: '" + key + "'");
        }
    }

    const bool has_version = doc.HasMember("version") && doc["version"].IsInt();
    const int source_version =
        dse::assets::ReadVersionEnvelope(doc, kBlueprintSchemaVersion, ".dbp", diag);

    // ADR-3: .dbp read path uses unified DTO/field table.
    BlueprintDto bdto;
    dse::assets::ReadFields(doc, kBlueprintFields,
                            sizeof(kBlueprintFields) / sizeof(kBlueprintFields[0]), &bdto);
    asset.name = std::move(bdto.name);
    asset.description = std::move(bdto.description);
    asset.author = std::move(bdto.author);

    if (has_version) asset.version = source_version;

    if (doc.HasMember("variables") && doc["variables"].IsArray()) {
        for (auto& v : doc["variables"].GetArray()) {
            if (!v.IsObject()) continue;
            BpVariableDto vdto;
            dse::assets::ReadFields(v, kBpVariableFields,
                                    sizeof(kBpVariableFields) / sizeof(kBpVariableFields[0]), &vdto);
            BpVariable var;
            var.name = std::move(vdto.name);
            var.type = BpVarTypeFromName(vdto.type.c_str());
            var.array_element_type = BpVarTypeFromName(vdto.array_element_type.c_str());
            var.default_bool = vdto.default_bool;
            var.default_int = vdto.default_int;
            var.default_float = vdto.default_float;
            var.default_string = std::move(vdto.default_string);
            for (int i = 0; i < 4; ++i) var.default_vec[i] = vdto.default_vec[i];
            var.is_exposed = vdto.is_exposed;
            asset.variables.push_back(std::move(var));
        }
    }

    if (doc.HasMember("graphs") && doc["graphs"].IsArray()) {
        for (auto& g : doc["graphs"].GetArray()) {
            if (!g.IsObject()) continue;
            BpGraphDto gdto;
            dse::assets::ReadFields(g, kBpGraphFields,
                                    sizeof(kBpGraphFields) / sizeof(kBpGraphFields[0]), &gdto);
            BpFunctionGraph graph;
            graph.name = std::move(gdto.name);
            graph.next_id = gdto.next_id;
            graph.is_pure = gdto.is_pure;

            if (g.HasMember("nodes") && g["nodes"].IsArray()) {
                for (auto& n : g["nodes"].GetArray()) {
                    if (!n.IsObject()) continue;
                    BpNodeDto ndto;
                    dse::assets::ReadFields(n, kBpNodeFields,
                                            sizeof(kBpNodeFields) / sizeof(kBpNodeFields[0]), &ndto);
                    BpNode node;
                    node.id = ndto.id;
                    node.name = std::move(ndto.name);
                    node.category = std::move(ndto.category);
                    node.comment = std::move(ndto.comment);
                    node.pos_x = ndto.pos_x;
                    node.pos_y = ndto.pos_y;
                    if (n.HasMember("inputs") && n["inputs"].IsArray()) {
                        for (auto& p : n["inputs"].GetArray()) {
                            if (!p.IsObject()) continue;
                            BpPin pin;
                            ReadPin(p, BpPinKind::Input, pin);
                            node.inputs.push_back(std::move(pin));
                        }
                    }
                    if (n.HasMember("outputs") && n["outputs"].IsArray()) {
                        for (auto& p : n["outputs"].GetArray()) {
                            if (!p.IsObject()) continue;
                            BpPin pin;
                            ReadPin(p, BpPinKind::Output, pin);
                            node.outputs.push_back(std::move(pin));
                        }
                    }
                    graph.nodes.push_back(std::move(node));
                }
            }

            if (g.HasMember("links") && g["links"].IsArray()) {
                for (auto& l : g["links"].GetArray()) {
                    if (!l.IsObject()) continue;
                    BpLinkDto ldto;
                    dse::assets::ReadFields(l, kBpLinkFields,
                                            sizeof(kBpLinkFields) / sizeof(kBpLinkFields[0]), &ldto);
                    BpLink link;
                    link.id = ldto.id;
                    link.from_pin = ldto.from_pin;
                    link.to_pin = ldto.to_pin;
                    graph.links.push_back(link);
                }
            }

            auto load_params = [](const rapidjson::Value& values, BpPinKind kind,
                                  std::vector<BpPin>& destination) {
                if (!values.IsArray()) return;
                for (auto& p : values.GetArray()) {
                    if (!p.IsObject()) continue;
                    BpParamDto pdto;
                    dse::assets::ReadFields(p, kBpParamFields,
                                            sizeof(kBpParamFields) / sizeof(kBpParamFields[0]), &pdto);
                    BpPin pin;
                    pin.kind = kind;
                    pin.id = pdto.id;
                    pin.name = std::move(pdto.name);
                    pin.type = BpPinTypeFromName(pdto.type.c_str());
                    destination.push_back(std::move(pin));
                }
            };
            if (g.HasMember("input_params")) load_params(g["input_params"], BpPinKind::Input, graph.input_params);
            if (g.HasMember("output_params")) load_params(g["output_params"], BpPinKind::Output, graph.output_params);

            asset.graphs.push_back(std::move(graph));
        }
    }

    if (doc.HasMember("interfaces") && doc["interfaces"].IsArray()) {
        for (auto& i : doc["interfaces"].GetArray()) {
            if (i.IsString()) asset.implemented_interfaces.push_back(i.GetString());
        }
    }

    MigrateAsset(asset, source_version, diag);
    diag.ok = true;
    return true;
}

bool SaveBlueprintAsset(const BlueprintAsset& asset, const std::string& path,
                        BlueprintDiagnostics& diag) {
    diag = BlueprintDiagnostics{};
    std::string json = SerializeBlueprintAsset(asset);
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) {
        diag.errors.push_back("cannot open for write: " + path);
        return false;
    }
    ofs.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!ofs.good()) {
        diag.errors.push_back("write failed: " + path);
        return false;
    }
    diag.ok = true;
    diag.source_version = kBlueprintSchemaVersion;
    return true;
}

bool LoadBlueprintAssetChecked(BlueprintAsset& asset, const std::string& path,
                               BlueprintDiagnostics& diag) {
    diag = BlueprintDiagnostics{};
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        diag.errors.push_back("cannot open .dbp: " + path);
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    if (!DeserializeBlueprintAsset(asset, content, diag)) {
        DEBUG_LOG_ERROR("[Blueprint] .dbp 解析失败: %s", path.c_str());
        return false;
    }
    asset.file_path = path;
    return true;
}

}  // namespace dse::bp
