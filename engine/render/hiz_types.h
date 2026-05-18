/**
 * @file hiz_types.h
 * @brief Hi-Z Occlusion Culling GPU 数据布局
 *
 * 原位于 modules/gameplay_3d/rendering/mesh_render_system.h，
 * 移至 engine/ 以消除 engine/ → modules/ 的反向依赖。
 */

#ifndef DSE_HIZ_TYPES_H
#define DSE_HIZ_TYPES_H

#include <glm/glm.hpp>

namespace dse {
namespace gameplay3d {

/// Hi-Z AABB 数据（std430 layout: 2 × vec4 per object）
struct HiZAABB {
    glm::vec4 min_point;  // xyz = world min, w = 0
    glm::vec4 max_point;  // xyz = world max, w = 0
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_HIZ_TYPES_H
