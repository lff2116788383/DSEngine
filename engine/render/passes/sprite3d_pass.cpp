/**
 * @file sprite3d_pass.cpp
 * @brief Sprite3DPass implementation.
 */

#include "engine/render/passes/sprite3d_pass.h"

#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/world.h"
#include "engine/platform/screen.h"
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

        const float world_z = transform.local_to_world[3].z;
        item.sprite3d_depth_bucket = static_cast<int>(
            std::floor((world_z + sprite.sorting_bias) * kSprite3DDepthBucketScale));

        frame_items_.push_back(item);
    }

    std::stable_sort(frame_items_.begin(), frame_items_.end(),
                     [](const SpriteDrawItem& a, const SpriteDrawItem& b) {
        // M2 key: (depth_bucket, texture, blend). Smaller bucket/bias is
        // drawn first; with depth_test=Less and depth_write=on the first
        // equal-depth sprite wins, so foreground layers (negative bias) cover
        // the character correctly.
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
}

void Sprite3DPass::Render(CommandBuffer& cmd, const FrameContext& frame) {
    if (!rhi_device_ || frame_items_.empty()) return;
    const glm::vec2 viewport(static_cast<float>(Screen::render_width()),
                             static_cast<float>(Screen::render_height()));
    batch_.DrawSprite3D(cmd, *rhi_device_, frame_items_,
                        frame.view, frame.projection, viewport, frame.camera_offset);
}

void Sprite3DPass::Shutdown() {
    if (rhi_device_) batch_.Shutdown(*rhi_device_);
}

} // namespace render
} // namespace dse
