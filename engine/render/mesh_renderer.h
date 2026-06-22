/**
 * @file mesh_renderer.h
 * @brief 后端无关的静态 forward PBR 网格渲染器（B2b-1）。
 *
 * 用 B0/P0 的通用绘制原语（BindPipeline / BindUniformBuffer /
 * BindTexture(2D) / BindVertexBuffer / BindIndexBuffer / DrawIndexed）组合出「带材质的
 * 单方向光 PBR 网格」绘制，端到端验证 mesh 类消费者所需的原语在三后端上都正确。
 *
 * 复用内建程序 BuiltinProgram::ForwardPbr（forward_pbr.vert/.frag，真实 Cook-Torrance +
 * 5 纹理槽 + PerFrame/PerScene/PerMaterial UBO）。
 *
 * 关键约定：顶点在 CPU 侧预变换到世界空间（pos/normal/tangent），VS 仅施加 vp，
 * 避免依赖各后端不一致的 push-constant/model 语义（与既有批渲染世界空间约定一致）。
 *
 * 注意：这是逐特性搭建的第一步（静态 PBR forward），不替代生产 DrawMeshBatch；
 * 蒙皮 / 硬件实例化 / shadow / GPU-driven 为后续步骤。
 */

#ifndef DSE_RENDER_MESH_RENDERER_H
#define DSE_RENDER_MESH_RENDERER_H

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "engine/render/rhi/rhi_handle.h"
#include "engine/render/rhi/rhi_types.h"  // IndexType（ExternalShadedMesh 用）

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/// forward PBR 顶点（局部空间输入；MeshRenderer 内部按 model 预变换到世界空间）。
/// 内存布局须与 forward_pbr.vert 输入一致：pos\@0 / color\@1 / uv\@2 / normal\@3 / tangent\@4。
struct MeshVertex {
    glm::vec3 position{0.0f};
    glm::vec4 color{1.0f};
    glm::vec2 uv{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 tangent{1.0f, 0.0f, 0.0f};
};

/// 无光照 2D 顶点（B2b-6：spine 2D 蒙皮迁移）。位置须为**世界空间**（spine runtime 已用
/// computeWorldVertices 做完 2D 蒙皮），内存布局须与 BuiltinProgram::Sprite2D 输入一致：
/// pos\@0(vec3) / color\@1(vec4) / uv\@2(vec2)，紧凑 36 字节。
struct Unlit2DVertex {
    glm::vec3 position{0.0f};  ///< 世界空间位置（pos.z 通常为 0）
    glm::vec4 color{1.0f};     ///< 顶点色（与纹理相乘）
    glm::vec2 uv{0.0f};        ///< 纹理坐标
};

/// 蒙皮 forward PBR 顶点（局部/绑定空间输入；VS 按 bone index/weight 混合后施 vp）。
/// 内存布局须与 forward_pbr_skinned.vert 输入一致：
/// pos\@0 / color\@1 / uv\@2 / normal\@3 / tangent\@4 / boneIndices\@5 / boneWeights\@6。
struct SkinnedMeshVertex {
    glm::vec3 position{0.0f};
    glm::vec4 color{1.0f};
    glm::vec2 uv{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 tangent{1.0f, 0.0f, 0.0f};
    glm::vec4 bone_indices{0.0f};               ///< 4 骨骼索引（float 承载）
    glm::vec4 bone_weights{1.0f, 0.0f, 0.0f, 0.0f}; ///< 4 骨骼权重（和应为 1）
};

/// PBR 材质参数 + 5 纹理槽（句柄 0 表示缺省，回退到内建 1x1 白纹理并关闭对应贴图采样）。
struct MeshMaterial {
    glm::vec3 albedo{1.0f};       ///< 基础色（与纹理、顶点色相乘）
    float metallic = 0.0f;        ///< 金属度 [0,1]
    float roughness = 0.5f;       ///< 粗糙度 [0,1]
    float ao = 1.0f;              ///< 环境光遮蔽常数
    float normal_strength = 1.0f; ///< 法线贴图强度
    glm::vec3 emissive{0.0f};     ///< 自发光颜色
    float alpha_cutoff = 0.5f;    ///< alpha test 阈值
    bool alpha_test = false;      ///< 开启 alpha test（< cutoff 丢弃）

    unsigned int albedo_tex = 0;              ///< u_texture
    unsigned int normal_tex = 0;              ///< u_normal_map
    unsigned int metallic_roughness_tex = 0;  ///< u_metallic_roughness_map
    unsigned int emissive_tex = 0;            ///< u_emissive_map
    unsigned int occlusion_tex = 0;           ///< u_occlusion_map
};

/// 高级 shading 材质（B2c-1）：在 MeshMaterial 基础上扩展 shading_mode 0/2-6 +
/// SSS / clearcoat / anisotropy / POM / alpha-test / double-sided。配 BuiltinProgram::ForwardShaded。
/// 纹理槽含义同 MeshMaterial；在 FaceSDF(6) 模式下 albedo_tex 槽存 SDF 灰度图。
struct ShadedMaterial {
    glm::vec3 albedo{1.0f};       ///< 基础色
    float metallic = 0.0f;        ///< 金属度 [0,1]
    float roughness = 0.5f;       ///< 粗糙度 [0,1]
    float ao = 1.0f;              ///< 环境光遮蔽常数
    float normal_strength = 1.0f; ///< 法线贴图强度
    glm::vec3 emissive{0.0f};     ///< 自发光颜色
    float alpha_cutoff = 0.5f;    ///< alpha test 阈值
    bool alpha_test = false;      ///< 开启 alpha test（< cutoff 丢弃）
    bool double_sided = false;    ///< 双面（关背面剔除 + 背面法线翻转）
    int shading_mode = 0;         ///< 0=PBR, 2=HalfLambert-Skin, 3=HalfLambert-Static, 4=Toon, 5=Watercolor, 6=FaceSDF

