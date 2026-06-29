#include "engine/reflect/component_reflection.h"

#include <typeindex>

#include "engine/reflect/reflect.h"

#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/components_3d_tree.h"

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
    t.field("base_color", &GrassComponent::base_color);
    t.field("tip_color", &GrassComponent::tip_color);
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
    // Volumetric Fog
    t.field("fog_enabled", &PostProcessComponent::fog_enabled);
    t.field("fog_color", &PostProcessComponent::fog_color);
    t.field("fog_density", &PostProcessComponent::fog_density).range(0.0, 1.0);
    t.field("fog_height_falloff", &PostProcessComponent::fog_height_falloff).range(0.0, 4.0);
    t.field("fog_height_offset", &PostProcessComponent::fog_height_offset);
    t.field("fog_start", &PostProcessComponent::fog_start).range(0.0, 10000.0);
    t.field("fog_end", &PostProcessComponent::fog_end).range(0.0, 10000.0);
    t.field("fog_steps", &PostProcessComponent::fog_steps).range(1.0, 64.0);
    t.field("fog_sun_scatter", &PostProcessComponent::fog_sun_scatter).range(0.0, 4.0);
}

}  // namespace

void EnsureCoreReflectionRegistered() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    RegisterGrass();
    RegisterTree();
    RegisterPostProcess();
}

}  // namespace dse::reflect
