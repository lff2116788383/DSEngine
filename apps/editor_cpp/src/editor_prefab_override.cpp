#include "editor_prefab_override.h"
#include "editor_prefab.h"
#include "editor_prefab_marker.h"
#include "editor_context.h"
#include "editor_icons.h"
#include "editor_shared_components.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "imgui.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <cmath>

namespace dse::editor {

namespace {

std::string Vec3ToString(const glm::vec3& v) {
    char buf[128];
    snprintf(buf, sizeof(buf), "(%.3f, %.3f, %.3f)", v.x, v.y, v.z);
    return buf;
}

std::string QuatToString(const glm::quat& q) {
    char buf[128];
    snprintf(buf, sizeof(buf), "(%.3f, %.3f, %.3f, %.3f)", q.w, q.x, q.y, q.z);
    return buf;
}

std::string FloatToString(float v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", v);
    return buf;
}

bool Vec3Equal(const glm::vec3& a, const glm::vec3& b, float eps = 0.001f) {
    return std::abs(a.x - b.x) < eps && std::abs(a.y - b.y) < eps && std::abs(a.z - b.z) < eps;
}

bool QuatEqual(const glm::quat& a, const glm::quat& b, float eps = 0.001f) {
    return std::abs(a.w - b.w) < eps && std::abs(a.x - b.x) < eps &&
           std::abs(a.y - b.y) < eps && std::abs(a.z - b.z) < eps;
}

glm::vec3 ReadVec3(const rapidjson::Value& arr) {
    return glm::vec3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat());
}

glm::quat ReadQuat(const rapidjson::Value& arr) {
    return glm::quat(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat());
}

/// Load the prefab JSON document from disk. Returns false on failure.
bool LoadPrefabDoc(const std::string& path, rapidjson::Document& doc) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    doc.Parse(json.c_str());
    return !doc.HasParseError() && doc.IsObject();
}

/// Compare transform between prefab JSON and current entity, add overrides
void CompareTransform(entt::registry& reg, entt::entity entity,
                      const rapidjson::Document& doc,
                      std::vector<PrefabPropertyOverride>& overrides) {
    if (!reg.all_of<TransformComponent>(entity)) return;
    auto& tf = reg.get<TransformComponent>(entity);

    if (doc.HasMember("transform") && doc["transform"].IsObject()) {
        auto& t = doc["transform"];
        if (t.HasMember("position") && t["position"].IsArray()) {
            glm::vec3 orig = ReadVec3(t["position"]);
            if (!Vec3Equal(tf.position, orig)) {
                overrides.push_back({"Transform", "Position", Vec3ToString(orig), Vec3ToString(tf.position)});
            }
        }
        if (t.HasMember("rotation") && t["rotation"].IsArray()) {
            glm::quat orig = ReadQuat(t["rotation"]);
            if (!QuatEqual(tf.rotation, orig)) {
                overrides.push_back({"Transform", "Rotation", QuatToString(orig), QuatToString(tf.rotation)});
            }
        }
        if (t.HasMember("scale") && t["scale"].IsArray()) {
            glm::vec3 orig = ReadVec3(t["scale"]);
            if (!Vec3Equal(tf.scale, orig)) {
                overrides.push_back({"Transform", "Scale", Vec3ToString(orig), Vec3ToString(tf.scale)});
            }
        }
    }
}

/// Compare mesh renderer
void CompareMeshRenderer(entt::registry& reg, entt::entity entity,
                          const rapidjson::Document& doc,
                          std::vector<PrefabPropertyOverride>& overrides) {
    bool has_comp = reg.all_of<dse::MeshRendererComponent>(entity);
    bool has_json = doc.HasMember("mesh_renderer") && doc["mesh_renderer"].IsObject();

    if (has_comp && !has_json) {
        overrides.push_back({"MeshRenderer", "(added)", "", "new"});
    } else if (!has_comp && has_json) {
        overrides.push_back({"MeshRenderer", "(removed)", "existed", ""});
    } else if (has_comp && has_json) {
        auto& mesh = reg.get<dse::MeshRendererComponent>(entity);
        auto& m = doc["mesh_renderer"];
        if (m.HasMember("mesh_path") && m["mesh_path"].IsString()) {
            std::string orig = m["mesh_path"].GetString();
            if (mesh.mesh_path != orig) {
                overrides.push_back({"MeshRenderer", "MeshPath", orig, mesh.mesh_path});
            }
        }
        if (m.HasMember("visible") && m["visible"].IsBool()) {
            if (mesh.visible != m["visible"].GetBool()) {
                overrides.push_back({"MeshRenderer", "Visible",
                    m["visible"].GetBool() ? "true" : "false",
                    mesh.visible ? "true" : "false"});
            }
        }
    }
}

/// Compare name component
void CompareName(entt::registry& reg, entt::entity entity,
                  const rapidjson::Document& doc,
                  std::vector<PrefabPropertyOverride>& overrides) {
    if (!reg.all_of<EditorNameComponent>(entity)) return;
    auto& name = reg.get<EditorNameComponent>(entity);
    if (doc.HasMember("name") && doc["name"].IsString()) {
        std::string orig = doc["name"].GetString();
        if (name.name != orig) {
            overrides.push_back({"Name", "Name", orig, name.name});
        }
    }
}

} // namespace

