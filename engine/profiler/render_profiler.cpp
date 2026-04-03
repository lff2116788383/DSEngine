/**
 * @file render_profiler.cpp
 * @brief 渲染性能分析器实现
 */

#include "engine/profiler/render_profiler.h"
#include <sstream>
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

} // namespace profiler
} // namespace dse
