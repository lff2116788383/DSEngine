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

    // ä¸é€æ˜Žå‡ ä½• PSOï¼šå†™/æµ‹æ·±åº¦ï¼ˆLessï¼‰ã€èƒŒé¢å‰”é™¤ã€ä¸æ··åˆã€‚
    PipelineStateDesc desc;
    desc.blend_enabled = false;
    desc.depth_test_enabled = true;
    desc.depth_write_enabled = true;
    desc.depth_func = CompareFunc::Less;
    desc.culling_enabled = true;
    desc.cull_face = CullFace::Back;
    pso_ = device.CreatePipelineState(desc);

    // 1x1 ç™½çº¹ç†ï¼šç¼ºçœçº¹ç†æ§½å›žé€€ï¼ˆé‡‡æ ·å¾— 1.0ï¼Œé…åˆ flags å…³é—­å¯¹åº”è´´å›¾ï¼‰ã€‚
    const unsigned char white[4] = {255, 255, 255, 255};
    white_tex_ = device.CreateTexture2D(1, 1, white, /*linear_filter=*/true);
    // Final-Feat-8: 1x1 ç™½è‰² cubeï¼ˆ6 é¢ï¼‰ï¼Œç‚¹å…‰ shadow cube ç¼ºçœæ§½å›žé€€ã€‚é‡‡æ ·å¾— .r=1.0 â†’
    // closestDepth=radiusï¼Œ(cur-bias)>radius æ’å‡ â†’ ä¸äº§ç”Ÿé˜´å½±ã€‚ä¿è¯ä¸‰åŽç«¯ cube descriptor ç»´åº¦åŒ¹é…ã€‚
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
    // æ— å…‰ç…§ 2D çš„ä¸‰ä¸ªæ··åˆ PSOï¼ˆä¸Ž SpriteBatchRenderer::PsoForBlend ä¸€è‡´ï¼‰ï¼šå…³æ·±åº¦æµ‹è¯•/å†™å…¥/å‰”é™¤ã€‚
    // alpha é»˜è®¤ï¼šcolor = SrcAlpha/OneMinusSrcAlphaï¼Œalpha é€šé“ One/OneMinusSrcAlphaï¼ˆåˆ†ç¦»ï¼‰ã€‚
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
    d_desc.size = sizeof(DrawElementsIndirectCommand);  // å•æ¡é—´æŽ¥ç»˜åˆ¶å‘½ä»¤
    d_desc.usage = GpuBufferUsage::kIndirect;
    d_desc.is_dynamic = true;
    indirect_buffer_ = device.CreateGpuBuffer(d_desc, nullptr);
}

