/**
 * @file frame_pipeline_snapshot.cpp
 * @brief FramePipeline thin snapshot capture â€” CaptureThinSnapshot.
 */

#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/frame_pipeline_impl.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/components_3d_fluid.h"
#include "engine/ecs/components_3d_weather.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/ui.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/time.h"
#include "engine/core/event_bus.h"
#include "engine/core/service_locator.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_manager.h"
#include "engine/render/passes/builtin_passes.h"
#include "engine/render/passes/render_pass_interface.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>

void FramePipeline::CaptureThinSnapshot() {
    auto& snap = write_snapshot();
    snap.Reset();

    auto* world = runtime_context_.world;
    if (!world) return;
    auto& reg = world->registry();

    // â”€â”€ 1. 3D Cameraï¼ˆpriority + entity id ç¡®å®šå”¯ä¸€ä¸»ç›¸æœºï¼‰â”€â”€
    {
        auto view = reg.view<dse::Camera3DComponent>();
        entt::entity best = entt::null;
        int best_priority = std::numeric_limits<int>::min();
        std::uint32_t best_id = std::numeric_limits<std::uint32_t>::max();
        for (auto e : view) {
            auto& cam = view.get<dse::Camera3DComponent>(e);
            if (!cam.enabled) continue;
            auto eid = static_cast<std::uint32_t>(e);
            if (best == entt::null ||
                cam.priority > best_priority ||
                (cam.priority == best_priority && eid < best_id)) {
                best = e;
                best_priority = cam.priority;
                best_id = eid;
            }
        }
        if (best != entt::null) {
            auto& cam = view.get<dse::Camera3DComponent>(best);
            auto& c = snap.camera_3d;
            c.valid = true;
            c.fov = cam.fov;
            c.near_clip = cam.near_clip;
            c.far_clip = cam.far_clip;
            if (reg.all_of<TransformComponent>(best)) {
                auto& tf = reg.get<TransformComponent>(best);
                c.position = tf.position;
                c.forward = tf.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                c.up = tf.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                c.right = tf.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
                // Camera-Relative Rendering: view matrix ä»¥åŽŸç‚¹ä¸ºç›¸æœºä½ç½®
                c.view = glm::lookAt(glm::vec3(0.0f), c.forward, c.up);
                c.shadow_center = c.position + c.forward * 50.0f;
                snap.camera_offset = c.position;
            }
        }
    }

    // â”€â”€ 2. 2D Camera fallback â”€â”€
    {
        auto view = reg.view<CameraComponent>();
        entt::entity best = entt::null;
        int best_priority = std::numeric_limits<int>::min();
        std::uint32_t best_id = std::numeric_limits<std::uint32_t>::max();
        for (auto e : view) {
            auto& cam = view.get<CameraComponent>(e);
            if (!cam.enabled) continue;
            auto eid = static_cast<std::uint32_t>(e);
            if (best == entt::null ||
                cam.priority > best_priority ||
                (cam.priority == best_priority && eid < best_id)) {
                best = e;
                best_priority = cam.priority;
                best_id = eid;
            }
        }
        if (best != entt::null) {
            auto& cam = view.get<CameraComponent>(best);
            snap.camera_2d.valid = true;
            snap.camera_2d.view = cam.view;
            snap.camera_2d.projection = cam.projection;
        }
    }

    // â”€â”€ 3. Skyboxï¼ˆå« lazy load å†™å›žï¼Œä¸»çº¿ç¨‹å®‰å…¨ï¼‰â”€â”€
    {
        auto view = reg.view<dse::SkyboxComponent>();
        for (auto e : view) {
            auto& sb = view.get<dse::SkyboxComponent>(e);
            if (!sb.enabled) continue;
            if (sb.cubemap_handle == 0 && !sb.cubemap_path.empty()) {
                if (auto cubemap = runtime_context_.asset_manager->LoadCubemap(sb.cubemap_path)) {
                    sb.cubemap_handle = cubemap->GetHandle();
                }
            }
            if (sb.cubemap_handle != 0) {
                snap.skybox.valid = true;
                snap.skybox.cubemap_handle = sb.cubemap_handle;
                if (reg.all_of<TransformComponent>(e)) {
                    snap.skybox.has_transform = true;
                    snap.skybox.rotation = reg.get<TransformComponent>(e).rotation;
                }
            }
            break;
        }
    }

    // â”€â”€ 3b. Atmosphere Skyï¼ˆç¨‹åºåŒ–å¤§æ°”æ•£å°„å¤©ç©ºï¼Œä¼˜å…ˆäºŽ cubemap skyboxï¼‰â”€â”€
    {
        auto view = reg.view<dse::AtmosphereComponent>();
        for (auto e : view) {
            auto& atm = view.get<dse::AtmosphereComponent>(e);
            if (!atm.enabled) continue;
            auto& s = snap.atmosphere_sky;
            s.valid = true;
            s.planet_radius     = atm.planet_radius;
            s.atmosphere_height = atm.atmosphere_height;
            s.rayleigh_coeff    = atm.rayleigh_coeff;
            s.rayleigh_scale_height = atm.rayleigh_scale_height;
            s.mie_coeff         = atm.mie_coeff;
            s.mie_scale_height  = atm.mie_scale_height;
            s.mie_g             = atm.mie_g;
            s.mie_albedo        = atm.mie_albedo;
            s.ozone_coeff       = atm.ozone_coeff;
            s.ozone_center_h    = atm.ozone_center_h;
            s.ozone_width       = atm.ozone_width;
            s.sun_intensity     = atm.sun_intensity;
            s.sun_disk_angle    = atm.sun_disk_angle;
            s.transmittance_lut_width  = atm.transmittance_lut_width;
            s.transmittance_lut_height = atm.transmittance_lut_height;
            s.sky_view_steps    = atm.sky_view_steps;
            break;
        }
    }

    // â”€â”€ 4. Directional Light â”€â”€
    {
        auto view = reg.view<dse::DirectionalLight3DComponent>();
        for (auto e : view) {
            auto& dl = view.get<dse::DirectionalLight3DComponent>(e);
            if (!dl.enabled) continue;
            auto& s = snap.directional_light;
            s.valid = true;
            s.cast_shadow = dl.cast_shadow;
            s.direction = dl.direction;
            s.color = dl.color;
            s.intensity = dl.intensity;
            s.ambient_intensity = dl.ambient_intensity;
            s.shadow_strength = dl.shadow_strength;
            for (int i = 0; i < CSM_CASCADES; ++i)
                s.cascade_splits[i] = dl.cascade_splits[i];
            s.cascade_split_lambda = dl.cascade_split_lambda;
            break;
        }
    }

    // å¤§æ°”å¤©ç©ºéœ€è¦å¤ªé˜³æ–¹å‘ï¼ˆä»Žæ–¹å‘å…‰å–åï¼‰
    if (snap.atmosphere_sky.valid && snap.directional_light.valid) {
        const float dir_len2 = glm::dot(snap.directional_light.direction, snap.directional_light.direction);
        if (dir_len2 > 1e-8f) {
            snap.atmosphere_sky.sun_direction = -glm::normalize(snap.directional_light.direction);
        }
    }

    // â”€â”€ 5. Spot Lightsï¼ˆshadow-casting, max 4ï¼‰â”€â”€
    {
        auto view = reg.view<TransformComponent, dse::SpotLightComponent>();
        snap.spot_shadow_count = 0;
        for (auto e : view) {
            if (snap.spot_shadow_count >= dse::render::RenderThinSnapshot::kMaxSpotShadowLights) break;
            auto& light = view.get<dse::SpotLightComponent>(e);
            if (!light.enabled || !light.cast_shadow) continue;
            auto& tf = view.get<TransformComponent>(e);
            auto& sl = snap.spot_lights[snap.spot_shadow_count];
            sl.position = tf.position;
            sl.forward = glm::normalize(tf.rotation * light.direction);
            sl.up = tf.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            if (std::abs(glm::dot(sl.forward, sl.up)) > 0.98f) {
                sl.up = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            sl.outer_cone_angle = light.outer_cone_angle;
            sl.radius = light.radius;
            ++snap.spot_shadow_count;
        }
    }

    // â”€â”€ 6. Point Lightsï¼ˆshadow-casting, max 4ï¼‰â”€â”€
    {
        auto view = reg.view<TransformComponent, dse::PointLightComponent>();
        snap.point_shadow_count = 0;
        for (auto e : view) {
            if (snap.point_shadow_count >= dse::render::RenderThinSnapshot::kMaxPointShadowLights) break;
            auto& light = view.get<dse::PointLightComponent>(e);
            if (!light.enabled || !light.cast_shadow) continue;
            auto& tf = view.get<TransformComponent>(e);
            auto& pl = snap.point_lights[snap.point_shadow_count];
            pl.position = tf.position;
            pl.radius = light.radius;
            ++snap.point_shadow_count;
        }
    }

    // â”€â”€ 7. PostProcessï¼ˆåˆå¹¶ 13 ä¸ª Pass çš„é‡å¤æŸ¥è¯¢ï¼‰â”€â”€
    {
        auto view = reg.view<dse::PostProcessComponent>();
        for (auto e : view) {
            auto& pp = view.get<dse::PostProcessComponent>(e);
            if (!pp.enabled) continue;
            auto& s = snap.post_process;
            s.valid = true;
            s.enabled = pp.enabled;
            s.bloom_enabled = pp.bloom_enabled;
            s.bloom_threshold = pp.bloom_threshold;
            s.bloom_intensity = pp.bloom_intensity;
            s.bloom_knee = pp.bloom_knee;
            s.bloom_mip_weight = pp.bloom_mip_weight;
            s.color_grading_enabled = pp.color_grading_enabled;
            s.exposure = pp.exposure;
            s.gamma = pp.gamma;
            s.ssao_enabled = pp.ssao_enabled;
            s.ssao_radius = pp.ssao_radius;
            s.ssao_bias = pp.ssao_bias;
            s.ssao_sample_count = pp.ssao_sample_count;
            s.ssao_power = pp.ssao_power;
            s.ssao_intensity = pp.ssao_intensity;
            s.auto_exposure_enabled = pp.auto_exposure_enabled;
            s.exposure_min = pp.exposure_min;
            s.exposure_max = pp.exposure_max;
            s.adaptation_speed_up = pp.adaptation_speed_up;
            s.adaptation_speed_down = pp.adaptation_speed_down;
            s.exposure_compensation = pp.exposure_compensation;
            s.color_lut_handle = pp.color_lut_handle;
            s.color_lut_intensity = pp.color_lut_intensity;
            s.vignette_enabled = pp.vignette_enabled;
            s.vignette_intensity = pp.vignette_intensity;
            s.vignette_radius = pp.vignette_radius;
            s.vignette_softness = pp.vignette_softness;
            s.film_grain_enabled = pp.film_grain_enabled;
            s.film_grain_intensity = pp.film_grain_intensity;
            s.film_grain_time_scale = pp.film_grain_time_scale;
            s.fxaa_enabled = pp.fxaa_enabled;
            s.taa_enabled = pp.taa_enabled;
            s.taa_blend_factor = pp.taa_blend_factor;
            s.contact_shadow_enabled = pp.contact_shadow_enabled;
            s.contact_shadow_strength = pp.contact_shadow_strength;
            s.contact_shadow_steps = pp.contact_shadow_steps;
            s.contact_shadow_step_size = pp.contact_shadow_step_size;
            s.dof_enabled = pp.dof_enabled;
            s.dof_focus_distance = pp.dof_focus_distance;
            s.dof_focus_range = pp.dof_focus_range;
            s.dof_bokeh_radius = pp.dof_bokeh_radius;
            s.motion_blur_enabled = pp.motion_blur_enabled;
            s.motion_blur_intensity = pp.motion_blur_intensity;
            s.motion_blur_samples = pp.motion_blur_samples;
            s.ssr_enabled = pp.ssr_enabled;
            s.ssr_max_distance = pp.ssr_max_distance;
            s.ssr_thickness = pp.ssr_thickness;
            s.ssr_step_size = pp.ssr_step_size;
            s.ssr_max_steps = pp.ssr_max_steps;
            s.ssr_fade_distance = pp.ssr_fade_distance;
            s.ssr_max_roughness = pp.ssr_max_roughness;
            s.outline_enabled = pp.outline_enabled;
            s.outline_color = pp.outline_color;
            s.outline_thickness = pp.outline_thickness;
            s.outline_depth_threshold = pp.outline_depth_threshold;
            s.outline_normal_threshold = pp.outline_normal_threshold;
            s.light_shaft_enabled = pp.light_shaft_enabled;
            s.light_shaft_color = pp.light_shaft_color;
            s.light_shaft_density = pp.light_shaft_density;
            s.light_shaft_weight = pp.light_shaft_weight;
            s.light_shaft_decay = pp.light_shaft_decay;
            s.light_shaft_exposure = pp.light_shaft_exposure;
            s.light_shaft_intensity = pp.light_shaft_intensity;
            s.light_shaft_samples = pp.light_shaft_samples;
            s.fog_enabled = pp.fog_enabled;
            s.fog_color = pp.fog_color;
            s.fog_density = pp.fog_density;
            s.fog_height_falloff = pp.fog_height_falloff;
            s.fog_height_offset = pp.fog_height_offset;
            s.fog_start = pp.fog_start;
            s.fog_end = pp.fog_end;
            s.fog_steps = pp.fog_steps;
            s.fog_sun_scatter = pp.fog_sun_scatter;
            break;
        }
    }

    // â”€â”€ 7b. Volumetric Cloud â”€â”€
    {
        auto view = reg.view<dse::VolumetricCloudComponent>();
        for (auto e : view) {
            auto& cloud = view.get<dse::VolumetricCloudComponent>(e);
            if (!cloud.enabled) continue;
            auto& vc = snap.volumetric_cloud;
            vc.valid = true;
            vc.half_resolution = cloud.half_resolution;
            vc.cloud_bottom = cloud.cloud_bottom;
            vc.cloud_top = cloud.cloud_top;
            vc.coverage = cloud.coverage;
            vc.density = cloud.density;
            vc.shape_scale = cloud.shape_scale;
            vc.detail_scale = cloud.detail_scale;
            vc.detail_strength = cloud.detail_strength;
            vc.erosion = cloud.erosion;
            vc.wind_offset_x = cloud.wind_direction.x * cloud.wind_speed * Time::TimeSinceStartup();
            vc.wind_offset_z = cloud.wind_direction.y * cloud.wind_speed * Time::TimeSinceStartup();
            vc.silver_intensity = cloud.silver_intensity;
            vc.powder_strength = cloud.powder_strength;
            vc.ambient_strength = cloud.ambient_strength;
            break;
        }
    }

    // â”€â”€ 8. Water surfaces â”€â”€
    {
        auto view = reg.view<dse::WaterComponent>();
        snap.water_count = 0;
        for (auto e : view) {
            if (snap.water_count >= dse::render::RenderThinSnapshot::kMaxWaterSurfaces) break;
            auto& w = view.get<dse::WaterComponent>(e);
            if (!w.enabled) continue;
            auto& ws = snap.waters[snap.water_count];
            ws.water_level = w.water_level;
            ws.deep_color = w.deep_color;
            ws.shallow_color = w.shallow_color;
            ws.max_depth = w.max_depth;
            ws.transparency = w.transparency;
            ws.wave_amplitude = w.wave_amplitude;
            ws.wave_frequency = w.wave_frequency;
            ws.wave_speed = w.wave_speed;
            ws.wave_direction = w.wave_direction;
            ws.refraction_strength = w.refraction_strength;
            ws.reflection_strength = w.reflection_strength;
            ws.specular_power = w.specular_power;
            ws.caustic_intensity = w.caustic_intensity;
            ws.caustic_scale = w.caustic_scale;
            ws.foam_intensity = w.foam_intensity;
            ws.foam_depth_threshold = w.foam_depth_threshold;
            ws.underwater_fog_density = w.underwater_fog_density;
            ws.underwater_fog_color = w.underwater_fog_color;
            ++snap.water_count;
        }
    }

    // â”€â”€ 9. Decals â”€â”€
    {
        auto view = reg.view<TransformComponent, dse::DecalComponent>();
        snap.decal_count = 0;
        for (auto e : view) {
            if (snap.decal_count >= dse::render::RenderThinSnapshot::kMaxDecals) break;
            auto& dc = view.get<dse::DecalComponent>(e);
            if (!dc.enabled || dc.albedo_texture == 0) continue;
            auto& tf = view.get<TransformComponent>(e);
            auto& d = snap.decals[snap.decal_count];
            d.albedo_texture = dc.albedo_texture;
            d.color = dc.color;
            d.angle_fade = dc.angle_fade;
            d.position = tf.position;
            d.rotation = tf.rotation;
            d.scale = tf.scale;
            ++snap.decal_count;
        }
    }

    // â”€â”€ 9b. Weather â”€â”€
    {
        auto view = reg.view<dse::WeatherComponent>();
        for (auto e : view) {
            auto& wc = view.get<dse::WeatherComponent>(e);
            if (!wc.enabled) continue;
            auto& w = snap.weather;
            w.valid = true;
            w.type = static_cast<int>(wc.type);
            w.intensity = wc.intensity;
            w.wind_x = wc.wind_x;
            w.wind_z = wc.wind_z;
            w.spawn_radius = wc.spawn_radius;
            w.spawn_height = wc.spawn_height;
            w.max_particles = wc.max_particles;
            w.color = (wc.type == dse::WeatherType::Rain) ? wc.rain_color : wc.snow_color;
            break;
        }
    }

    // â”€â”€ 10. Light Probe SHï¼ˆè·ç¦»åŠ æƒæ··åˆæœ€è¿‘ä¸¤ä¸ª probeï¼‰â”€â”€
    {
        glm::vec3 cam_pos = snap.camera_3d.position;
        auto probe_view = reg.view<TransformComponent, dse::LightProbeComponent>();
        float best_dist = std::numeric_limits<float>::max();
        float second_dist = std::numeric_limits<float>::max();
        const dse::LightProbeComponent* best_probe = nullptr;
        const dse::LightProbeComponent* second_probe = nullptr;

        for (auto e : probe_view) {
            auto& probe = probe_view.get<dse::LightProbeComponent>(e);
            if (!probe.enabled) continue;
            auto& tf = probe_view.get<TransformComponent>(e);
            float dist = glm::distance(tf.position, cam_pos);
            if (dist < best_dist) {
                second_dist = best_dist;
                second_probe = best_probe;
                best_dist = dist;
                best_probe = &probe;
            } else if (dist < second_dist) {
                second_dist = dist;
                second_probe = &probe;
            }
        }

        if (best_probe) {
            snap.light_probe_sh.valid = true;
            if (second_probe && best_dist < best_probe->influence_radius &&
                second_dist < second_probe->influence_radius) {
                float total = best_dist + second_dist;
                if (total < 0.001f) total = 0.001f;
                float w1 = 1.0f - best_dist / total;
                float w2 = 1.0f - second_dist / total;
                float wsum = w1 + w2;
                w1 /= wsum; w2 /= wsum;
                for (int i = 0; i < 9; ++i) {
                    glm::vec3 blended = best_probe->sh_coefficients[i] * w1 +
                                        second_probe->sh_coefficients[i] * w2;
                    snap.light_probe_sh.coefficients[i] = glm::vec4(blended, 0.0f);
                }
            } else {
                for (int i = 0; i < 9; ++i) {
                    snap.light_probe_sh.coefficients[i] = glm::vec4(best_probe->sh_coefficients[i], 0.0f);
                }
            }
        }
    }

    // â”€â”€ 11. DDGI Config â”€â”€
    {
        auto gi_view = reg.view<dse::GIProbeVolumeComponent>();
        for (auto e : gi_view) {
            auto& gi = gi_view.get<dse::GIProbeVolumeComponent>(e);
            if (!gi.enabled) continue;
            auto& cfg = snap.ddgi_config;
            cfg.enabled = true;
            cfg.needs_reinit = gi.needs_reinit_;
            cfg.origin = gi.origin;
            cfg.extent = gi.extent;
            cfg.resolution_x = gi.resolution_x;
            cfg.resolution_y = gi.resolution_y;
            cfg.resolution_z = gi.resolution_z;
            cfg.irradiance_texels = gi.irradiance_texels;
            cfg.visibility_texels = gi.visibility_texels;
            cfg.rays_per_probe = gi.rays_per_probe;
            cfg.hysteresis = gi.hysteresis;
            cfg.gi_intensity = gi.gi_intensity;
            cfg.normal_bias = gi.normal_bias;
            gi.needs_reinit_ = false;
            break;
        }
    }
}
