#include "editor_profiler_panel.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "imgui.h"

namespace dse::editor {
namespace {

constexpr int kProfilerHistoryMaxSamples = 180;

std::vector<float>& CPUFrameHistory() {
    static std::vector<float> history;
    return history;
}

std::vector<float>& FPSHistory() {
    static std::vector<float> history;
    return history;
}

std::vector<float>& RenderDrawCallHistory() {
    static std::vector<float> history;
    return history;
}

std::vector<float>& MemoryUsageHistory() {
    static std::vector<float> history;
    return history;
}

int& LastProfiledFrame() {
    static int frame = -1;
    return frame;
}

std::string& ProfilerExportStatus() {
    static std::string status;
    return status;
}

std::filesystem::path GetProjectRootPath() {
    return std::filesystem::current_path().lexically_normal();
}

std::filesystem::path GetEditorBinPath() {
    const std::filesystem::path path = GetProjectRootPath() / "bin";
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path GetEditorExportDirectory() {
    const std::filesystem::path path = GetEditorBinPath() / "editor_exports";
    std::filesystem::create_directories(path);
    return path;
}

void ExportTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::trunc);
    if (ofs.is_open()) {
        ofs << content;
    }
}

void PushHistorySample(std::vector<float>& history, float value) {
    history.push_back(value);
    if (static_cast<int>(history.size()) > kProfilerHistoryMaxSamples) {
        history.erase(history.begin(), history.begin() + (history.size() - kProfilerHistoryMaxSamples));
    }
}

} // namespace

void DrawProfilerPanel(EditorProfilerContext& context) {
    auto& g_cpu_profiler = context.cpu_profiler;
    auto& g_memory_profiler = context.memory_profiler;
    auto& g_render_profiler = context.render_profiler;
    auto& g_cpu_frame_history = CPUFrameHistory();
    auto& g_fps_history = FPSHistory();
    auto& g_render_draw_call_history = RenderDrawCallHistory();
    auto& g_memory_usage_history = MemoryUsageHistory();
    auto& g_last_profiled_frame = LastProfiledFrame();
    auto& g_profiler_export_status = ProfilerExportStatus();

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

} // namespace dse::editor