    float sss_strength = 0.0f;          ///< 次表面散射强度（0=关）
    glm::vec3 sss_tint{0.0f};           ///< SSS 色调（零向量=用默认肉色）
    float clear_coat = 0.0f;            ///< 清漆层强度（0=关）
    float clear_coat_roughness = 0.1f;  ///< 清漆层粗糙度
    float anisotropy = 0.0f;            ///< 各向异性 [-1,1]（0=各向同性）
    float pom_height_scale = 0.0f;      ///< 视差遮蔽高度缩放（0=关；需法线/高度贴图）

    glm::vec3 toon_shadow_color{0.15f, 0.1f, 0.18f}; ///< Toon/FaceSDF 阴影色
    float toon_shadow_threshold = 0.35f;   ///< Toon 阴影阈值
    float toon_shadow_softness = 0.05f;    ///< Toon/FaceSDF 边缘柔度
    float toon_specular_size = 0.6f;       ///< Toon 高光尺寸阈值
    float toon_specular_strength = 0.8f;   ///< Toon 高光强度
    float toon_rim_strength = 0.3f;        ///< Toon/FaceSDF 边缘光强度

    float watercolor_paper_strength = 0.3f;  ///< 水彩纸张颗粒
    float watercolor_edge_darkening = 0.4f;  ///< 水彩边缘加深
    float watercolor_color_bleed = 0.2f;     ///< 水彩色彩渗透
    float watercolor_pigment_density = 1.0f; ///< 水彩颜料浓度

    unsigned int albedo_tex = 0;              ///< u_texture（FaceSDF 模式为 SDF 图）
    unsigned int normal_tex = 0;              ///< u_normal_map（.a = POM 高度）
    unsigned int metallic_roughness_tex = 0;  ///< u_metallic_roughness_map
    unsigned int emissive_tex = 0;            ///< u_emissive_map
    unsigned int occlusion_tex = 0;           ///< u_occlusion_map

    // 地形 splatmap（B2c-3）。splat_enabled 时 albedo 由 4 层权重混合取代。
    bool splat_enabled = false;                       ///< 开启 splatmap 4 层混合
    unsigned int splat_weight_map = 0;                ///< 权重图（rgba = 4 层权重）
    unsigned int splat_layers[4] = {0, 0, 0, 0};      ///< 4 个 layer albedo
    glm::vec4 splat_tiling{10.0f};                    ///< 每 layer UV tiling

    // 积雪（B2c-3）。snow_coverage>0 时朝上表面按阈值/锐利度混入雪面。
    float snow_coverage = 0.0f;                        ///< 积雪覆盖率 [0,1]（0=关）
    glm::vec3 snow_albedo{0.92f, 0.93f, 0.96f};        ///< 雪面反照率
    float snow_roughness = 0.75f;                      ///< 雪面粗糙度
    float snow_normal_threshold = 0.4f;                ///< N.y 阈值
    float snow_edge_sharpness = 3.0f;                  ///< 边缘锐利度（pow 指数）

    // 透明 WBOIT（B2c-4）。0=不透明直写；1=accumulation 通道（加性混合，深度不写）；
    // 2=revealage 通道（ZERO/ONE_MINUS_SRC_ALPHA 乘性混合，深度不写）。
    int wboit_mode = 0;

    // CSM 方向光阴影（Final-Feat-1）。receive_shadow 开启后 DrawShaded 从 device 全局渲染状态
    // （light_space_matrix / cascade_splits / shadow_atlas_region / shadow_map[0]）取 CSM 数据采样。
    bool receive_shadow = false;        ///< 接收方向光 CSM 阴影（默认关，不回归既有调用）
    float shadow_strength = 1.0f;       ///< 阴影强度 [0,1]

    // 植被风弯曲（B2b-6，tree/grass 迁移）。开启后 DrawInstancedShaded / DrawSharedTemplateInstanced
    // 从 device 全局渲染状态取 foliage_wind/foliage_push 喂入 forward_shaded_instanced.vert 施风；
    // 默认关 → 喂零 wind，VS 整段跳过，不回归既有非植被实例化调用。
    bool foliage = false;               ///< 顶点风弯曲（tree/grass）
};

/// 单方向光。
struct DirectionalLight {
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; ///< 光线传播方向（从光源射向场景）
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float ambient = 0.03f; ///< 常数环境项系数
    bool enabled = true;
};

/// clustered 点光（B2c-2）。布局对应 ubo_types.h PointLightEntry（≤64，UBO fallback）。
/// 平方反比半径衰减；shadow 字段保留给后续点光阴影步骤。
struct ShadedPointLight {
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    glm::vec3 position{0.0f};
    float radius = 10.0f;
    bool cast_shadow = false;
    int shadow_index = -1;
};

/// 聚光灯（Final-Feat-4）。布局对应 ubo_types.h SpotLightEntry（≤64，UBO fallback）。
/// 平方反比半径衰减 + 内/外锥半角平滑过渡；shadow 字段保留给后续聚光灯阴影步骤。
struct ShadedSpotLight {
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    glm::vec3 position{0.0f};
    float radius = 10.0f;
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; ///< 光线传播方向（光源→场景）
    float inner_cone = 12.5f;               ///< 内锥半角（度）：theta>innerCos 全亮
    float outer_cone = 17.5f;               ///< 外锥半角（度）：theta<outerCos 全暗
    bool cast_shadow = false;
    int shadow_index = -1;
};

/// 全局光照（B2c-5）。SH L2 间接漫反射（LightProbe）+ DDGI irradiance atlas 探针体。
/// 两者皆关（默认）时退化为 DirectionalLight::ambient 平坦环境光，与 B2c-4 输出一致。
struct ShadedGI {
    // LightProbe SH（L2，9 系数；仅 xyz 有效）。
    bool sh_enabled = false;
    glm::vec4 sh_coefficients[9] = {};

