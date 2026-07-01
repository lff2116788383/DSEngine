/**
 * @file mesh_renderer_instancing.cpp
 * @brief MeshRenderer hardware instancing and indirect rendering methods.
 */

#include "engine/render/mesh_renderer_internal.h"

using namespace dse::render::mesh_internal;

namespace dse {
namespace render {
void MeshRenderer::DrawSharedTemplateInstanced(CommandBuffer& cmd, RhiDevice& device,
                                               const ExternalShadedMesh& tmpl,
                                               uint32_t index_count,
                                               uint32_t first_index,
                                               const std::vector<glm::mat4>& instance_models,
                                               const glm::mat4& view,
                                               const glm::mat4& proj,
                                               const glm::vec3& camera_pos,
                                               const ShadedMaterial& material,
                                               const DirectionalLight& light,
                                               const std::vector<ShadedPointLight>& point_lights,
                                               const ShadedGI& gi,
                                               const std::vector<ShadedSpotLight>& spot_lights) {
    if (index_count == 0 || instance_models.empty() ||
        !tmpl.vertex_buffer || !tmpl.index_buffer) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedShaded);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾›å®žä¾‹åŒ–é«˜çº§ shading å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // æ¯å®žä¾‹ model çŸ©é˜µå†™å…¥å†…éƒ¨å®žä¾‹ SSBOï¼ˆä¸–ç•Œç©ºé—´ï¼Œ0 åŸºç´¢å¼•ï¼‰ã€‚å…±äº«çš„æ˜¯ã€Œé¡¶ç‚¹æ¨¡æ¿ã€ï¼Œå®žä¾‹çŸ©é˜µä»é€æ¬¡æäº¤ã€‚
    const size_t inst_bytes = instance_models.size() * sizeof(glm::mat4);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, instance_models.data());

