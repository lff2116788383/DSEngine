#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/base/time.h"
#include <algorithm>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>

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
    cmd_buffer.DrawBatch(items);
}

void UIRenderSystem::Render(World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height) {
    std::vector<SpriteDrawItem> items;
    auto view = world.registry().view<UIRendererComponent>();
    
    for (auto entity : view) {
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible) {
            continue;
        }
        
        SpriteDrawItem item;
        item.texture_handle = ui.texture_handle;
        
        // 1. Calculate Anchor Position (assuming parent is screen for now, in a real system we'd traverse a hierarchy)
        float parent_w = static_cast<float>(screen_width);
        float parent_h = static_cast<float>(screen_height);
        
        glm::vec2 anchor_pos = glm::vec2(
            parent_w * ui.anchor_min.x,
            parent_h * ui.anchor_min.y
        );

        // 2. Calculate Pivot Offset
        glm::vec2 pivot_offset = glm::vec2(
            -ui.size.x * ui.pivot.x,
            -ui.size.y * ui.pivot.y
        );

        // 3. Final Position
        glm::vec2 final_pos = anchor_pos + ui.position + pivot_offset;

        // Build model matrix for UI
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(final_pos.x, final_pos.y, 0.0f));
        model = glm::scale(model, glm::vec3(ui.size.x, ui.size.y, 1.0f));
        
        ui.runtime_model = model; // Cache for raycasting/event bubbling
        item.model = model;
        
        // Apply tint based on event state
        glm::vec4 final_color = ui.color;
        if (ui.interactable) {
            if (ui.is_pressed) final_color *= 0.8f;
            else if (ui.is_hovered) final_color *= 1.2f;
        }

        item.color = final_color;
        item.uv = ui.uv;
        item.sorting_layer = 1000; // UI is usually on top
        item.order_in_layer = ui.order;
        items.push_back(item);
    }
    
    if (items.empty()) {
        return;
    }

    std::sort(items.begin(), items.end(), [](const SpriteDrawItem& a, const SpriteDrawItem& b) {
        if (a.texture_handle != b.texture_handle) {
            return a.texture_handle < b.texture_handle;
        }
        return a.order_in_layer < b.order_in_layer;
    });
    
    // Orthographic projection matching screen pixels, origin at bottom-left
    glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(screen_width), 0.0f, static_cast<float>(screen_height), -1.0f, 1.0f);
    glm::mat4 view_mat = glm::mat4(1.0f);
    
    cmd_buffer.SetCamera(view_mat, ortho);
    cmd_buffer.DrawBatch(items);
}
