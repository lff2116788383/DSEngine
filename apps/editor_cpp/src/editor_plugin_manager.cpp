#include "editor_plugin_manager.h"
#include "editor_console_panel.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <rapidjson/document.h>
#include <imgui.h>

#include "engine/platform/process.h"

namespace dse::editor {

// ─── PluginManager ──────────────────────────────────────────────────────────

PluginManager::~PluginManager() {
    StopAll();
}

void PluginManager::ScanPlugins(const std::filesystem::path& plugins_dir) {
    plugins_dir_ = plugins_dir;
    plugins_.clear();

    if (!std::filesystem::exists(plugins_dir)) {
        std::filesystem::create_directories(plugins_dir);
        return;
    }

    for (auto& entry : std::filesystem::directory_iterator(plugins_dir)) {
        if (!entry.is_directory()) continue;

        auto json_path = entry.path() / "plugin.json";
        if (!std::filesystem::exists(json_path)) continue;

        PluginInstance inst;
        inst.directory = entry.path();
        if (ParsePluginJson(json_path, inst.metadata)) {
            plugins_.push_back(std::move(inst));
        }
    }

    std::cerr << "[PluginManager] Found " << plugins_.size() << " plugin(s) in "
              << plugins_dir.string() << std::endl;
}

bool PluginManager::ParsePluginJson(const std::filesystem::path& json_path, PluginMetadata& out) {
    std::ifstream ifs(json_path);
    if (!ifs.is_open()) return false;

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError() || !doc.IsObject()) return false;

    if (doc.HasMember("name") && doc["name"].IsString())
        out.name = doc["name"].GetString();
    else
        out.name = json_path.parent_path().filename().string();

    if (doc.HasMember("version") && doc["version"].IsString())
        out.version = doc["version"].GetString();
    if (doc.HasMember("author") && doc["author"].IsString())
        out.author = doc["author"].GetString();
    if (doc.HasMember("description") && doc["description"].IsString())
        out.description = doc["description"].GetString();
    if (doc.HasMember("runtime") && doc["runtime"].IsString())
        out.runtime = doc["runtime"].GetString();
    if (doc.HasMember("entry") && doc["entry"].IsString())
        out.entry = doc["entry"].GetString();
    if (doc.HasMember("requires_ui") && doc["requires_ui"].IsBool())
        out.requires_ui = doc["requires_ui"].GetBool();
    if (doc.HasMember("ui_port") && doc["ui_port"].IsInt())
        out.ui_port = doc["ui_port"].GetInt();

    return true;
}

std::string PluginManager::ResolveRuntime(const std::string& runtime) {
    if (runtime == "python") return "python";
    if (runtime == "node") return "node";
    if (runtime == "executable") return "";
    return runtime;  // 直接用 runtime 字段作为命令
}

bool PluginManager::StartPlugin(size_t index) {
    if (index >= plugins_.size()) return false;
    auto& plugin = plugins_[index];

    if (plugin.state == PluginState::Running) return true;

    std::string runtime_cmd = ResolveRuntime(plugin.metadata.runtime);
    std::filesystem::path entry_path = plugin.directory / plugin.metadata.entry;

    if (!std::filesystem::exists(entry_path)) {
        plugin.last_error = "Entry file not found: " + entry_path.string();
        plugin.state = PluginState::Error;
        return false;
    }

    // Launch via the shared managed-process service (explicit arg array, no shell
    // string concatenation). The handle is kept for later PollStatus()/StopPlugin().
    platform::ManagedProcessOptions opts;
    if (runtime_cmd.empty()) {
        opts.process.executable = entry_path.string();
    } else {
        opts.process.executable = runtime_cmd;
        opts.process.args = {entry_path.string()};
    }
    opts.process.working_dir = plugin.directory;
    opts.no_window = true;

    std::string err;
    if (!plugin.process.Start(opts, &err)) {
        plugin.last_error = err;
        plugin.state = PluginState::Error;
        return false;
    }
    plugin.process_id = plugin.process.Pid();

    plugin.state = PluginState::Running;
    plugin.enabled = true;
    plugin.last_error.clear();

    EditorLog(LogLevel::Info, "[Plugin] Started: " + plugin.metadata.name + " (PID " +
              std::to_string(plugin.process_id) + ")");
    return true;
}

void PluginManager::StopPlugin(size_t index) {
    if (index >= plugins_.size()) return;
    auto& plugin = plugins_[index];

    if (plugin.state != PluginState::Running) {
        plugin.state = PluginState::Stopped;
        plugin.enabled = false;
        return;
    }

    plugin.process.Kill();

    plugin.state = PluginState::Stopped;
    plugin.enabled = false;
    plugin.process_id = 0;

    EditorLog(LogLevel::Info, "[Plugin] Stopped: " + plugin.metadata.name);
}

void PluginManager::StopAll() {
    for (size_t i = 0; i < plugins_.size(); ++i) {
        if (plugins_[i].state == PluginState::Running) {
            StopPlugin(i);
        }
    }
}

void PluginManager::PollStatus() {
    for (auto& plugin : plugins_) {
        if (plugin.state != PluginState::Running) continue;

        int exit_code = 0;
        if (plugin.process.TryGetExitCode(exit_code)) {
            plugin.state = PluginState::Stopped;
            plugin.enabled = false;
            plugin.process_id = 0;
            if (exit_code != 0) {
                plugin.state = PluginState::Error;
                plugin.last_error = "Exited with code " + std::to_string(exit_code);
            }
        }
    }
}

// ─── ImGui Panel ────────────────────────────────────────────────────────────

void DrawPluginManagerPanel(PluginManager& manager) {
    ImGui::Text("Plugins (%d)", static_cast<int>(manager.GetPluginCount()));
    ImGui::Separator();

    if (manager.GetPluginCount() == 0) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            "No plugins found.\nCreate a plugin in the 'plugins/' directory\nwith a plugin.json file.");
        return;
    }

    for (size_t i = 0; i < manager.GetPluginCount(); ++i) {
        auto& plugin = manager.GetPlugins()[i];

        ImGui::PushID(static_cast<int>(i));

        // 状态指示灯
        ImVec4 color;
        const char* state_text;
        switch (plugin.state) {
            case PluginState::Running:
                color = ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
                state_text = "Running";
                break;
            case PluginState::Error:
                color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
                state_text = "Error";
                break;
            default:
                color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                state_text = "Stopped";
                break;
        }

        ImGui::ColorButton("##status", color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(8, 8));
        ImGui::SameLine();

        bool tree_open = ImGui::TreeNode("##tree", "%s", plugin.metadata.name.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);

        if (plugin.state == PluginState::Running) {
            if (ImGui::SmallButton("Stop")) {
                manager.StopPlugin(i);
            }
        } else {
            if (ImGui::SmallButton("Start")) {
                manager.StartPlugin(i);
            }
        }

        if (tree_open) {
            ImGui::Text("Version: %s", plugin.metadata.version.c_str());
            ImGui::Text("Author: %s", plugin.metadata.author.c_str());
            ImGui::Text("Runtime: %s", plugin.metadata.runtime.c_str());
            ImGui::Text("Entry: %s", plugin.metadata.entry.c_str());
            ImGui::Text("State: %s", state_text);
            if (plugin.state == PluginState::Running) {
                ImGui::Text("PID: %u", static_cast<unsigned>(plugin.process_id));
            }
            if (!plugin.last_error.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error: %s", plugin.last_error.c_str());
            }
            if (!plugin.metadata.description.empty()) {
                ImGui::TextWrapped("%s", plugin.metadata.description.c_str());
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
        ImGui::Separator();
    }
}

} // namespace dse::editor
