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

using Writer = rapidjson::PrettyWriter<rapidjson::StringBuffer>;

void WritePin(Writer& w, const BpPin& pin) {
    w.StartObject();
    w.Key("id");             w.Int(pin.id);
    w.Key("name");           w.String(pin.name.c_str());
    w.Key("type");           w.String(BpPinTypeName(pin.type));
    w.Key("default_float");  w.Double(static_cast<double>(pin.default_float));
    w.Key("default_int");    w.Int(pin.default_int);
    w.Key("default_bool");   w.Bool(pin.default_bool);
    w.Key("default_string"); w.String(pin.default_string.c_str());
    w.Key("default_vec");
    w.StartArray();
    for (int i = 0; i < 4; ++i) w.Double(static_cast<double>(pin.default_vec[i]));
    w.EndArray();
    w.EndObject();
}

void WriteParam(Writer& w, const BpPin& pin) {
    w.StartObject();
    w.Key("id");   w.Int(pin.id);
    w.Key("name"); w.String(pin.name.c_str());
    w.Key("type"); w.String(BpPinTypeName(pin.type));
    w.EndObject();
}

// 计算图内出现过的最大 id（节点/引脚/连线），用于 legacy 迁移回填 next_id。
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
    if (p.HasMember("id") && p["id"].IsInt()) pin.id = p["id"].GetInt();
    if (p.HasMember("name") && p["name"].IsString()) pin.name = p["name"].GetString();
    if (p.HasMember("type") && p["type"].IsString()) pin.type = BpPinTypeFromName(p["type"].GetString());
    if (p.HasMember("default_float") && p["default_float"].IsNumber()) pin.default_float = p["default_float"].GetFloat();
    if (p.HasMember("default_int") && p["default_int"].IsInt()) pin.default_int = p["default_int"].GetInt();
    if (p.HasMember("default_bool") && p["default_bool"].IsBool()) pin.default_bool = p["default_bool"].GetBool();
    if (p.HasMember("default_string") && p["default_string"].IsString()) pin.default_string = p["default_string"].GetString();
    if (p.HasMember("default_vec") && p["default_vec"].IsArray()) {
        auto arr = p["default_vec"].GetArray();
        for (int i = 0; i < 4 && i < static_cast<int>(arr.Size()); ++i)
            if (arr[i].IsNumber()) pin.default_vec[i] = arr[i].GetFloat();
    }
}

}  // namespace

