#ifndef DSE_GAMEPLAY3D_ANIM_BLEND_WEIGHTS_H
#define DSE_GAMEPLAY3D_ANIM_BLEND_WEIGHTS_H

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

// 纯计算的 blend space 权重函数（不依赖 AssetManager / 骨架），便于单元测试与
// 在 layer 混合系统与 legacy Animator3D 混合树之间复用。
namespace dse {
namespace gameplay3d {
namespace anim_blend {

/// 1D blend space 权重：在按 threshold 升序排列的采样点间做线性插值。
/// 返回与 thresholds 等长、求和为 1 的权重；空输入返回空向量。
inline std::vector<float> ComputeBlend1DWeights(const std::vector<float>& thresholds,
                                                float param) {
    std::vector<float> weights(thresholds.size(), 0.0f);
    if (thresholds.empty()) return weights;
    if (thresholds.size() == 1) {
        weights[0] = 1.0f;
        return weights;
    }

    std::size_t lo = 0, hi = thresholds.size() - 1;
    for (std::size_t i = 0; i < thresholds.size(); ++i) {
        if (thresholds[i] <= param) lo = i;
        if (thresholds[i] >= param) { hi = i; break; }
    }
    if (param <= thresholds.front()) {
        lo = hi = 0;
    } else if (param >= thresholds.back()) {
        lo = hi = thresholds.size() - 1;
    }

    if (lo == hi || std::abs(thresholds[hi] - thresholds[lo]) <= 0.0001f) {
        weights[lo] = 1.0f;
    } else {
        const float t = std::clamp(
            (param - thresholds[lo]) / (thresholds[hi] - thresholds[lo]), 0.0f, 1.0f);
        weights[lo] = 1.0f - t;
        weights[hi] = t;
    }
    return weights;
}

/// 2D blend space 权重：Shepard 反距离加权（IDW, p=2），支持任意点布局。
/// 精确命中某采样点时该点权重为 1。返回求和为 1 的归一化权重；空输入返回空向量。
inline std::vector<float> ComputeBlend2DWeights(const std::vector<glm::vec2>& points,
                                                glm::vec2 param) {
    std::vector<float> weights(points.size(), 0.0f);
    if (points.empty()) return weights;

    for (std::size_t i = 0; i < points.size(); ++i) {
        const glm::vec2 d = param - points[i];
        if (glm::dot(d, d) < 1e-6f) {
            weights[i] = 1.0f;  // 精确命中
            return weights;
        }
    }

    float total = 0.0f;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const glm::vec2 d = param - points[i];
        const float w = 1.0f / std::max(glm::dot(d, d), 1e-8f);
        weights[i] = w;
        total += w;
    }
    if (total > 0.0f) {
        for (float& w : weights) w /= total;
    }
    return weights;
}

} // namespace anim_blend
} // namespace gameplay3d
} // namespace dse

#endif // DSE_GAMEPLAY3D_ANIM_BLEND_WEIGHTS_H
