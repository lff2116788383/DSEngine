#include "editor_vegetation_panel.h"
#include "editor_vegetation_brush_core.h"
#include "editor_terrain_panel_core.h"  // WorldToScreen / ScreenToWorldOnTerrain
#include "editor_context.h"

#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/vegetation_mask.h"
#include "editor_undo.h"
#include "editor_shortcuts.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace dse::editor {

VegetationEditorState& GetVegetationEditorState() {
    static VegetationEditorState state;
    return state;
}

namespace {

// 返回激活实体上目标组件的密度遮罩指针；不存在返回 nullptr。
VegetationDensityMask* GetActiveMask(entt::registry& registry,
                                     const VegetationEditorState& state) {
    if (state.active_entity == entt::null || !registry.valid(state.active_entity))
        return nullptr;
    if (state.target == VegetationTarget::Grass) {
        if (registry.all_of<GrassComponent>(state.active_entity))
            return &registry.get<GrassComponent>(state.active_entity).density_mask;
    } else {
        if (registry.all_of<TreeComponent>(state.active_entity))
            return &registry.get<TreeComponent>(state.active_entity).density_mask;
    }
    return nullptr;
}

bool ActiveEntityHasTarget(entt::registry& registry,
                           const VegetationEditorState& state) {
    if (state.active_entity == entt::null || !registry.valid(state.active_entity))
        return false;
    return state.target == VegetationTarget::Grass
               ? registry.all_of<GrassComponent>(state.active_entity)
               : registry.all_of<TreeComponent>(state.active_entity);
}

// 以激活实体为中心计算遮罩世界范围。
void ComputeMaskExtents(entt::registry& registry,
                        const VegetationEditorState& state,
                        glm::vec2& out_world_min,
                        glm::vec2& out_world_size) {
    glm::vec3 center(0.0f);
    if (state.active_entity != entt::null && registry.valid(state.active_entity) &&
        registry.all_of<TransformComponent>(state.active_entity)) {
        center = registry.get<TransformComponent>(state.active_entity).position;
    }
    float half = state.mask_world_size * 0.5f;
    out_world_min = glm::vec2(center.x - half, center.z - half);
    out_world_size = glm::vec2(state.mask_world_size, state.mask_world_size);
}

}  // namespace

// ---------------------------------------------------------------------------
// Panel drawing
// ---------------------------------------------------------------------------

