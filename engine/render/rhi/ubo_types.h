/**
 * @file ubo_types.h
 * @brief UBO 数据结构定义 - 匹配 GLSL std140 布局
 *
 * P6 UBO 化核心类型：
 * - PerFrameUBO: 每帧更新一次（VP矩阵、相机位置）
 * - PerSceneUBO: 每帧更新一次（方向光、阴影参数、级联分割）
 * - PerMaterialUBO: 每材质切换更新（PBR 材质参数）
 *
 * 设计约束：
 * - 所有结构体严格遵循 std140 对齐规则
 * - vec3 类型统一使用 vec4 + 显式注释标记有效分量，避免对齐陷阱
 * - 标志位（bool/int）统一使用 float 传递，避免 int/bool 在 std140 下的驱动兼容问题
 */

#ifndef DSE_RENDER_UBO_TYPES_H
#define DSE_RENDER_UBO_TYPES_H

#include <glm/glm.hpp>

namespace dse {
namespace render {

// ============================================================
// UBO 绑定点分配
// ============================================================

/// UBO 绑定点枚举 - 与 GLSL uniform block binding 对应
enum class UBOBindingPoint : unsigned int {
    PerFrame        = 0,  ///< 每帧更新的相机/投影数据
    PerScene        = 1,  ///< 每帧更新的光照/阴影数据
    PerMaterial     = 2,  ///< 每材质更新的 PBR 参数
    PointLights     = 3,  ///< 点光源数据
    SpotLights      = 4,  ///< 聚光灯数据
    SpotLightData   = 5,  ///< 聚光灯空间矩阵
    BoneMatrices    = 6,  ///< 骨骼矩阵
    MorphWeights    = 7,  ///< 变形目标权重
    LightProbeData  = 8,  ///< Light Probe SH 球谐系数
    Count           = 9,  ///< 绑定点总数
};

// ============================================================
// PerFrame UBO (binding = 0)
// ============================================================

/**
 * @struct PerFrameUBO
 * @brief 每帧更新一次的相机与投影数据
 *
 * 对应 GLSL:
 *   layout(std140) uniform PerFrame {
 *       mat4 vp;            // 0-63
 *       mat4 view;          // 64-127
 *       vec4 camera_pos;    // 128-143, xyz = 相机世界位置
 *   };
 *
 * 更新频率：每帧 1 次
 * 共享范围：PBR / 天空盒 / 粒子 着色器
 */
struct PerFrameUBO {
    glm::mat4 vp;          ///< 投影×视图矩阵
    glm::mat4 view;        ///< 视图矩阵
    glm::vec4 camera_pos;  ///< xyz = 相机世界位置，w 未使用
};
static_assert(sizeof(PerFrameUBO) == 144, "PerFrameUBO must be 144 bytes for std140 layout");

// ============================================================
// PerScene UBO (binding = 1)
// ============================================================

/**
 * @struct PerSceneUBO
 * @brief 每帧更新的方向光与 CSM 阴影数据
 *
 * 对应 GLSL:
 *   layout(std140) uniform PerScene {
 *       vec4 light_dir_and_enabled;     // 0-15,   xyz = 光方向, w = lighting_enabled
 *       vec4 light_color_and_ambient;   // 16-31,  xyz = 光颜色, w = ambient_intensity
 *       vec4 light_params;              // 32-47,  x = intensity, y = shadow_strength, z = receive_shadow
 *       vec4 cascade_splits;            // 48-63,  xyz = 级联分割距离
 *       mat4 light_space_matrices[3];   // 64-255, CSM 光空间矩阵
 *   };
 *
 * 更新频率：每帧 1 次（在阴影 Pass 前更新）
 * 共享范围：PBR 着色器
 *
 * @note 点光源/聚光灯暂保留为独立 uniform（struct 数组在 std140 下对齐复杂），
 *       后续可扩展为独立 UBO binding。
 */
struct PerSceneUBO {
    glm::vec4 light_dir_and_enabled;     ///< xyz = 光方向(指向光源), w = lighting_enabled (0.0/1.0)
    glm::vec4 light_color_and_ambient;   ///< xyz = 光颜色, w = ambient_intensity
    glm::vec4 light_params;              ///< x = light_intensity, y = shadow_strength, z = receive_shadow (0.0/1.0)
    glm::vec4 cascade_splits;            ///< xyz = CSM 级联分割距离, w 未使用
    glm::mat4 light_space_matrices[3];   ///< CSM 三个级联的光空间矩阵
};
static_assert(sizeof(PerSceneUBO) == 256, "PerSceneUBO must be 256 bytes for std140 layout");

// ============================================================
// PerMaterial UBO (binding = 2)
// ============================================================

/**
 * @struct PerMaterialUBO
 * @brief 每材质切换时更新的 PBR 材质参数
 *
 * 对应 GLSL:
 *   layout(std140) uniform PerMaterial {
 *       vec4 albedo;             // 0-15,   xyz = albedo, w = metallic
 *       vec4 roughness_ao;       // 16-31,  x = roughness, y = ao, z = normal_strength, w = alpha_cutoff
 *       vec4 emissive;           // 32-47,  xyz = emissive, w = alpha_test (0.0/1.0)
 *       vec4 flags;              // 48-63,  x = has_normal_map, y = has_mr_map, z = has_emissive_map, w = has_occlusion_map
 *   };
 *
 * 更新频率：每材质切换 1 次
 * 共享范围：PBR 着色器
 */
struct PerMaterialUBO {
    glm::vec4 albedo;           ///< xyz = 材质基色, w = 金属度
    glm::vec4 roughness_ao;     ///< x = 粗糙度, y = AO, z = 法线强度, w = Alpha 裁剪阈值
    glm::vec4 emissive;         ///< xyz = 自发光颜色, w = alpha_test (0.0/1.0)
    glm::vec4 flags;            ///< x = has_normal_map, y = has_metallic_roughness_map, z = has_emissive_map, w = has_occlusion_map (均为 0.0/1.0)
    glm::vec4 extra_params;     ///< x = sss_strength, y = clear_coat, z = clear_coat_roughness, w = anisotropy
    glm::vec4 extra_params2;    ///< x = pom_height_scale (0=off), y/z/w = sss_tint RGB (0,0,0=default skin)
    glm::vec4 toon_shadow_color; ///< xyz = shadow tint color, w = shadow_threshold
    glm::vec4 toon_params;       ///< x = shadow_softness, y = specular_size, z = specular_strength, w = rim_strength
};
static_assert(sizeof(PerMaterialUBO) == 128, "PerMaterialUBO must be 128 bytes for std140 layout");

// ============================================================
// PointLights UBO (binding = 3)
// ============================================================

struct PointLightEntry {
    glm::vec3 color;   float intensity;
    glm::vec3 position; float radius;
    int cast_shadow;   int shadow_index;
    glm::vec2 _pad;
};
static_assert(sizeof(PointLightEntry) == 48, "PointLightEntry must be 48 bytes for std140");

static constexpr int kMaxPointLightsUBO = 64;

struct PointLightsUBO {
    int u_point_light_count;
    int _pad0, _pad1, _pad2;
    PointLightEntry u_point_lights[kMaxPointLightsUBO];
};
static_assert(sizeof(PointLightsUBO) == 16 + 48 * kMaxPointLightsUBO, "PointLightsUBO size mismatch");

// ============================================================
// SpotLights UBO (binding = 4)
// ============================================================

struct SpotLightEntry {
    glm::vec3 color;   float intensity;
    glm::vec3 position; float radius;
    glm::vec3 direction; float inner_cone;
    float outer_cone;
    int cast_shadow;   int shadow_index;
    float _pad;
};
static_assert(sizeof(SpotLightEntry) == 64, "SpotLightEntry must be 64 bytes for std140");

static constexpr int kMaxSpotLightsUBO = 64;

struct SpotLightsUBO {
    int u_spot_light_count;
    int _pad0, _pad1, _pad2;
    SpotLightEntry u_spot_lights[kMaxSpotLightsUBO];
};
static_assert(sizeof(SpotLightsUBO) == 16 + 64 * kMaxSpotLightsUBO, "SpotLightsUBO size mismatch");

// ============================================================
// SpotLightData UBO (binding = 5)
// ============================================================

struct SpotLightDataUBO {
    glm::mat4 u_spot_light_space_matrices[4];
};
static_assert(sizeof(SpotLightDataUBO) == 256, "SpotLightDataUBO must be 256 bytes");

// ============================================================
// BoneMatrices UBO (binding = 6)
// ============================================================

static constexpr int kMaxBones = 100;
struct BoneMatricesUBO {
    glm::mat4 u_bone_matrices[kMaxBones];
};
static_assert(sizeof(BoneMatricesUBO) == 6400, "BoneMatricesUBO must be 6400 bytes");

// ============================================================
// MorphWeights UBO (binding = 7)
// std140 规则: float 数组每元素占 16 bytes
// ============================================================

static constexpr int kMaxMorphTargets = 4;
struct MorphWeightsUBO {
    glm::vec4 u_morph_weights[kMaxMorphTargets]; ///< 只用 .x 分量
};
static_assert(sizeof(MorphWeightsUBO) == 64, "MorphWeightsUBO must be 64 bytes for std140");

// ============================================================
// LightProbeData UBO (binding = 8)
// ============================================================

/**
 * @struct LightProbeDataUBO
 * @brief Light Probe SH L2 球谐系数，用于间接漫反射光照
 *
 * 对应 GLSL:
 *   layout(std140) uniform LightProbeData {
 *       vec4 sh_coefficients[9];  // SH L2 系数 (RGB in xyz, w unused)
 *       vec4 probe_params;        // x = sh_enabled (0.0/1.0), yzw unused
 *   };
 *
 * 更新频率：每帧 1 次（基于相机位置混合最近探针）
 * 共享范围：PBR 着色器
 */
struct LightProbeDataUBO {
    glm::vec4 sh_coefficients[9]; ///< SH L2 系数 (9 × vec4, 仅 xyz 有效)
    glm::vec4 probe_params;       ///< x = sh_enabled (0.0/1.0), yzw 未使用
};
static_assert(sizeof(LightProbeDataUBO) == 160, "LightProbeDataUBO must be 160 bytes for std140 layout");

} // namespace render
} // namespace dse

#endif // DSE_RENDER_UBO_TYPES_H