    // DDGI 探针体。irradiance_atlas=0 或 ddgi_enabled=false 时不启用。
    bool ddgi_enabled = false;
    unsigned int ddgi_irradiance_atlas = 0;
    glm::vec3 ddgi_grid_origin{0.0f};
    glm::vec3 ddgi_grid_spacing{1.0f};
    glm::ivec3 ddgi_grid_resolution{0};
    int ddgi_irradiance_texels = 8;       ///< 每探针 octahedral 分辨率（含 1px 边界）
    float ddgi_gi_intensity = 1.0f;
    float ddgi_normal_bias = 0.2f;
};

/// Morph target（形变目标，Final-Feat-5）。每个 target 提供与基网格顶点一一对应的
/// 位置/法线增量（局部空间，内部按 model 预变换：位置用 model 线性部分、法线用法线矩阵）。
/// 最终顶点 = 基顶点 + Σ weight_i * delta_i，在 VS 内按 gl_VertexIndex 取增量加权求和。
struct MeshMorphTarget {
    std::vector<glm::vec3> position_deltas;  ///< 顶点位置增量（局部空间），size 须 == 基网格顶点数
    std::vector<glm::vec3> normal_deltas;    ///< 顶点法线增量（局部空间）；空 = 该 target 不形变法线
    float weight = 0.0f;                     ///< 该 target 当前权重
};

/// 外部常驻顶点/索引缓冲网格（Final-Feat-6）。顶点须为 ForwardShaded 兼容的 GpuMeshVertex
/// **世界空间**布局（用 MeshRenderer::BuildShadedWorldVertexBuffer 构建），索引为 16/32 位。
/// 缓冲由调用方持有/释放（device.DeleteGpuBuffer），不随 MeshRenderer 生命周期回收。
/// 多个 tile 可共享同一对 VB/IB，按 [first_index, first_index+index_count) 索引子段分别绘制
///（index_count_override）——这正是 tiled terrain 的「一份共享地形缓冲、每 tile 画各自索引段、
/// 零每帧顶点重传」用法。
struct ExternalShadedMesh {
    BufferHandle vertex_buffer;                 ///< GpuMeshVertex 世界空间顶点（调用方持有）
    BufferHandle index_buffer;                  ///< 索引缓冲（调用方持有）
    IndexType index_type = IndexType::UInt16;   ///< 索引元素类型（16/32 位）
};

/**
 * @class MeshRenderer
 * @brief 用通用原语绘制单个带材质的 PBR 网格。资源（PSO / UBO / 白纹理 / VB / IB）首帧懒创建，
 *        动态 VB/IB 按需扩容。
 */
class MeshRenderer {
public:
    /// 记录一次 PBR 网格绘制。
    /// @param vertices   局部空间顶点（内部按 model 预变换到世界空间）
    /// @param indices    16 位索引
    /// @param model      模型矩阵
    /// @param view/proj  相机视图 / 投影矩阵（vp = proj * view 下发到 PerFrame）
    /// @param camera_pos 世界空间相机位置（高光计算用）
    /// @param material   材质参数 + 纹理
    /// @param light      单方向光
    void Draw(CommandBuffer& cmd, RhiDevice& device,
              const std::vector<MeshVertex>& vertices,
              const std::vector<uint16_t>& indices,
              const glm::mat4& model,
              const glm::mat4& view,
              const glm::mat4& proj,
              const glm::vec3& camera_pos,
              const MeshMaterial& material,
              const DirectionalLight& light);

    /// 记录一次蒙皮 PBR 网格绘制（B2b-2）。顶点为局部/绑定空间，VS 按 bone index/weight
    /// 混合骨骼矩阵后施 vp（骨骼矩阵走 SSBO\@slot 0，复用静态 PBR frag）。
    /// @param vertices       局部/绑定空间顶点 + 骨骼索引/权重
    /// @param indices        16 位索引
    /// @param model          模型矩阵（内部预乘进每根骨骼矩阵）
    /// @param bone_matrices  绑定→局部空间的骨骼矩阵（内部左乘 model 得世界空间）
    /// @param view/proj      相机视图 / 投影矩阵（proj 须含 GetProjectionCorrection）
    /// @param camera_pos     世界空间相机位置
    /// @param material       材质参数 + 纹理
    /// @param light          单方向光
    void DrawSkinned(CommandBuffer& cmd, RhiDevice& device,
                     const std::vector<SkinnedMeshVertex>& vertices,
                     const std::vector<uint16_t>& indices,
                     const glm::mat4& model,
                     const std::vector<glm::mat4>& bone_matrices,
                     const glm::mat4& view,
                     const glm::mat4& proj,
                     const glm::vec3& camera_pos,
                     const MeshMaterial& material,
                     const DirectionalLight& light);

    /// 记录一次硬件实例化 PBR 网格绘制（B2b-3）。顶点为局部空间，每实例 model 矩阵走
    /// instance SSBO\@slot 0，VS 按 gl_InstanceIndex 取出后施 model + vp（复用静态 PBR frag）。
    /// 契约：DX11 SV_InstanceID 始终从 0 起，故内部恒以 DrawIndexedInstanced(first_instance=0)
    /// 配 0 基 SSBO 索引（RHI_PRIMITIVE_CONTRACT §6）。
    /// @param vertices         局部空间顶点（所有实例共享）
    /// @param indices          16 位索引
    /// @param instance_models  每实例 model 矩阵（世界空间），实例数 = size()
    /// @param view/proj        相机视图 / 投影矩阵（proj 须含 GetProjectionCorrection）
    /// @param camera_pos       世界空间相机位置
    /// @param material         材质参数 + 纹理（所有实例共享）
    /// @param light            单方向光
    void DrawInstanced(CommandBuffer& cmd, RhiDevice& device,
                       const std::vector<MeshVertex>& vertices,
                       const std::vector<uint16_t>& indices,
                       const std::vector<glm::mat4>& instance_models,
                       const glm::mat4& view,
                       const glm::mat4& proj,
                       const glm::vec3& camera_pos,
                       const MeshMaterial& material,
                       const DirectionalLight& light);

