#include "editor_anim_state_machine.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/animation_state_machine.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace dse::editor {

namespace {

// ── Visual node state ─────────────────────────────────────────────
struct NodeVisual {
    ImVec2 pos = {0, 0};
    ImVec2 size = {160, 60};
};

struct ASMEditorState {
    std::unordered_map<std::string, NodeVisual> node_positions;
    ImVec2 scroll_offset = {0, 0};

    // Transition creation
    bool creating_link = false;
    std::string link_source;

    // Selected
    std::string selected_state;
    std::string selected_transition_src;
    int selected_transition_idx = -1;

    float zoom = 1.0f;
    bool needs_auto_layout = true;

    // Track which entity is being edited to detect changes
    entt::entity last_entity = entt::null;
};

ASMEditorState& GetASMState() {
    static ASMEditorState state;
    return state;
}

// ── Auto-layout: arrange states in a grid ──────────────────────────
void AutoLayoutStates(const std::unordered_map<std::string, dse::gameplay3d::AnimState>& states,
                       const std::string& default_state,
                       ASMEditorState& es) {
    es.node_positions.clear();
    float x = 80.0f, y = 80.0f;
    int col = 0;
    const float spacing_x = 220.0f;
    const float spacing_y = 100.0f;
    const int cols = 4;

    // Place default state first
    if (!default_state.empty() && states.count(default_state)) {
        es.node_positions[default_state] = {{x, y}, {160, 60}};
        col++;
    }

    for (auto it = states.begin(); it != states.end(); ++it) {
        if (it->first == default_state) continue;
        float nx = x + static_cast<float>(col % cols) * spacing_x;
        float ny = y + static_cast<float>(col / cols) * spacing_y;
        es.node_positions[it->first] = {{nx, ny}, {160, 60}};
        col++;
    }
}

// ── Draw a bezier arrow between two nodes ──────────────────────────
void DrawTransitionArrow(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 color, bool selected) {
    float thickness = selected ? 3.0f : 1.5f;

    ImVec2 dir = ImVec2(to.x - from.x, to.y - from.y);
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 1.0f) return;

    float tangent_len = len * 0.4f;
    ImVec2 cp1 = ImVec2(from.x + tangent_len, from.y);
    ImVec2 cp2 = ImVec2(to.x - tangent_len, to.y);

    dl->AddBezierCubic(from, cp1, cp2, to, color, thickness);

    // Arrowhead
    ImVec2 norm = ImVec2(dir.x / len, dir.y / len);
    ImVec2 perp = ImVec2(-norm.y, norm.x);
    float arrow_size = 8.0f;
    ImVec2 arrow_tip = to;
    ImVec2 arrow_a = ImVec2(to.x - norm.x * arrow_size + perp.x * arrow_size * 0.5f,
                             to.y - norm.y * arrow_size + perp.y * arrow_size * 0.5f);
    ImVec2 arrow_b = ImVec2(to.x - norm.x * arrow_size - perp.x * arrow_size * 0.5f,
                             to.y - norm.y * arrow_size - perp.y * arrow_size * 0.5f);
    dl->AddTriangleFilled(arrow_tip, arrow_a, arrow_b, color);
}

// ── Condition mode name ────────────────────────────────────────────
const char* ConditionModeName(dse::gameplay3d::AnimConditionMode mode) {
    switch (mode) {
        case dse::gameplay3d::AnimConditionMode::Greater:  return ">";
        case dse::gameplay3d::AnimConditionMode::Less:     return "<";
        case dse::gameplay3d::AnimConditionMode::Equals:   return "==";
        case dse::gameplay3d::AnimConditionMode::NotEqual: return "!=";
        case dse::gameplay3d::AnimConditionMode::If:       return "If";
        case dse::gameplay3d::AnimConditionMode::IfNot:    return "IfNot";
    }
    return "?";
}

const char* ParamTypeName(dse::gameplay3d::AnimParamType t) {
    switch (t) {
        case dse::gameplay3d::AnimParamType::Float:   return "Float";
        case dse::gameplay3d::AnimParamType::Int:     return "Int";
        case dse::gameplay3d::AnimParamType::Bool:    return "Bool";
        case dse::gameplay3d::AnimParamType::Trigger: return "Trigger";
    }
    return "?";
}

