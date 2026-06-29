#ifndef DSE_ECS_VEGETATION_MASK_H
#define DSE_ECS_VEGETATION_MASK_H

#include <algorithm>
#include <vector>
#include <glm/glm.hpp>

namespace dse {

/**
 * @struct VegetationDensityMask
 * @brief 可在编辑器中绘制的植被密度遮罩（世界 XZ 平面上的规则网格）。
 *
 * weights 为行优先 [resolution_z][resolution_x]，取值 [0,1]：
 *   - 1 = 该处保留全部植被实例
 *   - 0 = 该处不生成植被
 * 遮罩 inactive（weights 为空 / 分辨率 < 2）时表示"全图均匀满密度"，
 * 与旧场景完全向后兼容（GrassSystem/TreeSystem 不做任何剔除）。
 *
 * 网格覆盖世界范围 [world_min, world_min + world_size]（仅 XZ）。
 */
struct VegetationDensityMask {
    int resolution_x = 0;
    int resolution_z = 0;
    glm::vec2 world_min = glm::vec2(0.0f);   ///< 网格 (0,0) 角点的世界 XZ
    glm::vec2 world_size = glm::vec2(0.0f);  ///< 网格覆盖的世界 XZ 尺寸
    std::vector<float> weights;              ///< size = resolution_x * resolution_z；空=均匀满密度

    /// 遮罩是否有效（分辨率 >=2 且 weights 尺寸匹配）。
    bool active() const {
        return resolution_x >= 2 && resolution_z >= 2 &&
               static_cast<int>(weights.size()) == resolution_x * resolution_z;
    }
};

/**
 * @brief 在世界坐标 (world_x, world_z) 处双线性采样植被密度权重。
 * @return inactive 遮罩恒返回 1.0；落在网格范围之外返回 0.0；否则返回 [0,1] 双线性插值。
 */
inline float SampleVegetationMask(const VegetationDensityMask& mask,
                                  float world_x, float world_z) {
    if (!mask.active()) return 1.0f;
    if (mask.world_size.x <= 0.0f || mask.world_size.y <= 0.0f) return 1.0f;

    const int rx = mask.resolution_x;
    const int rz = mask.resolution_z;
    float gx = (world_x - mask.world_min.x) / mask.world_size.x * static_cast<float>(rx - 1);
    float gz = (world_z - mask.world_min.y) / mask.world_size.y * static_cast<float>(rz - 1);
    if (gx < 0.0f || gz < 0.0f ||
        gx > static_cast<float>(rx - 1) || gz > static_cast<float>(rz - 1)) {
        return 0.0f;
    }

    int ix = static_cast<int>(gx);
    int iz = static_cast<int>(gz);
    float fx = gx - static_cast<float>(ix);
    float fz = gz - static_cast<float>(iz);
    int ix1 = std::min(ix + 1, rx - 1);
    int iz1 = std::min(iz + 1, rz - 1);

    float w00 = mask.weights[static_cast<size_t>(iz) * rx + ix];
    float w10 = mask.weights[static_cast<size_t>(iz) * rx + ix1];
    float w01 = mask.weights[static_cast<size_t>(iz1) * rx + ix];
    float w11 = mask.weights[static_cast<size_t>(iz1) * rx + ix1];
    float w0 = w00 + fx * (w10 - w00);
    float w1 = w01 + fx * (w11 - w01);
    return w0 + fz * (w1 - w0);
}

}  // namespace dse

#endif  // DSE_ECS_VEGETATION_MASK_H