    /// 记录一次 GPU-driven 间接实例化 PBR 网格绘制（B2b-5）。语义同 DrawInstanced（每实例 model
    /// 走 instance SSBO\@slot 0，VS 按 gl_InstanceIndex 取出，复用 ForwardPbrInstanced），区别在于
    /// 绘制参数（index/instance count）来自 GPU 端 indirect buffer，经 CommandBuffer::DrawIndexedIndirect
    /// 发起——可由 compute 剔除/LOD pass 在 GPU 上回写 instance_count（写 0 即 culled）。
    /// 契约：DX11 SV_InstanceID 始终从 0 起，base_instance 偏移须经 SSBO 偏移表达（RHI_PRIMITIVE_CONTRACT §6），
    /// 故内部 indirect command 的 base_instance 恒 0、配 0 基 SSBO 索引。
    /// @param vertices         局部空间顶点（所有实例共享）
    /// @param indices          16 位索引
    /// @param instance_models  每实例 model 矩阵（世界空间），实例数 = size()
    /// @param view/proj        相机视图 / 投影矩阵（proj 须含 GetProjectionCorrection）
    /// @param camera_pos       世界空间相机位置
    /// @param material         材质参数 + 纹理（所有实例共享）
    /// @param light            单方向光
    void DrawIndirect(CommandBuffer& cmd, RhiDevice& device,
                      const std::vector<MeshVertex>& vertices,
                      const std::vector<uint16_t>& indices,
                      const std::vector<glm::mat4>& instance_models,
                      const glm::mat4& view,
                      const glm::mat4& proj,
                      const glm::vec3& camera_pos,
                      const MeshMaterial& material,
                      const DirectionalLight& light);

    /// 记录一次仅深度绘制（B2b-4 shadow / depth-only）。顶点在 CPU 侧预变换到世界空间，
    /// VS 仅施 vp（复用静态 forward_pbr.vert + 空 shadow.frag）；只写深度、不输出颜色，
    /// 须配 has_color=false / has_depth=true 的渲染目标。用于 shadow map / depth pre-pass。
    /// @param vertices  局部空间顶点（内部按 model 预变换到世界空间）
    /// @param indices   16 位索引
    /// @param model     模型矩阵
    /// @param view/proj 相机视图 / 投影矩阵（proj 须含 GetProjectionCorrection，否则深度方向错）
    void DrawDepthOnly(CommandBuffer& cmd, RhiDevice& device,
                       const std::vector<MeshVertex>& vertices,
                       const std::vector<uint16_t>& indices,
                       const glm::mat4& model,
                       const glm::mat4& view,
                       const glm::mat4& proj);

    /// 记录一次高级 shading 网格绘制（B2c-1）。语义同 Draw（顶点 CPU 预变换到世界空间，
    /// VS 仅施 vp），但复用 BuiltinProgram::ForwardShaded 支持 shading_mode 0/2-6 +
    /// SSS/clearcoat/anisotropy/POM/alpha-test/double-sided（单方向光，无 shadow map/点光）。
    /// @param material 高级 shading 材质参数 + 纹理
    /// @param light    单方向光
    /// @param point_lights clustered 点光（B2c-2，≤64；超出截断；空=仅方向光，输出与 B2c-1 一致）
    /// @param gi       全局光照（B2c-5；默认关 → 退化为平坦环境光，与 B2c-4 输出一致）
    void DrawShaded(CommandBuffer& cmd, RhiDevice& device,
                    const std::vector<MeshVertex>& vertices,
                    const std::vector<uint16_t>& indices,
                    const glm::mat4& model,
                    const glm::mat4& view,
                    const glm::mat4& proj,
                    const glm::vec3& camera_pos,
                    const ShadedMaterial& material,
                    const DirectionalLight& light,
                    const std::vector<ShadedPointLight>& point_lights = {},
                    const ShadedGI& gi = {},
                    const std::vector<ShadedSpotLight>& spot_lights = {});

    /// 记录一次蒙皮 + 高级 shading 网格绘制（Final-Feat-2）。融合 DrawSkinned 的骨骼
    /// 顶点装配（顶点局部/绑定空间，骨骼矩阵走 SSBO\@slot 0，VS 施骨骼混合 + vp）与
    /// DrawShaded 的高级 shading 片元（shading_mode 0/2-6 + SSS/clearcoat/anisotropy/POM/
    /// alpha-test/double-sided + clustered 点光 + 地形 splat/雪 + WBOIT + GI + CSM 阴影），
    /// 复用 BuiltinProgram::ForwardSkinnedShaded（forward_shaded_skinned.vert + forward_shaded.frag）。
    /// @param vertices       局部/绑定空间顶点 + 骨骼索引/权重
    /// @param indices        16 位索引
    /// @param model          模型矩阵（内部预乘进每根骨骼矩阵）
    /// @param bone_matrices  绑定→局部空间的骨骼矩阵（内部左乘 model 得世界空间）
    /// @param view/proj      相机视图 / 投影矩阵（proj 须含 GetProjectionCorrection）
    /// @param camera_pos     世界空间相机位置
    /// @param material       高级 shading 材质参数 + 纹理
    /// @param light          单方向光
    /// @param point_lights   clustered 点光（≤64；超出截断；空=仅方向光）
    /// @param gi             全局光照（默认关 → 退化为平坦环境光）
    void DrawSkinnedShaded(CommandBuffer& cmd, RhiDevice& device,
                           const std::vector<SkinnedMeshVertex>& vertices,
                           const std::vector<uint16_t>& indices,
                           const glm::mat4& model,
                           const std::vector<glm::mat4>& bone_matrices,
                           const glm::mat4& view,
                           const glm::mat4& proj,
                           const glm::vec3& camera_pos,
                           const ShadedMaterial& material,
                           const DirectionalLight& light,
                           const std::vector<ShadedPointLight>& point_lights = {},
                           const ShadedGI& gi = {},
                           const std::vector<ShadedSpotLight>& spot_lights = {});