void DrawSelectedStateInspector(dse::gameplay3d::AnimationStateMachine& asm_ref, ASMEditorState& es) {
    if (es.selected_state.empty()) return;

    std::unordered_map<std::string, dse::gameplay3d::AnimState>& mutable_states = asm_ref.GetStatesMutable();
    auto state_it = mutable_states.find(es.selected_state);
    if (state_it == mutable_states.end()) return;

    dse::gameplay3d::AnimState& st = state_it->second;
    ImGui::Separator();
    ImGui::Text("State: %s", st.name.c_str());

    if (!st.is_blend_tree) {
        char path_buf[256] = {};
        strncpy_s(path_buf, sizeof(path_buf), st.danim_path.c_str(), _TRUNCATE);
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputText("Clip Path", path_buf, sizeof(path_buf))) {
            st.danim_path = path_buf;
        }
    }
    ImGui::DragFloat("Speed", &st.speed, 0.01f, 0.0f, 10.0f);
    ImGui::Checkbox("Loop", &st.loop);

    if (st.is_blend_tree) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.6f, 1, 1), "Blend Tree");
        char bp_buf[128] = {};
        strncpy_s(bp_buf, sizeof(bp_buf), st.blend_parameter.c_str(), _TRUNCATE);
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("Blend Param", bp_buf, sizeof(bp_buf))) {
            st.blend_parameter = bp_buf;
        }

        for (int i = 0; i < static_cast<int>(st.blend_nodes.size()); i++) {
            ImGui::PushID(i);
            dse::gameplay3d::BlendTreeNode& bn = st.blend_nodes[i];
            char bn_buf[256] = {};
            strncpy_s(bn_buf, sizeof(bn_buf), bn.danim_path.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(200);
            if (ImGui::InputText("##clip", bn_buf, sizeof(bn_buf))) {
                bn.danim_path = bn_buf;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::DragFloat("##thresh", &bn.threshold, 0.01f);
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                st.blend_nodes.erase(st.blend_nodes.begin() + i);
                i--;
            }
            ImGui::PopID();
        }
        if (ImGui::SmallButton("+ Add Blend Node")) {
            st.blend_nodes.push_back({});
        }
    }

    // Transitions
    ImGui::Separator();
    ImGui::Text("Transitions (%d):", (int)st.transitions.size());
    for (int ti = 0; ti < static_cast<int>(st.transitions.size()); ti++) {
        dse::gameplay3d::AnimTransition& trans = st.transitions[ti];
        ImGui::PushID(ti);
        bool sel = (es.selected_transition_src == es.selected_state && es.selected_transition_idx == ti);
        if (ImGui::Selectable(("-> " + trans.target_state).c_str(), sel)) {
            es.selected_transition_src = es.selected_state;
            es.selected_transition_idx = ti;
        }
        if (sel) {
            ImGui::Indent();
            ImGui::Checkbox("Has Exit Time", &trans.has_exit_time);
            if (trans.has_exit_time) {
                ImGui::DragFloat("Exit Time", &trans.exit_time, 0.01f, 0.0f, 1.0f);
            }
            ImGui::DragFloat("Duration", &trans.transition_duration, 0.01f, 0.0f, 5.0f);

            ImGui::Text("Conditions (%d):", (int)trans.conditions.size());
            for (int ci = 0; ci < static_cast<int>(trans.conditions.size()); ci++) {
                ImGui::PushID(ci);
                dse::gameplay3d::AnimTransitionCondition& cond = trans.conditions[ci];
                char cn_buf[128] = {};
                strncpy_s(cn_buf, sizeof(cn_buf), cond.parameter_name.c_str(), _TRUNCATE);
                ImGui::SetNextItemWidth(100);
                if (ImGui::InputText("##pn", cn_buf, sizeof(cn_buf))) {
                    cond.parameter_name = cn_buf;
                }
                ImGui::SameLine();
                ImGui::Text("%s", ConditionModeName(cond.mode));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60);
                ImGui::DragFloat("##th", &cond.threshold, 0.01f);
                ImGui::SameLine();
                if (ImGui::SmallButton("X##c")) {
                    trans.conditions.erase(trans.conditions.begin() + ci);
                    ci--;
                }
                ImGui::PopID();
            }
            if (ImGui::SmallButton("+ Condition")) {
                dse::gameplay3d::AnimTransitionCondition nc;
                nc.mode = dse::gameplay3d::AnimConditionMode::Greater;
                trans.conditions.push_back(nc);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete Transition")) {
                st.transitions.erase(st.transitions.begin() + ti);
                es.selected_transition_idx = -1;
                ti--;
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
}

} // namespace

