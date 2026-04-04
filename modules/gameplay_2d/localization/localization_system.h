/**
 * @file localization_system.h
 * @brief 国际化系统，支持多语言、字体管理、RTL 文本
 */

#ifndef DSE_LOCALIZATION_SYSTEM_H
#define DSE_LOCALIZATION_SYSTEM_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace dse {
namespace gameplay2d {

/**
 * @enum TextDirection
 * @brief 文本方向枚举
 */
enum class TextDirection {
    LTR,  ///< 从左到右（Left-to-Right）
    RTL   ///< 从右到左（Right-to-Left）
};

/**
 * @class LocalizationSystem
 * @brief 国际化系统，管理多语言文本、语言切换、参数化文本
 */
class LocalizationSystem {
public:
    /**
     * @brief 构造函数
     */
    LocalizationSystem() = default;
    
    /**
     * @brief 析构函数
     */
    ~LocalizationSystem() = default;
    
    /**
     * @brief 加载多语言配置文件（JSON 格式）
     * @param language_code 语言代码（如 "en", "zh", "ar"）
     * @param config_path 配置文件路径
     * @return 加载是否成功
     * @example
     * // localization.LoadLanguage("en", "data/localization/en.json");
     */
    bool LoadLanguage(const std::string& language_code, const std::string& config_path);
    
    /**
     * @brief 设置当前语言
     * @param language_code 语言代码
     * @return 设置是否成功
     * @example
     * // localization.SetCurrentLanguage("zh");
     */
    bool SetCurrentLanguage(const std::string& language_code);
    
    /**
     * @brief 获取当前语言代码
     * @return 当前语言代码
     */
    const std::string& GetCurrentLanguage() const { return current_language_; }
    
    /**
     * @brief 获取已加载的所有语言代码
     * @return 语言代码列表
     */
    std::vector<std::string> GetAvailableLanguages() const;
    
    /**
     * @brief 获取本地化文本
     * @param key 文本键
     * @param default_text 默认文本（如果键不存在）
     * @return 本地化文本
     * @example
     * // std::string text = localization.GetText("ui.button.ok", "OK");
     */
    std::string GetText(const std::string& key, const std::string& default_text = "") const;
    
    /**
     * @brief 获取带参数的本地化文本
     * @param key 文本键
     * @param params 参数映射（参数名 -> 参数值）
     * @param default_text 默认文本
     * @return 本地化文本（已替换参数）
     * @example
     * // std::unordered_map<std::string, std::string> params = {{"name", "Player"}};
     * // std::string text = localization.GetTextWithParams("ui.greeting", params, "Hello {name}");
     */
    std::string GetTextWithParams(
        const std::string& key,
        const std::unordered_map<std::string, std::string>& params,
        const std::string& default_text = ""
    ) const;
    
    /**
     * @brief 检测文本方向
     * @param text 文本内容
     * @return 文本方向
     * @example
     * // TextDirection dir = localization.DetectTextDirection("مرحبا");
     */
    TextDirection DetectTextDirection(const std::string& text) const;
    
    /**
     * @brief 检查语言是否为 RTL（从右到左）
     * @param language_code 语言代码
     * @return 是否为 RTL 语言
     */
    bool IsRTLLanguage(const std::string& language_code) const;
    
    /**
     * @brief 注册语言变更回调
     * @param callback 回调函数，参数为新语言代码
     * @return 回调 ID（用于后续注销）
     * @example
     * // int callback_id = localization.OnLanguageChanged([](const std::string& lang) {
     * //     std::cout << "Language changed to: " << lang << std::endl;
     * // });
     */
    int OnLanguageChanged(std::function<void(const std::string&)> callback);
    
    /**
     * @brief 注销语言变更回调
     * @param callback_id 回调 ID
     */
    void UnregisterLanguageChangeCallback(int callback_id);
    
    /**
     * @brief 清空所有已加载的语言数据
     */
    void Clear();
    
    /**
     * @brief 获取系统单例
     * @return LocalizationSystem 单例
     */
    static LocalizationSystem& GetInstance();

private:
    /**
     * @brief 替换文本中的参数
     * @param text 原始文本
     * @param params 参数映射
     * @return 替换后的文本
     */
    std::string ReplaceParams(
        const std::string& text,
        const std::unordered_map<std::string, std::string>& params
    ) const;
    
    /**
     * @brief 检查字符是否为 RTL 字符
     * @param ch 字符
     * @return 是否为 RTL 字符
     */
    bool IsRTLCharacter(unsigned char ch) const;
    
    // 多语言数据存储：语言代码 -> (键 -> 文本)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> language_data_;
    
    // 当前语言代码
    std::string current_language_ = "en";
    
    // RTL 语言列表
    std::vector<std::string> rtl_languages_ = {"ar", "he", "fa", "ur"};
    
    // 语言变更回调
    std::unordered_map<int, std::function<void(const std::string&)>> language_change_callbacks_;
    int next_callback_id_ = 0;
};

} // namespace gameplay2d
} // namespace dse

#endif // DSE_LOCALIZATION_SYSTEM_H
