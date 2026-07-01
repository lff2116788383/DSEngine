/**
 * @file atlas_asset.cpp
 * @brief Atlas 资产加载/保存实现
 */

#include "engine/assets/atlas_asset.h"
#include <fstream>
#include <sstream>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

bool AtlasAsset::LoadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::stringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError() || !doc.IsObject()) return false;

    if (doc.HasMember("texture") && doc["texture"].IsString())
        texture_path = doc["texture"].GetString();
    if (doc.HasMember("width") && doc["width"].IsInt())
        atlas_width = doc["width"].GetInt();
    if (doc.HasMember("height") && doc["height"].IsInt())
        atlas_height = doc["height"].GetInt();

    entries.clear();
    if (doc.HasMember("entries") && doc["entries"].IsArray()) {
        for (auto& je : doc["entries"].GetArray()) {
            if (!je.IsObject()) continue;
            AtlasEntry entry;
            if (je.HasMember("name") && je["name"].IsString())
                entry.name = je["name"].GetString();
            if (je.HasMember("rotated") && je["rotated"].IsBool())
                entry.rotated = je["rotated"].GetBool();

            if (je.HasMember("pixel_rect") && je["pixel_rect"].IsObject()) {
                auto& pr = je["pixel_rect"];
                int px = pr.HasMember("x") && pr["x"].IsInt() ? pr["x"].GetInt() : 0;
                int py = pr.HasMember("y") && pr["y"].IsInt() ? pr["y"].GetInt() : 0;
                int pw = pr.HasMember("w") && pr["w"].IsInt() ? pr["w"].GetInt() : 0;
                int ph = pr.HasMember("h") && pr["h"].IsInt() ? pr["h"].GetInt() : 0;
                entry.pixel_rect = glm::ivec4(px, py, pw, ph);
            }

            if (je.HasMember("uv_rect") && je["uv_rect"].IsObject()) {
                auto& ur = je["uv_rect"];
                float ux = ur.HasMember("x") && ur["x"].IsNumber() ? ur["x"].GetFloat() : 0.0f;
                float uy = ur.HasMember("y") && ur["y"].IsNumber() ? ur["y"].GetFloat() : 0.0f;
                float uw = ur.HasMember("w") && ur["w"].IsNumber() ? ur["w"].GetFloat() : 1.0f;
                float uh = ur.HasMember("h") && ur["h"].IsNumber() ? ur["h"].GetFloat() : 1.0f;
                entry.uv_rect = glm::vec4(ux, uy, uw, uh);
            } else if (atlas_width > 0 && atlas_height > 0) {
                float aw = (float)atlas_width;
                float ah = (float)atlas_height;
                entry.uv_rect = glm::vec4(
                    entry.pixel_rect.x / aw, entry.pixel_rect.y / ah,
                    entry.pixel_rect.z / aw, entry.pixel_rect.w / ah);
            }

            if (je.HasMember("pivot") && je["pivot"].IsObject()) {
                auto& pv = je["pivot"];
                entry.pivot.x = pv.HasMember("x") && pv["x"].IsNumber() ? pv["x"].GetFloat() : 0.5f;
                entry.pivot.y = pv.HasMember("y") && pv["y"].IsNumber() ? pv["y"].GetFloat() : 0.5f;
            }

            if (je.HasMember("original_size") && je["original_size"].IsObject()) {
                auto& os = je["original_size"];
                entry.original_size.x = os.HasMember("w") && os["w"].IsInt() ? os["w"].GetInt() : 0;
                entry.original_size.y = os.HasMember("h") && os["h"].IsInt() ? os["h"].GetInt() : 0;
            }

            entries.push_back(entry);
        }
    }

    RebuildIndex();
    return true;
}

bool AtlasAsset::SaveToFile(const std::string& path) const {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("texture", rapidjson::Value(texture_path.c_str(), alloc), alloc);
    doc.AddMember("width", atlas_width, alloc);
    doc.AddMember("height", atlas_height, alloc);

    rapidjson::Value jarr(rapidjson::kArrayType);
    for (auto& e : entries) {
        rapidjson::Value je(rapidjson::kObjectType);
        je.AddMember("name", rapidjson::Value(e.name.c_str(), alloc), alloc);
        je.AddMember("rotated", e.rotated, alloc);

        rapidjson::Value pr(rapidjson::kObjectType);
        pr.AddMember("x", e.pixel_rect.x, alloc);
        pr.AddMember("y", e.pixel_rect.y, alloc);
        pr.AddMember("w", e.pixel_rect.z, alloc);
        pr.AddMember("h", e.pixel_rect.w, alloc);
        je.AddMember("pixel_rect", pr, alloc);

        rapidjson::Value ur(rapidjson::kObjectType);
        ur.AddMember("x", e.uv_rect.x, alloc);
        ur.AddMember("y", e.uv_rect.y, alloc);
        ur.AddMember("w", e.uv_rect.z, alloc);
        ur.AddMember("h", e.uv_rect.w, alloc);
        je.AddMember("uv_rect", ur, alloc);

        rapidjson::Value pv(rapidjson::kObjectType);
        pv.AddMember("x", e.pivot.x, alloc);
        pv.AddMember("y", e.pivot.y, alloc);
        je.AddMember("pivot", pv, alloc);

        rapidjson::Value os(rapidjson::kObjectType);
        os.AddMember("w", e.original_size.x, alloc);
        os.AddMember("h", e.original_size.y, alloc);
        je.AddMember("original_size", os, alloc);

        jarr.PushBack(je, alloc);
    }
    doc.AddMember("entries", jarr, alloc);

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);

    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << sb.GetString();
    return true;
}

const AtlasEntry* AtlasAsset::FindEntry(const std::string& name) const {
    auto it = name_index.find(name);
    if (it != name_index.end() && it->second < (int)entries.size()) {
        return &entries[it->second];
    }
    return nullptr;
}

glm::vec4 AtlasAsset::GetEntryUV(const std::string& name) const {
    auto* e = FindEntry(name);
    return e ? e->uv_rect : glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
}

void AtlasAsset::RebuildIndex() {
    name_index.clear();
    for (int i = 0; i < (int)entries.size(); ++i) {
        name_index[entries[i].name] = i;
    }
}
