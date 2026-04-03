/**
 * @file ui_system.cpp
 * @brief 用户界面(UI)系统，管理 UI 元素的布局、事件响应和渲染
 */

#include "ui_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/core/event_bus.h"
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace dse {
namespace gameplay2d {
namespace {
/**
 * @brief 解析十六进制颜色字符串（如 "ff0000" 或 "ff0000ff"）
 * @param hex 十六进制颜色字符串（不含 #）
 * @param fallback 解析失败时的回退颜色
 * @return 解析得到的 RGBA 颜色向量
 */
glm::vec4 ParseHexColor(const std::string& hex, const glm::vec4& fallback) {
    if (hex.size() != 6 && hex.size() != 8) {
        return fallback;
    }
    for (char ch : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return fallback;
        }
    }
    const auto to_channel = [&](std::size_t offset) {
        return static_cast<float>(std::strtoul(hex.substr(offset, 2).c_str(), nullptr, 16)) / 255.0f;
    };
    glm::vec4 color(to_channel(0), to_channel(2), to_channel(4), 1.0f);
    if (hex.size() == 8) {
        color.a = to_channel(6);
    }
    return color;
}
}

void UISystem::Update(entt::registry& registry, float dt, const glm::vec2& screen_size, const glm::vec2& mouse_pos, bool is_mouse_down) {
    SyncLabels(registry);
    UpdateLayout(registry, screen_size);
    HandleEvents(registry, dt, mouse_pos, is_mouse_down);
}

void UISystem::SyncLabels(entt::registry& registry) {
    auto view = registry.view<UILabelComponent, UIRendererComponent>();
    for (auto entity : view) {
        auto& label = view.get<UILabelComponent>(entity);
        auto& ui = view.get<UIRendererComponent>(entity);
        auto* rich = registry.try_get<UIRichTextComponent>(entity);
        if (rich && rich->dirty) {
            label.dirty = true;
        }
        if (!label.dirty) {
            continue;
        }
        if (label.numeric_mode && !rich) {
            label.text = std::to_string(label.number_value);
        }

        for (auto glyph_entity : label.runtime_glyph_entities) {
            if (registry.valid(glyph_entity)) {
                registry.destroy(glyph_entity);
            }
        }
        label.runtime_glyph_entities.clear();

        std::vector<RichGlyph> glyphs;
        if (rich) {
            glyphs = BuildRichGlyphs(rich->text, rich->default_color);
        } else {
            glyphs.reserve(label.text.size());
            for (char ch : label.text) {
                glyphs.push_back({ch, label.color});
            }
        }

        if (glyphs.empty()) {
            label.dirty = false;
            if (rich) {
                rich->dirty = false;
            }
            continue;
        }

        const int atlas_cols = label.atlas_cols > 0 ? label.atlas_cols : 1;
        const int atlas_rows = label.atlas_rows > 0 ? label.atlas_rows : 1;
        const float inv_cols = 1.0f / static_cast<float>(atlas_cols);
        const float inv_rows = 1.0f / static_cast<float>(atlas_rows);
        const unsigned int font_texture = label.font_texture_handle != 0 ? label.font_texture_handle : ui.texture_handle;

        auto spawn_glyph = [&](const glm::vec2& local_position, const glm::vec4& color, const glm::vec4& uv, int order) {
            const Entity glyph_entity = registry.create();
            auto& glyph_ui = registry.emplace<UIRendererComponent>(glyph_entity);
            glyph_ui.texture_handle = font_texture;
            glyph_ui.color = color;
            glyph_ui.visible = ui.visible;
            glyph_ui.interactable = false;
            glyph_ui.anchor_min = ui.anchor_min;
            glyph_ui.anchor_max = ui.anchor_max;
            glyph_ui.pivot = glm::vec2(0.0f, 0.5f);
            glyph_ui.size = label.glyph_size;
            glyph_ui.position = local_position;
            glyph_ui.order = order;
            glyph_ui.uv = uv;
            label.runtime_glyph_entities.push_back(glyph_entity);
        };

        for (std::size_t i = 0; i < glyphs.size(); ++i) {
            const int glyph_code = static_cast<unsigned char>(glyphs[i].ch) - label.ascii_start;
            if (glyph_code < 0) {
                continue;
            }
            const int col = glyph_code % atlas_cols;
            const int row = glyph_code / atlas_cols;
            if (row >= atlas_rows) {
                continue;
            }

            const glm::vec2 base_position = ui.position + label.offset + glm::vec2(static_cast<float>(i) * (label.glyph_size.x + label.spacing), 0.0f);
            const int base_order = ui.order + static_cast<int>(i) * 10 + 1;
            const float u0 = static_cast<float>(col) * inv_cols;
            const float v0 = static_cast<float>(row) * inv_rows;
            const glm::vec4 uv = glm::vec4(u0, v0, u0 + inv_cols, v0 + inv_rows);
            if (rich && rich->enable_shadow) {
                spawn_glyph(base_position + rich->shadow_offset, rich->shadow_color, uv, base_order);
            }
            if (rich && rich->enable_outline) {
                const float outline = std::max(0.0f, rich->outline_width);
                if (outline > 0.0f) {
                    spawn_glyph(base_position + glm::vec2(outline, 0.0f), rich->outline_color, uv, base_order + 1);
                    spawn_glyph(base_position + glm::vec2(-outline, 0.0f), rich->outline_color, uv, base_order + 2);
                    spawn_glyph(base_position + glm::vec2(0.0f, outline), rich->outline_color, uv, base_order + 3);
                    spawn_glyph(base_position + glm::vec2(0.0f, -outline), rich->outline_color, uv, base_order + 4);
                }
            }
            spawn_glyph(base_position, glyphs[i].color, uv, base_order + 5);
        }

        label.dirty = false;
        if (rich) {
            rich->dirty = false;
        }
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
        ui.runtime_model = glm::scale(ui.runtime_model, glm::vec3(ui.size * ui.scale, 1.0f));
    }
}

void UISystem::HandleEvents(entt::registry& registry, float dt, const glm::vec2& mouse_pos, bool is_mouse_down) {
    auto view = registry.view<UIRendererComponent>();
    for (auto entity : view) {
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible || !ui.interactable) continue;
        auto* button = registry.try_get<UIButtonComponent>(entity);
        auto* joystick = registry.try_get<UIJoystickComponent>(entity);
        if (IsBlockedByMask(registry, entity, mouse_pos)) {
            ui.is_hovered = false;
            ui.is_pressed = false;
            if (joystick && joystick->reset_on_release) {
                joystick->is_dragging = false;
                joystick->direction = glm::vec2(0.0f);
            }
            continue;
        }

        glm::vec3 pos = glm::vec3(ui.runtime_model[3]);
        glm::vec3 scale = glm::vec3(glm::length(glm::vec3(ui.runtime_model[0])), 
                                    glm::length(glm::vec3(ui.runtime_model[1])), 
                                    glm::length(glm::vec3(ui.runtime_model[2])));

        float half_w = scale.x / 2.0f;
        float half_h = scale.y / 2.0f;

        bool is_hovering = (mouse_pos.x >= pos.x - half_w && mouse_pos.x <= pos.x + half_w &&
                            mouse_pos.y >= pos.y - half_h && mouse_pos.y <= pos.y + half_h);

        if (joystick) {
            if (is_mouse_down && is_hovering && !joystick->is_dragging) {
                joystick->is_dragging = true;
                joystick->drag_anchor = joystick->follow_pointer ? mouse_pos : glm::vec2(pos.x, pos.y);
            }
            if (joystick->is_dragging) {
                if (is_mouse_down) {
                    const glm::vec2 delta = mouse_pos - joystick->drag_anchor;
                    const float radius = std::max(1.0f, joystick->max_radius);
                    const float length = glm::length(delta);
                    if (length > 0.0f) {
                        const glm::vec2 clamped = length > radius ? (delta / length) * radius : delta;
                        joystick->direction = clamped / radius;
                    } else {
                        joystick->direction = glm::vec2(0.0f);
                    }
                } else {
                    joystick->is_dragging = false;
                    if (joystick->reset_on_release) {
                        joystick->direction = glm::vec2(0.0f);
                    }
                }
            }
        }

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
                dse::core::EventBus::Instance().Publish<dse::core::UiClickEvent>(static_cast<std::uint32_t>(entity));
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
        float target_scale = 1.0f;
        if (ui.is_pressed) {
            target_scale = ui.pressed_scale;
        } else if (ui.is_hovered) {
            target_scale = ui.hover_scale;
        }
        float speed = ui.scale_lerp_speed * dt;
        if (speed < 0.0f) {
            speed = 0.0f;
        }
        if (speed > 1.0f) {
            speed = 1.0f;
        }
        ui.scale += (target_scale - ui.scale) * speed;
    }
}

