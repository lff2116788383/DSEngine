/**
 * @file gl_gpu_timer.cpp
 * @brief OpenGL GPU Timestamp Query 实现
 */

#include "engine/render/rhi/opengl/gl_gpu_timer.h"
#include "engine/base/debug.h"

#include <glad/glad.h>

namespace dse {
namespace render {

GLGpuTimer::~GLGpuTimer() {
    Shutdown();
}

void GLGpuTimer::Init() {
    if (initialized_) return;

    if (glQueryCounter == nullptr || glGetQueryObjectui64v == nullptr) {
        DEBUG_LOG_WARN("[GLGpuTimer] GL timer query not supported");
        return;
    }

    initialized_ = true;
    write_frame_ = 0;
    read_frame_ = 1;
    DEBUG_LOG_INFO("[GLGpuTimer] Initialized (double-buffered timestamp queries)");
}

void GLGpuTimer::Shutdown() {
    if (!initialized_) return;
    for (auto& slot : slots_) {
        for (int f = 0; f < kFrameCount; ++f) {
            if (slot.queries[f][0]) glDeleteQueries(2, slot.queries[f]);
        }
    }
    slots_.clear();
    name_to_id_.clear();
    initialized_ = false;
}

GpuTimerId GLGpuTimer::GetOrCreateGpuTimer(const std::string& name) {
    if (!initialized_) return kInvalidGpuTimerId;

    auto it = name_to_id_.find(name);
    if (it != name_to_id_.end()) return it->second;

    GpuTimerId id = static_cast<GpuTimerId>(slots_.size() + 1);
    TimerSlot slot;
    slot.name = name;
    for (int f = 0; f < kFrameCount; ++f) {
        glGenQueries(2, slot.queries[f]);
        // 初始化 query 避免首帧读取未发出的 query
        glQueryCounter(slot.queries[f][0], GL_TIMESTAMP);
        glQueryCounter(slot.queries[f][1], GL_TIMESTAMP);
    }
    slots_.push_back(std::move(slot));
    name_to_id_[name] = id;
    return id;
}

void GLGpuTimer::BeginGpuTimer(GpuTimerId id) {
    if (!initialized_ || id == kInvalidGpuTimerId) return;
    size_t idx = id - 1;
    if (idx >= slots_.size()) return;
    glQueryCounter(slots_[idx].queries[write_frame_][0], GL_TIMESTAMP);
}

void GLGpuTimer::EndGpuTimer(GpuTimerId id) {
    if (!initialized_ || id == kInvalidGpuTimerId) return;
    size_t idx = id - 1;
    if (idx >= slots_.size()) return;
    glQueryCounter(slots_[idx].queries[write_frame_][1], GL_TIMESTAMP);
}

float GLGpuTimer::GetGpuTimerResultMs(GpuTimerId id) const {
    if (!initialized_ || id == kInvalidGpuTimerId) return -1.0f;
    size_t idx = id - 1;
    if (idx >= slots_.size()) return -1.0f;
    return slots_[idx].last_result_ms;
}

void GLGpuTimer::ResetGpuTimers() {
    // 切换帧 index
    write_frame_ = (write_frame_ + 1) % kFrameCount;
    read_frame_ = (read_frame_ + 1) % kFrameCount;
}

void GLGpuTimer::ResolveGpuTimers() {
    if (!initialized_) return;
    for (auto& slot : slots_) {
        GLuint q_begin = slot.queries[read_frame_][0];
        GLuint q_end = slot.queries[read_frame_][1];

        GLint available = GL_FALSE;
        glGetQueryObjectiv(q_end, GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available) {
            slot.last_result_ms = -1.0f;
            continue;
        }

        GLuint64 ts_begin = 0, ts_end = 0;
        glGetQueryObjectui64v(q_begin, GL_QUERY_RESULT, &ts_begin);
        glGetQueryObjectui64v(q_end, GL_QUERY_RESULT, &ts_end);

        // 纳秒 → 毫秒
        slot.last_result_ms = static_cast<float>(ts_end - ts_begin) / 1'000'000.0f;
    }
}

std::vector<IRhiGpuTimer::GpuTimerEntry> GLGpuTimer::GetAllGpuTimerResults() const {
    std::vector<IRhiGpuTimer::GpuTimerEntry> results;
    results.reserve(slots_.size());
    for (const auto& slot : slots_) {
        results.push_back({slot.name, slot.last_result_ms});
    }
    return results;
}

} // namespace render
} // namespace dse
