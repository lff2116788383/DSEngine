/**
 * @file mesh_renderer.cpp
 * @brief MeshRenderer resource management and basic draw methods.
 *
 * Advanced shading, shadow, and instancing methods are in:
 *   mesh_renderer_shaded.cpp, mesh_renderer_shadow.cpp, mesh_renderer_instancing.cpp
 */

#include "engine/render/mesh_renderer_internal.h"

using namespace dse::render::mesh_internal;

namespace dse {
namespace render {
unsigned int MeshRenderer::SelectShadedPso(RhiDevice& device, const ShadedMaterial& material) {
    const auto& grs = device.GetGlobalRenderState();
    if (grs.wireframe_mode) return pso_wireframe_;
    if (grs.overdraw_mode) return pso_overdraw_;
    unsigned int pso = material.double_sided ? pso_no_cull_ : pso_;
    if (material.wboit_mode == 1) pso = pso_wboit_accum_;
    else if (material.wboit_mode == 2) pso = pso_wboit_reveal_;
    return pso;
}

void MeshRenderer::EnsureResources(RhiDevice& device) {
    if (init_) return;

    // 不透明几何 PSO：写/测深度（Less）、背面剔除、不混合。
    PipelineStateDesc desc;
    desc.blend_enabled = false;
    desc.depth_test_enabled = true;
    desc.depth_write_enabled = true;
    desc.depth_func = CompareFunc::Less;
    desc.culling_enabled = true;
    desc.cull_face = CullFace::Back;
    pso_ = device.CreatePipelineState(desc);

    // 1x1 白纹理：缺省纹理槽回退（采样得 1.0，配合 flags 关闭对应贴图）。
    const unsigned char white[4] = {255, 255, 255, 255};
    white_tex_ = device.CreateTexture2D(1, 1, white, /*linear_filter=*/true);
    // Final-Feat-8: 1x1 白色 cube（6 面），点光 shadow cube 缺省槽回退。采样得 .r=1.0 →
    // closestDepth=radius，(cur-bias)>radius 恒假 → 不产生阴影。保证三后端 cube descriptor 维度匹配。
    const unsigned char* white_faces[6] = {white, white, white, white, white, white};
    white_cube_tex_ = device.CreateTextureCube(1, 1, white_faces, /*linear_filter=*/true);

    GpuBufferDesc f_desc;
    f_desc.size = sizeof(FwdPerFrameUBO);
    f_desc.usage = GpuBufferUsage::kUniform;
    f_desc.is_dynamic = true;
    per_frame_ubo_ = device.CreateGpuBuffer(f_desc, nullptr);

    GpuBufferDesc s_desc;
    s_desc.size = sizeof(FwdPerSceneUBO);
    s_desc.usage = GpuBufferUsage::kUniform;
    s_desc.is_dynamic = true;
    per_scene_ubo_ = device.CreateGpuBuffer(s_desc, nullptr);

    GpuBufferDesc m_desc;
    m_desc.size = sizeof(FwdPerMaterialUBO);
    m_desc.usage = GpuBufferUsage::kUniform;
    m_desc.is_dynamic = true;
    per_material_ubo_ = device.CreateGpuBuffer(m_desc, nullptr);

    init_ = true;
}

void MeshRenderer::EnsureUnlit2DResources(RhiDevice& device) {
    // 无光照 2D 的三个混合 PSO（与 SpriteBatchRenderer::PsoForBlend 一致）：关深度测试/写入/剔除。
    // alpha 默认：color = SrcAlpha/OneMinusSrcAlpha，alpha 通道 One/OneMinusSrcAlpha（分离）。
    if (pso_unlit2d_alpha_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::SrcAlpha;
        desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.alpha_blend_src = BlendFactor::One;
        desc.alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.depth_test_enabled = false;
        desc.depth_write_enabled = false;
        desc.culling_enabled = false;
        pso_unlit2d_alpha_ = device.CreatePipelineState(desc);
    }
    if (pso_unlit2d_additive_ == 0) {  // additiveï¼šSrcAlpha/One
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::SrcAlpha;
        desc.blend_dst = BlendFactor::One;
        desc.alpha_blend_src = BlendFactor::SrcAlpha;
        desc.alpha_blend_dst = BlendFactor::One;
        desc.depth_test_enabled = false;
        desc.depth_write_enabled = false;
        desc.culling_enabled = false;
        pso_unlit2d_additive_ = device.CreatePipelineState(desc);
    }
    if (pso_unlit2d_multiply_ == 0) {  // multiplyï¼šDstColor/Zero
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::DstColor;
        desc.blend_dst = BlendFactor::Zero;
        desc.alpha_blend_src = BlendFactor::DstColor;
        desc.alpha_blend_dst = BlendFactor::Zero;
        desc.depth_test_enabled = false;
        desc.depth_write_enabled = false;
        desc.culling_enabled = false;
        pso_unlit2d_multiply_ = device.CreatePipelineState(desc);
    }
}

void MeshRenderer::EnsureVertexCapacity(RhiDevice& device, size_t vertex_bytes) {
    if (vbo_ && vbo_capacity_ >= vertex_bytes) return;
    if (vbo_) device.DeleteGpuBuffer(vbo_);
    GpuBufferDesc vb_desc;
    vb_desc.size = vertex_bytes;
    vb_desc.usage = GpuBufferUsage::kVertex;
    vb_desc.is_dynamic = true;
    vbo_ = device.CreateGpuBuffer(vb_desc, nullptr);
    vbo_capacity_ = vertex_bytes;
}

void MeshRenderer::EnsureIndexCapacity(RhiDevice& device, size_t index_bytes) {
    if (ibo_ && ibo_capacity_ >= index_bytes) return;
    if (ibo_) device.DeleteGpuBuffer(ibo_);
    GpuBufferDesc ib_desc;
    ib_desc.size = index_bytes;
    ib_desc.usage = GpuBufferUsage::kIndex;
    ib_desc.is_dynamic = true;
    ibo_ = device.CreateGpuBuffer(ib_desc, nullptr);
    ibo_capacity_ = index_bytes;
}

void MeshRenderer::EnsureBoneCapacity(RhiDevice& device, size_t bone_bytes) {
    if (bone_ssbo_ && bone_ssbo_capacity_ >= bone_bytes) return;
    if (bone_ssbo_) device.DeleteGpuBuffer(bone_ssbo_);
    GpuBufferDesc b_desc;
    b_desc.size = bone_bytes;
    b_desc.usage = GpuBufferUsage::kStorage;
    b_desc.is_dynamic = true;
    bone_ssbo_ = device.CreateGpuBuffer(b_desc, nullptr);
    bone_ssbo_capacity_ = bone_bytes;
}

void MeshRenderer::EnsureInstanceCapacity(RhiDevice& device, size_t instance_bytes) {
    if (instance_ssbo_ && instance_ssbo_capacity_ >= instance_bytes) return;
    if (instance_ssbo_) device.DeleteGpuBuffer(instance_ssbo_);
    GpuBufferDesc i_desc;
    i_desc.size = instance_bytes;
    i_desc.usage = GpuBufferUsage::kStorage;
    i_desc.is_dynamic = true;
    instance_ssbo_ = device.CreateGpuBuffer(i_desc, nullptr);
    instance_ssbo_capacity_ = instance_bytes;
}

void MeshRenderer::EnsureMorphCapacity(RhiDevice& device, size_t morph_bytes) {
    if (morph_ssbo_ && morph_ssbo_capacity_ >= morph_bytes) return;
    if (morph_ssbo_) device.DeleteGpuBuffer(morph_ssbo_);
    GpuBufferDesc m_desc;
    m_desc.size = morph_bytes;
    m_desc.usage = GpuBufferUsage::kStorage;
    m_desc.is_dynamic = true;
    morph_ssbo_ = device.CreateGpuBuffer(m_desc, nullptr);
    morph_ssbo_capacity_ = morph_bytes;
}

void MeshRenderer::EnsureIndirectBuffer(RhiDevice& device) {
    if (indirect_buffer_) return;
    GpuBufferDesc d_desc;
    d_desc.size = sizeof(DrawElementsIndirectCommand);  // 单条间接绘制命令
    d_desc.usage = GpuBufferUsage::kIndirect;
    d_desc.is_dynamic = true;
    indirect_buffer_ = device.CreateGpuBuffer(d_desc, nullptr);
}

void MeshRenderer::EnsureShadedResources(RhiDevice& device) {
    // 不剔除 PSO（double-sided 用），与 pso_ 同状态但关背面剔除。
    if (pso_no_cull_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = false;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = true;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = false;
        pso_no_cull_ = device.CreatePipelineState(desc);
    }
    // WBOIT accumulation PSO（B2c-4）：加性混合（color/alpha 均 ONE/ONE），深度测试开但不写、不剔除，
    // 使各透明片元贡献顺序无关地累加（着色器 wboit_mode=1 输出预乘加权 color/alpha）。
    if (pso_wboit_accum_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::One;
        desc.blend_dst = BlendFactor::One;
        desc.alpha_blend_src = BlendFactor::One;
        desc.alpha_blend_dst = BlendFactor::One;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = false;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = false;
        pso_wboit_accum_ = device.CreatePipelineState(desc);
    }
    // WBOIT revealage PSO（B2c-4）：ZERO/ONE_MINUS_SRC_ALPHA 乘性混合（dst *= (1-srcAlpha)），
    // 深度测试开但不写、不剔除（着色器 wboit_mode=2 输出 (0,0,0,alpha)）。
    if (pso_wboit_reveal_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::Zero;
        desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.alpha_blend_src = BlendFactor::Zero;
        desc.alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = false;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = false;
        pso_wboit_reveal_ = device.CreatePipelineState(desc);
    }
    // 编辑器线框视图模式 PSO（阶段4-M2）：与 pso_ 同状态（写/测深度、背面剔除、不混合），仅 wireframe=true。
    if (pso_wireframe_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = false;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = true;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = true;
        desc.cull_face = CullFace::Back;
        desc.wireframe = true;
        pso_wireframe_ = device.CreatePipelineState(desc);
    }
    // 编辑器 overdraw 视图模式 PSO（阶段4-M2）：加性混合 ONE/ONE + 深度测试开但不写、不剔除，
    // 配合 ApplyEditorMaterialOverride 的固定低强度材质，使重叠片元以亮度叠加显示过度绘制
    //（与执行器 DX11 SetOverdrawMode / Vulkan overdraw_mode_ 语义一致）。
    if (pso_overdraw_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::One;
        desc.blend_dst = BlendFactor::One;
        desc.alpha_blend_src = BlendFactor::One;
        desc.alpha_blend_dst = BlendFactor::One;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = false;
        desc.depth_func = CompareFunc::LessEqual;
        desc.culling_enabled = false;
        pso_overdraw_ = device.CreatePipelineState(desc);
    }
    // 扩展 PerMaterial UBO（160B）。
    if (!per_material_shaded_ubo_) {
        GpuBufferDesc m_desc;
        m_desc.size = sizeof(FwdShadedMaterialUBO);
        m_desc.usage = GpuBufferUsage::kUniform;
        m_desc.is_dynamic = true;
        per_material_shaded_ubo_ = device.CreateGpuBuffer(m_desc, nullptr);
    }
    // 点光 UBO（3088B，binding=3；count=0 时退化为纯方向光，输出与 B2c-1 一致）。
    if (!per_point_lights_ubo_) {
        GpuBufferDesc p_desc;
        p_desc.size = sizeof(PointLightsUBO);
        p_desc.usage = GpuBufferUsage::kUniform;
        p_desc.is_dynamic = true;
        per_point_lights_ubo_ = device.CreateGpuBuffer(p_desc, nullptr);
    }
    // 地形参数 UBO（48B，slot=4；splat_enabled=0 且 snow_coverage=0 时与 B2c-2 输出一致）。
    if (!per_terrain_ubo_) {
        GpuBufferDesc t_desc;
        t_desc.size = sizeof(TerrainParamsUBO);
        t_desc.usage = GpuBufferUsage::kUniform;
        t_desc.is_dynamic = true;
        per_terrain_ubo_ = device.CreateGpuBuffer(t_desc, nullptr);
    }
    // LightProbe SH UBO（160B，slot=5；sh_enabled=0 时不影响间接光，B2c-5）。
    if (!per_light_probe_ubo_) {
        GpuBufferDesc lp_desc;
        lp_desc.size = sizeof(LightProbeDataUBO);
        lp_desc.usage = GpuBufferUsage::kUniform;
        lp_desc.is_dynamic = true;
        per_light_probe_ubo_ = device.CreateGpuBuffer(lp_desc, nullptr);
    }
    // DDGI 参数 UBO（64B，slot=6；ddgi_enabled=0 时不影响间接光，B2c-5）。
    if (!per_ddgi_ubo_) {
        GpuBufferDesc d_desc;
        d_desc.size = sizeof(FwdDDGIParamsUBO);
        d_desc.usage = GpuBufferUsage::kUniform;
        d_desc.is_dynamic = true;
        per_ddgi_ubo_ = device.CreateGpuBuffer(d_desc, nullptr);
    }
    // 聚光灯 UBO（4112B，set7.b1/slot=7；count=0 时无聚光灯贡献，输出与 Final-Feat-3 一致）。
    if (!per_spot_lights_ubo_) {
        GpuBufferDesc sl_desc;
        sl_desc.size = sizeof(SpotLightsUBO);
        sl_desc.usage = GpuBufferUsage::kUniform;
        sl_desc.is_dynamic = true;
        per_spot_lights_ubo_ = device.CreateGpuBuffer(sl_desc, nullptr);
    }
}

void MeshRenderer::DrawBatch(CommandBuffer& cmd, RhiDevice& device,
                             const std::vector<MeshDrawItem>& items,
                             const glm::mat4& view,
                             const glm::mat4& proj) {
    if (items.empty()) return;

    const DrawExecutorGlobalState& grs = device.GetGlobalRenderState();
    const bool depth_only = grs.current_pass_depth_only;
    const bool gbuffer_mode = grs.gbuffer_rendering_mode;
    const glm::vec3 camera_pos = glm::vec3(glm::inverse(view)[3]);

    // --- Shadow-cull 预算（仅 depth-only + ortho 阴影 pass；常数/算法与三后端执行器逐位一致）---
    // is_ortho 用 proj[2][3]≈0 判定（与执行器同式：perspective=-1/ortho=0；clip 修正不改该元素）。
    const bool is_ortho = std::abs(proj[2][3]) < 0.01f;
    const bool shadow_cull_active = depth_only && is_ortho;
    // PreZ（透视 depth-only）：蒙皮实例 VS 骨骼开销极大、阴影收益低，整体跳过（与执行器一致）。
    const bool prez_skip_skinned = depth_only && !is_ortho;
    float shadow_cull_limit = 0.0f;
    size_t shadow_inst_budget = SIZE_MAX;
    if (shadow_cull_active && std::abs(proj[0][0]) > 1e-6f) {
        constexpr float kShadowCullMargin       = 150.0f;
        constexpr float kBudgetOrthoThreshold   = 2000.0f;
        constexpr float kBudgetBaseInstances    = 800.0f;
        constexpr float kBudgetMinInstances     = 64.0f;
        constexpr float kSkinnedShadowSkipOrtho = 1500.0f;
        constexpr float kSkinnedBudgetOrtho     = 400.0f;
        constexpr float kSkinnedBudgetBase      = 200.0f;
        const float ortho_size = 1.0f / proj[0][0];
        shadow_cull_limit = ortho_size + kShadowCullMargin;
        if (ortho_size > kBudgetOrthoThreshold) {
            shadow_inst_budget = static_cast<size_t>(
                std::max(kBudgetBaseInstances * kBudgetOrthoThreshold / ortho_size, kBudgetMinInstances));
        }
        if (ortho_size > kSkinnedShadowSkipOrtho) {
            shadow_inst_budget = 0;
        } else if (ortho_size > kSkinnedBudgetOrtho) {
            shadow_inst_budget = static_cast<size_t>(
                std::max(kSkinnedBudgetBase * kSkinnedBudgetOrtho / ortho_size, 0.0f));
        }
    }

    for (const auto& item : items) {
        // 顶点/索引数据源：优先 shared_vertex_ptr（共享模板），否则 item 内联缓冲（与执行器同序）。
        const BatchVertex* vtx_data = item.shared_vertex_ptr ? item.shared_vertex_ptr : item.vertices.data();
        const uint32_t* idx_data = item.shared_index_ptr ? item.shared_index_ptr : item.indices.data();
        const size_t vtx_count = item.shared_vertex_ptr ? item.shared_vertex_count : item.vertices.size();
        const size_t idx_count = item.shared_index_ptr ? item.shared_index_count : item.indices.size();
        if (vtx_count == 0 || idx_count == 0) continue;

        const bool is_instanced = item.instance_transforms.size() > 1;
        const bool skinned_instanced = item.skinned
            && (!item.per_instance_bones.empty() || !item.bone_palette.empty())
            && is_instanced;
        const bool single_skinned = item.skinned && !is_instanced && !item.bone_matrices.empty();

        // 索引 uint32 → uint16（MeshRenderer 逐变体方法契约为 16 位；cpu_mesh 顶点数 < 65536）。
        std::vector<uint16_t> indices16(idx_count);
        for (size_t k = 0; k < idx_count; ++k)
            indices16[k] = static_cast<uint16_t>(idx_data[k]);

        const ShadedMaterial material = BatchToShadedMaterial(item);
        const DirectionalLight light = BatchToDirLight(item);
        const std::vector<ShadedPointLight> point_lights = BatchToPointLights(item);
        const std::vector<ShadedSpotLight> spot_lights = BatchToSpotLights(item);
        const ShadedGI gi{};  // 与已迁移的 terrain/tree/grass 一致：CPU mesh forward 路径不带 DDGI/SH。

        // --- 实例可见集（仅 instanced）：depth-only ortho 阴影 pass 按预算 + lightspace 裁剪 ---
        std::vector<glm::mat4> vis_models;
        std::vector<int> vis_palette_idx;
        if (is_instanced) {
            if (prez_skip_skinned && skinned_instanced) continue;  // PreZ 跳过蒙皮实例
            const size_t n = item.instance_transforms.size();
            vis_models.reserve(n);
            if (skinned_instanced) vis_palette_idx.reserve(n);
            for (size_t j = 0; j < n; ++j) {
                if (shadow_cull_active) {
                    if (vis_models.size() >= shadow_inst_budget) break;
                    if (shadow_cull_limit > 0.0f) {
                        const glm::vec3 wp(item.instance_transforms[j][3]);
                        const glm::vec4 ls = view * glm::vec4(wp, 1.0f);
                        if (std::abs(ls.x) > shadow_cull_limit || std::abs(ls.y) > shadow_cull_limit)
                            continue;
                    }
                }
                vis_models.push_back(item.instance_transforms[j]);
                if (skinned_instanced) {
                    const int pidx = (j < item.instance_bone_palette_idx.size())
                        ? item.instance_bone_palette_idx[j] : 0;
                    vis_palette_idx.push_back(pidx);
                }
            }
            if (vis_models.empty()) continue;  // 全部被剔除
        }

        // ===== GBuffer 模式（RSM；非 depth-only）：蒙皮/实例在 CPU 展开为静态世界几何 =====
        if (gbuffer_mode) {
            const unsigned int albedo_tex = item.texture_handle;
            if (skinned_instanced) {
                for (size_t j = 0; j < vis_models.size(); ++j) {
                    const int pidx = vis_palette_idx[j];
                    const std::vector<glm::mat4>& pal =
                        (pidx >= 0 && pidx < static_cast<int>(item.bone_palette.size()))
                            ? item.bone_palette[pidx] : item.bone_palette[0];
                    std::vector<MeshVertex> sk = SkinBatchToLocal(vtx_data, vtx_count, pal);
                    DrawGBuffer(cmd, device, sk, indices16, vis_models[j], view, proj, albedo_tex);
                }
            } else if (is_instanced) {
                std::vector<MeshVertex> mverts(vtx_count);
                for (size_t i = 0; i < vtx_count; ++i) mverts[i] = BatchToMeshVertex(vtx_data[i]);
                for (const auto& mdl : vis_models)
                    DrawGBuffer(cmd, device, mverts, indices16, mdl, view, proj, albedo_tex);
            } else if (single_skinned) {
                std::vector<MeshVertex> sk = SkinBatchToLocal(vtx_data, vtx_count, item.bone_matrices);
                DrawGBuffer(cmd, device, sk, indices16, item.model, view, proj, albedo_tex);
            } else {
                std::vector<MeshVertex> mverts(vtx_count);
                for (size_t i = 0; i < vtx_count; ++i) mverts[i] = BatchToMeshVertex(vtx_data[i]);
                DrawGBuffer(cmd, device, mverts, indices16, item.model, view, proj, albedo_tex);
            }
            continue;
        }

        // ===== forward / depth-only：复用 forward program（depth-only RT 无颜色附件 → frag 丢弃）=====
        if (skinned_instanced) {
            std::vector<SkinnedMeshVertex> sverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) sverts[i] = BatchToSkinnedVertex(vtx_data[i]);
            DrawSkinnedInstancedShaded(cmd, device, sverts, indices16, vis_models,
                                       item.bone_palette, vis_palette_idx, view, proj, camera_pos,
                                       material, light, point_lights, gi, spot_lights);
        } else if (is_instanced) {
            std::vector<MeshVertex> mverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) mverts[i] = BatchToMeshVertex(vtx_data[i]);
            // 后端未提供实例化 shaded 内建着色器时（如当前上下文缺少 SSBO 支持，
            // DrawInstancedShaded 会因 program==0 直接 return），逐实例回退到非实例
            // DrawShaded，避免整批合批 mesh 静默不渲染。
            if (device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedShaded) == 0) {
                for (const auto& mdl : vis_models)
                    DrawShaded(cmd, device, mverts, indices16, mdl, view, proj, camera_pos,
                               material, light, point_lights, gi, spot_lights);
            } else {
                DrawInstancedShaded(cmd, device, mverts, indices16, vis_models, view, proj, camera_pos,
                                    material, light, point_lights, gi, spot_lights);
            }
        } else if (single_skinned) {
            std::vector<SkinnedMeshVertex> sverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) sverts[i] = BatchToSkinnedVertex(vtx_data[i]);
            DrawSkinnedShaded(cmd, device, sverts, indices16, item.model, item.bone_matrices,
                              view, proj, camera_pos, material, light, point_lights, gi, spot_lights);
        } else {
            std::vector<MeshVertex> mverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) mverts[i] = BatchToMeshVertex(vtx_data[i]);
            DrawShaded(cmd, device, mverts, indices16, item.model, view, proj, camera_pos,
                       material, light, point_lights, gi, spot_lights);
        }
    }
}

