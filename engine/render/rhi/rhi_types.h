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
 * - 绘制项 (SpriteDrawItem / MeshDrawItem / HairDrawItem)
 * - 顶点格式 (BatchVertex)
 * - 渲染统计 (RenderStats)
 */

#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <cstdint>
#include "engine/render/rhi/rhi_handle.h"
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

/// 图元拓扑（管线状态的一部分）。默认三角形列表，发线状几何（如毛发）用线带。
/// DX11→D3D11_PRIMITIVE_TOPOLOGY_* / GL→glDraw* mode / Vulkan→VkPrimitiveTopology。
enum class PrimitiveTopology : unsigned int {
    TriangleList  = 0,   ///< 三角形列表（默认）
    LineStrip     = 1,   ///< 线带（毛发逐 strand 绘制）
    LineList      = 2,   ///< 线段列表
    PointList     = 3,   ///< 点列表
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

/// 纹理采样过滤模式
enum class TextureFilter : uint8_t {
    Nearest = 0,   ///< 点采样：像素画/像素完美渲染，缩放时保持硬边
    Linear  = 1,   ///< 线性插值：平滑缩放（默认）
};

/// 纹理环绕(wrap)模式
enum class TextureWrap : uint8_t {
    Repeat      = 0,   ///< 平铺重复（3D 可平铺贴图所需，默认以向后兼容）
    ClampToEdge = 1,   ///< 边缘钳制：图集/精灵防出血(bleeding)的正确选择
};

/// 纹理采样描述：过滤 + 环绕。默认 {Linear, Repeat} 保持旧行为不变。
struct TextureSamplerDesc {
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap   wrap   = TextureWrap::Repeat;

