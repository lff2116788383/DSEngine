/**
 * @file editor_csharp_panel.cpp
 * @brief 编辑器 C# 脚本支持：Inspector 组件面板 + 独立管理面板（构建/热重载/状态）
 */

#include "editor_csharp_panel.h"
#include "editor_inspector_registry.h"
#include "editor_icons.h"
#include "editor_task_service.h"
#include "editor_console_panel.h"

#include "engine/ecs/script.h"
#include "engine/platform/process.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "editor_panel_registry.h"

namespace dse::editor {

namespace {

// 缓存用于 InputText 的静态 buffer
static char s_class_name_buf[256] = "";
static entt::entity s_last_csharp_entity = entt::null;

// C# 项目构建状态
enum class CSharpBuildStatus {
    Idle,
    Building,
    Success,
    Failed
};

static CSharpBuildStatus s_build_status = CSharpBuildStatus::Idle;
static std::string s_build_output;
static bool s_csharp_host_active = false;

// 从编辑器工作目录向上查找仓库内的 GameScripts/DSEngine.sln。
std::filesystem::path FindGameScriptsSolution() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::current_path(ec);
    for (int i = 0; i < 8 && !dir.empty(); ++i) {
        fs::path candidate = dir / "GameScripts" / "DSEngine.sln";
        if (fs::exists(candidate, ec)) return candidate;
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    return {};
}

// 通过统一进程服务 + 后台任务服务真实执行 `dotnet build`，退出码决定成功/失败。
// 实时输出进入 Background Tasks 面板；完成回调在 UI 线程更新状态与摘要。
void StartCSharpBuild(const std::string& config, const std::string& label,
                      const std::string& success_msg) {
    std::filesystem::path sln = FindGameScriptsSolution();
    if (sln.empty()) {
        s_build_status = CSharpBuildStatus::Failed;
        s_build_output = "Cannot locate GameScripts/DSEngine.sln under the working directory";
        EditorLog(LogLevel::Error, "[CSharp] " + s_build_output);
        return;
    }
    s_build_status = CSharpBuildStatus::Building;
    s_build_output = label + " running... (see Background Tasks)";

    std::string sln_str = sln.string();
    std::string cfg = config;
    auto exit_code = std::make_shared<std::atomic<int>>(-1);
    auto launched = std::make_shared<std::atomic<bool>>(false);
    auto launch_err = std::make_shared<std::string>();

    BackgroundTaskService::Get().Submit(
        label,
        [sln_str, cfg, exit_code, launched, launch_err](TaskContext& ctx) -> bool {
            ctx.SetProgress(-1.0f, "dotnet build");
            dse::platform::ProcessOptions opts;
            opts.executable = "dotnet";
            opts.args = {"build", sln_str, "-c", cfg, "--nologo"};
            opts.merge_stderr = true;
            const std::atomic<bool>& cancel = ctx.CancelFlag();
            dse::platform::ProcessResult r = dse::platform::RunProcess(
                opts,
                [&ctx](std::string_view line, bool is_err) {
                    if (!line.empty()) ctx.Log(std::string(line), is_err);
                },
                std::chrono::milliseconds::zero(),
                &cancel);
            launched->store(r.launched);
            exit_code->store(r.exit_code);
            if (!r.launched) {
                *launch_err = r.error.empty() ? "dotnet not found on PATH" : r.error;
                ctx.Fail(*launch_err);
                return false;
            }
            if (r.canceled) return false;
            if (r.exit_code != 0) {
                ctx.Fail("dotnet build exited with code " + std::to_string(r.exit_code));
                return false;
            }
            return true;
        },
        [label, success_msg, exit_code, launched, launch_err](bool success) {
            s_build_status = success ? CSharpBuildStatus::Success : CSharpBuildStatus::Failed;
            if (success) {
                s_build_output = success_msg;
                EditorLog(LogLevel::Info, "[CSharp] " + label + ": " + success_msg);
            } else if (!launched->load()) {
                s_build_output = label + " failed: " + *launch_err;
                EditorLog(LogLevel::Error, "[CSharp] " + s_build_output);
            } else {
                s_build_output = label + " failed (dotnet exit code " +
                                 std::to_string(exit_code->load()) + "); see Background Tasks log";
                EditorLog(LogLevel::Error, "[CSharp] " + s_build_output);
            }
        });
}

} // namespace

// ─── Inspector Section: C# Script Component ─────────────────────────────────

void DrawCSharpScriptSection(EditorContext& ctx) {
    if (!ctx.registry.all_of<CSharpScriptComponent>(ctx.selected_entity)) return;
    auto& csharp = ctx.registry.get<CSharpScriptComponent>(ctx.selected_entity);

    if (!ImGui::CollapsingHeader(MDI_ICON_CODE "  C# Script", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Columns(2, "csharp_cols", false);
    ImGui::SetColumnWidth(0, 110.0f);

    // 同步 buffer
    if (s_last_csharp_entity != ctx.selected_entity) {
        s_last_csharp_entity = ctx.selected_entity;
        std::strncpy(s_class_name_buf, csharp.class_name.c_str(), sizeof(s_class_name_buf) - 1);
        s_class_name_buf[sizeof(s_class_name_buf) - 1] = '\0';
    }

    // Class Name
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Class");
    ImGui::NextColumn();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##csharp_class", s_class_name_buf, sizeof(s_class_name_buf))) {
        csharp.class_name = s_class_name_buf;
    }
    // Drag-drop: accept .cs files from Asset Browser
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string p(static_cast<const char*>(payload->Data));
            if (p.ends_with(".cs")) {
                // Extract class name from file name (convention: FileName.cs → FileName)
                std::filesystem::path fp(p);
                std::string name = fp.stem().string();
                csharp.class_name = name;
                std::strncpy(s_class_name_buf, name.c_str(), sizeof(s_class_name_buf) - 1);
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::NextColumn();

    // Enabled
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Enabled");
    ImGui::NextColumn();
    ImGui::SetNextItemWidth(-1);
    ImGui::Checkbox("##csharp_enabled", &csharp.enabled);
    ImGui::NextColumn();

    // Bound status (read-only indicator)
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Status");
    ImGui::NextColumn();
    if (csharp.is_bound) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Bound");
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Unbound");
    }
    ImGui::NextColumn();

    ImGui::Columns(1);
}

// ─── Standalone Panel: C# Script Management ─────────────────────────────────

void DrawCSharpPanel(EditorContext& ctx) {
    ImGui::TextDisabled("C# Scripting (.NET 8 CoreCLR)");
    ImGui::Separator();

    // Status
    ImGui::Text("Runtime:");
    ImGui::SameLine();
    if (s_csharp_host_active) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Active");
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "Inactive");
    }

