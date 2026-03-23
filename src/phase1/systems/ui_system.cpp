#include "ui_system.h"
#include "../ecs/components_2d.h"
#include "core/event_bus.h"
#include <glm/gtc/matrix_transform.hpp>

namespace dse {
namespace phase1 {

void UISystem::Update(entt::registry& registry, float dt, const glm::vec2& screen_size, const glm::vec2& mouse_pos, bool is_mouse_down) {
    (void)dt;
    SyncLabels(registry);
    UpdateLayout(registry, screen_size);
    HandleEvents(registry, mouse_pos, is_mouse_down);
}

void UISystem::SyncLabels(entt::registry& registry) {
    auto view = registry.view<UILabelComponent, UIRendererComponent>();
    for (auto entity : view) {
        auto& label = view.get<UILabelComponent>(entity);
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!label.dirty) {
            continue;
        }

        for (auto glyph_entity : label.runtime_glyph_entities) {
            if (registry.valid(glyph_entity)) {
                registry.destroy(glyph_entity);
            }
        }
        label.runtime_glyph_entities.clear();

        if (label.text.empty()) {
            label.dirty = false;
            continue;
        }

        const int atlas_cols = label.atlas_cols > 0 ? label.atlas_cols : 1;
        const int atlas_rows = label.atlas_rows > 0 ? label.atlas_rows : 1;
        const float inv_cols = 1.0f / static_cast<float>(atlas_cols);
        const float inv_rows = 1.0f / static_cast<float>(atlas_rows);
        const unsigned int font_texture = label.font_texture_handle != 0 ? label.font_texture_handle : ui.texture_handle;

        for (std::size_t i = 0; i < label.text.size(); ++i) {
            const int glyph_code = static_cast<unsigned char>(label.text[i]) - label.ascii_start;
            if (glyph_code < 0) {
                continue;
            }
            const int col = glyph_code % atlas_cols;
            const int row = glyph_code / atlas_cols;
            if (row >= atlas_rows) {
                continue;
            }

            const Entity glyph_entity = registry.create();
            auto& glyph_ui = registry.emplace<UIRendererComponent>(glyph_entity);
            glyph_ui.texture_handle = font_texture;
            glyph_ui.color = label.color;
            glyph_ui.visible = ui.visible;
            glyph_ui.interactable = false;
            glyph_ui.anchor_min = ui.anchor_min;
            glyph_ui.anchor_max = ui.anchor_max;
            glyph_ui.pivot = glm::vec2(0.0f, 0.5f);
            glyph_ui.size = label.glyph_size;
            glyph_ui.position = ui.position + label.offset + glm::vec2(static_cast<float>(i) * (label.glyph_size.x + label.spacing), 0.0f);
            glyph_ui.order = ui.order + static_cast<int>(i) + 1;
            const float u0 = static_cast<float>(col) * inv_cols;
            const float v0 = static_cast<float>(row) * inv_rows;
            glyph_ui.uv = glm::vec4(u0, v0, u0 + inv_cols, v0 + inv_rows);
            label.runtime_glyph_entities.push_back(glyph_entity);
        }

        label.dirty = false;
    }
}

void UISystem::UpdateLayout(entt::registry& registry, const glm::vec2& screen_size) {
    auto view = registry.view<UIRendererComponent>();
    for (auto entity : view) {
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible) continue;

        // Calculate absolute position based on anchors and screen size
        // Assuming orthographic projection where 0,0 is bottom-left or center
        // Let's assume 0,0 is center for this simple implementation
        glm::vec2 anchor_pos = screen_size * (ui.anchor_min - glm::vec2(0.5f));
        glm::vec2 final_pos = anchor_pos + ui.position;
        
        // Pivot offset
        glm::vec2 pivot_offset = ui.size * (ui.pivot - glm::vec2(0.5f));
        final_pos -= pivot_offset;

        // Create runtime model matrix
        ui.runtime_model = glm::translate(glm::mat4(1.0f), glm::vec3(final_pos, 0.0f));
        ui.runtime_model = glm::scale(ui.runtime_model, glm::vec3(ui.size, 1.0f));
    }
}

void UISystem::HandleEvents(entt::registry& registry, const glm::vec2& mouse_pos, bool is_mouse_down) {
    auto view = registry.view<UIRendererComponent>();
    for (auto entity : view) {
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible || !ui.interactable) continue;
        auto* button = registry.try_get<UIButtonComponent>(entity);

        // Extract translation from runtime model
        glm::vec3 pos = glm::vec3(ui.runtime_model[3]);
        // Extract scale from runtime model
        glm::vec3 scale = glm::vec3(glm::length(glm::vec3(ui.runtime_model[0])), 
                                    glm::length(glm::vec3(ui.runtime_model[1])), 
                                    glm::length(glm::vec3(ui.runtime_model[2])));

        float half_w = scale.x / 2.0f;
        float half_h = scale.y / 2.0f;

        bool is_hovering = (mouse_pos.x >= pos.x - half_w && mouse_pos.x <= pos.x + half_w &&
                            mouse_pos.y >= pos.y - half_h && mouse_pos.y <= pos.y + half_h);

        if (is_hovering && !ui.is_hovered) {
            ui.is_hovered = true;
            if (ui.on_pointer_enter) ui.on_pointer_enter(entity);
            if (button && button->on_pointer_enter) button->on_pointer_enter(entity);
        } else if (!is_hovering && ui.is_hovered) {
            ui.is_hovered = false;
            ui.is_pressed = false;
            if (ui.on_pointer_exit) ui.on_pointer_exit(entity);
            if (button && button->on_pointer_exit) button->on_pointer_exit(entity);
        }

        if (ui.is_hovered) {
            if (is_mouse_down && !ui.is_pressed) {
                ui.is_pressed = true;
            } else if (!is_mouse_down && ui.is_pressed) {
                ui.is_pressed = false;
                if (ui.on_click) ui.on_click(entity);
                if (button && button->on_click) button->on_click(entity);
                core::EventBus::Instance().Publish<core::UiClickEvent>(static_cast<std::uint32_t>(entity));
            }
        }

        if (button) {
            if (ui.is_pressed) {
                ui.color = button->pressed_color;
            } else if (ui.is_hovered) {
                ui.color = button->hover_color;
            } else {
                ui.color = button->normal_color;
            }
        }
    }
}

} // namespace phase1
} // namespace dse