    /// 记录一次硬件实例化 + 高级 shading 网格绘制（Final-Feat-3）。融合 DrawInstanced 的
    /// 每实例 model 矩阵 SSBO\@slot 0（按 gl_InstanceIndex 取，VS 施 model + vp）与 DrawShaded
    /// 的高级 shading 片元（shading_mode 0/2-6 + SSS/clearcoat/anisotropy/POM/alpha-test/
    /// double-sided + clustered 点光 + 地形 splat/雪 + WBOIT + GI + CSM 阴影），
    /// 复用 BuiltinProgram::ForwardInstancedShaded（forward_shaded_instanced.vert + forward_shaded.frag）。
    /// @param vertices        局部空间顶点（VS 按实例 model 变换，不在 CPU 预变换）
    /// @param indices         16 位索引
    /// @param instance_models 每实例 model 矩阵（世界空间，0 基索引；契约 first_instance=0）
    /// @param view/proj       相机视图 / 投影矩阵（proj 须含 GetProjectionCorrection）
    /// @param camera_pos      世界空间相机位置
    /// @param material        高级 shading 材质参数 + 纹理
    /// @param light           单方向光
    /// @param point_lights    clustered 点光（≤64；超出截断；空=仅方向光）
    /// @param gi              全局光照（默认关 → 退化为平坦环境光）
    void DrawInstancedShaded(CommandBuffer& cmd, RhiDevice& device,
                             const std::vector<MeshVertex>& vertices,
                             const std::vector<uint16_t>& indices,
                             const std::vector<glm::mat4>& instance_models,
                             const glm::mat4& view,
                             const glm::mat4& proj,
                             const glm::vec3& camera_pos,
                             const ShadedMaterial& material,
                             const DirectionalLight& light,
                             const std::vector<ShadedPointLight>& point_lights = {},
                             const ShadedGI& gi = {},
                             const std::vector<ShadedSpotLight>& spot_lights = {});

    /// 记录一次蒙皮 + 硬件实例化 + 高级 shading 网格绘制（阶段4-M1）。融合 DrawSkinnedShaded 的
    /// 骨骼顶点装配与 DrawInstancedShaded 的每实例 model SSBO，复用 BuiltinProgram::
    /// ForwardSkinnedInstancedShaded（forward_shaded_skinned_instanced.vert + forward_shaded.frag）。
    /// 骨骼调色板在 CPU 侧按 bone-palette 去重：多个实例可共享同一份调色板，密排进骨骼 SSBO\@slot1，
    /// 每实例的 instance SSBO\@slot0 条目记 {world model, bone_offset=该调色板在密排中的起始下标}。
    /// VS 先骨骼混合（绑定→局部空间，不预乘 model），再施每实例 model 到世界空间，最后 vp。
    /// material/light/point_lights/gi/spot_lights 语义与 DrawSkinnedShaded / DrawInstancedShaded 完全一致。
    /// @param vertices            局部/绑定空间顶点 + 骨骼索引/权重（所有实例共享）
    /// @param indices             16 位索引
    /// @param instance_models     每实例 world-space model 矩阵（0 基索引；契约 first_instance=0），实例数 = size()
    /// @param bone_palettes       骨骼调色板列表（每份 = 绑定→局部空间骨骼矩阵，未预乘 model；多实例可共享）
    /// @param instance_palette_idx 每实例引用的调色板下标（size 须 == instance_models.size()）
    /// @param view/proj           相机视图 / 投影矩阵（proj 须含 GetProjectionCorrection）
    /// @param camera_pos          世界空间相机位置
    /// @param material            高级 shading 材质参数 + 纹理
    /// @param light               单方向光
    /// @param point_lights        clustered 点光（≤64；超出截断；空=仅方向光）
    /// @param gi                  全局光照（默认关 → 退化为平坦环境光）
    /// @param spot_lights         聚光灯（≤64；超出截断；空=无聚光灯）
    void DrawSkinnedInstancedShaded(CommandBuffer& cmd, RhiDevice& device,
                                    const std::vector<SkinnedMeshVertex>& vertices,
                                    const std::vector<uint16_t>& indices,
                                    const std::vector<glm::mat4>& instance_models,
                                    const std::vector<std::vector<glm::mat4>>& bone_palettes,
                                    const std::vector<int>& instance_palette_idx,
                                    const glm::mat4& view,
                                    const glm::mat4& proj,
                                    const glm::vec3& camera_pos,
                                    const ShadedMaterial& material,
                                    const DirectionalLight& light,
                                    const std::vector<ShadedPointLight>& point_lights = {},
                                    const ShadedGI& gi = {},
                                    const std::vector<ShadedSpotLight>& spot_lights = {});

    /// 记录一次 Morph target + 高级 shading 网格绘制（Final-Feat-5）。基顶点在 CPU 侧预变换到世界空间
    /// （同 DrawShaded），每个 morph target 的位置/法线增量亦按 model 预变换为世界空间增量写入 morph
    /// 增量 SSBO\@slot 0（布局 [target*vertex_count+vertex]）；权重/计数写入 morph 权重 UBO\@slot 8；
    /// VS 按 gl_VertexIndex 加权求和后施 vp，复用 BuiltinProgram::ForwardMorphShaded
    ///（forward_shaded_morph.vert + forward_shaded.frag）与 DrawShaded 全套高级 shading 片元。
    /// morph_targets 为空时退化为静态 DrawShaded 输出（VS 形变循环不执行，不回归）。
    /// @param vertices       局部空间基顶点（内部按 model 预变换到世界空间）
    /// @param indices        16 位索引
    /// @param morph_targets  形变目标列表（≤64；每个的 deltas 须与 vertices 等长，否则该 target 跳过）
    /// @param model          模型矩阵（基顶点 + 增量均按此预变换）
    /// @param view/proj      相机视图 / 投影矩阵（proj 须含 GetProjectionCorrection）
    /// @param camera_pos     世界空间相机位置
    /// @param material       高级 shading 材质参数 + 纹理
    /// @param light          单方向光
    /// @param point_lights   clustered 点光（≤64；超出截断；空=仅方向光）
    /// @param gi             全局光照（默认关 → 退化为平坦环境光）
    /// @param spot_lights    聚光灯（≤64；超出截断；空=无聚光灯）
    void DrawMorphShaded(CommandBuffer& cmd, RhiDevice& device,
                         const std::vector<MeshVertex>& vertices,
                         const std::vector<uint16_t>& indices,
                         const std::vector<MeshMorphTarget>& morph_targets,
                         const glm::mat4& model,
                         const glm::mat4& view,
                         const glm::mat4& proj,
                         const glm::vec3& camera_pos,
                         const ShadedMaterial& material,
                         const DirectionalLight& light,
                         const std::vector<ShadedPointLight>& point_lights = {},
                         const ShadedGI& gi = {},
                         const std::vector<ShadedSpotLight>& spot_lights = {});

