/**
 * @file tween.h
 * @brief 补间动画工具，提供基础的缓动函数和插值计算
 */

#ifndef DSE_UTILS_TWEEN_H
#define DSE_UTILS_TWEEN_H

#include <functional>
#include <cmath>

namespace dse {
namespace utils {

enum class EaseType {
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad
};

/**
 * @class Tween
 * @brief 补间计算类，提供多种缓动曲线评估及线性插值方法
 */
class Tween {
public:
    /**
     * @brief 评估缓动曲线在给定时间点的值
     * @param type 缓动曲线类型
     * @param t 归一化的时间参数 [0.0, 1.0]
     * @return 缓动后的插值权重
     */
    static float Evaluate(EaseType type, float t) {
        // Clamp t between 0 and 1
        t = std::fmax(0.0f, std::fmin(1.0f, t));
        
        switch (type) {
            case EaseType::Linear:
                return t;
            case EaseType::EaseInQuad:
                return t * t;
            case EaseType::EaseOutQuad:
                return t * (2.0f - t);
            case EaseType::EaseInOutQuad:
                return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
            default:
                return t;
        }
    }

    /**
     * @brief 使用指定的缓动类型在两个值之间进行插值
     * @param start 起始值
     * @param end 目标值
     * @param t 归一化的时间参数 [0.0, 1.0]
     * @param type 缓动曲线类型，默认为线性
     * @return 插值结果
     */
    static float Lerp(float start, float end, float t, EaseType type = EaseType::Linear) {
        float eased_t = Evaluate(type, t);
        return start + (end - start) * eased_t;
    }
};

} // namespace utils
} // namespace dse

#endif // DSE_UTILS_TWEEN_H
