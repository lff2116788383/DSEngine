/**
 * @file hair_instance.h
 * @brief 毛发实例 GPU 资源管理
 *
 * HairInstance 持有一份 HairAsset 在 GPU 上的运行时数据：
 * - position SSBO (当前帧 + 上一帧，用于 Verlet 积分)
 * - tangent SSBO
 * - strand info SSBO
 * - simulation parameters
 *
 * 依赖方向: engine/ 层
 */

#ifndef DSE_RENDER_HAIR_INSTANCE_H
#define DSE_RENDER_HAIR_INSTANCE_H

#include "engine/render/hair/hair_asset.h"
#include "engine/render/rhi/rhi_handle.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace dse {
namespace render {

class RhiDevice;

/// 毛发物理模拟参数
struct HairSimParams {
    float damping          = 0.04f;   ///< 速度衰减
    float stiffness_local  = 0.8f;    ///< 局部形状保持刚度
    float stiffness_global = 0.4f;    ///< 全局形状保持刚度
    float gravity_magnitude = 9.81f;  ///< 重力加速度
    glm::vec3 gravity_dir  = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 wind         = glm::vec3(0.0f);  ///< 风力向量
    float wind_turbulence  = 0.2f;
    int local_constraint_iterations  = 2;  ///< 局部约束迭代次数
    int length_constraint_iterations = 2;  ///< 长度约束迭代次数
};

/// 毛发渲染参数
struct HairRenderParams {
    glm::vec4 root_color = glm::vec4(0.1f, 0.05f, 0.02f, 1.0f);
    glm::vec4 tip_color  = glm::vec4(0.4f, 0.25f, 0.15f, 1.0f);
    float fiber_radius   = 0.04f;     ///< 发丝半径（世界空间）
    float opacity        = 0.9f;      ///< 全局不透明度
    float specular_power_primary   = 80.0f;   ///< Kajiya-Kay 一次高光指数
    float specular_power_secondary = 20.0f;   ///< Kajiya-Kay 二次高光指数
    float specular_strength_primary   = 0.6f;
    float specular_strength_secondary = 0.3f;
    glm::vec3 specular_color = glm::vec3(1.0f, 0.9f, 0.8f);
    float shadow_density = 0.5f;       ///< DOM 阴影密度
    bool  receive_shadow = true;
    bool  cast_shadow    = true;
};

/// 毛发 LOD 参数
struct HairLODParams {
    float lod0_distance = 20.0f;   ///< 全精度距离
    float lod1_distance = 40.0f;   ///< 降低 strand 数
    float lod2_distance = 80.0f;   ///< 最低质量
    float cull_distance = 120.0f;  ///< 完全剔除
    float lod1_strand_ratio = 0.5f;  ///< LOD1 strand 保留比例
    float lod2_strand_ratio = 0.25f; ///< LOD2 strand 保留比例
};

/// 毛发 GPU 实例（运行时）
struct HairInstance {
    const HairAsset* asset = nullptr;

    /// GPU SSBO 句柄
    BufferHandle position_ssbo;       ///< vec4[total_verts] 当前位置
    BufferHandle position_prev_ssbo;  ///< vec4[total_verts] 上帧位置（Verlet）
    BufferHandle position_rest_ssbo;  ///< vec4[total_verts] 静止姿态
    BufferHandle tangent_ssbo;        ///< vec4[total_verts] 切线
    BufferHandle strand_info_ssbo;    ///< uvec2[num_strands] (offset, count)

    /// 参数
    HairSimParams    sim_params;
    HairRenderParams render_params;
    HairLODParams    lod_params;

    /// 变换（挂载到角色头部）
    glm::mat4 world_transform = glm::mat4(1.0f);

    /// 运行时 LOD 状态
    int current_lod = 0;           ///< 0=full, 1=mid, 2=low, 3=culled
    uint32_t active_strand_count = 0;  ///< 当前帧实际模拟的 strand 数
    uint32_t total_vertex_count  = 0;  ///< asset->num_vertices()

    bool gpu_resources_valid = false;

    /// Compute shader 懒编译失败后置位，后续帧直接跳过（避免每帧重试 + 错误刷屏）。
    /// 桌面后端不会触发；仅当某后端 SupportsCompute()=true 但未提供该特性手译 WGSL（返回 0 句柄）时生效。
    bool compute_unavailable_ = false;

    /// Compute shader 句柄（懒加载，首次 Simulate 时编译）
    unsigned int cs_integrate_   = 0;
    unsigned int cs_length_      = 0;
    unsigned int cs_local_shape_ = 0;
    unsigned int cs_tangent_     = 0;

    /// CPU 侧 per-strand 绘制参数（用于 glMultiDrawArrays）
    std::vector<int> draw_firsts_;   ///< 每 strand 的顶点起始索引
    std::vector<int> draw_counts_;   ///< 每 strand 的顶点数

    /// 在 GPU 上创建 SSBO 资源
    bool CreateGPUResources(RhiDevice* rhi, const HairAsset& hair_asset);

    /// 释放 GPU 资源
    void DestroyGPUResources(RhiDevice* rhi);

    /// 上传初始位置数据
    void UploadInitialPositions(RhiDevice* rhi, const HairAsset& hair_asset);

    /// 更新 LOD 级别（基于相机距离）
    void UpdateLOD(float camera_distance);

    /// 物理模拟：执行四个 compute pass（懒加载 shader，首次调用时编译）
    void Simulate(RhiDevice* rhi, float dt, float time);

    /// 释放 compute shader 句柄（Shutdown 时调用）
    void DestroyComputeShaders(RhiDevice* rhi);
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_HAIR_INSTANCE_H