void MeshRenderer::EnsureShadedResources(RhiDevice& device) {
    // ä¸å‰”é™¤ PSOï¼ˆdouble-sided ç”¨ï¼‰ï¼Œä¸Ž pso_ åŒçŠ¶æ€ä½†å…³èƒŒé¢å‰”é™¤ã€‚
    if (pso_no_cull_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = false;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = true;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = false;
        pso_no_cull_ = device.CreatePipelineState(desc);
    }
    // WBOIT accumulation PSOï¼ˆB2c-4ï¼‰ï¼šåŠ æ€§æ··åˆï¼ˆcolor/alpha å‡ ONE/ONEï¼‰ï¼Œæ·±åº¦æµ‹è¯•å¼€ä½†ä¸å†™ã€ä¸å‰”é™¤ï¼Œ
    // ä½¿å„é€æ˜Žç‰‡å…ƒè´¡çŒ®é¡ºåºæ— å…³åœ°ç´¯åŠ ï¼ˆç€è‰²å™¨ wboit_mode=1 è¾“å‡ºé¢„ä¹˜åŠ æƒ color/alphaï¼‰ã€‚
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
    // WBOIT revealage PSOï¼ˆB2c-4ï¼‰ï¼šZERO/ONE_MINUS_SRC_ALPHA ä¹˜æ€§æ··åˆï¼ˆdst *= (1-srcAlpha)ï¼‰ï¼Œ
    // æ·±åº¦æµ‹è¯•å¼€ä½†ä¸å†™ã€ä¸å‰”é™¤ï¼ˆç€è‰²å™¨ wboit_mode=2 è¾“å‡º (0,0,0,alpha)ï¼‰ã€‚
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
    // ç¼–è¾‘å™¨çº¿æ¡†è§†å›¾æ¨¡å¼ PSOï¼ˆé˜¶æ®µ4-M2ï¼‰ï¼šä¸Ž pso_ åŒçŠ¶æ€ï¼ˆå†™/æµ‹æ·±åº¦ã€èƒŒé¢å‰”é™¤ã€ä¸æ··åˆï¼‰ï¼Œä»… wireframe=trueã€‚
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
    // ç¼–è¾‘å™¨ overdraw è§†å›¾æ¨¡å¼ PSOï¼ˆé˜¶æ®µ4-M2ï¼‰ï¼šåŠ æ€§æ··åˆ ONE/ONE + æ·±åº¦æµ‹è¯•å¼€ä½†ä¸å†™ã€ä¸å‰”é™¤ï¼Œ
    // é…åˆ ApplyEditorMaterialOverride çš„å›ºå®šä½Žå¼ºåº¦æè´¨ï¼Œä½¿é‡å ç‰‡å…ƒä»¥äº®åº¦å åŠ æ˜¾ç¤ºè¿‡åº¦ç»˜åˆ¶
    //ï¼ˆä¸Žæ‰§è¡Œå™¨ DX11 SetOverdrawMode / Vulkan overdraw_mode_ è¯­ä¹‰ä¸€è‡´ï¼‰ã€‚
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
    // æ‰©å±• PerMaterial UBOï¼ˆ160Bï¼‰ã€‚
    if (!per_material_shaded_ubo_) {
        GpuBufferDesc m_desc;
        m_desc.size = sizeof(FwdShadedMaterialUBO);
        m_desc.usage = GpuBufferUsage::kUniform;
        m_desc.is_dynamic = true;
        per_material_shaded_ubo_ = device.CreateGpuBuffer(m_desc, nullptr);
    }
    // ç‚¹å…‰ UBOï¼ˆ3088Bï¼Œbinding=3ï¼›count=0 æ—¶é€€åŒ–ä¸ºçº¯æ–¹å‘å…‰ï¼Œè¾“å‡ºä¸Ž B2c-1 ä¸€è‡´ï¼‰ã€‚
    if (!per_point_lights_ubo_) {
        GpuBufferDesc p_desc;
        p_desc.size = sizeof(PointLightsUBO);
        p_desc.usage = GpuBufferUsage::kUniform;
        p_desc.is_dynamic = true;
        per_point_lights_ubo_ = device.CreateGpuBuffer(p_desc, nullptr);
    }
    // åœ°å½¢å‚æ•° UBOï¼ˆ48Bï¼Œslot=4ï¼›splat_enabled=0 ä¸” snow_coverage=0 æ—¶ä¸Ž B2c-2 è¾“å‡ºä¸€è‡´ï¼‰ã€‚
    if (!per_terrain_ubo_) {
        GpuBufferDesc t_desc;
        t_desc.size = sizeof(TerrainParamsUBO);
        t_desc.usage = GpuBufferUsage::kUniform;
        t_desc.is_dynamic = true;
        per_terrain_ubo_ = device.CreateGpuBuffer(t_desc, nullptr);
    }
    // LightProbe SH UBOï¼ˆ160Bï¼Œslot=5ï¼›sh_enabled=0 æ—¶ä¸å½±å“é—´æŽ¥å…‰ï¼ŒB2c-5ï¼‰ã€‚
    if (!per_light_probe_ubo_) {
        GpuBufferDesc lp_desc;
        lp_desc.size = sizeof(LightProbeDataUBO);
        lp_desc.usage = GpuBufferUsage::kUniform;
        lp_desc.is_dynamic = true;
        per_light_probe_ubo_ = device.CreateGpuBuffer(lp_desc, nullptr);
    }
    // DDGI å‚æ•° UBOï¼ˆ64Bï¼Œslot=6ï¼›ddgi_enabled=0 æ—¶ä¸å½±å“é—´æŽ¥å…‰ï¼ŒB2c-5ï¼‰ã€‚
    if (!per_ddgi_ubo_) {
        GpuBufferDesc d_desc;
        d_desc.size = sizeof(FwdDDGIParamsUBO);
        d_desc.usage = GpuBufferUsage::kUniform;
        d_desc.is_dynamic = true;
        per_ddgi_ubo_ = device.CreateGpuBuffer(d_desc, nullptr);
    }
    // èšå…‰ç¯ UBOï¼ˆ4112Bï¼Œset7.b1/slot=7ï¼›count=0 æ—¶æ— èšå…‰ç¯è´¡çŒ®ï¼Œè¾“å‡ºä¸Ž Final-Feat-3 ä¸€è‡´ï¼‰ã€‚
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

    // --- Shadow-cull é¢„ç®—ï¼ˆä»… depth-only + ortho é˜´å½± passï¼›å¸¸æ•°/ç®—æ³•ä¸Žä¸‰åŽç«¯æ‰§è¡Œå™¨é€ä½ä¸€è‡´ï¼‰---
    // is_ortho ç”¨ proj[2][3]â‰ˆ0 åˆ¤å®šï¼ˆä¸Žæ‰§è¡Œå™¨åŒå¼ï¼šperspective=-1/ortho=0ï¼›clip ä¿®æ­£ä¸æ”¹è¯¥å…ƒç´ ï¼‰ã€‚
    const bool is_ortho = std::abs(proj[2][3]) < 0.01f;
    const bool shadow_cull_active = depth_only && is_ortho;
    // PreZï¼ˆé€è§† depth-onlyï¼‰ï¼šè’™çš®å®žä¾‹ VS éª¨éª¼å¼€é”€æžå¤§ã€é˜´å½±æ”¶ç›Šä½Žï¼Œæ•´ä½“è·³è¿‡ï¼ˆä¸Žæ‰§è¡Œå™¨ä¸€è‡´ï¼‰ã€‚
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
        // é¡¶ç‚¹/ç´¢å¼•æ•°æ®æºï¼šä¼˜å…ˆ shared_vertex_ptrï¼ˆå…±äº«æ¨¡æ¿ï¼‰ï¼Œå¦åˆ™ item å†…è”ç¼“å†²ï¼ˆä¸Žæ‰§è¡Œå™¨åŒåºï¼‰ã€‚
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

        // ç´¢å¼• uint32 â†’ uint16ï¼ˆMeshRenderer é€å˜ä½“æ–¹æ³•å¥‘çº¦ä¸º 16 ä½ï¼›cpu_mesh é¡¶ç‚¹æ•° < 65536ï¼‰ã€‚
        std::vector<uint16_t> indices16(idx_count);
        for (size_t k = 0; k < idx_count; ++k)
            indices16[k] = static_cast<uint16_t>(idx_data[k]);

        const ShadedMaterial material = BatchToShadedMaterial(item);
        const DirectionalLight light = BatchToDirLight(item);
        const std::vector<ShadedPointLight> point_lights = BatchToPointLights(item);
        const std::vector<ShadedSpotLight> spot_lights = BatchToSpotLights(item);
        const ShadedGI gi{};  // ä¸Žå·²è¿ç§»çš„ terrain/tree/grass ä¸€è‡´ï¼šCPU mesh forward è·¯å¾„ä¸å¸¦ DDGI/SHã€‚

        // --- å®žä¾‹å¯è§é›†ï¼ˆä»… instancedï¼‰ï¼šdepth-only ortho é˜´å½± pass æŒ‰é¢„ç®— + lightspace è£å‰ª ---
        std::vector<glm::mat4> vis_models;
        std::vector<int> vis_palette_idx;
        if (is_instanced) {
            if (prez_skip_skinned && skinned_instanced) continue;  // PreZ è·³è¿‡è’™çš®å®žä¾‹
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
            if (vis_models.empty()) continue;  // å…¨éƒ¨è¢«å‰”é™¤
        }

        // ===== GBuffer æ¨¡å¼ï¼ˆRSMï¼›éž depth-onlyï¼‰ï¼šè’™çš®/å®žä¾‹åœ¨ CPU å±•å¼€ä¸ºé™æ€ä¸–ç•Œå‡ ä½• =====
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

        // ===== forward / depth-onlyï¼šå¤ç”¨ forward programï¼ˆdepth-only RT æ— é¢œè‰²é™„ä»¶ â†’ frag ä¸¢å¼ƒï¼‰=====
        if (skinned_instanced) {
            std::vector<SkinnedMeshVertex> sverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) sverts[i] = BatchToSkinnedVertex(vtx_data[i]);
            DrawSkinnedInstancedShaded(cmd, device, sverts, indices16, vis_models,
                                       item.bone_palette, vis_palette_idx, view, proj, camera_pos,
                                       material, light, point_lights, gi, spot_lights);
        } else if (is_instanced) {
            std::vector<MeshVertex> mverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) mverts[i] = BatchToMeshVertex(vtx_data[i]);
            DrawInstancedShaded(cmd, device, mverts, indices16, vis_models, view, proj, camera_pos,
                                material, light, point_lights, gi, spot_lights);
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
    // å±€éƒ¨ç©ºé—´æ‰“åŒ…ï¼ˆä¸åš model é¢„å˜æ¢ï¼›æ¯å®žä¾‹ model ç”± VS æŒ‰ gl_InstanceIndex å˜æ¢ï¼‰ã€‚
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
    vb_desc.is_dynamic = false;  // å¸¸é©»é™æ€æ¨¡æ¿ç¼“å†²ï¼ˆå¤šå®žä¾‹/å¤šå¸§å…±äº«ï¼‰
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
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾›è’™çš® forward PBR å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- é¡¶ç‚¹æ‰“åŒ…ï¼ˆå±€éƒ¨/ç»‘å®šç©ºé—´ï¼ŒVS æ–½éª¨éª¼æ··åˆ + vpï¼Œä¸åœ¨ CPU é¢„å˜æ¢ï¼‰ ---
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

    // --- éª¨éª¼çŸ©é˜µï¼šå·¦ä¹˜ model å¾—ä¸–ç•Œç©ºé—´ï¼Œå†™å…¥ SSBO ---
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

    // --- UBO å¡«å……ï¼ˆä¸Žé™æ€è·¯å¾„åŒæž„ï¼‰ ---
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
    // éª¨éª¼çŸ©é˜µ SSBO\@slot 0ï¼ˆä¸‰åŽç«¯é€šç”¨è¯­ä¹‰ï¼šGL binding0 / Vulkan ä½ç½®0 / DX11 t0 ç» @SSBO_LOW_REGISTERSï¼‰ã€‚
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
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾› forward PBR å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- CPU ä¾§é¢„å˜æ¢é¡¶ç‚¹åˆ°ä¸–ç•Œç©ºé—´ ---
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

    // --- UBO å¡«å…… ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);  // L = æŒ‡å‘å…‰æº
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

    // --- çº¹ç†ï¼ˆç¼ºçœå›žé€€åˆ°ç™½çº¹ç†ï¼›flat unit 0..4ï¼‰ ---
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
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾› sprite2d å†…å»ºç€è‰²å™¨

    EnsureResources(device);          // å¤ç”¨ per_frame_ubo_ï¼ˆ176Bï¼Œvp åœ¨é¦–ï¼‰/ vbo_ / ibo_ / white_tex_
    EnsureUnlit2DResources(device);   // æ‡’å»ºæ— å…‰ç…§ 2D æ··åˆ PSO
    if (!per_frame_ubo_) return;

    // é¡¶ç‚¹å·²æ˜¯ä¸–ç•Œç©ºé—´ï¼ˆspine runtime computeWorldVertices å·²åš 2D è’™çš®ï¼‰ï¼›æŒ‰ Sprite2D å¸ƒå±€æ‰“åŒ…ã€‚
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

    // Sprite2D PerFrame å—ï¼ˆFwdPerFrameUBO ä¸Ž SpritePerFrameUBO åŒå¸ƒå±€ï¼Œç€è‰²å™¨ä»…ç”¨ vpï¼‰ã€‚
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
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());                       // PerFrame @ set0.b0ï¼ˆä»… vpï¼‰
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
