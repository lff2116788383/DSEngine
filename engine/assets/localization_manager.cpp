/**
 * @file localization_manager.cpp
 * @brief 本地化管理器实现
 */

#include "engine/assets/localization_manager.h"
#include "engine/base/debug.h"

#include <fstream>
#include <sstream>

namespace dse {
namespace assets {

// 极简 JSON 字符串键值对解析（仅支持 flat { "key": "value", ... } 格式）
static bool ParseFlatJsonKV(const std::string& json,
                            std::unordered_map<std::string, std::string>& out) {
    out.clear();
    size_t pos = json.find('{');
    if (pos == std::string::npos) return false;
    ++pos;

    auto skip_ws = [&]() {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' ||
               json[pos] == '\n' || json[pos] == '\r'))
            ++pos;
    };

    auto parse_string = [&](std::string& result) -> bool {
        skip_ws();
        if (pos >= json.size() || json[pos] != '"') return false;
        ++pos;
        result.clear();
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        if (pos >= json.size()) return false;
        ++pos; // skip closing "
        return true;
    };

    while (pos < json.size()) {
        skip_ws();
        if (pos >= json.size() || json[pos] == '}') break;
        if (json[pos] == ',') { ++pos; continue; }

        std::string key, value;
        if (!parse_string(key)) return false;
        skip_ws();
        if (pos >= json.size() || json[pos] != ':') return false;
        ++pos;
        if (!parse_string(value)) return false;

        out[key] = value;
    }
    return true;
}

bool LocalizationManager::LoadLocale(const std::string& path, const std::string& locale) {
    std::ifstream file(path);
    if (!file.is_open()) {
        DEBUG_LOG_ERROR("LocalizationManager: 无法打开文件 {}", path);
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return LoadLocaleFromString(ss.str(), locale);
}

bool LocalizationManager::LoadLocaleFromString(const std::string& json_str, const std::string& locale) {
    std::unordered_map<std::string, std::string> entries;
    if (!ParseFlatJsonKV(json_str, entries)) {
        DEBUG_LOG_ERROR("LocalizationManager: JSON 解析失败, locale={}", locale);
        return false;
    }
    locales_[locale] = std::move(entries);
    if (current_locale_.empty()) {
        current_locale_ = locale;
    }
    DEBUG_LOG_INFO("LocalizationManager: 加载 locale={}, 条目数={}",
                   locale, locales_[locale].size());
    return true;
}

void LocalizationManager::SetCurrentLocale(const std::string& locale) {
    if (current_locale_ == locale) return;
    current_locale_ = locale;
    for (auto& cb : locale_changed_callbacks_) {
        if (cb) cb(current_locale_);
    }
}

std::vector<std::string> LocalizationManager::GetAvailableLocales() const {
    std::vector<std::string> result;
    result.reserve(locales_.size());
    for (auto& [locale, _] : locales_) {
        result.push_back(locale);
    }
    return result;
}

std::string LocalizationManager::Get(const std::string& key) const {
    auto locale_it = locales_.find(current_locale_);
    if (locale_it == locales_.end()) return key;
    auto entry_it = locale_it->second.find(key);
    return entry_it != locale_it->second.end() ? entry_it->second : key;
}

std::string LocalizationManager::Get(
    const std::string& key,
    const std::unordered_map<std::string, std::string>& params) const {
    std::string text = Get(key);
    if (params.empty()) return text;
    return ReplaceParams(text, params);
}

std::string LocalizationManager::GetForLocale(const std::string& locale,
                                               const std::string& key) const {
    auto locale_it = locales_.find(locale);
    if (locale_it == locales_.end()) return "";
    auto entry_it = locale_it->second.find(key);
    return entry_it != locale_it->second.end() ? entry_it->second : "";
}

bool LocalizationManager::HasKey(const std::string& key) const {
    auto locale_it = locales_.find(current_locale_);
    if (locale_it == locales_.end()) return false;
    return locale_it->second.count(key) > 0;
}

size_t LocalizationManager::GetEntryCount() const {
    return GetEntryCount(current_locale_);
}

size_t LocalizationManager::GetEntryCount(const std::string& locale) const {
    auto it = locales_.find(locale);
    return it != locales_.end() ? it->second.size() : 0;
}

void LocalizationManager::OnLocaleChanged(std::function<void(const std::string&)> callback) {
    locale_changed_callbacks_.push_back(std::move(callback));
}

std::string LocalizationManager::ReplaceParams(
    const std::string& text,
    const std::unordered_map<std::string, std::string>& params) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ) {
        if (text[i] == '{') {
            size_t end = text.find('}', i + 1);
            if (end != std::string::npos) {
                std::string param_name = text.substr(i + 1, end - i - 1);
                auto it = params.find(param_name);
                if (it != params.end()) {
                    result += it->second;
                } else {
                    result += text.substr(i, end - i + 1);
                }
                i = end + 1;
            } else {
                result += text[i];
                ++i;
            }
        } else {
            result += text[i];
            ++i;
        }
    }
    return result;
}

} // namespace assets
} // namespace dse