PrefabOverrideInfo ComputePrefabOverrides(entt::registry& registry, entt::entity entity) {
    PrefabOverrideInfo info;
    info.entity = entity;

    if (!registry.valid(entity)) return info;
    if (!registry.all_of<PrefabMarkerComponent>(entity)) return info;

    info.prefab_source_path = registry.get<PrefabMarkerComponent>(entity).source_path;
    if (info.prefab_source_path.empty()) return info;

    rapidjson::Document doc;
    if (!LoadPrefabDoc(info.prefab_source_path, doc)) return info;

    CompareName(registry, entity, doc, info.overrides);
    CompareTransform(registry, entity, doc, info.overrides);
    CompareMeshRenderer(registry, entity, doc, info.overrides);

    // Check for new/removed components at a high level
    for (auto& ov : info.overrides) {
        if (ov.property_name == "(added)") info.has_new_components = true;
        if (ov.property_name == "(removed)") info.has_removed_components = true;
    }

    return info;
}

bool RevertPrefabOverride(entt::registry& registry, entt::entity entity,
                           const PrefabPropertyOverride& override_info) {
    if (!registry.valid(entity)) return false;
    if (!registry.all_of<PrefabMarkerComponent>(entity)) return false;

    auto& marker = registry.get<PrefabMarkerComponent>(entity);
    rapidjson::Document doc;
    if (!LoadPrefabDoc(marker.source_path, doc)) return false;

    // Revert Transform properties
    if (override_info.component_name == "Transform" && registry.all_of<TransformComponent>(entity)) {
        auto& tf = registry.get<TransformComponent>(entity);
        if (doc.HasMember("transform") && doc["transform"].IsObject()) {
            auto& t = doc["transform"];
            if (override_info.property_name == "Position" && t.HasMember("position")) {
                tf.position = ReadVec3(t["position"]);
                tf.dirty = true;
            } else if (override_info.property_name == "Rotation" && t.HasMember("rotation")) {
                tf.rotation = ReadQuat(t["rotation"]);
                tf.dirty = true;
            } else if (override_info.property_name == "Scale" && t.HasMember("scale")) {
                tf.scale = ReadVec3(t["scale"]);
                tf.dirty = true;
            }
        }
        return true;
    }

    if (override_info.component_name == "Name" && registry.all_of<EditorNameComponent>(entity)) {
        if (doc.HasMember("name") && doc["name"].IsString()) {
            registry.get<EditorNameComponent>(entity).name = doc["name"].GetString();
        }
        return true;
    }

    return false;
}

bool RevertAllPrefabOverrides(entt::registry& registry, entt::entity entity) {
    auto info = ComputePrefabOverrides(registry, entity);
    if (info.overrides.empty()) return false;

    for (auto& ov : info.overrides) {
        RevertPrefabOverride(registry, entity, ov);
    }
    return true;
}

bool ApplyOverridesToPrefab(entt::registry& registry, entt::entity entity) {
    if (!registry.valid(entity)) return false;
    if (!registry.all_of<PrefabMarkerComponent>(entity)) return false;

    auto& marker = registry.get<PrefabMarkerComponent>(entity);
    return SaveEntityAsPrefab(registry, entity, marker.source_path);
}

void DrawPrefabOverrideSection(EditorContext& ctx) {
    if (ctx.selected_entity == entt::null) return;
    if (!ctx.registry.valid(ctx.selected_entity)) return;
    if (!IsPrefabInstance(ctx.registry, ctx.selected_entity)) return;

    auto info = ComputePrefabOverrides(ctx.registry, ctx.selected_entity);

    ImGui::Separator();
    ImU32 prefab_color = info.overrides.empty()
        ? IM_COL32(100, 200, 100, 255)
        : IM_COL32(255, 180, 80, 255);

    ImGui::PushStyleColor(ImGuiCol_Text, ImColor(prefab_color).Value);
    bool open = ImGui::CollapsingHeader(MDI_ICON_CONTENT_COPY "  Prefab Instance",
                                         ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor();

    if (!open) return;

    ImGui::TextDisabled("Source: %s", info.prefab_source_path.c_str());

    if (info.overrides.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "No overrides (matches prefab)");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%d override(s)",
                          static_cast<int>(info.overrides.size()));

        ImGui::Columns(3, "prefab_overrides", true);
        ImGui::SetColumnWidth(0, 140.0f);
        ImGui::SetColumnWidth(1, 100.0f);
        ImGui::Text("Property"); ImGui::NextColumn();
        ImGui::Text("Original"); ImGui::NextColumn();
        ImGui::Text("Current"); ImGui::NextColumn();
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(info.overrides.size()); i++) {
            auto& ov = info.overrides[i];
            ImGui::PushID(i);

            // Property name with component prefix
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s.%s",
                              ov.component_name.c_str(), ov.property_name.c_str());
            ImGui::NextColumn();

            // Original value
            ImGui::TextDisabled("%s", ov.original_value.c_str());
            ImGui::NextColumn();

            // Current value + revert button
            ImGui::Text("%s", ov.current_value.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Revert")) {
                RevertPrefabOverride(ctx.registry, ctx.selected_entity, ov);
            }
            ImGui::NextColumn();

            ImGui::PopID();
        }
        ImGui::Columns(1);
    }

    ImGui::Spacing();
    if (!info.overrides.empty()) {
        if (ImGui::Button("Revert All", ImVec2(100, 0))) {
            RevertAllPrefabOverrides(ctx.registry, ctx.selected_entity);
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply to Prefab", ImVec2(120, 0))) {
            ApplyOverridesToPrefab(ctx.registry, ctx.selected_entity);
        }
    }
}

} // namespace dse::editor
