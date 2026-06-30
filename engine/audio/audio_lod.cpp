/**
 * @file audio_lod.cpp
 * @brief 大世界音频 LOD 系统实现
 */

#include "engine/audio/audio_lod.h"
#include <algorithm>
#include <cmath>

namespace dse {
namespace audio {

void AudioLODSystem::Init(const AudioLODConfig& config) {
    config_ = config;
    initialized_ = true;
}

void AudioLODSystem::Shutdown() {
    sources_.clear();
    initialized_ = false;
}

uint32_t AudioLODSystem::RegisterSource(const std::string& asset_path, const glm::vec3& position,
                                         float max_distance, float priority) {
    uint32_t id = next_source_id_++;
    AudioSourceLOD source;
    source.source_id = id;
    source.asset_path = asset_path;
    source.world_position = position;
    source.max_distance = max_distance;
    source.priority = priority;
    source.current_level = AudioLODLevel::Full;
    sources_[id] = std::move(source);
    return id;
}

void AudioLODSystem::UnregisterSource(uint32_t source_id) {
    sources_.erase(source_id);
}

void AudioLODSystem::UpdateSourcePosition(uint32_t source_id, const glm::vec3& position) {
    auto it = sources_.find(source_id);
    if (it != sources_.end()) {
        it->second.world_position = position;
    }
}

void AudioLODSystem::SetSourceVolume(uint32_t source_id, float volume) {
    auto it = sources_.find(source_id);
    if (it != sources_.end()) {
        it->second.base_volume = volume;
    }
}

void AudioLODSystem::SetSourcePriority(uint32_t source_id, float priority) {
    auto it = sources_.find(source_id);
    if (it != sources_.end()) {
        it->second.priority = priority;
    }
}

void AudioLODSystem::SetSourceAttenuation(uint32_t source_id, AttenuationModel model) {
    auto it = sources_.find(source_id);
    if (it != sources_.end()) {
        it->second.attenuation = model;
    }
}

void AudioLODSystem::SetSourceMaxDistance(uint32_t source_id, float distance) {
    auto it = sources_.find(source_id);
    if (it != sources_.end()) {
        it->second.max_distance = distance;
    }
}

void AudioLODSystem::Tick(const glm::vec3& listener_position, const glm::vec3& listener_forward,
                           float delta_time) {
    if (!initialized_) return;
    (void)listener_forward; // Reserved for directional attenuation

    time_since_update_ += delta_time;
    if (time_since_update_ < config_.update_interval) return;
    time_since_update_ = 0.0f;
    last_listener_pos_ = listener_position;

    for (auto& [id, source] : sources_) {
        float distance = glm::length(listener_position - source.world_position);
        source.current_distance = distance;

        // Compute distance attenuation
        float attenuation = ComputeAttenuation(source, distance);
        source.current_volume = source.base_volume * attenuation;

        // Evaluate LOD level
        AudioLODLevel desired = EvaluateLevel(distance, source.current_volume);

        // Apply hysteresis for upgrading to higher quality
        if (desired < source.current_level) {
            float threshold = 0.0f;
            switch (desired) {
                case AudioLODLevel::Full:
                    threshold = config_.full_distance / config_.hysteresis_factor;
                    break;
                case AudioLODLevel::Reduced:
                    threshold = config_.reduced_distance / config_.hysteresis_factor;
                    break;
                case AudioLODLevel::Virtual:
                    threshold = config_.virtual_distance / config_.hysteresis_factor;
                    break;
                default: break;
            }
            if (distance > threshold) {
                desired = source.current_level;
            }
        }

        source.current_level = desired;
    }

    EnforceActiveLimits();
}

float AudioLODSystem::ComputeAttenuation(const AudioSourceLOD& source, float distance) const {
    if (distance <= source.reference_distance) return 1.0f;
    if (distance >= source.max_distance) return 0.0f;

    float range = source.max_distance - source.reference_distance;
    float normalized = (distance - source.reference_distance) / range;

    switch (source.attenuation) {
        case AttenuationModel::Linear:
            return 1.0f - normalized;
        case AttenuationModel::Logarithmic:
            return 1.0f - std::log2(1.0f + normalized) / std::log2(2.0f);
        case AttenuationModel::InverseSquare: {
            float ratio = source.reference_distance / distance;
            return ratio * ratio;
        }
        case AttenuationModel::Custom:
            return 1.0f - normalized; // Fallback to linear
    }
    return 0.0f;
}

AudioLODLevel AudioLODSystem::EvaluateLevel(float distance, float effective_volume) const {
    // If volume is below audible threshold, cull
    if (effective_volume < config_.min_audible_volume) {
        return AudioLODLevel::Culled;
    }

    if (distance <= config_.full_distance) return AudioLODLevel::Full;
    if (distance <= config_.reduced_distance) return AudioLODLevel::Reduced;
    if (distance <= config_.virtual_distance) return AudioLODLevel::Virtual;
    return AudioLODLevel::Culled;
}

void AudioLODSystem::EnforceActiveLimits() {
    // Count active sources (Full + Reduced)
    std::vector<std::pair<uint32_t, float>> active_sources; // (id, priority_score)

    for (auto& [id, source] : sources_) {
        if (source.current_level == AudioLODLevel::Full ||
            source.current_level == AudioLODLevel::Reduced) {
            // Priority score: higher priority + closer distance = higher score
            float score = source.priority + (1.0f / (source.current_distance + 1.0f));
            active_sources.push_back({id, score});
        }
    }

    // If over limit, virtualize lowest priority sources
    if (active_sources.size() > config_.max_active_sources) {
        std::sort(active_sources.begin(), active_sources.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        for (size_t i = config_.max_active_sources; i < active_sources.size(); ++i) {
            auto it = sources_.find(active_sources[i].first);
            if (it != sources_.end()) {
                it->second.current_level = AudioLODLevel::Virtual;
            }
        }
    }

    // Also enforce max full sources
    std::vector<std::pair<uint32_t, float>> full_sources;
    for (auto& [id, source] : sources_) {
        if (source.current_level == AudioLODLevel::Full) {
            float score = source.priority + (1.0f / (source.current_distance + 1.0f));
            full_sources.push_back({id, score});
        }
    }

    if (full_sources.size() > config_.max_full_sources) {
        std::sort(full_sources.begin(), full_sources.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        for (size_t i = config_.max_full_sources; i < full_sources.size(); ++i) {
            auto it = sources_.find(full_sources[i].first);
            if (it != sources_.end()) {
                it->second.current_level = AudioLODLevel::Reduced;
            }
        }
    }
}

AudioLODLevel AudioLODSystem::GetSourceLevel(uint32_t source_id) const {
    auto it = sources_.find(source_id);
    return (it != sources_.end()) ? it->second.current_level : AudioLODLevel::Culled;
}

float AudioLODSystem::GetSourceEffectiveVolume(uint32_t source_id) const {
    auto it = sources_.find(source_id);
    return (it != sources_.end()) ? it->second.current_volume : 0.0f;
}

bool AudioLODSystem::IsSourceAudible(uint32_t source_id) const {
    auto it = sources_.find(source_id);
    if (it == sources_.end()) return false;
    return it->second.current_level == AudioLODLevel::Full ||
           it->second.current_level == AudioLODLevel::Reduced;
}

uint32_t AudioLODSystem::GetRegisteredSourceCount() const {
    return static_cast<uint32_t>(sources_.size());
}

uint32_t AudioLODSystem::GetActiveSourceCount() const {
    uint32_t count = 0;
    for (const auto& [id, source] : sources_) {
        if (source.current_level == AudioLODLevel::Full ||
            source.current_level == AudioLODLevel::Reduced) {
            ++count;
        }
    }
    return count;
}

uint32_t AudioLODSystem::GetVirtualSourceCount() const {
    uint32_t count = 0;
    for (const auto& [id, source] : sources_) {
        if (source.current_level == AudioLODLevel::Virtual) ++count;
    }
    return count;
}

uint32_t AudioLODSystem::GetCulledSourceCount() const {
    uint32_t count = 0;
    for (const auto& [id, source] : sources_) {
        if (source.current_level == AudioLODLevel::Culled) ++count;
    }
    return count;
}

AudioLODSystem::LODStats AudioLODSystem::GetStats() const {
    LODStats stats{};
    for (const auto& [id, source] : sources_) {
        switch (source.current_level) {
            case AudioLODLevel::Full: ++stats.full; break;
            case AudioLODLevel::Reduced: ++stats.reduced; break;
            case AudioLODLevel::Virtual: ++stats.virtual_count; break;
            case AudioLODLevel::Culled: ++stats.culled; break;
        }
    }
    return stats;
}

void AudioLODSystem::RebaseOrigin(const glm::vec3& offset) {
    for (auto& [id, source] : sources_) {
        source.world_position -= offset;
    }
}

} // namespace audio
} // namespace dse
