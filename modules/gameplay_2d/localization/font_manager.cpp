/**
 * @file font_manager.cpp
 * @brief 字体管理系统实现
 */

#include "font_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace dse {
namespace gameplay2d {

FontManager::~FontManager() {
    Clear();
}

bool FontManager::RegisterFont(const std::string& font_id, const std::string& font_path, int font_size) {
    if (fonts_.find(font_id) != fonts_.end()) {
        spdlog::warn("Font already registered: {}", font_id);
        return false;
    }
    
    FontAsset font_asset;
    font_asset.font_id = font_id;
    font_asset.font_path = font_path;
    font_asset.font_size = font_size;
    font_asset.is_loaded = false;
    font_asset.font_data = nullptr;
    
    fonts_[font_id] = font_asset;
    
    spdlog::info("Font registered: {} (path: {}, size: {})", font_id, font_path, font_size);
    return true;
}

bool FontManager::LoadFont(const std::string& font_id) {
    auto it = fonts_.find(font_id);
    if (it == fonts_.end()) {
        spdlog::error("Font not found: {}", font_id);
        return false;
    }
    
    FontAsset& font = it->second;
    
    if (font.is_loaded) {
        spdlog::warn("Font already loaded: {}", font_id);
        return true;
    }
    
    // TODO: 实际的字体加载逻辑应该由具体的渲染后端实现
    // 这里只是标记为已加载
    font.is_loaded = true;
    
    spdlog::info("Font loaded: {}", font_id);
    return true;
}

void FontManager::UnloadFont(const std::string& font_id) {
    auto it = fonts_.find(font_id);
    if (it == fonts_.end()) {
        spdlog::warn("Font not found: {}", font_id);
        return;
    }
    
    FontAsset& font = it->second;
    
    if (!font.is_loaded) {
        return;
    }
    
    // TODO: 实际的字体卸载逻辑应该由具体的渲染后端实现
    font.is_loaded = false;
    font.font_data = nullptr;
    
    spdlog::info("Font unloaded: {}", font_id);
}

FontAsset* FontManager::GetFont(const std::string& font_id) {
    auto it = fonts_.find(font_id);
    if (it == fonts_.end()) {
        // 如果字体不存在，尝试使用默认字体
        if (font_id != default_font_id_) {
            spdlog::warn("Font not found: {}, using default font", font_id);
            return GetFont(default_font_id_);
        }
        spdlog::error("Font not found: {}", font_id);
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
            spdlog::warn("Font not found: {}, using default font", font_id);
            return GetFont(default_font_id_);
        }
        spdlog::error("Font not found: {}", font_id);
        return nullptr;
    }
    
    return &it->second;
}

bool FontManager::SetDefaultFont(const std::string& font_id) {
    if (fonts_.find(font_id) == fonts_.end()) {
        spdlog::error("Font not found: {}", font_id);
        return false;
    }
    
    default_font_id_ = font_id;
    spdlog::info("Default font set to: {}", font_id);
    return true;
}

bool FontManager::SetFontForLanguage(const std::string& language_code, const std::string& font_id) {
    if (fonts_.find(font_id) == fonts_.end()) {
        spdlog::error("Font not found: {}", font_id);
        return false;
    }
    
    language_font_map_[language_code] = font_id;
    spdlog::info("Font set for language {}: {}", language_code, font_id);
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
        spdlog::error("Primary font not found: {}", primary_font_id);
        return;
    }
    
    if (fonts_.find(fallback_font_id) == fonts_.end()) {
        spdlog::error("Fallback font not found: {}", fallback_font_id);
        return;
    }
    
    auto& fallbacks = font_fallbacks_[primary_font_id];
    if (std::find(fallbacks.begin(), fallbacks.end(), fallback_font_id) == fallbacks.end()) {
        fallbacks.push_back(fallback_font_id);
        spdlog::info("Font fallback added: {} -> {}", primary_font_id, fallback_font_id);
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
