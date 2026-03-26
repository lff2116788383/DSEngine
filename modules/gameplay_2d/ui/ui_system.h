/**
 * @file ui_system.h
 * @brief 用户界面(UI)系统，管理 UI 元素的布局、事件响应和渲染
 */

#ifndef DSE_UI_SYSTEM_H
#define DSE_UI_SYSTEM_H

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace dse {
namespace gameplay2d {

/**
 * @class UISystem
 * @brief UI 管理系统，负责所有界面元素的层级布局计算和交互事件(如点击、悬停)的分发
 */
class UISystem {
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
     * @brief 同步文本标签组件的数据到渲染格式
     * @param registry ECS 注册表
     */
    void SyncLabels(entt::registry& registry);
    
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
};

} // namespace gameplay2d
} // namespace dse

#endif
