/**
 * @file sprite3d_pass.cpp
 * @brief Sprite3DPass implementation.
 */

#include "engine/render/passes/sprite3d_pass.h"

#include "engine/core/env_config.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/world.h"
#include "engine/platform/screen.h"
#include "engine/render/mesh_renderer.h"
#include "engine/render/render_scene_view.h"
#include "engine/render/render_snapshot.h"
#include "engine/render/rhi/rhi_device.h"

#include <algorithm>
#include <cmath>

namespace dse {
namespace render {

namespace {
// Coarse depth buckets keep batching effective while preserving front/back
// ordering. Smaller values => fewer draw calls; larger values => finer sorting.
// 10.0 gives 0.1 world-unit resolution and keeps the M2 perf scene under the
// 100-draw-call target when using a shared atlas texture.
constexpr float kSprite3DDepthBucketScale = 10.0f;

const glm::vec2 kSprite3DCorner[4] = {
    {-0.5f, 0.0f},
    { 0.5f, 0.0f},
    { 0.5f, 1.0f},
    {-0.5f, 1.0f},
};

void BuildBillboardAxes(const SpriteDrawItem& item, const glm::mat4& view,
                        glm::vec3& right, glm::vec3& up) {
    right = glm::vec3(1.0f, 0.0f, 0.0f);
    up = glm::vec3(0.0f, 1.0f, 0.0f);

    if (item.sprite3d_billboard == 0) {
        const glm::vec3 mx(item.model[0]);
        const glm::vec3 my(item.model[1]);
        const float lx = glm::length(mx);
        const float ly = glm::length(my);
        if (lx > 1.0e-6f) right = mx / lx;
        if (ly > 1.0e-6f) up = my / ly;
        return;
    }

    // view row 0 = right, view row 1 = up (GLM is column-major).
    right = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
    if (item.sprite3d_billboard == 1) {
        up = glm::vec3(0.0f, 1.0f, 0.0f);
    } else {
        up = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
    }
}

}  // namespace

void Sprite3DPass::ExtractFrameRenderData(World& world) {
    frame_items_.clear();

    auto view = world.registry().view<Sprite3DComponent, TransformComponent>();
    frame_items_.reserve(view.size_hint());

    for (auto entity : view) {
        const auto& sprite = view.get<Sprite3DComponent>(entity);
        const auto& transform = view.get<TransformComponent>(entity);

        SpriteDrawItem item;
        item.sprite3d = true;
        item.texture_handle = sprite.texture_handle;
        item.uv = sprite.uv_rect;  // (u0, v0, u1, v1) for the Sprite3D path
        item.model = transform.local_to_world;
        item.color = sprite.color_tint;
        item.color.a *= sprite.opacity;
        item.sprite3d_size = glm::vec2(sprite.size_w, sprite.size_h);
        item.sprite3d_anchor_y = sprite.anchor_y;
        item.sprite3d_billboard = sprite.billboard;
        item.sprite3d_z_offset = sprite.z_offset;
        item.sprite3d_sorting_bias = sprite.sorting_bias;
        item.sprite3d_emissive = sprite.emissive;
        item.sprite3d_lit = sprite.lit;
        item.sprite3d_receive_shadow = sprite.receive_shadow;
        item.sprite3d_normal_handle = sprite.normal_handle;
        item.sprite3d_emissive_handle = sprite.emissive_handle;
        item.sprite3d_normal_strength = sprite.normal_strength;
        item.sprite3d_contact_shadow = sprite.contact_shadow;
        item.sprite3d_contact_shadow_radius = sprite.contact_shadow_radius;
        item.sprite3d_contact_shadow_opacity = sprite.contact_shadow_opacity;

        frame_items_.push_back(item);
    }
    // Sorting needs the frame view matrix (camera-relative depth), so it is
    // performed in Render() instead of here.
}

void Sprite3DPass::Render(CommandBuffer& cmd, const FrameContext& frame) {
    if (!rhi_device_ || frame_items_.empty()) return;

    // M2 sort key: (depth_bucket, texture, blend). depth_bucket is derived from
    // camera-relative view-space depth so camera rotation/movement do not make
    // absolute world Z diverge from actual view depth. A smaller sorting_bias is
    // closer to the camera. Negating the effective distance keeps the ascending
    // stable_sort order far -> near, preserving depth-test friendly overdraw.
    for (auto& item : frame_items_) {
        const glm::vec3 world_pos = glm::vec3(item.model[3]);
        const glm::vec3 relative_pos = world_pos - frame.camera_offset;
        const float view_depth = -(frame.view * glm::vec4(relative_pos, 1.0f)).z;
        const float effective_depth = view_depth + item.sprite3d_sorting_bias;
        item.sprite3d_depth_bucket = static_cast<int>(
            std::floor(-effective_depth * kSprite3DDepthBucketScale));
    }

    std::stable_sort(frame_items_.begin(), frame_items_.end(),
                     [](const SpriteDrawItem& a, const SpriteDrawItem& b) {
        if (a.sprite3d_depth_bucket != b.sprite3d_depth_bucket)
            return a.sprite3d_depth_bucket < b.sprite3d_depth_bucket;
        const uint32_t at = a.texture_handle.raw();
        const uint32_t bt = b.texture_handle.raw();
        if (at != bt) return at < bt;
        if (a.blend_mode != b.blend_mode) return a.blend_mode < b.blend_mode;
        if (a.sprite3d_sorting_bias != b.sprite3d_sorting_bias)
            return a.sprite3d_sorting_bias < b.sprite3d_sorting_bias;
        return a.order_in_layer < b.order_in_layer;
    });

    // sorting_bias < 0 is the explicit foreground overlay contract: unlit items
    // are drawn after the depth-tested Sprite3D batch with depth test/write off,
    // so a physically farther foreground layer can still cover a closer actor.
    auto normal_end = std::stable_partition(
        frame_items_.begin(), frame_items_.end(),
        [](const SpriteDrawItem& item) { return item.sprite3d_sorting_bias >= 0.0f; });

    std::vector<SpriteDrawItem> normal_unlit;
    std::vector<SpriteDrawItem> normal_lit;
    std::vector<SpriteDrawItem> fg_unlit;
    for (auto it = frame_items_.begin(); it != normal_end; ++it) {
        if (!it->sprite3d_lit) normal_unlit.push_back(*it);
        else normal_lit.push_back(*it);
    }
    for (auto it = normal_end; it != frame_items_.end(); ++it) {
        if (!it->sprite3d_lit) fg_unlit.push_back(*it);
        else normal_lit.push_back(*it);  // lit foreground keeps depth semantics
    }

    const glm::vec2 viewport(static_cast<float>(Screen::render_width()),
                             static_cast<float>(Screen::render_height()));
    std::vector<SpriteDrawItem> contact_items;
    const bool has_contact = std::any_of(frame_items_.begin(), frame_items_.end(),
        [](const SpriteDrawItem& it) { return it.sprite3d_contact_shadow; });
    if (has_contact) EnsureContactShadowTexture(*rhi_device_);
    for (const auto& item : frame_items_) {
        if (!item.sprite3d_contact_shadow || !contact_shadow_tex_) continue;
        SpriteDrawItem shadow = item;
        shadow.texture_handle = contact_shadow_tex_;
        shadow.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        shadow.sprite3d_lit = false;
        shadow.sprite3d_receive_shadow = false;
        shadow.sprite3d_billboard = 0;  // None: explicit local XZ axes below
        shadow.sprite3d_anchor_y = 0.5f;
        shadow.sprite3d_size = glm::vec2(item.sprite3d_contact_shadow_radius * 2.0f,
                                         item.sprite3d_contact_shadow_radius * 2.0f);
        shadow.sprite3d_z_offset = 0.0f;
        shadow.sprite3d_sorting_bias = 0.0f;
        shadow.color = glm::vec4(0.0f, 0.0f, 0.0f, item.sprite3d_contact_shadow_opacity);
        shadow.model = glm::mat4(1.0f);
        shadow.model[0] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);  // local right = world +X
        shadow.model[1] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);  // local up = world +Z
        shadow.model[2] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        const glm::vec3 p = glm::vec3(item.model[3]) + glm::vec3(0.0f, 0.01f, 0.0f);
        shadow.model[3] = glm::vec4(p, 1.0f);
        contact_items.push_back(shadow);
    }
    if (!contact_items.empty()) {
        batch_.DrawSprite3D(cmd, *rhi_device_, contact_items,
                            frame.view, frame.projection, viewport, frame.camera_offset, false,
                            frame.snapshot);
    }
    if (!normal_unlit.empty()) {
        batch_.DrawSprite3D(cmd, *rhi_device_, normal_unlit,
                            frame.view, frame.projection, viewport, frame.camera_offset, false,
                            frame.snapshot);
    }
    if (!normal_lit.empty()) {
        RenderLit(cmd, frame, normal_lit);
    }
    if (!fg_unlit.empty()) {
        batch_.DrawSprite3D(cmd, *rhi_device_, fg_unlit,
                            frame.view, frame.projection, viewport, frame.camera_offset, true,
                            frame.snapshot);
    }
}

