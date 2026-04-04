#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#include <Windows.h>
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_internal.h"

#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/memory_profiler.h"
#include "engine/profiler/render_profiler.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

// Helper component for editor to display names in Hierarchy
struct EditorNameComponent {
    std::string name;
};


void SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Deep tech dark theme based on Editor_UI_Architecture.md
    colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.10f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 0.66f, 1.00f, 1.00f); // Tech Blue
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 0.66f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.00f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.00f, 0.66f, 1.00f, 0.80f); // Tech Blue hover
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.00f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 0.66f, 1.00f, 0.40f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.00f, 0.66f, 1.00f, 0.80f);
    colors[ImGuiCol_Separator]              = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.00f, 0.66f, 1.00f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.00f, 0.66f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.00f, 0.66f, 1.00f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.00f, 0.66f, 1.00f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.00f, 0.66f, 1.00f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.00f, 0.66f, 1.00f, 0.80f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.54f, 0.17f, 0.89f, 0.70f); // Neon Purple for docking preview
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
    
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(6.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
}

enum class EditorState {
    Edit,
    Play
};

static EditorState g_editor_state = EditorState::Edit;
static std::unique_ptr<entt::registry> g_backup_registry;
static dse::profiler::CPUProfiler g_cpu_profiler;
static dse::profiler::MemoryProfiler g_memory_profiler;
static dse::profiler::RenderProfiler g_render_profiler;
static std::vector<float> g_cpu_frame_history;
static std::vector<float> g_fps_history;
static std::vector<float> g_render_draw_call_history;
static std::vector<float> g_memory_usage_history;
static int g_last_profiled_frame = -1;
static std::string g_profiler_export_status;
static std::vector<std::string> g_editor_languages;
static int g_editor_language_index = 0;

namespace {

constexpr int kProfilerHistoryMaxSamples = 180;

std::filesystem::path GetProjectRootPath() {
    std::filesystem::path path;
#if defined(_WIN32)
    std::wstring module_path(MAX_PATH, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (size > 0) {
        module_path.resize(size);
        path = std::filesystem::path(module_path).parent_path();
    }
#endif
    if (path.empty()) {
        path = std::filesystem::current_path();
    }
    if (path.filename() == "bin" || path.filename() == "build_vs2022") {
        path = path.parent_path();
    }
    return path.lexically_normal();
}

std::filesystem::path GetEditorBinPath() {
    const std::filesystem::path path = GetProjectRootPath() / "bin";
    std::filesystem::create_directories(path);
    return path;
}

void PushHistorySample(std::vector<float>& history, float value) {
    history.push_back(value);
    if (static_cast<int>(history.size()) > kProfilerHistoryMaxSamples) {
        history.erase(history.begin(), history.begin() + (history.size() - kProfilerHistoryMaxSamples));
    }
}

void MarkAllUILabelsDirty(entt::registry& registry) {
    auto view = registry.view<UILabelComponent>();
    for (auto entity : view) {
        view.get<UILabelComponent>(entity).dirty = true;
    }
}

std::filesystem::path GetEditorExportDirectory() {
    std::filesystem::path path = GetEditorBinPath() / "editor_exports";
    std::filesystem::create_directories(path);
    return path;
}

void ExportTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::trunc);
    if (ofs.is_open()) {
        ofs << content;
    }
}

void EnsureEditorLocalizationData() {
    auto& localization = dse::gameplay2d::LocalizationSystem::GetInstance();
    localization.Clear();

    const std::filesystem::path dir = GetEditorBinPath() / "editor_localization";
    std::filesystem::create_directories(dir);

    const std::array<std::pair<const char*, const char*>, 3> seeds = {{
        {"en", R"({"editor":{"preview":{"title":"Editor Preview","status":"Language: {lang}","selection":"Selected: {entity}"}}})"},
        {"zh", R"({"editor":{"preview":{"title":"\u7f16\u8f91\u5668\u9884\u89c8","status":"\u5f53\u524d\u8bed\u8a00\uff1a{lang}","selection":"\u5f53\u524d\u9009\u4e2d\uff1a{entity}"}}})"},
        {"ja", R"({"editor":{"preview":{"title":"\u30a8\u30c7\u30a3\u30bf\u30fc\u30d7\u30ec\u30d3\u30e5\u30fc","status":"\u73fe\u5728\u306e\u8a00\u8a9e: {lang}","selection":"\u9078\u629e\u4e2d: {entity}"}}})"}
    }};

    g_editor_languages.clear();
    for (const auto& seed : seeds) {
        const std::filesystem::path file_path = dir / (std::string(seed.first) + ".json");
        ExportTextFile(file_path, seed.second);
        if (localization.LoadLanguage(seed.first, file_path.string())) {
            g_editor_languages.emplace_back(seed.first);
        }
    }

    if (!g_editor_languages.empty()) {
        localization.SetCurrentLanguage(g_editor_languages.front());
        g_editor_language_index = 0;
    }
}

} // namespace

