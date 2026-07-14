/**
 * @file dse_api_rendering_light.cpp
 * @brief DSEngine C ABI - Rendering Light 扩展
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_render.h"

using namespace dse;
using namespace dse_api_internal;


extern "C" void dse_rendering_add_skybox(uint32_t e, const char* cubemap_path) {
    World* world = GW();
    if (!world) return;
    auto& sb = world->registry().emplace_or_replace<SkyboxComponent>(TE(e));
    if (cubemap_path) sb.cubemap_path = cubemap_path;
}

extern "C" void dse_rendering_add_gi_probe(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<GIProbeVolumeComponent>(TE(e));
}

extern "C" void dse_rendering_set_gi_probe(uint32_t e, float gi_intensity, float ox, float oy, float oz,
                                           float ex, float ey, float ez,
                                           int res_x, int res_y, int res_z) {
    World* world = GW();
    if (!world) return;
    auto* gi = world->registry().try_get<GIProbeVolumeComponent>(TE(e));
    if (!gi) return;
    if (!Keep(gi_intensity)) gi->gi_intensity = gi_intensity;
    if (!Keep(ox)) gi->origin.x = ox;
    if (!Keep(oy)) gi->origin.y = oy;
    if (!Keep(oz)) gi->origin.z = oz;
    if (!Keep(ex)) gi->extent.x = ex;
    if (!Keep(ey)) gi->extent.y = ey;
    if (!Keep(ez)) gi->extent.z = ez;
    if (res_x > 0) gi->resolution_x = res_x;
    if (res_y > 0) gi->resolution_y = res_y;
    if (res_z > 0) gi->resolution_z = res_z;
    gi->needs_reinit_ = true;
}

extern "C" void dse_rendering_set_gi_probe_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* gi = world->registry().try_get<GIProbeVolumeComponent>(TE(e));
    if (gi) gi->enabled = (enabled != 0);
}

extern "C" int dse_rendering_get_gi_probe(uint32_t e, float* out_gi_intensity,
                                          float* out_origin, float* out_extent, int* out_resolution) {
    World* world = GW();
    if (!world) return 0;
    const auto* gi = world->registry().try_get<GIProbeVolumeComponent>(TE(e));
    if (!gi) return 0;
    if (out_gi_intensity) *out_gi_intensity = gi->gi_intensity;
    if (out_origin) { out_origin[0] = gi->origin.x; out_origin[1] = gi->origin.y; out_origin[2] = gi->origin.z; }
    if (out_extent) { out_extent[0] = gi->extent.x; out_extent[1] = gi->extent.y; out_extent[2] = gi->extent.z; }
    if (out_resolution) { out_resolution[0] = gi->resolution_x; out_resolution[1] = gi->resolution_y; out_resolution[2] = gi->resolution_z; }
    return 1;
}

extern "C" void dse_rendering_add_light_probe(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<LightProbeComponent>(TE(e));
}

extern "C" void dse_rendering_set_light_probe(uint32_t e, float influence_radius) {
    World* world = GW();
    if (!world) return;
    auto* lp = world->registry().try_get<LightProbeComponent>(TE(e));
    if (!lp) return;
    if (!Keep(influence_radius)) lp->influence_radius = influence_radius;
    lp->needs_rebake = true;
}

extern "C" void dse_rendering_set_light_probe_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* lp = world->registry().try_get<LightProbeComponent>(TE(e));
    if (lp) lp->enabled = (enabled != 0);
}

extern "C" void dse_rendering_add_reflection_probe(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<ReflectionProbeComponent>(TE(e));
}

extern "C" void dse_rendering_set_reflection_probe(uint32_t e, float influence_radius, int resolution) {
    World* world = GW();
    if (!world) return;
    auto* rp = world->registry().try_get<ReflectionProbeComponent>(TE(e));
    if (!rp) return;
    if (!Keep(influence_radius)) rp->influence_radius = influence_radius;
    if (resolution > 0) rp->resolution = resolution;
    rp->needs_rebake = true;
}

extern "C" void dse_rendering_set_reflection_probe_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* rp = world->registry().try_get<ReflectionProbeComponent>(TE(e));
    if (rp) rp->enabled = (enabled != 0);
}

extern "C" int dse_dir_light_has(uint32_t e) {
    World* world = GW();
    return (world && world->registry().try_get<DirectionalLight3DComponent>(TE(e))) ? 1 : 0;
}

extern "C" int dse_point_light_has(uint32_t e) {
    World* world = GW();
    return (world && world->registry().try_get<PointLightComponent>(TE(e))) ? 1 : 0;
}

extern "C" int dse_spot_light_has(uint32_t e) {
    World* world = GW();
    return (world && world->registry().try_get<SpotLightComponent>(TE(e))) ? 1 : 0;
}

extern "C" int dse_sky_light_has(uint32_t e) {
    World* world = GW();
    return (world && world->registry().try_get<SkyLightComponent>(TE(e))) ? 1 : 0;
}

extern "C" int dse_dir_light_get_shadow_params(uint32_t e, int* out_cast_shadow, float* out_strength,
                                               float* out_c0, float* out_c1, float* out_c2,
                                               float* out_lambda) {
    World* world = GW();
    if (!world) return 0;
    const auto* light = world->registry().try_get<DirectionalLight3DComponent>(TE(e));
    if (!light) return 0;
    if (out_cast_shadow) *out_cast_shadow = light->cast_shadow ? 1 : 0;
    if (out_strength) *out_strength = light->shadow_strength;
    if (out_c0) *out_c0 = light->cascade_splits[0];
    if (out_c1) *out_c1 = light->cascade_splits[1];
    if (out_c2) *out_c2 = light->cascade_splits[2];
    if (out_lambda) *out_lambda = light->cascade_split_lambda;
    return 1;
}

extern "C" void dse_rendering_set_gi_probe_bias(uint32_t e, float normal_bias, float hysteresis) {
    World* world = GW();
    if (!world) return;
    auto* gi = world->registry().try_get<GIProbeVolumeComponent>(TE(e));
    if (!gi) return;
    if (!Keep(normal_bias)) gi->normal_bias = normal_bias;
    if (!Keep(hysteresis)) gi->hysteresis = hysteresis;
}

extern "C" int dse_rendering_get_gi_probe_ex(uint32_t e, int* out_enabled, float* out_normal_bias) {
    World* world = GW();
    if (!world) return 0;
    const auto* gi = world->registry().try_get<GIProbeVolumeComponent>(TE(e));
    if (!gi) return 0;
    if (out_enabled) *out_enabled = gi->enabled ? 1 : 0;
    if (out_normal_bias) *out_normal_bias = gi->normal_bias;
    return 1;
}

extern "C" void dse_rendering_set_light_probe_ex(uint32_t e, float influence_radius, int needs_rebake) {
    World* world = GW();
    if (!world) return;
    auto* lp = world->registry().try_get<LightProbeComponent>(TE(e));
    if (!lp) return;
    if (!Keep(influence_radius)) lp->influence_radius = influence_radius;
    if (needs_rebake >= 0) lp->needs_rebake = (needs_rebake != 0);
}

extern "C" void dse_rendering_set_reflection_probe_ex(uint32_t e, float influence_radius,
                                                      float box_x, float box_y, float box_z,
                                                      int resolution) {
    World* world = GW();
    if (!world) return;
    auto* rp = world->registry().try_get<ReflectionProbeComponent>(TE(e));
    if (!rp) return;
    if (!Keep(influence_radius)) rp->influence_radius = influence_radius;
    if (!Keep(box_x)) rp->box_size_x = box_x;
    if (!Keep(box_y)) rp->box_size_y = box_y;
    if (!Keep(box_z)) rp->box_size_z = box_z;
    if (resolution > 0) rp->resolution = resolution;
}

