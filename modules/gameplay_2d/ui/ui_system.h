/**
 * @file ui_system.h
 * @brief 用户界面(UI)系统，管理 UI 元素的布局、事件响应和渲染
 */

#ifndef DSE_UI_SYSTEM_H
#define DSE_UI_SYSTEM_H

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "engine/core/dse_export.h"

namespace dse {
namespace gameplay2d {

/**
 * @class UISystem
 * @brief UI 管理系统，负责所有界面元素的层级布局计算和交互事件(如点击、悬停)的分发
 */
class DSE_EXPORT UISystem {
public:
    UISystem() = default;
    ~UISystem() = default;

    /**
     * @brief 更新 UI 布局并处理输入事件
     * @param registry ECS 注册表
     * @param dt 距离上一帧的增量时间
     * @param screen_size 当前屏幕尺寸
     * @param mouse_pos 当前鼠标位置
     * @param is_mouse_down 当前鼠标主键是否按下
     * @example
     * // ui_system.Update(world.registry(), dt, screen_size, mouse_pos, is_down);
     */
    void Update(entt::registry& registry, float dt, const glm::vec2& screen_size, const glm::vec2& mouse_pos, bool is_mouse_down);

private:
    /**
     * @struct RichGlyph
     * @brief 存储富文本单个字符及其解析后颜色的结构体
     */
    struct RichGlyph {
        char ch;            ///< 字符
        glm::vec4 color;    ///< 该字符的颜色
    };

    /**
     * @brief 同步文本标签组件的数据到渲染格式
     * @param registry ECS 注册表
     */
    void SyncLabels(entt::registry& registry);

    /**
     * @brief 更新 UI 动画组件并回写到渲染组件
     * @param registry ECS 注册表
     * @param dt 增量时间
     */
    void UpdateAnimations(entt::registry& registry, float dt);

    /**
     * @brief 解析富文本字符串并构建带有颜色信息的字符序列
     * @param text 包含颜色标签的原始字符串
     * @param default_color 默认文本颜色
     * @return 解析后的富文本字符序列
     */
    std::vector<RichGlyph> BuildRichGlyphs(const std::string& text, const glm::vec4& default_color) const;

    /**
     * @brief 检查给定点是否位于指定 UI 实体的渲染矩形内
     * @param registry ECS 注册表
     * @param entity UI 实体
     * @param point 待检测的点坐标
     * @return 如果在内部则返回 true
     */
    bool IsPointInsideUIRect(entt::registry& registry, entt::entity entity, const glm::vec2& point) const;

    /**
     * @brief 检查给定点的交互是否被任何父级遮罩阻挡
     * @param registry ECS 注册表
     * @param entity UI 实体
     * @param point 待检测的点坐标
     * @return 如果被遮罩拦截则返回 true
     */
    bool IsBlockedByMask(entt::registry& registry, entt::entity entity, const glm::vec2& point) const;
    
    /**
     * @brief 基于锚点和父子层级关系，计算并更新 UI 元素的绝对屏幕矩形
     * @param registry ECS 注册表
     * @param screen_size 当前屏幕尺寸
     */
    void UpdateLayout(entt::registry& registry, const glm::vec2& screen_size);
    
    /**
     * @brief 处理 UI 元素的交互事件，发射点击或状态变更事件到 EventBus
     * @param registry ECS 注册表
     * @param dt 增量时间
     * @param mouse_pos 鼠标位置
     * @param is_mouse_down 鼠标是否按下
     */
    void HandleEvents(entt::registry& registry, float dt, const glm::vec2& mouse_pos, bool is_mouse_down);

    /**
     * @brief 更新滑动条组件的拖拽交互与值映射
     */
    void UpdateSliders(entt::registry& registry, float dt, const glm::vec2& mouse_pos, bool is_mouse_down);

    /**
     * @brief 更新开关组件的切换状态与过渡动画
     */
    void UpdateToggles(entt::registry& registry, float dt);

    /**
     * @brief 更新滚动视图的拖拽滚动、惯性衰减与弹性回弹
     */
    void UpdateScrollViews(entt::registry& registry, float dt, const glm::vec2& mouse_pos, bool is_mouse_down);

    /**
     * @brief 更新文本输入框的光标闪烁
     */
    void UpdateTextInputs(entt::registry& registry, float dt);

    bool was_mouse_down_ = false;
};

} // namespace gameplay2d
} // namespace dse

#endif
