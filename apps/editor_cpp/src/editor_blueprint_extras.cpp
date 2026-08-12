/**
 * @file editor_blueprint_extras.cpp
 * @brief Blueprint editor enhancements:
 *   #1 Debugger (execution visualization, breakpoints, watch)
 *   #2 Undo/Redo (snapshot-based command pattern)
 *   #3 Node Search (fuzzy flat-list popup)
 *   #4 Comments & Groups (annotation boxes, node grouping)
 *   #5 Asset Thumbnail (mini graph preview)
 *   #6 Templates (preset blueprints)
 */

#include "editor_blueprint.h"
#include "editor_blueprint_vm.h"
#include "editor_blueprint_compiler.h"
#include "editor_undo.h"
#include "editor_icons.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <deque>
#include <cstring>
#include <cmath>

namespace dse::editor::bp {

// ═══════════════════════════════════════════════════════════════════════════
// #1 — Blueprint Debugger
// ═══════════════════════════════════════════════════════════════════════════

bool BpDebugState::HasBreakpoint(int node_id) const {
    return std::find(breakpoint_nodes.begin(), breakpoint_nodes.end(), node_id) != breakpoint_nodes.end();
}

void BpDebugState::ToggleBreakpoint(int node_id) {
    auto it = std::find(breakpoint_nodes.begin(), breakpoint_nodes.end(), node_id);
    if (it != breakpoint_nodes.end()) breakpoint_nodes.erase(it);
    else breakpoint_nodes.push_back(node_id);
}

void BpDebugState::ClearBreakpoints() { breakpoint_nodes.clear(); }

void BpDebugState::Reset() {
    active = false;
    paused = false;
    current_node_id = -1;
    current_instruction = 0;
    watch_values.clear();
    execution_log.clear();
}

// ─── Real (non-simulated) step-debug session ────────────────────────────────
//
// Start compiles the blueprint to the same VM bytecode the runtime executes and
// pauses before the first instruction. Step/Continue drive the actual VM one
// instruction at a time via BlueprintVM::StepOnce; the highlighted node and
// watch values are read straight from live VM state, and breakpoints are honored
// through the compiler-emitted pc→node map.

namespace {

struct BpDebugSession {
    CompiledBlueprint compiled;
    BlueprintInstance instance;
    VmContext ctx;
    StepState step;
    bool valid = false;
};

BpDebugSession& DebugSession() {
    static BpDebugSession s;
    return s;
}

const CompiledFunction* PickEntryFunction(const CompiledBlueprint& bp) {
    for (const auto& f : bp.functions) if (f.name == "on_update") return &f;
    for (const auto& f : bp.functions) if (f.name == "on_init") return &f;
    return bp.functions.empty() ? nullptr : &bp.functions[0];
}

void PushLog(BpDebugState& dbg, std::string line) {
    dbg.execution_log.push_back(std::move(line));
    int overflow = static_cast<int>(dbg.execution_log.size()) - dbg.max_log_lines;
    if (overflow > 0)
        dbg.execution_log.erase(dbg.execution_log.begin(),
                                dbg.execution_log.begin() + overflow);
}

void RefreshWatch(BpDebugState& dbg, const BlueprintInstance& inst,
                  const BlueprintAsset& asset) {
    dbg.watch_values.clear();
    for (size_t i = 0; i < inst.variables.size() && i < asset.variables.size(); ++i) {
        const auto& v = inst.variables[i];
        std::string val;
        switch (v.type) {
            case BpValue::Type::Float:  val = std::to_string(v.f); break;
            case BpValue::Type::Int:    val = std::to_string(v.i); break;
            case BpValue::Type::Bool:   val = v.b ? "true" : "false"; break;
            case BpValue::Type::String: val = v.str; break;
            default:                    val = "<nil>"; break;
        }
        dbg.watch_values.emplace_back(asset.variables[i].name, val);
    }
}

void StartDebugSession() {
    auto& state = GetBlueprintEditorState();
    auto& dbg = state.debug;
    auto& S = DebugSession();
    S = BpDebugSession{};
    dbg.Reset();

    if (state.asset.graphs.empty()) {
        dbg.active = true;
        PushLog(dbg, "[Debug] No graphs to execute");
        return;
    }

    S.compiled = CompileToByteCode(state.asset);
    const CompiledFunction* entry = PickEntryFunction(S.compiled);
    if (!entry) {
        dbg.active = true;
        PushLog(dbg, "[Debug] Compilation produced no executable function");
        return;
    }

    S.instance.blueprint = &S.compiled;
    S.instance.variables = S.compiled.default_variables;
    S.instance.initialized = true;
    S.ctx.instance = &S.instance;
    S.ctx.entity_id = 0;
    S.ctx.delta_time = 0.016f;

    std::vector<BpValue> args;
    if (entry->name == "on_update") args.push_back(BpValue::Float(S.ctx.delta_time));
    BlueprintVM::Get().BeginStep(S.step, *entry, S.ctx, args);
    S.valid = true;

    dbg.active = true;
    dbg.paused = true;
    dbg.current_instruction = 0;
    dbg.current_node_id = BlueprintVM::Get().CurrentNode(S.step);
    PushLog(dbg, "[Debug] Session started (" + entry->name + "), paused at entry");
    RefreshWatch(dbg, S.instance, state.asset);
}

void StepDebugSession() {
    auto& state = GetBlueprintEditorState();
    auto& dbg = state.debug;
    auto& S = DebugSession();
    if (!dbg.active || !S.valid) return;
    dbg.paused = true;
    if (S.step.finished) { PushLog(dbg, "[Debug] Execution already finished"); return; }

    int node = BlueprintVM::Get().StepOnce(S.step, S.ctx);
    ++dbg.current_instruction;
    if (S.step.finished) {
        dbg.current_node_id = -1;
        PushLog(dbg, "[Debug] Execution finished after " +
                     std::to_string(dbg.current_instruction) + " instructions");
    } else {
        dbg.current_node_id = node;
        PushLog(dbg, "[Debug] Step -> pc=" + std::to_string(S.step.pc) +
                     " node=" + std::to_string(node));
    }
    RefreshWatch(dbg, S.instance, state.asset);
    if (BlueprintVM::Get().HasError())
        PushLog(dbg, "[Error] " + BlueprintVM::Get().GetLastError());
}

void ContinueDebugSession() {
    auto& state = GetBlueprintEditorState();
    auto& dbg = state.debug;
    auto& S = DebugSession();
    if (!dbg.active || !S.valid) return;
    if (S.step.finished) { PushLog(dbg, "[Debug] Execution already finished"); return; }

    dbg.paused = false;
    constexpr int kGuard = 200000;
    int guard = 0;
    bool hit_breakpoint = false;
    while (!S.step.finished && guard++ < kGuard) {
        int node = BlueprintVM::Get().StepOnce(S.step, S.ctx);
        ++dbg.current_instruction;
        if (S.step.finished) break;
        if (node >= 0 && dbg.HasBreakpoint(node)) {
            dbg.current_node_id = node;
            dbg.paused = true;
            hit_breakpoint = true;
            PushLog(dbg, "[Debug] Breakpoint hit at node " + std::to_string(node));
            break;
        }
    }
    if (S.step.finished) {
        dbg.current_node_id = -1;
        PushLog(dbg, "[Debug] Execution finished after " +
                     std::to_string(dbg.current_instruction) + " instructions");
    } else if (!hit_breakpoint) {
        PushLog(dbg, "[Debug] Paused (instruction budget reached)");
        dbg.paused = true;
    }
    RefreshWatch(dbg, S.instance, state.asset);
    if (BlueprintVM::Get().HasError())
        PushLog(dbg, "[Error] " + BlueprintVM::Get().GetLastError());
}

void StopDebugSession() {
    DebugSession() = BpDebugSession{};
    GetBlueprintEditorState().debug.Reset();
}

}  // namespace

void DrawBlueprintDebugPanel() {
    auto& state = GetBlueprintEditorState();
    auto& dbg = state.debug;

    ImGui::BeginChild("##bp_debug_panel", ImVec2(0, 180), true);
    ImGui::Text(MDI_ICON_BUG "  Blueprint Debugger");
    ImGui::Separator();

    // Control buttons
    if (!dbg.active) {
        if (ImGui::Button(MDI_ICON_PLAY " Start")) {
            StartDebugSession();
        }
    } else {
        if (ImGui::Button(MDI_ICON_STOP " Stop")) {
            StopDebugSession();
        }
        ImGui::SameLine();
        if (dbg.paused) {
            if (ImGui::Button(MDI_ICON_PLAY " Continue")) {
                ContinueDebugSession();
            }
            ImGui::SameLine();
            if (ImGui::Button(MDI_ICON_SKIP_NEXT " Step")) {
                StepDebugSession();
            }
        } else {
            if (ImGui::Button(MDI_ICON_PAUSE " Pause")) {
                dbg.paused = true;
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Breakpoints: %d", static_cast<int>(dbg.breakpoint_nodes.size()));
    }

    // Watch values
    if (!dbg.watch_values.empty()) {
        ImGui::Separator();
        ImGui::Text("Watch:");
        ImGui::Columns(2, "##bp_watch", true);
        ImGui::SetColumnWidth(0, 100);
        for (const auto& [name, val] : dbg.watch_values) {
            ImGui::TextUnformatted(name.c_str());
            ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", val.c_str());
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
    }

    // Execution log (scrolling)
    if (!dbg.execution_log.empty()) {
        ImGui::Separator();
        ImGui::BeginChild("##bp_exec_log", ImVec2(0, 60), false);
        for (const auto& line : dbg.execution_log) {
            if (line.find("[Error]") != std::string::npos)
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", line.c_str());
            else
                ImGui::TextUnformatted(line.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

    ImGui::EndChild();
}

void BpDebugStep() { StepDebugSession(); }

void BpDebugContinue() { ContinueDebugSession(); }

void BpDebugStop() { StopDebugSession(); }

// ═══════════════════════════════════════════════════════════════════════════
// #2 — Blueprint Undo/Redo (snapshot-based)
// ═══════════════════════════════════════════════════════════════════════════

namespace {

struct BpUndoSnapshot {
    BlueprintAsset asset;
    std::string description;
};

struct BpUndoSystem {
    std::deque<BpUndoSnapshot> undo_stack;
    std::deque<BpUndoSnapshot> redo_stack;
    static constexpr int MAX_HISTORY = 50;
};

BpUndoSystem& GetUndoSystem() {
    static BpUndoSystem s_undo;
    return s_undo;
}

} // anonymous

void BpPushUndoState(const char* description) {
    auto& sys = GetUndoSystem();
    auto& state = GetBlueprintEditorState();

    BpUndoSnapshot snap;
    snap.asset = state.asset;
    snap.description = description;
    sys.undo_stack.push_back(std::move(snap));
    sys.redo_stack.clear();

    while (static_cast<int>(sys.undo_stack.size()) > BpUndoSystem::MAX_HISTORY)
        sys.undo_stack.pop_front();
}

void BpUndo() {
    auto& sys = GetUndoSystem();
    if (sys.undo_stack.empty()) return;

    auto& state = GetBlueprintEditorState();

    // Save current state to redo
    BpUndoSnapshot redo_snap;
    redo_snap.asset = state.asset;
    redo_snap.description = "redo";
    sys.redo_stack.push_back(std::move(redo_snap));

    // Restore from undo stack
    state.asset = sys.undo_stack.back().asset;
    sys.undo_stack.pop_back();
    state.dirty = true;
}

void BpRedo() {
    auto& sys = GetUndoSystem();
    if (sys.redo_stack.empty()) return;

    auto& state = GetBlueprintEditorState();

    // Save current to undo
    BpUndoSnapshot undo_snap;
    undo_snap.asset = state.asset;
    undo_snap.description = "undo";
    sys.undo_stack.push_back(std::move(undo_snap));

    // Restore from redo stack
    state.asset = sys.redo_stack.back().asset;
    sys.redo_stack.pop_back();
    state.dirty = true;
}

bool BpCanUndo() { return !GetUndoSystem().undo_stack.empty(); }
bool BpCanRedo() { return !GetUndoSystem().redo_stack.empty(); }

// ═══════════════════════════════════════════════════════════════════════════
// #3 — Node Search (flat list with fuzzy matching)
// ═══════════════════════════════════════════════════════════════════════════

namespace {

bool FuzzyMatch(const std::string& text, const std::string& pattern) {
    if (pattern.empty()) return true;
    size_t pi = 0;
    for (size_t ti = 0; ti < text.size() && pi < pattern.size(); ++ti) {
        if (tolower(text[ti]) == tolower(pattern[pi])) ++pi;
    }
    return pi == pattern.size();
}

} // anonymous

void DrawNodeSearchPopup() {
    auto& state = GetBlueprintEditorState();

    if (state.search_focus_requested) {
        ImGui::SetKeyboardFocusHere();
        state.search_focus_requested = false;
    }

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##bp_search", "Search nodes...", state.node_search_buf, sizeof(state.node_search_buf));

    ImGui::Separator();

    const auto& reg = NodeRegistry::Get();
    std::string filter = state.node_search_buf;
    int shown = 0;

    ImGui::BeginChild("##bp_search_results", ImVec2(280, 300), false);

    if (filter.empty()) {
        // Show categories when no search
        for (const auto& cat : reg.Categories()) {
            if (ImGui::TreeNode(cat.c_str())) {
                for (const auto& tmpl : reg.All()) {
                    if (tmpl.category != cat) continue;
                    if (ImGui::Selectable(tmpl.name.c_str())) {
                        if (!state.asset.graphs.empty()) {
                            auto& graph = state.asset.graphs[state.active_graph_index];
                            BpPushUndoState("Create Node");
                            // Reuse the CreateNodeFromTemplate function via extern
                            BpNode node;
                            node.id = graph.next_id++;
                            node.name = tmpl.name;
                            node.category = tmpl.category;
                            node.position = state.create_menu_pos;
                            node.header_color = tmpl.header_color;
                            node.code_template = tmpl.code_template;
                            for (auto pin : tmpl.inputs) { pin.id = graph.next_id++; pin.kind = BpPinKind::Input; node.inputs.push_back(pin); }
                            for (auto pin : tmpl.outputs) { pin.id = graph.next_id++; pin.kind = BpPinKind::Output; node.outputs.push_back(pin); }
                            graph.nodes.push_back(std::move(node));
                            state.dirty = true;
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    if (!tmpl.description.empty() && ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", tmpl.description.c_str());
                }
                ImGui::TreePop();
            }
        }
    } else {
        // Flat filtered list with fuzzy match
        for (const auto& tmpl : reg.All()) {
            if (!FuzzyMatch(tmpl.name, filter) && !FuzzyMatch(tmpl.category, filter))
                continue;
            ++shown;

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::Text("[%s]", tmpl.category.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();

            if (ImGui::Selectable(tmpl.name.c_str())) {
                if (!state.asset.graphs.empty()) {
                    auto& graph = state.asset.graphs[state.active_graph_index];
                    BpPushUndoState("Create Node");
                    BpNode node;
                    node.id = graph.next_id++;
                    node.name = tmpl.name;
                    node.category = tmpl.category;
                    node.position = state.create_menu_pos;
                    node.header_color = tmpl.header_color;
                    node.code_template = tmpl.code_template;
                    for (auto pin : tmpl.inputs) { pin.id = graph.next_id++; pin.kind = BpPinKind::Input; node.inputs.push_back(pin); }
                    for (auto pin : tmpl.outputs) { pin.id = graph.next_id++; pin.kind = BpPinKind::Output; node.outputs.push_back(pin); }
                    graph.nodes.push_back(std::move(node));
                    state.dirty = true;
                }
                state.node_search_buf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
            if (!tmpl.description.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tmpl.description.c_str());
        }
        if (shown == 0) {
            ImGui::TextDisabled("No matching nodes");
        }
    }

    ImGui::EndChild();
}

// ═══════════════════════════════════════════════════════════════════════════
// #4 — Comment Boxes & Node Groups
// ═══════════════════════════════════════════════════════════════════════════

void DrawBpComments(ImDrawList* draw_list, ImVec2 canvas_pos) {
    auto& state = GetBlueprintEditorState();

    for (int i = 0; i < static_cast<int>(state.comments.size()); ++i) {
        auto& comment = state.comments[i];
        ImVec2 cpos(canvas_pos.x + state.scroll_offset.x + comment.position.x,
                    canvas_pos.y + state.scroll_offset.y + comment.position.y);
        ImVec2 cend(cpos.x + comment.size.x, cpos.y + comment.size.y);

        // Background
        draw_list->AddRectFilled(cpos, cend, comment.color, 6.0f);
        // Border
        bool selected = (state.selected_comment == i);
        draw_list->AddRect(cpos, cend,
            selected ? IM_COL32(255, 200, 50, 200) : IM_COL32(100, 100, 100, 150), 6.0f, 0, 2.0f);
        // Title bar
        draw_list->AddRectFilled(cpos, ImVec2(cend.x, cpos.y + 22), IM_COL32(50, 50, 50, 200), 6.0f, ImDrawFlags_RoundCornersTop);
        // Text
        draw_list->AddText(ImVec2(cpos.x + 6, cpos.y + 4), IM_COL32(220, 220, 220, 255), comment.text.c_str());
    }

    // Draw node groups (colored rectangles behind grouped nodes)
    for (const auto& group : state.groups) {
        if (group.node_ids.empty()) continue;

        // Find bounding box of all nodes in group
        float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
        bool found_any = false;
        if (!state.asset.graphs.empty()) {
            const auto& graph = state.asset.graphs[state.active_graph_index];
            for (const auto& node : graph.nodes) {
                if (std::find(group.node_ids.begin(), group.node_ids.end(), node.id) != group.node_ids.end()) {
                    min_x = std::min(min_x, node.position.x);
                    min_y = std::min(min_y, node.position.y);
                    max_x = std::max(max_x, node.position.x + node.size.x);
                    max_y = std::max(max_y, node.position.y + node.size.y);
                    found_any = true;
                }
            }
        }
        if (!found_any) continue;

        // Expand bounds by padding
        float pad = 20.0f;
        ImVec2 gpos(canvas_pos.x + state.scroll_offset.x + min_x - pad,
                    canvas_pos.y + state.scroll_offset.y + min_y - pad - 20);
        ImVec2 gend(canvas_pos.x + state.scroll_offset.x + max_x + pad,
                    canvas_pos.y + state.scroll_offset.y + max_y + pad);

        draw_list->AddRectFilled(gpos, gend, group.color, 8.0f);
        draw_list->AddRect(gpos, gend, IM_COL32(150, 200, 150, 180), 8.0f, 0, 1.5f);
        draw_list->AddText(ImVec2(gpos.x + 6, gpos.y + 4), IM_COL32(200, 255, 200, 255), group.name.c_str());
    }
}

void BpAddComment(ImVec2 pos) {
    auto& state = GetBlueprintEditorState();
    BpPushUndoState("Add Comment");
    BpComment comment;
    comment.id = static_cast<int>(state.comments.size()) + 1;
    comment.text = "Comment";
    comment.position = pos;
    state.comments.push_back(std::move(comment));
}

void BpAddNodeGroup(const std::string& name, const std::vector<int>& node_ids) {
    auto& state = GetBlueprintEditorState();
    BpPushUndoState("Add Node Group");
    BpNodeGroup group;
    group.id = static_cast<int>(state.groups.size()) + 1;
    group.name = name;
    group.node_ids = node_ids;
    state.groups.push_back(std::move(group));
}

// ═══════════════════════════════════════════════════════════════════════════
// #6 — Blueprint Templates
// ═══════════════════════════════════════════════════════════════════════════

BpTemplateRegistry& BpTemplateRegistry::Get() {
    static BpTemplateRegistry s_reg;
    return s_reg;
}

const BpTemplate* BpTemplateRegistry::Find(const std::string& name) const {
    for (const auto& t : templates_)
        if (t.name == name) return &t;
    return nullptr;
}

namespace {

BlueprintAsset MakeTemplateAsset(const char* name, const std::vector<std::string>& node_names) {
    BlueprintAsset asset;
    asset.name = name;
    asset.version = 1;

    BpFunctionGraph graph;
    graph.name = "EventGraph";
    graph.next_id = 1;

    float y_offset = 0;
    const auto& reg = NodeRegistry::Get();
    for (const auto& nn : node_names) {
        const NodeTemplate* tmpl = reg.Find(nn);
        if (!tmpl) continue;
        BpNode node;
        node.id = graph.next_id++;
        node.name = tmpl->name;
        node.category = tmpl->category;
        node.position = ImVec2(100 + y_offset * 0.5f, 50 + y_offset);
        node.header_color = tmpl->header_color;
        node.code_template = tmpl->code_template;
        for (auto pin : tmpl->inputs) { pin.id = graph.next_id++; pin.kind = BpPinKind::Input; node.inputs.push_back(pin); }
        for (auto pin : tmpl->outputs) { pin.id = graph.next_id++; pin.kind = BpPinKind::Output; node.outputs.push_back(pin); }
        graph.nodes.push_back(std::move(node));
        y_offset += 120;
    }

    asset.graphs.push_back(std::move(graph));
    return asset;
}

} // anonymous

void BpTemplateRegistry::RegisterDefaults() {
    if (!templates_.empty()) return;

    // Movement templates
    templates_.push_back({"FPS Controller", "Movement",
        "Basic first-person controller with WASD + mouse look",
        MakeTemplateAsset("FPS Controller", {"On Update", "Get Input Axis", "Get Position", "Vec3 Add", "Set Position"})});

    templates_.push_back({"Top-Down Movement", "Movement",
        "Top-down character movement with 8-directional input",
        MakeTemplateAsset("Top-Down Movement", {"On Update", "Get Input Axis", "Vec3 Scale", "Set Velocity"})});

    templates_.push_back({"Smooth Follow", "Movement",
        "Camera smooth follow with Lerp",
        MakeTemplateAsset("Smooth Follow", {"On Update", "Get Position", "Lerp", "Set Position"})});

    // AI templates
    templates_.push_back({"Patrol", "AI",
        "AI patrol between waypoints",
        MakeTemplateAsset("AI Patrol", {"On Update", "Get Position", "Distance", "Branch", "Move To"})});

    templates_.push_back({"Chase Player", "AI",
        "Chase player when within range",
        MakeTemplateAsset("Chase Player", {"On Update", "Distance", "Compare Float", "Branch", "Move To"})});

    // Interaction templates
    templates_.push_back({"Pickup Item", "Interaction",
        "Collectible item with overlap detection",
        MakeTemplateAsset("Pickup Item", {"On Begin Overlap", "Print", "Destroy Self"})});

    templates_.push_back({"Door Trigger", "Interaction",
        "Door that opens on overlap and closes after delay",
        MakeTemplateAsset("Door Trigger", {"On Begin Overlap", "Set Variable", "Delay", "Set Variable"})});

    templates_.push_back({"Health System", "Gameplay",
        "Damage/heal with clamped health variable",
        MakeTemplateAsset("Health System", {"On Init", "Set Variable", "Subtract", "Clamp", "Compare Float", "Branch"})});

    templates_.push_back({"Timer Loop", "Utility",
        "Repeating timer that fires an event every N seconds",
        MakeTemplateAsset("Timer Loop", {"On Update", "Add", "Compare Float", "Branch", "Set Variable", "Print"})});

    templates_.push_back({"Spawn System", "Gameplay",
        "Spawn entities at intervals with count limit",
        MakeTemplateAsset("Spawn System", {"On Update", "Compare Float", "Branch", "Spawn Actor", "Add"})});

    // Build categories
    for (const auto& t : templates_) {
        if (std::find(categories_.begin(), categories_.end(), t.category) == categories_.end())
            categories_.push_back(t.category);
    }
}

void DrawBpTemplatePanel() {
    auto& reg = BpTemplateRegistry::Get();
    reg.RegisterDefaults();

    ImGui::BeginChild("##bp_templates", ImVec2(250, 0), true);
    ImGui::Text(MDI_ICON_FILE_DOCUMENT_OUTLINE "  Templates");
    ImGui::Separator();

    for (const auto& cat : reg.Categories()) {
        if (ImGui::TreeNode(cat.c_str())) {
            for (const auto& tmpl : reg.All()) {
                if (tmpl.category != cat) continue;
                if (ImGui::Selectable(tmpl.name.c_str())) {
                    ApplyBpTemplate(tmpl.name);
                }
                if (ImGui::IsItemHovered() && !tmpl.description.empty()) {
                    ImGui::SetTooltip("%s", tmpl.description.c_str());
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::EndChild();
}

bool ApplyBpTemplate(const std::string& template_name) {
    auto& reg = BpTemplateRegistry::Get();
    reg.RegisterDefaults();
    const BpTemplate* tmpl = reg.Find(template_name);
    if (!tmpl) return false;

    auto& state = GetBlueprintEditorState();
    BpPushUndoState("Apply Template");

    // Copy template asset into current state
    state.asset = tmpl->asset;
    state.active_graph_index = 0;
    state.dirty = true;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Test Accessors
// ═══════════════════════════════════════════════════════════════════════════

int BpCommentCount() { return static_cast<int>(GetBlueprintEditorState().comments.size()); }
int BpGroupCount() { return static_cast<int>(GetBlueprintEditorState().groups.size()); }
int BpTemplateCount() { BpTemplateRegistry::Get().RegisterDefaults(); return static_cast<int>(BpTemplateRegistry::Get().All().size()); }
bool BpDebuggerActive() { return GetBlueprintEditorState().debug.active; }

}  // namespace dse::editor::bp
