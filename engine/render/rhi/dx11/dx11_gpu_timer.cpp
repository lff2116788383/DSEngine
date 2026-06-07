/**
 * @file dx11_gpu_timer.cpp
 * @brief D3D11 GPU Timestamp Query 实现
 */

#include "engine/render/rhi/dx11/dx11_gpu_timer.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/base/debug.h"

namespace dse {
namespace render {

DX11GpuTimer::~DX11GpuTimer() {
    Shutdown();
}

void DX11GpuTimer::Init(DX11Context* context) {
    if (initialized_ || !context) return;
    context_ = context;

    ID3D11Device* device = context_->device();
    if (!device) return;

    // 创建双缓冲 disjoint query
    for (int f = 0; f < kFrameCount; ++f) {
        D3D11_QUERY_DESC desc{};
        desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        if (FAILED(device->CreateQuery(&desc, &disjoint_query_[f]))) {
            DEBUG_LOG_WARN("[DX11GpuTimer] Failed to create disjoint query");
            return;
        }
        disjoint_issued_[f] = false;
    }

    initialized_ = true;
    write_frame_ = 0;
    read_frame_ = 1;
    DEBUG_LOG_INFO("[DX11GpuTimer] Initialized (double-buffered timestamp queries)");
}

void DX11GpuTimer::Shutdown() {
    if (!initialized_) return;
    slots_.clear();
    name_to_id_.clear();
    for (int f = 0; f < kFrameCount; ++f) {
        disjoint_query_[f].Reset();
    }
    initialized_ = false;
}

GpuTimerId DX11GpuTimer::GetOrCreateGpuTimer(const std::string& name) {
    if (!initialized_) return kInvalidGpuTimerId;

    auto it = name_to_id_.find(name);
    if (it != name_to_id_.end()) return it->second;

    ID3D11Device* device = context_->device();
    if (!device) return kInvalidGpuTimerId;

    GpuTimerId id = static_cast<GpuTimerId>(slots_.size() + 1);
    TimerSlot slot;
    slot.name = name;

    for (int f = 0; f < kFrameCount; ++f) {
        D3D11_QUERY_DESC desc{};
        desc.Query = D3D11_QUERY_TIMESTAMP;
        if (FAILED(device->CreateQuery(&desc, &slot.begin_query[f])) ||
            FAILED(device->CreateQuery(&desc, &slot.end_query[f]))) {
            DEBUG_LOG_WARN("[DX11GpuTimer] Failed to create timestamp query for '{}'", name);
            return kInvalidGpuTimerId;
        }
    }

    slots_.push_back(std::move(slot));
    name_to_id_[name] = id;
    return id;
}

void DX11GpuTimer::BeginGpuTimer(GpuTimerId id) {
    if (!initialized_ || id == kInvalidGpuTimerId) return;
    size_t idx = id - 1;
    if (idx >= slots_.size()) return;

    ID3D11DeviceContext* dc = context_->device_context();
    if (!dc) return;

    dc->End(slots_[idx].begin_query[write_frame_].Get());
}

void DX11GpuTimer::EndGpuTimer(GpuTimerId id) {
    if (!initialized_ || id == kInvalidGpuTimerId) return;
    size_t idx = id - 1;
    if (idx >= slots_.size()) return;

    ID3D11DeviceContext* dc = context_->device_context();
    if (!dc) return;

    dc->End(slots_[idx].end_query[write_frame_].Get());
}

float DX11GpuTimer::GetGpuTimerResultMs(GpuTimerId id) const {
    if (!initialized_ || id == kInvalidGpuTimerId) return -1.0f;
    size_t idx = id - 1;
    if (idx >= slots_.size()) return -1.0f;
    return slots_[idx].last_result_ms;
}

void DX11GpuTimer::ResetGpuTimers() {
    if (!initialized_) return;

    // 切换帧 index
    write_frame_ = (write_frame_ + 1) % kFrameCount;
    read_frame_ = (read_frame_ + 1) % kFrameCount;

    // 开始本帧 disjoint query
    ID3D11DeviceContext* dc = context_->device_context();
    if (dc) {
        dc->Begin(disjoint_query_[write_frame_].Get());
        disjoint_issued_[write_frame_] = true;
    }
}

void DX11GpuTimer::ResolveGpuTimers() {
    if (!initialized_) return;

    ID3D11DeviceContext* dc = context_->device_context();
    if (!dc) return;

    // 结束本帧 disjoint query
    dc->End(disjoint_query_[write_frame_].Get());

    // 读取上一帧结果
    if (!disjoint_issued_[read_frame_]) return;

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint_data{};
    HRESULT hr = dc->GetData(disjoint_query_[read_frame_].Get(),
                             &disjoint_data, sizeof(disjoint_data), D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (hr != S_OK || disjoint_data.Disjoint) {
        // 数据尚未就绪或时钟不可靠
        for (auto& slot : slots_) slot.last_result_ms = -1.0f;
        return;
    }

    for (auto& slot : slots_) {
        UINT64 ts_begin = 0, ts_end = 0;
        HRESULT hr_begin = dc->GetData(slot.begin_query[read_frame_].Get(),
                                       &ts_begin, sizeof(ts_begin), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        HRESULT hr_end = dc->GetData(slot.end_query[read_frame_].Get(),
                                     &ts_end, sizeof(ts_end), D3D11_ASYNC_GETDATA_DONOTFLUSH);

        if (hr_begin == S_OK && hr_end == S_OK && ts_end >= ts_begin) {
            // ticks → 秒 → 毫秒
            double seconds = static_cast<double>(ts_end - ts_begin) /
                             static_cast<double>(disjoint_data.Frequency);
            slot.last_result_ms = static_cast<float>(seconds * 1000.0);
        } else {
            slot.last_result_ms = -1.0f;
        }
    }
}

std::vector<IRhiGpuTimer::GpuTimerEntry> DX11GpuTimer::GetAllGpuTimerResults() const {
    std::vector<IRhiGpuTimer::GpuTimerEntry> results;
    results.reserve(slots_.size());
    for (const auto& slot : slots_) {
        results.push_back({slot.name, slot.last_result_ms});
    }
    return results;
}

} // namespace render
} // namespace dse
