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

#include "engine/core/asset_version_envelope.h"
#include "engine/core/asset_dto.h"

namespace dse {
namespace shadergraph {

namespace {

using Alloc = rapidjson::Document::AllocatorType;

struct ShaderGraphDto {
    int next_id = 100;
};

struct ShaderGraphPinDto {
    int id = 0;
    std::string name;
    std::string type = "Float";
    int kind = 0;
    float default_value[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct ShaderGraphNodeDto {
    int id = 0;
    std::string name;
    std::string category;
    glm::vec2 pos{0.0f, 0.0f};
    unsigned int header_color = 0;
};

struct ShaderGraphLinkDto {
    int id = 0;
    int from_pin = 0;
    int to_pin = 0;
};

constexpr dse::assets::FieldDesc kShaderGraphFields[] = {
    {"next_id", dse::assets::FieldType::Int, offsetof(ShaderGraphDto, next_id)},
};

constexpr dse::assets::FieldDesc kShaderGraphPinFields[] = {
    {"id", dse::assets::FieldType::Int, offsetof(ShaderGraphPinDto, id)},
    {"name", dse::assets::FieldType::String, offsetof(ShaderGraphPinDto, name)},
    {"type", dse::assets::FieldType::String, offsetof(ShaderGraphPinDto, type)},
    {"kind", dse::assets::FieldType::Int, offsetof(ShaderGraphPinDto, kind)},
    {"default", dse::assets::FieldType::FloatArray4, offsetof(ShaderGraphPinDto, default_value)},
};

constexpr dse::assets::FieldDesc kShaderGraphNodeFields[] = {
    {"id", dse::assets::FieldType::Int, offsetof(ShaderGraphNodeDto, id)},
    {"name", dse::assets::FieldType::String, offsetof(ShaderGraphNodeDto, name)},
    {"category", dse::assets::FieldType::String, offsetof(ShaderGraphNodeDto, category)},
    {"pos", dse::assets::FieldType::Vec2, offsetof(ShaderGraphNodeDto, pos)},
    {"color", dse::assets::FieldType::UInt, offsetof(ShaderGraphNodeDto, header_color)},
};

constexpr dse::assets::FieldDesc kShaderGraphLinkFields[] = {
    {"id", dse::assets::FieldType::Int, offsetof(ShaderGraphLinkDto, id)},
    {"from", dse::assets::FieldType::Int, offsetof(ShaderGraphLinkDto, from_pin)},
    {"to", dse::assets::FieldType::Int, offsetof(ShaderGraphLinkDto, to_pin)},
};


void WriteGraphBody(const ShaderGraphAsset& g, rapidjson::Value& out, Alloc& alloc) {
    out.SetObject();
    ShaderGraphDto gdto;
    gdto.next_id = g.next_id;
    dse::assets::WriteFields(out, alloc, kShaderGraphFields,
                             sizeof(kShaderGraphFields) / sizeof(kShaderGraphFields[0]), &gdto);

    rapidjson::Value nodes(rapidjson::kArrayType);
    for (const auto& n : g.nodes) {
        ShaderGraphNodeDto ndto;
        ndto.id = n.id;
        ndto.name = n.name;
        ndto.category = n.category;
        ndto.pos = glm::vec2(n.pos[0], n.pos[1]);
        ndto.header_color = n.header_color;
        rapidjson::Value nj(rapidjson::kObjectType);
        dse::assets::WriteFields(nj, alloc, kShaderGraphNodeFields,
                                 sizeof(kShaderGraphNodeFields) / sizeof(kShaderGraphNodeFields[0]), &ndto);

        rapidjson::Value inputs(rapidjson::kArrayType);
        for (const auto& p : n.inputs) {
            ShaderGraphPinDto pdto;
            pdto.id = p.id;
            pdto.name = p.name;
            pdto.type = PinTypeName(p.type);
            pdto.kind = p.kind == PinKind::Input ? 0 : 1;
            for (int i = 0; i < 4; ++i) pdto.default_value[i] = p.default_value[i];
            rapidjson::Value pj(rapidjson::kObjectType);
            dse::assets::WriteFields(pj, alloc, kShaderGraphPinFields,
                                     sizeof(kShaderGraphPinFields) / sizeof(kShaderGraphPinFields[0]), &pdto);
            inputs.PushBack(pj, alloc);
        }
        nj.AddMember("inputs", inputs, alloc);

        rapidjson::Value outputs(rapidjson::kArrayType);
        for (const auto& p : n.outputs) {
            ShaderGraphPinDto pdto;
            pdto.id = p.id;
            pdto.name = p.name;
            pdto.type = PinTypeName(p.type);
            pdto.kind = p.kind == PinKind::Input ? 0 : 1;
            for (int i = 0; i < 4; ++i) pdto.default_value[i] = p.default_value[i];
            rapidjson::Value pj(rapidjson::kObjectType);
            dse::assets::WriteFields(pj, alloc, kShaderGraphPinFields,
                                     sizeof(kShaderGraphPinFields) / sizeof(kShaderGraphPinFields[0]), &pdto);
            outputs.PushBack(pj, alloc);
        }
        nj.AddMember("outputs", outputs, alloc);
        nodes.PushBack(nj, alloc);
    }
    out.AddMember("nodes", nodes, alloc);

    rapidjson::Value links(rapidjson::kArrayType);
    for (const auto& l : g.links) {
        ShaderGraphLinkDto ldto;
        ldto.id = l.id;
        ldto.from_pin = l.from_pin;
        ldto.to_pin = l.to_pin;
        rapidjson::Value lj(rapidjson::kObjectType);
        dse::assets::WriteFields(lj, alloc, kShaderGraphLinkFields,
                                 sizeof(kShaderGraphLinkFields) / sizeof(kShaderGraphLinkFields[0]), &ldto);
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

    // ADR-3: .dshadergraph read path uses unified DTO/field table.
    ShaderGraphDto gdto;
    dse::assets::ReadFields(in, kShaderGraphFields,
                            sizeof(kShaderGraphFields) / sizeof(kShaderGraphFields[0]), &gdto);
    out.next_id = gdto.next_id;
    if (out.next_id < 100) out.next_id = 100;

    if (in.HasMember("nodes") && in["nodes"].IsArray()) {
        for (const auto& nj : in["nodes"].GetArray()) {
            if (!nj.IsObject()) {
                diag.warnings.push_back("skipped non-object node entry");
                continue;
            }
            ShaderGraphNodeDto ndto;
            dse::assets::ReadFields(nj, kShaderGraphNodeFields,
                                    sizeof(kShaderGraphNodeFields) / sizeof(kShaderGraphNodeFields[0]), &ndto);
            NodeDesc n;
            n.id = ndto.id;
            n.name = std::move(ndto.name);
            n.category = std::move(ndto.category);
            n.pos[0] = ndto.pos.x;
            n.pos[1] = ndto.pos.y;
            n.header_color = ndto.header_color;

            if (nj.HasMember("inputs") && nj["inputs"].IsArray()) {
                for (const auto& pj : nj["inputs"].GetArray()) {
                    if (!pj.IsObject()) continue;
                    ShaderGraphPinDto pdto;
                    dse::assets::ReadFields(pj, kShaderGraphPinFields,
                                            sizeof(kShaderGraphPinFields) / sizeof(kShaderGraphPinFields[0]), &pdto);
                    PinDesc p;
                    p.id = pdto.id;
                    p.name = std::move(pdto.name);
                    p.type = PinTypeFromName(pdto.type.c_str());
                    p.kind = pdto.kind == 0 ? PinKind::Input : PinKind::Output;
                    for (int i = 0; i < 4; ++i) p.default_value[i] = pdto.default_value[i];
                    n.inputs.push_back(std::move(p));
                }
            }
            if (nj.HasMember("outputs") && nj["outputs"].IsArray()) {
                for (const auto& pj : nj["outputs"].GetArray()) {
                    if (!pj.IsObject()) continue;
                    ShaderGraphPinDto pdto;
                    dse::assets::ReadFields(pj, kShaderGraphPinFields,
                                            sizeof(kShaderGraphPinFields) / sizeof(kShaderGraphPinFields[0]), &pdto);
                    PinDesc p;
                    p.id = pdto.id;
                    p.name = std::move(pdto.name);
                    p.type = PinTypeFromName(pdto.type.c_str());
                    p.kind = pdto.kind == 0 ? PinKind::Input : PinKind::Output;
                    for (int i = 0; i < 4; ++i) p.default_value[i] = pdto.default_value[i];
                    n.outputs.push_back(std::move(p));
                }
            }
            out.nodes.push_back(std::move(n));
        }
    }

    if (in.HasMember("links") && in["links"].IsArray()) {
        for (const auto& lj : in["links"].GetArray()) {
            if (!lj.IsObject()) continue;
            ShaderGraphLinkDto ldto;
            dse::assets::ReadFields(lj, kShaderGraphLinkFields,
                                    sizeof(kShaderGraphLinkFields) / sizeof(kShaderGraphLinkFields[0]), &ldto);
            LinkDesc l;
            l.id = ldto.id;
            l.from_pin = ldto.from_pin;
            l.to_pin = ldto.to_pin;
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
    dse::assets::WriteVersionEnvelope(doc, kShaderGraphSchemaVersion, alloc);
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

    dse::assets::ReadVersionEnvelope(doc, kShaderGraphSchemaVersion, ".dshadergraph", diag);

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
