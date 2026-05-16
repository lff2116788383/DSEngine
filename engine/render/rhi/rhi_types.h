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

/// 压缩纹理格式（BCn / DXT 系列）
enum class CompressedTextureFormat : unsigned int {
    BC1_UNORM = 0,   ///< DXT1 (RGB, 4bpp)
    BC1_SRGB  = 1,   ///< DXT1 sRGB
    BC2_UNORM = 2,   ///< DXT3 (RGBA, 8bpp)
    BC3_UNORM = 3,   ///< DXT5 (RGBA, 8bpp)
    BC3_SRGB  = 4,   ///< DXT5 sRGB
    BC4_UNORM = 5,   ///< 单通道 (4bpp)
    BC5_UNORM = 6,   ///< 双通道 (8bpp, 法线贴图)
    BC7_UNORM = 7,   ///< 高质量 RGBA (8bpp)
    BC7_SRGB  = 8,   ///< 高质量 RGBA sRGB
};

/// 压缩纹理 Mip 级别数据
struct CompressedMipLevel {
    const unsigned char* data = nullptr;
    size_t size = 0;
    int width = 0;
    int height = 0;
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
    int color_attachment_count = 1; ///< MRT 颜色附件数量 (1~8)，has_color=true 时生效

    bool operator==(const RenderTargetDesc& o) const {
        return width == o.width && height == o.height &&
               has_color == o.has_color && has_depth == o.has_depth &&
               generate_mipmaps == o.generate_mipmaps && cube_map == o.cube_map &&
               msaa_samples == o.msaa_samples && allow_uav == o.allow_uav &&
               color_attachment_count == o.color_attachment_count;
    }
};

/// 管线状态描述符（RHI 无关）
struct PipelineStateDesc {
    bool blend_enabled = true;
    BlendFactor blend_src = BlendFactor::SrcAlpha;
    BlendFactor blend_dst = BlendFactor::OneMinusSrcAlpha;
    BlendFactor alpha_blend_src = BlendFactor::One;               ///< Alpha 通道源因子
    BlendFactor alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;  ///< Alpha 通道目标因子
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
    unsigned int ebo_override = 0;       ///< 非零时绑定此 EBO 覆盖 VAO 默认的 element buffer
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
    int shading_mode = 0;  ///< 0=PBR, 2=HalfLambert-Skin, 3=HalfLambert-Static, 4=Toon/Cel, 5=Watercolor
    glm::vec3 material_albedo = glm::vec3(1.0f);
    float material_metallic = 0.0f;
    float material_roughness = 1.0f;
    float material_ao = 1.0f;
    glm::vec3 material_emissive = glm::vec3(0.0f);
    float material_normal_strength = 1.0f;
    float material_alpha_cutoff = 0.5f;
    bool material_alpha_test = false;
    bool material_double_sided = false;
    float material_sss_strength = 0.0f;
    glm::vec3 material_sss_tint = glm::vec3(0.0f);  ///< SSS 散射色调，(0,0,0) 使用默认皮肤色
    float material_clear_coat = 0.0f;
    float material_clear_coat_roughness = 0.1f;
    float material_anisotropy = 0.0f;
    float material_pom_height_scale = 0.0f;
    glm::vec3 toon_shadow_color = glm::vec3(0.15f, 0.1f, 0.18f);
    float toon_shadow_threshold = 0.35f;
    float toon_shadow_softness = 0.05f;
    float toon_specular_size = 0.6f;
    float toon_specular_strength = 0.8f;
    float toon_rim_strength = 0.3f;
    float watercolor_paper_strength = 0.3f;
    float watercolor_edge_darkening = 0.4f;
    float watercolor_color_bleed = 0.2f;
    float watercolor_pigment_density = 1.0f;
    bool material_uses_instance_data = false;
    bool receive_shadow = true;
    bool depth_test_enabled = true;
    bool depth_write_enabled = true;

    // Terrain splatmap
    bool splat_enabled = false;
    unsigned int splat_weight_map_handle = 0;
    unsigned int splat_layer_handles[4] = {0, 0, 0, 0};
    glm::vec4 splat_tiling = glm::vec4(10.0f); ///< per-layer UV tiling

    int wboit_mode = 0;  ///< 0=normal, 1=WBOIT accumulation, 2=WBOIT revealage

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

    /// GPU Instancing: 逐实例模型矩阵（非空时触发 instanced draw）
    std::vector<glm::mat4> instance_transforms;
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

/// 毛发绘制项 (per-instance)
struct HairDrawItem {
    unsigned int position_ssbo = 0;     ///< 当前帧位置 SSBO (vec4[N])
    unsigned int tangent_ssbo = 0;      ///< 切线 SSBO (vec4[N])
    uint32_t total_vertex_count = 0;
    uint32_t strand_count = 0;
    const int* strand_firsts = nullptr; ///< per-strand 起始索引 (CPU array)
    const int* strand_counts = nullptr; ///< per-strand 顶点数 (CPU array)
    glm::mat4 world_transform = glm::mat4(1.0f);

    glm::vec4 root_color = glm::vec4(0.1f, 0.05f, 0.02f, 1.0f);
    glm::vec4 tip_color  = glm::vec4(0.4f, 0.25f, 0.15f, 1.0f);
    float fiber_radius = 0.04f;
    float opacity = 0.9f;
    float specular_primary = 80.0f;
    float specular_secondary = 20.0f;
    float specular_strength_primary = 0.6f;
    float specular_strength_secondary = 0.3f;
    glm::vec3 specular_color = glm::vec3(1.0f, 0.9f, 0.8f);

    glm::vec3 light_direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color = glm::vec3(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.2f;
};

// ============================================================
// GPU Driven Indirect Draw
// ============================================================

/// 对应 GL DrawElementsIndirectCommand / VkDrawIndexedIndirectCommand
struct DrawElementsIndirectCommand {
    uint32_t count;          ///< index count
    uint32_t instance_count; ///< GPU cull 写 0 表示 culled
    uint32_t first_index;    ///< mega IBO 中的起始 index 偏移
    int32_t  base_vertex;    ///< mega VBO 中的基础顶点偏移
    uint32_t base_instance;  ///< 用于索引 instance data
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
    int instanced_draw_calls = 0;  ///< GPU Instancing draw call 数
    int instanced_mesh_count = 0;  ///< GPU Instancing 合批的实体总数
    int indirect_draw_calls = 0;   ///< GPU Driven indirect draw call 数
    int gpu_culled_count = 0;      ///< GPU 剔除的对象数
};
