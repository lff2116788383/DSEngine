#include "editor_hierarchy_panel.h"
#include "editor_entity_snapshot.h"

#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/script.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include "editor_shortcuts.h"
#include "editor_console_panel.h"
#include "editor_selection.h"
#include "editor_prefab.h"
#include "editor_project.h"
#include "apps/editor_cpp/core/command_bus.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>
#include <optional>
#include <memory>
#include <unordered_set>
#include <filesystem>

namespace dse::editor {

namespace {

// Hierarchy panel persistent state
static char s_search_filter[128] = "";
static entt::entity s_renaming_entity = entt::null;
static char s_rename_buf[64] = "";
static entt::entity s_last_clicked_entity = entt::null;

const char* GetEntityTypeIcon(entt::registry& registry, entt::entity entity) {
    if (registry.all_of<dse::Camera3DComponent>(entity) || registry.all_of<CameraComponent>(entity)) {
        return MDI_ICON_VIDEO;
    } else if (registry.all_of<dse::DirectionalLight3DComponent>(entity) ||
               registry.all_of<dse::PointLightComponent>(entity) ||
               registry.all_of<dse::SpotLightComponent>(entity)) {
        return MDI_ICON_LIGHTBULB;
    } else if (registry.all_of<dse::MeshRendererComponent>(entity)) {
        return MDI_ICON_SPHERE;
    } else if (registry.all_of<UIRendererComponent>(entity)) {
        return MDI_ICON_IMAGE;
    } else if (registry.all_of<dse::ParticleSystem3DComponent>(entity) ||
               registry.all_of<ParticleEmitterComponent>(entity)) {
        return MDI_ICON_CREATION;
    } else if (registry.all_of<dse::TerrainComponent>(entity)) {
        return MDI_ICON_TERRAIN;
    } else if (registry.all_of<dse::SubSceneComponent>(entity)) {
        return MDI_ICON_IMAGE_MULTIPLE;
    }
    return MDI_ICON_CUBE_OUTLINE;
}

bool MatchesSearchFilter(const std::string& name) {
    if (s_search_filter[0] == '\0') return true;
    // Case-insensitive substring match
    std::string lower_name = name;
    std::string lower_filter = s_search_filter;
    for (auto& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (auto& c : lower_filter) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower_name.find(lower_filter) != std::string::npos;
}

std::vector<entt::entity> GetChildren(entt::registry& registry, entt::entity parent) {
    std::vector<entt::entity> children;
    for (auto entity : registry.storage<entt::entity>()) {
        if (!registry.valid(entity)) continue;
        if (registry.all_of<ParentComponent>(entity)) {
            if (registry.get<ParentComponent>(entity).parent == parent) {
                children.push_back(entity);
            }
        }
    }
    // Sort by SiblingIndex
    std::sort(children.begin(), children.end(), [&registry](entt::entity a, entt::entity b) {
        int ia = registry.all_of<SiblingIndexComponent>(a) ? registry.get<SiblingIndexComponent>(a).index : 0;
        int ib = registry.all_of<SiblingIndexComponent>(b) ? registry.get<SiblingIndexComponent>(b).index : 0;
        return ia < ib;
    });
    return children;
}

// 把层级面板的 reparent/reorder 写操作收敛到 CommandBus（复用
// dsengine_entity_reparent 工具与统一撤销栈），消除"面板直接改 registry"双写。
//   parent == std::nullopt → 挂到根（detach）；否则挂到给定父实体。
//   sibling == std::nullopt → 不显式设置兄弟序；否则写入 sibling_index。
// command_bus 缺失（理论上仅非 GUI 装配）时退回与工具等价的直写 + LambdaCommand 兜底。
void ReparentViaBus(EditorContext& context, entt::entity dragged,
                    std::optional<entt::entity> parent,
                    std::optional<int> sibling) {
    if (context.command_bus) {
        core::ReparentEntityCmd cmd;
        cmd.entity_id = static_cast<uint32_t>(dragged);
        if (parent.has_value())
            cmd.parent = static_cast<uint32_t>(*parent);
        if (sibling.has_value())
            cmd.sibling_index = *sibling;
        context.command_bus->dispatch(cmd, context.engine);
        return;
    }

    // ── Fallback：command_bus 不可用时，等价复刻工具行为（应用 + 完整 undo/redo）──
    auto& reg = context.registry;
    if (!reg.valid(dragged)) return;
    entt::entity new_parent = parent.value_or(entt::null);
    bool has_sibling = sibling.has_value();
    int new_sibling = sibling.value_or(0);

    entt::entity old_parent = entt::null;
    int old_sibling = 0;
    if (reg.all_of<ParentComponent>(dragged)) old_parent = reg.get<ParentComponent>(dragged).parent;
    if (reg.all_of<SiblingIndexComponent>(dragged)) old_sibling = reg.get<SiblingIndexComponent>(dragged).index;

    auto apply = [&reg, dragged, new_parent, has_sibling, new_sibling]() {
        if (!reg.valid(dragged)) return;
        if (new_parent == entt::null) {
            if (reg.all_of<ParentComponent>(dragged)) reg.remove<ParentComponent>(dragged);
        } else if (reg.all_of<ParentComponent>(dragged)) {
            reg.get<ParentComponent>(dragged).parent = new_parent;
        } else {
            reg.emplace<ParentComponent>(dragged, new_parent);
        }
        if (has_sibling) {
            if (reg.all_of<SiblingIndexComponent>(dragged)) reg.get<SiblingIndexComponent>(dragged).index = new_sibling;
            else reg.emplace<SiblingIndexComponent>(dragged, new_sibling);
        }
        if (reg.all_of<TransformComponent>(dragged)) reg.get<TransformComponent>(dragged).dirty = true;
    };
    apply();
    GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
        "Reparent Entity",
        apply,
        [&reg, dragged, old_parent, old_sibling]() {
            if (!reg.valid(dragged)) return;
            if (old_parent == entt::null) {
                if (reg.all_of<ParentComponent>(dragged)) reg.remove<ParentComponent>(dragged);
            } else if (reg.all_of<ParentComponent>(dragged)) {
                reg.get<ParentComponent>(dragged).parent = old_parent;
            } else {
                reg.emplace<ParentComponent>(dragged, old_parent);
            }
            if (reg.all_of<SiblingIndexComponent>(dragged)) reg.get<SiblingIndexComponent>(dragged).index = old_sibling;
            if (reg.all_of<TransformComponent>(dragged)) reg.get<TransformComponent>(dragged).dirty = true;
        }), false);
}

// 把 create / delete / duplicate 写操作收敛到 CommandBus（复用 dsengine_entity_*
// 工具与统一撤销栈），消除"面板直接改 registry + 自挂 LambdaCommand"的双写。
// command_bus 缺失（理论上仅非 GUI 装配）时退回与工具等价的全组件快照直写兜底。

// 创建实体：name + 额外组件类型（如 "UIRenderer"）。返回新实体（失败为 entt::null）。
entt::entity CreateEntityViaBus(EditorContext& context, const std::string& name,
                                const std::vector<std::string>& components) {
    if (context.command_bus) {
        core::CreateEntityCmd cmd;
        cmd.name = name;
        cmd.components = components;
        auto res = context.command_bus->dispatch(cmd, context.engine);
        if (res.ok && res.data.IsObject() && res.data.HasMember("entity_id") &&
            res.data["entity_id"].IsUint()) {
            return static_cast<entt::entity>(res.data["entity_id"].GetUint());
        }
        return entt::null;
    }

    // ── Fallback：等价复刻工具行为（创建 + 完整 undo/redo）──
    auto& world_ref = context.world;
    auto& reg_ref = context.registry;
    auto new_ent = world_ref.CreateEntity();
    reg_ref.emplace<EditorNameComponent>(new_ent, name);
    reg_ref.emplace<TransformComponent>(new_ent);
    for (const auto& t : components) {
        if ((t == "UIRenderer" || t == "UIRendererComponent") &&
            !reg_ref.all_of<UIRendererComponent>(new_ent)) {
            reg_ref.emplace<UIRendererComponent>(new_ent);
        }
    }
    auto snapshot = std::make_shared<EntitySnapshot>(EntitySnapshot::Capture(reg_ref, new_ent));
    auto tracked = std::make_shared<entt::entity>(new_ent);
    GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
        "Create Entity",
        [&world_ref, &reg_ref, snapshot, tracked]() {
            if (*tracked != entt::null && reg_ref.valid(*tracked)) return;
            *tracked = snapshot->Restore(world_ref, reg_ref);
        },
        [&world_ref, &reg_ref, tracked]() {
            if (*tracked != entt::null && reg_ref.valid(*tracked)) {
                world_ref.DestroyEntity(*tracked);
                *tracked = entt::null;
            }
        }), false);
    return new_ent;
}