void DrawVegetationEditorPanel(EditorContext& ctx) {
    auto& registry = ctx.registry;
    auto selected_entity = ctx.selected_entity;
    ImGui::Begin("Vegetation Brush");

    auto& state = GetVegetationEditorState();

    // Target selection
    {
        bool grass = (state.target == VegetationTarget::Grass);
        if (grass) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
        if (ImGui::Button("Grass", ImVec2(80, 24))) state.target = VegetationTarget::Grass;
        if (grass) ImGui::PopStyleColor();
        ImGui::SameLine();
        bool tree = (state.target == VegetationTarget::Tree);
        if (tree) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.55f, 0.25f, 1.0f));
        if (ImGui::Button("Tree", ImVec2(80, 24))) state.target = VegetationTarget::Tree;
        if (tree) ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // Auto-select entity with the target component
    if (selected_entity != entt::null && registry.valid(selected_entity)) {
        bool has = state.target == VegetationTarget::Grass
                       ? registry.all_of<GrassComponent>(selected_entity)
                       : registry.all_of<TreeComponent>(selected_entity);
        if (has) {
            state.active_entity = selected_entity;
            state.editing_active = true;
        }
    }

    if (ActiveEntityHasTarget(registry, state)) {
        ImGui::Text("Painting %s on Entity %u",
                    state.target == VegetationTarget::Grass ? "Grass" : "Tree",
                    static_cast<unsigned>(state.active_entity));
        ImGui::SameLine();
        if (ImGui::SmallButton("Deselect##veg")) {
            state.active_entity = entt::null;
            state.editing_active = false;
        }

        ImGui::Separator();
        ImGui::Text("Brush Mode");
        {
            bool plant = (state.brush_mode == VegetationBrushMode::Plant);
            if (plant) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            if (ImGui::Button("Plant", ImVec2(80, 24))) state.brush_mode = VegetationBrushMode::Plant;
            if (plant) ImGui::PopStyleColor();
            ImGui::SameLine();
            bool clear = (state.brush_mode == VegetationBrushMode::Clear);
            if (clear) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.3f, 1.0f));
            if (ImGui::Button("Clear", ImVec2(80, 24))) state.brush_mode = VegetationBrushMode::Clear;
            if (clear) ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Text("Brush Settings");
        ImGui::SliderFloat("Radius", &state.brush_radius, 0.5f, 50.0f, "%.1f");
        ImGui::SliderFloat("Strength", &state.brush_strength, 0.01f, 2.0f, "%.2f");
        ImGui::SliderFloat("Falloff", &state.brush_falloff, 0.0f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::Text("Mask");
        ImGui::SliderInt("Resolution", &state.mask_resolution, 16, 512);
        ImGui::SliderFloat("World Size", &state.mask_world_size, 16.0f, 1000.0f, "%.0f");

        auto* mask = GetActiveMask(registry, state);
        if (mask && mask->active()) {
            ImGui::Text("Mask: %dx%d  covering %.0fx%.0f",
                        mask->resolution_x, mask->resolution_z,
                        mask->world_size.x, mask->world_size.y);
        } else {
            ImGui::TextDisabled("Mask not initialized (paint to create).");
        }

        ImGui::Separator();
        if (ImGui::Button("Fill Mask (All Vegetation)") && mask) {
            glm::vec2 wmin, wsize;
            ComputeMaskExtents(registry, state, wmin, wsize);
            EnsureVegetationMask(*mask, wmin, wsize, state.mask_resolution, state.mask_resolution, 1.0f);
            std::fill(mask->weights.begin(), mask->weights.end(), 1.0f);
        }
        if (ImGui::Button("Clear Mask (No Vegetation)") && mask) {
            glm::vec2 wmin, wsize;
            ComputeMaskExtents(registry, state, wmin, wsize);
            EnsureVegetationMask(*mask, wmin, wsize, state.mask_resolution, state.mask_resolution, 0.0f);
            std::fill(mask->weights.begin(), mask->weights.end(), 0.0f);
        }
        if (ImGui::Button("Remove Mask (Uniform Density)") && mask) {
            *mask = VegetationDensityMask{};
        }

    } else {
        ImGui::TextDisabled("Select a %s entity to start painting.",
                            state.target == VegetationTarget::Grass ? "Grass" : "Tree");
        state.editing_active = false;

        ImGui::Separator();
        ImGui::Text("Available:");
        if (state.target == VegetationTarget::Grass) {
            auto v = registry.view<GrassComponent>();
            for (auto e : v) {
                char buf[64]; snprintf(buf, sizeof(buf), "Grass Entity %u", static_cast<unsigned>(e));
                if (ImGui::Selectable(buf)) { state.active_entity = e; state.editing_active = true; }
            }
        } else {
            auto v = registry.view<TreeComponent>();
            for (auto e : v) {
                char buf[64]; snprintf(buf, sizeof(buf), "Tree Entity %u", static_cast<unsigned>(e));
                if (ImGui::Selectable(buf)) { state.active_entity = e; state.editing_active = true; }
            }
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Viewport brush overlay
// ---------------------------------------------------------------------------

void DrawVegetationBrushOverlay(entt::registry& registry,
                                const glm::vec2& window_pos,
                                const glm::vec2& panel_size,
                                const glm::mat4& view,
                                const glm::mat4& proj) {
    auto& state = GetVegetationEditorState();
    if (!state.editing_active) return;
    if (!ActiveEntityHasTarget(registry, state)) return;
    if (!registry.all_of<TransformComponent>(state.active_entity)) return;

    auto& tf = registry.get<TransformComponent>(state.active_entity);

    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec3 hit = ScreenToWorldOnTerrain(glm::vec2(mouse.x, mouse.y), view, proj,
                                           window_pos, panel_size, tf.position.y);
    state.last_brush_hit = hit;
    state.last_brush_hit_valid = true;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int segments = 48;
    ImU32 brush_color = (state.brush_mode == VegetationBrushMode::Plant)
                            ? IM_COL32(80, 220, 80, 150)
                            : IM_COL32(220, 90, 60, 150);

    ImVec2 pts[48];
    for (int i = 0; i < segments; i++) {
        float angle = static_cast<float>(i) / static_cast<float>(segments) * 6.28318530f;
        glm::vec3 wp = hit + glm::vec3(std::cos(angle) * state.brush_radius, 0.0f,
                                       std::sin(angle) * state.brush_radius);
        glm::vec2 sp = WorldToScreen(wp, view, proj, window_pos, panel_size);
        pts[i] = ImVec2(sp.x, sp.y);
    }
    dl->AddPolyline(pts, segments, brush_color, ImDrawFlags_Closed, 2.0f);

    glm::vec2 center = WorldToScreen(hit, view, proj, window_pos, panel_size);
    dl->AddLine(ImVec2(center.x - 6, center.y), ImVec2(center.x + 6, center.y), brush_color, 1.0f);
    dl->AddLine(ImVec2(center.x, center.y - 6), ImVec2(center.x, center.y + 6), brush_color, 1.0f);
}

// ---------------------------------------------------------------------------
// Viewport paint handling
// ---------------------------------------------------------------------------

bool HandleVegetationViewportPaint(entt::registry& registry,
                                   const glm::vec2& window_pos,
                                   const glm::vec2& panel_size,
                                   const glm::mat4& view,
                                   const glm::mat4& proj,
                                   float delta_time) {
    auto& state = GetVegetationEditorState();
    if (!state.editing_active) return false;
    if (!ActiveEntityHasTarget(registry, state)) return false;
    if (!registry.all_of<TransformComponent>(state.active_entity)) return false;

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyAlt) return false;  // Alt = orbit

    bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);

    // Stroke end -> push undo
    if (!mouse_down && state.painting) {
        state.painting = false;
        auto* mask = GetActiveMask(registry, state);
        if (mask && state.mask_snapshot != mask->weights) {
            entt::entity ent = state.active_entity;
            VegetationTarget target = state.target;
            std::vector<float> old_w = state.mask_snapshot;
            std::vector<float> new_w = mask->weights;
            std::string merge_id = "veg_mask_" + std::to_string(static_cast<uint32_t>(ent));

            auto apply = [&reg = registry, ent, target](const std::vector<float>& w) {
                if (!reg.valid(ent)) return;
                VegetationDensityMask* m = nullptr;
                if (target == VegetationTarget::Grass && reg.all_of<GrassComponent>(ent))
                    m = &reg.get<GrassComponent>(ent).density_mask;
                else if (target == VegetationTarget::Tree && reg.all_of<TreeComponent>(ent))
                    m = &reg.get<TreeComponent>(ent).density_mask;
                if (m) m->weights = w;
            };
            auto cmd = std::make_unique<LambdaCommand>(
                "Vegetation Paint",
                [apply, new_w]() { apply(new_w); },
                [apply, old_w]() { apply(old_w); },
                merge_id);
            GetUndoRedoManager().Execute(std::move(cmd), true);
        }
        return false;
    }

    if (!mouse_down) return false;

    auto& tf = registry.get<TransformComponent>(state.active_entity);
    auto* mask = GetActiveMask(registry, state);
    if (!mask) return false;

    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec3 hit = ScreenToWorldOnTerrain(glm::vec2(mouse.x, mouse.y), view, proj,
                                           window_pos, panel_size, tf.position.y);

    // Initialize / re-extent mask centered on the active entity on stroke start
    if (!state.painting) {
        state.painting = true;
        glm::vec2 wmin, wsize;
        ComputeMaskExtents(registry, state, wmin, wsize);
        // 首次创建：Plant 模式从空白(0)起步以便"刷哪长哪"；Clear 模式从满(1)起步。
        float init_value = (state.brush_mode == VegetationBrushMode::Plant) ? 0.0f : 1.0f;
        EnsureVegetationMask(*mask, wmin, wsize, state.mask_resolution, state.mask_resolution, init_value);
        state.mask_snapshot = mask->weights;
    }

    ApplyVegetationBrush(*mask, hit, state.brush_radius, state.brush_strength,
                         state.brush_falloff,
                         state.brush_mode == VegetationBrushMode::Plant,
                         delta_time);
    return true;
}

}  // namespace dse::editor
