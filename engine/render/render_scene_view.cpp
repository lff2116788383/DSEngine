#include "engine/render/render_scene_view.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/render/gi/lightmap_baker.h"

#include <cstring>
#include <limits>

namespace dse::render {

void ExtractRenderSceneView(const World& world, RenderSceneView& out) {
    out.Clear();
    const auto& reg = world.registry();

    // Point lights
    auto point_view = reg.view<TransformComponent, PointLightComponent>();
    for (auto entity : point_view) {
        const auto& tf = point_view.get<TransformComponent>(entity);
        const auto& pl = point_view.get<PointLightComponent>(entity);
        if (!pl.enabled) continue;
        RenderPointLight rpl;
        rpl.position = tf.position;
        rpl.color = pl.color;
        rpl.intensity = pl.intensity;
        rpl.radius = pl.radius;
        rpl.cast_shadow = pl.cast_shadow;
        rpl.shadow_map_handle = pl.shadow_map_handle;
        out.point_lights.push_back(rpl);
    }

    // Spot lights
    auto spot_view = reg.view<TransformComponent, SpotLightComponent>();
    for (auto entity : spot_view) {
        const auto& tf = spot_view.get<TransformComponent>(entity);
        const auto& sl = spot_view.get<SpotLightComponent>(entity);
        if (!sl.enabled) continue;
        RenderSpotLight rsl;
        rsl.position = tf.position;
        rsl.direction = sl.direction;
        rsl.color = sl.color;
        rsl.intensity = sl.intensity;
        rsl.range = sl.radius;
        rsl.inner_cone = sl.inner_cone_angle;
        rsl.outer_cone = sl.outer_cone_angle;
        rsl.cast_shadow = sl.cast_shadow;
        rsl.shadow_map_handle = sl.shadow_map_handle;
        out.spot_lights.push_back(rsl);
    }

    // Directional lights
    auto dir_view = reg.view<DirectionalLight3DComponent>();
    for (auto entity : dir_view) {
        const auto& dl = dir_view.get<DirectionalLight3DComponent>(entity);
        if (!dl.enabled) continue;
        RenderDirectionalLight rdl;
        rdl.direction = dl.direction;
        rdl.color = dl.color;
        rdl.intensity = dl.intensity;
        rdl.cast_shadow = dl.cast_shadow;
        rdl.cascade_count = CSM_CASCADES;
        rdl.cascade_split_lambda = dl.cascade_split_lambda;
        out.directional_lights.push_back(rdl);
    }

    // Reflection probes
    auto refl_view = reg.view<TransformComponent, dse::ReflectionProbeComponent>();
    for (auto entity : refl_view) {
        const auto& tf = refl_view.get<TransformComponent>(entity);
        const auto& rp = refl_view.get<dse::ReflectionProbeComponent>(entity);
        if (!rp.enabled) continue;
        RenderReflectionProbe rrp;
        rrp.position = tf.position;
        rrp.box_min = tf.position - glm::vec3(rp.box_size_x, rp.box_size_y, rp.box_size_z) * 0.5f;
        rrp.box_max = tf.position + glm::vec3(rp.box_size_x, rp.box_size_y, rp.box_size_z) * 0.5f;
        rrp.cubemap_handle = rp.cubemap_handle;
        rrp.intensity = 1.0f;
        rrp.box_projection = rp.use_box_projection;
        out.reflection_probes.push_back(rrp);
    }

    // Light probes
    auto lp_view = reg.view<TransformComponent, dse::LightProbeComponent>();
    for (auto entity : lp_view) {
        const auto& tf = lp_view.get<TransformComponent>(entity);
        const auto& lp = lp_view.get<dse::LightProbeComponent>(entity);
        if (!lp.enabled) continue;
        RenderLightProbe rlp;
        rlp.position = tf.position;
        rlp.radius = lp.influence_radius;
        rlp.baked = !lp.needs_rebake;
        static_assert(sizeof(lp.sh_coefficients) == 9 * sizeof(glm::vec3));
        std::memcpy(rlp.sh_coefficients, lp.sh_coefficients, sizeof(rlp.sh_coefficients));
        out.light_probes.push_back(rlp);
    }

    // Skybox (take first enabled)
    auto sky_view = reg.view<dse::SkyboxComponent>();
    for (auto entity : sky_view) {
        const auto& sb = sky_view.get<dse::SkyboxComponent>(entity);
        if (!sb.enabled) continue;
        out.skybox.present = true;
        out.skybox.cubemap_handle = sb.cubemap_handle;
        out.skybox.intensity = 1.0f;
        out.skybox.rotation = 0.0f;
        break;
    }

    // Lightmaps
    auto lm_view = reg.view<LightmapComponent>();
    for (auto entity : lm_view) {
        const auto& lm = lm_view.get<LightmapComponent>(entity);
        RenderLightmap rlm;
        rlm.entity_index = static_cast<uint32_t>(entity);
        rlm.lightmap_handle = lm.lightmap_handle;
        rlm.intensity = lm.intensity;
        rlm.st_offset = lm.st_offset;
        rlm.use_ao = lm.use_ao;
        out.lightmaps.push_back(rlm);
    }
}

const RenderReflectionProbe* FindClosestReflectionProbe(
        const RenderSceneView& scene, const glm::vec3& position) {
    const RenderReflectionProbe* best = nullptr;
    float best_dist = std::numeric_limits<float>::max();
    for (const auto& probe : scene.reflection_probes) {
        const glm::vec3 d = probe.position - position;
        const float dist = glm::dot(d, d);
        if (dist < best_dist) {
            best_dist = dist;
            best = &probe;
        }
    }
    return best;
}

const RenderLightProbe* FindClosestLightProbe(
        const RenderSceneView& scene, const glm::vec3& position) {
    const RenderLightProbe* best = nullptr;
    float best_dist = std::numeric_limits<float>::max();
    for (const auto& probe : scene.light_probes) {
        const glm::vec3 d = probe.position - position;
        const float dist = glm::dot(d, d);
        if (dist < best_dist) {
            best_dist = dist;
            best = &probe;
        }
    }
    return best;
}

} // namespace dse::render
