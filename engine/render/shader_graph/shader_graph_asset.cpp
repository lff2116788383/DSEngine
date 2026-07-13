/**
 * @file shader_graph_asset.cpp
 * @brief Shader Graph 版本化 .dshadergraph 序列化实现
 */

#include "engine/render/shader_graph/shader_graph_asset.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_set>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace dse {
namespace shadergraph {

namespace {

using Alloc = rapidjson::Document::AllocatorType;

rapidjson::Value Str(const std::string& s, Alloc& alloc) {
    return rapidjson::Value(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
}

int ReadInt(const rapidjson::Value& obj, const char* key, int fallback) {
    if (obj.HasMember(key) && obj[key].IsInt()) return obj[key].GetInt();
    return fallback;
}

std::string ReadString(const rapidjson::Value& obj, const char* key) {
    if (obj.HasMember(key) && obj[key].IsString()) return obj[key].GetString();
    return std::string();
}

void WritePin(const PinDesc& p, rapidjson::Value& out, Alloc& alloc) {
    out.SetObject();
    out.AddMember("id", p.id, alloc);
    out.AddMember("name", Str(p.name, alloc), alloc);
    out.AddMember("type", Str(PinTypeName(p.type), alloc), alloc);
    out.AddMember("kind", p.kind == PinKind::Input ? 0 : 1, alloc);
    rapidjson::Value def(rapidjson::kArrayType);
    for (float v : p.default_value) def.PushBack(v, alloc);
    out.AddMember("default", def, alloc);
}

PinDesc ReadPin(const rapidjson::Value& in) {
    PinDesc p;
    p.id = ReadInt(in, "id", 0);
    p.name = ReadString(in, "name");
    p.type = PinTypeFromName(ReadString(in, "type").c_str());
    p.kind = ReadInt(in, "kind", 0) == 0 ? PinKind::Input : PinKind::Output;
    if (in.HasMember("default") && in["default"].IsArray()) {
        const auto& arr = in["default"];
        for (rapidjson::SizeType i = 0; i < arr.Size() && i < 4; ++i) {
            if (arr[i].IsNumber()) p.default_value[i] = arr[i].GetFloat();
        }
    }
    return p;
}

void WriteGraphBody(const ShaderGraphAsset& g, rapidjson::Value& out, Alloc& alloc) {
    out.SetObject();
    out.AddMember("next_id", g.next_id, alloc);

    rapidjson::Value nodes(rapidjson::kArrayType);
    for (const auto& n : g.nodes) {
        rapidjson::Value nj(rapidjson::kObjectType);
        nj.AddMember("id", n.id, alloc);
        nj.AddMember("name", Str(n.name, alloc), alloc);
        nj.AddMember("category", Str(n.category, alloc), alloc);
        rapidjson::Value pos(rapidjson::kArrayType);
        pos.PushBack(n.pos[0], alloc).PushBack(n.pos[1], alloc);
        nj.AddMember("pos", pos, alloc);
        nj.AddMember("color", n.header_color, alloc);
        rapidjson::Value inputs(rapidjson::kArrayType);
        for (const auto& p : n.inputs) {
            rapidjson::Value pj;
            WritePin(p, pj, alloc);
            inputs.PushBack(pj, alloc);
        }
        nj.AddMember("inputs", inputs, alloc);
        rapidjson::Value outputs(rapidjson::kArrayType);
        for (const auto& p : n.outputs) {
            rapidjson::Value pj;
            WritePin(p, pj, alloc);
            outputs.PushBack(pj, alloc);
        }
        nj.AddMember("outputs", outputs, alloc);
        nodes.PushBack(nj, alloc);
    }
    out.AddMember("nodes", nodes, alloc);

    rapidjson::Value links(rapidjson::kArrayType);
    for (const auto& l : g.links) {
        rapidjson::Value lj(rapidjson::kObjectType);
        lj.AddMember("id", l.id, alloc);
        lj.AddMember("from", l.from_pin, alloc);
        lj.AddMember("to", l.to_pin, alloc);
        links.PushBack(lj, alloc);
    }
    out.AddMember("links", links, alloc);
}

bool ReadGraphBody(const rapidjson::Value& in, ShaderGraphAsset& out,
                   ShaderGraphDiagnostics& diag) {
    if (!in.IsObject()) {
        diag.errors.push_back("shader graph body is not an object");
        return false;
    }
    out.next_id = ReadInt(in, "next_id", 100);
    if (out.next_id < 100) out.next_id = 100;

    if (in.HasMember("nodes") && in["nodes"].IsArray()) {
        for (const auto& nj : in["nodes"].GetArray()) {
            if (!nj.IsObject()) {
                diag.warnings.push_back("skipped non-object node entry");
                continue;
            }
            NodeDesc n;
            n.id = ReadInt(nj, "id", 0);
            n.name = ReadString(nj, "name");
            n.category = ReadString(nj, "category");
            if (nj.HasMember("pos") && nj["pos"].IsArray() && nj["pos"].Size() >= 2) {
                if (nj["pos"][0].IsNumber()) n.pos[0] = nj["pos"][0].GetFloat();
                if (nj["pos"][1].IsNumber()) n.pos[1] = nj["pos"][1].GetFloat();
            }
            if (nj.HasMember("color") && nj["color"].IsUint()) {
                n.header_color = nj["color"].GetUint();
            }
            if (nj.HasMember("inputs") && nj["inputs"].IsArray()) {
                for (const auto& pj : nj["inputs"].GetArray())
                    if (pj.IsObject()) n.inputs.push_back(ReadPin(pj));
            }
            if (nj.HasMember("outputs") && nj["outputs"].IsArray()) {
                for (const auto& pj : nj["outputs"].GetArray())
                    if (pj.IsObject()) n.outputs.push_back(ReadPin(pj));
            }
            out.nodes.push_back(std::move(n));
        }
    }

    if (in.HasMember("links") && in["links"].IsArray()) {
        for (const auto& lj : in["links"].GetArray()) {
            if (!lj.IsObject()) continue;
            LinkDesc l;
            l.id = ReadInt(lj, "id", 0);
            l.from_pin = ReadInt(lj, "from", 0);
            l.to_pin = ReadInt(lj, "to", 0);
            out.links.push_back(l);
        }
    }
    return true;
}

}  // namespace

const char* PinTypeName(PinType type) {
    switch (type) {
        case PinType::Float: return "Float";
        case PinType::Vec2: return "Vec2";
        case PinType::Vec3: return "Vec3";
        case PinType::Vec4: return "Vec4";
        case PinType::Color: return "Color";
        case PinType::Texture2D: return "Texture2D";
        case PinType::Sampler: return "Sampler";
    }
    return "Float";
}

PinType PinTypeFromName(const char* name) {
    if (name) {
        if (std::strcmp(name, "Vec2") == 0) return PinType::Vec2;
        if (std::strcmp(name, "Vec3") == 0) return PinType::Vec3;
        if (std::strcmp(name, "Vec4") == 0) return PinType::Vec4;
        if (std::strcmp(name, "Color") == 0) return PinType::Color;
        if (std::strcmp(name, "Texture2D") == 0) return PinType::Texture2D;
        if (std::strcmp(name, "Sampler") == 0) return PinType::Sampler;
    }
    return PinType::Float;
}

bool ValidateShaderGraph(const ShaderGraphAsset& graph, std::string& error) {
    std::unordered_set<int> pin_ids;
    std::unordered_set<int> input_pins;
    std::unordered_set<int> output_pins;
    for (const auto& n : graph.nodes) {
        for (const auto& p : n.inputs) {
            if (!pin_ids.insert(p.id).second) {
                error = "duplicate pin id " + std::to_string(p.id);
                return false;
            }
            input_pins.insert(p.id);
        }
        for (const auto& p : n.outputs) {
            if (!pin_ids.insert(p.id).second) {
                error = "duplicate pin id " + std::to_string(p.id);
                return false;
            }
            output_pins.insert(p.id);
        }
    }
    for (const auto& l : graph.links) {
        if (output_pins.find(l.from_pin) == output_pins.end()) {
            error = "link " + std::to_string(l.id) +
                    " from_pin does not resolve to an output pin";
            return false;
        }
        if (input_pins.find(l.to_pin) == input_pins.end()) {
            error = "link " + std::to_string(l.id) +
                    " to_pin does not resolve to an input pin";
            return false;
        }
    }
    return true;
}

std::string SerializeShaderGraph(const ShaderGraphAsset& graph) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();
    doc.AddMember("version", kShaderGraphSchemaVersion, alloc);
    rapidjson::Value body(rapidjson::kObjectType);
    WriteGraphBody(graph, body, alloc);
    doc.AddMember("graph", body, alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return std::string(buffer.GetString(), buffer.GetSize());
}

bool DeserializeShaderGraph(const std::string& json, ShaderGraphAsset& out,
                            ShaderGraphDiagnostics& diag) {
    rapidjson::Document doc;
    doc.Parse(json.c_str());
    if (doc.HasParseError()) {
        diag.errors.push_back("JSON parse error");
        return false;
    }
    if (!doc.IsObject()) {
        diag.errors.push_back("root is not an object");
        return false;
    }

    diag.source_version = (doc.HasMember("version") && doc["version"].IsInt())
                              ? doc["version"].GetInt()
                              : 0;
    if (diag.source_version > kShaderGraphSchemaVersion) {
        diag.warnings.push_back("asset version newer than supported; reading best-effort");
    }

    const rapidjson::Value* body = nullptr;
    if (doc.HasMember("graph") && doc["graph"].IsObject()) {
        body = &doc["graph"];
    } else {
        // 旧版裸图（无 version/graph 包裹）：按 version 0 迁移读取。
        body = &doc;
        if (diag.source_version == 0) diag.migrated = true;
    }

    if (!ReadGraphBody(*body, out, diag)) return false;
    diag.ok = true;
    return true;
}

bool SaveShaderGraphToFile(const ShaderGraphAsset& graph, const std::string& path,
                           ShaderGraphDiagnostics& diag) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        diag.errors.push_back("cannot open file for write: " + path);
        return false;
    }
    std::string json = SerializeShaderGraph(graph);
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!out.good()) {
        diag.errors.push_back("write failed: " + path);
        return false;
    }
    diag.ok = true;
    return true;
}

bool LoadShaderGraphFromFile(const std::string& path, ShaderGraphAsset& out,
                             ShaderGraphDiagnostics& diag) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        diag.errors.push_back("cannot open file for read: " + path);
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return DeserializeShaderGraph(ss.str(), out, diag);
}

}  // namespace shadergraph
}  // namespace dse
