/**
 * @file truetype_font.h
 * @brief TrueType 字体加载与位图图集生成
 *
 * 基于 stb_truetype + stb_rect_pack，在 CPU 侧将 TTF 字体光栅化为位图图集。
 * 生成的 atlas 可直接上传为 GPU 纹理供 UI/Label 渲染使用。
 */

#ifndef DSE_RENDER_FONT_TRUETYPE_FONT_H
#define DSE_RENDER_FONT_TRUETYPE_FONT_H

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <memory>
#include "engine/core/dse_export.h"

namespace dse {
namespace render {

/// 单个字形的度量与 UV 信息
struct GlyphMetrics {
    int codepoint = 0;           ///< Unicode 码点
    float advance_x = 0.0f;     ///< 水平步进（像素）
    float bearing_x = 0.0f;     ///< 左侧 bearing（像素）
    float bearing_y = 0.0f;     ///< 上方 bearing（像素）
    float width = 0.0f;         ///< 字形位图宽度（像素）
    float height = 0.0f;        ///< 字形位图高度（像素）
    glm::vec4 uv = {0, 0, 0, 0}; ///< 在图集中的 UV (u0, v0, u1, v1)
};

/// 字体图集配置
struct FontAtlasConfig {
    float font_size = 32.0f;            ///< 光栅化字号（像素）
    int atlas_width = 1024;             ///< 图集宽度
    int atlas_height = 1024;            ///< 图集高度
    int padding = 1;                    ///< 字形间距
    int first_codepoint = 32;           ///< 起始码点
    int num_codepoints = 95;            ///< 码点数量（默认 ASCII 可打印字符）
    std::vector<int> extra_codepoints;  ///< 额外码点（如中文常用字）
};

/**
 * @class TrueTypeFont
 * @brief 从 TTF 文件生成位图字体图集
 */
class DSE_EXPORT TrueTypeFont {
public:
    TrueTypeFont() = default;
    ~TrueTypeFont() = default;

    TrueTypeFont(const TrueTypeFont&) = delete;
    TrueTypeFont& operator=(const TrueTypeFont&) = delete;
    TrueTypeFont(TrueTypeFont&&) = default;
    TrueTypeFont& operator=(TrueTypeFont&&) = default;

    /**
     * @brief 从 TTF 文件加载字体并生成图集
     * @param ttf_path TTF 文件路径
     * @param config 图集配置
     * @return 成功返回 true
     */
    bool LoadFromFile(const std::string& ttf_path, const FontAtlasConfig& config = {});

    /**
     * @brief 从内存中的 TTF 数据生成图集
     * @param ttf_data TTF 文件的原始字节
     * @param config 图集配置
     * @return 成功返回 true
     */
    bool LoadFromMemory(const std::vector<uint8_t>& ttf_data, const FontAtlasConfig& config = {});

    /// 是否已成功加载
    bool IsValid() const { return !atlas_bitmap_.empty(); }

    /// 获取图集位图数据（单通道 8-bit）
    const std::vector<uint8_t>& GetAtlasBitmap() const { return atlas_bitmap_; }

    /// 图集尺寸
    int GetAtlasWidth() const { return atlas_width_; }
    int GetAtlasHeight() const { return atlas_height_; }

    /// 字号
    float GetFontSize() const { return font_size_; }

    /// 行高（像素）
    float GetLineHeight() const { return line_height_; }

    /// 基线（像素）
    float GetAscent() const { return ascent_; }
    float GetDescent() const { return descent_; }

    /// 查询字形信息
    const GlyphMetrics* GetGlyph(int codepoint) const;

    /// 获取所有已加载字形
    const std::unordered_map<int, GlyphMetrics>& GetGlyphs() const { return glyphs_; }

    /**
     * @brief 测量文本的像素宽度
     * @param text UTF-8 文本
     * @return 像素宽度
     */
    float MeasureTextWidth(const std::string& text) const;

    struct CharLayout {
        float x = 0.0f;
        float y = 0.0f;
        int codepoint = 0;
    };

    /// 排版参数
    struct LayoutParams {
        float max_width = 0.0f;         ///< 最大宽度 (0=不限制)
        int align = 0;                  ///< 0=左, 1=居中, 2=右
        int overflow = 0;               ///< 0=换行, 1=截断, 2=省略号
        int max_lines = 0;             ///< 最大行数 (0=不限制)
        float line_spacing_extra = 0.0f;///< 额外行间距
    };

    /// 排版结果
    struct LayoutResult {
        std::vector<CharLayout> chars;
        float total_width = 0.0f;      ///< 实际渲染宽度 (最宽行)
        float total_height = 0.0f;     ///< 实际渲染高度
        int line_count = 0;            ///< 总行数
    };

    /// 单行布局（向后兼容）
    std::vector<CharLayout> LayoutText(const std::string& text) const;

    /// 带排版参数的多行布局
    LayoutResult LayoutTextEx(const std::string& text, const LayoutParams& params) const;

    /// 直接设置度量（供 FontService SDF 路径使用，避免双重解析）
    void SetMetrics(float font_size, float ascent, float descent, float line_height,
                    int atlas_w, int atlas_h);

    /// 插入/更新字形度量
    void SetGlyph(int codepoint, const GlyphMetrics& gm);

    /// 替换 atlas bitmap（外部生成的 SDF 数据）
    void SetAtlasBitmap(std::vector<uint8_t>&& bitmap, int width, int height);

private:
    std::vector<uint8_t> atlas_bitmap_;
    int atlas_width_ = 0;
    int atlas_height_ = 0;
    float font_size_ = 0.0f;
    float line_height_ = 0.0f;
    float ascent_ = 0.0f;
    float descent_ = 0.0f;
    std::unordered_map<int, GlyphMetrics> glyphs_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_FONT_TRUETYPE_FONT_H
