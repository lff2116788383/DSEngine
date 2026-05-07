#include "editor_lua_console.h"

#include "imgui.h"
#include "editor_icons.h"
#include "engine/scripting/lua/lua_runtime.h"

#include <deque>
#include <string>
#include <vector>

namespace dse::editor {

namespace {

struct LuaConsoleEntry {
    std::string text;
    bool is_input = false;    // true = user input line, false = output/error
    bool is_error = false;
};

struct LuaConsoleState {
    char input_buf[1024] = {};
    std::deque<LuaConsoleEntry> history;
    std::vector<std::string> command_history;
    int history_index = -1;
    bool scroll_to_bottom = false;
    bool focus_input = true;
    static constexpr int kMaxHistory = 500;
};

LuaConsoleState& GetState() {
    static LuaConsoleState state;
    return state;
}

void ExecuteCommand(const std::string& cmd) {
    auto& state = GetState();

    // Record input
    state.history.push_back({"> " + cmd, true, false});
    state.command_history.push_back(cmd);
    state.history_index = -1;

    // Execute
    std::string result;
    bool success = dse::runtime::ExecuteLuaString(cmd.c_str(), &result);

    if (!result.empty()) {
        state.history.push_back({result, false, !success});
    } else if (success) {
        state.history.push_back({"(ok)", false, false});
    }

    // Trim history
    while (static_cast<int>(state.history.size()) > LuaConsoleState::kMaxHistory) {
        state.history.pop_front();
    }

    state.scroll_to_bottom = true;
}

int InputCallback(ImGuiInputTextCallbackData* data) {
    auto& state = GetState();
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        if (state.command_history.empty()) return 0;

        if (data->EventKey == ImGuiKey_UpArrow) {
            if (state.history_index < 0) {
                state.history_index = static_cast<int>(state.command_history.size()) - 1;
            } else if (state.history_index > 0) {
                state.history_index--;
            }
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (state.history_index >= 0) {
                state.history_index++;
                if (state.history_index >= static_cast<int>(state.command_history.size())) {
                    state.history_index = -1;
                }
            }
        }

        const char* history_str = (state.history_index >= 0)
            ? state.command_history[state.history_index].c_str()
            : "";
        data->DeleteChars(0, data->BufTextLen);
        data->InsertChars(0, history_str);
    }
    return 0;
}

} // namespace

void DrawLuaConsolePanel() {
    auto& state = GetState();

    ImGui::Begin(MDI_ICON_CODE "  Lua Console");

    // Toolbar
    if (ImGui::SmallButton("Clear")) {
        state.history.clear();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("| Lua Mem: %.1f KB", dse::runtime::GetLuaMemoryUsage() / 1024.0f);

    ImGui::Separator();

    // Output area
    float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("LuaOutput", ImVec2(0, -footer_height), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : state.history) {
        if (entry.is_input) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
        } else if (entry.is_error) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 0.8f, 1.0f));
        }
        ImGui::TextWrapped("%s", entry.text.c_str());
        ImGui::PopStyleColor();
    }

    if (state.scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        state.scroll_to_bottom = false;
    }

    ImGui::EndChild();

    // Input line
    ImGui::Separator();
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue
                                    | ImGuiInputTextFlags_CallbackHistory;

    if (state.focus_input) {
        ImGui::SetKeyboardFocusHere();
        state.focus_input = false;
    }

    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##lua_input", state.input_buf, sizeof(state.input_buf), input_flags, InputCallback)) {
        std::string cmd(state.input_buf);
        if (!cmd.empty()) {
            ExecuteCommand(cmd);
            state.input_buf[0] = '\0';
        }
        state.focus_input = true;
    }
    ImGui::PopItemWidth();

    // Keep focus on input after clicking elsewhere in the window
    if (ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
        state.focus_input = true;
    }

    ImGui::End();
}

} // namespace dse::editor
