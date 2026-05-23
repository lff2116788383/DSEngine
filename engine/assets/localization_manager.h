/**
 * @file localization_manager.h
 * @brief 本地化管理器，支持 JSON 语言包加载、参数替换与语言切换
 *
 * 与 UILabelComponent::use_localization / localization_key 字段配合使用。
 */

#ifndef DSE_ASSETS_LOCALIZATION_MANAGER_H
#define DSE_ASSETS_LOCALIZATION_MANAGER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include "engine/core/dse_export.h"

namespace dse {
namespace assets {

/**
 * @class LocalizationManager
 * @brief 管理多语言翻译文本的加载与查询
 *
 * JSON 格式示例:
 * {
 *   "ui.button.ok": "确定",
 *   "ui.greeting": "你好, {name}！你有 {count} 条消息。"
 * }
 */
class DSE_EXPORT LocalizationManager {
public:
    LocalizationManager() = default;
    ~LocalizationManager() = default;

    /**
     * @brief 从 JSON 文件加载语言包
     * @param path JSON 文件路径
     * @param locale 语言标识（如 "zh-CN", "en-US"）
     * @return 成功返回 true
     */
    bool LoadLocale(const std::string& path, const std::string& locale);

    /**
     * @brief 从 JSON 字符串加载语言包
     * @param json_str JSON 内容
     * @param locale 语言标识
     * @return 成功返回 true
     */
    bool LoadLocaleFromString(const std::string& json_str, const std::string& locale);

    /**
     * @brief 切换当前活跃语言
     * @param locale 语言标识
     */
    void SetCurrentLocale(const std::string& locale);

    /// 获取当前语言标识
    const std::string& GetCurrentLocale() const { return current_locale_; }

    /// 获取所有已加载的语言标识列表
    std::vector<std::string> GetAvailableLocales() const;

    /**
     * @brief 根据 key 获取翻译文本
     * @param key 本地化键（如 "ui.button.ok"）
     * @return 翻译文本；若当前语言未找到则返回 key 本身
     */
    std::string Get(const std::string& key) const;

    /**
     * @brief 根据 key 获取翻译文本并替换参数
     * @param key 本地化键
     * @param params 参数映射（如 {{"name", "Alice"}, {"count", "3"}}）
     * @return 替换参数后的翻译文本
     *
     * 模板格式: "你好, {name}！" + params["name"]="Alice" → "你好, Alice！"
     */
    std::string Get(const std::string& key,
                    const std::unordered_map<std::string, std::string>& params) const;

    /**
     * @brief 在指定语言包中查找 key
     * @param locale 语言标识
     * @param key 本地化键
     * @return 翻译文本；未找到返回空字符串
     */
    std::string GetForLocale(const std::string& locale, const std::string& key) const;

    /// 检查 key 在当前语言中是否存在
    bool HasKey(const std::string& key) const;

    /// 当前语言包的条目数
    size_t GetEntryCount() const;

    /// 当前语言包的条目数（指定 locale）
    size_t GetEntryCount(const std::string& locale) const;

    /// 注册语言变更监听回调
    void OnLocaleChanged(std::function<void(const std::string&)> callback);

private:
    /// 对文本执行参数替换 {key} → value
    static std::string ReplaceParams(const std::string& text,
                                     const std::unordered_map<std::string, std::string>& params);

    /// locale → (key → value)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> locales_;
    std::string current_locale_;
    std::vector<std::function<void(const std::string&)>> locale_changed_callbacks_;
};

} // namespace assets
} // namespace dse

#endif // DSE_ASSETS_LOCALIZATION_MANAGER_H
