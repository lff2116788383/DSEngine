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

namespace dse {
namespace scripting {

namespace {

using Alloc = rapidjson::Document::AllocatorType;

rapidjson::Value Str(const std::string& s, Alloc& alloc) {
    return rapidjson::Value(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
}

std::string ReadString(const rapidjson::Value& obj, const char* key) {
    if (obj.HasMember(key) && obj[key].IsString()) return obj[key].GetString();
    return std::string();
}

void WriteScriptBody(const ScriptMetadata& m, rapidjson::Value& out, Alloc& alloc) {
    out.SetObject();
    out.AddMember("class_name", Str(m.class_name, alloc), alloc);
    out.AddMember("full_name", Str(m.full_name, alloc), alloc);
    out.AddMember("base_type", Str(m.base_type, alloc), alloc);
    out.AddMember("assembly", Str(m.assembly, alloc), alloc);
    out.AddMember("source_path", Str(m.source_path, alloc), alloc);

    rapidjson::Value fields(rapidjson::kArrayType);
    for (const auto& f : m.fields) {
        rapidjson::Value fj(rapidjson::kObjectType);
        fj.AddMember("name", Str(f.name, alloc), alloc);
        fj.AddMember("type", Str(ScriptFieldTypeName(f.type), alloc), alloc);
        fj.AddMember("tooltip", Str(f.tooltip, alloc), alloc);
        rapidjson::Value num(rapidjson::kArrayType);
        for (float v : f.number_default) num.PushBack(v, alloc);
        fj.AddMember("number_default", num, alloc);
        fj.AddMember("string_default", Str(f.string_default, alloc), alloc);
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
    out.class_name = ReadString(in, "class_name");
    out.full_name = ReadString(in, "full_name");
    out.base_type = ReadString(in, "base_type");
    out.assembly = ReadString(in, "assembly");
    out.source_path = ReadString(in, "source_path");

    if (in.HasMember("fields") && in["fields"].IsArray()) {
        for (const auto& fj : in["fields"].GetArray()) {
            if (!fj.IsObject()) {
                diag.warnings.push_back("skipped non-object field entry");
                continue;
            }
            ScriptFieldMeta f;
            f.name = ReadString(fj, "name");
            f.type = ScriptFieldTypeFromName(ReadString(fj, "type").c_str());
            f.tooltip = ReadString(fj, "tooltip");
            if (fj.HasMember("number_default") && fj["number_default"].IsArray()) {
                const auto& arr = fj["number_default"];
                for (rapidjson::SizeType i = 0; i < arr.Size() && i < 4; ++i) {
                    if (arr[i].IsNumber()) f.number_default[i] = arr[i].GetFloat();
                }
            }
            f.string_default = ReadString(fj, "string_default");
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
    doc.AddMember("version", kScriptMetaSchemaVersion, alloc);
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

    diag.source_version = (doc.HasMember("version") && doc["version"].IsInt())
                              ? doc["version"].GetInt()
                              : 0;
    if (diag.source_version > kScriptMetaSchemaVersion) {
        diag.warnings.push_back("asset version newer than supported; reading best-effort");
    }

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
