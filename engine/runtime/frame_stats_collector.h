/**
 * @file frame_stats_collector.h
 * @brief Collects and reports per-frame timing and render statistics.
 *        Extracted from FramePipeline to separate stats accounting concerns.
 */

#ifndef DSE_FRAME_STATS_COLLECTOR_H
#define DSE_FRAME_STATS_COLLECTOR_H

#include <cstddef>

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