void DrawAnimStateMachinePanel(EditorContext& ctx) {
    ImGui::Begin("Anim State Machine");

    entt::entity entity = ctx.selected_entity;

    if (entity == entt::null || !ctx.registry.valid(entity)) {
        ImGui::TextDisabled("Select an entity with AnimatorComponent");
        ImGui::End();
        return;
    }

    if (!ctx.registry.all_of<dse::Animator3DComponent>(entity)) {
        ImGui::TextDisabled("Selected entity has no Animator3DComponent");
        ImGui::End();
        return;
    }

    dse::Animator3DComponent& animator = ctx.registry.get<dse::Animator3DComponent>(entity);
    dse::gameplay3d::AnimationStateMachine* asm_ptr = animator.state_machine.get();
    if (!asm_ptr) {
        ImGui::TextDisabled("AnimatorComponent has no state machine");
        ImGui::End();
        return;
    }

    dse::gameplay3d::AnimationStateMachine& asm_ref = *asm_ptr;
    const std::unordered_map<std::string, dse::gameplay3d::AnimState>& states = asm_ref.GetStates();
    const std::unordered_map<std::string, dse::gameplay3d::AnimParameter>& params = asm_ref.GetParameters();
    const std::string& default_state = asm_ref.GetDefaultState();
    ASMEditorState& es = GetASMState();

    // Reset layout when switching to a different entity
    if (es.last_entity != entity) {
        es.last_entity = entity;
        es.needs_auto_layout = true;
        es.selected_state.clear();
        es.selected_transition_src.clear();
        es.selected_transition_idx = -1;
    }

    // Auto-layout on first open or entity change
    if (es.needs_auto_layout) {
        AutoLayoutStates(states, default_state, es);
        es.needs_auto_layout = false;
    }

    // ── Parameters side panel ──────────────────────────────────────
    float sidebar_width = 200.0f;
    ImGui::BeginChild("##ASMParams", ImVec2(sidebar_width, 0), true);
    ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Parameters");
    ImGui::Separator();

    for (std::unordered_map<std::string, dse::gameplay3d::AnimParameter>::const_iterator pit = params.begin(); pit != params.end(); ++pit) {
        const std::string& pname = pit->first;
        const dse::gameplay3d::AnimParameter& param = pit->second;
        ImGui::PushID(pname.c_str());
        ImGui::TextDisabled("[%s]", ParamTypeName(param.type));
        ImGui::SameLine();
        ImGui::Text("%s", pname.c_str());
        ImGui::SameLine(sidebar_width - 70);

        switch (param.type) {
            case dse::gameplay3d::AnimParamType::Float: {
                float val = std::get<float>(param.value);
                ImGui::SetNextItemWidth(60);
                if (ImGui::DragFloat("##v", &val, 0.01f)) {
                    asm_ref.SetFloat(pname, val);
                }
                break;
            }
            case dse::gameplay3d::AnimParamType::Int: {
                int val = std::get<int>(param.value);
                ImGui::SetNextItemWidth(60);
                if (ImGui::DragInt("##v", &val)) {
                    asm_ref.SetInt(pname, val);
                }
                break;
            }
            case dse::gameplay3d::AnimParamType::Bool: {
                bool val = std::get<bool>(param.value);
                if (ImGui::Checkbox("##v", &val)) {
                    asm_ref.SetBool(pname, val);
                }
                break;
            }
            case dse::gameplay3d::AnimParamType::Trigger: {
                if (ImGui::SmallButton("Fire")) {
                    asm_ref.SetTrigger(pname);
                }
                break;
            }
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    // Add parameter
    {
        static char s_new_param_name[128] = "";
        static int s_new_param_type = 0;
        ImGui::SetNextItemWidth(100);
        ImGui::InputText("##pname", s_new_param_name, sizeof(s_new_param_name));
        ImGui::SameLine();
        const char* type_names[] = {"Float", "Int", "Bool", "Trigger"};
        ImGui::SetNextItemWidth(70);
        ImGui::Combo("##ptype", &s_new_param_type, type_names, 4);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##addp") && s_new_param_name[0] != '\0') {
            auto pt = static_cast<dse::gameplay3d::AnimParamType>(s_new_param_type);
            switch (pt) {
                case dse::gameplay3d::AnimParamType::Float:   asm_ref.AddParameter(s_new_param_name, pt, 0.0f); break;
                case dse::gameplay3d::AnimParamType::Int:     asm_ref.AddParameter(s_new_param_name, pt, 0); break;
                case dse::gameplay3d::AnimParamType::Bool:    asm_ref.AddParameter(s_new_param_name, pt, false); break;
                case dse::gameplay3d::AnimParamType::Trigger: asm_ref.AddTrigger(s_new_param_name); break;
            }
            s_new_param_name[0] = '\0';
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 1, 0.5f, 1), "States: %d", (int)states.size());
    for (std::unordered_map<std::string, dse::gameplay3d::AnimState>::const_iterator sit2 = states.begin(); sit2 != states.end(); ++sit2) {
        bool is_default = (sit2->first == default_state);
        bool is_active = (sit2->first == animator.current_state_name);
        if (is_active) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1, 0.3f, 1));
        else if (is_default) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.8f, 0.2f, 1));
        const char* suffix = is_active ? " *" : (is_default ? " (default)" : "");
        ImGui::BulletText("%s%s", sit2->first.c_str(), suffix);
        if (is_active || is_default) ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // ── Canvas area ────────────────────────────────────────────────
    ImGui::BeginChild("##ASMCanvas", ImVec2(0, 0), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background grid
    float grid_step = 32.0f * es.zoom;
    ImU32 grid_color = IM_COL32(50, 50, 50, 120);
    float gx0 = fmodf(es.scroll_offset.x, grid_step);
    if (gx0 < 0) gx0 += grid_step;
    for (float x = gx0; x < canvas_size.x; x += grid_step) {
        dl->AddLine(ImVec2(canvas_pos.x + x, canvas_pos.y),
                    ImVec2(canvas_pos.x + x, canvas_pos.y + canvas_size.y), grid_color);
    }
    float gy0 = fmodf(es.scroll_offset.y, grid_step);
    if (gy0 < 0) gy0 += grid_step;
    for (float y = gy0; y < canvas_size.y; y += grid_step) {
        dl->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + y),
                    ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + y), grid_color);
    }

    // Canvas panning
    if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        es.scroll_offset.x += delta.x;
        es.scroll_offset.y += delta.y;
    }

    // Zoom
    if (ImGui::IsWindowHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0) {
            es.zoom = std::clamp(es.zoom + wheel * 0.1f, 0.3f, 3.0f);
        }
    }

    // ── Draw transitions (arrows) ──────────────────────────────────
    for (std::unordered_map<std::string, dse::gameplay3d::AnimState>::const_iterator ait = states.begin(); ait != states.end(); ++ait) {
        const std::string& sname = ait->first;
        const dse::gameplay3d::AnimState& astate = ait->second;
        if (es.node_positions.find(sname) == es.node_positions.end()) continue;
        NodeVisual& src_vis = es.node_positions[sname];
        ImVec2 src_center = ImVec2(
            canvas_pos.x + es.scroll_offset.x + src_vis.pos.x + src_vis.size.x * 0.5f,
            canvas_pos.y + es.scroll_offset.y + src_vis.pos.y + src_vis.size.y * 0.5f);

        for (int ti = 0; ti < static_cast<int>(astate.transitions.size()); ti++) {
            const dse::gameplay3d::AnimTransition& trans = astate.transitions[ti];
            if (es.node_positions.find(trans.target_state) == es.node_positions.end()) continue;
            NodeVisual& dst_vis = es.node_positions[trans.target_state];
            ImVec2 dst_center = ImVec2(
                canvas_pos.x + es.scroll_offset.x + dst_vis.pos.x + dst_vis.size.x * 0.5f,
                canvas_pos.y + es.scroll_offset.y + dst_vis.pos.y + dst_vis.size.y * 0.5f);

            bool is_sel = (es.selected_transition_src == sname && es.selected_transition_idx == ti);
            ImU32 arrow_color = is_sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(200, 200, 200, 180);
            DrawTransitionArrow(dl, src_center, dst_center, arrow_color, is_sel);
        }
    }

    // ── Draw link creation in progress ─────────────────────────────
    if (es.creating_link && !es.link_source.empty()) {
        std::unordered_map<std::string, NodeVisual>::iterator link_it = es.node_positions.find(es.link_source);
        if (link_it != es.node_positions.end()) {
            ImVec2 from = ImVec2(
                canvas_pos.x + es.scroll_offset.x + link_it->second.pos.x + link_it->second.size.x * 0.5f,
                canvas_pos.y + es.scroll_offset.y + link_it->second.pos.y + link_it->second.size.y * 0.5f);
            ImVec2 to = ImGui::GetIO().MousePos;
            dl->AddBezierCubic(from,
                ImVec2(from.x + 60, from.y), ImVec2(to.x - 60, to.y),
                to, IM_COL32(100, 255, 100, 200), 2.0f);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            es.creating_link = false;
        }
    }

    // ── Draw state nodes ───────────────────────────────────────────
    // Collect keys first to avoid iterator invalidation in mutation paths
    std::vector<std::string> state_keys;
    state_keys.reserve(states.size());
    for (std::unordered_map<std::string, dse::gameplay3d::AnimState>::const_iterator kit = states.begin(); kit != states.end(); ++kit) {
        state_keys.push_back(kit->first);
    }

    for (size_t ki = 0; ki < state_keys.size(); ki++) {
        const std::string& sname = state_keys[ki];
        std::unordered_map<std::string, dse::gameplay3d::AnimState>::const_iterator state_it = states.find(sname);
        if (state_it == states.end()) continue;
        const dse::gameplay3d::AnimState& cur_state = state_it->second;

        if (es.node_positions.find(sname) == es.node_positions.end()) {
            es.node_positions[sname] = {{80, 80}, {160, 60}};
        }
        NodeVisual& vis = es.node_positions[sname];
        ImVec2 node_min = ImVec2(canvas_pos.x + es.scroll_offset.x + vis.pos.x,
                                  canvas_pos.y + es.scroll_offset.y + vis.pos.y);
        ImVec2 node_max = ImVec2(node_min.x + vis.size.x, node_min.y + vis.size.y);

        bool is_default = (sname == default_state);
        bool is_selected = (es.selected_state == sname);
        bool is_blend_tree = cur_state.is_blend_tree;
        bool is_active = (sname == animator.current_state_name);

        ImU32 node_bg = is_active ? IM_COL32(30, 90, 30, 240)
                      : is_default ? IM_COL32(60, 80, 40, 230)
                      : is_blend_tree ? IM_COL32(60, 50, 80, 230)
                      : IM_COL32(50, 55, 65, 230);
        ImU32 node_border = is_selected ? IM_COL32(255, 200, 50, 255)
                          : is_active  ? IM_COL32(50, 255, 50, 255)
                          : IM_COL32(120, 120, 140, 255);

        dl->AddRectFilled(node_min, node_max, node_bg, 6.0f);
        dl->AddRect(node_min, node_max, node_border, 6.0f, 0, is_selected ? 2.5f : 1.0f);

        // State name
        ImVec2 text_pos = ImVec2(node_min.x + 8, node_min.y + 6);
        dl->AddText(text_pos, IM_COL32(255, 255, 255, 255), sname.c_str());

        // Subtitle: clip name or "Blend Tree"
        ImVec2 sub_pos = ImVec2(node_min.x + 8, node_min.y + 24);
        if (is_blend_tree) {
            dl->AddText(sub_pos, IM_COL32(180, 150, 255, 200), "Blend Tree");
        } else if (!cur_state.danim_path.empty()) {
            std::string clip_name = cur_state.danim_path;
            size_t slash = clip_name.rfind('/');
            if (slash != std::string::npos) clip_name = clip_name.substr(slash + 1);
            if (clip_name.size() > 20) clip_name = clip_name.substr(0, 17) + "...";
            dl->AddText(sub_pos, IM_COL32(150, 200, 150, 200), clip_name.c_str());
        }

        // Speed/loop info
        ImVec2 info_pos = ImVec2(node_min.x + 8, node_min.y + 40);
        char info[64];
        snprintf(info, sizeof(info), "x%.1f %s", cur_state.speed, cur_state.loop ? "Loop" : "Once");
        dl->AddText(info_pos, IM_COL32(150, 150, 150, 180), info);

        // Default badge
        if (is_default) {
            dl->AddText(ImVec2(node_max.x - 50, node_min.y + 6),
                IM_COL32(255, 220, 50, 255), "Entry");
        }
        // Active badge + progress
        if (is_active) {
            dl->AddText(ImVec2(node_max.x - 18, node_max.y - 14),
                IM_COL32(50, 255, 50, 255), "*");
            // Progress bar
            float progress = animator.normalized_time;
            float bar_y = node_max.y - 3.0f;
            dl->AddRectFilled(ImVec2(node_min.x, bar_y), ImVec2(node_max.x, bar_y + 3.0f),
                IM_COL32(30, 30, 30, 200));
            dl->AddRectFilled(ImVec2(node_min.x, bar_y),
                ImVec2(node_min.x + vis.size.x * progress, bar_y + 3.0f),
                IM_COL32(50, 255, 50, 220));
        }

        // Interaction
        std::string btn_id = "##node_" + sname;
        ImGui::SetCursorScreenPos(node_min);
        ImGui::InvisibleButton(btn_id.c_str(), vis.size);

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            es.selected_state = sname;
            es.selected_transition_src.clear();
            es.selected_transition_idx = -1;

            if (es.creating_link && es.link_source != sname) {
                std::unordered_map<std::string, dse::gameplay3d::AnimState>& ms = asm_ref.GetStatesMutable();
                std::unordered_map<std::string, dse::gameplay3d::AnimState>::iterator src_it = ms.find(es.link_source);
                if (src_it != ms.end()) {
                    dse::gameplay3d::AnimTransition new_trans;
                    new_trans.target_state = sname;
                    new_trans.has_exit_time = true;
                    new_trans.exit_time = 1.0f;
                    new_trans.transition_duration = 0.25f;
                    src_it->second.transitions.push_back(new_trans);
                }
                es.creating_link = false;
                es.link_source.clear();
            }
        }

        // Drag node
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !es.creating_link) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            vis.pos.x += delta.x;
            vis.pos.y += delta.y;
        }

        // Right-click context menu
        std::string ctx_id = "##NodeCtx_" + sname;
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            es.selected_state = sname;
            ImGui::OpenPopup(ctx_id.c_str());
        }
        if (ImGui::BeginPopup(ctx_id.c_str())) {
            if (ImGui::MenuItem("Set as Default")) {
                asm_ref.SetDefaultState(sname);
            }
            if (ImGui::MenuItem("Create Transition")) {
                es.creating_link = true;
                es.link_source = sname;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete State")) {
                std::unordered_map<std::string, dse::gameplay3d::AnimState>& del_states = asm_ref.GetStatesMutable();
                del_states.erase(sname);
                es.node_positions.erase(sname);
                if (es.selected_state == sname) es.selected_state.clear();
            }
            ImGui::EndPopup();
        }
    }

    // ── Right-click on canvas to add new state ─────────────────────
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("##CanvasCtx");
    }
    if (ImGui::BeginPopup("##CanvasCtx")) {
        if (ImGui::MenuItem("Add State")) {
            static int s_new_state_counter = 0;
            std::string new_name = "NewState_" + std::to_string(s_new_state_counter++);
            dse::gameplay3d::AnimState new_state;
            new_state.name = new_name;
            asm_ref.AddState(new_state);

            ImVec2 mouse = ImGui::GetIO().MousePos;
            es.node_positions[new_name] = {
                {mouse.x - canvas_pos.x - es.scroll_offset.x,
                 mouse.y - canvas_pos.y - es.scroll_offset.y},
                {160, 60}};
        }
        if (ImGui::MenuItem("Add Blend Tree State")) {
            static int s_bt_counter = 0;
            std::string new_name = "BlendTree_" + std::to_string(s_bt_counter++);
            dse::gameplay3d::AnimState new_state;
            new_state.name = new_name;
            new_state.is_blend_tree = true;
            asm_ref.AddState(new_state);

            ImVec2 mouse = ImGui::GetIO().MousePos;
            es.node_positions[new_name] = {
                {mouse.x - canvas_pos.x - es.scroll_offset.x,
                 mouse.y - canvas_pos.y - es.scroll_offset.y},
                {160, 60}};
        }
        if (ImGui::MenuItem("Auto Layout")) {
            AutoLayoutStates(states, default_state, es);
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();

    // ── Inspector side: selected state details ─────────────────────
    DrawSelectedStateInspector(asm_ref, es);

    ImGui::End();
}

} // namespace dse::editor
