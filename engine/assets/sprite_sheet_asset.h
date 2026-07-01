/**
 * @file sprite_sheet_asset.h
 * @brief Sprite Sheet 资产加载器 (.dsprite JSON 格式)
 */

#ifndef DSE_ASSETS_SPRITE_SHEET_ASSET_H
#define DSE_ASSETS_SPRITE_SHEET_ASSET_H

#include <glm/glm.hpp>
#include <string>
#include <vector>

/**
 * @struct SpriteFrame
 * @brief 精灵图帧数据 (从 .dsprite 加载)
 */
struct SpriteFrame {
    std::string name;                ///< 帧名称
    glm::vec4 uv_rect;              ///< 归一化 UV 矩形 (x, y, w, h)
    glm::ivec4 pixel_rect;          ///< 像素坐标矩形 (x, y, w, h)
    glm::vec2 pivot = {0.5f, 0.5f}; ///< 锚点 (归一化 [0,1])
    int index = 0;                   ///< 帧索引
};

/**
 * @struct SpriteSheetAsset
 * @brief 精灵图集资产，包含帧列表和源纹理引用
 */
struct SpriteSheetAsset {
    std::string texture_path;        ///< 源纹理路径
    int texture_width = 0;           ///< 纹理像素宽度
    int texture_height = 0;          ///< 纹理像素高度
    std::vector<SpriteFrame> frames; ///< 所有帧数据

    /**
     * @brief 从 .dsprite JSON 文件加载
     * @param path 资产路径
     * @return 是否成功
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief 保存为 .dsprite JSON 文件
     * @param path 输出路径
     * @return 是否成功
     */
    bool SaveToFile(const std::string& path) const;

    /**
     * @brief 按名称查找帧
     * @return 帧指针，找不到返回 nullptr
     */
    const SpriteFrame* FindFrame(const std::string& name) const;

    /**
     * @brief 按索引获取帧 UV rect
     * @return UV rect (x, y, w, h)
     */
    glm::vec4 GetFrameUV(int index) const;
};

#endif // DSE_ASSETS_SPRITE_SHEET_ASSET_H
