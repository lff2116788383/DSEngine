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

/// 单方向光。
struct DirectionalLight {
    glm::vec3 direction{0.0f, -1.0f, 0.0f}; ///< 光线传播方向（从光源射向场景）
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float ambient = 0.03f; ///< 常数环境项系数
    bool enabled = true;
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

    /// 释放内建资源（可选；设备析构时缓冲随之回收）
    void Shutdown(RhiDevice& device);

private:
    void EnsureResources(RhiDevice& device);
    void EnsureVertexCapacity(RhiDevice& device, size_t vertex_bytes);
    void EnsureIndexCapacity(RhiDevice& device, size_t index_bytes);
    void EnsureBoneCapacity(RhiDevice& device, size_t bone_bytes);
    void EnsureInstanceCapacity(RhiDevice& device, size_t instance_bytes);

    unsigned int pso_ = 0;
    unsigned int white_tex_ = 0;
    BufferHandle vbo_;
    BufferHandle ibo_;
    BufferHandle per_frame_ubo_;
    BufferHandle per_scene_ubo_;
    BufferHandle per_material_ubo_;
    BufferHandle bone_ssbo_;
    BufferHandle instance_ssbo_;
    size_t vbo_capacity_ = 0;
    size_t ibo_capacity_ = 0;
    size_t bone_ssbo_capacity_ = 0;
    size_t instance_ssbo_capacity_ = 0;
    bool init_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_MESH_RENDERER_H