    // å…±äº«æ¨¡æ¿ VB/IB ç”± BuildShadedLocalVertexBuffer + caller æä¾›ï¼Œå¸¸é©»ä¸é‡ä¼ ã€‚
    // --- UBO å¡«å……ï¼ˆä¸Ž DrawInstancedShaded åŒæºï¼‰---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    // æ¤è¢«é£Žå¼¯æ›²ï¼ˆB2b-6ï¼Œtreeï¼‰ï¼šmaterial.foliage æ—¶å–‚å…¥ grs é£Žå‚ï¼Œå¦åˆ™ä¿æŒé›¶ â†’ VS æ•´æ®µè·³è¿‡ã€‚
    if (material.foliage) {
        const auto& grs_f = device.GetGlobalRenderState();
        frame.foliage_wind = grs_f.foliage_wind;
        frame.foliage_push = grs_f.foliage_push;
    }
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    const auto& grs = device.GetGlobalRenderState();
    scene.light_params = glm::vec4(light.intensity,
                                   material.shadow_strength,
                                   material.receive_shadow ? 1.0f : 0.0f, 0.0f);
    scene.cascade_splits = glm::vec4(grs.cascade_splits[0], grs.cascade_splits[1],
                                     grs.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        scene.light_space_matrices[i] = grs.light_space_matrix[i];
        scene.shadow_atlas_regions[i] = grs.shadow_atlas_region[i];
    }
    // Final-Feat-8: èšå…‰ç¯ light-space çŸ©é˜µï¼ˆç‚¹/èšå…‰é˜´å½±æŽ¥æ”¶ï¼Œä¸Žæ‰§è¡Œå™¨ DrawMeshBatch åŒæºï¼‰ã€‚
    for (int i = 0; i < 4; ++i)
        scene.spot_light_space_matrices[i] = grs.spot_light_space_matrix[i];
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdShadedMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    mat.mode_params = glm::vec4(static_cast<float>(material.shading_mode),
                                material.double_sided ? 1.0f : 0.0f,
                                material.anisotropy, material.pom_height_scale);
    mat.sss = glm::vec4(material.sss_tint, material.sss_strength);
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    PointLightsUBO plights{};
    const int pl_count = static_cast<int>(
        (std::min)(point_lights.size(), static_cast<size_t>(kMaxPointLightsUBO)));
    plights.u_point_light_count = pl_count;
    for (int i = 0; i < pl_count; ++i) {
        const ShadedPointLight& pl = point_lights[i];
        plights.u_point_lights[i].color = pl.color;
        plights.u_point_lights[i].intensity = pl.intensity;
        plights.u_point_lights[i].position = pl.position;
        plights.u_point_lights[i].radius = pl.radius;
        plights.u_point_lights[i].cast_shadow = pl.cast_shadow ? 1 : 0;
        plights.u_point_lights[i].shadow_index = pl.shadow_index;
    }
    device.UpdateGpuBuffer(per_point_lights_ubo_, 0, sizeof(plights), &plights);

    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    SpotLightsUBO slights{};
    FillSpotLightsUBO(spot_lights, slights);
    device.UpdateGpuBuffer(per_spot_lights_ubo_, 0, sizeof(slights), &slights);

    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    unsigned int pso = SelectShadedPso(device, material);
    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    cmd.BindTexture(11u, tex_or_white(grs.shadow_map[0]), TextureDim::Tex2D);
    // Final-Feat-8: èšå…‰ç¯ 2D é˜´å½±å›¾ï¼ˆflat unit 12-15ï¼‰/ ç‚¹å…‰ cube é˜´å½±ï¼ˆflat unit 16-19ï¼‰ã€‚
    // æœªç”¨æ§½ä½ç»‘é»˜è®¤ç™½çº¹ç†/ç™½ cubeï¼ˆé‡‡æ ·å¾—æ·±åº¦=1 â†’ ä¸äº§ç”Ÿé˜´å½±ï¼‰ï¼Œä¿è¯ä¸‰åŽç«¯ descriptor ç»´åº¦åŒ¹é…ã€‚
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1
    // æ¯å®žä¾‹ model SSBO\@slot 0ï¼ˆä¸Ž DrawInstancedShaded åŒæºï¼‰ã€‚
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    // å…±äº«å±€éƒ¨ç©ºé—´æ¨¡æ¿ VB/IBï¼ˆcaller æŒæœ‰ã€å¸¸é©»ï¼‰ï¼ŒæŒ‰ index_count_override å­æ®µå¯¹æ¯å®žä¾‹ç»˜åˆ¶ã€‚
    cmd.BindVertexBuffer(0u, tmpl.vertex_buffer.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(tmpl.index_buffer.raw(), tmpl.index_type);
    // å¥‘çº¦ï¼šfirst_instance æ’ 0ï¼ŒDX11 SV_InstanceID ä»Ž 0 èµ·ï¼Œåç§»å·²ç”± 0 åŸº SSBO ç´¢å¼•è¡¨è¾¾ã€‚
    cmd.DrawIndexedInstanced(index_count, static_cast<uint32_t>(instance_models.size()),
                             first_index, 0, 0u);
}


void MeshRenderer::DrawInstanced(CommandBuffer& cmd, RhiDevice& device,
                                 const std::vector<MeshVertex>& vertices,
                                 const std::vector<uint16_t>& indices,
                                 const std::vector<glm::mat4>& instance_models,
                                 const glm::mat4& view,
                                 const glm::mat4& proj,
                                 const glm::vec3& camera_pos,
                                 const MeshMaterial& material,
                                 const DirectionalLight& light) {
    if (vertices.empty() || indices.empty() || instance_models.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbrInstanced);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾›å®žä¾‹åŒ– forward PBR å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- é¡¶ç‚¹æ‰“åŒ…ï¼ˆå±€éƒ¨ç©ºé—´ï¼ŒVS æŒ‰å®žä¾‹ model å˜æ¢ï¼Œä¸åœ¨ CPU é¢„å˜æ¢ï¼‰ ---
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

    // --- æ¯å®žä¾‹ model çŸ©é˜µå†™å…¥ SSBOï¼ˆä¸–ç•Œç©ºé—´ï¼Œ0 åŸºç´¢å¼•ï¼‰ ---
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
    // æ¯å®žä¾‹ model SSBO\@slot 0ï¼ˆä¸‰åŽç«¯é€šç”¨è¯­ä¹‰ï¼šGL binding0 / Vulkan ä½ç½®0 / DX11 t0 ç» @SSBO_LOW_REGISTERSï¼‰ã€‚
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // å¥‘çº¦ï¼šfirst_instance æ’ 0ï¼ŒDX11 SV_InstanceID ä»Ž 0 èµ·ï¼Œåç§»å·²ç”± 0 åŸº SSBO ç´¢å¼•è¡¨è¾¾ã€‚
    cmd.DrawIndexedInstanced(static_cast<uint32_t>(indices.size()),
                             static_cast<uint32_t>(instance_models.size()),
                             0u, 0, 0u);
}

void MeshRenderer::DrawIndirect(CommandBuffer& cmd, RhiDevice& device,
                                const std::vector<MeshVertex>& vertices,
                                const std::vector<uint16_t>& indices,
                                const std::vector<glm::mat4>& instance_models,
                                const glm::mat4& view,
                                const glm::mat4& proj,
                                const glm::vec3& camera_pos,
                                const MeshMaterial& material,
                                const DirectionalLight& light) {
    if (vertices.empty() || indices.empty() || instance_models.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbrInstanced);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾›å®žä¾‹åŒ– forward PBR å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- é¡¶ç‚¹æ‰“åŒ…ï¼ˆå±€éƒ¨ç©ºé—´ï¼ŒVS æŒ‰å®žä¾‹ model å˜æ¢ï¼Œä¸Ž DrawInstanced åŒæž„ï¼‰ ---
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

    // --- æ¯å®žä¾‹ model çŸ©é˜µå†™å…¥ SSBOï¼ˆä¸–ç•Œç©ºé—´ï¼Œ0 åŸºç´¢å¼•ï¼‰ ---
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

    // --- é—´æŽ¥ç»˜åˆ¶å‘½ä»¤å†™å…¥ indirect bufferï¼ˆCPU ç«¯å¡«ï¼ŒGPU-driven pass äº¦å¯å›žå†™ï¼‰ ---
    // base_instance æ’ 0ï¼šDX11 SV_InstanceID ä»Ž 0 èµ·ï¼Œåç§»å·²ç”± 0 åŸº SSBO ç´¢å¼•è¡¨è¾¾ï¼ˆÂ§6ï¼‰ã€‚
    EnsureIndirectBuffer(device);
    if (!indirect_buffer_) return;
    DrawElementsIndirectCommand draw_cmd{};
    draw_cmd.count = static_cast<uint32_t>(indices.size());
    draw_cmd.instance_count = static_cast<uint32_t>(instance_models.size());
    draw_cmd.first_index = 0u;
    draw_cmd.base_vertex = 0;
    draw_cmd.base_instance = 0u;
    device.UpdateGpuBuffer(indirect_buffer_, 0, sizeof(draw_cmd), &draw_cmd);

    // --- UBO å¡«å……ï¼ˆä¸Žé™æ€/å®žä¾‹åŒ–è·¯å¾„åŒæž„ï¼‰ ---
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
    // æ¯å®žä¾‹ model SSBO\@slot 0ï¼ˆä¸Ž DrawInstanced åŒè¯­ä¹‰ï¼‰ã€‚
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // é—´æŽ¥ç»˜åˆ¶ï¼šç»˜åˆ¶å‚æ•°å–è‡ª indirect buffer åç§» 0 å¤„çš„ DrawElementsIndirectCommandã€‚
    cmd.DrawIndexedIndirect(indirect_buffer_.raw(), 0u);
}

} // namespace render
} // namespace dse
