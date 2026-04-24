/**
 * @file gameplay.h
 * @brief 玩法微调参数组件
 */

#ifndef DSE_ECS_COMPONENTS_2D_GAMEPLAY_H
#define DSE_ECS_COMPONENTS_2D_GAMEPLAY_H

/**
 * @struct GameplayTuningComponent
 * @brief 全局/关卡级别的玩法微调参数组件
 */
struct GameplayTuningComponent {
    float leaf_min_distance = 80.0f;                     ///< 树叶的最小判定距离
    float leaf_move_left = 140.0f;                       ///< 树叶向左移动的阈值
    float leaf_move_right = 410.0f;                      ///< 树叶向右移动的阈值
    float jump_speed_scale = 15.0f;                      ///< 跳跃速度的缩放系数
    float jump_speed_max = 18.0f;                        ///< 允许的最大跳跃速度
    float camera_follow_damping = 0.02f;                 ///< 摄像机跟随的默认阻尼
};

#endif // DSE_ECS_COMPONENTS_2D_GAMEPLAY_H
