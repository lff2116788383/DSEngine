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

class Tween {
public:
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

    static float Lerp(float start, float end, float t, EaseType type = EaseType::Linear) {
        float eased_t = Evaluate(type, t);
        return start + (end - start) * eased_t;
    }
};

} // namespace utils
} // namespace dse

#endif // DSE_UTILS_TWEEN_H