std::string SerializeBlueprintAsset(const BlueprintAsset& asset) {
    rapidjson::StringBuffer sb;
    Writer w(sb);
    w.StartObject();
    w.Key("name");        w.String(asset.name.c_str());
    w.Key("version");     w.Int(kBlueprintSchemaVersion);
    w.Key("description"); w.String(asset.description.c_str());
    w.Key("author");      w.String(asset.author.c_str());

    w.Key("variables");
    w.StartArray();
    for (const auto& var : asset.variables) {
        w.StartObject();
        w.Key("name");           w.String(var.name.c_str());
        w.Key("type");           w.String(BpVarTypeName(var.type));
        w.Key("default_bool");   w.Bool(var.default_bool);
        w.Key("default_int");    w.Int(var.default_int);
        w.Key("default_float");  w.Double(static_cast<double>(var.default_float));
        w.Key("default_string"); w.String(var.default_string.c_str());
        w.Key("default_vec");
        w.StartArray();
        for (int i = 0; i < 4; ++i) w.Double(static_cast<double>(var.default_vec[i]));
        w.EndArray();
        w.Key("is_exposed");     w.Bool(var.is_exposed);
        w.EndObject();
    }
    w.EndArray();

    w.Key("graphs");
    w.StartArray();
    for (const auto& g : asset.graphs) {
        w.StartObject();
        w.Key("name");    w.String(g.name.c_str());
        w.Key("next_id"); w.Int(g.next_id);
        w.Key("is_pure"); w.Bool(g.is_pure);

        w.Key("nodes");
        w.StartArray();
        for (const auto& n : g.nodes) {
            w.StartObject();
            w.Key("id");       w.Int(n.id);
            w.Key("name");     w.String(n.name.c_str());
            w.Key("category"); w.String(n.category.c_str());
            w.Key("pos_x");    w.Double(static_cast<double>(n.pos_x));
            w.Key("pos_y");    w.Double(static_cast<double>(n.pos_y));
            w.Key("comment");  w.String(n.comment.c_str());
            w.Key("inputs");
            w.StartArray();
            for (const auto& p : n.inputs) WritePin(w, p);
            w.EndArray();
            w.Key("outputs");
            w.StartArray();
            for (const auto& p : n.outputs) WritePin(w, p);
            w.EndArray();
            w.EndObject();
        }
        w.EndArray();

        w.Key("links");
        w.StartArray();
        for (const auto& l : g.links) {
            w.StartObject();
            w.Key("id");       w.Int(l.id);
            w.Key("from_pin"); w.Int(l.from_pin);
            w.Key("to_pin");   w.Int(l.to_pin);
            w.EndObject();
        }
        w.EndArray();

        w.Key("input_params");
        w.StartArray();
        for (const auto& p : g.input_params) WriteParam(w, p);
        w.EndArray();
        w.Key("output_params");
        w.StartArray();
        for (const auto& p : g.output_params) WriteParam(w, p);
        w.EndArray();

        w.EndObject();
    }
    w.EndArray();

    w.Key("interfaces");
    w.StartArray();
    for (const auto& iface : asset.implemented_interfaces) w.String(iface.c_str());
    w.EndArray();

    w.EndObject();
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

    // 前向兼容：记录未知顶层字段，但不失败、不静默丢弃。
    for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
        std::string key = it->name.GetString();
        if (KnownTopLevelKeys().count(key) == 0) {
            diag.warnings.push_back("unknown top-level field ignored: '" + key + "'");
        }
    }

    // 版本：缺失视为 legacy(0)。字段级读取与前向兼容策略走共享信封助手；
    // has_version 仍单独判定，仅当文件显式带 version 时才回填 asset.version。
    const bool has_version = doc.HasMember("version") && doc["version"].IsInt();
    const int source_version =
        dse::assets::ReadVersionEnvelope(doc, kBlueprintSchemaVersion, ".dbp", diag);

    if (doc.HasMember("name") && doc["name"].IsString()) asset.name = doc["name"].GetString();
    if (has_version) asset.version = source_version;
    if (doc.HasMember("description") && doc["description"].IsString())
        asset.description = doc["description"].GetString();
    if (doc.HasMember("author") && doc["author"].IsString())
        asset.author = doc["author"].GetString();

    if (doc.HasMember("variables") && doc["variables"].IsArray()) {
        for (auto& v : doc["variables"].GetArray()) {
            if (!v.IsObject()) continue;
            BpVariable var;
            if (v.HasMember("name") && v["name"].IsString()) var.name = v["name"].GetString();
            if (v.HasMember("type") && v["type"].IsString()) var.type = BpVarTypeFromName(v["type"].GetString());
            if (v.HasMember("default_bool") && v["default_bool"].IsBool()) var.default_bool = v["default_bool"].GetBool();
            if (v.HasMember("default_int") && v["default_int"].IsInt()) var.default_int = v["default_int"].GetInt();
            if (v.HasMember("default_float") && v["default_float"].IsNumber()) var.default_float = v["default_float"].GetFloat();
            if (v.HasMember("default_string") && v["default_string"].IsString()) var.default_string = v["default_string"].GetString();
            if (v.HasMember("default_vec") && v["default_vec"].IsArray()) {
                auto arr = v["default_vec"].GetArray();
                for (int i = 0; i < 4 && i < static_cast<int>(arr.Size()); ++i)
                    if (arr[i].IsNumber()) var.default_vec[i] = arr[i].GetFloat();
            }
            if (v.HasMember("is_exposed") && v["is_exposed"].IsBool()) var.is_exposed = v["is_exposed"].GetBool();
            asset.variables.push_back(std::move(var));
        }
    }

    if (doc.HasMember("graphs") && doc["graphs"].IsArray()) {
        for (auto& g : doc["graphs"].GetArray()) {
            if (!g.IsObject()) continue;
            BpFunctionGraph graph;
            if (g.HasMember("name") && g["name"].IsString()) graph.name = g["name"].GetString();
            if (g.HasMember("next_id") && g["next_id"].IsInt()) graph.next_id = g["next_id"].GetInt();
            if (g.HasMember("is_pure") && g["is_pure"].IsBool()) graph.is_pure = g["is_pure"].GetBool();

            if (g.HasMember("nodes") && g["nodes"].IsArray()) {
                for (auto& n : g["nodes"].GetArray()) {
                    if (!n.IsObject()) continue;
                    BpNode node;
                    if (n.HasMember("id") && n["id"].IsInt()) node.id = n["id"].GetInt();
                    if (n.HasMember("name") && n["name"].IsString()) node.name = n["name"].GetString();
                    if (n.HasMember("category") && n["category"].IsString()) node.category = n["category"].GetString();
                    if (n.HasMember("pos_x") && n["pos_x"].IsNumber()) node.pos_x = n["pos_x"].GetFloat();
                    if (n.HasMember("pos_y") && n["pos_y"].IsNumber()) node.pos_y = n["pos_y"].GetFloat();
                    if (n.HasMember("comment") && n["comment"].IsString()) node.comment = n["comment"].GetString();
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
                    BpLink link;
                    if (l.HasMember("id") && l["id"].IsInt()) link.id = l["id"].GetInt();
                    if (l.HasMember("from_pin") && l["from_pin"].IsInt()) link.from_pin = l["from_pin"].GetInt();
                    if (l.HasMember("to_pin") && l["to_pin"].IsInt()) link.to_pin = l["to_pin"].GetInt();
                    graph.links.push_back(link);
                }
            }

            auto load_params = [](const rapidjson::Value& values, BpPinKind kind,
                                  std::vector<BpPin>& destination) {
                if (!values.IsArray()) return;
                for (auto& p : values.GetArray()) {
                    if (!p.IsObject()) continue;
                    BpPin pin;
                    pin.kind = kind;
                    if (p.HasMember("id") && p["id"].IsInt()) pin.id = p["id"].GetInt();
                    if (p.HasMember("name") && p["name"].IsString()) pin.name = p["name"].GetString();
                    if (p.HasMember("type") && p["type"].IsString()) pin.type = BpPinTypeFromName(p["type"].GetString());
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
