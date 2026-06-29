/**
 * @file touch.h
 * @brief 触摸输入抽象层 — 平台无关的多点触控状态管理
 *
 * 与 Input（键盘/鼠标/手柄）平级的全局触摸状态容器。平台层（Android 触屏、
 * 桌面端鼠标模拟）通过 RecordTouch() 喂入触点事件，游戏逻辑通过查询接口读取，
 * 每帧末由引擎主循环调用 Update() 推进触点相位并清理已结束触点。
 */

#ifndef DSE_INPUT_TOUCH_H
#define DSE_INPUT_TOUCH_H

#include <array>
#include "glm/glm.hpp"
#include "engine/core/dse_export.h"

namespace dse::input {

/**
 * @enum TouchPhase
 * @brief 触点生命周期相位（数值与 platform::PlatformApp::TouchCallback 的 phase 约定一致）。
 */
enum class TouchPhase {
    None       = 0,  ///< 无效/空触点
    Began      = 1,  ///< 本帧刚按下
    Moved      = 2,  ///< 本帧发生移动
    Stationary = 3,  ///< 按住但本帧未移动
    Ended      = 4,  ///< 本帧抬起
    Cancelled  = 5   ///< 本帧被系统取消（来电/手势拦截等）
};

/**
 * @struct TouchPoint
 * @brief 单个触点快照。
 */
struct TouchPoint {
    int        finger_id = -1;            ///< 平台分配的触点 ID（同一根手指生命周期内稳定）
    glm::vec2  position{0.0f, 0.0f};      ///< 当前位置（屏幕像素，左上原点）
    glm::vec2  delta{0.0f, 0.0f};         ///< 相对上一帧的位移
    TouchPhase phase = TouchPhase::None;  ///< 当前相位
};

/**
 * @class Touch
 * @brief 全局多点触控状态管理类（接口风格对齐 Input）。
 */
class DSE_EXPORT Touch {
public:
    static constexpr int kMaxTouchPoints = 10;

    /**
     * @brief 平台层喂入触点事件（数据源入口）。
     * @param finger_id 触点 ID
     * @param x,y       屏幕坐标
     * @param phase     触点相位
     *
     * Began 新建触点；Moved 更新位置并累计 delta；Ended/Cancelled 标记结束
     * （保留至本帧末，供查询读取后由 Update() 清除）。
     */
    static void RecordTouch(int finger_id, float x, float y, TouchPhase phase);

    /**
     * @brief 每帧末调用：清除 Ended/Cancelled 触点，Began/Moved → Stationary，归零 delta。
     */
    static void Update();

    /// 当前活跃触点数量。
    static int GetTouchCount();

    /**
     * @brief 按紧凑索引 [0, GetTouchCount()) 读取触点。
     * @return 索引有效返回 true 并写入 out。
     */
    static bool TryGetTouch(int index, TouchPoint& out);

    /**
     * @brief 按 finger_id 读取触点。
     * @return 找到返回 true 并写入 out。
     */
    static bool GetTouchById(int finger_id, TouchPoint& out);

    /// 本帧是否存在任意处于 Began 相位的触点。
    static bool IsAnyTouchDown();

    /// 是否至少有一个活跃触点（按住中）。
    static bool IsAnyTouchActive();

    /// 清空全部触点状态。
    static void Reset();

private:
    static std::array<TouchPoint, kMaxTouchPoints> touches_;
    static int touch_count_;

    static int FindIndexById(int finger_id);
};

} // namespace dse::input

#endif // DSE_INPUT_TOUCH_H
