#include "engine/reflect/component_reflection.h"

#include <typeindex>

#include "engine/reflect/reflect.h"

#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/ecs/components_3d_foliage.h"
#include "engine/ecs/components_3d_navmesh.h"
#include "engine/ecs/components_3d_terrain_tile.h"

namespace dse::reflect {

namespace {

void RegisterGrass() {
    using dse::GrassComponent;
    auto t = DSE_REFLECT_TYPE(GrassComponent);
    t.field("enabled", &GrassComponent::enabled);
    t.field("density", &GrassComponent::density).range(0.0, 16.0).tooltip("每平方单位草叶数");
    t.field("spawn_radius", &GrassComponent::spawn_radius).range(1.0, 500.0);
    t.field("seed", &GrassComponent::seed);
    t.field("chunk_size", &GrassComponent::chunk_size).range(1.0, 64.0);
    t.field("blade_width", &GrassComponent::blade_width).range(0.0, 2.0);
    t.field("blade_height", &GrassComponent::blade_height).range(0.0, 8.0);
    t.field("blade_height_variation", &GrassComponent::blade_height_variation).range(0.0, 1.0);
    t.field("base_color", &GrassComponent::base_color).color();
    t.field("tip_color", &GrassComponent::tip_color).color();
    t.field("wind_direction", &GrassComponent::wind_direction);
    t.field("wind_speed", &GrassComponent::wind_speed).range(0.0, 10.0);
    t.field("wind_strength", &GrassComponent::wind_strength).range(0.0, 4.0);
    t.field("wind_turbulence", &GrassComponent::wind_turbulence).range(0.0, 4.0);
    t.field("lod_near", &GrassComponent::lod_near).range(0.0, 500.0);
    t.field("lod_far", &GrassComponent::lod_far).range(0.0, 500.0);
    t.field("fade_range", &GrassComponent::fade_range).range(0.0, 50.0);
    t.field("cast_shadow", &GrassComponent::cast_shadow);
    t.field("shadow_distance", &GrassComponent::shadow_distance).range(0.0, 200.0);
    // density_mask / cached_instance_count_ 为复杂/运行时字段，暂不反射。
}

void RegisterTree() {
    using dse::TreeComponent;
    auto t = DSE_REFLECT_TYPE(TreeComponent);
    t.field("enabled", &TreeComponent::enabled);
    t.field("mesh_path", &TreeComponent::mesh_path);
    t.field("lod1_mesh_path", &TreeComponent::lod1_mesh_path);
    t.field("billboard_texture_path", &TreeComponent::billboard_texture_path);
    t.field("density", &TreeComponent::density).range(0.0, 1.0).tooltip("每平方米树木数");
    t.field("spawn_radius", &TreeComponent::spawn_radius).range(1.0, 1000.0);
    t.field("seed", &TreeComponent::seed);
    t.field("chunk_size", &TreeComponent::chunk_size).range(1.0, 256.0);
    t.field("min_scale", &TreeComponent::min_scale).range(0.0, 10.0);
    t.field("max_scale", &TreeComponent::max_scale).range(0.0, 10.0);
    t.field("height_variation", &TreeComponent::height_variation).range(0.0, 1.0);
    t.field("random_rotation", &TreeComponent::random_rotation);
    t.field("lod1_distance", &TreeComponent::lod1_distance).range(0.0, 1000.0);
    t.field("billboard_distance", &TreeComponent::billboard_distance).range(0.0, 1000.0);
    t.field("cull_distance", &TreeComponent::cull_distance).range(0.0, 2000.0);
    t.field("wind_strength", &TreeComponent::wind_strength).range(0.0, 4.0);
    t.field("wind_speed", &TreeComponent::wind_speed).range(0.0, 10.0);
    t.field("cast_shadow", &TreeComponent::cast_shadow);
    t.field("shadow_distance", &TreeComponent::shadow_distance).range(0.0, 500.0);
}

void RegisterPostProcess() {
    using dse::PostProcessComponent;
    auto t = DSE_REFLECT_TYPE(PostProcessComponent);
    t.field("enabled", &PostProcessComponent::enabled);
    // Bloom
    t.field("bloom_enabled", &PostProcessComponent::bloom_enabled);
    t.field("bloom_threshold", &PostProcessComponent::bloom_threshold).range(0.0, 10.0);
    t.field("bloom_intensity", &PostProcessComponent::bloom_intensity).range(0.0, 4.0);
    t.field("bloom_knee", &PostProcessComponent::bloom_knee).range(0.0, 1.0);
    t.field("bloom_mip_weight", &PostProcessComponent::bloom_mip_weight).range(0.0, 1.0);
    // Color Grading
    t.field("color_grading_enabled", &PostProcessComponent::color_grading_enabled);
    t.field("exposure", &PostProcessComponent::exposure).range(0.0, 16.0);
    t.field("gamma", &PostProcessComponent::gamma).range(0.1, 4.0);
    // SSAO
    t.field("ssao_enabled", &PostProcessComponent::ssao_enabled);
    t.field("ssao_radius", &PostProcessComponent::ssao_radius).range(0.0, 4.0);
    t.field("ssao_bias", &PostProcessComponent::ssao_bias).range(0.0, 1.0);
    t.field("ssao_sample_count", &PostProcessComponent::ssao_sample_count).range(1.0, 128.0);
    t.field("ssao_power", &PostProcessComponent::ssao_power).range(0.0, 8.0);
    t.field("ssao_intensity", &PostProcessComponent::ssao_intensity).range(0.0, 4.0);
    // Auto Exposure
    t.field("auto_exposure_enabled", &PostProcessComponent::auto_exposure_enabled);
    t.field("exposure_min", &PostProcessComponent::exposure_min).range(0.0, 16.0);
    t.field("exposure_max", &PostProcessComponent::exposure_max).range(0.0, 32.0);
    t.field("adaptation_speed_up", &PostProcessComponent::adaptation_speed_up).range(0.0, 16.0);
    t.field("adaptation_speed_down", &PostProcessComponent::adaptation_speed_down).range(0.0, 16.0);
    t.field("exposure_compensation", &PostProcessComponent::exposure_compensation).range(-8.0, 8.0);
    // Color LUT
    t.field("color_lut_intensity", &PostProcessComponent::color_lut_intensity).range(0.0, 1.0);
    // Vignette
    t.field("vignette_enabled", &PostProcessComponent::vignette_enabled);
    t.field("vignette_intensity", &PostProcessComponent::vignette_intensity).range(0.0, 1.0);
    t.field("vignette_radius", &PostProcessComponent::vignette_radius).range(0.0, 2.0);
    t.field("vignette_softness", &PostProcessComponent::vignette_softness).range(0.0, 1.0);
    // Film Grain
    t.field("film_grain_enabled", &PostProcessComponent::film_grain_enabled);
    t.field("film_grain_intensity", &PostProcessComponent::film_grain_intensity).range(0.0, 1.0);
    t.field("film_grain_time_scale", &PostProcessComponent::film_grain_time_scale).range(0.0, 16.0);
    // FXAA / TAA
    t.field("fxaa_enabled", &PostProcessComponent::fxaa_enabled);
    t.field("taa_enabled", &PostProcessComponent::taa_enabled);
    t.field("taa_blend_factor", &PostProcessComponent::taa_blend_factor).range(0.0, 1.0);
    // Contact Shadow
    t.field("contact_shadow_enabled", &PostProcessComponent::contact_shadow_enabled);
    t.field("contact_shadow_strength", &PostProcessComponent::contact_shadow_strength).range(0.0, 1.0);
    t.field("contact_shadow_steps", &PostProcessComponent::contact_shadow_steps).range(1.0, 128.0);
    t.field("contact_shadow_step_size", &PostProcessComponent::contact_shadow_step_size).range(0.0, 4.0);
    // DOF
    t.field("dof_enabled", &PostProcessComponent::dof_enabled);
    t.field("dof_focus_distance", &PostProcessComponent::dof_focus_distance).range(0.0, 10000.0);
    t.field("dof_focus_range", &PostProcessComponent::dof_focus_range).range(0.0, 10000.0);
    t.field("dof_bokeh_radius", &PostProcessComponent::dof_bokeh_radius).range(0.0, 32.0);
    // Motion Blur
    t.field("motion_blur_enabled", &PostProcessComponent::motion_blur_enabled);
    t.field("motion_blur_intensity", &PostProcessComponent::motion_blur_intensity).range(0.0, 4.0);
    t.field("motion_blur_samples", &PostProcessComponent::motion_blur_samples).range(1.0, 64.0);
    // SSR
    t.field("ssr_enabled", &PostProcessComponent::ssr_enabled);
    t.field("ssr_max_distance", &PostProcessComponent::ssr_max_distance).range(0.0, 10000.0);
    t.field("ssr_thickness", &PostProcessComponent::ssr_thickness).range(0.0, 16.0);
    t.field("ssr_step_size", &PostProcessComponent::ssr_step_size).range(0.0, 16.0);
    t.field("ssr_max_steps", &PostProcessComponent::ssr_max_steps).range(1.0, 256.0);
    t.field("ssr_fade_distance", &PostProcessComponent::ssr_fade_distance).range(0.0, 1.0);
    t.field("ssr_max_roughness", &PostProcessComponent::ssr_max_roughness).range(0.0, 1.0);
    // Outline
    t.field("outline_enabled", &PostProcessComponent::outline_enabled);
    t.field("outline_color", &PostProcessComponent::outline_color).color();
    t.field("outline_thickness", &PostProcessComponent::outline_thickness).range(0.0, 16.0);
    t.field("outline_depth_threshold", &PostProcessComponent::outline_depth_threshold).range(0.0, 4.0);
    t.field("outline_normal_threshold", &PostProcessComponent::outline_normal_threshold).range(0.0, 4.0);
    // Light Shaft
    t.field("light_shaft_enabled", &PostProcessComponent::light_shaft_enabled);
    t.field("light_shaft_color", &PostProcessComponent::light_shaft_color).color();
    t.field("light_shaft_density", &PostProcessComponent::light_shaft_density).range(0.0, 4.0);
    t.field("light_shaft_weight", &PostProcessComponent::light_shaft_weight).range(0.0, 4.0);
    t.field("light_shaft_decay", &PostProcessComponent::light_shaft_decay).range(0.0, 1.0);
    t.field("light_shaft_exposure", &PostProcessComponent::light_shaft_exposure).range(0.0, 4.0);
    t.field("light_shaft_intensity", &PostProcessComponent::light_shaft_intensity).range(0.0, 4.0);
    t.field("light_shaft_samples", &PostProcessComponent::light_shaft_samples).range(1.0, 256.0);
    // Volumetric Fog
    t.field("fog_enabled", &PostProcessComponent::fog_enabled);
    t.field("fog_color", &PostProcessComponent::fog_color).color();
    t.field("fog_density", &PostProcessComponent::fog_density).range(0.0, 1.0);
    t.field("fog_height_falloff", &PostProcessComponent::fog_height_falloff).range(0.0, 4.0);
    t.field("fog_height_offset", &PostProcessComponent::fog_height_offset);
    t.field("fog_start", &PostProcessComponent::fog_start).range(0.0, 10000.0);
    t.field("fog_end", &PostProcessComponent::fog_end).range(0.0, 10000.0);
    t.field("fog_steps", &PostProcessComponent::fog_steps).range(1.0, 64.0);
    t.field("fog_sun_scatter", &PostProcessComponent::fog_sun_scatter).range(0.0, 4.0);
}

// ─── 扩展注册：更多可序列化组件（元数据 + 通用 JSON 驱动可消费；不改既有场景手写路径） ──
// 仅注册标量/向量/字符串/枚举字段；std::vector / C 数组 / GPU 句柄 / 运行时内部状态不反射。

void RegisterMeshRenderer() {
    using dse::MeshRendererComponent;
    // 真实组件上的枚举：演示 Enum 支持端到端可用（须先于引用它的类型注册）。
    DSE_REFLECT_ENUM(MeshRendererComponent::MaterialDataSource)
        .value("ComponentFallback", MeshRendererComponent::MaterialDataSource::ComponentFallback)
        .value("MaterialInstance", MeshRendererComponent::MaterialDataSource::MaterialInstance);

    auto t = DSE_REFLECT_TYPE(MeshRendererComponent);
    t.field("mesh_path", &MeshRendererComponent::mesh_path);
    t.field("shader_variant", &MeshRendererComponent::shader_variant);
    t.field("color", &MeshRendererComponent::color).color();
    t.field("emissive", &MeshRendererComponent::emissive).color();
    t.field("metallic", &MeshRendererComponent::metallic).range(0.0, 1.0);
    t.field("roughness", &MeshRendererComponent::roughness).range(0.0, 1.0);
    t.field("ao", &MeshRendererComponent::ao).range(0.0, 1.0);
    t.field("normal_strength", &MeshRendererComponent::normal_strength).range(0.0, 4.0);
    t.field("material_alpha_cutoff", &MeshRendererComponent::material_alpha_cutoff).range(0.0, 1.0);
    t.field("material_alpha_test", &MeshRendererComponent::material_alpha_test);
    t.field("material_double_sided", &MeshRendererComponent::material_double_sided);
    t.field("sss_strength", &MeshRendererComponent::sss_strength).range(0.0, 1.0);
    t.field("sss_tint", &MeshRendererComponent::sss_tint).color();
    t.field("clear_coat", &MeshRendererComponent::clear_coat).range(0.0, 1.0);
    t.field("clear_coat_roughness", &MeshRendererComponent::clear_coat_roughness).range(0.0, 1.0);
    t.field("anisotropy", &MeshRendererComponent::anisotropy).range(-1.0, 1.0);
    t.field("pom_height_scale", &MeshRendererComponent::pom_height_scale).range(0.0, 1.0);
    t.field("receive_shadow", &MeshRendererComponent::receive_shadow);
    t.field("depth_test_enabled", &MeshRendererComponent::depth_test_enabled);
    t.field("depth_write_enabled", &MeshRendererComponent::depth_write_enabled);
    t.field("visible", &MeshRendererComponent::visible);
    t.field("is_static", &MeshRendererComponent::is_static);
    t.field("sorting_layer", &MeshRendererComponent::sorting_layer);
    t.field("order_in_layer", &MeshRendererComponent::order_in_layer);
    t.field("material_data_source", &MeshRendererComponent::material_data_source);
}

void RegisterCamera3D() {
    using dse::Camera3DComponent;
    auto t = DSE_REFLECT_TYPE(Camera3DComponent);
    t.field("enabled", &Camera3DComponent::enabled);
    t.field("priority", &Camera3DComponent::priority);
    t.field("fov", &Camera3DComponent::fov).range(1.0, 179.0);
    t.field("aspect_ratio", &Camera3DComponent::aspect_ratio).range(0.1, 10.0);
    t.field("near_clip", &Camera3DComponent::near_clip).range(0.001, 100.0);
    t.field("far_clip", &Camera3DComponent::far_clip).range(1.0, 100000.0);
    // view/projection 为运行时矩阵，不反射。
}

void RegisterDecal() {
    using dse::DecalComponent;
    auto t = DSE_REFLECT_TYPE(DecalComponent);
    t.field("enabled", &DecalComponent::enabled);
    t.field("color", &DecalComponent::color).color();
    t.field("angle_fade", &DecalComponent::angle_fade).range(0.0, 1.0);
}

void RegisterPointLight() {
    using dse::PointLightComponent;
    auto t = DSE_REFLECT_TYPE(PointLightComponent);
    t.field("enabled", &PointLightComponent::enabled);
    t.field("color", &PointLightComponent::color).color();
    t.field("intensity", &PointLightComponent::intensity).range(0.0, 100.0);
    t.field("radius", &PointLightComponent::radius).range(0.0, 1000.0);
    t.field("falloff", &PointLightComponent::falloff).range(0.0, 8.0);
    t.field("cast_shadow", &PointLightComponent::cast_shadow);
}

void RegisterSpotLight() {
    using dse::SpotLightComponent;
    auto t = DSE_REFLECT_TYPE(SpotLightComponent);
    t.field("enabled", &SpotLightComponent::enabled);
    t.field("color", &SpotLightComponent::color).color();
    t.field("direction", &SpotLightComponent::direction);
    t.field("intensity", &SpotLightComponent::intensity).range(0.0, 100.0);
    t.field("radius", &SpotLightComponent::radius).range(0.0, 1000.0);
    t.field("falloff", &SpotLightComponent::falloff).range(0.0, 8.0);
    t.field("inner_cone_angle", &SpotLightComponent::inner_cone_angle).range(0.0, 90.0);
    t.field("outer_cone_angle", &SpotLightComponent::outer_cone_angle).range(0.0, 90.0);
    t.field("cast_shadow", &SpotLightComponent::cast_shadow);
}

void RegisterDirectionalLight() {
    using dse::DirectionalLight3DComponent;
    auto t = DSE_REFLECT_TYPE(DirectionalLight3DComponent);
    t.field("enabled", &DirectionalLight3DComponent::enabled);
    t.field("direction", &DirectionalLight3DComponent::direction);
    t.field("color", &DirectionalLight3DComponent::color).color();
    t.field("intensity", &DirectionalLight3DComponent::intensity).range(0.0, 100.0);
    t.field("ambient_intensity", &DirectionalLight3DComponent::ambient_intensity).range(0.0, 4.0);
    t.field("shadow_strength", &DirectionalLight3DComponent::shadow_strength).range(0.0, 1.0);
    t.field("cast_shadow", &DirectionalLight3DComponent::cast_shadow);
    t.field("cascade_split_lambda", &DirectionalLight3DComponent::cascade_split_lambda).range(0.0, 1.0);
    t.field("cascade_splits", &DirectionalLight3DComponent::cascade_splits);  // 固定 C 数组
}

void RegisterSkyLight() {
    using dse::SkyLightComponent;
    auto t = DSE_REFLECT_TYPE(SkyLightComponent);
    t.field("enabled", &SkyLightComponent::enabled);
    t.field("up_color", &SkyLightComponent::up_color).color();
    t.field("down_color", &SkyLightComponent::down_color).color();
    t.field("intensity", &SkyLightComponent::intensity).range(0.0, 16.0);
}

void RegisterSkybox() {
    using dse::SkyboxComponent;
    auto t = DSE_REFLECT_TYPE(SkyboxComponent);
    t.field("enabled", &SkyboxComponent::enabled);
    t.field("cubemap_path", &SkyboxComponent::cubemap_path);
}

void RegisterFreeCameraController() {
    using dse::FreeCameraControllerComponent;
    auto t = DSE_REFLECT_TYPE(FreeCameraControllerComponent);
    t.field("enabled", &FreeCameraControllerComponent::enabled);
    t.field("move_speed", &FreeCameraControllerComponent::move_speed).range(0.0, 100.0);
    t.field("mouse_sensitivity", &FreeCameraControllerComponent::mouse_sensitivity).range(0.0, 4.0);
    t.field("pitch", &FreeCameraControllerComponent::pitch);
    t.field("yaw", &FreeCameraControllerComponent::yaw);
}

void RegisterSubScene() {
    using dse::SubSceneComponent;
    auto t = DSE_REFLECT_TYPE(SubSceneComponent);
    t.field("enabled", &SubSceneComponent::enabled);
    t.field("scene_path", &SubSceneComponent::scene_path);
}

void RegisterLODGroup() {
    using dse::LODLevelConfig;
    using dse::LODGroupComponent;
    {
        auto c = DSE_REFLECT_TYPE(LODLevelConfig);  // 须先于引用它的容器字段注册
        c.field("mesh_path", &LODLevelConfig::mesh_path);
        c.field("screen_size_threshold", &LODLevelConfig::screen_size_threshold).range(0.0, 1.0);
        // mesh_handle / loaded 为运行时状态，跳过。
    }
    auto t = DSE_REFLECT_TYPE(LODGroupComponent);
    t.field("enabled", &LODGroupComponent::enabled);
    t.field("levels", &LODGroupComponent::levels);  // std::vector<LODLevelConfig>
    t.field("global_scale", &LODGroupComponent::global_scale).range(0.0, 100.0);
    t.field("hysteresis", &LODGroupComponent::hysteresis).range(0.0, 1.0);
    t.field("min_screen_size", &LODGroupComponent::min_screen_size).range(0.0, 1.0);
    t.field("original_mesh_path", &LODGroupComponent::original_mesh_path);
    // current_lod / lod_culled 为运行时状态，跳过。
}

void RegisterBoundingBox() {
    using dse::BoundingBoxComponent;
    auto t = DSE_REFLECT_TYPE(BoundingBoxComponent);
    t.field("min_extents", &BoundingBoxComponent::min_extents);
    t.field("max_extents", &BoundingBoxComponent::max_extents);
}

void RegisterWater() {
    using dse::WaterComponent;
    auto t = DSE_REFLECT_TYPE(WaterComponent);
    t.field("enabled", &WaterComponent::enabled);
    t.field("water_level", &WaterComponent::water_level);
    t.field("deep_color", &WaterComponent::deep_color).color();
    t.field("shallow_color", &WaterComponent::shallow_color).color();
    t.field("max_depth", &WaterComponent::max_depth).range(0.0, 1000.0);
    t.field("transparency", &WaterComponent::transparency).range(0.0, 1.0);
    t.field("wave_amplitude", &WaterComponent::wave_amplitude).range(0.0, 10.0);
    t.field("wave_frequency", &WaterComponent::wave_frequency).range(0.0, 16.0);
    t.field("wave_speed", &WaterComponent::wave_speed).range(0.0, 16.0);
    t.field("wave_direction", &WaterComponent::wave_direction);
    t.field("refraction_strength", &WaterComponent::refraction_strength).range(0.0, 1.0);
    t.field("reflection_strength", &WaterComponent::reflection_strength).range(0.0, 1.0);
    t.field("specular_power", &WaterComponent::specular_power).range(1.0, 512.0);
    t.field("caustic_intensity", &WaterComponent::caustic_intensity).range(0.0, 4.0);
    t.field("caustic_scale", &WaterComponent::caustic_scale).range(0.0, 64.0);
    t.field("foam_intensity", &WaterComponent::foam_intensity).range(0.0, 4.0);
    t.field("foam_depth_threshold", &WaterComponent::foam_depth_threshold).range(0.0, 100.0);
    t.field("underwater_fog_density", &WaterComponent::underwater_fog_density).range(0.0, 1.0);
    t.field("underwater_fog_color", &WaterComponent::underwater_fog_color).color();
}

void RegisterLightProbe() {
    using dse::LightProbeComponent;
    auto t = DSE_REFLECT_TYPE(LightProbeComponent);
    t.field("enabled", &LightProbeComponent::enabled);
    t.field("influence_radius", &LightProbeComponent::influence_radius).range(0.0, 1000.0);
    t.field("sh_coefficients", &LightProbeComponent::sh_coefficients);  // glm::vec3[9] 固定数组
    t.field("show_debug", &LightProbeComponent::show_debug);
    // needs_rebake 为运行时标志，跳过。
}

void RegisterReflectionProbe() {
    using dse::ReflectionProbeComponent;
    auto t = DSE_REFLECT_TYPE(ReflectionProbeComponent);
    t.field("enabled", &ReflectionProbeComponent::enabled);
    t.field("influence_radius", &ReflectionProbeComponent::influence_radius).range(0.0, 1000.0);
    t.field("box_size_x", &ReflectionProbeComponent::box_size_x).range(0.0, 1000.0);
    t.field("box_size_y", &ReflectionProbeComponent::box_size_y).range(0.0, 1000.0);
    t.field("box_size_z", &ReflectionProbeComponent::box_size_z).range(0.0, 1000.0);
    t.field("use_box_projection", &ReflectionProbeComponent::use_box_projection);
    t.field("resolution", &ReflectionProbeComponent::resolution).range(16.0, 2048.0);
    t.field("show_debug", &ReflectionProbeComponent::show_debug);
    // cubemap_handle / needs_rebake 为运行时状态，跳过。
}

void RegisterGIProbeVolume() {
    using dse::GIProbeVolumeComponent;
    auto t = DSE_REFLECT_TYPE(GIProbeVolumeComponent);
    t.field("enabled", &GIProbeVolumeComponent::enabled);
    t.field("origin", &GIProbeVolumeComponent::origin);
    t.field("extent", &GIProbeVolumeComponent::extent);
    t.field("resolution_x", &GIProbeVolumeComponent::resolution_x).range(1.0, 256.0);
    t.field("resolution_y", &GIProbeVolumeComponent::resolution_y).range(1.0, 256.0);
    t.field("resolution_z", &GIProbeVolumeComponent::resolution_z).range(1.0, 256.0);
    t.field("irradiance_texels", &GIProbeVolumeComponent::irradiance_texels).range(1.0, 64.0);
    t.field("visibility_texels", &GIProbeVolumeComponent::visibility_texels).range(1.0, 64.0);
    t.field("rays_per_probe", &GIProbeVolumeComponent::rays_per_probe).range(1.0, 4096.0);
    t.field("hysteresis", &GIProbeVolumeComponent::hysteresis).range(0.0, 1.0);
    t.field("gi_intensity", &GIProbeVolumeComponent::gi_intensity).range(0.0, 16.0);
    t.field("normal_bias", &GIProbeVolumeComponent::normal_bias).range(0.0, 4.0);
    t.field("view_bias", &GIProbeVolumeComponent::view_bias).range(0.0, 4.0);
    t.field("show_debug_probes", &GIProbeVolumeComponent::show_debug_probes);
    // needs_reinit_ 为运行时状态，跳过。
}

void RegisterFoliage() {
    using dse::FoliageComponent;
    auto t = DSE_REFLECT_TYPE(FoliageComponent);
    t.field("enabled", &FoliageComponent::enabled);
    t.field("wind_strength", &FoliageComponent::wind_strength).range(0.0, 10.0);
    t.field("stiffness", &FoliageComponent::stiffness).range(0.0, 1.0);
    t.field("phase_offset", &FoliageComponent::phase_offset);
    t.field("push_response", &FoliageComponent::push_response).range(0.0, 4.0);
}

void RegisterDynamicObstacle() {
    using dse::DynamicObstacleComponent;
    DSE_REFLECT_ENUM(DynamicObstacleComponent::Shape)
        .value("Box", DynamicObstacleComponent::Shape::Box)
        .value("Cylinder", DynamicObstacleComponent::Shape::Cylinder);

    auto t = DSE_REFLECT_TYPE(DynamicObstacleComponent);
    t.field("enabled", &DynamicObstacleComponent::enabled);
    t.field("shape", &DynamicObstacleComponent::shape);
    t.field("box_extents", &DynamicObstacleComponent::box_extents);
    t.field("cylinder_radius", &DynamicObstacleComponent::cylinder_radius).range(0.0, 100.0);
    t.field("cylinder_height", &DynamicObstacleComponent::cylinder_height).range(0.0, 100.0);
    // obstacle_ref_ / dirty_ 为运行时状态，不反射。
}

void RegisterNavMeshAutoRebake() {
    using dse::NavMeshAutoRebakeComponent;
    auto t = DSE_REFLECT_TYPE(NavMeshAutoRebakeComponent);
    t.field("enabled", &NavMeshAutoRebakeComponent::enabled);
    t.field("tile_size", &NavMeshAutoRebakeComponent::tile_size).range(1.0, 256.0);
    t.field("rebake_cooldown", &NavMeshAutoRebakeComponent::rebake_cooldown).range(0.0, 60.0);
    t.field("collect_terrain", &NavMeshAutoRebakeComponent::collect_terrain);
    t.field("collect_mesh_renderers", &NavMeshAutoRebakeComponent::collect_mesh_renderers);
    t.field("agent_height", &NavMeshAutoRebakeComponent::agent_height).range(0.1, 10.0);
    t.field("agent_radius", &NavMeshAutoRebakeComponent::agent_radius).range(0.1, 10.0);
    t.field("agent_max_climb", &NavMeshAutoRebakeComponent::agent_max_climb).range(0.0, 10.0);
    t.field("agent_max_slope", &NavMeshAutoRebakeComponent::agent_max_slope).range(0.0, 90.0);
    t.field("cell_size", &NavMeshAutoRebakeComponent::cell_size).range(0.01, 10.0);
    t.field("cell_height", &NavMeshAutoRebakeComponent::cell_height).range(0.01, 10.0);
    // cooldown_timer_ / needs_full_rebake_ / baked_tile_count_ 为运行时状态，不反射。
}

void RegisterTerrainTileManager() {
    using dse::TerrainTileManagerComponent;
    auto t = DSE_REFLECT_TYPE(TerrainTileManagerComponent);
    t.field("enabled", &TerrainTileManagerComponent::enabled);
    t.field("tile_world_size", &TerrainTileManagerComponent::tile_world_size).range(1.0, 1024.0);
    t.field("tile_resolution", &TerrainTileManagerComponent::tile_resolution).range(8.0, 512.0);
    t.field("max_height", &TerrainTileManagerComponent::max_height).range(0.0, 10000.0);
    t.field("max_lod_levels", &TerrainTileManagerComponent::max_lod_levels).range(1.0, 8.0);
    t.field("lod_distance_factor", &TerrainTileManagerComponent::lod_distance_factor).range(1.0, 1000.0);
    t.field("load_radius", &TerrainTileManagerComponent::load_radius).range(1.0, 10000.0);
    t.field("unload_radius", &TerrainTileManagerComponent::unload_radius).range(1.0, 10000.0);
    t.field("heightmap_pattern", &TerrainTileManagerComponent::heightmap_pattern);
    t.field("splat_texture_paths", &TerrainTileManagerComponent::splat_texture_paths);  // std::string[4]
    t.field("splat_tiling", &TerrainTileManagerComponent::splat_tiling);
    t.field("base_texture_path", &TerrainTileManagerComponent::base_texture_path);
    t.field("use_procedural", &TerrainTileManagerComponent::use_procedural);
    t.field("procedural_base_height", &TerrainTileManagerComponent::procedural_base_height);
    // tiles / GPU handles / statistics 为运行时状态，不反射。
}

}  // namespace

void EnsureCoreReflectionRegistered() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    RegisterGrass();
    RegisterTree();
    RegisterPostProcess();
    RegisterMeshRenderer();
    RegisterCamera3D();
    RegisterDecal();
    RegisterPointLight();
    RegisterSpotLight();
    RegisterDirectionalLight();
    RegisterSkyLight();
    RegisterSkybox();
    RegisterFreeCameraController();
    RegisterSubScene();
    RegisterLODGroup();
    RegisterBoundingBox();
    RegisterWater();
    RegisterLightProbe();
    RegisterReflectionProbe();
    RegisterGIProbeVolume();
    RegisterFoliage();
    RegisterDynamicObstacle();
    RegisterNavMeshAutoRebake();
    RegisterTerrainTileManager();
}

}  // namespace dse::reflect