    /// 构建一份 ForwardShaded 兼容的常驻顶点缓冲（GpuMeshVertex 世界空间布局，Final-Feat-6）。
    /// 把局部空间 vertices 按 model 预变换到世界空间（位置 model、法线用法线矩阵、切线 model 线性部分，
    /// 与 DrawShaded 的 CPU 预变换一致）后建一份**静态** GPU 顶点缓冲（is_dynamic=false）。
    /// 返回的句柄由调用方持有并负责释放（device.DeleteGpuBuffer）；供 DrawShadedExternal 复用。
    /// vertices 为空时返回空句柄。
    static BufferHandle BuildShadedWorldVertexBuffer(RhiDevice& device,
                                                     const std::vector<MeshVertex>& vertices,
                                                     const glm::mat4& model);

    /// 记录一次「外部常驻 VAO/EBO + index_count_override」高级 shading 绘制（Final-Feat-6）。
    /// 复用 BuiltinProgram::ForwardShaded（顶点须世界空间，VS 仅施 vp），但不在内部重传顶点/索引：
    /// 绑定调用方持有的 mesh.vertex_buffer / mesh.index_buffer，按 [first_index, first_index+index_count)
    /// 索引子段 DrawIndexed —— 适配 tiled terrain：一份共享地形 VB/IB，每 tile 一次本调用画各自索引段。
    /// material/light/point_lights/gi/spot_lights 语义与 DrawShaded 完全一致（含 splat/雪/WBOIT/GI/CSM）。
    /// index_count==0 或 mesh 句柄无效时直接返回（不绘制）。
    /// @param mesh         外部常驻 VB/IB + 索引类型
    /// @param index_count  本次绘制的索引数（index_count_override；从 first_index 起）
    /// @param first_index  起始索引偏移（tile 在共享 IB 中的子段起点）
    void DrawShadedExternal(CommandBuffer& cmd, RhiDevice& device,
                            const ExternalShadedMesh& mesh,
                            uint32_t index_count,
                            uint32_t first_index,
                            const glm::mat4& view,
                            const glm::mat4& proj,
                            const glm::vec3& camera_pos,
                            const ShadedMaterial& material,
                            const DirectionalLight& light,
                            const std::vector<ShadedPointLight>& point_lights = {},
                            const ShadedGI& gi = {},
                            const std::vector<ShadedSpotLight>& spot_lights = {});

    /// 记录一次 GBuffer 几何通道绘制（阶段4-M3：取代 DrawMeshBatch 在 gbuffer_rendering_mode 下的延迟几何输出）。
    /// 复用 BuiltinProgram::GBufferMesh（forward_pbr.vert + gbuffer.frag）：顶点 CPU 预变换到世界空间、VS 仅施 vp，
    /// 片元向 MRT 输出 gAlbedo(loc0)=texColor×vColor / gNormal(loc1)=normalize(N)×0.5+0.5 / gPosition(loc2)=world pos。
    /// 供 ShadowRSMPass（DDGI 反射阴影图 RSM）→DDGIUpdatePass 采样生成 VPL（虚拟点光）。
    /// 须配 color_attachment_count≥3 的 MRT RenderTarget；vertices/indices 为空时直接返回（不绘制）。
    /// @param model       world-space model 矩阵（CPU 预变换顶点位置/法线/切线，与 DrawShaded 同源）
    /// @param view/proj   相机视图/投影（vp = proj*view；proj 须含 GetProjectionCorrection）
    /// @param albedo_tex  反照率纹理句柄（0 → 回退内建 1x1 白纹理，gAlbedo 输出纯顶点色）
    void DrawGBuffer(CommandBuffer& cmd, RhiDevice& device,
                     const std::vector<MeshVertex>& vertices,
                     const std::vector<uint16_t>& indices,
                     const glm::mat4& model,
                     const glm::mat4& view,
                     const glm::mat4& proj,
                     unsigned int albedo_tex = 0);

    /// 构建一份**局部空间**模板顶点缓冲（GpuMeshVertex 布局，Final-Feat-7）。与 BuildShadedWorldVertexBuffer
    /// 不同：**不**做 model 预变换（顶点保持局部空间），因为每个实例各有 model 矩阵、由 VS 按实例变换。
    /// 建一份静态 GPU 顶点缓冲（is_dynamic=false）供大量实例共享（shared_vertex_ptr：一份模板顶点 + 每实例
    /// model 矩阵，省去 N 份顶点副本）。返回的句柄由调用方持有并负责释放（device.DeleteGpuBuffer）。
    /// vertices 为空时返回空句柄。
    static BufferHandle BuildShadedLocalVertexBuffer(RhiDevice& device,
                                                     const std::vector<MeshVertex>& vertices);

