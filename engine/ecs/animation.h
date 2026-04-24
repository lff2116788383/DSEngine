/**
 * @file animation.h
 * @brief 2D 帧动画状态机组件
 */

#ifndef DSE_ECS_COMPONENTS_2D_ANIMATION_H
#define DSE_ECS_COMPONENTS_2D_ANIMATION_H

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

class TextureAsset;

/**
 * @struct AnimationState
 * @brief 动画状态，包含一组序列帧和关键帧事件
 */
struct AnimationState {
    std::string name;                                    ///< 状态名称
    std::vector<std::shared_ptr<TextureAsset>> frames;   ///< 帧纹理列表
    std::vector<unsigned int> frame_handles;             ///< 帧句柄列表 (用于 Lua 绑定优化)
    std::vector<std::pair<float, std::string>> events;   ///< 时间点触发的事件列表
    std::vector<std::pair<int, int>> segments;           ///< 动画片段区间
    float frame_rate = 10.0f;                            ///< 播放帧率
    bool loop = true;                                    ///< 是否循环播放
};

/**
 * @struct AnimationTransition
 * @brief 动画状态转换条件
 */
struct AnimationTransition {
    std::string to_state;                                ///< 目标状态名称
    std::string condition_param;                         ///< 条件参数名 (如 "is_walking")
    bool condition_value;                                ///< 触发转换所需的条件值
};

/**
 * @struct AnimatorComponent
 * @brief 动画状态机组件，控制多状态的帧动画播放与切换
 */
struct AnimatorComponent {
    std::unordered_map<std::string, AnimationState> states;              ///< 所有可用状态
    std::unordered_map<std::string, std::vector<AnimationTransition>> transitions; ///< 状态间转换规则
    std::unordered_map<std::string, bool> bool_params;                   ///< 布尔型控制参数
    std::unordered_map<std::string, float> float_params;                 ///< 浮点型控制参数

    std::string current_state = "";                      ///< 当前处于的状态
    float current_time = 0.0f;                           ///< 当前状态已播放的时间
    int current_frame = 0;                               ///< 当前显示的帧索引
    int segment_start_frame = 0;                         ///< 分段播放的起始帧
    int segment_end_frame = -1;                          ///< 分段播放的结束帧
    bool segment_loop = true;                            ///< 分段是否循环
    bool playing = true;                                 ///< 是否正在播放
    std::vector<std::string> fired_events;               ///< 当前帧触发的事件列表
    
    /**
     * @brief 设置布尔参数以驱动状态机
     */
    void SetBool(const std::string& name, bool value) { bool_params[name] = value; }
    /**
     * @brief 设置浮点参数以驱动状态机
     */
    void SetFloat(const std::string& name, float value) { float_params[name] = value; }
    /**
     * @brief 指定播放当前状态的某个分段
     */
    void PlaySegment(int start_frame, int end_frame, bool loop_segment) {
        segment_start_frame = start_frame < 0 ? 0 : start_frame;
        segment_end_frame = end_frame;
        segment_loop = loop_segment;
        current_time = 0.0f;
        current_frame = segment_start_frame;
        playing = true;
    }
};

#endif // DSE_ECS_COMPONENTS_2D_ANIMATION_H
