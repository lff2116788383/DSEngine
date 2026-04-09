/**
 * @file localization_system.cpp
 * @brief 国际化系统实现
 */

#include "localization_system.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include "engine/base/debug.h"

namespace dse {
namespace gameplay2d {

bool LocalizationSystem::LoadLanguage(const std::string& language_code, const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            DEBUG_LOG_ERROR("Failed to open localization file: {}", config_path);
            return false;
        }
        
        rapidjson::IStreamWrapper isw(file);
        rapidjson::Document document;
        document.ParseStream(isw);
        
        if (document.HasParseError()) {
            DEBUG_LOG_ERROR("Failed to parse localization JSON: {}", config_path);
            return false;
        }
        
        if (!document.IsObject()) {
            DEBUG_LOG_ERROR("Localization JSON root must be an object: {}", config_path);
            return false;
        }
        
        // 清空该语言的旧数据
        language_data_[language_code].clear();
        
        // 递归加载所有键值对
        std::function<void(const rapidjson::Value&, const std::string&)> load_recursive =
            [&](const rapidjson::Value& obj, const std::string& prefix) {
                for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
                    std::string key = prefix.empty() ? it->name.GetString() : prefix + "." + it->name.GetString();
                    
                    if (it->value.IsString()) {
                        language_data_[language_code][key] = it->value.GetString();
                    } else if (it->value.IsObject()) {
                        load_recursive(it->value, key);
                    }
                }
            };
        
        load_recursive(document, "");
        
        DEBUG_LOG_INFO("Loaded localization for language: {} ({} entries)",
                       language_code, language_data_[language_code].size());
        return true;
        
    } catch (const std::exception& e) {
        DEBUG_LOG_ERROR("Exception while loading localization: {}", e.what());
        return false;
    }
}

bool LocalizationSystem::SetCurrentLanguage(const std::string& language_code) {
    if (language_data_.find(language_code) == language_data_.end()) {
        DEBUG_LOG_WARN("Language not loaded: {}", language_code);
        return false;
    }
    
    if (current_language_ != language_code) {
        current_language_ = language_code;
        
        // 触发回调
        for (auto& [id, callback] : language_change_callbacks_) {
            if (callback) {
                callback(language_code);
            }
        }
        
        DEBUG_LOG_INFO("Language changed to: {}", language_code);
    }
    
    return true;
}

std::vector<std::string> LocalizationSystem::GetAvailableLanguages() const {
    std::vector<std::string> languages;
    for (const auto& [lang_code, _] : language_data_) {
        languages.push_back(lang_code);
    }
    return languages;
}

std::string LocalizationSystem::GetText(const std::string& key, const std::string& default_text) const {
    auto it = language_data_.find(current_language_);
    if (it == language_data_.end()) {
        return default_text;
    }
    
    auto text_it = it->second.find(key);
    if (text_it == it->second.end()) {
        DEBUG_LOG_WARN("Localization key not found: {} (language: {})", key, current_language_);
        return default_text;
    }
    
    return text_it->second;
}

std::string LocalizationSystem::GetTextWithParams(
    const std::string& key,
    const std::unordered_map<std::string, std::string>& params,
    const std::string& default_text
) const {
    std::string text = GetText(key, default_text);
    return ReplaceParams(text, params);
}

TextDirection LocalizationSystem::DetectTextDirection(const std::string& text) const {
    // 检查文本中的第一个强方向字符
    for (unsigned char ch : text) {
        if (IsRTLCharacter(ch)) {
            return TextDirection::RTL;
        }
        // ASCII 字母表示 LTR
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
            return TextDirection::LTR;
        }
    }
    
    // 默认为 LTR
    return TextDirection::LTR;
}

bool LocalizationSystem::IsRTLLanguage(const std::string& language_code) const {
    return std::find(rtl_languages_.begin(), rtl_languages_.end(), language_code) != rtl_languages_.end();
}

int LocalizationSystem::OnLanguageChanged(std::function<void(const std::string&)> callback) {
    int callback_id = next_callback_id_++;
    language_change_callbacks_[callback_id] = callback;
    return callback_id;
}

void LocalizationSystem::UnregisterLanguageChangeCallback(int callback_id) {
    language_change_callbacks_.erase(callback_id);
}

void LocalizationSystem::Clear() {
    language_data_.clear();
    current_language_ = "en";
    language_change_callbacks_.clear();
    next_callback_id_ = 0;
}

std::string LocalizationSystem::ReplaceParams(
    const std::string& text,
    const std::unordered_map<std::string, std::string>& params
) const {
    std::string result = text;
    
    for (const auto& [param_name, param_value] : params) {
        std::string placeholder = "{" + param_name + "}";
        size_t pos = 0;
        
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), param_value);
            pos += param_value.length();
        }
    }
    
    return result;
}

bool LocalizationSystem::IsRTLCharacter(unsigned char ch) const {
    // 阿拉伯字符范围：0xD8-0xDF (UTF-8 编码的阿拉伯字符的第一字节)
    // 希伯来字符范围：0xD7 (UTF-8 编码的希伯来字符的第一字节)
    // 这是一个简化的检测，实际应该使用 Unicode 库
    return (ch >= 0xD7 && ch <= 0xDF);
}

LocalizationSystem& LocalizationSystem::GetInstance() {
    static LocalizationSystem instance;
    return instance;
}

} // namespace gameplay2d
} // namespace dse