BufferHandle MeshRenderer::BuildShadedLocalVertexBuffer(RhiDevice& device,
                                                        const std::vector<MeshVertex>& vertices) {
    if (vertices.empty()) return BufferHandle{};
    // 局部空间打包（不做 model 预变换；每实例 model 由 VS 按 gl_InstanceIndex 变换）。
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
    GpuBufferDesc vb_desc;
    vb_desc.size = gpu_verts.size() * sizeof(GpuMeshVertex);
    vb_desc.usage = GpuBufferUsage::kVertex;
    vb_desc.is_dynamic = false;  // 常驻静态模板缓冲（多实例/多帧共享）
    return device.CreateGpuBuffer(vb_desc, gpu_verts.data());
}


void MeshRenderer::DrawSkinned(CommandBuffer& cmd, RhiDevice& device,
                               const std::vector<SkinnedMeshVertex>& vertices,
                               const std::vector<uint16_t>& indices,
                               const glm::mat4& model,
                               const std::vector<glm::mat4>& bone_matrices,
                               const glm::mat4& view,
                               const glm::mat4& proj,
                               const glm::vec3& camera_pos,
                               const MeshMaterial& material,
                               const DirectionalLight& light) {
    if (vertices.empty() || indices.empty() || bone_matrices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbrSkinned);
    if (program == 0) return;  // 该后端未提供蒙皮 forward PBR 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- 顶点打包（局部/绑定空间，VS 施骨骼混合 + vp，不在 CPU 预变换） ---
    std::vector<GpuSkinnedVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const SkinnedMeshVertex& v = vertices[i];
        GpuSkinnedVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = v.normal.x; g.ny = v.normal.y; g.nz = v.normal.z;
        g.tx = v.tangent.x; g.ty = v.tangent.y; g.tz = v.tangent.z;
        g.bi0 = v.bone_indices.x; g.bi1 = v.bone_indices.y;
        g.bi2 = v.bone_indices.z; g.bi3 = v.bone_indices.w;
        g.bw0 = v.bone_weights.x; g.bw1 = v.bone_weights.y;
        g.bw2 = v.bone_weights.z; g.bw3 = v.bone_weights.w;
    }

    // --- 骨骼矩阵：左乘 model 得世界空间，写入 SSBO ---
    std::vector<glm::mat4> world_bones(bone_matrices.size());
    for (size_t i = 0; i < bone_matrices.size(); ++i) {
        world_bones[i] = model * bone_matrices[i];
    }
    const size_t bone_bytes = world_bones.size() * sizeof(glm::mat4);
    EnsureBoneCapacity(device, bone_bytes);
    if (!bone_ssbo_) return;
    device.UpdateGpuBuffer(bone_ssbo_, 0, bone_bytes, world_bones.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuSkinnedVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- UBO 填充（与静态路径同构） ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    scene.light_params = glm::vec4(light.intensity, 0.0f, 0.0f, 0.0f);
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdPerMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    device.UpdateGpuBuffer(per_material_ubo_, 0, sizeof(mat), &mat);

    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
        VertexAttr{5u, 4u, 60u},   // bone indices
        VertexAttr{6u, 4u, 76u},   // bone weights
    };

    cmd.BindPipeline(device.GetGraphicsPipeline(pso_, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());     // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());     // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_ubo_.raw());  // PerMaterial @ set2.b0
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    // 骨骼矩阵 SSBO\@slot 0（三后端通用语义：GL binding0 / Vulkan 位置0 / DX11 t0 经 @SSBO_LOW_REGISTERS）。
    cmd.BindStorageBuffer(0u, bone_ssbo_.raw(), 0u, static_cast<uint32_t>(bone_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuSkinnedVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::Draw(CommandBuffer& cmd, RhiDevice& device,
                        const std::vector<MeshVertex>& vertices,
                        const std::vector<uint16_t>& indices,
                        const glm::mat4& model,
                        const glm::mat4& view,
                        const glm::mat4& proj,
                        const glm::vec3& camera_pos,
                        const MeshMaterial& material,
                        const DirectionalLight& light) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbr);
    if (program == 0) return;  // 该后端未提供 forward PBR 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- CPU 侧预变换顶点到世界空间 ---
    const glm::mat3 normal_matrix = glm::inverseTranspose(glm::mat3(model));
    const glm::mat3 model3 = glm::mat3(model);
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        const glm::vec3 wp = glm::vec3(model * glm::vec4(v.position, 1.0f));
        const glm::vec3 wn = glm::normalize(normal_matrix * v.normal);
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

    // --- UBO 填充 ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);  // L = 指向光源
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    scene.light_params = glm::vec4(light.intensity, 0.0f, 0.0f, 0.0f);
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdPerMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    device.UpdateGpuBuffer(per_material_ubo_, 0, sizeof(mat), &mat);

    // --- 纹理（缺省回退到白纹理；flat unit 0..4） ---
    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    cmd.BindPipeline(device.GetGraphicsPipeline(pso_, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());     // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());     // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_ubo_.raw());  // PerMaterial @ set2.b0
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}


void MeshRenderer::DrawUnlit2D(CommandBuffer& cmd, RhiDevice& device,
                              const std::vector<Unlit2DVertex>& vertices,
                              const std::vector<uint16_t>& indices,
                              const glm::mat4& view,
                              const glm::mat4& proj,
                              unsigned int texture,
                              unsigned int blend_mode) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::Sprite2D);
    if (program == 0) return;  // 该后端未提供 sprite2d 内建着色器

    EnsureResources(device);          // 复用 per_frame_ubo_（176B，vp 在首）/ vbo_ / ibo_ / white_tex_
    EnsureUnlit2DResources(device);   // 懒建无光照 2D 混合 PSO
    if (!per_frame_ubo_) return;

    // 顶点已是世界空间（spine runtime computeWorldVertices 已做 2D 蒙皮）；按 Sprite2D 布局打包。
    std::vector<GpuUnlit2DVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const Unlit2DVertex& v = vertices[i];
        GpuUnlit2DVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
    }

    const size_t vbytes = gpu_verts.size() * sizeof(GpuUnlit2DVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // Sprite2D PerFrame 块（FwdPerFrameUBO 与 SpritePerFrameUBO 同布局，着色器仅用 vp）。
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
    };

    unsigned int pso = pso_unlit2d_alpha_;
    if (blend_mode == 1) pso = pso_unlit2d_additive_;
    else if (blend_mode == 2) pso = pso_unlit2d_multiply_;

    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());                       // PerFrame @ set0.b0（仅 vp）
    cmd.BindTexture(0u, texture ? texture : white_tex_, TextureDim::Tex2D); // u_texture @ slot 0
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuUnlit2DVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::Shutdown(RhiDevice& device) {
    if (vbo_) device.DeleteGpuBuffer(vbo_);
    if (ibo_) device.DeleteGpuBuffer(ibo_);
    if (per_frame_ubo_) device.DeleteGpuBuffer(per_frame_ubo_);
    if (per_scene_ubo_) device.DeleteGpuBuffer(per_scene_ubo_);
    if (per_material_ubo_) device.DeleteGpuBuffer(per_material_ubo_);
    if (per_material_shaded_ubo_) device.DeleteGpuBuffer(per_material_shaded_ubo_);
    if (per_point_lights_ubo_) device.DeleteGpuBuffer(per_point_lights_ubo_);
    if (per_terrain_ubo_) device.DeleteGpuBuffer(per_terrain_ubo_);
    if (per_light_probe_ubo_) device.DeleteGpuBuffer(per_light_probe_ubo_);
    if (per_ddgi_ubo_) device.DeleteGpuBuffer(per_ddgi_ubo_);
    if (bone_ssbo_) device.DeleteGpuBuffer(bone_ssbo_);
    if (instance_ssbo_) device.DeleteGpuBuffer(instance_ssbo_);
    if (indirect_buffer_) device.DeleteGpuBuffer(indirect_buffer_);
    vbo_ = ibo_ = per_frame_ubo_ = per_scene_ubo_ = per_material_ubo_ = BufferHandle{};
    per_material_shaded_ubo_ = BufferHandle{};
    per_point_lights_ubo_ = BufferHandle{};
    per_terrain_ubo_ = BufferHandle{};
    per_light_probe_ubo_ = BufferHandle{};
    per_ddgi_ubo_ = BufferHandle{};
    bone_ssbo_ = BufferHandle{};
    instance_ssbo_ = BufferHandle{};
    indirect_buffer_ = BufferHandle{};
    vbo_capacity_ = ibo_capacity_ = bone_ssbo_capacity_ = instance_ssbo_capacity_ = 0;
    init_ = false;
}

} // namespace render
} // namespace dse