void Sprite3DPass::RenderLit(CommandBuffer& cmd, const FrameContext& frame,
                             const std::vector<SpriteDrawItem>& items) {
    if (!frame.mesh_renderer || items.empty()) return;

    DirectionalLight light{};
    light.enabled = false;
    if (frame.snapshot && frame.snapshot->directional_light.valid) {
        const auto& dl = frame.snapshot->directional_light;
        light.direction = dl.direction;
        light.color = dl.color;
        light.intensity = dl.intensity;
        light.ambient = dl.ambient_intensity;
        light.enabled = true;
    } else if (frame.scene_view && !frame.scene_view->directional_lights.empty()) {
        const auto& dl = frame.scene_view->directional_lights.front();
        light.direction = dl.direction;
        light.color = dl.color;
        light.intensity = dl.intensity;
        light.ambient = 0.1f;
        light.enabled = true;
    }

    std::vector<ShadedPointLight> point_lights;
    std::vector<ShadedSpotLight> spot_lights;
    int point_shadow_index = 0;
    int spot_shadow_index = 0;
    if (frame.scene_view) {
        point_lights.reserve(frame.scene_view->point_lights.size());
        for (const auto& pl : frame.scene_view->point_lights) {
            ShadedPointLight out{};
            out.color = pl.color;
            out.intensity = pl.intensity;
            out.position = pl.position - frame.camera_offset;
            out.radius = pl.radius * std::max(0.1f, pl.falloff);
            if (pl.cast_shadow && point_shadow_index < 4) {
                out.cast_shadow = true;
                out.shadow_index = point_shadow_index++;
            }
            point_lights.push_back(out);
        }

        spot_lights.reserve(frame.scene_view->spot_lights.size());
        for (const auto& sl : frame.scene_view->spot_lights) {
            ShadedSpotLight out{};
            out.color = sl.color;
            out.intensity = sl.intensity;
            out.position = sl.position - frame.camera_offset;
            out.direction = sl.direction;
            out.radius = sl.range * std::max(0.1f, sl.falloff);
            out.inner_cone = sl.inner_cone;
            out.outer_cone = sl.outer_cone;
            if (sl.cast_shadow && spot_shadow_index < 4) {
                out.cast_shadow = true;
                out.shadow_index = spot_shadow_index++;
            }
            spot_lights.push_back(out);
        }
    }

    const glm::mat4 identity(1.0f);
    const glm::vec3 camera_pos(0.0f);

    for (const auto& item : items) {
        glm::vec3 right;
        glm::vec3 up;
        BuildBillboardAxes(item, frame.view, right, up);

        glm::vec3 base = glm::vec3(item.model[3]) - frame.camera_offset;
        base.z += item.sprite3d_z_offset;

        const float u0 = item.uv.x;
        const float v0 = item.uv.y;
        const float u1 = item.uv.z;
        const float v1 = item.uv.w;
        const glm::vec2 uvs[4] = {
            {u0, v0}, {u1, v0}, {u1, v1}, {u0, v1},
        };

        std::vector<MeshVertex> vertices(4);
        for (int k = 0; k < 4; ++k) {
            const glm::vec2 corner = kSprite3DCorner[k];
            const glm::vec3 world_pos = base
                + right * (corner.x * item.sprite3d_size.x)
                + up * ((corner.y - item.sprite3d_anchor_y) * item.sprite3d_size.y);

            MeshVertex& v = vertices[static_cast<size_t>(k)];
            v.position = world_pos;
            v.color = item.color;
            v.uv = uvs[k];
            v.tangent = glm::normalize(right);
            v.normal = glm::normalize(glm::cross(right, up));
            if (glm::length(v.normal) < 1.0e-6f) v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        const std::vector<uint16_t> indices = {0, 1, 2, 0, 2, 3};

        ShadedMaterial material{};
        material.albedo = item.color.rgb;
        material.metallic = 0.0f;
        material.roughness = 0.65f;
        material.ao = 1.0f;
        material.alpha_cutoff = 0.1f;
        material.alpha_test = true;
        material.double_sided = true;  // billboard must not be culled by winding
        material.emissive = item.sprite3d_emissive;
        material.receive_shadow = item.sprite3d_receive_shadow;
        material.shadow_strength = 1.0f;
        material.albedo_tex = item.texture_handle;
        material.normal_tex = item.sprite3d_normal_handle;
        material.normal_strength = item.sprite3d_normal_strength;
        material.emissive_tex = item.sprite3d_emissive_handle;
        const bool force_ubo = core::env::IsSet(core::env::names::kSprite3dForceUbo);
        material.direct_cluster_lights =
            !force_ubo && frame.light_buffer != nullptr && frame.cluster_grid != nullptr;

        frame.mesh_renderer->DrawShaded(cmd, *rhi_device_, vertices, indices, identity,
                                         frame.view, frame.projection, camera_pos,
                                         material, light, point_lights, ShadedGI{}, spot_lights,
                                         frame.light_buffer, frame.cluster_grid);
    }
}

void Sprite3DPass::EnsureContactShadowTexture(RhiDevice& device) {
    if (contact_shadow_tex_) return;

    constexpr int kSize = 64;
    std::vector<unsigned char> pixels(static_cast<size_t>(kSize) * kSize * 4u, 0);
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float nx = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize);
            const float ny = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize);
            const float dx = (nx - 0.5f) * 2.0f;
            const float dy = (ny - 0.5f) * 2.0f;
            const float d = std::sqrt(dx * dx + dy * dy);
            float a = 1.0f - std::min(d, 1.0f);
            a = a * a * (3.0f - 2.0f * a);  // smoothstep
            const size_t i = (static_cast<size_t>(y) * kSize + x) * 4u;
            pixels[i + 0] = 0;
            pixels[i + 1] = 0;
            pixels[i + 2] = 0;
            pixels[i + 3] = static_cast<unsigned char>(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }

    TextureSamplerDesc sampler{};
    sampler.filter = TextureFilter::Linear;
    sampler.wrap = TextureWrap::ClampToEdge;
    contact_shadow_tex_ = device.CreateTexture2D(kSize, kSize, pixels.data(), sampler);
}
void Sprite3DPass::Shutdown() {
    if (rhi_device_) {
        if (contact_shadow_tex_) rhi_device_->DeleteTexture(contact_shadow_tex_);
        batch_.Shutdown(*rhi_device_);
    }
    contact_shadow_tex_ = {};
}

} // namespace render
} // namespace dse