std::vector<UISystem::RichGlyph> UISystem::BuildRichGlyphs(const std::string& text, const glm::vec4& default_color) const {
    std::vector<RichGlyph> glyphs;
    glyphs.reserve(text.size());
    glm::vec4 current_color = default_color;
    std::size_t i = 0;
    while (i < text.size()) {
        if (i + 8 < text.size() && text.compare(i, 8, "<color=#") == 0) {
            const std::size_t end = text.find('>', i + 8);
            if (end != std::string::npos) {
                current_color = ParseHexColor(text.substr(i + 8, end - (i + 8)), current_color);
                i = end + 1;
                continue;
            }
        }
        if (i + 8 <= text.size() && text.compare(i, 8, "</color>") == 0) {
            current_color = default_color;
            i += 8;
            continue;
        }
        glyphs.push_back({text[i], current_color});
        ++i;
    }
    return glyphs;
}

bool UISystem::IsPointInsideUIRect(entt::registry& registry, entt::entity entity, const glm::vec2& point) const {
    if (!registry.valid(entity) || !registry.all_of<UIRendererComponent>(entity)) {
        return false;
    }
    const auto& ui = registry.get<UIRendererComponent>(entity);
    const glm::vec3 pos = glm::vec3(ui.runtime_model[3]);
    const glm::vec3 scale = glm::vec3(glm::length(glm::vec3(ui.runtime_model[0])),
                                      glm::length(glm::vec3(ui.runtime_model[1])),
                                      glm::length(glm::vec3(ui.runtime_model[2])));
    const float half_w = scale.x * 0.5f;
    const float half_h = scale.y * 0.5f;
    return point.x >= pos.x - half_w && point.x <= pos.x + half_w &&
           point.y >= pos.y - half_h && point.y <= pos.y + half_h;
}

