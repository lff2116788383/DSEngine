/**
 * @file script_metadata.cpp
 * @brief C# 脚本元数据 .dscriptmeta 版本化序列化实现
 */

#include "engine/scripting/script_metadata.h"

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
namespace scripting {

namespace {

using Alloc = rapidjson::Document::AllocatorType;

struct ScriptMetadataDto {
    std::string class_name;
    std::string full_name;
    std::string base_type;
    std::string assembly;
    std::string source_path;
};

struct ScriptFieldMetaDto {
    std::string name;
    std::string type;
    std::string tooltip;
    float number_default[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    std::string string_default;
};

constexpr dse::assets::FieldDesc kScriptMetadataFields[] = {
    {"class_name", dse::assets::FieldType::String, offsetof(ScriptMetadataDto, class_name)},
    {"full_name", dse::assets::FieldType::String, offsetof(ScriptMetadataDto, full_name)},
    {"base_type", dse::assets::FieldType::String, offsetof(ScriptMetadataDto, base_type)},
    {"assembly", dse::assets::FieldType::String, offsetof(ScriptMetadataDto, assembly)},
    {"source_path", dse::assets::FieldType::String, offsetof(ScriptMetadataDto, source_path)},
};

constexpr dse::assets::FieldDesc kScriptFieldMetaFields[] = {
    {"name", dse::assets::FieldType::String, offsetof(ScriptFieldMetaDto, name)},
    {"type", dse::assets::FieldType::String, offsetof(ScriptFieldMetaDto, type)},
    {"tooltip", dse::assets::FieldType::String, offsetof(ScriptFieldMetaDto, tooltip)},
    {"number_default", dse::assets::FieldType::FloatArray4, offsetof(ScriptFieldMetaDto, number_default)},
    {"string_default", dse::assets::FieldType::String, offsetof(ScriptFieldMetaDto, string_default)},
};

void WriteScriptBody(const ScriptMetadata& m, rapidjson::Value& out, Alloc& alloc) {
    out.SetObject();
    ScriptMetadataDto dto;
    dto.class_name = m.class_name;
    dto.full_name = m.full_name;
    dto.base_type = m.base_type;
    dto.assembly = m.assembly;
    dto.source_path = m.source_path;
    dse::assets::WriteFields(out, alloc, kScriptMetadataFields,
                             sizeof(kScriptMetadataFields) / sizeof(kScriptMetadataFields[0]), &dto);

    rapidjson::Value fields(rapidjson::kArrayType);
    for (const auto& f : m.fields) {
        ScriptFieldMetaDto fdto;
        fdto.name = f.name;
        fdto.type = ScriptFieldTypeName(f.type);
        fdto.tooltip = f.tooltip;
        for (int i = 0; i < 4; ++i) fdto.number_default[i] = f.number_default[i];
        fdto.string_default = f.string_default;

        rapidjson::Value fj(rapidjson::kObjectType);
        dse::assets::WriteFields(fj, alloc, kScriptFieldMetaFields,
                                 sizeof(kScriptFieldMetaFields) / sizeof(kScriptFieldMetaFields[0]), &fdto);
        fields.PushBack(fj, alloc);
    }
    out.AddMember("fields", fields, alloc);
}

bool ReadScriptBody(const rapidjson::Value& in, ScriptMetadata& out,
                    ScriptMetaDiagnostics& diag) {
    if (!in.IsObject()) {
        diag.errors.push_back("script metadata body is not an object");
        return false;
    }
    // ADR-3：.dscriptmeta body 读取路径收敛到统一 DTO/字段表。
    ScriptMetadataDto dto;
    dse::assets::ReadFields(in, kScriptMetadataFields,
                            sizeof(kScriptMetadataFields) / sizeof(kScriptMetadataFields[0]), &dto);
    out.class_name = std::move(dto.class_name);
    out.full_name = std::move(dto.full_name);
    out.base_type = std::move(dto.base_type);
    out.assembly = std::move(dto.assembly);
    out.source_path = std::move(dto.source_path);

    if (in.HasMember("fields") && in["fields"].IsArray()) {
        for (const auto& fj : in["fields"].GetArray()) {
            if (!fj.IsObject()) {
                diag.warnings.push_back("skipped non-object field entry");
                continue;
            }
            ScriptFieldMetaDto fdto;
            dse::assets::ReadFields(fj, kScriptFieldMetaFields,
                                    sizeof(kScriptFieldMetaFields) / sizeof(kScriptFieldMetaFields[0]),
                                    &fdto);
            ScriptFieldMeta f;
            f.name = std::move(fdto.name);
            f.type = ScriptFieldTypeFromName(fdto.type.c_str());
            f.tooltip = std::move(fdto.tooltip);
            for (int i = 0; i < 4; ++i) f.number_default[i] = fdto.number_default[i];
            f.string_default = std::move(fdto.string_default);
            out.fields.push_back(std::move(f));
        }
    }
    return true;
}

}  // namespace

const char* ScriptFieldTypeName(ScriptFieldType type) {
    switch (type) {
        case ScriptFieldType::Bool: return "Bool";
        case ScriptFieldType::Int: return "Int";
        case ScriptFieldType::Float: return "Float";
        case ScriptFieldType::String: return "String";
        case ScriptFieldType::Vec2: return "Vec2";
        case ScriptFieldType::Vec3: return "Vec3";
        case ScriptFieldType::Vec4: return "Vec4";
        case ScriptFieldType::Color: return "Color";
        case ScriptFieldType::EntityRef: return "EntityRef";
        case ScriptFieldType::AssetRef: return "AssetRef";
    }
    return "Float";
}

ScriptFieldType ScriptFieldTypeFromName(const char* name) {
    if (name) {
        if (std::strcmp(name, "Bool") == 0) return ScriptFieldType::Bool;
        if (std::strcmp(name, "Int") == 0) return ScriptFieldType::Int;
        if (std::strcmp(name, "String") == 0) return ScriptFieldType::String;
        if (std::strcmp(name, "Vec2") == 0) return ScriptFieldType::Vec2;
        if (std::strcmp(name, "Vec3") == 0) return ScriptFieldType::Vec3;
        if (std::strcmp(name, "Vec4") == 0) return ScriptFieldType::Vec4;
        if (std::strcmp(name, "Color") == 0) return ScriptFieldType::Color;
        if (std::strcmp(name, "EntityRef") == 0) return ScriptFieldType::EntityRef;
        if (std::strcmp(name, "AssetRef") == 0) return ScriptFieldType::AssetRef;
    }
    return ScriptFieldType::Float;
}

bool ValidateScriptMetadata(const ScriptMetadata& meta, std::string& error) {
    if (meta.class_name.empty()) {
        error = "script metadata has empty class_name";
        return false;
    }
    std::unordered_set<std::string> names;
    for (const auto& f : meta.fields) {
        if (f.name.empty()) {
            error = "script metadata has a field with empty name";
            return false;
        }
        if (!names.insert(f.name).second) {
            error = "duplicate field name '" + f.name + "'";
            return false;
        }
    }
    return true;
}

std::string SerializeScriptMetadata(const ScriptMetadata& meta) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();
    dse::assets::WriteVersionEnvelope(doc, kScriptMetaSchemaVersion, alloc);
    rapidjson::Value body(rapidjson::kObjectType);
    WriteScriptBody(meta, body, alloc);
    doc.AddMember("script", body, alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return std::string(buffer.GetString(), buffer.GetSize());
}

bool DeserializeScriptMetadata(const std::string& json, ScriptMetadata& out,
                               ScriptMetaDiagnostics& diag) {
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

    dse::assets::ReadVersionEnvelope(doc, kScriptMetaSchemaVersion, ".dscriptmeta", diag);

    const rapidjson::Value* body = nullptr;
    if (doc.HasMember("script") && doc["script"].IsObject()) {
        body = &doc["script"];
    } else {
        // 旧版裸对象（无 version/script 包裹）：按 version 0 迁移读取。
        body = &doc;
        if (diag.source_version == 0) diag.migrated = true;
    }

    if (!ReadScriptBody(*body, out, diag)) return false;
    diag.ok = true;
    return true;
}

bool SaveScriptMetadataToFile(const ScriptMetadata& meta, const std::string& path,
                              ScriptMetaDiagnostics& diag) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        diag.errors.push_back("cannot open file for write: " + path);
        return false;
    }
    std::string json = SerializeScriptMetadata(meta);
    out.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!out.good()) {
        diag.errors.push_back("write failed: " + path);
        return false;
    }
    diag.ok = true;
    return true;
}

bool LoadScriptMetadataFromFile(const std::string& path, ScriptMetadata& out,
                                ScriptMetaDiagnostics& diag) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        diag.errors.push_back("cannot open file for read: " + path);
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return DeserializeScriptMetadata(ss.str(), out, diag);
}

}  // namespace scripting
}  // namespace dse
