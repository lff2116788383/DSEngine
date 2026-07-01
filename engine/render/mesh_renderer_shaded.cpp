/**
 * @file mesh_renderer_shaded.cpp
 * @brief MeshRenderer advanced shading methods (PBR, skinned, morph).
 */

#include "engine/render/mesh_renderer_internal.h"

using namespace dse::render::mesh_internal;

namespace dse {
namespace render {
void MeshRenderer::DrawShaded(CommandBuffer& cmd, RhiDevice& device,
                              const std::vector<MeshVertex>& vertices,
                              const std::vector<uint16_t>& indices,
                              const glm::mat4& model,
                              const glm::mat4& view,
                              const glm::mat4& proj,
                              const glm::vec3& camera_pos,
                              const ShadedMaterial& material,
                              const DirectionalLight& light,
                              const std::vector<ShadedPointLight>& point_lights,
                              const ShadedGI& gi,
                              const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardShaded);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾›é«˜çº§ shading å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- CPU ä¾§é¢„å˜æ¢é¡¶ç‚¹åˆ°ä¸–ç•Œç©ºé—´ï¼ˆä¸Ž Draw ä¸€è‡´ï¼‰---
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
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    // CSM æ–¹å‘å…‰é˜´å½±ï¼šä»Ž device å…¨å±€æ¸²æŸ“çŠ¶æ€å–çº§è”çŸ©é˜µ/åˆ†è£‚/atlas åŒºåŸŸï¼ˆä¸Žæ‰§è¡Œå™¨ DrawMeshBatch åŒæºï¼‰ã€‚
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
    // clearcoat.z å¤ç”¨ä¸º wboit_modeï¼ˆB2c-4ï¼‰ï¼šç€è‰²å™¨æ®æ­¤åˆ‡æ¢ accumulation/revealage è¾“å‡ºã€‚
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    // ç‚¹å…‰ UBOï¼ˆclustered â‰¤64ï¼Œè¶…å‡ºæˆªæ–­ï¼‰ã€‚
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

    // åœ°å½¢å‚æ•° UBOï¼ˆsplat 4 å±‚ + ç§¯é›ªï¼›å‡å…³é—­æ—¶è¾“å‡ºä¸Ž B2c-2 ä¸€è‡´ï¼‰ã€‚
    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    // LightProbe SH UBOï¼ˆslot=5ï¼‰ã€‚sh_enabled=0 æ—¶ probe_params.x=0 â†’ ç€è‰²å™¨ä¸å– SHã€‚
    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    // DDGI å‚æ•° UBOï¼ˆslot=6ï¼‰ã€‚ddgi_enabled=0 æˆ–æ—  atlas æ—¶ origin.w=0 â†’ ç€è‰²å™¨ä¸å– DDGIã€‚
    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    // èšå…‰ç¯ UBOï¼ˆset7.b1/slot=7ï¼ŒFinal-Feat-4ï¼‰ã€‚count=0 æ—¶ç€è‰²å™¨æ— èšå…‰ç¯è´¡çŒ®ã€‚
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

