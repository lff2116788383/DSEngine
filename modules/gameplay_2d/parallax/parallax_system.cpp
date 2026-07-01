/**
 * @file parallax_system.cpp
 * @brief 视差滚动系统实现
 */

#include "modules/gameplay_2d/parallax/parallax_system.h"
#include "engine/ecs/parallax_2d.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/camera.h"
#include <algorithm>

void ParallaxSystem::Update(World& world, float delta_time) {
    auto& reg = world.registry();

    // Find active camera position
    glm::vec2 cam_pos = {0.0f, 0.0f};
    auto cam_view = reg.view<CameraComponent, TransformComponent>();
    for (auto e : cam_view) {
        auto& cam = cam_view.get<CameraComponent>(e);
        if (cam.enabled) {
            auto& t = cam_view.get<TransformComponent>(e);
            cam_pos = glm::vec2(t.position.x, t.position.y);
            break;
        }
    }

    // Update parallax layers
    auto view = reg.view<ParallaxComponent>();
    for (auto entity : view) {
        auto& parallax = view.get<ParallaxComponent>(entity);
        if (!parallax.enabled) continue;

        parallax.accumulated_scroll.x += delta_time;
        parallax.accumulated_scroll.y += delta_time;

        for (auto& layer : parallax.layers) {
            if (!layer.visible) continue;
            // UV offset based on camera position and scroll factor
            layer.offset_x = cam_pos.x * layer.scroll_factor_x + layer.auto_scroll_x * parallax.accumulated_scroll.x;
            layer.offset_y = cam_pos.y * layer.scroll_factor_y + layer.auto_scroll_y * parallax.accumulated_scroll.y;
        }
    }
}

void ParallaxSystem::Render(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame) {
    if (!rhi_device_) return;

    auto& reg = world.registry();
    std::vector<SpriteDrawItem> items;

    auto view = reg.view<ParallaxComponent, TransformComponent>();
    for (auto entity : view) {
        auto& parallax = view.get<ParallaxComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        if (!parallax.enabled) continue;

        for (auto& layer : parallax.layers) {
            if (!layer.visible || layer.texture_handle == 0) continue;

            SpriteDrawItem item{};
            item.texture_handle = layer.texture_handle;
            item.color = layer.tint * glm::vec4(1.0f, 1.0f, 1.0f, layer.opacity);
            item.sorting_layer = layer.sorting_order;

            // Position quad covering viewport with UV offset for scrolling
            float s = layer.scale;
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(transform.position.x, transform.position.y, 0.0f));
            model = glm::scale(model, glm::vec3(s, s, 1.0f));
            item.model = model;

            // UV offset for parallax scrolling
            item.uv = glm::vec4(layer.offset_x, layer.offset_y, 1.0f, 1.0f);

            items.push_back(item);
        }
    }

    if (!items.empty()) {
        std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
            return a.sorting_layer < b.sorting_layer;
        });
        sprite_batch_.Draw(cmd_buffer, *rhi_device_, items, frame.view, frame.projection);
    }
}