void DrawProfilerPanel() {
    ImGui::Begin("Profiler");

    if (ImGui::Button("Reset Profilers")) {
        g_cpu_profiler.Reset();
        g_memory_profiler.Reset();
        g_render_profiler.Reset();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Editor-side runtime metrics preview");

    const auto& frame = g_cpu_profiler.GetFrameStats();
    const auto& cpu_stats = g_cpu_profiler.GetStats();
    const auto& cpu_samples = g_cpu_profiler.GetCurrentFrameSamples();
    const auto memory_snapshot = g_memory_profiler.GetSnapshot();
    const auto& memory_categories = g_memory_profiler.GetCategoryStats();
    const auto memory_leaks = g_memory_profiler.DetectLeaks();
    const auto& render_frame = g_render_profiler.GetCurrentFrameStats();
    const auto& render_acc = g_render_profiler.GetAccumulatedStats();

    if (frame.frame_count != g_last_profiled_frame) {
        g_last_profiled_frame = frame.frame_count;
        PushHistorySample(g_cpu_frame_history, static_cast<float>(frame.frame_time_ms));
        PushHistorySample(g_fps_history, static_cast<float>(frame.fps));
        PushHistorySample(g_render_draw_call_history, static_cast<float>(render_frame.draw_calls));
        PushHistorySample(g_memory_usage_history, static_cast<float>(memory_snapshot.current_usage / 1024.0));
    }

    if (ImGui::Button("Export CPU CSV")) {
        const auto dir = GetEditorExportDirectory();
        ExportTextFile(dir / "cpu_profiler.csv", g_cpu_profiler.ExportCSV());
        ExportTextFile(dir / "cpu_profiler.json", g_cpu_profiler.ExportJSON());
        g_profiler_export_status = "Exported CPU profiler to bin/editor_exports";
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Memory CSV")) {
        const auto dir = GetEditorExportDirectory();
        ExportTextFile(dir / "memory_profiler.csv", g_memory_profiler.ExportCSV());
        g_profiler_export_status = "Exported memory profiler to bin/editor_exports";
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Render CSV")) {
        const auto dir = GetEditorExportDirectory();
        ExportTextFile(dir / "render_profiler.csv", g_render_profiler.ExportCSV());
        g_profiler_export_status = "Exported render profiler to bin/editor_exports";
    }
    if (!g_profiler_export_status.empty()) {
        ImGui::TextDisabled("%s", g_profiler_export_status.c_str());
    }

    if (ImGui::CollapsingHeader("CPU", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Frame Time: %.3f ms", frame.frame_time_ms);
        ImGui::Text("Average Frame Time: %.3f ms", frame.avg_frame_time_ms);
        ImGui::Text("FPS: %.1f", frame.fps);
        ImGui::Text("Average FPS: %.1f", frame.avg_fps);
        ImGui::Text("Frame Count: %d", frame.frame_count);
        if (!g_cpu_frame_history.empty()) {
            ImGui::PlotLines("Frame Time History", g_cpu_frame_history.data(), static_cast<int>(g_cpu_frame_history.size()), 0, nullptr, 0.0f, 40.0f, ImVec2(0, 70));
            ImGui::PlotLines("FPS History", g_fps_history.data(), static_cast<int>(g_fps_history.size()), 0, nullptr, 0.0f, 240.0f, ImVec2(0, 70));
        }

        if (ImGui::TreeNode("Current Frame Samples")) {
            for (const auto& sample : cpu_samples) {
                ImGui::Indent(sample.depth * 12.0f);
                ImGui::BulletText("%s - %.3f ms", sample.name.c_str(), sample.duration_ms);
                ImGui::Unindent(sample.depth * 12.0f);
            }
            if (cpu_samples.empty()) {
                ImGui::TextDisabled("No CPU samples recorded yet.");
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Accumulated CPU Stats")) {
            if (ImGui::BeginTable("cpu_stats_table", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Calls");
                ImGui::TableSetupColumn("Avg ms");
                ImGui::TableSetupColumn("Min ms");
                ImGui::TableSetupColumn("Max ms");
                ImGui::TableSetupColumn("Total ms");
                ImGui::TableHeadersRow();

                for (const auto& [name, stat] : cpu_stats) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name.c_str());
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%d", stat.call_count);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f", stat.avg_ms);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f", stat.min_ms);
                    ImGui::TableSetColumnIndex(4); ImGui::Text("%.3f", stat.max_ms);
                    ImGui::TableSetColumnIndex(5); ImGui::Text("%.3f", stat.total_ms);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Current Usage: %.2f KB", memory_snapshot.current_usage / 1024.0f);
        ImGui::Text("Peak Usage: %.2f KB", memory_snapshot.peak_usage / 1024.0f);
        ImGui::Text("Total Allocated: %.2f KB", memory_snapshot.total_allocated / 1024.0f);
        ImGui::Text("Total Freed: %.2f KB", memory_snapshot.total_freed / 1024.0f);
        ImGui::Text("Active Allocations: %d", memory_snapshot.active_allocations);
        if (!g_memory_usage_history.empty()) {
            ImGui::PlotLines("Usage History (KB)", g_memory_usage_history.data(), static_cast<int>(g_memory_usage_history.size()), 0, nullptr, 0.0f, *std::max_element(g_memory_usage_history.begin(), g_memory_usage_history.end()) + 1.0f, ImVec2(0, 70));
        }

        if (!memory_leaks.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Potential Leaks: %d", static_cast<int>(memory_leaks.size()));
            for (const auto& leak : memory_leaks) {
                ImGui::BulletText("%s", leak.c_str());
            }
        }

        if (ImGui::BeginTable("memory_stats_table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Tag");
            ImGui::TableSetupColumn("Current KB");
            ImGui::TableSetupColumn("Peak KB");
            ImGui::TableSetupColumn("Allocated KB");
            ImGui::TableSetupColumn("Freed KB");
            ImGui::TableHeadersRow();

            for (const auto& [tag, stat] : memory_categories) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(tag.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.2f", stat.current_bytes / 1024.0f);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", stat.peak_bytes / 1024.0f);
                ImGui::TableSetColumnIndex(3); ImGui::Text("%.2f", stat.total_allocated / 1024.0f);
                ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", stat.total_freed / 1024.0f);
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Draw Calls: %d", render_frame.draw_calls);
        ImGui::Text("Triangles: %d", render_frame.triangle_count);
        ImGui::Text("Vertices: %d", render_frame.vertex_count);
        ImGui::Text("Sprites: %d", render_frame.sprite_count);
        ImGui::Text("Batches: %d", render_frame.batch_count);
        ImGui::Text("Texture Binds: %d", render_frame.texture_binds);
        ImGui::Text("Shader Switches: %d", render_frame.shader_switches);
        ImGui::Text("Texture Memory: %.2f KB", render_frame.texture_memory / 1024.0f);
        ImGui::Separator();
        ImGui::Text("Avg Draw Calls: %.2f", render_acc.avg_draw_calls);
        ImGui::Text("Avg Triangles: %.2f", render_acc.avg_triangles);
        ImGui::Text("Avg Vertices: %.2f", render_acc.avg_vertices);
        ImGui::Text("Peak Draw Calls: %d", render_acc.peak_draw_calls);
        ImGui::Text("Peak Triangles: %d", render_acc.peak_triangles);
        ImGui::Text("Peak Vertices: %d", render_acc.peak_vertices);
        ImGui::Text("Profiled Frames: %d", render_acc.frame_count);
        if (!g_render_draw_call_history.empty()) {
            ImGui::PlotLines("Draw Call History", g_render_draw_call_history.data(), static_cast<int>(g_render_draw_call_history.size()), 0, nullptr, 0.0f, *std::max_element(g_render_draw_call_history.begin(), g_render_draw_call_history.end()) + 1.0f, ImVec2(0, 70));
        }
    }

    ImGui::End();
}

void CopyRegistry(entt::registry& dst, entt::registry& src) {
    dst.clear();
    for (auto entity : src.storage<entt::entity>()) {
        if (!src.valid(entity)) continue; // Skip destroyed/recycled entities
        auto new_ent = dst.create(entity);
        
        if (src.all_of<EditorNameComponent>(entity)) dst.emplace<EditorNameComponent>(new_ent, src.get<EditorNameComponent>(entity));
        if (src.all_of<TransformComponent>(entity)) dst.emplace<TransformComponent>(new_ent, src.get<TransformComponent>(entity));
        if (src.all_of<SpriteRendererComponent>(entity)) dst.emplace<SpriteRendererComponent>(new_ent, src.get<SpriteRendererComponent>(entity));
        if (src.all_of<UILabelComponent>(entity)) dst.emplace<UILabelComponent>(new_ent, src.get<UILabelComponent>(entity));
        if (src.all_of<RigidBody2DComponent>(entity)) dst.emplace<RigidBody2DComponent>(new_ent, src.get<RigidBody2DComponent>(entity));
        if (src.all_of<ParticleEmitterComponent>(entity)) dst.emplace<ParticleEmitterComponent>(new_ent, src.get<ParticleEmitterComponent>(entity));
    }
}

void SaveScene(entt::registry& registry, const std::string& filepath) {
    rapidjson::Document doc;
    doc.SetArray();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    for (auto entity : registry.storage<entt::entity>()) {
        if (!registry.valid(entity)) continue; // Skip destroyed/recycled entities
        rapidjson::Value ent_obj(rapidjson::kObjectType);
        ent_obj.AddMember("id", static_cast<uint32_t>(entity), allocator);

        if (registry.all_of<EditorNameComponent>(entity)) {
            rapidjson::Value name_val;
            name_val.SetString(registry.get<EditorNameComponent>(entity).name.c_str(), allocator);
            ent_obj.AddMember("name", name_val, allocator);
        }

        if (registry.all_of<TransformComponent>(entity)) {
            auto& t = registry.get<TransformComponent>(entity);
            rapidjson::Value t_obj(rapidjson::kObjectType);
            
            rapidjson::Value pos(rapidjson::kArrayType);
            pos.PushBack(t.position.x, allocator).PushBack(t.position.y, allocator).PushBack(t.position.z, allocator);
            t_obj.AddMember("position", pos, allocator);
            
            rapidjson::Value rot(rapidjson::kArrayType);
            rot.PushBack(t.rotation.x, allocator).PushBack(t.rotation.y, allocator).PushBack(t.rotation.z, allocator).PushBack(t.rotation.w, allocator);
            t_obj.AddMember("rotation", rot, allocator);
            
            rapidjson::Value scale(rapidjson::kArrayType);
            scale.PushBack(t.scale.x, allocator).PushBack(t.scale.y, allocator).PushBack(t.scale.z, allocator);
            t_obj.AddMember("scale", scale, allocator);
            
            ent_obj.AddMember("transform", t_obj, allocator);
        }
        
        if (registry.all_of<SpriteRendererComponent>(entity)) {
            auto& s = registry.get<SpriteRendererComponent>(entity);
            rapidjson::Value s_obj(rapidjson::kObjectType);
            
            rapidjson::Value path_val;
            path_val.SetString(s.shader_variant.c_str(), allocator);
            s_obj.AddMember("path", path_val, allocator);
            
            rapidjson::Value color(rapidjson::kArrayType);
            color.PushBack(s.color.r, allocator).PushBack(s.color.g, allocator).PushBack(s.color.b, allocator).PushBack(s.color.a, allocator);
            s_obj.AddMember("color", color, allocator);
            
            ent_obj.AddMember("sprite", s_obj, allocator);
        }

        doc.PushBack(ent_obj, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs(filepath);
    if (ofs.is_open()) {
        ofs << buffer.GetString();
    }
}

void LoadScene(entt::registry& registry, const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    rapidjson::Document doc;
    doc.Parse(content.c_str());

    if (!doc.IsArray()) return;

    registry.clear();
    for (auto& v : doc.GetArray()) {
        auto entity = registry.create();
        
        if (v.HasMember("name") && v["name"].IsString()) {
            registry.emplace<EditorNameComponent>(entity, v["name"].GetString());
        }

        if (v.HasMember("transform") && v["transform"].IsObject()) {
            auto& t_obj = v["transform"];
            auto& t = registry.emplace<TransformComponent>(entity);
            if (t_obj.HasMember("position") && t_obj["position"].IsArray()) {
                auto pos = t_obj["position"].GetArray();
                if (pos.Size() >= 3) t.position = glm::vec3(pos[0].GetFloat(), pos[1].GetFloat(), pos[2].GetFloat());
            }
            if (t_obj.HasMember("rotation") && t_obj["rotation"].IsArray()) {
                auto rot = t_obj["rotation"].GetArray();
                if (rot.Size() >= 4) t.rotation = glm::quat(rot[3].GetFloat(), rot[0].GetFloat(), rot[1].GetFloat(), rot[2].GetFloat());
            }
            if (t_obj.HasMember("scale") && t_obj["scale"].IsArray()) {
                auto scale = t_obj["scale"].GetArray();
                if (scale.Size() >= 3) t.scale = glm::vec3(scale[0].GetFloat(), scale[1].GetFloat(), scale[2].GetFloat());
            }
        }
        
        if (v.HasMember("sprite") && v["sprite"].IsObject()) {
            auto& s_obj = v["sprite"];
            auto& s = registry.emplace<SpriteRendererComponent>(entity);
            if (s_obj.HasMember("path") && s_obj["path"].IsString()) {
                s.shader_variant = s_obj["path"].GetString();
            }
            if (s_obj.HasMember("color") && s_obj["color"].IsArray()) {
                auto color = s_obj["color"].GetArray();
                if (color.Size() >= 4) s.color = glm::vec4(color[0].GetFloat(), color[1].GetFloat(), color[2].GetFloat(), color[3].GetFloat());
            }
        }
    }
}

void DrawEditorUI(dse::runtime::EngineInstance& engine, unsigned int scene_texture, unsigned int game_texture) {
    static entt::entity selected_entity = entt::null;
    World& world = engine.pipeline()->world();
    auto& registry = world.registry();
    
    static bool inspector_active = true;
    static bool inspector_static = false;
    static bool sprite_flip_x = false;
    static bool sprite_flip_y = false;
    static bool collider_is_trigger = false;
    static char localization_preview_key[128] = "editor.preview.status";
    static char localization_preview_fallback[128] = "Language: {lang}";
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        static bool first_time = true;
        if (first_time) {
            first_time = false;

            ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            // Split the dockspace into nodes -- Unity Style
            auto dock_id_main = dockspace_id;
            
            // 1. Split top for toolbar
            auto dock_id_top = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Up, 0.05f, nullptr, &dock_id_main);
            
            // 2. Split bottom for Project/Console/Animation
            auto dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.30f, nullptr, &dock_id_main);
            
            // 3. Split left for Hierarchy
            auto dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, nullptr, &dock_id_main);
            
            // 4. Split right for Inspector
            auto dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, nullptr, &dock_id_main);

            // We now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow("Toolbar", dock_id_top);
            ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Project", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Animation", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Profiler", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Tile Palette", dock_id_bottom); // New 2D panel
            ImGui::DockBuilderDockWindow("Scene", dock_id_main);
            ImGui::DockBuilderDockWindow("Game", dock_id_main);
            
            // Set some nodes to hide their tab bar if needed (e.g., Toolbar)
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id_top);
            if (node) {
                node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeY;
            }
            
            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                registry.clear();
            }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {
                LoadScene(registry, "scene.json");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                SaveScene(registry, "scene.json");
            }
            if (ImGui::MenuItem("Save As...")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Toolbar Panel
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 4));
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);
    float avail_width = ImGui::GetContentRegionAvail().x;
    
    // Tools (Left)
    static int current_gizmo_operation = 0; // 0: Translate
    ImGui::SetCursorPosX(10);
    
    ImGui::PushStyleColor(ImGuiCol_Button, current_gizmo_operation == -1 ? ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("[H]", ImVec2(32, 24))) { current_gizmo_operation = -1; } // Hand
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, current_gizmo_operation == 0 ? ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("[M]", ImVec2(32, 24))) { current_gizmo_operation = 0; } // Move
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, current_gizmo_operation == 1 ? ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("[R]", ImVec2(32, 24))) { current_gizmo_operation = 1; } // Rotate
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, current_gizmo_operation == 2 ? ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] : ImGui::GetStyle().Colors[ImGuiCol_Button]);
    if (ImGui::Button("[S]", ImVec2(32, 24))) { current_gizmo_operation = 2; } // Scale
    ImGui::PopStyleColor();
    ImGui::SameLine();
    
    // 2D/3D Toggle
    ImGui::SetCursorPosX(10 + 4 * 36 + 20); // Spacing after transform tools
    static bool is2D = false;
    if (is2D) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
    }
    if (ImGui::Button("2D", ImVec2(32, 24))) { is2D = !is2D; }
    if (is2D) {
        ImGui::PopStyleColor();
    }

    // Play Controls (Center)
    ImGui::SameLine();
    ImGui::SetCursorPosX((avail_width / 2.0f) - 60);
    
    if (g_editor_state == EditorState::Edit) {
        if (ImGui::Button(">", ImVec2(32, 24))) { // Play
            g_editor_state = EditorState::Play;
            g_backup_registry = std::make_unique<entt::registry>();
            CopyRegistry(*g_backup_registry, registry);
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("[]", ImVec2(32, 24))) { // Stop
            g_editor_state = EditorState::Edit;
            CopyRegistry(registry, *g_backup_registry);
            g_backup_registry.reset();
            selected_entity = entt::null;
        }
        ImGui::PopStyleColor();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("||", ImVec2(32, 24))) {} // Pause
    ImGui::SameLine();
    if (ImGui::Button(">|", ImVec2(32, 24))) {} // Step

    // Account/Settings (Right)
    ImGui::SameLine();
    ImGui::SetCursorPosX(avail_width - 320);
    if (!g_editor_languages.empty()) {
        std::vector<const char*> language_items;
        language_items.reserve(g_editor_languages.size());
        for (const auto& lang : g_editor_languages) {
            language_items.push_back(lang.c_str());
        }
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##LanguagePreview", &g_editor_language_index, language_items.data(), static_cast<int>(language_items.size()))) {
            auto& localization = dse::gameplay2d::LocalizationSystem::GetInstance();
            localization.SetCurrentLanguage(g_editor_languages[g_editor_language_index]);
            MarkAllUILabelsDirty(registry);
        }
        ImGui::SameLine();
    }
    ImGui::Button("Collab", ImVec2(60, 24));
    ImGui::SameLine();
    ImGui::Button("Layers", ImVec2(60, 24));

    ImGui::End();
    ImGui::PopStyleVar();

    // Panels (Unity-style layout)
    ImGui::Begin("Hierarchy");
    
    if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto entity : registry.storage<entt::entity>()) {
            if (!registry.valid(entity)) continue; // Skip destroyed/recycled entities
            std::string entity_name = "Entity " + std::to_string(static_cast<uint32_t>(entity));
            if (registry.all_of<EditorNameComponent>(entity)) {
                entity_name = registry.get<EditorNameComponent>(entity).name;
            }
            
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (selected_entity == entity) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            
            ImGui::TreeNodeEx((void*)(uintptr_t)entity, flags, "%s", entity_name.c_str());
            if (ImGui::IsItemClicked()) {
                selected_entity = entity;
            }
        }
        ImGui::TreePop();
    }
    
    // Right click menu for Hierarchy
    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::MenuItem("Create Empty Entity")) {
            auto new_ent = world.CreateEntity();
            registry.emplace<EditorNameComponent>(new_ent, "New Entity");
            selected_entity = new_ent;
        }
        if (selected_entity != entt::null && ImGui::MenuItem("Delete Entity")) {
            world.DestroyEntity(selected_entity);
            selected_entity = entt::null;
        }
        ImGui::EndPopup();
    }
    
    ImGui::End();

    ImGui::Begin("Inspector");
    
    if (selected_entity != entt::null && registry.valid(selected_entity)) {
        // Header component
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
        ImGui::AlignTextToFramePadding();
        ImGui::Checkbox("##Active", &inspector_active); ImGui::SameLine();
        
        char nameBuf[64] = "";
        if (registry.all_of<EditorNameComponent>(selected_entity)) {
            strncpy(nameBuf, registry.get<EditorNameComponent>(selected_entity).name.c_str(), sizeof(nameBuf) - 1);
        } else {
            strncpy(nameBuf, "Entity", sizeof(nameBuf) - 1);
        }
        
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 70);
        if (ImGui::InputText("##Name", nameBuf, sizeof(nameBuf))) {
            if (registry.all_of<EditorNameComponent>(selected_entity)) {
                registry.get<EditorNameComponent>(selected_entity).name = nameBuf;
            } else {
                registry.emplace<EditorNameComponent>(selected_entity, nameBuf);
            }
        }
        ImGui::PopItemWidth();
        
        ImGui::SameLine();
        ImGui::Checkbox("Static", &inspector_static);
        ImGui::Separator();

        // Helper macro for left-aligned labels in Inspector
        #define INSPECTOR_PROPERTY(label, code) \
            ImGui::AlignTextToFramePadding(); \
            ImGui::Text(label); \
            ImGui::NextColumn(); \
            ImGui::SetNextItemWidth(-1); \
            code; \
            ImGui::NextColumn();

        if (registry.all_of<TransformComponent>(selected_entity)) {
            auto& transform = registry.get<TransformComponent>(selected_entity);
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Columns(2, "transform_cols", false);
                ImGui::SetColumnWidth(0, 80.0f);
                
                float pos[3] = {transform.position.x, transform.position.y, transform.position.z};
                INSPECTOR_PROPERTY("Position", if (ImGui::DragFloat3("##pos", pos, 0.1f)) {
                    transform.position = glm::vec3(pos[0], pos[1], pos[2]);
                });
                
                // transform.rotation is a glm::quat, we might want to convert to Euler angles for editing, but for now we will just use a dummy logic or simple representation.
                glm::vec3 euler = glm::degrees(glm::eulerAngles(transform.rotation));
                float rot[3] = {euler.x, euler.y, euler.z};
                if (is2D) {
                    INSPECTOR_PROPERTY("Rotation", if (ImGui::DragFloat("##rotZ", &rot[2], 0.1f)) {
                        euler.z = rot[2];
                        transform.rotation = glm::quat(glm::radians(euler));
                    });
                } else {
                    INSPECTOR_PROPERTY("Rotation", if (ImGui::DragFloat3("##rot", rot, 0.1f)) {
                        euler = glm::vec3(rot[0], rot[1], rot[2]);
                        transform.rotation = glm::quat(glm::radians(euler));
                    });
                }
                
                float scale[3] = {transform.scale.x, transform.scale.y, transform.scale.z};
                INSPECTOR_PROPERTY("Scale", if (ImGui::DragFloat3("##scale", scale, 0.1f)) {
                    transform.scale = glm::vec3(scale[0], scale[1], scale[2]);
                });
                
                ImGui::Columns(1);
            }
        }
        
        if (registry.all_of<SpriteRendererComponent>(selected_entity)) {
            auto& sprite = registry.get<SpriteRendererComponent>(selected_entity);
            if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Columns(2, "spriterenderer_cols", false);
                ImGui::SetColumnWidth(0, 80.0f);
                
                // Asset drop target
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Sprite");
                ImGui::NextColumn();
                ImGui::SetNextItemWidth(-1);
                ImGui::Button(sprite.shader_variant.empty() ? "None (Texture)" : "Texture Set", ImVec2(-1, 0));
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                        const char* path = (const char*)payload->Data;
                        sprite.shader_variant = path; // Mocking setting texture path
                    }
                    ImGui::EndDragDropTarget();
                }
                ImGui::NextColumn();
                
                float color[4] = {sprite.color.r, sprite.color.g, sprite.color.b, sprite.color.a};
                INSPECTOR_PROPERTY("Color", if (ImGui::ColorEdit4("##color", color)) {
                    sprite.color = glm::vec4(color[0], color[1], color[2], color[3]);
                });
                
                ImGui::Columns(1);
            }
        }

        if (registry.all_of<RigidBody2DComponent>(selected_entity)) {
            auto& rb2d = registry.get<RigidBody2DComponent>(selected_entity);
            if (ImGui::CollapsingHeader("RigidBody 2D", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Columns(2, "rb2d_cols", false);
                ImGui::SetColumnWidth(0, 80.0f);
                
                const char* bodyTypes[] = { "Static", "Kinematic", "Dynamic" };
                int currentType = static_cast<int>(rb2d.type);
                INSPECTOR_PROPERTY("Body Type", if (ImGui::Combo("##type", &currentType, bodyTypes, IM_ARRAYSIZE(bodyTypes))) {
                    rb2d.type = static_cast<RigidBody2DType>(currentType);
                });
                
                float vel[2] = {rb2d.velocity.x, rb2d.velocity.y};
                INSPECTOR_PROPERTY("Velocity", if (ImGui::DragFloat2("##vel", vel, 0.1f)) {
                    rb2d.velocity = glm::vec2(vel[0], vel[1]);
                });
                
                INSPECTOR_PROPERTY("Gravity", ImGui::DragFloat("##grav", &rb2d.gravity_scale, 0.1f));
                
                ImGui::Columns(1);
            }
        }

        if (registry.all_of<UILabelComponent>(selected_entity)) {
            auto& label = registry.get<UILabelComponent>(selected_entity);
            if (ImGui::CollapsingHeader("UI Label", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Columns(2, "uilabel_cols", false);
                ImGui::SetColumnWidth(0, 110.0f);

                char text_buf[256] = {};
                std::strncpy(text_buf, label.text.c_str(), sizeof(text_buf) - 1);
                INSPECTOR_PROPERTY("Text", if (ImGui::InputText("##label_text", text_buf, sizeof(text_buf))) {
                    label.text = text_buf;
                    label.dirty = true;
                });

                INSPECTOR_PROPERTY("Localized", if (ImGui::Checkbox("##use_loc", &label.use_localization)) {
                    label.dirty = true;
                });

                char key_buf[128] = {};
                std::strncpy(key_buf, label.localization_key.c_str(), sizeof(key_buf) - 1);
                INSPECTOR_PROPERTY("Loc Key", if (ImGui::InputText("##loc_key", key_buf, sizeof(key_buf))) {
                    label.localization_key = key_buf;
                    label.dirty = true;
                });

                char fallback_buf[256] = {};
                std::strncpy(fallback_buf, label.fallback_text.c_str(), sizeof(fallback_buf) - 1);
                INSPECTOR_PROPERTY("Fallback", if (ImGui::InputText("##fallback_text", fallback_buf, sizeof(fallback_buf))) {
                    label.fallback_text = fallback_buf;
                    label.dirty = true;
                });

                char param_name[64] = "name";
                char param_value[128] = "Player";
                INSPECTOR_PROPERTY("Param", ImGui::InputText("##param_name", param_name, sizeof(param_name)););
                INSPECTOR_PROPERTY("Value", ImGui::InputText("##param_value", param_value, sizeof(param_value)););
                INSPECTOR_PROPERTY("Apply Param", if (ImGui::Button("Set/Update Param", ImVec2(-1, 0))) {
                    label.localization_params[param_name] = param_value;
                    label.dirty = true;
                });

                ImGui::Columns(1);
            }
        }

        if (registry.all_of<ParticleEmitterComponent>(selected_entity)) {
            auto& emitter = registry.get<ParticleEmitterComponent>(selected_entity);
            if (ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Columns(2, "particle_cols", false);
                ImGui::SetColumnWidth(0, 110.0f);

                INSPECTOR_PROPERTY("Emitting", ImGui::Checkbox("##emitting", &emitter.emitting));
                INSPECTOR_PROPERTY("Max Particles", ImGui::DragInt("##max_particles", &emitter.max_particles, 1.0f, 1, 5000));
                INSPECTOR_PROPERTY("Emit Rate", ImGui::DragFloat("##emit_rate", &emitter.emit_rate, 0.1f, 0.0f, 1000.0f));
                INSPECTOR_PROPERTY("Burst", if (ImGui::Button("Emit 10", ImVec2(-1, 0))) { emitter.pending_burst += 10; });
                INSPECTOR_PROPERTY("Life Time", ImGui::DragFloat("##life_time", &emitter.start_life_time, 0.05f, 0.05f, 30.0f));
                INSPECTOR_PROPERTY("Start Size", ImGui::DragFloat("##start_size", &emitter.start_size, 0.05f, 0.01f, 100.0f));
                INSPECTOR_PROPERTY("Start Color", ImGui::ColorEdit4("##start_color", glm::value_ptr(emitter.start_color)));
                INSPECTOR_PROPERTY("Gravity", ImGui::DragFloat3("##gravity", glm::value_ptr(emitter.gravity), 0.05f));
                INSPECTOR_PROPERTY("Random Params", ImGui::Checkbox("##random_params", &emitter.use_random_params));
                INSPECTOR_PROPERTY("Size Curve", ImGui::Checkbox("##size_curve_enabled", &emitter.size_curve.enabled));
                INSPECTOR_PROPERTY("Size End", ImGui::DragFloat("##size_curve_end", &emitter.size_curve.end_value, 0.05f, 0.0f, 100.0f));
                INSPECTOR_PROPERTY("Alpha Curve", ImGui::Checkbox("##alpha_curve_enabled", &emitter.alpha_curve.enabled));
                INSPECTOR_PROPERTY("Alpha End", ImGui::DragFloat("##alpha_curve_end", &emitter.alpha_curve.end_value, 0.01f, 0.0f, 1.0f));
                INSPECTOR_PROPERTY("Speed Curve", ImGui::Checkbox("##speed_curve_enabled", &emitter.speed_curve.enabled));
                INSPECTOR_PROPERTY("Speed End", ImGui::DragFloat("##speed_curve_end", &emitter.speed_curve.end_value, 0.05f, 0.0f, 10.0f));

                const char* collision_modes[] = { "None", "GroundPlane", "Box2D" };
                int collision_mode = static_cast<int>(emitter.collision_mode);
                INSPECTOR_PROPERTY("Collision", if (ImGui::Combo("##collision_mode", &collision_mode, collision_modes, IM_ARRAYSIZE(collision_modes))) {
                    emitter.collision_mode = static_cast<ParticleCollisionMode>(collision_mode);
                });
                INSPECTOR_PROPERTY("Bounce", ImGui::DragFloat("##collision_bounce", &emitter.collision_bounce, 0.01f, 0.0f, 1.0f));
                INSPECTOR_PROPERTY("Ground Y", ImGui::DragFloat("##ground_y", &emitter.ground_y, 0.05f));

                ImGui::Columns(1);
                ImGui::TextDisabled("Active Particles: %d", static_cast<int>(emitter.particles.size()));
            }
        }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - 60);
    if (ImGui::Button("Add Component", ImVec2(120, 30))) {
        ImGui::OpenPopup("AddComponentPopup");
    }
    
    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (ImGui::MenuItem("Transform")) {
            if (!registry.all_of<TransformComponent>(selected_entity)) {
                registry.emplace<TransformComponent>(selected_entity);
            }
        }
        if (ImGui::MenuItem("Name")) {
            if (!registry.all_of<EditorNameComponent>(selected_entity)) {
                registry.emplace<EditorNameComponent>(selected_entity, "New Component");
            }
        }
        if (ImGui::MenuItem("Sprite Renderer")) {
            if (!registry.all_of<SpriteRendererComponent>(selected_entity)) {
                registry.emplace<SpriteRendererComponent>(selected_entity);
            }
        }
        if (ImGui::MenuItem("RigidBody 2D")) {
            if (!registry.all_of<RigidBody2DComponent>(selected_entity)) {
                registry.emplace<RigidBody2DComponent>(selected_entity);
            }
        }
        if (ImGui::MenuItem("UI Label")) {
            if (!registry.all_of<UILabelComponent>(selected_entity)) {
                auto& label = registry.emplace<UILabelComponent>(selected_entity);
                label.text = "Label";
                label.fallback_text = "Label";
            }
        }
        if (ImGui::MenuItem("Particle Emitter")) {
            if (!registry.all_of<ParticleEmitterComponent>(selected_entity)) {
                registry.emplace<ParticleEmitterComponent>(selected_entity);
            }
        }
        // More components can be added here
        ImGui::EndPopup();
    }
    
    ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("No Entity Selected");
    }
    ImGui::End();

    ImGui::Begin("Project");
    ImGui::Text("Assets");
    ImGui::Separator();
    
    // Simple file browser
    // Since the executable runs from the bin/ directory or project root, we need to find the correct data path
    static std::filesystem::path base_data_path = []() {
        std::filesystem::path p = GetProjectRootPath();

        std::filesystem::path target_path = p / "samples" / "lua" / "data";
        
        // If data folder doesn't exist, try creating it or fallback to root
        if (!std::filesystem::exists(target_path)) {
            try {
                std::filesystem::create_directories(target_path);
            } catch (...) {
                return p;
            }
        }
        return target_path;
    }();
    
    static std::filesystem::path current_path = base_data_path;
    
    if (ImGui::BeginPopupContextWindow("ProjectContextMenu")) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) {
                std::string new_folder = (current_path / "NewFolder").string();
                std::filesystem::create_directory(new_folder);
            }
            if (ImGui::MenuItem("Lua Script")) {
                std::string new_script = (current_path / "NewScript.lua").string();
                std::ofstream ofs(new_script);
                if (ofs.is_open()) {
                    ofs << "-- New Lua Script\n";
                    ofs.close();
                }
            }
            if (ImGui::MenuItem("Material")) {
                std::string new_mat = (current_path / "NewMaterial.mat").string();
                std::ofstream ofs(new_mat);
                if (ofs.is_open()) {
                    ofs << "{\n  \"shader\": \"default\",\n  \"color\": [1,1,1,1]\n}\n";
                    ofs.close();
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    
    if (!std::filesystem::exists(current_path)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Data path not found: %s", current_path.string().c_str());
    } else {
        if (current_path != base_data_path) {
            if (ImGui::Button("<- Back")) {
                current_path = current_path.parent_path();
            }
            ImGui::Separator();
        }
        
        try {
            for (const auto& entry : std::filesystem::directory_iterator(current_path)) {
                const auto& path = entry.path();
                auto filename = path.filename().string();
                
                if (entry.is_directory()) {
                    if (ImGui::Selectable((std::string("[DIR] ") + filename).c_str())) {
                        current_path /= path.filename();
                    }
                } else {
                    ImGui::Selectable((std::string("[FILE] ") + filename).c_str());
                    
                    // Support Drag and Drop
                    if (ImGui::BeginDragDropSource()) {
                        std::string relative_path = std::filesystem::relative(path, base_data_path).string();
                        ImGui::SetDragDropPayload("ASSET_PATH", relative_path.c_str(), relative_path.size() + 1);
                        ImGui::Text("%s", filename.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Error reading directory: %s", e.what());
        }
    }
    ImGui::End();

    ImGui::Begin("Console");
    ImGui::Text("[Info] Engine initialized successfully.");
    ImGui::Text("[Info] Loaded default scene.");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[Warning] Missing texture 'skybox_diffuse'.");
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Error] Failed to load shader 'standard_pbr.glsl'.");
    ImGui::End();

    ImGui::Begin("Localization Preview");
    auto& localization = dse::gameplay2d::LocalizationSystem::GetInstance();
    ImGui::Text("Current Language: %s", localization.GetCurrentLanguage().c_str());
    ImGui::InputText("Preview Key", localization_preview_key, sizeof(localization_preview_key));
    ImGui::InputText("Fallback", localization_preview_fallback, sizeof(localization_preview_fallback));
    std::unordered_map<std::string, std::string> preview_params;
    preview_params["lang"] = localization.GetCurrentLanguage();
    preview_params["entity"] = selected_entity == entt::null ? std::string("None") : std::to_string(static_cast<uint32_t>(selected_entity));
    const std::string preview_text = localization.GetTextWithParams(localization_preview_key, preview_params, localization_preview_fallback);
    ImGui::Separator();
    ImGui::TextWrapped("%s", preview_text.c_str());
    if (selected_entity != entt::null && registry.valid(selected_entity) && registry.all_of<UILabelComponent>(selected_entity)) {
        if (ImGui::Button("Apply To Selected UILabel")) {
            auto& label = registry.get<UILabelComponent>(selected_entity);
            label.use_localization = true;
            label.localization_key = localization_preview_key;
            label.fallback_text = localization_preview_fallback;
            label.localization_params = preview_params;
            label.dirty = true;
        }
    } else {
        ImGui::TextDisabled("Select a UILabel entity to apply preview settings.");
    }
    ImGui::End();

    DrawProfilerPanel();
    
    ImGui::Begin("Animation");
    ImGui::Text("No animated object selected.");
    ImGui::End();

    ImGui::Begin("Tile Palette");
    if (!is2D) {
        ImGui::TextDisabled("Tile Palette is only available in 2D mode.");
    } else {
        ImGui::Button("Active Brush", ImVec2(120, 24)); ImGui::SameLine();
        ImGui::Button("Paint", ImVec2(60, 24)); ImGui::SameLine();
        ImGui::Button("Erase", ImVec2(60, 24));
        
        ImGui::Separator();
        ImGui::Text("Select a tilemap to start painting.");
        
        // Mock tile palette grid
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 6; x++) {
                draw_list->AddRectFilled(
                    ImVec2(p.x + x * 32, p.y + y * 32), 
                    ImVec2(p.x + x * 32 + 30, p.y + y * 32 + 30), 
                    IM_COL32(80 + (x+y)*10, 100 + x*10, 120 + y*10, 255)
                );
            }
        }
    }
    ImGui::End();

    // The Scene Viewport Panel (Where engine editor rendering would happen)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Scene");
    ImVec2 scenePanelSize = ImGui::GetContentRegionAvail();
    
    // Process Picking
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 window_pos = ImGui::GetWindowPos();
        // Just a mock for picking logic - would normally use real world coordinates from engine camera
        float x = (mouse_pos.x - window_pos.x) - scenePanelSize.x / 2.0f;
        float y = (mouse_pos.y - window_pos.y) - scenePanelSize.y / 2.0f;
        
        // Simple distance check
        float min_dist = 10000.0f;
        entt::entity picked = entt::null;
        for (auto entity : registry.storage<entt::entity>()) {
            if (!registry.valid(entity)) continue; // Skip destroyed/recycled entities
            if (registry.all_of<TransformComponent>(entity)) {
                auto& t = registry.get<TransformComponent>(entity);
                // Mock coordinate mapping
                float dx = (t.position.x * 50.0f) - x;
                float dy = (-t.position.y * 50.0f) - y;
                float dist = dx*dx + dy*dy;
                if (dist < 2500.0f && dist < min_dist) { // 50 pixel radius
                    min_dist = dist;
                    picked = entity;
                }
            }
        }
        if (picked != entt::null) {
            selected_entity = picked;
        }
    }

    if (scene_texture != 0) {
        // Render engine FBO texture, note UVs are flipped vertically because OpenGL textures are bottom-up
        ImGui::Image((ImTextureID)(intptr_t)scene_texture, scenePanelSize, ImVec2(0, 1), ImVec2(1, 0));
        
    } else {
        // Fallback placeholder
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + scenePanelSize.x, p_min.y + scenePanelSize.y);
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(40, 40, 40, 255));
        for (float i = 0; i < scenePanelSize.x; i += 50) {
            draw_list->AddLine(ImVec2(p_min.x + i, p_min.y), ImVec2(p_min.x + i, p_max.y), IM_COL32(60, 60, 60, 255));
        }
        for (float i = 0; i < scenePanelSize.y; i += 50) {
            draw_list->AddLine(ImVec2(p_min.x, p_min.y + i), ImVec2(p_max.x, p_min.y + i), IM_COL32(60, 60, 60, 255));
        }
        draw_list->AddText(ImVec2(p_min.x + scenePanelSize.x/2 - 50, p_min.y + scenePanelSize.y/2), IM_COL32(200, 200, 200, 255), "Scene View");
    }
    ImGui::End();
    ImGui::PopStyleVar();

    // The Game Viewport Panel (Independent Game Camera view)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Game");
    ImVec2 gamePanelSize = ImGui::GetContentRegionAvail();
    if (game_texture != 0) {
        ImGui::Image((ImTextureID)(intptr_t)game_texture, gamePanelSize, ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 p_min = ImGui::GetCursorScreenPos();
        ImVec2 p_max = ImVec2(p_min.x + gamePanelSize.x, p_min.y + gamePanelSize.y);
        draw_list->AddRectFilled(p_min, p_max, IM_COL32(20, 20, 20, 255));
        draw_list->AddText(ImVec2(p_min.x + gamePanelSize.x/2 - 40, p_min.y + gamePanelSize.y/2), IM_COL32(150, 150, 150, 255), "Game View");
    }
    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::End(); // End DockSpace window
}

