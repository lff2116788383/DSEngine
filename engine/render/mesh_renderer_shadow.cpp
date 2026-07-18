/**
 * @file mesh_renderer_shadow.cpp
 * @brief MeshRenderer shadow/depth-only rendering methods.
 */

#include "engine/render/mesh_renderer_internal.h"

using namespace dse::render::mesh_internal;

namespace dse {
namespace render {
void MeshRenderer::DrawDepthOnly(CommandBuffer& cmd, RhiDevice& device,
                                 const std::vector<MeshVertex>& vertices,
                                 const std::vector<uint16_t>& indices,
                                 const glm::mat4& model,
                                 const glm::mat4& view,
                                 const glm::mat4& proj) {
    if (vertices.empty() || indices.empty()) return;

    ShaderHandle program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbrDepth);
    if (!program) return;  // 该后端未提供 depth-only 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_) return;

    // --- CPU 侧预变换顶点到世界空间（仅 position 影响深度；normal/tangent 复用以保持布局一致）---
    const glm::mat3 model3 = glm::mat3(model);
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        const glm::vec3 wp = glm::vec3(model * glm::vec4(v.position, 1.0f));
        const glm::vec3 wn = model3 * v.normal;
        const glm::vec3 wt = model3 * v.tangent;
        GpuMeshVertex& g = gpu_verts[i];
        g.px = wp.x; g.py = wp.y; g.pz = wp.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = wn.x; g.ny = wn.y; g.nz = wn.z;
        g.tx = wt.x; g.ty = wt.y; g.tz = wt.z;
    }

    const size_t vbytes = gpu_verts.size() * sizeof(GpuMeshVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- 仅 PerFrame UBO（shadow.frag 空，不需 scene/material/纹理）---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(0.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    cmd.BindPipeline(device.GetGraphicsPipeline(pso_, program));  // 写/测深度（Less）、背面剔除
    cmd.BindUniformBuffer(0u, per_frame_ubo_);  // PerFrame @ set0.b0
    cmd.BindVertexBuffer(0u, vbo_, static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_, IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::DrawDepthOnlyInstanced(CommandBuffer& cmd, RhiDevice& device,
                                          const std::vector<MeshVertex>& vertices,
                                          const std::vector<uint16_t>& indices,
                                          const std::vector<glm::mat4>& instance_models,
                                          const glm::mat4& view,
                                          const glm::mat4& proj,
                                          bool foliage) {
    if (vertices.empty() || indices.empty() || instance_models.empty()) return;

    ShaderHandle program = device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedDepth);
    if (!program) return;  // 该后端未提供实例化 depth-only 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_) return;

    // --- 局部空间顶点打包（VS 按实例 model 变换，不在 CPU 预变换）---
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        GpuMeshVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = v.normal.x; g.ny = v.normal.y; g.nz = v.normal.z;
        g.tx = v.tangent.x; g.ty = v.tangent.y; g.tz = v.tangent.z;
    }

    const size_t inst_bytes = instance_models.size() * sizeof(glm::mat4);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, instance_models.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuMeshVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- 仅 PerFrame UBO（shadow.frag 空，不需 scene/material/纹理）+ 可选植被风 ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(0.0f);
    if (foliage) {
        const auto& grs = device.GetGlobalRenderState();
        frame.foliage_wind = grs.foliage_wind;
        frame.foliage_push = grs.foliage_push;
    }
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u}, VertexAttr{1u, 4u, 12u}, VertexAttr{2u, 2u, 28u},
        VertexAttr{3u, 3u, 36u}, VertexAttr{4u, 3u, 48u},
    };

    cmd.BindPipeline(device.GetGraphicsPipeline(pso_, program));  // 写/测深度（Less）、背面剔除
    cmd.BindUniformBuffer(0u, per_frame_ubo_);  // PerFrame @ set0.b0
    // 每实例 model SSBO\@slot 0（与 DrawInstancedShaded 同源）。
    cmd.BindStorageBuffer(0u, instance_ssbo_, 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindVertexBuffer(0u, vbo_, static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_, IndexType::UInt16);
    // 契约：first_instance 恒 0，偏移已由 0 基 SSBO 索引表达。
    cmd.DrawIndexedInstanced(static_cast<uint32_t>(indices.size()),
                             static_cast<uint32_t>(instance_models.size()),
                             0u, 0, 0u);
}

void MeshRenderer::DrawDepthOnlySharedTemplateInstanced(CommandBuffer& cmd, RhiDevice& device,
                                                        const ExternalShadedMesh& tmpl,
                                                        uint32_t index_count,
                                                        uint32_t first_index,
                                                        const std::vector<glm::mat4>& instance_models,
                                                        const glm::mat4& view,
                                                        const glm::mat4& proj,
                                                        bool foliage) {
    if (index_count == 0 || instance_models.empty() ||
        !tmpl.vertex_buffer || !tmpl.index_buffer) return;

    ShaderHandle program = device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedDepth);
    if (!program) return;

    EnsureResources(device);
    if (!per_frame_ubo_) return;

    const size_t inst_bytes = instance_models.size() * sizeof(glm::mat4);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, instance_models.data());

    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(0.0f);
    if (foliage) {
        const auto& grs = device.GetGlobalRenderState();
        frame.foliage_wind = grs.foliage_wind;
        frame.foliage_push = grs.foliage_push;
    }
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u}, VertexAttr{1u, 4u, 12u}, VertexAttr{2u, 2u, 28u},
        VertexAttr{3u, 3u, 36u}, VertexAttr{4u, 3u, 48u},
    };

    cmd.BindPipeline(device.GetGraphicsPipeline(pso_, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_);
    cmd.BindStorageBuffer(0u, instance_ssbo_, 0u, static_cast<uint32_t>(inst_bytes));
    // 共享局部空间模板 VB/IB（caller 持有、常驻），按子段对每实例绘制。
    cmd.BindVertexBuffer(0u, tmpl.vertex_buffer, static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(tmpl.index_buffer, tmpl.index_type);
    cmd.DrawIndexedInstanced(index_count, static_cast<uint32_t>(instance_models.size()),
                             first_index, 0, 0u);
}
} // namespace render
} // namespace dse
