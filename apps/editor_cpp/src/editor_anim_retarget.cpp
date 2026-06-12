// 骨骼动画重定向面板（ImGui）。导入源/目标模型 → 自动/手动建立骨骼映射 →
// 烘焙出以目标骨架命名的新动画 (.danim)。纯映射逻辑见 editor_anim_retarget_core。

#include "editor_anim_retarget.h"
#include "editor_anim_retarget_core.h"
#include "editor_context.h"
#include "editor_file_dialog.h"

#include "engine/assets/compiler/importer.h"
#include "engine/assets/compiler/raw_scene_data.h"

#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace dse::editor {

namespace {

using namespace dse::asset::compiler;
namespace rt = dse::editor::retarget;

struct ModelData {
    std::string path;
    std::vector<std::string> bone_names;
    RawSceneData scene;
    bool loaded = false;
    std::string status;
};

struct RetargetState {
    ModelData source;
    ModelData target;
    rt::BoneMap map;
    int selected_anim = 0;
    char out_dir[260] = "assets/animations";
    char out_name[128] = "retargeted";
    char src_path[260] = "";
    char tgt_path[260] = "";
    std::string bake_status;
};

RetargetState& State() {
    static RetargetState s;
    return s;
}

bool HasExt(const std::string& path, const char* ext) {
    if (path.size() < std::strlen(ext)) return false;
    std::string tail = path.substr(path.size() - std::strlen(ext));
    std::transform(tail.begin(), tail.end(), tail.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return tail == ext;
}

bool ImportModel(const std::string& path, ModelData& out) {
    out = ModelData{};
    out.path = path;
#if !defined(DSE_EDITOR_HAS_IMPORTER)
    out.status = "Importer not built (assimp unavailable)";
    return false;
#else
    bool ok = false;
    if (HasExt(path, ".gltf") || HasExt(path, ".glb")) {
        GltfImporter imp;
        ok = imp.Import(path, out.scene);
    } else if (HasExt(path, ".fbx")) {
        FbxImporter imp;
        ok = imp.Import(path, out.scene);
    } else {
        out.status = "Unsupported format (use .gltf/.glb/.fbx)";
        return false;
    }
    if (!ok) {
        out.status = "Import failed";
        return false;
    }
    out.bone_names.clear();
    out.bone_names.reserve(out.scene.skeleton.size());
    for (const auto& b : out.scene.skeleton) out.bone_names.push_back(b.name);
    out.loaded = true;
    out.status = std::to_string(out.bone_names.size()) + " bones, " +
                 std::to_string(out.scene.animations.size()) + " anims";
    return true;
#endif
}

void RecomputeMap(RetargetState& s) {
    if (s.source.loaded && s.target.loaded) {
        s.map = rt::AutoMapBones(s.source.bone_names, s.target.bone_names);
    } else {
        s.map = rt::BoneMap{};
    }
}

ImVec4 MatchColor(rt::MatchType t) {
    switch (t) {
        case rt::MatchType::Exact:      return ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
        case rt::MatchType::Normalized: return ImVec4(0.6f, 0.9f, 0.5f, 1.0f);
        case rt::MatchType::Humanoid:   return ImVec4(0.5f, 0.8f, 1.0f, 1.0f);
        case rt::MatchType::Manual:     return ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
        default:                        return ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
    }
}

const char* MatchLabel(rt::MatchType t) {
    switch (t) {
        case rt::MatchType::Exact:      return "exact";
        case rt::MatchType::Normalized: return "norm";
        case rt::MatchType::Humanoid:   return "humanoid";
        case rt::MatchType::Manual:     return "manual";
        default:                        return "none";
    }
}

void BakeRetargeted(RetargetState& s) {
    if (!s.source.loaded || !s.target.loaded) { s.bake_status = "Load both models first"; return; }
    if (s.source.scene.animations.empty()) { s.bake_status = "Source has no animations"; return; }
    int ai = std::clamp(s.selected_anim, 0, static_cast<int>(s.source.scene.animations.size()) - 1);

    RawAnimation retargeted = rt::RetargetAnimation(
        s.source.scene.animations[ai], s.source.bone_names, s.target.bone_names, s.map);

    if (retargeted.channels.empty()) { s.bake_status = "No mapped channels — check mapping"; return; }

#if !defined(DSE_EDITOR_HAS_IMPORTER)
    s.bake_status = "Cooker not built (assimp unavailable)";
#else
    // 用目标骨架 + 重定向后的动画拼出一份 RawSceneData，复用 cooker 写 .danim。
    RawSceneData out_scene;
    out_scene.skeleton = s.target.scene.skeleton;
    out_scene.animations.push_back(retargeted);

    MeshCooker cooker;
    const bool ok = cooker.CookToDanim(out_scene, s.out_dir, s.out_name);
    s.bake_status = ok ? ("Baked " + std::to_string(retargeted.channels.size()) +
                          " channels -> " + std::string(s.out_dir) + "/" + s.out_name + ".danim")
                       : "Cook failed (check output dir)";
#endif
}

void DrawModelRow(const char* label, ModelData& model, char (&path_buf)[260],
                  RetargetState& s) {
    ImGui::PushID(label);
    ImGui::Text("%s", label);
    ImGui::SameLine(90);
    ImGui::SetNextItemWidth(360);
    ImGui::InputText("##path", path_buf, sizeof(path_buf));
    ImGui::SameLine();
    if (ImGui::Button("...")) {
        std::string picked = OpenSceneFileDialog();  // 复用通用文件选择
        if (!picked.empty()) {
            std::strncpy(path_buf, picked.c_str(), sizeof(path_buf) - 1);
            path_buf[sizeof(path_buf) - 1] = '\0';
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        ImportModel(path_buf, model);
        RecomputeMap(s);
    }
    if (model.loaded) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", model.status.c_str());
    } else if (!model.status.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", model.status.c_str());
    }
    ImGui::PopID();
}

}  // namespace

void DrawAnimRetargetPanel(EditorContext& /*ctx*/) {
    RetargetState& s = State();

    ImGui::TextWrapped("Retarget a skeletal animation from a source skeleton onto a target skeleton "
                       "by bone-name mapping (exact / normalized / humanoid synonyms + manual override).");
    ImGui::Separator();

    DrawModelRow("Source", s.source, s.src_path, s);
    DrawModelRow("Target", s.target, s.tgt_path, s);

    ImGui::Separator();

    if (!(s.source.loaded && s.target.loaded)) {
        ImGui::TextDisabled("Load both a source and a target model to build a bone mapping.");
        return;
    }

    const int mapped = rt::MappedCount(s.map);
    ImGui::Text("Mapped %d / %d source bones", mapped, static_cast<int>(s.source.bone_names.size()));
    ImGui::SameLine();
    if (ImGui::Button("Auto Map")) RecomputeMap(s);

    // ── 映射表 ────────────────────────────────────────────────────────────
    if (ImGui::BeginTable("retarget_map", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0, 280))) {
        ImGui::TableSetupColumn("Source Bone");
        ImGui::TableSetupColumn("Match", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Target Bone");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(s.map.matches.size()); ++i) {
            rt::BoneMatch& m = s.map.matches[i];
            ImGui::PushID(i);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(s.source.bone_names[i].c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(MatchColor(m.type), "%s", MatchLabel(m.type));

            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1);
            const char* preview = (m.target_index >= 0)
                ? s.target.bone_names[m.target_index].c_str() : "(unmapped)";
            if (ImGui::BeginCombo("##tgt", preview)) {
                if (ImGui::Selectable("(unmapped)", m.target_index < 0)) {
                    rt::SetManualMapping(s.map, i, -1);
                }
                for (int t = 0; t < static_cast<int>(s.target.bone_names.size()); ++t) {
                    const bool sel = (m.target_index == t);
                    if (ImGui::Selectable(s.target.bone_names[t].c_str(), sel)) {
                        rt::SetManualMapping(s.map, i, t);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    // ── 烘焙 ──────────────────────────────────────────────────────────────
    const auto& anims = s.source.scene.animations;
    if (!anims.empty()) {
        const char* cur = anims[std::clamp(s.selected_anim, 0, (int)anims.size() - 1)].name.c_str();
        ImGui::SetNextItemWidth(220);
        if (ImGui::BeginCombo("Animation", cur)) {
            for (int a = 0; a < static_cast<int>(anims.size()); ++a) {
                const bool sel = (s.selected_anim == a);
                const char* nm = anims[a].name.empty() ? "(unnamed)" : anims[a].name.c_str();
                if (ImGui::Selectable(nm, sel)) s.selected_anim = a;
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextDisabled("Source model has no animations to retarget.");
    }

    ImGui::SetNextItemWidth(220);
    ImGui::InputText("Output Dir", s.out_dir, sizeof(s.out_dir));
    ImGui::SameLine();
    if (ImGui::Button("...##od")) {
        std::string dir = BrowseFolderDialog("Select output folder");
        if (!dir.empty()) { std::strncpy(s.out_dir, dir.c_str(), sizeof(s.out_dir) - 1); s.out_dir[sizeof(s.out_dir) - 1] = '\0'; }
    }
    ImGui::SetNextItemWidth(220);
    ImGui::InputText("Base Name", s.out_name, sizeof(s.out_name));

    ImGui::BeginDisabled(anims.empty());
    if (ImGui::Button("Bake Retargeted .danim")) BakeRetargeted(s);
    ImGui::EndDisabled();

    if (!s.bake_status.empty()) {
        ImGui::SameLine();
        ImGui::TextWrapped("%s", s.bake_status.c_str());
    }
}

}  // namespace dse::editor
