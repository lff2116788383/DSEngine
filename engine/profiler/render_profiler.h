#ifndef DSE_RENDER_PROFILER_H
#define DSE_RENDER_PROFILER_H

#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dse {
namespace profiler {

struct RenderFrameStats {
    int draw_calls = 0;
    int triangle_count = 0;
    int vertex_count = 0;
    int sprite_count = 0;
    int batch_count = 0;
    size_t texture_memory = 0;
    int texture_binds = 0;
    int shader_switches = 0;
    int instanced_draw_calls = 0;
    int instanced_mesh_count = 0;
    int indirect_draw_calls = 0;
    int gpu_culled_count = 0;
};

struct RenderAccumulatedStats {
    double avg_draw_calls = 0.0;
    double avg_triangles = 0.0;
    double avg_vertices = 0.0;
    int peak_draw_calls = 0;
    int peak_triangles = 0;
    int peak_vertices = 0;
    int frame_count = 0;
    long long total_draw_calls = 0;
    long long total_triangles = 0;
    long long total_vertices = 0;
};

struct RenderFrameEvent {
    double timestamp_us = 0.0;
    RenderFrameStats stats;
};

class RenderProfiler {
public:
    RenderProfiler() = default;
    ~RenderProfiler() = default;

    void BeginFrame();
    void EndFrame();
    void RecordDrawCall(int vertex_count, int triangle_count);
    void RecordSpriteBatch(int sprite_count);
    void RecordTextureBind();
    void RecordShaderSwitch();
    void SetTextureMemory(size_t bytes);

    /// 从 RHI 帧统计批量写入，单次加锁 O(1)
    void UpdateFromRhi(int draw_calls, int vertex_count, int triangle_count,
                       int sprite_count, int texture_binds, int shader_switches);
    const RenderFrameStats& GetCurrentFrameStats() const { return current_frame_; }
    const RenderAccumulatedStats& GetAccumulatedStats() const { return accumulated_; }
    void Reset();
    std::string ExportCSV() const;
    std::string ExportChromeTrace() const;

    static constexpr size_t kMaxFrameEvents = 3600; // ~60s at 60fps

private:
    RenderFrameStats current_frame_;
    RenderFrameStats last_frame_;
    RenderAccumulatedStats accumulated_;
    std::deque<RenderFrameEvent> frame_events_;
    std::chrono::high_resolution_clock::time_point origin_time_ = std::chrono::high_resolution_clock::now();
    mutable std::mutex mutex_;
};

} // namespace profiler
} // namespace dse

#endif // DSE_RENDER_PROFILER_H
