/**
 * @file mesh_renderer.h
 * @brief 后端无关的静态 forward PBR 网格渲染器（B2b-1）。
 *
 * 用 B0/P0 的通用绘制原语（SetPipelineState / BindShaderProgram / BindUniformBuffer /
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

    /// 释放内建资源（可选；设备析构时缓冲随之回收）
    void Shutdown(RhiDevice& device);

private:
    void EnsureResources(RhiDevice& device);
    void EnsureVertexCapacity(RhiDevice& device, size_t vertex_bytes);
    void EnsureIndexCapacity(RhiDevice& device, size_t index_bytes);
    void EnsureBoneCapacity(RhiDevice& device, size_t bone_bytes);
    void EnsureInstanceCapacity(RhiDevice& device, size_t instance_bytes);
    void EnsureIndirectBuffer(RhiDevice& device);
    void EnsureShadedResources(RhiDevice& device);

    unsigned int pso_ = 0;
    unsigned int pso_no_cull_ = 0;  ///< double-sided 用的不剔除 PSO（DrawShaded 按需懒创建）
    unsigned int pso_wboit_accum_ = 0;   ///< WBOIT accumulation：加性混合 ONE/ONE，深度测试不写（B2c-4）
    unsigned int pso_wboit_reveal_ = 0;  ///< WBOIT revealage：ZERO/ONE_MINUS_SRC_ALPHA 乘性混合，深度测试不写（B2c-4）
    BufferHandle per_material_shaded_ubo_;  ///< 扩展 PerMaterial UBO（160B，ForwardShaded 专用）
    BufferHandle per_point_lights_ubo_;     ///< 点光 UBO（3088B，binding=3，B2c-2；count=0 时退化为纯方向光）
    BufferHandle per_terrain_ubo_;          ///< 地形参数 UBO（48B，slot=4，B2c-3；splat 4 层 + 积雪）
    BufferHandle per_light_probe_ubo_;      ///< LightProbe SH UBO（160B，slot=5，B2c-5）
    BufferHandle per_ddgi_ubo_;             ///< DDGI 参数 UBO（64B，slot=6，B2c-5）
    BufferHandle per_spot_lights_ubo_;      ///< 聚光灯 UBO（4112B，set7.b1，slot=7，Final-Feat-4；count=0 时无聚光灯）
    unsigned int white_tex_ = 0;
    BufferHandle vbo_;
    BufferHandle ibo_;
    BufferHandle per_frame_ubo_;
    BufferHandle per_scene_ubo_;
    BufferHandle per_material_ubo_;
    BufferHandle bone_ssbo_;
    BufferHandle instance_ssbo_;
    BufferHandle indirect_buffer_;  ///< GPU-driven 间接绘制命令缓冲（B2b-5，单条 DrawElementsIndirectCommand）
    size_t vbo_capacity_ = 0;
    size_t ibo_capacity_ = 0;
    size_t bone_ssbo_capacity_ = 0;
    size_t instance_ssbo_capacity_ = 0;
    bool init_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_MESH_RENDERER_H
