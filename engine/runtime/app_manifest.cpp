#include "engine/runtime/app_manifest.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

namespace dse::runtime {

namespace {

std::string ArgbToHex(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08X", v);
    return std::string(buf);
}

// 读取颜色：支持 JSON 数字（0xAARRGGBB）或形如 "0xFF1E1E28" / "#1E1E28" 的字符串。
bool ReadArgb(const rapidjson::Value& obj, const char* key, uint32_t& out) {
    if (!obj.HasMember(key)) return false;
    const rapidjson::Value& v = obj[key];
    if (v.IsUint()) { out = v.GetUint(); return true; }
    if (v.IsUint64()) { out = static_cast<uint32_t>(v.GetUint64()); return true; }
    if (v.IsInt()) { out = static_cast<uint32_t>(v.GetInt()); return true; }
    if (v.IsString()) {
        std::string s = v.GetString();
        if (!s.empty() && s[0] == '#') s = "0xFF" + s.substr(1);
        try {
            out = static_cast<uint32_t>(std::stoul(s, nullptr, 0));
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

bool ReadInt(const rapidjson::Value& obj, const char* key, int& out) {
    if (obj.HasMember(key) && obj[key].IsInt()) { out = obj[key].GetInt(); return true; }
    return false;
}

bool ReadString(const rapidjson::Value& obj, const char* key, std::string& out) {
    if (obj.HasMember(key) && obj[key].IsString()) { out = obj[key].GetString(); return true; }
    return false;
}

} // namespace

bool LoadAppManifest(const std::string& path, AppManifest& out) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError() || !doc.IsObject()) {
        return false;
    }

    const std::filesystem::path base_dir = std::filesystem::path(path).parent_path();

    if (doc.HasMember("window") && doc["window"].IsObject()) {
        const auto& w = doc["window"];
        if (ReadString(w, "title", out.window_title)) out.has_window_title = true;
        int width = 0, height = 0;
        const bool got_w = ReadInt(w, "width", width);
        const bool got_h = ReadInt(w, "height", height);
        if (got_w || got_h) {
            out.has_window_size = true;
            if (got_w) out.window_width = width;
            if (got_h) out.window_height = height;
        }
    }

    if (doc.HasMember("splash") && doc["splash"].IsObject()) {
        const auto& s = doc["splash"];
        out.has_splash = true;
        dse::platform::SplashConfig& cfg = out.splash;

        if (s.HasMember("enabled") && s["enabled"].IsBool()) cfg.enabled = s["enabled"].GetBool();

        std::string image;
        if (ReadString(s, "image", image) && !image.empty()) {
            std::filesystem::path img_path(image);
            if (img_path.is_relative() && !base_dir.empty()) {
                img_path = base_dir / img_path;
            }
            cfg.image_path = img_path.string();
        }

        ReadString(s, "app_name", cfg.app_name);
        ReadString(s, "initial_status", cfg.initial_status);

        ReadInt(s, "card_width", cfg.card_width);
        ReadInt(s, "card_height", cfg.card_height);
        ReadInt(s, "logo_size", cfg.logo_size);
        ReadInt(s, "fade_in_ms", cfg.fade_in_ms);
        ReadInt(s, "fade_out_ms", cfg.fade_out_ms);
        ReadInt(s, "min_display_ms", cfg.min_display_ms);

        ReadArgb(s, "background_argb", cfg.bg_argb);
        ReadArgb(s, "title_argb", cfg.title_argb);
        ReadArgb(s, "status_argb", cfg.status_argb);
        ReadArgb(s, "accent_argb", cfg.accent_argb);
    }

    return true;
}

bool WriteAppManifest(const std::string& path, const AppManifest& manifest) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    auto str = [&alloc](const std::string& s) {
        return rapidjson::Value(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
    };

    if (manifest.has_window_title || manifest.has_window_size) {
        rapidjson::Value w(rapidjson::kObjectType);
        if (manifest.has_window_title) w.AddMember("title", str(manifest.window_title), alloc);
        if (manifest.has_window_size) {
            w.AddMember("width", manifest.window_width, alloc);
            w.AddMember("height", manifest.window_height, alloc);
        }
        doc.AddMember("window", w, alloc);
    }

    if (manifest.has_splash) {
        const auto& cfg = manifest.splash;
        rapidjson::Value s(rapidjson::kObjectType);
        s.AddMember("enabled", cfg.enabled, alloc);
        s.AddMember("image", str(cfg.image_path), alloc);
        s.AddMember("app_name", str(cfg.app_name), alloc);
        s.AddMember("initial_status", str(cfg.initial_status), alloc);
        s.AddMember("card_width", cfg.card_width, alloc);
        s.AddMember("card_height", cfg.card_height, alloc);
        s.AddMember("logo_size", cfg.logo_size, alloc);
        s.AddMember("background_argb", str(ArgbToHex(cfg.bg_argb)), alloc);
        s.AddMember("title_argb", str(ArgbToHex(cfg.title_argb)), alloc);
        s.AddMember("status_argb", str(ArgbToHex(cfg.status_argb)), alloc);
        s.AddMember("accent_argb", str(ArgbToHex(cfg.accent_argb)), alloc);
        s.AddMember("fade_in_ms", cfg.fade_in_ms, alloc);
        s.AddMember("min_display_ms", cfg.min_display_ms, alloc);
        s.AddMember("fade_out_ms", cfg.fade_out_ms, alloc);
        doc.AddMember("splash", s, alloc);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs(path, std::ios::trunc | std::ios::binary);
    if (!ofs.is_open()) return false;
    ofs << buffer.GetString();
    return ofs.good();
}

} // namespace dse::runtime
