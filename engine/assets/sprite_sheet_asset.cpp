/**
 * @file sprite_sheet_asset.cpp
 * @brief SpriteSheet 资产加载/保存实现
 */

#include "engine/assets/sprite_sheet_asset.h"
#include <fstream>
#include <sstream>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

bool SpriteSheetAsset::LoadFromFile(const std::string& path) {
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
        texture_width = doc["width"].GetInt();
    if (doc.HasMember("height") && doc["height"].IsInt())
        texture_height = doc["height"].GetInt();

    frames.clear();
    if (doc.HasMember("frames") && doc["frames"].IsArray()) {
        for (auto& jf : doc["frames"].GetArray()) {
            if (!jf.IsObject()) continue;
            SpriteFrame frame;
            if (jf.HasMember("name") && jf["name"].IsString())
                frame.name = jf["name"].GetString();
            if (jf.HasMember("index") && jf["index"].IsInt())
                frame.index = jf["index"].GetInt();
            else
                frame.index = (int)frames.size();

            if (jf.HasMember("pixel_rect") && jf["pixel_rect"].IsObject()) {
                auto& pr = jf["pixel_rect"];
                int px = pr.HasMember("x") && pr["x"].IsInt() ? pr["x"].GetInt() : 0;
                int py = pr.HasMember("y") && pr["y"].IsInt() ? pr["y"].GetInt() : 0;
                int pw = pr.HasMember("w") && pr["w"].IsInt() ? pr["w"].GetInt() : 0;
                int ph = pr.HasMember("h") && pr["h"].IsInt() ? pr["h"].GetInt() : 0;
                frame.pixel_rect = glm::ivec4(px, py, pw, ph);
            }

            if (jf.HasMember("uv_rect") && jf["uv_rect"].IsObject()) {
                auto& ur = jf["uv_rect"];
                float ux = ur.HasMember("x") && ur["x"].IsNumber() ? ur["x"].GetFloat() : 0.0f;
                float uy = ur.HasMember("y") && ur["y"].IsNumber() ? ur["y"].GetFloat() : 0.0f;
                float uw = ur.HasMember("w") && ur["w"].IsNumber() ? ur["w"].GetFloat() : 1.0f;
                float uh = ur.HasMember("h") && ur["h"].IsNumber() ? ur["h"].GetFloat() : 1.0f;
                frame.uv_rect = glm::vec4(ux, uy, uw, uh);
            } else if (texture_width > 0 && texture_height > 0) {
                float tw = (float)texture_width;
                float th = (float)texture_height;
                frame.uv_rect = glm::vec4(
                    frame.pixel_rect.x / tw, frame.pixel_rect.y / th,
                    frame.pixel_rect.z / tw, frame.pixel_rect.w / th);
            }

            if (jf.HasMember("pivot") && jf["pivot"].IsObject()) {
                auto& pv = jf["pivot"];
                frame.pivot.x = pv.HasMember("x") && pv["x"].IsNumber() ? pv["x"].GetFloat() : 0.5f;
                frame.pivot.y = pv.HasMember("y") && pv["y"].IsNumber() ? pv["y"].GetFloat() : 0.5f;
            }

            frames.push_back(frame);
        }
    }
    return true;
}

bool SpriteSheetAsset::SaveToFile(const std::string& path) const {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("texture", rapidjson::Value(texture_path.c_str(), alloc), alloc);
    doc.AddMember("width", texture_width, alloc);
    doc.AddMember("height", texture_height, alloc);

    rapidjson::Value jarr(rapidjson::kArrayType);
    for (auto& f : frames) {
        rapidjson::Value jf(rapidjson::kObjectType);
        jf.AddMember("name", rapidjson::Value(f.name.c_str(), alloc), alloc);
        jf.AddMember("index", f.index, alloc);

        rapidjson::Value pr(rapidjson::kObjectType);
        pr.AddMember("x", f.pixel_rect.x, alloc);
        pr.AddMember("y", f.pixel_rect.y, alloc);
        pr.AddMember("w", f.pixel_rect.z, alloc);
        pr.AddMember("h", f.pixel_rect.w, alloc);
        jf.AddMember("pixel_rect", pr, alloc);

        rapidjson::Value ur(rapidjson::kObjectType);
        ur.AddMember("x", f.uv_rect.x, alloc);
        ur.AddMember("y", f.uv_rect.y, alloc);
        ur.AddMember("w", f.uv_rect.z, alloc);
        ur.AddMember("h", f.uv_rect.w, alloc);
        jf.AddMember("uv_rect", ur, alloc);

        rapidjson::Value pv(rapidjson::kObjectType);
        pv.AddMember("x", f.pivot.x, alloc);
        pv.AddMember("y", f.pivot.y, alloc);
        jf.AddMember("pivot", pv, alloc);

        jarr.PushBack(jf, alloc);
    }
    doc.AddMember("frames", jarr, alloc);

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);

    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << sb.GetString();
    return true;
}

const SpriteFrame* SpriteSheetAsset::FindFrame(const std::string& name) const {
    for (auto& f : frames) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

glm::vec4 SpriteSheetAsset::GetFrameUV(int index) const {
    if (index >= 0 && index < (int)frames.size()) {
        return frames[index].uv_rect;
    }
    return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
}
