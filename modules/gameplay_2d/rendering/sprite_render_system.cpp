/**
 * @file sprite_render_system.cpp
 * @brief 渲染硬件接口(RHI)抽象层，提供跨图形API的底层渲染命令封装
 */

#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/base/time.h"
#include <algorithm>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>

void Expand9SliceItems(const SpriteDrawItem& base_item,
                       const glm::vec2& final_pos,
                       const glm::vec2& size,
                       const glm::vec4& uv,
                       const glm::vec4& border,
                       const glm::vec2& src_size,
                       std::vector<SpriteDrawItem>& out_items) {
    // 边框 UV 分量 → 屏幕像素尺寸
    // src_size > 0：固定角块模式（弹性面板）；= 0：等比缩放模式。
    const bool fixed_corner = src_size.x > 0.0f && src_size.y > 0.0f;
    float bl = border.x * (fixed_corner ? src_size.x : size.x);   // left
    float bb = border.y * (fixed_corner ? src_size.y : size.y);   // bottom
    float br = border.z * (fixed_corner ? src_size.x : size.x);   // right
    float bt = border.w * (fixed_corner ? src_size.y : size.y);   // top

    float x0 = final_pos.x - size.x * 0.5f;
    float y0 = final_pos.y - size.y * 0.5f;

    // 屏幕列分割点 (x) 和行分割点 (y, y 轴朝上)
    float xcols[4] = { x0, x0 + bl, x0 + size.x - br, x0 + size.x };
    float yrows[4] = { y0, y0 + bb, y0 + size.y - bt, y0 + size.y };

    // UV 列分割点
    float ucols[4] = {
        uv.x,
        uv.x + border.x * uv.z,
        uv.x + (1.0f - border.z) * uv.z,
        uv.x + uv.z
    };
    // UV 行分割点
    float vrows[4] = {
        uv.y,
        uv.y + border.y * uv.w,
        uv.y + (1.0f - border.w) * uv.w,
        uv.y + uv.w
    };

    out_items.reserve(out_items.size() + 9);
    for (int col = 0; col < 3; ++col) {
        float cw = xcols[col + 1] - xcols[col];
        if (cw <= 0.0f) continue;
        for (int row = 0; row < 3; ++row) {
            float ch = yrows[row + 1] - yrows[row];
            if (ch <= 0.0f) continue;

            float cx = (xcols[col] + xcols[col + 1]) * 0.5f;
            float cy = (yrows[row] + yrows[row + 1]) * 0.5f;

            SpriteDrawItem item = base_item;
            glm::mat4 m = glm::mat4(1.0f);
            m = glm::translate(m, glm::vec3(cx, cy, 0.0f));
            m = glm::scale(m, glm::vec3(cw, ch, 1.0f));
            item.model = m;
            item.uv = glm::vec4(
                ucols[col],
                vrows[row],
                ucols[col + 1] - ucols[col],
                vrows[row + 1] - vrows[row]
            );
            out_items.push_back(item);
        }
    }
}

void SpriteRenderSystem::Render(World& world, CommandBuffer& cmd_buffer) {
    std::vector<SpriteDrawItem> items;
    auto view = world.registry().view<SpriteRendererComponent, TransformComponent>();
    
    for (auto entity : view) {
        auto& sprite = view.get<SpriteRendererComponent>(entity);
        if (!sprite.visible) {
            continue;
        }
        auto& transform = view.get<TransformComponent>(entity);
        
        SpriteDrawItem item;
        item.texture_handle = sprite.texture_handle;
        item.material_instance_id = sprite.material_instance_id;
        item.shader_variant_key = static_cast<unsigned int>(std::hash<std::string>{}(sprite.shader_variant));
        item.blend_mode = static_cast<unsigned int>(sprite.blend_mode);
        item.model = transform.local_to_world;
        item.color = sprite.color;
        sprite.uv_offset += sprite.uv_scroll_speed * Time::delta_time();
        item.uv = sprite.uv;
        item.uv.x += sprite.uv_offset.x;
        item.uv.y += sprite.uv_offset.y;
        item.sorting_layer = sprite.sorting_layer;
        item.order_in_layer = sprite.order_in_layer;
        items.push_back(item);
    }
    
    std::sort(items.begin(), items.end(), [](const SpriteDrawItem& a, const SpriteDrawItem& b) {
        if (a.sorting_layer != b.sorting_layer) {
            return a.sorting_layer < b.sorting_layer;
        }
        if (a.shader_variant_key != b.shader_variant_key) {
            return a.shader_variant_key < b.shader_variant_key;
        }
        if (a.material_instance_id != b.material_instance_id) {
            return a.material_instance_id < b.material_instance_id;
        }
        if (a.texture_handle != b.texture_handle) {
            return a.texture_handle < b.texture_handle;
        }
        if (a.blend_mode != b.blend_mode) {
            return a.blend_mode < b.blend_mode;
        }
        return a.order_in_layer < b.order_in_layer;
    });
    if (rhi_device_ && !items.empty()) {
        sprite_batch_.Draw(cmd_buffer, *rhi_device_, items,
                           cmd_buffer.GetViewMatrix(), cmd_buffer.GetProjectionMatrix());
    }
}

