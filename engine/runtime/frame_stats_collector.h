/**
 * @file frame_stats_collector.h
 * @brief Collects and reports per-frame timing and render statistics.
 *        Extracted from FramePipeline to separate stats accounting concerns.
 */

#ifndef DSE_FRAME_STATS_COLLECTOR_H
#define DSE_FRAME_STATS_COLLECTOR_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <mutex>

/**
 * @class SegmentTimingStats
 * @brief Thread-safe accumulator for one pipeline segment's per-frame CPU time.
 *
 * Records milliseconds per frame and reports avg / min / max plus P50/P95/P99
 * over a rolling window. Kept thread-safe because Execute-phase segments are
 * recorded on the render thread while the snapshot is read on the main thread
 * (see RENDER_PREP_DOUBLE_BUFFER_DESIGN.md Phase 0).
 */
class SegmentTimingStats {
public:
    struct Snapshot {
        float avg_ms = 0.0f;
        float min_ms = 0.0f;
        float max_ms = 0.0f;
        float p50_ms = 0.0f;
        float p95_ms = 0.0f;
        float p99_ms = 0.0f;
        int count = 0;
    };

    void Record(float ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        sum_ms_ += ms;
        ++count_;
        if (ms < min_ms_) min_ms_ = ms;
        if (ms > max_ms_) max_ms_ = ms;
        ring_[ring_pos_ % kRingCap] = ms;
        ++ring_pos_;
    }

    Snapshot GetSnapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        Snapshot s;
        s.count = count_;
        s.avg_ms = count_ > 0 ? static_cast<float>(sum_ms_ / static_cast<double>(count_)) : 0.0f;
        s.min_ms = count_ > 0 ? min_ms_ : 0.0f;
        s.max_ms = max_ms_;
        const std::size_t n = ring_pos_ < kRingCap ? ring_pos_ : kRingCap;
        if (n > 0) {
            std::array<float, kRingCap> sorted{};
            std::copy_n(ring_.begin(), n, sorted.begin());
            std::sort(sorted.begin(), sorted.begin() + n);
            const auto pick = [&](double p) {
                std::size_t idx = static_cast<std::size_t>(p * static_cast<double>(n - 1) + 0.5);
                if (idx >= n) idx = n - 1;
                return sorted[idx];
            };
            s.p50_ms = pick(0.50);
            s.p95_ms = pick(0.95);
            s.p99_ms = pick(0.99);
        }
        return s;
    }

    void Reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        sum_ms_ = 0.0;
        count_ = 0;
        min_ms_ = 1e9f;
        max_ms_ = 0.0f;
        ring_pos_ = 0;
    }

private:
    static constexpr std::size_t kRingCap = 256;
    mutable std::mutex mutex_;
    double sum_ms_ = 0.0;
    int count_ = 0;
    float min_ms_ = 1e9f;
    float max_ms_ = 0.0f;
    std::array<float, kRingCap> ring_{};
    std::size_t ring_pos_ = 0;
};

/**
 * @class FrameStatsCollector
 * @brief Tracks per-frame timing (update/fixed/render), frame counters, and
 *        periodic stats accumulation for logging.
 *
 * FramePipeline delegates timing measurement and counter storage to this class.
 * The actual stats *logging* (which needs World/RHI/AssetManager access) stays
 * in FramePipeline::CollectRuntimeStats(), using data exposed by this class.
 */
class FrameStatsCollector {
public:
    /// Record the duration of one Update phase (milliseconds).
    void RecordUpdate(float ms) {
        update_time_ms_ += ms;
        ++update_samples_;
    }

    /// Record the duration of one FixedUpdate phase (milliseconds).
    void RecordFixed(float ms) {
        fixed_time_ms_ += ms;
        ++fixed_samples_;
    }

    /// Record the duration of one Render phase (milliseconds).
    void RecordRender(float ms) {
        render_time_ms_ += ms;
        ++render_samples_;
    }

    // ── Pipeline segment timings (Phase 0 measurement) ──
    // Update/Prepare/Wait recorded on the main thread; Execute/Gpu recorded on
    // the render thread when DSE_RENDER_THREAD=1. All accesses are mutex-guarded.
    void RecordUpdateSegment(float ms) { update_seg_.Record(ms); }
    void RecordPrepareSegment(float ms) { prepare_seg_.Record(ms); }
    void RecordWaitSegment(float ms) { wait_seg_.Record(ms); }
    void RecordExecuteSegment(float ms) { execute_seg_.Record(ms); }
    void RecordGpuSegment(float ms) { gpu_seg_.Record(ms); }

