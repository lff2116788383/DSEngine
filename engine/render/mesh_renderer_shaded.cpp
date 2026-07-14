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

    // Shader Graph 自定义命名程序（仅 GL 有效）优先；否则用内建 ForwardShaded。
    // 逐 draw 绑定的 PerFrame UBO / 贴图槽与内建路径共用，自定义程序按同一约定取数据。
    unsigned int program = material.custom_program != 0
        ? material.custom_program
        : device.GetBuiltinProgram(BuiltinProgram::ForwardShaded);
    if (program == 0) return;  // 该后端未提供高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- CPU 侧预变换顶点到世界空间（与 Draw 一致）---
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
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    // CSM 方向光阴影：从 device 全局渲染状态取级联矩阵/分裂/atlas 区域（与执行器 DrawMeshBatch 同源）。
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
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
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
    // clearcoat.z 复用为 wboit_mode（B2c-4）：着色器据此切换 accumulation/revealage 输出。
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    // 点光 UBO（clustered ≤64，超出截断）。
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

    // 地形参数 UBO（splat 4 层 + 积雪；均关闭时输出与 B2c-2 一致）。
    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    // LightProbe SH UBO（slot=5）。sh_enabled=0 时 probe_params.x=0 → 着色器不取 SH。
    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    // DDGI 参数 UBO（slot=6）。ddgi_enabled=0 或无 atlas 时 origin.w=0 → 着色器不取 DDGI。
    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    // 聚光灯 UBO（set7.b1/slot=7，Final-Feat-4）。count=0 时着色器无聚光灯贡献。
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

    // PSO 选择：WBOIT 透明通道优先（accumulation/revealage），否则按 double-sided 选剔除状态。
    unsigned int pso = SelectShadedPso(device, material);
    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
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
    // 地形 splat 纹理（slot 5-9，flat unit 5-9）。未用时绑白纹理保证三后端 descriptor 有定义。
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    // DDGI irradiance atlas（slot 10，flat unit 10）。未启用时绑白纹理保证三后端 descriptor 有定义。
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    // CSM shadow atlas（slot 11，flat unit 11）。无 shadow map（grs.shadow_map[0]==0）或 receive_shadow 关时
    // 绑白纹理：着色器内 receive_shadow 门控已返回 0，白纹理深度=1 也不会产生阴影。
    cmd.BindTexture(11u, tex_or_white(grs.shadow_map[0]), TextureDim::Tex2D);
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
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
    // CPU 侧预变换到世界空间（与 DrawShaded 完全同源：位置 model、法线 normal-matrix、切线 model 线性部分）。
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
    vb_desc.is_dynamic = false;  // 常驻静态缓冲（tiled terrain：上传一次，多 tile 子段复用）
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

    // Shader Graph 自定义命名程序（仅 GL 有效）优先；否则用内建 ForwardShaded。
    unsigned int program = material.custom_program != 0
        ? material.custom_program
        : device.GetBuiltinProgram(BuiltinProgram::ForwardShaded);
    if (program == 0) return;  // 该后端未提供高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // 外部常驻 VB/IB：顶点已是世界空间（BuildShadedWorldVertexBuffer 预变换），故不在此重传/预变换。
    // --- UBO 填充（与 DrawShaded 同源）---
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
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
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
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    // 外部常驻缓冲：绑 caller 持有的 VB/IB，按 index_count_override 子段绘制。
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
    if (program == 0) return;  // 该后端未提供 GBuffer-mesh 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_) return;

    // --- CPU 侧预变换顶点到世界空间（与 DrawShaded 同源；gPosition 直接取此世界坐标）---
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

    // PerFrame（set0.b0）：vp = proj*view，gbuffer.frag 不读但须保持 descriptor layout 兼容。
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(0.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    // PerScene（set1.b0）：占位，仅为保持与 PBR 管线 descriptor layout 一致。
    FwdPerSceneUBO scene{};
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    // 不透明几何 PSO（写/测深度、背面剔除、不混合）：MRT 各 attachment 共用此关混合状态。
    cmd.BindPipeline(device.GetGraphicsPipeline(pso_, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());  // PerFrame @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());  // PerScene @ set1.b0（占位）
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
    if (program == 0) return;  // 该后端未提供蒙皮高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

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

    // --- UBO 填充（与 DrawShaded 同构） ---
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
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
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

    // 聚光灯 UBO（set7.b1/slot=7，Final-Feat-4）。count=0 时着色器无聚光灯贡献。
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

    // PSO 选择：与 DrawShaded 一致（WBOIT 透明优先，否则按 double-sided 选剔除）。
    unsigned int pso = SelectShadedPso(device, material);
    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
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
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1ï¼ˆFinal-Feat-4ï¼‰
    // 骨骼矩阵 SSBO\@slot 0（三后端通用：GL binding0 / Vulkan 位置0(set7) / DX11 t0 经 @SSBO_LOW_REGISTERS）。
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
    if (program == 0) return;  // 该后端未提供实例化高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- 顶点打包（局部空间，VS 按实例 model 变换，不在 CPU 预变换） ---
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

    // --- 每实例 model 矩阵写入 SSBO（世界空间，0 基索引） ---
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

    // --- UBO 填充（与 DrawShaded 同构） ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    // 植被风弯曲（B2b-6，grass）：material.foliage 时喂入 grs 风参，否则保持零 → VS 整段跳过。
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
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
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

    // 聚光灯 UBO（set7.b1/slot=7，Final-Feat-4）。count=0 时着色器无聚光灯贡献。
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

    // PSO 选择：与 DrawShaded 一致（WBOIT 透明优先，否则按 double-sided 选剔除）。
    unsigned int pso = SelectShadedPso(device, material);
    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
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
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1ï¼ˆFinal-Feat-4ï¼‰
    // 每实例 model SSBO\@slot 0（三后端通用：GL binding0 / Vulkan 位置0(set7) / DX11 t0 经 @SSBO_LOW_REGISTERS）。
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // 契约：first_instance 恒 0，DX11 SV_InstanceID 从 0 起，偏移已由 0 基 SSBO 索引表达。
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
    if (program == 0) return;  // 该后端未提供蒙皮×实例化高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- 顶点打包（局部/绑定空间，VS 施骨骼混合 + 每实例 model + vp，不在 CPU 预变换） ---
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

    // --- 骨骼调色板去重密排（绑定→局部空间，不预乘 model），记每份调色板起始下标 ---
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

    // --- 每实例 {model, bone_offset}（80B MeshSkinnedInst）写入实例 SSBO（0 基索引） ---
    std::vector<MeshSkinnedInstGPU> insts(instance_models.size());
    for (size_t j = 0; j < instance_models.size(); ++j) {
        const int pi = instance_palette_idx[j];
        if (pi < 0 || pi >= static_cast<int>(palette_base.size())) return;  // 调色板下标越界，整批放弃
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

    // --- UBO 填充（与 DrawShaded / DrawInstancedShaded 同构） ---
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

    // 聚光灯 UBO（set7.b1/slot=7，Final-Feat-4）。count=0 时着色器无聚光灯贡献。
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

    // PSO 选择：与 DrawShaded 一致（WBOIT 透明优先，否则按 double-sided 选剔除）。
    unsigned int pso = SelectShadedPso(device, material);
    cmd.BindPipeline(device.GetGraphicsPipeline(pso, program));
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
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
    // 实例 SSBO\@slot0（set8.b0）+ 骨骼 SSBO\@slot1（set8.b1）；三后端 @SSBO_LOW_REGISTERS → DX11 t0/t1、GL binding0/1、Vulkan rank0/1。
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindStorageBuffer(1u, bone_ssbo_.raw(), 0u, static_cast<uint32_t>(bone_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuSkinnedVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // 契约：first_instance 恒 0，DX11 SV_InstanceID 从 0 起，偏移已由 0 基 SSBO 索引表达。
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
    if (program == 0) return;  // 该后端未提供 morph 高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- CPU 侧预变换基顶点到世界空间（与 DrawShaded 一致）---
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

    // --- morph 增量：CPU 侧按权重线性合并为每顶点单条世界空间增量，打包成 SSBO ---
    // morph 为线性混合 final = base + Σ_t w_t·delta_t，故权重在 CPU 叠加（与本路径既有「顶点
    // CPU 预变换」一致，且规避 VS 侧额外 UBO 在三后端绑定语义上的分歧）。仅收纳 deltas 与基网格
    // 等长的有效 target（≤64）；位置增量用 model 线性部分、法线增量用法线矩阵（不含平移）。
    std::vector<GpuMorphDelta> morph_deltas(vertices.size());  // 值初始化为 0
    int active_targets = 0;
    for (const MeshMorphTarget& tgt : morph_targets) {
        if (tgt.position_deltas.size() != vertices.size()) continue;  // 跳过不匹配 target
        if (active_targets >= kMaxFwdMorphTargets) break;
        ++active_targets;
        const float w = tgt.weight;
        if (w == 0.0f) continue;  // 零权重无贡献
        const bool has_nrm = tgt.normal_deltas.size() == vertices.size();
        for (size_t i = 0; i < vertices.size(); ++i) {
            const glm::vec3 wdp = model3 * tgt.position_deltas[i];
            const glm::vec3 wdn = has_nrm ? normal_matrix * tgt.normal_deltas[i] : glm::vec3(0.0f);
            GpuMorphDelta& d = morph_deltas[i];
            d.dx += w * wdp.x; d.dy += w * wdp.y; d.dz += w * wdp.z;
            d.nx += w * wdn.x; d.ny += w * wdn.y; d.nz += w * wdn.z;
        }
    }
    if (morph_deltas.empty()) morph_deltas.push_back(GpuMorphDelta{});  // 保证 SSBO 非空可绑定

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

    // --- UBO 填充（与 DrawShaded 同构） ---
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
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
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
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
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
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight    @ set7.b1ï¼ˆFinal-Feat-4ï¼‰
    // morph 合并增量 SSBO\@slot 0（三后端通用：GL binding0 / Vulkan SSBO 第0(set7.b0) / DX11 t0 经 @SSBO_LOW_REGISTERS）。
    cmd.BindStorageBuffer(0u, morph_ssbo_.raw(), 0u, static_cast<uint32_t>(morph_bytes));
    cmd.BindVertexBuffer(0u, vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

} // namespace render
} // namespace dse
