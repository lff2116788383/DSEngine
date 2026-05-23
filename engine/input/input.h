/**
 * @file input.h
 * @brief 输入系统，处理键盘、鼠标和控制器的输入事件映射
 */

//
// Created by captain on 2021/6/20.
//

#ifndef UNTITLED_INPUT_H
#define UNTITLED_INPUT_H

#include <array>
#include <unordered_map>
#include "glm/glm.hpp"
#include "engine/core/dse_export.h"

/**
 * @class Input
 * @brief 全局输入管理类，提供键盘和鼠标的输入状态查询。
 */
class DSE_EXPORT Input {
public:
    /**
     * @brief 记录按键事件，键盘按下记录数+1，键盘弹起记录数-1，当记录数为0，说明此时没有按键。
     * @param key_code 键盘按键的枚举值或键码
     * @param key_action 0松手 1按下 2持续按下
     */
    static void RecordKey(unsigned short key_code,unsigned short key_action);

    /**
     * @brief 判断当前帧，指定的键盘按键是否处于被按下的状态
     * @param key_code 键盘按键码
     * @return 如果按下返回 true，否则返回 false
     */
    static bool GetKey(unsigned short key_code);

    /**
     * @brief 判断当前帧，指定的键盘按键是否在这一帧刚刚被按下（按下瞬间）
     * @param key_code 键盘按键码
     * @return 如果刚刚按下返回 true
     */
    static bool GetKeyDown(unsigned short key_code);

    /**
     * @brief 判断当前帧，指定的键盘按键是否在这一帧刚刚被松开（松开瞬间）
     * @param key_code 键盘按键码
     * @return 如果刚刚松开返回 true
     */
    static bool GetKeyUp(unsigned short key_code);

    /**
     * @brief 在每帧结束时调用，用于重置本帧的输入状态标志（清理Down和Up状态）
     */
    static void Update();

    /**
     * @brief 判断是否按了鼠标某个按钮（持续按下状态）
     * @param mouse_button_index 0 表示主按钮（通常为左按钮），1 表示副按钮，2 表示中间按钮。
     * @return 如果按下返回 true
     */
    static bool GetMouseButton(unsigned short mouse_button_index);

    /**
     * @brief 指定鼠标按键是否在当前帧刚刚被按下
     * @param mouse_button_index 0 表示主按钮（通常为左按钮），1 表示副按钮，2 表示中间按钮。
     * @return 如果刚刚按下返回 true
     */
    static bool GetMouseButtonDown(unsigned short mouse_button_index);

    /**
     * @brief 鼠标按钮是否在当前帧刚刚被松开
     * @param mouse_button_index 0 表示主按钮（通常为左按钮），1 表示副按钮，2 表示中间按钮。
     * @return 如果刚刚松开返回 true
     */
    static bool GetMouseButtonUp(unsigned short mouse_button_index);

    /**
     * @brief 获取当前鼠标在屏幕上的坐标
     * @return 包含鼠标 x, y 坐标的二维向量
     */
    static glm::vec2 mousePosition(){return mouse_position_;}

    /**
     * @brief 记录鼠标在屏幕上的位置
     * @param x 鼠标的横坐标
     * @param y 鼠标的纵坐标
     */
    static void RecordMousePosition(float x,float y);

    /**
     * @brief 获取当前鼠标滚轮的滚动值
     * @return 鼠标滚轮的值
     */
    static float mouseScroll(){return mouse_scroll_;}

    /**
     * @brief 记录鼠标滚轮的滚动值
     * @param scroll 滚轮的滚动量
     */
    static void RecordMouseScroll(float scroll){mouse_scroll_=scroll;}

    /**
     * @brief 判断指定的按键或鼠标按钮是否在当前帧触发了双击
     * @param key_code 键盘或鼠标的按键码
     * @return 如果触发双击返回 true
     * @example if (Input::GetDoubleClick(MOUSE_BUTTON_LEFT)) { ... }
     */
    static bool GetDoubleClick(unsigned short key_code);

    /**
     * @brief 判断指定的按键或鼠标按钮是否处于长按状态
     * @param key_code 键盘或鼠标的按键码
     * @param duration_seconds 触发长按所需的最小持续时间（秒）
     * @return 如果长按时间达到阈值且当前仍未松开，返回 true
     * @example if (Input::GetLongPress(MOUSE_BUTTON_LEFT, 1.5f)) { ... }
     */
    static bool GetLongPress(unsigned short key_code, float duration_seconds = 1.0f);

    /**
     * @brief 获取当前帧鼠标或触摸的滑动增量（像素）
     * @return 滑动在 X 和 Y 轴上的增量向量
     * @example glm::vec2 delta = Input::GetSwipeDelta();
     */
    static glm::vec2 GetSwipeDelta();

    /**
     * @brief 检测设备当前是否处于摇晃状态（基于输入位移加速度的简单模拟）
     * @return 如果加速度超过阈值判定为摇晃，返回 true
     * @example if (Input::IsDeviceShaking()) { ... }
     */
    static bool IsDeviceShaking();

    /**
     * @brief 重置所有输入状态（用于测试隔离）
     *
     * 清除所有按键映射、鼠标位置、滚轮值和设备摇晃标记，
     * 使 Input 回到等价于进程刚启动的初始状态。
     */
    static void RecordGamepadAxis(int gamepad_id, int axis, float value);
    static float GetGamepadAxis(int gamepad_id, int axis);
    static void SetGamepadConnected(int gamepad_id, bool connected);
    static bool IsGamepadConnected(int gamepad_id);
    static void SetGamepadDeadZone(float dead_zone);
    static float GetGamepadDeadZone();

    static void Reset();

private:
    static std::unordered_map<unsigned short,unsigned short> key_event_map_; ///< 存储按键状态，0松手 1按下 2持续按下

    static std::unordered_map<unsigned short,unsigned short> key_event_map_current_frame_; ///< 存储当前帧按键状态，0无 1按下 2松手

    static std::unordered_map<unsigned short, float> key_down_timestamp_;      ///< 记录按键按下的时间戳
    static std::unordered_map<unsigned short, float> key_last_click_timestamp_;///< 记录按键上一次触发点击（松开）的时间戳
    static std::unordered_map<unsigned short, bool> key_double_click_frame_;   ///< 记录当前帧各按键是否触发了双击

    static glm::vec2 mouse_position_;          ///< 鼠标或触摸的当前位置
    static glm::vec2 previous_mouse_position_; ///< 上一帧记录的鼠标或触摸位置
    static glm::vec2 swipe_delta_;             ///< 当前帧计算出的滑动增量
    static glm::vec2 previous_swipe_delta_;    ///< 上一帧的滑动增量，用于计算加速度

    static float mouse_scroll_; ///< 鼠标滚轮滚动值
    static bool device_shaking_; ///< 当前帧是否触发摇晃判定

    static constexpr int kMaxGamepads = 4;
    static constexpr int kMaxAxes = 6;
    static std::array<std::array<float, 6>, 4> gamepad_axes_;
    static std::array<bool, 4> gamepad_connected_;
    static float gamepad_dead_zone_;

};


#endif //UNTITLED_INPUT_H