    /// 记录一次「共享网格模板 + 硬件实例化」高级 shading 绘制（Final-Feat-7：shared_vertex_ptr 去重）。
    /// 复用 BuiltinProgram::ForwardInstancedShaded（每实例 model 矩阵 SSBO\@slot0，VS 按 gl_InstanceIndex 取并
    /// 变换局部空间顶点/法线）。绑定调用方持有的**共享局部空间模板** tmpl.vertex_buffer / tmpl.index_buffer
    /// （由 BuildShadedLocalVertexBuffer 构建），按 [first_index, first_index+index_count) 子段对每个实例
    /// DrawIndexedInstanced —— 即大量 tree 实例共享一份顶点模板、各自 model 矩阵，显存只存一份模板顶点。
    /// material/light/point_lights/gi/spot_lights 语义与 DrawInstancedShaded 完全一致。
    /// index_count==0 / instance_models 空 / tmpl 句柄无效时直接返回（不绘制）。
    /// @param tmpl            共享局部空间模板 VB/IB + 索引类型（调用方持有，多次绘制/多帧复用）
    /// @param index_count     每实例绘制的索引数（index_count_override；从 first_index 起）
    /// @param first_index     起始索引偏移（模板内子网格起点）
    /// @param instance_models 每实例 world-space model 矩阵（写入内部实例 SSBO，0 基索引）
    void DrawSharedTemplateInstanced(CommandBuffer& cmd, RhiDevice& device,
                                     const ExternalShadedMesh& tmpl,
                                     uint32_t index_count,
                                     uint32_t first_index,
                                     const std::vector<glm::mat4>& instance_models,
                                     const glm::mat4& view,
                                     const glm::mat4& proj,
                                     const glm::vec3& camera_pos,
                                     const ShadedMaterial& material,
                                     const DirectionalLight& light,
                                     const std::vector<ShadedPointLight>& point_lights = {},
                                     const ShadedGI& gi = {},
                                     const std::vector<ShadedSpotLight>& spot_lights = {});

    /// 记录一次无光照 2D 三角网格绘制（B2b-6：spine 2D 蒙皮迁移，取代 DrawMeshBatch 对 spine 项的处理）。
    /// 顶点为**世界空间**（spine runtime computeWorldVertices 已做完 2D 骨骼蒙皮），复用
    /// BuiltinProgram::Sprite2D（VS 仅施 vp，frag = texture * vertexColor）——与 DrawMeshBatch 对
    /// lighting_enabled=false 的 2D 项语义一致：无光照、纹理 × 顶点色、关深度测试/写入/背面剔除。
    /// 支持**任意三角拓扑**（spine MeshAttachment 非仅 quad，故不能复用 quad-only 的 SpriteBatchRenderer）。
    /// vertices/indices 为空时直接返回（不绘制）。
    /// @param vertices   世界空间 2D 顶点（位置 + 顶点色 + uv）
    /// @param indices    16 位三角索引（任意拓扑）
    /// @param view/proj  相机视图 / 投影矩阵（vp = proj * view 下发到 Sprite2D PerFrame；proj 须含 GetProjectionCorrection）
    /// @param texture    纹理句柄（0 → 回退内建 1x1 白纹理，输出纯顶点色）
    /// @param blend_mode 0=alpha（默认）/ 1=additive / 2=multiply，与 SpriteBatchRenderer 一致
    void DrawUnlit2D(CommandBuffer& cmd, RhiDevice& device,
                     const std::vector<Unlit2DVertex>& vertices,
                     const std::vector<uint16_t>& indices,
                     const glm::mat4& view,
                     const glm::mat4& proj,
                     unsigned int texture,
                     unsigned int blend_mode = 0);

    /// 记录一次硬件实例化仅深度绘制（B2b-6：grass 深度/阴影 pass）。顶点为局部空间，每实例 model
    /// 矩阵走 instance SSBO\@slot0，VS 按 gl_InstanceIndex 取出后施 model + vp（可选植被风）+ 空 shadow.frag；
    /// 只写深度、不输出颜色，配 has_color=false RT。复用 BuiltinProgram::ForwardInstancedDepth。
    /// @param vertices        局部空间顶点（所有实例共享；VS 按实例 model 变换，不在 CPU 预变换）
    /// @param indices         16 位索引
    /// @param instance_models 每实例 world-space model 矩阵（写入内部实例 SSBO，0 基索引；契约 first_instance=0）
    /// @param view/proj       相机视图 / 投影矩阵（proj 须含 GetProjectionCorrection）
    /// @param foliage         true=喂入全局植被风参（与 forward pass 同算，避免阴影错位）；false=不位移
    void DrawDepthOnlyInstanced(CommandBuffer& cmd, RhiDevice& device,
                                const std::vector<MeshVertex>& vertices,
                                const std::vector<uint16_t>& indices,
                                const std::vector<glm::mat4>& instance_models,
                                const glm::mat4& view,
                                const glm::mat4& proj,
                                bool foliage = false);

    /// 记录一次「共享网格模板 + 硬件实例化」仅深度绘制（B2b-6：tree 深度/阴影 pass）。绑定调用方持有的
    /// 共享**局部空间**模板 tmpl.vertex_buffer / tmpl.index_buffer（由 BuildShadedLocalVertexBuffer 构建），
    /// 按 [first_index, first_index+index_count) 子段对每个实例 DrawIndexedInstanced；语义同
    /// DrawDepthOnlyInstanced（ForwardInstancedDepth + 仅 PerFrame UBO + 实例 SSBO + 可选植被风）。
    /// index_count==0 / instance_models 空 / tmpl 句柄无效时直接返回。
    void DrawDepthOnlySharedTemplateInstanced(CommandBuffer& cmd, RhiDevice& device,
                                              const ExternalShadedMesh& tmpl,
                                              uint32_t index_count,
                                              uint32_t first_index,
                                              const std::vector<glm::mat4>& instance_models,
                                              const glm::mat4& view,
                                              const glm::mat4& proj,
                                              bool foliage = false);

