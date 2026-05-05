/**
 * @file render_profiler.cpp
 * @brief 渲染性能分析器实现
 */

#include "engine/profiler/render_profiler.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace dse {
namespace profiler {

void RenderProfiler::BeginFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    last_frame_ = current_frame_;
    current_frame_ = RenderFrameStats{};
}

void RenderProfiler::EndFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    accumulated_.frame_count++;
    accumulated_.total_draw_calls += current_frame_.draw_calls;
    accumulated_.total_triangles += current_frame_.triangle_count;
    accumulated_.total_vertices += current_frame_.vertex_count;
    
    accumulated_.avg_draw_calls = static_cast<double>(accumulated_.total_draw_calls) / accumulated_.frame_count;
    accumulated_.avg_triangles = static_cast<double>(accumulated_.total_triangles) / accumulated_.frame_count;
    accumulated_.avg_vertices = static_cast<double>(accumulated_.total_vertices) / accumulated_.frame_count;
    
    accumulated_.peak_draw_calls = std::max(accumulated_.peak_draw_calls, current_frame_.draw_calls);
    accumulated_.peak_triangles = std::max(accumulated_.peak_triangles, current_frame_.triangle_count);
    accumulated_.peak_vertices = std::max(accumulated_.peak_vertices, current_frame_.vertex_count);

    RenderFrameEvent evt;
    evt.timestamp_us = std::chrono::duration<double, std::micro>(
        std::chrono::high_resolution_clock::now() - origin_time_
    ).count();
    evt.stats = current_frame_;
    frame_events_.push_back(evt);
}

void RenderProfiler::RecordDrawCall(int vertex_count, int triangle_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_frame_.draw_calls++;
    current_frame_.vertex_count += vertex_count;
    current_frame_.triangle_count += triangle_count;
}

void RenderProfiler::RecordSpriteBatch(int sprite_count) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_frame_.batch_count++;
    current_frame_.sprite_count += sprite_count;
    current_frame_.draw_calls++;
    current_frame_.vertex_count += sprite_count * 4;
    current_frame_.triangle_count += sprite_count * 2;
}

void RenderProfiler::RecordTextureBind() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_frame_.texture_binds++;
}

void RenderProfiler::RecordShaderSwitch() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_frame_.shader_switches++;
}

void RenderProfiler::SetTextureMemory(size_t bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_frame_.texture_memory = bytes;
}

void RenderProfiler::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    current_frame_ = RenderFrameStats{};
    last_frame_ = RenderFrameStats{};
    accumulated_ = RenderAccumulatedStats{};
    frame_events_.clear();
    origin_time_ = std::chrono::high_resolution_clock::now();
}

std::string RenderProfiler::ExportCSV() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "Metric,Current,Peak,Average\n";
    oss << "DrawCalls," << last_frame_.draw_calls << "," 
        << accumulated_.peak_draw_calls << "," << accumulated_.avg_draw_calls << "\n";
    oss << "Triangles," << last_frame_.triangle_count << "," 
        << accumulated_.peak_triangles << "," << accumulated_.avg_triangles << "\n";
    oss << "Vertices," << last_frame_.vertex_count << "," 
        << accumulated_.peak_vertices << "," << accumulated_.avg_vertices << "\n";
    oss << "Sprites," << last_frame_.sprite_count << ",0,0\n";
    oss << "Batches," << last_frame_.batch_count << ",0,0\n";
    oss << "TextureBinds," << last_frame_.texture_binds << ",0,0\n";
    oss << "ShaderSwitches," << last_frame_.shader_switches << ",0,0\n";
    oss << "TextureMemoryKB," << (last_frame_.texture_memory / 1024) << ",0,0\n";
    return oss.str();
}

std::string RenderProfiler::ExportChromeTrace() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "[\n";
    bool first = true;
    for (const auto& evt : frame_events_) {
        if (!first) oss << ",\n";
        first = false;
        oss << "{\"name\":\"render_stats\""
            << ",\"cat\":\"render\""
            << ",\"ph\":\"C\""
            << ",\"ts\":" << std::fixed << std::setprecision(1) << evt.timestamp_us
            << ",\"pid\":1"
            << ",\"tid\":1"
            << ",\"args\":{"
            << "\"draw_calls\":" << evt.stats.draw_calls
            << ",\"triangles\":" << evt.stats.triangle_count
            << ",\"vertices\":" << evt.stats.vertex_count
            << ",\"sprites\":" << evt.stats.sprite_count
            << ",\"batches\":" << evt.stats.batch_count
            << ",\"texture_binds\":" << evt.stats.texture_binds
            << ",\"shader_switches\":" << evt.stats.shader_switches
            << ",\"texture_memory\":" << evt.stats.texture_memory
            << "}}";
    }
    oss << "\n]";
    return oss.str();
}

} // namespace profiler
} // namespace dse