    // PSO é€‰æ‹©ï¼šWBOIT é€æ˜Žé€šé“ä¼˜å…ˆï¼ˆaccumulation/revealageï¼‰ï¼Œå¦åˆ™æŒ‰ double-sided é€‰å‰”é™¤çŠ¶æ€ã€‚
    unsigned int pso = SelectShadedPso(device, material);
    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0ï¼ˆæ‰©å±•ï¼‰
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0ï¼ˆB2c-2ï¼‰
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0ï¼ˆB2c-3ï¼‰
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0ï¼ˆB2c-5ï¼‰
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0ï¼ˆB2c-5ï¼‰
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1ï¼ˆFinal-Feat-4ï¼‰
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    // åœ°å½¢ splat çº¹ç†ï¼ˆslot 5-9ï¼Œflat unit 5-9ï¼‰ã€‚æœªç”¨æ—¶ç»‘ç™½çº¹ç†ä¿è¯ä¸‰åŽç«¯ descriptor æœ‰å®šä¹‰ã€‚
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    // DDGI irradiance atlasï¼ˆslot 10ï¼Œflat unit 10ï¼‰ã€‚æœªå¯ç”¨æ—¶ç»‘ç™½çº¹ç†ä¿è¯ä¸‰åŽç«¯ descriptor æœ‰å®šä¹‰ã€‚
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    // CSM shadow atlasï¼ˆslot 11ï¼Œflat unit 11ï¼‰ã€‚æ—  shadow mapï¼ˆgrs.shadow_map[0]==0ï¼‰æˆ– receive_shadow å…³æ—¶
    // ç»‘ç™½çº¹ç†ï¼šç€è‰²å™¨å†… receive_shadow é—¨æŽ§å·²è¿”å›ž 0ï¼Œç™½çº¹ç†æ·±åº¦=1 ä¹Ÿä¸ä¼šäº§ç”Ÿé˜´å½±ã€‚
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
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

BufferHandle MeshRenderer::BuildShadedWorldVertexBuffer(RhiDevice& device,
                                                        const std::vector<MeshVertex>& vertices,
                                                        const glm::mat4& model) {
    if (vertices.empty()) return BufferHandle{};
    // CPU ä¾§é¢„å˜æ¢åˆ°ä¸–ç•Œç©ºé—´ï¼ˆä¸Ž DrawShaded å®Œå…¨åŒæºï¼šä½ç½® modelã€æ³•çº¿ normal-matrixã€åˆ‡çº¿ model çº¿æ€§éƒ¨åˆ†ï¼‰ã€‚
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
    GpuBufferDesc vb_desc;
    vb_desc.size = gpu_verts.size() * sizeof(GpuMeshVertex);
    vb_desc.usage = GpuBufferUsage::kVertex;
    vb_desc.is_dynamic = false;  // å¸¸é©»é™æ€ç¼“å†²ï¼ˆtiled terrainï¼šä¸Šä¼ ä¸€æ¬¡ï¼Œå¤š tile å­æ®µå¤ç”¨ï¼‰
    return device.CreateGpuBuffer(vb_desc, gpu_verts.data());
}

void MeshRenderer::DrawShadedExternal(CommandBuffer& cmd, RhiDevice& device,
                                      const ExternalShadedMesh& mesh,
                                      uint32_t index_count,
                                      uint32_t first_index,
                                      const glm::mat4& view,
                                      const glm::mat4& proj,
                                      const glm::vec3& camera_pos,
                                      const ShadedMaterial& material,
                                      const DirectionalLight& light,
                                      const std::vector<ShadedPointLight>& point_lights,
                                      const ShadedGI& gi,
                                      const std::vector<ShadedSpotLight>& spot_lights) {
    if (index_count == 0 || !mesh.vertex_buffer || !mesh.index_buffer) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardShaded);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾›é«˜çº§ shading å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // å¤–éƒ¨å¸¸é©» VB/IBï¼šé¡¶ç‚¹å·²æ˜¯ä¸–ç•Œç©ºé—´ï¼ˆBuildShadedWorldVertexBuffer é¢„å˜æ¢ï¼‰ï¼Œæ•…ä¸åœ¨æ­¤é‡ä¼ /é¢„å˜æ¢ã€‚
    // --- UBO å¡«å……ï¼ˆä¸Ž DrawShaded åŒæºï¼‰---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
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
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());
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
    // å¤–éƒ¨å¸¸é©»ç¼“å†²ï¼šç»‘ caller æŒæœ‰çš„ VB/IBï¼ŒæŒ‰ index_count_override å­æ®µç»˜åˆ¶ã€‚
    cmd.BindVertexBuffer(0u, mesh.vertex_buffer.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(mesh.index_buffer.raw(), mesh.index_type);
    cmd.DrawIndexed(index_count, first_index, 0);
}

void MeshRenderer::DrawGBuffer(CommandBuffer& cmd, RhiDevice& device,
                               const std::vector<MeshVertex>& vertices,
                               const std::vector<uint16_t>& indices,
                               const glm::mat4& model,
                               const glm::mat4& view,
                               const glm::mat4& proj,
                               unsigned int albedo_tex) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::GBufferMesh);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾› GBuffer-mesh å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_) return;

    // --- CPU ä¾§é¢„å˜æ¢é¡¶ç‚¹åˆ°ä¸–ç•Œç©ºé—´ï¼ˆä¸Ž DrawShaded åŒæºï¼›gPosition ç›´æŽ¥å–æ­¤ä¸–ç•Œåæ ‡ï¼‰---
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

    // PerFrameï¼ˆset0.b0ï¼‰ï¼švp = proj*viewï¼Œgbuffer.frag ä¸è¯»ä½†é¡»ä¿æŒ descriptor layout å…¼å®¹ã€‚
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(0.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    // PerSceneï¼ˆset1.b0ï¼‰ï¼šå ä½ï¼Œä»…ä¸ºä¿æŒä¸Ž PBR ç®¡çº¿ descriptor layout ä¸€è‡´ã€‚
    FwdPerSceneUBO scene{};
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    // ä¸é€æ˜Žå‡ ä½• PSOï¼ˆå†™/æµ‹æ·±åº¦ã€èƒŒé¢å‰”é™¤ã€ä¸æ··åˆï¼‰ï¼šMRT å„ attachment å…±ç”¨æ­¤å…³æ··åˆçŠ¶æ€ã€‚
    cmd.BindPipeline(device.GetGraphicsPipeline(pso_, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());  // PerFrame @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());  // PerScene @ set1.b0ï¼ˆå ä½ï¼‰
    cmd.BindTexture(0u, albedo_tex ? albedo_tex : white_tex_, TextureDim::Tex2D);  // u_texture @ set2.b1
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}


void MeshRenderer::DrawSkinnedShaded(CommandBuffer& cmd, RhiDevice& device,
                                     const std::vector<SkinnedMeshVertex>& vertices,
                                     const std::vector<uint16_t>& indices,
                                     const glm::mat4& model,
                                     const std::vector<glm::mat4>& bone_matrices,
                                     const glm::mat4& view,
                                     const glm::mat4& proj,
                                     const glm::vec3& camera_pos,
                                     const ShadedMaterial& material,
                                     const DirectionalLight& light,
                                     const std::vector<ShadedPointLight>& point_lights,
                                     const ShadedGI& gi,
                                     const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty() || bone_matrices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardSkinnedShaded);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾›è’™çš®é«˜çº§ shading å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

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

    // --- UBO å¡«å……ï¼ˆä¸Ž DrawShaded åŒæž„ï¼‰ ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
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

    // èšå…‰ç¯ UBOï¼ˆset7.b1/slot=7ï¼ŒFinal-Feat-4ï¼‰ã€‚count=0 æ—¶ç€è‰²å™¨æ— èšå…‰ç¯è´¡çŒ®ã€‚
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
        VertexAttr{5u, 4u, 60u},   // bone indices
        VertexAttr{6u, 4u, 76u},   // bone weights
    };

    // PSO é€‰æ‹©ï¼šä¸Ž DrawShaded ä¸€è‡´ï¼ˆWBOIT é€æ˜Žä¼˜å…ˆï¼Œå¦åˆ™æŒ‰ double-sided é€‰å‰”é™¤ï¼‰ã€‚
    unsigned int pso = SelectShadedPso(device, material);
    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0ï¼ˆæ‰©å±•ï¼‰
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0
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
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1ï¼ˆFinal-Feat-4ï¼‰
    // éª¨éª¼çŸ©é˜µ SSBO\@slot 0ï¼ˆä¸‰åŽç«¯é€šç”¨ï¼šGL binding0 / Vulkan ä½ç½®0(set7) / DX11 t0 ç» @SSBO_LOW_REGISTERSï¼‰ã€‚
    cmd.BindStorageBuffer(0u, bone_ssbo_.raw(), 0u, static_cast<uint32_t>(bone_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuSkinnedVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::DrawInstancedShaded(CommandBuffer& cmd, RhiDevice& device,
                                       const std::vector<MeshVertex>& vertices,
                                       const std::vector<uint16_t>& indices,
                                       const std::vector<glm::mat4>& instance_models,
                                       const glm::mat4& view,
                                       const glm::mat4& proj,
                                       const glm::vec3& camera_pos,
                                       const ShadedMaterial& material,
                                       const DirectionalLight& light,
                                       const std::vector<ShadedPointLight>& point_lights,
                                       const ShadedGI& gi,
                                       const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty() || instance_models.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedShaded);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾›å®žä¾‹åŒ–é«˜çº§ shading å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

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

    // --- UBO å¡«å……ï¼ˆä¸Ž DrawShaded åŒæž„ï¼‰ ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    // æ¤è¢«é£Žå¼¯æ›²ï¼ˆB2b-6ï¼Œgrassï¼‰ï¼šmaterial.foliage æ—¶å–‚å…¥ grs é£Žå‚ï¼Œå¦åˆ™ä¿æŒé›¶ â†’ VS æ•´æ®µè·³è¿‡ã€‚
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

    // èšå…‰ç¯ UBOï¼ˆset7.b1/slot=7ï¼ŒFinal-Feat-4ï¼‰ã€‚count=0 æ—¶ç€è‰²å™¨æ— èšå…‰ç¯è´¡çŒ®ã€‚
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

    // PSO é€‰æ‹©ï¼šä¸Ž DrawShaded ä¸€è‡´ï¼ˆWBOIT é€æ˜Žä¼˜å…ˆï¼Œå¦åˆ™æŒ‰ double-sided é€‰å‰”é™¤ï¼‰ã€‚
    unsigned int pso = SelectShadedPso(device, material);
    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0ï¼ˆæ‰©å±•ï¼‰
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0
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
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1ï¼ˆFinal-Feat-4ï¼‰
    // æ¯å®žä¾‹ model SSBO\@slot 0ï¼ˆä¸‰åŽç«¯é€šç”¨ï¼šGL binding0 / Vulkan ä½ç½®0(set7) / DX11 t0 ç» @SSBO_LOW_REGISTERSï¼‰ã€‚
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // å¥‘çº¦ï¼šfirst_instance æ’ 0ï¼ŒDX11 SV_InstanceID ä»Ž 0 èµ·ï¼Œåç§»å·²ç”± 0 åŸº SSBO ç´¢å¼•è¡¨è¾¾ã€‚
    cmd.DrawIndexedInstanced(static_cast<uint32_t>(indices.size()),
                             static_cast<uint32_t>(instance_models.size()),
                             0u, 0, 0u);
}

void MeshRenderer::DrawSkinnedInstancedShaded(CommandBuffer& cmd, RhiDevice& device,
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
                                              const std::vector<ShadedPointLight>& point_lights,
                                              const ShadedGI& gi,
                                              const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty() || instance_models.empty() ||
        bone_palettes.empty() || instance_palette_idx.size() != instance_models.size()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardSkinnedInstancedShaded);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾›è’™çš®Ã—å®žä¾‹åŒ–é«˜çº§ shading å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- é¡¶ç‚¹æ‰“åŒ…ï¼ˆå±€éƒ¨/ç»‘å®šç©ºé—´ï¼ŒVS æ–½éª¨éª¼æ··åˆ + æ¯å®žä¾‹ model + vpï¼Œä¸åœ¨ CPU é¢„å˜æ¢ï¼‰ ---
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

    // --- éª¨éª¼è°ƒè‰²æ¿åŽ»é‡å¯†æŽ’ï¼ˆç»‘å®šâ†’å±€éƒ¨ç©ºé—´ï¼Œä¸é¢„ä¹˜ modelï¼‰ï¼Œè®°æ¯ä»½è°ƒè‰²æ¿èµ·å§‹ä¸‹æ ‡ ---
    std::vector<int> palette_base(bone_palettes.size());
    std::vector<glm::mat4> packed_bones;
    for (size_t p = 0; p < bone_palettes.size(); ++p) {
        palette_base[p] = static_cast<int>(packed_bones.size());
        packed_bones.insert(packed_bones.end(), bone_palettes[p].begin(), bone_palettes[p].end());
    }
    if (packed_bones.empty()) return;
    const size_t bone_bytes = packed_bones.size() * sizeof(glm::mat4);
    EnsureBoneCapacity(device, bone_bytes);
    if (!bone_ssbo_) return;
    device.UpdateGpuBuffer(bone_ssbo_, 0, bone_bytes, packed_bones.data());

    // --- æ¯å®žä¾‹ {model, bone_offset}ï¼ˆ80B MeshSkinnedInstï¼‰å†™å…¥å®žä¾‹ SSBOï¼ˆ0 åŸºç´¢å¼•ï¼‰ ---
    std::vector<MeshSkinnedInstGPU> insts(instance_models.size());
    for (size_t j = 0; j < instance_models.size(); ++j) {
        const int pi = instance_palette_idx[j];
        if (pi < 0 || pi >= static_cast<int>(palette_base.size())) return;  // è°ƒè‰²æ¿ä¸‹æ ‡è¶Šç•Œï¼Œæ•´æ‰¹æ”¾å¼ƒ
        insts[j].model = instance_models[j];
        insts[j].bone_offset = palette_base[pi];
        insts[j].pad0 = insts[j].pad1 = insts[j].pad2 = 0;
    }
    const size_t inst_bytes = insts.size() * sizeof(MeshSkinnedInstGPU);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, insts.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuSkinnedVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- UBO å¡«å……ï¼ˆä¸Ž DrawShaded / DrawInstancedShaded åŒæž„ï¼‰ ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
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

    // èšå…‰ç¯ UBOï¼ˆset7.b1/slot=7ï¼ŒFinal-Feat-4ï¼‰ã€‚count=0 æ—¶ç€è‰²å™¨æ— èšå…‰ç¯è´¡çŒ®ã€‚
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
        VertexAttr{5u, 4u, 60u},   // bone indices
        VertexAttr{6u, 4u, 76u},   // bone weights
    };

    // PSO é€‰æ‹©ï¼šä¸Ž DrawShaded ä¸€è‡´ï¼ˆWBOIT é€æ˜Žä¼˜å…ˆï¼Œå¦åˆ™æŒ‰ double-sided é€‰å‰”é™¤ï¼‰ã€‚
    unsigned int pso = SelectShadedPso(device, material);
    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0ï¼ˆæ‰©å±•ï¼‰
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0
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
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1ï¼ˆFinal-Feat-4ï¼‰
    // å®žä¾‹ SSBO\@slot0ï¼ˆset8.b0ï¼‰+ éª¨éª¼ SSBO\@slot1ï¼ˆset8.b1ï¼‰ï¼›ä¸‰åŽç«¯ @SSBO_LOW_REGISTERS â†’ DX11 t0/t1ã€GL binding0/1ã€Vulkan rank0/1ã€‚
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindStorageBuffer(1u, bone_ssbo_.raw(), 0u, static_cast<uint32_t>(bone_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuSkinnedVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // å¥‘çº¦ï¼šfirst_instance æ’ 0ï¼ŒDX11 SV_InstanceID ä»Ž 0 èµ·ï¼Œåç§»å·²ç”± 0 åŸº SSBO ç´¢å¼•è¡¨è¾¾ã€‚
    cmd.DrawIndexedInstanced(static_cast<uint32_t>(indices.size()),
                             static_cast<uint32_t>(instance_models.size()),
                             0u, 0, 0u);
}

void MeshRenderer::DrawMorphShaded(CommandBuffer& cmd, RhiDevice& device,
                                   const std::vector<MeshVertex>& vertices,
                                   const std::vector<uint16_t>& indices,
                                   const std::vector<MeshMorphTarget>& morph_targets,
                                   const glm::mat4& model,
                                   const glm::mat4& view,
                                   const glm::mat4& proj,
                                   const glm::vec3& camera_pos,
                                   const ShadedMaterial& material,
                                   const DirectionalLight& light,
                                   const std::vector<ShadedPointLight>& point_lights,
                                   const ShadedGI& gi,
                                   const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardMorphShaded);
    if (program == 0) return;  // è¯¥åŽç«¯æœªæä¾› morph é«˜çº§ shading å†…å»ºç€è‰²å™¨

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- CPU ä¾§é¢„å˜æ¢åŸºé¡¶ç‚¹åˆ°ä¸–ç•Œç©ºé—´ï¼ˆä¸Ž DrawShaded ä¸€è‡´ï¼‰---
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

    // --- morph å¢žé‡ï¼šCPU ä¾§æŒ‰æƒé‡çº¿æ€§åˆå¹¶ä¸ºæ¯é¡¶ç‚¹å•æ¡ä¸–ç•Œç©ºé—´å¢žé‡ï¼Œæ‰“åŒ…æˆ SSBO ---
    // morph ä¸ºçº¿æ€§æ··åˆ final = base + Î£_t w_tÂ·delta_tï¼Œæ•…æƒé‡åœ¨ CPU å åŠ ï¼ˆä¸Žæœ¬è·¯å¾„æ—¢æœ‰ã€Œé¡¶ç‚¹
    // CPU é¢„å˜æ¢ã€ä¸€è‡´ï¼Œä¸”è§„é¿ VS ä¾§é¢å¤– UBO åœ¨ä¸‰åŽç«¯ç»‘å®šè¯­ä¹‰ä¸Šçš„åˆ†æ­§ï¼‰ã€‚ä»…æ”¶çº³ deltas ä¸ŽåŸºç½‘æ ¼
    // ç­‰é•¿çš„æœ‰æ•ˆ targetï¼ˆâ‰¤64ï¼‰ï¼›ä½ç½®å¢žé‡ç”¨ model çº¿æ€§éƒ¨åˆ†ã€æ³•çº¿å¢žé‡ç”¨æ³•çº¿çŸ©é˜µï¼ˆä¸å«å¹³ç§»ï¼‰ã€‚
    std::vector<GpuMorphDelta> morph_deltas(vertices.size());  // å€¼åˆå§‹åŒ–ä¸º 0
    int active_targets = 0;
    for (const MeshMorphTarget& tgt : morph_targets) {
        if (tgt.position_deltas.size() != vertices.size()) continue;  // è·³è¿‡ä¸åŒ¹é… target
        if (active_targets >= kMaxFwdMorphTargets) break;
        ++active_targets;
        const float w = tgt.weight;
        if (w == 0.0f) continue;  // é›¶æƒé‡æ— è´¡çŒ®
        const bool has_nrm = tgt.normal_deltas.size() == vertices.size();
        for (size_t i = 0; i < vertices.size(); ++i) {
            const glm::vec3 wdp = model3 * tgt.position_deltas[i];
            const glm::vec3 wdn = has_nrm ? normal_matrix * tgt.normal_deltas[i] : glm::vec3(0.0f);
            GpuMorphDelta& d = morph_deltas[i];
            d.dx += w * wdp.x; d.dy += w * wdp.y; d.dz += w * wdp.z;
            d.nx += w * wdn.x; d.ny += w * wdn.y; d.nz += w * wdn.z;
        }
    }
    if (morph_deltas.empty()) morph_deltas.push_back(GpuMorphDelta{});  // ä¿è¯ SSBO éžç©ºå¯ç»‘å®š

    const size_t morph_bytes = morph_deltas.size() * sizeof(GpuMorphDelta);
    EnsureMorphCapacity(device, morph_bytes);
    if (!morph_ssbo_) return;
    device.UpdateGpuBuffer(morph_ssbo_, 0, morph_bytes, morph_deltas.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuMeshVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- UBO å¡«å……ï¼ˆä¸Ž DrawShaded åŒæž„ï¼‰ ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
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
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0ï¼ˆæ‰©å±•ï¼‰
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0
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
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight    @ set7.b1ï¼ˆFinal-Feat-4ï¼‰
    // morph åˆå¹¶å¢žé‡ SSBO\@slot 0ï¼ˆä¸‰åŽç«¯é€šç”¨ï¼šGL binding0 / Vulkan SSBO ç¬¬0(set7.b0) / DX11 t0 ç» @SSBO_LOW_REGISTERSï¼‰ã€‚
    cmd.BindStorageBuffer(0u, morph_ssbo_.raw(), 0u, static_cast<uint32_t>(morph_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

} // namespace render
} // namespace dse
