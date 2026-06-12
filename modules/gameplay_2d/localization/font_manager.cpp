/**
 * @file font_manager.cpp
 * @brief 字体管理系统实现
 */

#include "font_manager.h"
#include "engine/base/debug.h"
#include <algorithm>
#include <fstream>
#include <vector>

namespace dse {
namespace gameplay2d {

namespace {

// 字体文件原始字节的具体句柄。FontAsset::FontDataHandle 是不透明基类，
// 渲染后端可 dynamic_cast 到本类型取字节自行光栅化字形。
struct FontFileData : public FontAsset::FontDataHandle {
    std::vector<unsigned char> bytes;
};

// 嗅探 sfnt/TrueType/OpenType/WOFF 等常见字体容器魔数。仅用于告警，不强制拒绝，
// 以便后端支持其它字体格式。
bool LooksLikeFontContainer(const std::vector<unsigned char>& b) {
    if (b.size() < 4) return false;
    const unsigned char* p = b.data();
    auto tag = [&](unsigned char a, unsigned char c, unsigned char d, unsigned char e) {
        return p[0] == a && p[1] == c && p[2] == d && p[3] == e;
    };
    if (tag(0x00, 0x01, 0x00, 0x00)) return true;            // TrueType outlines
    if (tag('O', 'T', 'T', 'O')) return true;                 // OpenType CFF
    if (tag('t', 'r', 'u', 'e')) return true;                 // legacy TrueType
    if (tag('t', 't', 'c', 'f')) return true;                 // TrueType collection
    if (tag('t', 'y', 'p', '1')) return true;                 // PostScript Type1 sfnt
    if (tag('w', 'O', 'F', 'F')) return true;                 // WOFF / WOFF2
    return false;
}

} // namespace

FontManager::~FontManager() {
    Clear();
}

bool FontManager::RegisterFont(const std::string& font_id, const std::string& font_path, int font_size) {
    if (fonts_.find(font_id) != fonts_.end()) {
        DEBUG_LOG_WARN("Font already registered: {}", font_id);
        return false;
    }
    
    FontAsset font_asset;
    font_asset.font_id = font_id;
    font_asset.font_path = font_path;
    font_asset.font_size = font_size;
    font_asset.is_loaded = false;
    font_asset.font_data.reset();
    
    fonts_[font_id] = font_asset;
    
    DEBUG_LOG_INFO("Font registered: {} (path: {}, size: {})", font_id, font_path, font_size);
    return true;
}

bool FontManager::LoadFont(const std::string& font_id) {
    auto it = fonts_.find(font_id);
    if (it == fonts_.end()) {
        DEBUG_LOG_ERROR("Font not found: {}", font_id);
        return false;
    }
    
    FontAsset& font = it->second;
    
    if (font.is_loaded) {
        DEBUG_LOG_WARN("Font already loaded: {}", font_id);
        return true;
    }

    if (font.font_path.empty()) {
        DEBUG_LOG_ERROR("Font has no path, cannot load: {}", font_id);
        return false;
    }

    // 读取字体文件原始字节到内存。FontManager 与渲染后端解耦：本层负责把字体文件
    // 字节加载进 font_data 句柄，后端再从字节光栅化字形。
    std::ifstream in(font.font_path, std::ios::binary | std::ios::ate);
    if (!in) {
        DEBUG_LOG_ERROR("Failed to open font file: {} (path: {})", font_id, font.font_path);
        return false;
    }

    const std::streamoff size = in.tellg();
    if (size <= 0) {
        DEBUG_LOG_ERROR("Font file is empty: {} (path: {})", font_id, font.font_path);
        return false;
    }

    auto handle = std::make_shared<FontFileData>();
    handle->bytes.resize(static_cast<size_t>(size));
    in.seekg(0, std::ios::beg);
    if (!in.read(reinterpret_cast<char*>(handle->bytes.data()), size)) {
        DEBUG_LOG_ERROR("Failed to read font file: {} (path: {})", font_id, font.font_path);
        return false;
    }

    if (!LooksLikeFontContainer(handle->bytes)) {
        DEBUG_LOG_WARN("Font file is not a recognized sfnt/ttf/otf/woff container: {} (path: {})",
                       font_id, font.font_path);
    }

    font.font_data = std::move(handle);
    font.is_loaded = true;

    DEBUG_LOG_INFO("Font loaded: {} ({} bytes from {})",
                   font_id, static_cast<const FontFileData*>(font.font_data.get())->bytes.size(),
                   font.font_path);
    return true;
}

void FontManager::UnloadFont(const std::string& font_id) {
    auto it = fonts_.find(font_id);
    if (it == fonts_.end()) {
        DEBUG_LOG_WARN("Font not found: {}", font_id);
        return;
    }
    
    FontAsset& font = it->second;
    
    if (!font.is_loaded) {
        return;
    }
    
    // 释放字体字节句柄。共享指针引用计数归零时内存即回收。
    font.is_loaded = false;
    font.font_data.reset();
    
    DEBUG_LOG_INFO("Font unloaded: {}", font_id);
}

FontAsset* FontManager::GetFont(const std::string& font_id) {
    auto it = fonts_.find(font_id);
    if (it == fonts_.end()) {
        // 如果字体不存在，尝试使用默认字体
        if (font_id != default_font_id_) {
            DEBUG_LOG_WARN("Font not found: {}, using default font", font_id);
            return GetFont(default_font_id_);
        }
        DEBUG_LOG_ERROR("Font not found: {}", font_id);
        return nullptr;
    }
    
    FontAsset& font = it->second;
    
    // 如果字体未加载，自动加载
    if (!font.is_loaded) {
        LoadFont(font_id);
    }
    
    return &font;
}

const FontAsset* FontManager::GetFont(const std::string& font_id) const {
    auto it = fonts_.find(font_id);
    if (it == fonts_.end()) {
        // 如果字体不存在，尝试使用默认字体
        if (font_id != default_font_id_) {
            DEBUG_LOG_WARN("Font not found: {}, using default font", font_id);
            return GetFont(default_font_id_);
        }
        DEBUG_LOG_ERROR("Font not found: {}", font_id);
        return nullptr;
    }
    
    return &it->second;
}

bool FontManager::SetDefaultFont(const std::string& font_id) {
    if (fonts_.find(font_id) == fonts_.end()) {
        DEBUG_LOG_ERROR("Font not found: {}", font_id);
        return false;
    }
    
    default_font_id_ = font_id;
    DEBUG_LOG_INFO("Default font set to: {}", font_id);
    return true;
}

bool FontManager::SetFontForLanguage(const std::string& language_code, const std::string& font_id) {
    if (fonts_.find(font_id) == fonts_.end()) {
        DEBUG_LOG_ERROR("Font not found: {}", font_id);
        return false;
    }
    
    language_font_map_[language_code] = font_id;
    DEBUG_LOG_INFO("Font set for language {}: {}", language_code, font_id);
    return true;
}

std::string FontManager::GetFontForLanguage(const std::string& language_code) const {
    auto it = language_font_map_.find(language_code);
    if (it != language_font_map_.end()) {
        return it->second;
    }
    
    // 如果未设置，返回默认字体
    return default_font_id_;
}

void FontManager::AddFontFallback(const std::string& primary_font_id, const std::string& fallback_font_id) {
    if (fonts_.find(primary_font_id) == fonts_.end()) {
        DEBUG_LOG_ERROR("Primary font not found: {}", primary_font_id);
        return;
    }
    
    if (fonts_.find(fallback_font_id) == fonts_.end()) {
        DEBUG_LOG_ERROR("Fallback font not found: {}", fallback_font_id);
        return;
    }
    
    auto& fallbacks = font_fallbacks_[primary_font_id];
    if (std::find(fallbacks.begin(), fallbacks.end(), fallback_font_id) == fallbacks.end()) {
        fallbacks.push_back(fallback_font_id);
        DEBUG_LOG_INFO("Font fallback added: {} -> {}", primary_font_id, fallback_font_id);
    }
}

std::vector<std::string> FontManager::GetFontFallbacks(const std::string& font_id) const {
    auto it = font_fallbacks_.find(font_id);
    if (it != font_fallbacks_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> FontManager::GetAllFontIds() const {
    std::vector<std::string> font_ids;
    for (const auto& [font_id, _] : fonts_) {
        font_ids.push_back(font_id);
    }
    return font_ids;
}

void FontManager::Clear() {
    for (auto& [font_id, font] : fonts_) {
        UnloadFont(font_id);
    }
    fonts_.clear();
    default_font_id_ = "default";
    language_font_map_.clear();
    font_fallbacks_.clear();
}

FontManager& FontManager::GetInstance() {
    static FontManager instance;
    return instance;
}

} // namespace gameplay2d
} // namespace dse