    /// 由旧的 bool linear_filter 构造（环绕仍为 Repeat）
    static TextureSamplerDesc FromLinearFlag(bool linear_filter) {
        return TextureSamplerDesc{ linear_filter ? TextureFilter::Linear : TextureFilter::Nearest,
                                   TextureWrap::Repeat };
    }
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
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;  ///< 图元拓扑（毛发用 LineStrip）
    bool wireframe = false;  ///< 线框填充模式（编辑器视图模式；DX11 D3D11_FILL_WIREFRAME / GL glPolygonMode(LINE) / Vulkan VK_POLYGON_MODE_LINE）
};

/// 顶点属性描述（通用绘制原语 BindVertexBuffer 用，目前仅支持 float 分量）
struct VertexAttr {
    uint32_t location = 0;    ///< 着色器中的 attribute location
    uint32_t components = 0;  ///< 分量数 (1..4)，对应 float/vec2/vec3/vec4
    uint32_t offset = 0;      ///< 在顶点结构体内的字节偏移
};

// ============================================================
// 通用绘制原语枚举 (B0 — 见 docs/architecture/RHI_PRIMITIVE_CONTRACT.md)
// ============================================================

/// 索引缓冲元素类型
enum class IndexType : uint8_t {
    UInt16 = 0,
    UInt32 = 1,
};

/// 纹理维度（BindTexture 用，归并 cubemap / 2D / 2D 数组）
enum class TextureDim : uint8_t {
    Tex2D      = 0,
    TexCube    = 1,
    Tex2DArray = 2,
    Tex3D      = 3,   ///< 3D 体纹理（如 Color Grading LUT），GL 用 GL_TEXTURE_3D 绑定
};

/// 引擎内建着色器程序标识（由各后端懒初始化预编译着色器，经 RhiDevice::GetBuiltinProgram 暴露句柄）。
/// 取代「每效果一个 GetXxxShaderProgram 访问器」，避免设备接口随效果线性膨胀。
enum class BuiltinProgram : uint8_t {
    Skybox      = 0,  ///< 天空盒（cubemap 采样）
    Sprite2D    = 1,  ///< 2D sprite（pos\@0/color\@1/uv\@2 + PerFrame UBO + u_texture）
    SpriteFxSdf = 2,  ///< SDF 文本批（sprite_fx.vert + sprite_fx_sdf.frag + SpriteFx push-block UBO）
    SpriteFxVfx = 3,  ///< UI 特效批（sprite_fx.vert + sprite_fx_vfx.frag + SpriteFx push-block UBO）
    ForwardPbr  = 4,  ///< 静态 forward PBR（forward_pbr.vert/.frag + PerFrame/PerScene/PerMaterial UBO + 5 纹理槽）
    ForwardPbrSkinned = 5,  ///< 蒙皮 forward PBR（forward_pbr_skinned.vert + forward_pbr.frag + 骨骼矩阵 SSBO\@set3.b0）
    ForwardPbrInstanced = 6,  ///< 硬件实例化 forward PBR（forward_pbr_instanced.vert + forward_pbr.frag + 每实例 model SSBO\@set3.b0）
    ForwardPbrDepth = 7,  ///< 仅深度 pass（forward_pbr.vert + shadow.frag 空片元；只写深度、不输出颜色，配 has_color=false RT）
    ForwardShaded = 8,  ///< 高级 shading forward（forward_pbr.vert + forward_shaded.frag；shading_mode 0/2-6 + SSS/clearcoat/anisotropy/POM/alpha-test/double-sided + 扩展 PerMaterial UBO）
    ForwardSkinnedShaded = 9,  ///< 蒙皮 + 高级 shading 组合（forward_shaded_skinned.vert + forward_shaded.frag；骨骼矩阵 SSBO\@set7.b0 避开 frag set0-6）
    ForwardInstancedShaded = 10,  ///< 硬件实例化 + 高级 shading 组合（forward_shaded_instanced.vert + forward_shaded.frag；每实例 model SSBO\@set7.b0 避开 frag set0-6）
    ForwardMorphShaded = 11,  ///< Morph target + 高级 shading 组合（forward_shaded_morph.vert + forward_shaded.frag；morph 增量 SSBO\@set7.b0 + 权重 UBO\@set7.b3）
    ForwardInstancedDepth = 12,  ///< 实例化仅深度 pass（forward_shaded_instanced.vert + shadow.frag 空片元；每实例 model SSBO\@set7.b0 + 植被风；只写深度、不输出颜色，配 has_color=false RT）
    Particle3D = 13,  ///< 3D 粒子广告牌（particle_instanced.vert + particle.frag；每实例 pos/size/color SSBO\@set7.b0 + u_texture\@set2.b1；加性混合、不写深度，配 ParticleRenderer）
    HairStrand = 14,  ///< 毛发线带（hair.vert + hair.frag；position/tangent SSBO\@set7.b0/b1 + 组合 HairUniforms UBO\@set0.b0；LINE_STRIP 拓扑、逐 strand 绘制，配 HairRenderer）
    ForwardSkinnedInstancedShaded = 15,  ///< 蒙皮 + 硬件实例化 + 高级 shading 组合（forward_shaded_skinned_instanced.vert + forward_shaded.frag；实例 SSBO\@set8.b0 + 骨骼 SSBO\@set8.b1；bone-palette 去重，配 MeshRenderer::DrawSkinnedInstancedShaded）
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

/// 渲染目标深度回读结果（归一化深度 [0,1]，行优先；空 depth 表示该后端不支持或无深度附件）
struct RenderTargetDepthReadback {
    int width = 0;
    int height = 0;
    std::vector<float> depth;
};

// ============================================================
// 绘制项（Draw Item）
// ============================================================

/// 2D 精灵绘制项
struct SpriteVisualEffect {
    bool enabled = false;
    float corner_radius = 0.0f;
    float gradient_direction = 1.0f;
    float blur_radius = 0.0f;
    float blur_intensity = 1.0f;
    glm::vec2 rect_size = glm::vec2(0.0f);
    glm::vec4 gradient_start = glm::vec4(1.0f);
    glm::vec4 gradient_end = glm::vec4(1.0f);
};

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
    SpriteVisualEffect visual_effect;
    // SDF 文本着色器参数（仅 shader_variant_key == TEXT_SDF 时生效）
    float sdf_threshold = 0.5f;
    float sdf_smoothing = 0.1f;
    float sdf_outline_width = 0.0f;
    float sdf_shadow_softness = 0.0f;
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
    dse::render::VertexArrayHandle vao_override;       ///< 有效时直接使用此 VAO（仅 GL 后端有效）
    dse::render::BufferHandle ebo_override;       ///< 有效时绑定此 EBO 覆盖 VAO 默认的 element buffer
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
    std::vector<uint32_t> indices;
    int sorting_layer = 0;
    int order_in_layer = 0;

    bool lighting_enabled = false;
    int shading_mode = 0;  ///< 0=PBR, 2=HalfLambert-Skin, 3=HalfLambert-Static, 4=Toon/Cel, 5=Watercolor, 6=FaceSDF
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

    // Snow cover
    float snow_coverage = 0.0f;                        ///< 积雪覆盖率 [0,1]
    glm::vec3 snow_albedo = glm::vec3(0.92f, 0.93f, 0.96f); ///< 雪面反照率
    float snow_roughness = 0.75f;                      ///< 雪面粗糙度
    float snow_normal_threshold = 0.4f;                ///< N.y 阈值
    float snow_edge_sharpness = 3.0f;                  ///< 边缘锐利度

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

    bool foliage = false;  ///< 植被风弯曲标记

    /// GPU Instancing: 逐实例模型矩阵（非空时触发 instanced draw）
    std::vector<glm::mat4> instance_transforms;

    /// Skinned Instancing: 逐实例骨骼矩阵（与 instance_transforms 平行）
    /// 非空时 draw executor 会为每个实例计算独立的 bone_offset
    std::vector<std::vector<glm::mat4>> per_instance_bones;

    /// Bone Palette Instancing (优先于 per_instance_bones):
    /// bone_palette 存储唯一的骨骼矩阵集（去重后通常只有 2-60 个），
    /// instance_bone_palette_idx 存储每实例引用的 palette 索引。
    /// 10000 实例 × 2 动画 → bone_palette 仅 2 条目 vs per_instance_bones 10000 条目。
    std::vector<std::vector<glm::mat4>> bone_palette;
    std::vector<int> instance_bone_palette_idx;