void UIRenderSystem::Render(World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height, const glm::mat4& clip_correction) {
    static const unsigned int kSdfVariantKey = static_cast<unsigned int>(std::hash<std::string>{}("TEXT_SDF"));
    std::vector<SpriteDrawItem> items;
    auto view = world.registry().view<UIRendererComponent>();
    
    for (auto entity : view) {
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible) {
            continue;
        }
        
        // 1. Calculate Anchor Position
        float parent_w = static_cast<float>(screen_width);
        float parent_h = static_cast<float>(screen_height);
        
        glm::vec2 anchor_pos = glm::vec2(
            parent_w * ui.anchor_min.x,
            parent_h * ui.anchor_min.y
        );

        // 2. Convert authoring position/pivot into the quad center expected by DrawSpriteBatch.
        glm::vec2 pivot_to_center = glm::vec2(
            ui.size.x * (0.5f - ui.pivot.x),
            ui.size.y * (0.5f - ui.pivot.y)
        );

        // 3. Final center position
        glm::vec2 final_pos = anchor_pos + ui.position + pivot_to_center;

        // Build model matrix for UI (used for raycasting and normal draw)
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(final_pos.x, final_pos.y, 0.0f));
        model = glm::scale(model, glm::vec3(ui.size.x, ui.size.y, 1.0f));
        
        ui.runtime_model = model; // Cache for raycasting/event bubbling
        
        // Apply tint based on event state
        glm::vec4 final_color = ui.color;
        if (ui.interactable) {
            if (ui.is_pressed) final_color *= 0.8f;
            else if (ui.is_hovered) final_color *= 1.2f;
        }

        const unsigned int sdf_key = ui.use_sdf_shader ? kSdfVariantKey : 0;

        SpriteVisualEffect vfx_data;
        auto* vfx = world.registry().try_get<UIVisualEffectComponent>(entity);
        if (vfx && (vfx->corner_radius > 0.0f || vfx->blur_radius > 0.0f ||
                    vfx->gradient_color_start != glm::vec4(1.0f) || vfx->gradient_color_end != glm::vec4(1.0f))) {
            vfx_data.enabled = true;
            vfx_data.corner_radius = vfx->corner_radius;
            vfx_data.gradient_start = vfx->gradient_color_start;
            vfx_data.gradient_end = vfx->gradient_color_end;
            vfx_data.gradient_direction = static_cast<float>(static_cast<int>(vfx->gradient_direction));
            vfx_data.blur_radius = vfx->blur_radius;
            vfx_data.blur_intensity = vfx->blur_intensity;
            vfx_data.rect_size = ui.size;
        }

        if (ui.nine_slice_enabled) {
            SpriteDrawItem base_item;
            base_item.texture_handle = ui.texture_handle;
            base_item.color = final_color;
            base_item.sorting_layer = 1000;
            base_item.order_in_layer = ui.order;
            base_item.shader_variant_key = sdf_key;
            base_item.visual_effect = vfx_data;
            base_item.sdf_threshold = ui.sdf_threshold;
            base_item.sdf_smoothing = ui.sdf_smoothing;
            base_item.sdf_outline_width = ui.sdf_outline_width;
            base_item.sdf_shadow_softness = ui.sdf_shadow_softness;
            Expand9SliceItems(base_item, final_pos, ui.size, ui.uv, ui.nine_slice_border, ui.nine_slice_src_size, items);
        } else {
            SpriteDrawItem item;
            item.texture_handle = ui.texture_handle;
            item.model = model;
            item.color = final_color;
            item.uv = ui.uv;
            item.sorting_layer = 1000;
            item.order_in_layer = ui.order;
            item.shader_variant_key = sdf_key;
            item.visual_effect = vfx_data;
            item.sdf_threshold = ui.sdf_threshold;
            item.sdf_smoothing = ui.sdf_smoothing;
            item.sdf_outline_width = ui.sdf_outline_width;
            item.sdf_shadow_softness = ui.sdf_shadow_softness;
            items.push_back(item);
        }
    }
    
    if (items.empty()) {
        return;
    }

    std::sort(items.begin(), items.end(), [](const SpriteDrawItem& a, const SpriteDrawItem& b) {
        if (a.order_in_layer != b.order_in_layer) {
            return a.order_in_layer < b.order_in_layer;
        }
        if (a.shader_variant_key != b.shader_variant_key) {
            return a.shader_variant_key < b.shader_variant_key;
        }
        return a.texture_handle < b.texture_handle;
    });
    
    // Orthographic projection matching screen pixels, origin at bottom-left
    glm::mat4 ortho = clip_correction * glm::ortho(0.0f, static_cast<float>(screen_width), 0.0f, static_cast<float>(screen_height), -1.0f, 1.0f);
    glm::mat4 view_mat = glm::mat4(1.0f);

    if (rhi_device_) {
        sprite_batch_.Draw(cmd_buffer, *rhi_device_, items, view_mat, ortho);
    }
}
