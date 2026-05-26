#include "editor_lua_debugger.h"
#include "editor_context.h"
#include "editor_icons.h"
#include "editor_console_panel.h"

#include "engine/scripting/lua/lua_debugger.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <string>
#include <vector>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace dse::editor {

namespace {

// Source file cache for displaying code
struct SourceCache {
    std::unordered_map<std::string, std::vector<std::string>> files;

    const std::vector<std::string>& GetLines(const std::string& path) {
        auto it = files.find(path);
        if (it != files.end()) return it->second;
        std::vector<std::string>& lines = files[path];
        std::ifstream f(path);
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                lines.push_back(line);
            }
        }
        return lines;
    }

    void Invalidate() { files.clear(); }
};

SourceCache& GetSourceCache() {
    static SourceCache cache;
    return cache;
}

// Watch expression list
struct WatchState {
    char new_expr_buf[256] = "";
    std::vector<std::string> expressions;
};

WatchState& GetWatchState() {
    static WatchState state;
    return state;
}

// Breakpoint add dialog
struct BreakpointAddState {
    char source_buf[256] = "";
    int line_num = 1;
};

BreakpointAddState& GetBPAddState() {
    static BreakpointAddState state;
    return state;
}

} // namespace

void DrawLuaDebuggerPanel(EditorContext& ctx) {
    (void)ctx;
    auto& dbg = dse::scripting::LuaDebugger::Instance();

    ImGui::Begin("Lua Debugger");

    // ── Toolbar ──────────────────────────────────────────────────────────────
    {
        bool attached = dbg.IsAttached();
        bool enabled = dbg.IsEnabled();
        bool paused = dbg.IsPaused();

        // Enable/Disable toggle
        if (ImGui::Checkbox("Enable", &enabled)) {
            dbg.SetEnabled(enabled);
        }
        ImGui::SameLine();

        if (!attached) {
            ImGui::TextDisabled("Lua VM not running");
            ImGui::End();
            return;
        }

        // Execution controls
        ImGui::BeginDisabled(!paused);
        if (ImGui::Button(MDI_ICON_PLAY " Resume")) { dbg.Resume(); }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_SKIP_NEXT " Step Over")) { dbg.StepOver(); }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_ARROW_DOWN " Step Into")) { dbg.StepInto(); }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_ARROW_UP " Step Out")) { dbg.StepOut(); }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (paused) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.2f, 1.0f),
                "PAUSED at %s:%d", dbg.GetPausedSource().c_str(), dbg.GetPausedLine());
        } else if (enabled) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Running");
        }
    }

    ImGui::Separator();

    // ── Tab bar: Breakpoints | Call Stack | Locals | Watch | Source ───────────
    if (ImGui::BeginTabBar("##dbg_tabs")) {

        // ── Breakpoints Tab ──────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Breakpoints")) {
            auto bps = dbg.GetBreakpoints();
            for (size_t i = 0; i < bps.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::Text("%s : %d", bps[i].source.c_str(), bps[i].line);
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    dbg.RemoveBreakpoint(bps[i].source, bps[i].line);
                }
                ImGui::PopID();
            }
            ImGui::Separator();
            auto& bp_state = GetBPAddState();
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("Source", bp_state.source_buf, sizeof(bp_state.source_buf));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("Line", &bp_state.line_num);
            ImGui::SameLine();
            if (ImGui::Button("Add##bp")) {
                if (bp_state.source_buf[0] != '\0' && bp_state.line_num > 0) {
                    dbg.AddBreakpoint(bp_state.source_buf, bp_state.line_num);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear All")) {
                dbg.ClearAllBreakpoints();
            }
            ImGui::EndTabItem();
        }

        // ── Call Stack Tab ───────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Call Stack")) {
            if (dbg.IsPaused()) {
                auto stack = dbg.GetCallStack();
                for (size_t i = 0; i < stack.size(); ++i) {
                    const auto& frame = stack[i];
                    bool is_top = (i == 0);
                    if (is_top) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.4f, 1.0f));
                    ImGui::Text("#%d  %s  %s:%d",
                        static_cast<int>(i),
                        frame.function_name.c_str(),
                        frame.source.c_str(),
                        frame.line);
                    if (is_top) ImGui::PopStyleColor();
                }
            } else {
                ImGui::TextDisabled("Not paused");
            }
            ImGui::EndTabItem();
        }

        // ── Locals Tab ───────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Locals")) {
            if (dbg.IsPaused()) {
                auto locals = dbg.GetLocals();
                if (ImGui::BeginTable("##locals_tbl", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 120);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80);
                    ImGui::TableHeadersRow();
                    for (const auto& var : locals) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(var.name.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(var.value.c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextDisabled("%s", var.type.c_str());
                    }
                    ImGui::EndTable();
                }
            } else {
                ImGui::TextDisabled("Not paused");
            }
            ImGui::EndTabItem();
        }

        // ── Watch Tab ────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Watch")) {
            auto& ws = GetWatchState();
            if (dbg.IsPaused()) {
                for (size_t i = 0; i < ws.expressions.size(); ++i) {
                    auto results = dbg.EvaluateExpression(ws.expressions[i]);
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::Text("%s", ws.expressions[i].c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("=");
                    ImGui::SameLine();
                    if (!results.empty()) {
                        if (results[0].type == "error") {
                            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", results[0].value.c_str());
                        } else {
                            ImGui::Text("%s", results[0].value.c_str());
                        }
                    } else {
                        ImGui::TextDisabled("nil");
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) {
                        ws.expressions.erase(ws.expressions.begin() + static_cast<ptrdiff_t>(i));
                        --i;
                    }
                    ImGui::PopID();
                }
            } else {
                for (const auto& expr : ws.expressions) {
                    ImGui::Text("%s = ", expr.c_str());
                    ImGui::SameLine();
                    ImGui::TextDisabled("(not paused)");
                }
            }
            ImGui::Separator();
            ImGui::SetNextItemWidth(200);
            if (ImGui::InputText("##watch_expr", ws.new_expr_buf, sizeof(ws.new_expr_buf),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (ws.new_expr_buf[0] != '\0') {
                    ws.expressions.emplace_back(ws.new_expr_buf);
                    ws.new_expr_buf[0] = '\0';
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Watch")) {
                if (ws.new_expr_buf[0] != '\0') {
                    ws.expressions.emplace_back(ws.new_expr_buf);
                    ws.new_expr_buf[0] = '\0';
                }
            }
            ImGui::EndTabItem();
        }

        // ── Source Tab ───────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Source")) {
            if (dbg.IsPaused()) {
                std::string src = dbg.GetPausedSource();
                int line = dbg.GetPausedLine();
                auto& lines = GetSourceCache().GetLines(src);
                if (lines.empty()) {
                    ImGui::TextDisabled("Cannot load source: %s", src.c_str());
                } else {
                    ImGui::TextDisabled("%s", src.c_str());
                    ImGui::Separator();
                    // Show surrounding lines
                    int start = (line > 10) ? line - 10 : 1;
                    int end = (line + 20 <= static_cast<int>(lines.size())) ? line + 20 : static_cast<int>(lines.size());
                    for (int l = start; l <= end; ++l) {
                        bool is_current = (l == line);
                        bool has_bp = dbg.HasBreakpoint(src, l);
                        if (has_bp) {
                            ImGui::TextColored(ImVec4(1, 0, 0, 1), MDI_ICON_STOP);
                        } else {
                            ImGui::TextDisabled("  ");
                        }
                        ImGui::SameLine();
                        if (is_current) {
                            ImGui::TextColored(ImVec4(1, 1, 0.3f, 1), "%4d > %s", l, lines[static_cast<size_t>(l - 1)].c_str());
                        } else {
                            ImGui::Text("%4d   %s", l, lines[static_cast<size_t>(l - 1)].c_str());
                        }
                        // Click to toggle breakpoint
                        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
                            if (has_bp) {
                                dbg.RemoveBreakpoint(src, l);
                            } else {
                                dbg.AddBreakpoint(src, l);
                            }
                        }
                    }
                }
            } else {
                ImGui::TextDisabled("Not paused — source view available when execution is paused");
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace dse::editor
