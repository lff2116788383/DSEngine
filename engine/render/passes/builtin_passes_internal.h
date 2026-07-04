/**
 * @file builtin_passes_internal.h
 * @brief Shared helpers for builtin render pass implementations.
 */
#ifndef DSE_BUILTIN_PASSES_INTERNAL_H
#define DSE_BUILTIN_PASSES_INTERNAL_H

#include "engine/render/render_snapshot.h"
#include "engine/render/passes/render_pass_context.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace dse::render::pass_internal {

inline glm::vec3 FindShadowCenter(const RenderThinSnapshot& snapshot) {
    return snapshot.camera_3d.shadow_center;
}

/// Camera parameters for the camera the scene RT is actually rendered with
/// (editor camera override when active, otherwise the game camera snapshot).
/// view is camera-relative (consistent with camera-offset-shifted geometry);
/// proj carries NO backend clip correction.
struct ActiveCamera {
    bool valid = false;
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    float near_clip = 0.1f;
    float far_clip = 1000.0f;
    float fov_y = 60.0f;   // degrees
    float aspect = 1.0f;
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
};

inline ActiveCamera GetActiveCamera(const RenderPassContext& ctx, float fallback_aspect) {
    ActiveCamera cam;
    cam.aspect = fallback_aspect;
    const auto& snap = *ctx.snapshot;
    if (ctx.editor_mode && ctx.use_editor_camera) {
        cam.valid = true;
        cam.view = ctx.editor_view * glm::translate(glm::mat4(1.0f), ctx.camera_offset);
        cam.proj = ctx.editor_projection;
        const glm::mat4& p = ctx.editor_projection;
        if (p[2][3] != 0.0f) {  // perspective, GL depth convention (glm::perspective)
            const float p22 = p[2][2];
            const float p32 = p[3][2];
            if (p22 - 1.0f != 0.0f) cam.near_clip = p32 / (p22 - 1.0f);
            if (p22 + 1.0f != 0.0f) cam.far_clip  = p32 / (p22 + 1.0f);
            if (p[1][1] != 0.0f) {
                cam.fov_y = glm::degrees(2.0f * std::atan(1.0f / p[1][1]));
                if (p[0][0] != 0.0f) cam.aspect = p[1][1] / p[0][0];
            }
        }
        cam.right   = glm::vec3(cam.view[0][0], cam.view[1][0], cam.view[2][0]);
        cam.up      = glm::vec3(cam.view[0][1], cam.view[1][1], cam.view[2][1]);
        cam.forward = -glm::vec3(cam.view[0][2], cam.view[1][2], cam.view[2][2]);
    } else if (snap.camera_3d.valid) {
        cam.valid = true;
        cam.view = snap.camera_3d.view;
        cam.near_clip = snap.camera_3d.near_clip;
        cam.far_clip = snap.camera_3d.far_clip;
        cam.fov_y = snap.camera_3d.fov;
        cam.proj = glm::perspective(glm::radians(cam.fov_y), cam.aspect, cam.near_clip, cam.far_clip);
        cam.forward = snap.camera_3d.forward;
        cam.right = snap.camera_3d.right;
        cam.up = snap.camera_3d.up;
    }
    return cam;
}

struct DirectionalLightCamera {
    glm::mat4 view;
    glm::mat4 projection;
};

struct CascadeFit {
    float size;
    glm::vec3 center;
};

inline CascadeFit ComputeCascadeFit(
        const glm::mat4& inv_view,
        const glm::vec3& light_direction,
        float split_near,
        float split_far,
        float aspect,
        float tan_half_fov) {
    const glm::mat4 light_view = glm::lookAt(
        glm::vec3(0.0f), -glm::normalize(light_direction), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 inv_light_view = glm::inverse(light_view);

    glm::vec3 min_ls(1e9f);
    glm::vec3 max_ls(-1e9f);

    const float near_planes[2] = {split_near, split_far};
    for (float plane_dist : near_planes) {
        const float half_h = plane_dist * tan_half_fov;
        const float half_w = half_h * aspect;
        const glm::vec3 view_corners[4] = {
            {-half_w, -half_h, -plane_dist},
            { half_w, -half_h, -plane_dist},
            { half_w,  half_h, -plane_dist},
            {-half_w,  half_h, -plane_dist},
        };
        for (const auto& vc : view_corners) {
            const glm::vec3 world = glm::vec3(inv_view * glm::vec4(vc, 1.0f));
            const glm::vec3 ls = glm::vec3(light_view * glm::vec4(world, 1.0f));
            min_ls = glm::min(min_ls, ls);
            max_ls = glm::max(max_ls, ls);
        }
    }

    const float extent_x = (max_ls.x - min_ls.x) * 0.5f;
    const float extent_y = (max_ls.y - min_ls.y) * 0.5f;
    const glm::vec3 center_ls = (min_ls + max_ls) * 0.5f;
    const glm::vec3 center_world = glm::vec3(inv_light_view * glm::vec4(center_ls, 1.0f));
    return { std::max(extent_x, extent_y) * 1.05f, center_world };
}

inline DirectionalLightCamera ComputeDirectionalLightCamera(
        const glm::vec3& shadow_center,
        const glm::vec3& light_direction,
        float ortho_size,
        const glm::mat4& clip_correction,
        float shadow_map_res = 0.0f) {
    float far_dist = ortho_size * 4.0f;
    glm::vec3 light_dir_n = glm::normalize(light_direction);

    glm::vec3 center = shadow_center;
    if (shadow_map_res > 0.0f) {
        float texel_world_size = (2.0f * ortho_size) / shadow_map_res;
        glm::mat4 lv = glm::lookAt(
            shadow_center - light_dir_n * (far_dist * 0.5f),
            shadow_center, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::vec4 sc_ls = lv * glm::vec4(shadow_center, 1.0f);
        sc_ls.x = std::floor(sc_ls.x / texel_world_size) * texel_world_size;
        sc_ls.y = std::floor(sc_ls.y / texel_world_size) * texel_world_size;
        center = glm::vec3(glm::inverse(lv) * sc_ls);
    }

    glm::vec3 light_pos = center - light_dir_n * (far_dist * 0.5f);
    return {
        glm::lookAt(light_pos, center, glm::vec3(0.0f, 1.0f, 0.0f)),
        clip_correction * glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, 1.0f, far_dist)
    };
}

} // namespace dse::render::pass_internal

#endif // DSE_BUILTIN_PASSES_INTERNAL_H
