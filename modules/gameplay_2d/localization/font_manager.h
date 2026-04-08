/**
 * @file font_manager.h
 * @brief 字体管理系统，支持多字体加载、缓存、动态选择
 */

#ifndef DSE_FONT_MANAGER_H
#define DSE_FONT_MANAGER_H

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace dse {
namespace gameplay2d {

/**
 * @struct FontAsset
 * @brief 字体资产结构
 */
struct FontAsset {
    struct FontDataHandle {
        virtual ~FontDataHandle() = default;
    };

    std::string font_id;                                       ///< 字体唯一标识
    std::string font_path;                                     ///< 字体文件路径
    int font_size = 32;                                        ///< 字体大小（像素）
    bool is_loaded = false;                                    ///< 是否已加载
    std::shared_ptr<FontDataHandle> font_data;                 ///< 字体数据句柄（由具体渲染后端管理）
};

/**
 * @class FontManager
 * @brief 字体管理器，管理字体的加载、缓存、选择
 */
class FontManager {
public:
    /**
     * @brief 构造函数
     */
    FontManager() = default;
    
    /**
     * @brief 析构函数
     */
    ~FontManager();
    
    /**
     * @brief 注册字体
     * @param font_id 字体唯一标识
     * @param font_path 字体文件路径
     * @param font_size 字体大小（像素）
     * @return 注册是否成功
     * @example
     * // font_manager.RegisterFont("default", "data/fonts/Arial.ttf", 32);
     */
    bool RegisterFont(const std::string& font_id, const std::string& font_path, int font_size = 32);
    
    /**
     * @brief 加载字体
     * @param font_id 字体唯一标识
     * @return 加载是否成功
     * @example
     * // font_manager.LoadFont("default");
     */
    bool LoadFont(const std::string& font_id);
    
    /**
     * @brief 卸载字体
     * @param font_id 字体唯一标识
     */
    void UnloadFont(const std::string& font_id);
    
    /**
     * @brief 获取字体资产
     * @param font_id 字体唯一标识
     * @return 字体资产指针（如果不存在返回 nullptr）
     */
    FontAsset* GetFont(const std::string& font_id);
    
    /**
     * @brief 获取字体资产（常量版本）
     * @param font_id 字体唯一标识
     * @return 字体资产常量指针
     */
    const FontAsset* GetFont(const std::string& font_id) const;
    
    /**
     * @brief 设置默认字体
     * @param font_id 字体唯一标识
     * @return 设置是否成功
     */
    bool SetDefaultFont(const std::string& font_id);
    
    /**
     * @brief 获取默认字体
     * @return 默认字体的 ID
     */
    const std::string& GetDefaultFont() const { return default_font_id_; }
    
    /**
     * @brief 为语言设置字体
     * @param language_code 语言代码（如 "en", "zh", "ar"）
     * @param font_id 字体唯一标识
     * @return 设置是否成功
     * @example
     * // font_manager.SetFontForLanguage("zh", "chinese_font");
     */
    bool SetFontForLanguage(const std::string& language_code, const std::string& font_id);
    
    /**
     * @brief 获取语言对应的字体
     * @param language_code 语言代码
     * @return 字体 ID（如果未设置则返回默认字体）
     */
    std::string GetFontForLanguage(const std::string& language_code) const;
    
    /**
     * @brief 添加字体回退链
     * @param primary_font_id 主字体 ID
     * @param fallback_font_id 回退字体 ID
     * @example
     * // font_manager.AddFontFallback("chinese_font", "default");
     */
    void AddFontFallback(const std::string& primary_font_id, const std::string& fallback_font_id);
    
    /**
     * @brief 获取字体的回退链
     * @param font_id 字体 ID
     * @return 回退字体 ID 列表
     */
    std::vector<std::string> GetFontFallbacks(const std::string& font_id) const;
    
    /**
     * @brief 获取所有已注册的字体 ID
     * @return 字体 ID 列表
     */
    std::vector<std::string> GetAllFontIds() const;
    
    /**
     * @brief 清空所有字体
     */
    void Clear();
    
    /**
     * @brief 获取系统单例
     * @return FontManager 单例
     */
    static FontManager& GetInstance();

private:
    // 字体资产存储：字体 ID -> 字体资产
    std::unordered_map<std::string, FontAsset> fonts_;
    
    // 默认字体 ID
    std::string default_font_id_ = "default";
    
    // 语言到字体的映射：语言代码 -> 字体 ID
    std::unordered_map<std::string, std::string> language_font_map_;
    
    // 字体回退链：字体 ID -> 回退字体 ID 列表
    std::unordered_map<std::string, std::vector<std::string>> font_fallbacks_;
};

} // namespace gameplay2d
} // namespace dse

#endif // DSE_FONT_MANAGER_H