// 删除实体（全组件快照，撤销可完整还原）。
void DeleteEntityViaBus(EditorContext& context, entt::entity target) {
    if (!context.registry.valid(target)) return;
    if (context.command_bus) {
        core::DeleteEntityCmd cmd;
        cmd.entity_id = static_cast<uint32_t>(target);
        context.command_bus->dispatch(cmd, context.engine);
        return;
    }

    // ── Fallback：全组件快照 + 完整 undo/redo（do 销毁、undo 还原）──
    auto& world_ref = context.world;
    auto& reg_ref = context.registry;
    auto snapshot = std::make_shared<EntitySnapshot>(EntitySnapshot::Capture(reg_ref, target));
    auto tracked = std::make_shared<entt::entity>(target);
    GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
        "Delete Entity",
        [&world_ref, &reg_ref, tracked]() {
            if (*tracked != entt::null && reg_ref.valid(*tracked)) {
                world_ref.DestroyEntity(*tracked);
                *tracked = entt::null;
            }
        },
        [&world_ref, &reg_ref, snapshot, tracked]() {
            *tracked = snapshot->Restore(world_ref, reg_ref);
        }), false);
}

// 复制实体（全组件快照复制，挂根 + 改名 + 偏移）。返回副本（失败为 entt::null）。
entt::entity DuplicateEntityViaBus(EditorContext& context, entt::entity source) {
    if (!context.registry.valid(source)) return entt::null;
    if (context.command_bus) {
        core::DuplicateEntityCmd cmd;
        cmd.entity_id = static_cast<uint32_t>(source);
        auto res = context.command_bus->dispatch(cmd, context.engine);
        if (res.ok && res.data.IsObject() && res.data.HasMember("entity_id") &&
            res.data["entity_id"].IsUint()) {
            return static_cast<entt::entity>(res.data["entity_id"].GetUint());
        }
        return entt::null;
    }

    // ── Fallback：全组件快照复制 + 完整 undo/redo ──
    auto& world_ref = context.world;
    auto& reg_ref = context.registry;
    EntitySnapshot copy_snap = EntitySnapshot::Capture(reg_ref, source);
    copy_snap.parent.reset();
    copy_snap.sibling_index.reset();
    auto dst = copy_snap.Restore(world_ref, reg_ref);
    if (reg_ref.all_of<EditorNameComponent>(dst))
        reg_ref.get<EditorNameComponent>(dst).name += " (Copy)";
    if (reg_ref.all_of<TransformComponent>(dst)) {
        auto& tf = reg_ref.get<TransformComponent>(dst);
        tf.position += glm::vec3(0.5f, 0.0f, 0.5f);
        tf.dirty = true;
    }
    auto snapshot = std::make_shared<EntitySnapshot>(EntitySnapshot::Capture(reg_ref, dst));
    auto tracked = std::make_shared<entt::entity>(dst);
    GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
        "Duplicate Entity",
        [&world_ref, &reg_ref, snapshot, tracked]() {
            if (*tracked != entt::null && reg_ref.valid(*tracked)) return;
            *tracked = snapshot->Restore(world_ref, reg_ref);
        },
        [&world_ref, &reg_ref, tracked]() {
            if (*tracked != entt::null && reg_ref.valid(*tracked)) {
                world_ref.DestroyEntity(*tracked);
                *tracked = entt::null;
            }
        }), false);
    return dst;
}