    ImGui::Spacing();

    // Project info
    ImGui::Text("Project: GameScripts/DSEngine.Game");
    ImGui::Text("Runtime: GameScripts/DSEngine.Runtime");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Build button — real `dotnet build` of the GameScripts solution.
    bool is_building = (s_build_status == CSharpBuildStatus::Building);
    if (is_building) ImGui::BeginDisabled();
    if (ImGui::Button(MDI_ICON_COG "  Build C# Scripts", ImVec2(-1, 0))) {
        StartCSharpBuild("Debug", "C# Build",
                         "Build succeeded (managed assemblies up to date)");
    }
    if (is_building) ImGui::EndDisabled();

    ImGui::Spacing();

    // Hot Reload button — rebuild the managed assembly through the same real
    // dotnet path. A live in-process swap needs an active runtime host, which
    // the editor process does not embed; the rebuilt assembly is picked up on
    // the next Play. Status reflects the real dotnet result (no fake success).
    if (is_building) ImGui::BeginDisabled();
    if (ImGui::Button(MDI_ICON_ROTATE_3D_VARIANT "  Hot Reload", ImVec2(-1, 0))) {
        StartCSharpBuild("Debug", "C# Hot Reload",
                         "Rebuilt managed assembly; applies on next Play "
                         "(editor has no live runtime host for in-process swap)");
    }
    if (is_building) ImGui::EndDisabled();

    ImGui::Spacing();

    // Build output
    if (!s_build_output.empty()) {
        ImVec4 color;
        switch (s_build_status) {
            case CSharpBuildStatus::Success: color = ImVec4(0.3f, 0.9f, 0.3f, 1.0f); break;
            case CSharpBuildStatus::Failed:  color = ImVec4(0.9f, 0.3f, 0.3f, 1.0f); break;
            default: color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
        }
        ImGui::TextColored(color, "%s", s_build_output.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Quick-create script
    ImGui::TextDisabled("Create New Script");
    static char new_script_name[128] = "";
    ImGui::SetNextItemWidth(-80);
    ImGui::InputText("##new_cs_name", new_script_name, sizeof(new_script_name));
    ImGui::SameLine();
    if (ImGui::Button("Create") && new_script_name[0] != '\0') {
        // Generate a new DseScript subclass file
        std::filesystem::path game_dir = "GameScripts/DSEngine.Game";
        std::filesystem::path script_path = game_dir / (std::string(new_script_name) + ".cs");
        if (!std::filesystem::exists(script_path)) {
            std::ofstream out(script_path);
            if (out.is_open()) {
                out << "using DSEngine;\n\n"
                    << "public class " << new_script_name << " : DseScript {\n"
                    << "    public override void OnStart() {\n"
                    << "    }\n\n"
                    << "    public override void OnUpdate(float dt) {\n"
                    << "    }\n"
                    << "}\n";
                out.close();
                s_build_output = std::string("Created: ") + script_path.string();
            }
        } else {
            s_build_output = "File already exists: " + script_path.string();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Script list (scan entities with CSharpScriptComponent)
    ImGui::TextDisabled("Attached Scripts");
    auto view = ctx.registry.view<CSharpScriptComponent>();
    int count = 0;
    for (auto entity : view) {
        auto& comp = view.get<CSharpScriptComponent>(entity);
        ImGui::BulletText("Entity %u: %s %s",
            static_cast<unsigned>(entity),
            comp.class_name.empty() ? "(none)" : comp.class_name.c_str(),
            comp.is_bound ? "[Bound]" : "");
        count++;
    }
    if (count == 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No C# scripts attached");
    }
}

// P0-6 self-registration: data-driven; editor_app binds visibility by id.
DSE_EDITOR_PANEL([](dse::editor::PanelRegistry& reg) {
    dse::editor::PanelEntry e;
    e.id = "csharp";
    e.display_name = "C# Scripts";
    e.category = "Tool";
    e.menu_icon = MDI_ICON_CODE;
    e.order = 120;
    e.draw = [](dse::editor::EditorContext& ctx) {
        auto* self = dse::editor::PanelRegistry::Get().Find("csharp");
        bool* open = self ? self->visible : nullptr;
        ImGui::SetNextWindowSize(ImVec2(400, 450), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("C# Scripts", open)) {
            PanelRegistry::Get().DrawMaximizeRestoreButton();
            DrawCSharpPanel(ctx);
        }
        ImGui::End();
    };
    reg.Register(std::move(e));
});

} // namespace dse::editor
