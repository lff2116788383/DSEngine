/**
 * @file dx11_gpu_timer.h
 * @brief D3D11 GPU Timestamp Query 实现
 *
 * 使用 D3D11_QUERY_TIMESTAMP + D3D11_QUERY_TIMESTAMP_DISJOINT 双缓冲策略：
 * - 每帧写入 timestamp query pair
 * - 读取上一帧结果 + disjoint 频率
 */

#ifndef DSE_RENDER_DX11_GPU_TIMER_H
#define DSE_RENDER_DX11_GPU_TIMER_H

#include "engine/render/rhi/rhi_gpu_timer.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace dse {
namespace render {

using Microsoft::WRL::ComPtr;

class DX11Context;

class DX11GpuTimer {
public:
    DX11GpuTimer() = default;
    ~DX11GpuTimer();

    /// 初始化（需要 D3D11 device）
    void Init(DX11Context* context);
    void Shutdown();

    bool SupportsGpuTimer() const { return initialized_; }
    GpuTimerId GetOrCreateGpuTimer(const std::string& name);
    void BeginGpuTimer(GpuTimerId id);
    void EndGpuTimer(GpuTimerId id);
    float GetGpuTimerResultMs(GpuTimerId id) const;
    void ResetGpuTimers();
    void ResolveGpuTimers();
    std::vector<IRhiGpuTimer::GpuTimerEntry> GetAllGpuTimerResults() const;

private:
    static constexpr int kFrameCount = 2;

    struct TimerSlot {
        std::string name;
        ComPtr<ID3D11Query> begin_query[kFrameCount];
        ComPtr<ID3D11Query> end_query[kFrameCount];
        float last_result_ms = -1.0f;
    };

    DX11Context* context_ = nullptr;
    ComPtr<ID3D11Query> disjoint_query_[kFrameCount];
    std::vector<TimerSlot> slots_;
    std::unordered_map<std::string, GpuTimerId> name_to_id_;

    int write_frame_ = 0;
    int read_frame_ = 1;
    bool initialized_ = false;
    bool disjoint_issued_[kFrameCount] = {};
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_GPU_TIMER_H
