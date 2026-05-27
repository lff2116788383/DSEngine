/**
 * @file world_types.h
 * @brief 大世界坐标类型定义，提供编译期坐标空间区分
 *
 * 坐标空间约定：
 * - AbsoluteWorldPos: 双精度绝对坐标，用于序列化、streaming、origin 累加
 * - LocalWorldPos: 单精度近原点坐标，ECS TransformComponent 使用
 * - CameraRelativePos: 单精度相机相对坐标，仅用于传给 GPU 的 model matrix
 */

#ifndef DSE_CORE_WORLD_TYPES_H
#define DSE_CORE_WORLD_TYPES_H

#include <glm/glm.hpp>

namespace dse {

/// 绝对世界坐标（double），用于序列化、streaming、origin 累加
struct AbsoluteWorldPos {
    glm::dvec3 value{0.0};
};

/// 近原点世界坐标（float），ECS TransformComponent 使用
/// Floating Origin 保证此值始终在原点 ±threshold 范围内
struct LocalWorldPos {
    glm::vec3 value{0.0f};
};

/// 相机相对坐标（float），仅用于传给 GPU 的 model matrix
struct CameraRelativePos {
    glm::vec3 value{0.0f};
};

/// 转换：local world → camera relative
inline CameraRelativePos ToCameraRelative(LocalWorldPos pos, LocalWorldPos camera) {
    return {pos.value - camera.value};
}

/// 转换：local world → absolute（加上累积 origin 偏移）
inline AbsoluteWorldPos ToAbsolute(LocalWorldPos pos, const glm::dvec3& accumulated_origin) {
    return {accumulated_origin + glm::dvec3(pos.value)};
}

/// 转换：absolute → local world（减去累积 origin 偏移）
inline LocalWorldPos ToLocal(AbsoluteWorldPos pos, const glm::dvec3& accumulated_origin) {
    glm::dvec3 local = pos.value - accumulated_origin;
    return {glm::vec3(local)};
}

} // namespace dse

#endif // DSE_CORE_WORLD_TYPES_H
