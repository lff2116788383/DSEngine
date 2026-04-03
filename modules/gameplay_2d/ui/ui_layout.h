/**
 * @file ui_layout.h
 * @brief 高级 UI 布局系统，支持网格布局、自适应布局、锚点系统
 */

#ifndef DSE_UI_LAYOUT_H
#define DSE_UI_LAYOUT_H

#include <glm/glm.hpp>
#include <vector>
#include <entt/entt.hpp>

namespace dse {
namespace gameplay2d {

/**
 * @enum UIAnchor
 * @brief UI 锚点枚举
 */
enum class UIAnchor {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    Stretch  // 拉伸填充
};

/**
 * @enum GridLayoutAlignment
 * @brief 网格布局对齐方式
 */
enum class GridLayoutAlignment {
    UpperLeft,
    UpperCenter,
    UpperRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    LowerLeft,
    LowerCenter,
    LowerRight
};

/**
 * @struct UIAnchorData
 * @brief UI 锚点数据
 */
struct UIAnchorData {
    UIAnchor anchor = UIAnchor::MiddleCenter;  ///< 锚点类型
    glm::vec2 offset = glm::vec2(0.0f);        ///< 锚点偏移
    glm::vec2 size = glm::vec2(100.0f);        ///< 元素大小
};

/**
 * @struct GridLayoutData
 * @brief 网格布局数据
 */
struct GridLayoutData {
    int columns = 1;                           ///< 列数
    int rows = 0;                              ///< 行数（0 表示自动）
    glm::vec2 cell_size = glm::vec2(100.0f);  ///< 单元格大小
    glm::vec2 spacing = glm::vec2(10.0f);     ///< 单元格间距
    GridLayoutAlignment alignment = GridLayoutAlignment::UpperLeft;  ///< 对齐方式
};

/**
 * @struct CanvasScalerData
 * @brief Canvas 缩放器数据
 */
struct CanvasScalerData {
    glm::vec2 reference_resolution = glm::vec2(1920.0f, 1080.0f);  ///< 参考分辨率
    float scale_factor = 1.0f;                                      ///< 缩放因子
    bool match_width_or_height = true;                              ///< 是否匹配宽度或高度
};

/**
 * @class UILayoutCalculator
 * @brief UI 布局计算器，负责计算 UI 元素的最终位置和大小
 */
class UILayoutCalculator {
public:
    /**
     * @brief 计算锚点位置
     * @param anchor 锚点类型
     * @param parent_size 父元素大小
     * @param element_size 元素大小
     * @param offset 锚点偏移
     * @return 计算后的位置
     */
    static glm::vec2 CalculateAnchorPosition(
        UIAnchor anchor,
        const glm::vec2& parent_size,
        const glm::vec2& element_size,
        const glm::vec2& offset = glm::vec2(0.0f)
    );
    
    /**
     * @brief 计算网格布局中元素的位置
     * @param index 元素索引
     * @param layout_data 网格布局数据
     * @param parent_size 父元素大小
     * @return 计算后的位置
     */
    static glm::vec2 CalculateGridPosition(
        int index,
        const GridLayoutData& layout_data,
        const glm::vec2& parent_size
    );
    
    /**
     * @brief 计算 Canvas 缩放因子
     * @param current_resolution 当前分辨率
     * @param scaler_data Canvas 缩放器数据
     * @return 缩放因子
     */
    static float CalculateCanvasScale(
        const glm::vec2& current_resolution,
        const CanvasScalerData& scaler_data
    );
};

/**
 * @struct UIGridLayoutComponent
 * @brief 网格布局组件
 */
struct UIGridLayoutComponent {
    GridLayoutData layout_data;                 ///< 网格布局数据
    std::vector<entt::entity> children;         ///< 子元素列表
    bool dirty = true;                          ///< 是否需要重新计算布局
};

/**
 * @struct UICanvasScalerComponent
 * @brief Canvas 缩放器组件
 */
struct UICanvasScalerComponent {
    CanvasScalerData scaler_data;               ///< Canvas 缩放器数据
    glm::vec2 last_resolution = glm::vec2(0.0f);  ///< 上一帧的分辨率
    bool dirty = true;                          ///< 是否需要重新计算缩放
};

/**
 * @struct UIAnchorComponent
 * @brief UI 锚点组件
 */
struct UIAnchorComponent {
    UIAnchorData anchor_data;                   ///< 锚点数据
    glm::vec2 last_parent_size = glm::vec2(0.0f);  ///< 上一帧的父元素大小
    bool dirty = true;                          ///< 是否需要重新计算位置
};

} // namespace gameplay2d
} // namespace dse

#endif // DSE_UI_LAYOUT_H