int main() {
    // Allocate a console for WIN32 app so we can see crash output
    #if defined(_WIN32)
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    #endif
    
    std::cerr << "[Editor] Starting..." << std::endl;
    
    if (!glfwInit()) {
        std::cerr << "[Editor] glfwInit() failed!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "DSEngine Editor", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window in Editor." << std::endl;
        glfwTerminate();
        return -1;
    }

    std::cout << "Editor window created successfully." << std::endl;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL (gladLoadGL) in Editor." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    // NOTE: Multi-viewport on this editor build has been causing CRT heap assertions
    // on Windows during runtime/teardown. Keep docking enabled, but disable platform
    // viewports for stability until the backend lifetime issue is fully resolved.
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Redirect imgui.ini to bin folder so it doesn't clutter the project root
    static std::string imgui_ini_path = (GetEditorBinPath() / "editor_layout.ini").string();
    io.IniFilename = imgui_ini_path.c_str();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    SetupImGuiStyle();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize DSEngine
    dse::runtime::EngineRunConfig engine_config;
    engine_config.window_width = 1280;
    engine_config.window_height = 720;
    engine_config.window_title = "DSEngine Editor";
    engine_config.business_mode = BusinessMode::Lua;
    engine_config.enable_editor = true;
    engine_config.startup_lua_script_path = "samples/lua/main.lua";
    
    // We need to set DSE_DATA_ROOT before initializing the engine
    if (std::getenv("DSE_DATA_ROOT") == nullptr) {
#if defined(_WIN32)
        const std::string data_root = (GetProjectRootPath() / "samples" / "lua" / "data").string();
        _putenv_s("DSE_DATA_ROOT", data_root.c_str());
#else
        const std::string data_root = (GetProjectRootPath() / "samples" / "lua" / "data").string();
        setenv("DSE_DATA_ROOT", data_root.c_str(), 1);
#endif
    }

    {
        dse::runtime::EngineInstance engine_instance(engine_config);
        if (!engine_instance.Init()) {
            std::cerr << "Failed to initialize DSEngine in Editor." << std::endl;
            return -1;
        }

        EnsureEditorLocalizationData();

        std::cout << "Engine initialized successfully. Entering main loop..." << std::endl;

        // Main loop
        while (!glfwWindowShouldClose(window)) {
            g_cpu_profiler.BeginFrame();
            g_render_profiler.BeginFrame();
            g_memory_profiler.Reset();

            glfwPollEvents();

            // Tick Engine
            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "EngineTick");
                engine_instance.Tick();
            }

            unsigned int scene_texture = engine_instance.pipeline()->GetSceneTextureId();
            unsigned int game_texture = engine_instance.pipeline()->GetMainTextureId();

            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "EditorMetrics");
                World& profiler_world = engine_instance.pipeline()->world();
                auto& profiler_registry = profiler_world.registry();
                const int entity_count = static_cast<int>(profiler_registry.storage<entt::entity>().size());
                auto sprite_view = profiler_registry.view<SpriteRendererComponent>();
                const int sprite_count = static_cast<int>(std::distance(sprite_view.begin(), sprite_view.end()));

                g_memory_profiler.RecordAlloc("World.Entities", static_cast<size_t>(std::max(entity_count, 0)) * sizeof(entt::entity));
                g_memory_profiler.RecordAlloc("Render.SceneTexture", static_cast<size_t>(1280 * 720 * 4));
                g_memory_profiler.RecordAlloc("Render.GameTexture", static_cast<size_t>(1280 * 720 * 4));
                g_memory_profiler.RecordAlloc("UI.ImGui", static_cast<size_t>(256 * 1024));

                g_render_profiler.RecordSpriteBatch(std::max(sprite_count, 0));
                g_render_profiler.RecordDrawCall(6, 2);
                g_render_profiler.RecordTextureBind();
                g_render_profiler.RecordShaderSwitch();
                g_render_profiler.SetTextureMemory(static_cast<size_t>(1280 * 720 * 4 * 2));
            }

            // Start the Dear ImGui frame
            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "ImGuiFrame");
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
            }

            // Draw Editor UI
            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "DrawEditorUI");
                DrawEditorUI(engine_instance, scene_texture, game_texture);
            }

            // Rendering
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            {
                dse::profiler::ScopedCPUProfile scope(g_cpu_profiler, "ImGuiRender");
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }

            // Update and Render additional Platform Windows
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                GLFWwindow* backup_current_context = glfwGetCurrentContext();
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                glfwMakeContextCurrent(backup_current_context);
            }

            glfwSwapBuffers(window);

            g_render_profiler.EndFrame();
            g_cpu_profiler.EndFrame();
        }

        engine_instance.Shutdown();
    }

    // Cleanup
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::DestroyPlatformWindows();
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
