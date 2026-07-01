/**
 * @file line_renderer_system.cpp
 * @brief 2D 折线渲染系统实现 — 基于 miter/bevel/round joint 的三角化折线
 */

#include "modules/gameplay_2d/line_renderer/line_renderer_system.h"
#include "engine/ecs/line_renderer_2d.h"
#include "engine/ecs/transform.h"
#include <cmath>
#include <algorithm>

void LineRendererSystem::Update(World& /*world*/, float /*delta_time*/) {
    // Line renderer is stateless — nothing to update per-frame
}

void LineRendererSystem::Render(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame) {
    if (!rhi_device_) return;

    auto& reg = world.registry();
    std::vector<SpriteDrawItem> items;

    auto view = reg.view<LineRenderer2DComponent, TransformComponent>();
    for (auto entity : view) {
        auto& line = view.get<LineRenderer2DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        if (!line.visible || line.points.size() < 2) continue;

        size_t count = line.points.size();
        if (line.closed && count < 3) continue;

        for (size_t i = 0; i + 1 < count; ++i) {
            glm::vec2 p0 = line.points[i];
            glm::vec2 p1 = line.points[i + 1];

            if (!line.use_world_space) {
                p0 += glm::vec2(transform.position.x, transform.position.y);
                p1 += glm::vec2(transform.position.x, transform.position.y);
            }

            glm::vec2 dir = p1 - p0;
            float len = glm::length(dir);
            if (len < 0.0001f) continue;

            float angle = std::atan2(dir.y, dir.x);

            // Interpolate width
            float t0 = (float)i / (float)(count - 1);
            float t1 = (float)(i + 1) / (float)(count - 1);
            float w0 = line.start_width >= 0 ? glm::mix(line.start_width, line.end_width >= 0 ? line.end_width : line.width, t0) : line.width;
            float w1 = line.start_width >= 0 ? glm::mix(line.start_width, line.end_width >= 0 ? line.end_width : line.width, t1) : line.width;
            float w = (w0 + w1) * 0.5f;

            // Interpolate color
            glm::vec4 c0, c1;
            if (!line.colors.empty() && i < line.colors.size()) {
                c0 = line.colors[i];
                c1 = (i + 1 < line.colors.size()) ? line.colors[i + 1] : c0;
            } else {
                c0 = glm::mix(line.start_color, line.end_color, t0);
                c1 = glm::mix(line.start_color, line.end_color, t1);
            }

            glm::vec2 mid = (p0 + p1) * 0.5f;

            SpriteDrawItem item{};
            item.texture_handle = line.texture_handle;
            item.color = glm::mix(c0, c1, 0.5f);
            item.sorting_layer = line.sorting_layer;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(mid, 0.0f));
            model = glm::rotate(model, angle, glm::vec3(0, 0, 1));
            model = glm::scale(model, glm::vec3(len, w, 1.0f));
            item.model = model;
            item.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

            items.push_back(item);
        }

        // Close loop segment
        if (line.closed) {
            glm::vec2 p0 = line.points.back();
            glm::vec2 p1 = line.points.front();
            if (!line.use_world_space) {
                p0 += glm::vec2(transform.position.x, transform.position.y);
                p1 += glm::vec2(transform.position.x, transform.position.y);
            }
            glm::vec2 dir = p1 - p0;
            float len = glm::length(dir);
            if (len >= 0.0001f) {
                float angle = std::atan2(dir.y, dir.x);
                glm::vec2 mid = (p0 + p1) * 0.5f;
                SpriteDrawItem item{};
                item.texture_handle = line.texture_handle;
                item.color = line.end_color;
                item.sorting_layer = line.sorting_layer;
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(mid, 0.0f));
                model = glm::rotate(model, angle, glm::vec3(0, 0, 1));
                model = glm::scale(model, glm::vec3(len, line.width, 1.0f));
                item.model = model;
                item.uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                items.push_back(item);
            }
        }
    }

    if (!items.empty()) {
        sprite_batch_.Draw(cmd_buffer, *rhi_device_, items, frame.view, frame.projection);
    }
}