bool UISystem::IsBlockedByMask(entt::registry& registry, entt::entity entity, const glm::vec2& point) const {
    entt::entity cursor = entity;
    while (registry.valid(cursor)) {
        auto* parent = registry.try_get<ParentComponent>(cursor);
        if (registry.all_of<UIMaskComponent, UIRendererComponent>(cursor)) {
            const auto& mask = registry.get<UIMaskComponent>(cursor);
            if (mask.enabled && mask.block_outside_input) {
                auto& ui = registry.get<UIRendererComponent>(cursor);
                const glm::vec3 pos = glm::vec3(ui.runtime_model[3]) + glm::vec3(mask.offset, 0.0f);
                glm::vec2 effective_size = mask.size;
                if (effective_size.x <= 0.0f || effective_size.y <= 0.0f) {
                    const glm::vec3 scale = glm::vec3(glm::length(glm::vec3(ui.runtime_model[0])),
                                                      glm::length(glm::vec3(ui.runtime_model[1])),
                                                      glm::length(glm::vec3(ui.runtime_model[2])));
                    effective_size = glm::vec2(scale.x, scale.y);
                }
                const float half_w = effective_size.x * 0.5f;
                const float half_h = effective_size.y * 0.5f;
                const bool inside = point.x >= pos.x - half_w && point.x <= pos.x + half_w &&
                                    point.y >= pos.y - half_h && point.y <= pos.y + half_h;
                if (!inside) {
                    return true;
                }
            }
        }
        if (!parent || parent->parent == entt::null) {
            break;
        }
        cursor = parent->parent;
    }
    return false;
}

} // namespace gameplay2d
} // namespace dse
