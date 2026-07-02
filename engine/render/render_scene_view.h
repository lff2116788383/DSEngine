/**
 * @file render_scene_view.h
 * @brief Read-only render scene view decoupling the render layer from ECS.
 *
 * Render passes can operate on RenderSceneView instead of directly querying
 * entt::registry. Populated once per frame via ExtractRenderSceneView(World&).
 */
#ifndef DSE_RENDER_RENDER_SCENE_VIEW_H
#define DSE_RENDER_RENDER_SCENE_VIEW_H

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "engine/core/dse_export.h"

class World;

namespace dse::render {

struct RenderPointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius    = 10.0f;
    float falloff   = 1.0f;
    bool  cast_shadow = false;
    uint32_t shadow_map_handle = 0;
};

struct RenderSpotLight {
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};  ///< World-space direction (pre-rotated by transform)
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float range     = 10.0f;
    float falloff   = 1.0f;
    float inner_cone = 0.9f;
    float outer_cone = 0.8f;
    bool  cast_shadow = false;
    uint32_t shadow_map_handle = 0;
};

struct RenderDirectionalLight {
    glm::vec3 direction{-0.4f, -1.0f, -0.3f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    bool  cast_shadow = true;
    int   cascade_count = 3;
    float cascade_split_lambda = 0.75f;
};

struct RenderReflectionProbe {
    glm::vec3 position{0.0f};
    glm::vec3 box_min{-1.0f};
    glm::vec3 box_max{1.0f};
    uint32_t  cubemap_handle = 0;
    float     intensity = 1.0f;
    float     influence_radius = 10.0f;
    bool      box_projection = false;
};

struct RenderLightProbe {
    glm::vec3 position{0.0f};
    float     radius = 10.0f;
    float     sh_coefficients[27]{};
    bool      baked = false;
};

struct RenderSkybox {
    uint32_t cubemap_handle = 0;
    float    intensity = 1.0f;
    float    rotation  = 0.0f;
    bool     present   = false;
};

struct RenderMeshletInstance {
    uint32_t  mesh_id = 0;
    int       material_index = 0;
    glm::mat4 local_to_world{1.0f};
};

struct RenderLightmap {
    uint32_t entity_index = 0;
    uint32_t lightmap_handle = 0;
    float    intensity = 1.0f;
    glm::vec4 st_offset{1.0f, 1.0f, 0.0f, 0.0f};
    bool     use_ao = true;
};

/**
 * @struct RenderSceneView
 * @brief Frame-snapshot of all render-relevant data, extracted from ECS.
 *
 * Render passes can read from this struct instead of querying entt::registry.
 * Updated once per frame before render pass execution.
 */
struct DSE_EXPORT RenderSceneView {
    std::vector<RenderPointLight>       point_lights;
    std::vector<RenderSpotLight>        spot_lights;
    std::vector<RenderDirectionalLight> directional_lights;
    std::vector<RenderReflectionProbe>  reflection_probes;
    std::vector<RenderLightProbe>       light_probes;
    std::vector<RenderLightmap>         lightmaps;
    std::vector<RenderMeshletInstance>  meshlet_instances;
    RenderSkybox                        skybox;

    void Clear() {
        point_lights.clear();
        spot_lights.clear();
        directional_lights.clear();
        reflection_probes.clear();
        light_probes.clear();
        lightmaps.clear();
        meshlet_instances.clear();
        skybox = {};
    }
};

/// Extract render-relevant data from the ECS world into a RenderSceneView snapshot.
DSE_EXPORT void ExtractRenderSceneView(const World& world, RenderSceneView& out);

/// Find the closest reflection probe to a position.
DSE_EXPORT const RenderReflectionProbe* FindClosestReflectionProbe(
    const RenderSceneView& scene, const glm::vec3& position);

/// Find the closest light probe to a position.
DSE_EXPORT const RenderLightProbe* FindClosestLightProbe(
    const RenderSceneView& scene, const glm::vec3& position);

} // namespace dse::render

#endif // DSE_RENDER_RENDER_SCENE_VIEW_H
