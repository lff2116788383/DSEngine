/**
 * @file rhi_types.h
 * @brief RHI 层公共类型定义，集中管理所有渲染相关的数据描述结构体
 *
 * 包含：
 * - RHI 无关枚举（混合因子/深度函数/剔除面等）
 * - 渲染目标描述 (RenderTargetDesc)
 * - 管线状态描述 (PipelineStateDesc)
 * - 渲染通道描述 (RenderPassDesc)
 * - 渲染目标回读 (RenderTargetReadback)
 * - 绘制项 (SpriteDrawItem / MeshDrawItem / Particle3DDrawItem)
 * - 顶点格式 (BatchVertex)
 * - 渲染统计 (RenderStats)
 */

#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <cstdint>
#include <string>

// ============================================================
// RHI 无关枚举 — 替代原 OpenGL 硬编码常量
// ============================================================

/// RHI 后端类型
enum class RhiBackend : unsigned int {
    OpenGL = 0,
    Vulkan = 1,
    D3D11  = 2,
    Default = OpenGL  ///< 默认使用 OpenGL 后端
};

/// 混合因子
enum class BlendFactor : unsigned int {
    Zero = 0,
    One = 1,
    SrcAlpha = 2,
    OneMinusSrcAlpha = 3,
    DstAlpha = 4,
    OneMinusDstAlpha = 5,
    SrcColor = 6,
    OneMinusSrcColor = 7,
    DstColor = 8,
    OneMinusDstColor = 9,
};

/// 深度比较函数
enum class CompareFunc : unsigned int {
    Never = 0,
    Less = 1,
    Equal = 2,
    LessEqual = 3,
    Greater = 4,
    NotEqual = 5,
    GreaterEqual = 6,
    Always = 7,
};

/// 剔除面
enum class CullFace : unsigned int {
    None = 0,
    Front = 1,
    Back = 2,
    FrontAndBack = 3,
};

// ============================================================
// 渲染目标与管线状态描述
// ============================================================

/// 渲染目标描述符
struct RenderTargetDesc {
    int width = 0;
    int height = 0;
    bool has_color = true;
    bool has_depth = false;
    bool generate_mipmaps = false;  ///< Bloom Downsample 需要 mipmap
    bool cube_map = false;
    int msaa_samples = 1;           ///< MSAA 采样数（1 = 禁用，4 = 4x MSAA）
    bool allow_uav = false;         ///< 颜色附件是否支持 UAV（Compute Shader 写入）

    bool operator==(const RenderTargetDesc& o) const {
        return width == o.width && height == o.height &&
               has_color == o.has_color && has_depth == o.has_depth &&
               generate_mipmaps == o.generate_mipmaps && cube_map == o.cube_map &&
               msaa_samples == o.msaa_samples && allow_uav == o.allow_uav;
    }
};

/// 管线状态描述符（RHI 无关）
struct PipelineStateDesc {
    bool blend_enabled = true;
    BlendFactor blend_src = BlendFactor::SrcAlpha;
    BlendFactor blend_dst = BlendFactor::OneMinusSrcAlpha;
    bool depth_test_enabled = true;
    bool depth_write_enabled = true;
    CompareFunc depth_func = CompareFunc::Less;
    bool culling_enabled = true;
    CullFace cull_face = CullFace::Back;
};

/// 渲染通道描述符
struct RenderPassDesc {
    unsigned int render_target = 0;
    glm::vec4 clear_color = glm::vec4(0.0f);
    bool clear_color_enabled = false;
};

/// 渲染目标像素回读结果
struct RenderTargetReadback {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
};

// ============================================================
// 绘制项（Draw Item）
// ============================================================

/// 2D 精灵绘制项
struct SpriteDrawItem {
    unsigned int texture_handle = 0;
    unsigned int material_instance_id = 0;
    unsigned int shader_variant_key = 0;
    unsigned int blend_mode = 0;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
    glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
    int sorting_layer = 0;
    int order_in_layer = 0;
};

/// 批量渲染顶点格式
struct BatchVertex {
    glm::vec3 pos;
    glm::vec4 color;
    glm::vec2 uv;
    glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec4 weights = glm::vec4(0.0f);
    glm::vec4 joints = glm::vec4(0.0f);
};

/// 3D 网格绘制项
struct MeshDrawItem {
    unsigned int vao_override = 0;       ///< 非零时直接使用此 VAO（仅 GL 后端有效）
    unsigned int index_count_override = 0;

    unsigned int texture_handle = 0;
    unsigned int normal_map_handle = 0;
    unsigned int metallic_roughness_map_handle = 0;
    unsigned int emissive_map_handle = 0;
    unsigned int occlusion_map_handle = 0;
    unsigned int blend_mode = 0;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
    std::vector<BatchVertex> vertices;
    std::vector<unsigned short> indices;
    int sorting_layer = 0;
    int order_in_layer = 0;

    bool lighting_enabled = false;
    glm::vec3 material_albedo = glm::vec3(1.0f);
    float material_metallic = 0.0f;
    float material_roughness = 1.0f;
    float material_ao = 1.0f;
    glm::vec3 material_emissive = glm::vec3(0.0f);
    float material_normal_strength = 1.0f;
    float material_alpha_cutoff = 0.5f;
    bool material_alpha_test = false;
    bool material_double_sided = false;
    bool material_uses_instance_data = false;
    bool receive_shadow = true;
    bool depth_test_enabled = true;
    bool depth_write_enabled = true;

    std::string debug_label;
    glm::vec3 debug_world_bounds_min = glm::vec3(0.0f);
    glm::vec3 debug_world_bounds_max = glm::vec3(0.0f);

    glm::vec3 light_direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color = glm::vec3(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.2f;
    float shadow_strength = 0.5f;

    /// 点光源数据
    struct PointLightData {
        glm::vec3 color;
        glm::vec3 position;
        float intensity;
        float radius;
        bool cast_shadow = false;
        int shadow_index = -1;
    };
    std::vector<PointLightData> point_lights;

    /// 聚光灯数据
    struct SpotLightData {
        glm::vec3 color;
        glm::vec3 position;
        glm::vec3 direction;
        float intensity;
        float radius;
        float inner_cone;
        float outer_cone;
        bool cast_shadow = false;
        int shadow_index = -1;
    };
    std::vector<SpotLightData> spot_lights;

    bool skinned = false;
    std::vector<glm::mat4> bone_matrices;

    bool morph_enabled = false;
    std::vector<float> morph_weights;
};

/// 3D 粒子绘制项
struct Particle3DDrawItem {
    unsigned int texture_handle = 0;
    unsigned int material_instance_id = 0;
    unsigned int shader_variant_key = 0;
    unsigned int blend_mode = 0;
    int particle_count = 0;
    unsigned int instance_vbo = 0;       ///< 实例变换/颜色 VBO
};

// ============================================================
// 渲染统计与常量
// ============================================================

#define CSM_CASCADES 3

using DrawBatchItem = SpriteDrawItem;

/// 渲染统计信息
struct RenderStats {
    int sprite_count = 0;
    int mesh_count = 0;
    int draw_calls = 0;
    int material_switches = 0;
    int max_batch_sprites = 0;
    int render_passes = 0;
    int shadow_passes = 0;
    int particle_count = 0;  ///< 3D 粒子数量
};