    SegmentTimingStats::Snapshot UpdateSegment() const { return update_seg_.GetSnapshot(); }
    SegmentTimingStats::Snapshot PrepareSegment() const { return prepare_seg_.GetSnapshot(); }
    SegmentTimingStats::Snapshot WaitSegment() const { return wait_seg_.GetSnapshot(); }
    SegmentTimingStats::Snapshot ExecuteSegment() const { return execute_seg_.GetSnapshot(); }
    SegmentTimingStats::Snapshot GpuSegment() const { return gpu_seg_.GetSnapshot(); }

    /// Accumulate delta time for the periodic (1-second) stats window.
    void AccumulateStatsTimer(float dt) { stats_accumulator_ += dt; }

    /// @return True when the 1-second stats window has elapsed.
    bool StatsWindowElapsed() const { return stats_accumulator_ >= 1.0f; }

    /// Reset the stats accumulator and all timing/sample counters.
    void ResetAccumulators() {
        stats_accumulator_ = 0.0f;
        update_time_ms_ = 0.0f;
        fixed_time_ms_ = 0.0f;
        render_time_ms_ = 0.0f;
        update_samples_ = 0;
        fixed_samples_ = 0;
        render_samples_ = 0;
        update_seg_.Reset();
        prepare_seg_.Reset();
        wait_seg_.Reset();
        execute_seg_.Reset();
        gpu_seg_.Reset();
    }

    // ── Averages (for periodic logging) ──
    float AvgUpdateMs() const {
        return update_samples_ > 0 ? update_time_ms_ / static_cast<float>(update_samples_) : 0.0f;
    }
    float AvgFixedMs() const {
        return fixed_samples_ > 0 ? fixed_time_ms_ / static_cast<float>(fixed_samples_) : 0.0f;
    }
    float AvgRenderMs() const {
        return render_samples_ > 0 ? render_time_ms_ / static_cast<float>(render_samples_) : 0.0f;
    }

    // ── Per-frame render counters (set by FinalizeRuntimeRenderFrame) ──
    void SetLastFrameStats(int draw_calls, int material_switches,
                           int max_batch_sprites, int sprite_count) {
        last_draw_calls_ = draw_calls;
        last_material_switches_ = material_switches;
        last_max_batch_sprites_ = max_batch_sprites;
        last_sprite_count_ = sprite_count;
    }

    void SetGpuDrivenStats(int active, int indirect_draws, int total_instances) {
        last_gpu_driven_active_ = active;
        last_gpu_indirect_draw_count_ = indirect_draws;
        last_gpu_total_instances_ = total_instances;
    }

    // ── Getters ──
    int LastDrawCalls() const { return last_draw_calls_; }
    int LastMaterialSwitches() const { return last_material_switches_; }
    int LastMaxBatchSprites() const { return last_max_batch_sprites_; }
    int LastSpriteCount() const { return last_sprite_count_; }
    int LastGpuDrivenActive() const { return last_gpu_driven_active_; }
    int LastGpuIndirectDrawCount() const { return last_gpu_indirect_draw_count_; }
    int LastGpuTotalInstances() const { return last_gpu_total_instances_; }

private:
    // Timing accumulators (reset every stats window)
    float stats_accumulator_ = 0.0f;
    float update_time_ms_ = 0.0f;
    float fixed_time_ms_ = 0.0f;
    float render_time_ms_ = 0.0f;
    int update_samples_ = 0;
    int fixed_samples_ = 0;
    int render_samples_ = 0;

    // Pipeline segment timing with rolling percentiles (Phase 0 measurement)
    SegmentTimingStats update_seg_;
    SegmentTimingStats prepare_seg_;
    SegmentTimingStats wait_seg_;
    SegmentTimingStats execute_seg_;
    SegmentTimingStats gpu_seg_;

    // Per-frame render counters (updated each frame)
    int last_draw_calls_ = 0;
    int last_material_switches_ = 0;
    int last_max_batch_sprites_ = 0;
    int last_sprite_count_ = 0;
    int last_gpu_driven_active_ = 0;
    int last_gpu_indirect_draw_count_ = 0;
    int last_gpu_total_instances_ = 0;
};

#endif // DSE_FRAME_STATS_COLLECTOR_H