// Pre-computed set of entities visible under current search filter (includes ancestors of matches)
static std::unordered_set<entt::entity> s_visible_entities;
static std::string s_last_computed_filter;
static size_t s_last_computed_entity_count = 0;

void ComputeVisibleEntities(entt::registry& registry) {
    std::string current_filter = s_search_filter;
    size_t entity_count = registry.storage<entt::entity>().size();
    if (current_filter == s_last_computed_filter && entity_count == s_last_computed_entity_count)
        return; // No change
    s_last_computed_filter = current_filter;
    s_last_computed_entity_count = entity_count;
    s_visible_entities.clear();
    if (current_filter.empty()) return; // All visible when no filter

    // First pass: find all directly matching entities
    std::vector<entt::entity> matching;
    for (auto entity : registry.storage<entt::entity>()) {
        if (!registry.valid(entity)) continue;
        std::string name = "Entity";
        if (registry.all_of<EditorNameComponent>(entity))
            name = registry.get<EditorNameComponent>(entity).name;
        if (MatchesSearchFilter(name)) {
            matching.push_back(entity);
            s_visible_entities.insert(entity);
        }
    }
    // Second pass: propagate visibility to ancestors
    for (auto entity : matching) {
        entt::entity current = entity;
        while (registry.all_of<ParentComponent>(current)) {
            entt::entity parent = registry.get<ParentComponent>(current).parent;
            if (parent == entt::null || !registry.valid(parent)) break;
            if (s_visible_entities.count(parent)) break; // Already processed
            s_visible_entities.insert(parent);
            current = parent;
        }
    }
}

