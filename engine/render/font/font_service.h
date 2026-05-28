/**
 * @file font_service.h
 * @brief 字体服务 — 管理 TTF 字体加载、SDF 图集生成、GPU 纹理上传
 *
 * 主流方案：
 * 1. 通过 stb_truetype SDF 生成器产出有符号距离场图集
 * 2. 上传为 GPU 纹理，渲染时使用 SDF fragment shader 实现任意缩放
 * 3. 动态图集：按需光栅化新字形并扩展纹理
 * 4. 通过 ServiceLocator 注册，Lua 脚本可直接调用
 */

#ifndef DSE_RENDER_FONT_SERVICE_H
#define DSE_RENDER_FONT_SERVICE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"
#include "engine/render/font/truetype_font.h"

namespace dse {
namespace render {

/// 已加载字体的运行时数据
struct FontInstance {
    std::string font_id;
    std::string file_path;
    TrueTypeFont font;
    unsigned int gpu_texture_handle = 0;   ///< GPU 纹理句柄（单通道 SDF → RGBA8）
    bool sdf_mode = true;                  ///< 是否使用 SDF 模式
};

/// 字体服务配置
struct FontServiceConfig {
    float default_font_size = 48.0f;       ///< 默认 SDF 光栅化字号
    int default_atlas_width = 2048;        ///< 默认图集宽度
    int default_atlas_height = 2048;       ///< 默认图集高度
    int sdf_padding = 6;                   ///< SDF 扩展像素
    int sdf_on_edge_value = 128;           ///< SDF 边缘值 (0-255)
    float sdf_pixel_dist_scale = 128.0f / 6.0f; ///< SDF 距离缩放
    int first_codepoint = 32;              ///< 默认起始码点
    int num_codepoints = 95;               ///< 默认码点数 (ASCII)
};

/**
 * @class FontService
 * @brief 引擎字体服务，通过 ServiceLocator 注册
 */
class DSE_EXPORT FontService {
public:
    using TextureCreateFn = std::function<unsigned int(int w, int h, const unsigned char* rgba8, bool linear)>;
    using TextureDeleteFn = std::function<void(unsigned int handle)>;

    FontService() = default;
    ~FontService();

    FontService(const FontService&) = delete;
    FontService& operator=(const FontService&) = delete;

    /// 设置 GPU 纹理创建/销毁回调（由引擎初始化时注入）
    void SetTextureCallbacks(TextureCreateFn create_fn, TextureDeleteFn delete_fn);

    /**
     * @brief 加载 TTF 字体并生成 SDF 图集
     * @param font_id 唯一标识
     * @param ttf_path 文件路径
     * @param extra_codepoints 额外码点（如常用汉字）
     * @return 成功返回 true
     */
    bool LoadFont(const std::string& font_id, const std::string& ttf_path,
                  const std::vector<int>& extra_codepoints = {});

    /// 卸载字体并释放 GPU 资源
    void UnloadFont(const std::string& font_id);

    /// 设置默认字体
    bool SetDefaultFont(const std::string& font_id);

    /// 获取默认字体 ID
    const std::string& GetDefaultFontId() const { return default_font_id_; }

    /// 获取字体实例（不存在返回 nullptr）
    FontInstance* GetFont(const std::string& font_id);
    const FontInstance* GetFont(const std::string& font_id) const;

    /// 获取默认字体实例
    FontInstance* GetDefaultFont();

    /// 测量文本宽度（像素，基于指定字体或默认字体）
    float MeasureText(const std::string& text, const std::string& font_id = "",
                      float font_size = 0.0f) const;

    /// 获取行高
    float GetLineHeight(const std::string& font_id = "", float font_size = 0.0f) const;

    /// 获取配置
    FontServiceConfig& GetConfig() { return config_; }
    const FontServiceConfig& GetConfig() const { return config_; }

    /// 释放所有字体
    void Shutdown();

private:
    FontServiceConfig config_;
    std::unordered_map<std::string, std::unique_ptr<FontInstance>> fonts_;
    std::string default_font_id_;
    TextureCreateFn texture_create_fn_;
    TextureDeleteFn texture_delete_fn_;

    unsigned int UploadAtlasToGPU(const TrueTypeFont& font);
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_FONT_SERVICE_H
