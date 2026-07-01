/**
 * @file atlas_asset.h
 * @brief Sprite Atlas 资产加载器 (.datlas JSON 格式)
 */

#ifndef DSE_ASSETS_ATLAS_ASSET_H
#define DSE_ASSETS_ATLAS_ASSET_H

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @struct AtlasEntry
 * @brief Atlas 中单个子纹理的位置信息
 */
struct AtlasEntry {
    std::string name;                ///< 子纹理名称 (通常为原始文件名)
    glm::ivec4 pixel_rect;          ///< 在 Atlas 纹理上的像素矩形 (x, y, w, h)
    glm::vec4 uv_rect;              ///< 归一化 UV 矩形 (x, y, w, h)
    glm::vec2 pivot = {0.5f, 0.5f}; ///< 锚点
    bool rotated = false;            ///< 是否旋转 90 度打包
    glm::ivec2 original_size;        ///< 原始图片尺寸
};

/**
 * @struct AtlasAsset
 * @brief Sprite Atlas 资产，包含打包后的子纹理索引
 */
struct AtlasAsset {
    std::string texture_path;        ///< Atlas 纹理路径
    int atlas_width = 0;             ///< Atlas 像素宽度
    int atlas_height = 0;            ///< Atlas 像素高度
    std::vector<AtlasEntry> entries; ///< 所有子纹理条目
    std::unordered_map<std::string, int> name_index; ///< 名称到索引的快速查找

    /**
     * @brief 从 .datlas JSON 文件加载
     * @param path 资产路径
     * @return 是否成功
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief 保存为 .datlas JSON 文件
     * @param path 输出路径
     * @return 是否成功
     */
    bool SaveToFile(const std::string& path) const;

    /**
     * @brief 按名称查找子纹理
     * @return 条目指针，找不到返回 nullptr
     */
    const AtlasEntry* FindEntry(const std::string& name) const;

    /**
     * @brief 按名称获取 UV rect
     * @return UV rect (x, y, w, h)，找不到返回 (0,0,1,1)
     */
    glm::vec4 GetEntryUV(const std::string& name) const;

    /**
     * @brief 重建名称索引 (加载后自动调用)
     */
    void RebuildIndex();
};

#endif // DSE_ASSETS_ATLAS_ASSET_H