    /// Shared mesh template: 当非 null 时，执行器优先使用此数据替代 vertices/indices，
    /// 并在同 pass 内检测连续相同指针以跳过重复 VBO 上传。
    const BatchVertex* shared_vertex_ptr = nullptr;
    const uint32_t* shared_index_ptr = nullptr;
    uint32_t shared_vertex_count = 0;
    uint32_t shared_index_count = 0;
};

/// 毛发绘制项 (per-instance)
struct HairDrawItem {
    dse::render::BufferHandle position_ssbo;  ///< 当前帧位置 SSBO (vec4[N])
    dse::render::BufferHandle tangent_ssbo;   ///< 切线 SSBO (vec4[N])
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
// Compute 调度原语 (B-compute)
// ============================================================

/// CommandBuffer 级通用 compute 调度描述（取代把 bloom mip 等 compute 效果
/// 藏在 DrawPostProcess 里靠 effect_name 检测的旧做法）。
/// 语义：以 source_texture 作输入 SRV（combined image sampler binding 0），
/// 当前 BeginRenderPass 绑定的 RT 颜色附件作输出 UAV/storage image，跑一次 compute；
/// 线程组数与 texel 尺寸由后端按 RT/源纹理尺寸推导（8×8 tile），与旧 bloom CS 路径一致。
/// blend_weight 供 upsample 等需要混合权重的 CS 经参数块读取。
/// 仅 compute 后端（DX11 FL11+/Vulkan）消费；不支持 compute 的后端（GL）由高层渲染器
/// 经 GetBloomComputeShader()==0 回退全屏 quad，不会调到该原语，故默认空实现即可。
struct ComputeDispatch {
    unsigned int shader = 0;          ///< 设备级 compute shader handle
    unsigned int source_texture = 0;  ///< 输入 SRV（0 表示无输入）
    float blend_weight = 1.0f;        ///< 混合权重（upsample 累加用，downsample 取 1.0）
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


/// 渲染统计信息
struct RenderStats {
    int sprite_count = 0;
    int mesh_count = 0;
    int draw_calls = 0;
    int triangle_count = 0;        ///< 本帧提交的三角形总数
    int material_switches = 0;
    int max_batch_sprites = 0;
    int render_passes = 0;
    int shadow_passes = 0;
    int particle_count = 0;  ///< 3D 粒子数量
    int instanced_draw_calls = 0;  ///< GPU Instancing draw call 数
    int instanced_mesh_count = 0;  ///< GPU Instancing 合批的实体总数
    int indirect_draw_calls = 0;   ///< GPU Driven indirect draw call 数
    int gpu_culled_count = 0;      ///< GPU 剔除的对象数
    bool gpu_driven_supported = false;            ///< 本帧 RHI 能力是否支持 GPU Driven
    bool gpu_driven_requested = false;            ///< 本帧策略是否请求 GPU Driven
    bool gpu_driven_scene_prepared = false;       ///< 本帧 GPU Scene 是否已准备
    bool gpu_driven_active_this_frame = false;    ///< 本帧 GPU Driven 是否真实参与渲染
    int gpu_indirect_draw_count = 0;              ///< 本帧 GPU Driven draw command 数
    int gpu_total_instances = 0;                  ///< 本帧 GPU Driven instance 数
    int sorted_draw_calls = 0;     ///< 排序后提交给后端的实际 DrawCall 数
    int material_batch_count = 0;  ///< 排序后相邻同材质批次组数（越少表示合批率越高）
};

// ============================================================
// 材质排序键
// ============================================================

// ============================================================
// 资源状态（RenderGraph 自动屏障 + RHI 状态转换）
// ============================================================

/// 渲染资源状态枚举
enum class ResourceState : uint8_t {
    Undefined,          ///< 未知/初始状态
    RenderTarget,       ///< 作为颜色渲染目标写入
    DepthWrite,         ///< 作为深度/模板渲染目标写入
    DepthRead,          ///< 作为只读深度附件（深度测试但不写入）
    ShaderRead,         ///< 作为纹理/UBO 被着色器读取
    UnorderedAccess,    ///< 作为 UAV/SSBO 被 Compute Shader 读写
    CopySource,         ///< 作为拷贝/Blit 源
    CopyDest,           ///< 作为拷贝/Blit 目标
    Present             ///< 用于呈现到屏幕
};

/// 生成 MeshDrawItem 的 64 位排序键，用于 std::sort 减少 GPU 状态切换
/// 优先级(高→低): blend_mode > shading_mode > texture_handle > normal_map_handle
inline uint64_t MakeSortKey(const MeshDrawItem& item) noexcept {
    uint64_t key = 0;
    key |= (static_cast<uint64_t>(item.blend_mode   & 0xFu)        << 60u);
    key |= (static_cast<uint64_t>(item.shading_mode & 0xFFu)       << 52u);
    key |= (static_cast<uint64_t>(item.texture_handle)              << 20u);
    key |= (static_cast<uint64_t>(item.normal_map_handle & 0xFFFFFu));
    return key;
}