bool IsEntityVisibleInSearch(entt::entity entity) {
    if (s_search_filter[0] == '\0') return true;
    return s_visible_entities.count(entity) > 0;
}

void DrawEntityNode(EditorContext& context, entt::entity entity) {
    if (!context.registry.valid(entity)) return;

    std::string entity_name = "Entity " + std::to_string(static_cast<uint32_t>(entity));
    if (context.registry.all_of<EditorNameComponent>(entity)) {
        entity_name = context.registry.get<EditorNameComponent>(entity).name;
    }

    if (!IsEntityVisibleInSearch(entity)) return;

    // Prefab instance suffix
    bool is_prefab = IsPrefabInstance(context.registry, entity);
    std::string suffix = is_prefab ? " (Prefab)" : "";

    const char* type_icon = GetEntityTypeIcon(context.registry, entity);
    auto& selection = SelectionManager::Get();
    const bool is_selected = selection.Contains(entity);
    const bool is_renaming = (s_renaming_entity == entity);

    std::vector<entt::entity> children = GetChildren(context.registry, entity);
    bool has_children = !children.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
    if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;
    if (!has_children) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Draw rounded highlight for selected entity
    if (is_selected) {
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        float item_width = ImGui::GetContentRegionAvail().x;
        float item_height = ImGui::GetTextLineHeightWithSpacing();
        ImU32 highlight_color = is_prefab ? IM_COL32(50, 180, 100, 80) : IM_COL32(71, 143, 255, 80);
        ImGui::GetWindowDrawList()->AddRectFilled(
            cursor_pos,
            ImVec2(cursor_pos.x + item_width, cursor_pos.y + item_height),
            highlight_color, 4.0f);
    }

    if (is_renaming) {
        ImGui::TextUnformatted(type_icon);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::SetKeyboardFocusHere();

        auto commit_rename = [&](entt::entity ent) {
            if (s_rename_buf[0] != '\0') {
                std::string old_name = entity_name;
                std::string new_name = s_rename_buf;
                if (context.registry.all_of<EditorNameComponent>(ent)) {
                    context.registry.get<EditorNameComponent>(ent).name = new_name;
                } else {
                    context.registry.emplace<EditorNameComponent>(ent, new_name);
                }
                auto& reg = context.registry;
                GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
                    "Rename Entity",
                    []{},
                    [&reg, ent, old_name]() {
                        if (reg.valid(ent) && reg.all_of<EditorNameComponent>(ent)) {
                            reg.get<EditorNameComponent>(ent).name = old_name;
                        }
                    }), false);
            }
            s_renaming_entity = entt::null;
        };

        if (ImGui::InputText("##rename", s_rename_buf, sizeof(s_rename_buf),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            commit_rename(entity);
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            s_renaming_entity = entt::null;
        } else if (!ImGui::IsItemActive() && s_renaming_entity == entity &&
                   ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()) {
            commit_rename(entity);
        }
    } else {
        std::string display_name = std::string(type_icon) + "  " + entity_name + suffix;
        bool node_open = ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", display_name.c_str());

        // Click handling
        if (ImGui::IsItemClicked()) {
            auto& sel = SelectionManager::Get();
            if (ImGui::GetIO().KeyCtrl) {
                sel.Toggle(entity);
                context.selected_entity = sel.GetPrimary();
            } else if (ImGui::GetIO().KeyShift && s_last_clicked_entity != entt::null) {
                sel.Clear();
                bool in_range = false;
                for (auto e : context.registry.storage<entt::entity>()) {
                    if (!context.registry.valid(e)) continue;
                    if (e == entity || e == s_last_clicked_entity) {
                        sel.Add(e);
                        if (in_range) break;
                        in_range = true;
                        continue;
                    }
                    if (in_range) sel.Add(e);
                }
                context.selected_entity = entity;
            } else {
                sel.SetSingle(entity);
                context.selected_entity = entity;
            }
            s_last_clicked_entity = entity;
        }

        // Double-click to rename
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && !context.read_only) {
            s_renaming_entity = entity;
            std::strncpy(s_rename_buf, entity_name.c_str(), sizeof(s_rename_buf) - 1);
            s_rename_buf[sizeof(s_rename_buf) - 1] = '\0';
        }

        // Drag source
        if (!context.read_only && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entity, sizeof(entt::entity));
            ImGui::Text("%s", entity_name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drop target: reparent dragged entity under this one
        if (!context.read_only && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                entt::entity dragged = *static_cast<const entt::entity*>(payload->Data);
                if (dragged != entity && context.registry.valid(dragged)) {
                    // 收敛写路径：经 CommandBus 调 dsengine_entity_reparent（含环检测、
                    // 统一撤销栈），不再直接改 registry。挂到目标实体下，不动兄弟序。
                    ReparentViaBus(context, dragged, /*parent=*/entity, /*sibling=*/std::nullopt);
                    EditorLog(LogLevel::Info, "Reparented entity under " + entity_name);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Between-entity insertion drop target (blue insert line)
        if (!context.read_only) {
            ImVec2 cursor_after = ImGui::GetCursorScreenPos();
            float insert_h = 4.0f;
            ImRect insert_rect(ImVec2(cursor_after.x, cursor_after.y - insert_h),
                              ImVec2(cursor_after.x + ImGui::GetContentRegionAvail().x, cursor_after.y));
            ImGui::SetCursorScreenPos(insert_rect.Min);
            ImGui::InvisibleButton((std::string("##insert_") + std::to_string(static_cast<uint32_t>(entity))).c_str(),
                                  insert_rect.GetSize());
            if (ImGui::BeginDragDropTarget()) {
                // Draw blue insertion line
                ImGui::GetWindowDrawList()->AddRectFilled(
                    insert_rect.Min, insert_rect.Max,
                    IM_COL32(71, 143, 255, 200), 2.0f);
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                    entt::entity dragged = *static_cast<const entt::entity*>(payload->Data);
                    if (dragged != entity && context.registry.valid(dragged)) {
                        // Move dragged entity to same parent as this entity, with sibling index after this one
                        entt::entity target_parent = entt::null;
                        if (context.registry.all_of<ParentComponent>(entity)) {
                            target_parent = context.registry.get<ParentComponent>(entity).parent;
                        }
                        int target_sibling = 0;
                        if (context.registry.all_of<SiblingIndexComponent>(entity)) {
                            target_sibling = context.registry.get<SiblingIndexComponent>(entity).index + 1;
                        }

                        // 收敛写路径：经 CommandBus 调 dsengine_entity_reparent，挂到与
                        // 目标同父（root 时 detach）并显式写入 sibling_index，统一撤销栈。
                        std::optional<entt::entity> new_parent =
                            (target_parent == entt::null) ? std::nullopt
                                                          : std::optional<entt::entity>(target_parent);
                        ReparentViaBus(context, dragged, new_parent, target_sibling);
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }

        // Recurse into children
        if (has_children && node_open) {
            for (auto child : children) {
                DrawEntityNode(context, child);
            }
            ImGui::TreePop();
        }
    }
}

} // namespace

void DrawHierarchyPanel(EditorContext& context) {
    ImGui::Begin("Hierarchy");

    // Search bar
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##hierarchy_search", MDI_ICON_MAGNIFY "  Search entities...", s_search_filter, sizeof(s_search_filter));

    // Pre-compute visible entity set for search (O(N) once per frame)
    ComputeVisibleEntities(context.registry);

    ImGui::Separator();

    bool hierarchy_clicked = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0);

    if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Drop target on Scene root: unparent entity or instantiate prefab
        if (!context.read_only && ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                std::string asset_path(static_cast<const char*>(payload->Data));
                if (asset_path.size() > 7 && asset_path.substr(asset_path.size() - 7) == ".dscene") {
                    auto new_ent = context.world.CreateEntity();
                    std::string sub_name = std::filesystem::path(asset_path).stem().string();
                    context.registry.emplace<EditorNameComponent>(new_ent, sub_name);
                    context.registry.emplace<TransformComponent>(new_ent);
                    dse::SubSceneComponent sub;
                    sub.scene_path = asset_path;
                    context.registry.emplace<dse::SubSceneComponent>(new_ent, sub);
                    context.selected_entity = new_ent;
                    EditorLog(LogLevel::Info, "Created sub-scene instance: " + asset_path);
                } else if (asset_path.size() > 8 && asset_path.substr(asset_path.size() - 8) == ".dprefab") {
                    auto& proj = ProjectManager::Get();
                    std::filesystem::path base = proj.HasOpenProject()
                        ? proj.GetAssetDir()
                        : (std::filesystem::current_path() / "samples" / "lua" / "data");
                    std::filesystem::path full_path = base / asset_path;
                    entt::entity new_ent = InstantiatePrefab(context.world, context.registry, full_path.string());
                    if (new_ent != entt::null) {
                        context.selected_entity = new_ent;
                        EditorLog(LogLevel::Info, "Instantiated prefab: " + asset_path);
                    }
                }
            }
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
                entt::entity dragged = *static_cast<const entt::entity*>(payload->Data);
                if (context.registry.valid(dragged) && context.registry.all_of<ParentComponent>(dragged)) {
                    // 收敛到 CommandBus：detach（parent=null）走 dsengine_entity_reparent，
                    // 与自动化/无头共用同一撤销栈与完整 undo/redo。
                    ReparentViaBus(context, dragged, std::nullopt, std::nullopt);
                    EditorLog(LogLevel::Info, "Unparented entity");
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Draw root-level entities (those without a parent), sorted by SiblingIndex
        {
            std::vector<entt::entity> root_entities;
            for (auto entity : context.registry.storage<entt::entity>()) {
                if (!context.registry.valid(entity)) continue;
                if (context.registry.all_of<ParentComponent>(entity) &&
                    context.registry.get<ParentComponent>(entity).parent != entt::null) {
                    continue;
                }
                root_entities.push_back(entity);
            }
            std::sort(root_entities.begin(), root_entities.end(), [&](entt::entity a, entt::entity b) {
                int ia = context.registry.all_of<SiblingIndexComponent>(a) ? context.registry.get<SiblingIndexComponent>(a).index : 0;
                int ib = context.registry.all_of<SiblingIndexComponent>(b) ? context.registry.get<SiblingIndexComponent>(b).index : 0;
                return ia < ib;
            });
            for (auto entity : root_entities) {
                DrawEntityNode(context, entity);
            }
        }
        ImGui::TreePop();
    }

    if (hierarchy_clicked && !ImGui::IsAnyItemHovered()) {
        context.selected_entity = entt::null;
        SelectionManager::Get().Clear();
    }

    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Create Empty Entity", nullptr, false, !context.read_only)) {
            entt::entity created = CreateEntityViaBus(context, "New Entity", {});
            if (created != entt::null) {
                context.selected_entity = created;
                EditorLog(LogLevel::Info, "Created entity: New Entity");
            }
        }
        if (ImGui::MenuItem("Create UI Entity", nullptr, false, !context.read_only)) {
            entt::entity created = CreateEntityViaBus(context, "New UI Element", {"UIRenderer"});
            if (created != entt::null) {
                context.selected_entity = created;
                EditorLog(LogLevel::Info, "Created entity: New UI Element");
            }
        }
        if (ImGui::BeginMenu("Create 3D Object", !context.read_only)) {
            if (ImGui::MenuItem("Cube", nullptr, false, !context.read_only))  CreateEntity3DCube(context);
            if (ImGui::MenuItem("Sphere", nullptr, false, !context.read_only)) CreateEntity3DSphere(context);
            if (ImGui::MenuItem("Plane", nullptr, false, !context.read_only))  CreateEntity3DPlane(context);
            ImGui::Separator();
            if (ImGui::MenuItem("Camera 3D", nullptr, false, !context.read_only)) CreateEntity3DCamera(context);
            if (ImGui::MenuItem("Directional Light", nullptr, false, !context.read_only)) CreateEntity3DDirectionalLight(context);
            if (ImGui::MenuItem("Point Light", nullptr, false, !context.read_only))       CreateEntity3DPointLight(context);
            if (ImGui::MenuItem("Spot Light", nullptr, false, !context.read_only))        CreateEntity3DSpotLight(context);
            ImGui::Separator();
            if (ImGui::MenuItem("Physics Box", nullptr, false, !context.read_only))    CreateEntity3DPhysicsBox(context);
            if (ImGui::MenuItem("Physics Sphere", nullptr, false, !context.read_only)) CreateEntity3DPhysicsSphere(context);
            ImGui::Separator();
            if (ImGui::MenuItem("Audio Source", nullptr, false, !context.read_only))   CreateEntity3DAudioSource(context);
            if (ImGui::MenuItem("Audio Listener", nullptr, false, !context.read_only)) CreateEntity3DAudioListener(context);
            ImGui::Separator();
            if (ImGui::MenuItem("Particle System 3D", nullptr, false, !context.read_only)) {
                auto new_ent = context.world.CreateEntity();
                context.registry.emplace<EditorNameComponent>(new_ent, "Particle 3D");
                context.registry.emplace<TransformComponent>(new_ent);
                context.registry.emplace<dse::ParticleSystem3DComponent>(new_ent);
                context.selected_entity = new_ent;
            }
            ImGui::EndMenu();
        }
        if (context.selected_entity != entt::null && ImGui::MenuItem("Save as Prefab", nullptr, false, !context.read_only)) {
            std::string prefab_name = "Entity";
            if (context.registry.all_of<EditorNameComponent>(context.selected_entity)) {
                prefab_name = context.registry.get<EditorNameComponent>(context.selected_entity).name;
            }
            std::filesystem::path prefab_dir = std::filesystem::current_path() / "samples" / "lua" / "data" / "prefabs";
            std::filesystem::create_directories(prefab_dir);
            std::string prefab_path = (prefab_dir / (prefab_name + ".dprefab")).string();
            if (SaveEntityAsPrefab(context.registry, context.selected_entity, prefab_path)) {
                EditorLog(LogLevel::Info, "Saved prefab: " + prefab_path);
            } else {
                EditorLog(LogLevel::Error, "Failed to save prefab");
            }
        }
        if (context.selected_entity != entt::null && ImGui::MenuItem("Delete Entity", nullptr, false, !context.read_only)) {
            entt::entity to_delete = context.selected_entity;
            std::string deleted_name = "Entity";
            if (context.registry.all_of<EditorNameComponent>(to_delete)) {
                deleted_name = context.registry.get<EditorNameComponent>(to_delete).name;
            }
            DeleteEntityViaBus(context, to_delete);
            context.selected_entity = entt::null;
            EditorLog(LogLevel::Info, "Deleted entity: " + deleted_name);
        }
        if (context.selected_entity != entt::null && ImGui::MenuItem("Duplicate Entity", nullptr, false, !context.read_only)) {
            entt::entity dup = DuplicateEntityViaBus(context, context.selected_entity);
            if (dup != entt::null) {
                context.selected_entity = dup;
                EditorLog(LogLevel::Info, "Duplicated entity");
            }
        }
        if (context.read_only) {
            ImGui::Separator();
            ImGui::TextDisabled("Remote: 可选择实体查看/编辑属性，结构操作已禁用");
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

void BeginHierarchyRename(entt::entity entity, const std::string& current_name) {
    s_renaming_entity = entity;
    std::strncpy(s_rename_buf, current_name.c_str(), sizeof(s_rename_buf) - 1);
    s_rename_buf[sizeof(s_rename_buf) - 1] = '\0';
}

} // namespace dse::editor
