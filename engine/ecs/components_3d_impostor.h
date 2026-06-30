/**
 * @file components_3d_impostor.h
 * @brief Impostor LOD 组件 — 极远距离时用预烘焙多角度 billboard atlas 替代完整 3D 几何
 *
 * 八面体映射：水平 N 帧 × 垂直 M 帧 覆盖半球/全球视角，运行时根据相机方向
 * 选取最近帧（可选帧间插值）渲染为朝相机 quad。
 */

#ifndef DSE_ECS_COMPONENTS_3D_IMPOSTOR_H
#define DSE_ECS_COMPONENTS_3D_IMPOSTOR_H

#include <string>
#include <glm/glm.hpp>

namespace dse {

/// Impostor atlas 帧布局模式
enum class ImpostorFrameMode : int {
    HemiOctahedron = 0,  ///< 半球八面体映射（从上方+侧面，适合地面物体如树木）
    FullOctahedron = 1,  ///< 全球八面体映射（所有方向，适合飞行物体）
    Billboard      = 2,  ///< 简单水平旋转 billboard（仅水平角度帧）
};

/// Impostor LOD 组件
struct ImpostorComponent {
    bool enabled = true;

    /// Atlas 纹理路径（.dimpostor 格式，含 albedo + normal + depth 通道）
    std::string atlas_path;

    /// 帧布局
    ImpostorFrameMode frame_mode = ImpostorFrameMode::HemiOctahedron;
    int frames_x = 12;   ///< Atlas 水平帧数
    int frames_y = 3;    ///< Atlas 垂直帧数（HemiOctahedron: 3 = 0°/45°/90° 仰角）

    /// 距离控制
    float transition_distance = 100.0f;  ///< 开始切换到 impostor 的距离
    float fade_range          = 10.0f;   ///< 几何→impostor 的交叉渐变距离
    float cull_distance       = 500.0f;  ///< 超过此距离完全剔除 impostor

    /// 渲染参数
    float impostor_size = 1.0f;          ///< Billboard 缩放因子（相对于原始 mesh 包围球直径）
    glm::vec3 pivot_offset = glm::vec3(0.0f);  ///< Billboard 锚点偏移（相对中心，如树木底部）
    bool cast_shadow = false;            ///< Impostor 是否投射阴影（性能换质量）
    bool use_frame_interpolation = true; ///< 帧间双线性插值（减少旋转跳帧）
    float normal_strength = 1.0f;        ///< Atlas 法线图强度（0=平面 billboard，1=全法线）

    /// 自动模式：当 LODGroupComponent 存在时，transition_distance 自动取最远 LOD 的 screen_size
    bool auto_from_lod_group = true;

    // 运行时（ImpostorSystem 管理，用户不应手动写入）
    unsigned int atlas_texture_handle_ = 0;    ///< 已加载的 atlas GPU 纹理句柄
    unsigned int normal_texture_handle_ = 0;   ///< 已加载的法线 atlas GPU 纹理句柄
    bool atlas_loaded_ = false;
    float cached_bounds_radius_ = 0.0f;        ///< 包围球半径缓存（用于 billboard size）
};

} // namespace dse

#endif // DSE_ECS_COMPONENTS_3D_IMPOSTOR_H
