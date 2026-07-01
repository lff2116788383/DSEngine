/**
 * @file editor_visual_script_debugger.cpp
 * @brief Visual Script runtime debugger — breakpoints, step execution, variable watch, call stack
 */

#include "editor_visual_script_debugger.h"
#include "editor_icons.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

namespace dse::editor {

namespace {

static VsDebugSession s_debug_session;

// Simulated local variables for demo/visualization
void UpdateSimulatedLocals() {
    auto& session = s_debug_session;
    if (session.state != VsDebugState::Paused) return;

    session.local_variables.clear();
    session.local_variables.push_back({"self", "Entity(42)", "Entity", false});
    session.local_variables.push_back({"dt", "0.01667", "Float", false});
    session.local_variables.push_back({"position", "vec3(1.0, 2.5, -3.0)", "Vec3", true});
    session.local_variables.push_back({"health", "85.0", "Float", false});
    session.local_variables.push_back({"is_grounded", "true", "Bool", false});
}

void UpdateSimulatedCallStack() {
    auto& session = s_debug_session;
    if (session.state != VsDebugState::Paused) return;

    session.call_stack.clear();
    session.call_stack.push_back({"on_update", session.current_node, session.current_line});
    session.call_stack.push_back({"move_character", -1, 12});
    session.call_stack.push_back({"check_collision", -1, 45});
}

} // anonymous namespace

VsDebugSession& GetVsDebugSession() {
    return s_debug_session;
}

void VsToggleBreakpoint(int node_id) {
    auto& bps = s_debug_session.breakpoints;
    auto it = std::find_if(bps.begin(), bps.end(),
        [node_id](const VsBreakpoint& bp) { return bp.node_id == node_id; });
    if (it != bps.end()) {
        bps.erase(it);
    } else {
        VsBreakpoint bp;
        bp.node_id = node_id;
        bp.enabled = true;
        bps.push_back(bp);
    }
}

bool VsHasBreakpoint(int node_id) {
    for (auto& bp : s_debug_session.breakpoints) {
        if (bp.node_id == node_id && bp.enabled) return true;
    }
    return false;
}

void VsDebugStart() {
    s_debug_session.state = VsDebugState::Running;
    s_debug_session.current_node = -1;
    s_debug_session.current_line = 0;
    s_debug_session.step_count = 0;
    s_debug_session.execution_time = 0.0f;
    s_debug_session.last_error.clear();
    s_debug_session.execution_trace.clear();
    s_debug_session.call_stack.clear();
    s_debug_session.local_variables.clear();
}

void VsDebugStop() {
    s_debug_session.state = VsDebugState::Idle;
    s_debug_session.current_node = -1;
    s_debug_session.current_line = -1;
}

void VsDebugPause() {
    if (s_debug_session.state == VsDebugState::Running) {
        s_debug_session.state = VsDebugState::Paused;
        UpdateSimulatedLocals();
        UpdateSimulatedCallStack();
    }
}

void VsDebugStepOver() {
    s_debug_session.state = VsDebugState::StepOver;
    s_debug_session.step_count++;
    // Simulate advancing to next node
    s_debug_session.current_line++;
    s_debug_session.state = VsDebugState::Paused;
    UpdateSimulatedLocals();
    UpdateSimulatedCallStack();
}

void VsDebugStepInto() {
    s_debug_session.state = VsDebugState::StepInto;
    s_debug_session.step_count++;
    s_debug_session.current_line++;
    s_debug_session.state = VsDebugState::Paused;
    UpdateSimulatedLocals();
    UpdateSimulatedCallStack();
}

void VsDebugContinue() {
    if (s_debug_session.state == VsDebugState::Paused) {
        s_debug_session.state = VsDebugState::Running;
    }
}

void VsDebugAddWatch(const std::string& expr) {
    VsWatchVariable w;
    w.name = expr;
    w.value = "<not evaluated>";
    w.type = "Unknown";
    w.changed = false;
    s_debug_session.watch_variables.push_back(w);
}

void DrawVisualScriptDebugger(EditorContext& /*ctx*/) {
    auto& session = s_debug_session;

    ImGui::Begin(MDI_ICON_BUG "  Visual Script Debugger");

    // ─── Toolbar ─────────────────────────────────────────────────────────
    {
        bool is_idle = (session.state == VsDebugState::Idle);
        bool is_running = (session.state == VsDebugState::Running);
        bool is_paused = (session.state == VsDebugState::Paused);

        if (is_idle) {
            if (ImGui::Button(MDI_ICON_PLAY " Start")) VsDebugStart();
        } else {
            if (ImGui::Button(MDI_ICON_STOP " Stop")) VsDebugStop();
        }
        ImGui::SameLine();

        ImGui::BeginDisabled(!is_running);
        if (ImGui::Button(MDI_ICON_PAUSE " Pause")) VsDebugPause();
        ImGui::EndDisabled();
        ImGui::SameLine();

        ImGui::BeginDisabled(!is_paused);
        if (ImGui::Button(MDI_ICON_PLAY " Continue")) VsDebugContinue();
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_DEBUG_STEP_OVER " Step Over")) VsDebugStepOver();
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_DEBUG_STEP_INTO " Step Into")) VsDebugStepInto();
        ImGui::EndDisabled();

        // Status
        ImGui::SameLine();
        ImGui::Spacing(); ImGui::SameLine();
        const char* state_labels[] = {"Idle", "Running", "Paused", "Step Over", "Step Into"};
        ImVec4 state_colors[] = {
            {0.5f, 0.5f, 0.5f, 1.0f},
            {0.2f, 0.8f, 0.2f, 1.0f},
            {1.0f, 0.8f, 0.2f, 1.0f},
            {0.4f, 0.6f, 1.0f, 1.0f},
            {0.4f, 0.6f, 1.0f, 1.0f}
        };
        int si = static_cast<int>(session.state);
        ImGui::TextColored(state_colors[si], "%s", state_labels[si]);

        if (session.step_count > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("Steps: %d", session.step_count);
        }
    }

    if (!session.last_error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 80, 80, 255));
        ImGui::TextWrapped("Error: %s", session.last_error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // ─── Tabs: Breakpoints / Call Stack / Watch / Locals ─────────────────
    if (ImGui::BeginTabBar("DebugTabs")) {
        // ─── Breakpoints Tab ─────────────────────────────────────────────
        if (ImGui::BeginTabItem(MDI_ICON_CIRCLE_SLICE_8 " Breakpoints")) {
            if (ImGui::Button("Clear All")) {
                session.breakpoints.clear();
            }
            ImGui::SameLine();
            if (ImGui::Button("Disable All")) {
                for (auto& bp : session.breakpoints) bp.enabled = false;
            }

            ImGui::Separator();

            if (session.breakpoints.empty()) {
                ImGui::TextDisabled("No breakpoints set. Right-click a node to toggle breakpoint.");
            } else {
                for (int i = 0; i < static_cast<int>(session.breakpoints.size()); i++) {
                    auto& bp = session.breakpoints[i];
                    ImGui::PushID(i);
                    ImGui::Checkbox("##en", &bp.enabled);
                    ImGui::SameLine();
                    ImGui::Text("Node #%d", bp.node_id);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(hits: %d)", bp.hit_count);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) {
                        session.breakpoints.erase(session.breakpoints.begin() + i);
                        i--;
                    }

                    // Conditional breakpoint
                    if (bp.enabled && !bp.condition.empty()) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("if: %s", bp.condition.c_str());
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTabItem();
        }

        // ─── Call Stack Tab ──────────────────────────────────────────────
        if (ImGui::BeginTabItem(MDI_ICON_FORMAT_LIST_NUMBERED " Call Stack")) {
            if (session.call_stack.empty()) {
                ImGui::TextDisabled("Not paused or no call stack available.");
            } else {
                for (int i = 0; i < static_cast<int>(session.call_stack.size()); i++) {
                    auto& frame = session.call_stack[i];
                    bool is_current = (i == 0);
                    if (is_current) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 100, 255));
                    ImGui::Text("#%d  %s  (node %d, line %d)",
                        i, frame.function_name.c_str(), frame.node_id, frame.line_number);
                    if (is_current) ImGui::PopStyleColor();
                }
            }
            ImGui::EndTabItem();
        }

        // ─── Watch Tab ───────────────────────────────────────────────────
        if (ImGui::BeginTabItem(MDI_ICON_EYE " Watch")) {
            static char watch_buf[128] = "";
            ImGui::SetNextItemWidth(200);
            if (ImGui::InputText("##watch_expr", watch_buf, sizeof(watch_buf),
                                  ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (watch_buf[0]) {
                    VsDebugAddWatch(watch_buf);
                    watch_buf[0] = '\0';
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Add")) {
                if (watch_buf[0]) {
                    VsDebugAddWatch(watch_buf);
                    watch_buf[0] = '\0';
                }
            }

            ImGui::Separator();

            if (session.watch_variables.empty()) {
                ImGui::TextDisabled("Add expressions to watch.");
            } else {
                for (int i = 0; i < static_cast<int>(session.watch_variables.size()); i++) {
                    auto& w = session.watch_variables[i];
                    ImGui::PushID(i);
                    if (w.changed) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 80, 255));
                    ImGui::Text("%s", w.name.c_str());
                    ImGui::SameLine(150);
                    ImGui::Text("= %s", w.value.c_str());
                    ImGui::SameLine(350);
                    ImGui::TextDisabled("[%s]", w.type.c_str());
                    if (w.changed) ImGui::PopStyleColor();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) {
                        session.watch_variables.erase(session.watch_variables.begin() + i);
                        i--;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTabItem();
        }

        // ─── Locals Tab ──────────────────────────────────────────────────
        if (ImGui::BeginTabItem(MDI_ICON_VARIABLE " Locals")) {
            if (session.local_variables.empty()) {
                ImGui::TextDisabled("Pause execution to inspect local variables.");
            } else {
                ImGui::Columns(3, "locals_cols");
                ImGui::SetColumnWidth(0, 120);
                ImGui::SetColumnWidth(1, 200);
                ImGui::Text("Name"); ImGui::NextColumn();
                ImGui::Text("Value"); ImGui::NextColumn();
                ImGui::Text("Type"); ImGui::NextColumn();
                ImGui::Separator();

                for (auto& lv : session.local_variables) {
                    if (lv.changed) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 80, 255));
                    ImGui::Text("%s", lv.name.c_str()); ImGui::NextColumn();
                    ImGui::Text("%s", lv.value.c_str()); ImGui::NextColumn();
                    ImGui::TextDisabled("%s", lv.type.c_str()); ImGui::NextColumn();
                    if (lv.changed) ImGui::PopStyleColor();
                }
                ImGui::Columns(1);
            }
            ImGui::EndTabItem();
        }

        // ─── Execution Trace Tab ─────────────────────────────────────────
        if (ImGui::BeginTabItem(MDI_ICON_TRANSIT_CONNECTION_VARIANT " Trace")) {
            if (session.execution_trace.empty()) {
                ImGui::TextDisabled("No execution trace available.");
            } else {
                for (int i = static_cast<int>(session.execution_trace.size()) - 1; i >= 0; i--) {
                    ImGui::Text("%d. Node #%d", static_cast<int>(session.execution_trace.size()) - i, session.execution_trace[i]);
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ─── Test accessor ──────────────────────────────────────────────────────
static VsDebuggerTestState s_test_state;

VsDebuggerTestState& GetVsDebuggerState() {
    s_test_state.debug_state = s_debug_session.state;
    s_test_state.breakpoints = s_debug_session.breakpoints;
    return s_test_state;
}

} // namespace dse::editor