    /// 记录一队 CPU mesh 的批量绘制（阶段4-M4：取代 DrawMeshBatch ABI 在 RenderScene::
    /// DrawOpaqueCpu / DrawTransparent 上的全部职责）。逐 item 据 skinned/instanced/morph 字段
    /// 与全局渲染状态（GetGlobalRenderState 的 current_pass_depth_only / gbuffer_rendering_mode /
    /// wireframe/overdraw/force_unlit）路由到对应 Draw*Shaded / DrawGBuffer 方法（复用 M1–M3 成果）：
    ///   - gbuffer_rendering_mode：走 DrawGBuffer（蒙皮/实例在 CPU 预蒙皮/逐实例展开）；
    ///   - 否则（forward 或 depth-only）：走 DrawShaded / DrawSkinnedShaded / DrawInstancedShaded /
    ///     DrawSkinnedInstancedShaded（depth-only RT 无颜色附件 → frag 颜色被丢弃、仅写深度）。
    /// depth-only 阴影 pass（ortho）按三后端执行器原算法对实例做 shadow-cull 预算 + lightspace 裁剪；
    /// PreZ（透视 depth-only）整体跳过蒙皮实例。morph item 因 MeshDrawItem 不携带形变增量（与执行器
    /// 一致）退化为静态/蒙皮路径。索引按 16 位下发（cpu_mesh 顶点数 < 65536）。
    /// view/proj 由调用方从 FrameContext 传入（须含 GetProjectionCorrection，与执行器同源）。
    void DrawBatch(CommandBuffer& cmd, RhiDevice& device,
                   const std::vector<MeshDrawItem>& items,
                   const glm::mat4& view,
                   const glm::mat4& proj);

    /// 释放内建资源（可选；设备析构时缓冲随之回收）
    void Shutdown(RhiDevice& device);

private:
    void EnsureResources(RhiDevice& device);
    void EnsureVertexCapacity(RhiDevice& device, size_t vertex_bytes);
    void EnsureIndexCapacity(RhiDevice& device, size_t index_bytes);
    void EnsureBoneCapacity(RhiDevice& device, size_t bone_bytes);
    void EnsureInstanceCapacity(RhiDevice& device, size_t instance_bytes);
    void EnsureMorphCapacity(RhiDevice& device, size_t morph_bytes);
    void EnsureIndirectBuffer(RhiDevice& device);
    void EnsureShadedResources(RhiDevice& device);
    void EnsureUnlit2DResources(RhiDevice& device);  ///< 懒建无光照 2D 的 alpha/additive/multiply 混合 PSO（B2b-6）

    /// 据编辑器视图模式（GetGlobalRenderState 的 wireframe_mode/overdraw_mode）与材质属性挑选高级 shading
    /// forward 路径 PSO（阶段4-M2）。优先级：wireframe > overdraw > WBOIT(accum/reveal) > double-sided。
    /// force_unlit 不影响 PSO（仅经 ApplyEditorSceneOverride 关方向光），单独处理。
    unsigned int SelectShadedPso(RhiDevice& device, const ShadedMaterial& material);

    unsigned int pso_ = 0;
    unsigned int pso_no_cull_ = 0;  ///< double-sided 用的不剔除 PSO（DrawShaded 按需懒创建）
    unsigned int pso_wboit_accum_ = 0;   ///< WBOIT accumulation：加性混合 ONE/ONE，深度测试不写（B2c-4）
    unsigned int pso_wboit_reveal_ = 0;  ///< WBOIT revealage：ZERO/ONE_MINUS_SRC_ALPHA 乘性混合，深度测试不写（B2c-4）
    unsigned int pso_wireframe_ = 0;  ///< 编辑器线框视图模式 PSO（line-fill，与 pso_ 同状态但 wireframe=true，阶段4-M2）
    unsigned int pso_overdraw_ = 0;   ///< 编辑器 overdraw 视图模式 PSO（加性混合 ONE/ONE + 深度测试不写，阶段4-M2）
    unsigned int pso_unlit2d_alpha_ = 0;     ///< 无光照 2D alpha 混合 PSO（深度测试/写入/剔除全关，B2b-6）
    unsigned int pso_unlit2d_additive_ = 0;  ///< 无光照 2D additive 混合 PSO（B2b-6）
    unsigned int pso_unlit2d_multiply_ = 0;  ///< 无光照 2D multiply 混合 PSO（B2b-6）
    BufferHandle per_material_shaded_ubo_;  ///< 扩展 PerMaterial UBO（160B，ForwardShaded 专用）
    BufferHandle per_point_lights_ubo_;     ///< 点光 UBO（3088B，binding=3，B2c-2；count=0 时退化为纯方向光）
    BufferHandle per_terrain_ubo_;          ///< 地形参数 UBO（48B，slot=4，B2c-3；splat 4 层 + 积雪）
    BufferHandle per_light_probe_ubo_;      ///< LightProbe SH UBO（160B，slot=5，B2c-5）
    BufferHandle per_ddgi_ubo_;             ///< DDGI 参数 UBO（64B，slot=6，B2c-5）
    BufferHandle per_spot_lights_ubo_;      ///< 聚光灯 UBO（4112B，set7.b1，slot=7，Final-Feat-4；count=0 时无聚光灯）
    unsigned int white_tex_ = 0;
    unsigned int white_cube_tex_ = 0;       ///< 1x1 白色 cube：点光 shadow cube 缺省槽回退（Final-Feat-8）
    BufferHandle vbo_;
    BufferHandle ibo_;
    BufferHandle per_frame_ubo_;
    BufferHandle per_scene_ubo_;
    BufferHandle per_material_ubo_;
    BufferHandle bone_ssbo_;
    BufferHandle instance_ssbo_;
    BufferHandle morph_ssbo_;       ///< morph 增量 SSBO（set7.b0，slot=0，Final-Feat-5；[target*vertex_count+vertex]）
    BufferHandle indirect_buffer_;  ///< GPU-driven 间接绘制命令缓冲（B2b-5，单条 DrawElementsIndirectCommand）
    size_t vbo_capacity_ = 0;
    size_t ibo_capacity_ = 0;
    size_t bone_ssbo_capacity_ = 0;
    size_t instance_ssbo_capacity_ = 0;
    size_t morph_ssbo_capacity_ = 0;
    bool init_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_MESH_RENDERER_H
