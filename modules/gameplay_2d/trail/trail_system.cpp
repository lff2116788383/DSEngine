/**
 * @file trail_system.cpp
 * @brief 2D 拖尾渲染系统实现
 */

#include "modules/gameplay_2d/trail/trail_system.h"
#include "engine/ecs/trail_renderer_2d.h"
#include "engine/ecs/transform.h"
#include <algorithm>
#include <cmath>

void TrailSystem::Update(World& world, float delta_time) {
    auto& reg = world.registry();
    auto view = reg.view<TrailRenderer2DComponent, TransformComponent>();

    for (auto entity : view) {
        auto& trail = view.get<TrailRenderer2DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        // Age existing points
        for (auto& point : trail.points) {
            point.life_remaining -= delta_time;
        }

        // Remove dead points
        trail.points.erase(
            std::remove_if(trail.points.begin(), trail.points.end(),
                [](const TrailPoint& p) { return p.life_remaining <= 0.0f; }),
            trail.points.end());

        // Emit new point if emitting and moved enough
        if (trail.emitting) {
            glm::vec2 current_pos = glm::vec2(transform.position.x, transform.position.y);
            bool should_add = trail.points.empty();

            if (!should_add) {
                float dist = glm::length(current_pos - trail.points.back().position);
                should_add = dist >= trail.min_vertex_distance;
            }

            if (should_add && (int)trail.points.size() < trail.max_points) {
                TrailPoint point;
                point.position = current_pos;
                point.width = trail.start_width;
                point.life_remaining = trail.lifetime;
                point.color = trail.start_color;
                trail.points.push_back(point);
            }
        }

        // Interpolate width and color based on remaining life
        for (auto& point : trail.points) {
            float t = 1.0f - (point.life_remaining / trail.lifetime);
            t = std::clamp(t, 0.0f, 1.0f);
            point.width = glm::mix(trail.start_width, trail.end_width, t);
            point.color = glm::mix(trail.start_color, trail.end_color, t);
        }
    }
}

void TrailSystem::Render(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame) {
    if (!rhi_device_) return;

    auto& reg = world.registry();
    std::vector<SpriteDrawItem> items;

    auto view = reg.view<TrailRenderer2DComponent>();
    for (auto entity : view) {
        auto& trail = view.get<TrailRenderer2DComponent>(entity);
        if (trail.points.size() < 2) continue;

        // Generate quad strip from trail points
        for (size_t i = 0; i + 1 < trail.points.size(); ++i) {
            auto& p0 = trail.points[i];
            auto& p1 = trail.points[i + 1];

            glm::vec2 dir = p1.position - p0.position;
            float len = glm::length(dir);
            if (len < 0.001f) continue;

            glm::vec2 mid = (p0.position + p1.position) * 0.5f;
            float angle = std::atan2(dir.y, dir.x);
            float w = (p0.width + p1.width) * 0.5f;

            SpriteDrawItem item{};
            item.texture_handle = trail.texture_handle;
            item.color = glm::mix(p0.color, p1.color, 0.5f);
            item.sorting_layer = trail.sorting_layer;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(mid, 0.0f));
            model = glm::rotate(model, angle, glm::vec3(0, 0, 1));
            model = glm::scale(model, glm::vec3(len, w, 1.0f));
            item.model = model;

            float u0 = (float)i / (float)(trail.points.size() - 1);
            float u1 = (float)(i + 1) / (float)(trail.points.size() - 1);
            item.uv = glm::vec4(u0, 0.0f, u1 - u0, 1.0f);

            items.push_back(item);
        }
    }

    if (!items.empty()) {
        sprite_batch_.Draw(cmd_buffer, *rhi_device_, items, frame.view, frame.projection);
    }
}
