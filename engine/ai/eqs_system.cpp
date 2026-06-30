/**
 * @file eqs_system.cpp
 * @brief 环境查询系统实现
 */

#include "engine/ai/eqs_system.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>

namespace dse {
namespace ai {

static constexpr float PI = 3.14159265358979323846f;

void EQSSystem::Init() {
    initialized_ = true;
}

void EQSSystem::Shutdown() {
    templates_.clear();
    template_ids_.clear();
    custom_scorers_.clear();
    next_id_ = 0;
    initialized_ = false;
}

uint32_t EQSSystem::CreateTemplate(const std::string& name) {
    uint32_t id = next_id_++;
    QueryTemplate tmpl;
    tmpl.name = name;
    templates_.push_back(tmpl);
    template_ids_.push_back(id);
    return id;
}

void EQSSystem::DestroyTemplate(uint32_t template_id) {
    for (size_t i = 0; i < template_ids_.size(); ++i) {
        if (template_ids_[i] == template_id) {
            templates_.erase(templates_.begin() + i);
            template_ids_.erase(template_ids_.begin() + i);
            return;
        }
    }
}

void EQSSystem::SetGenerator(uint32_t template_id, const GeneratorConfig& config) {
    for (size_t i = 0; i < template_ids_.size(); ++i) {
        if (template_ids_[i] == template_id) {
            templates_[i].generator = config;
            return;
        }
    }
}

void EQSSystem::AddScorer(uint32_t template_id, const ScorerConfig& config) {
    for (size_t i = 0; i < template_ids_.size(); ++i) {
        if (template_ids_[i] == template_id) {
            templates_[i].scorers.push_back(config);
            return;
        }
    }
}

void EQSSystem::ClearScorers(uint32_t template_id) {
    for (size_t i = 0; i < template_ids_.size(); ++i) {
        if (template_ids_[i] == template_id) {
            templates_[i].scorers.clear();
            return;
        }
    }
}

void EQSSystem::SetCombineMode(uint32_t template_id, CombineMode mode) {
    for (size_t i = 0; i < template_ids_.size(); ++i) {
        if (template_ids_[i] == template_id) {
            templates_[i].combine_mode = mode;
            return;
        }
    }
}

void EQSSystem::SetMaxResults(uint32_t template_id, uint32_t max_results) {
    for (size_t i = 0; i < template_ids_.size(); ++i) {
        if (template_ids_[i] == template_id) {
            templates_[i].max_results = max_results;
            return;
        }
    }
}

uint32_t EQSSystem::RegisterCustomScorer(const std::string& name, CustomScorerFunc func) {
    uint32_t id = static_cast<uint32_t>(custom_scorers_.size());
    custom_scorers_.emplace_back(name, std::move(func));
    return id;
}

void EQSSystem::SetCustomScorerForTemplate(uint32_t template_id, uint32_t scorer_index, uint32_t custom_id) {
    (void)template_id; (void)scorer_index; (void)custom_id;
}

std::vector<QueryCandidate> EQSSystem::GeneratePoints(const GeneratorConfig& config,
                                                       const glm::vec3& querier_pos) const {
    std::vector<QueryCandidate> candidates;
    glm::vec3 center = (glm::length(config.center) > 0.001f) ? config.center : querier_pos;

    switch (config.type) {
        case GeneratorType::Grid: {
            float half = config.radius;
            int count_per_axis = static_cast<int>(2.0f * half / config.spacing) + 1;
            for (int z = 0; z < count_per_axis && static_cast<int>(candidates.size()) < config.max_points; ++z) {
                for (int x = 0; x < count_per_axis && static_cast<int>(candidates.size()) < config.max_points; ++x) {
                    float px = center.x - half + x * config.spacing;
                    float pz = center.z - half + z * config.spacing;
                    float dist = std::sqrt((px - center.x) * (px - center.x) + (pz - center.z) * (pz - center.z));
                    if (dist > config.radius || dist < config.inner_radius) continue;

                    QueryCandidate c;
                    c.position = glm::vec3(px, center.y + config.height_offset, pz);
                    if (height_func_) c.position.y = height_func_(px, pz) + config.height_offset;
                    candidates.push_back(c);
                }
            }
            break;
        }
        case GeneratorType::Ring: {
            float circumference = 2.0f * PI * config.radius;
            int point_count = std::min(config.max_points, static_cast<int>(circumference / config.spacing));
            for (int i = 0; i < point_count; ++i) {
                float angle = 2.0f * PI * i / point_count;
                float px = center.x + std::cos(angle) * config.radius;
                float pz = center.z + std::sin(angle) * config.radius;

                QueryCandidate c;
                c.position = glm::vec3(px, center.y + config.height_offset, pz);
                if (height_func_) c.position.y = height_func_(px, pz) + config.height_offset;
                candidates.push_back(c);
            }
            break;
        }
        case GeneratorType::Cone: {
            float half_angle = glm::radians(config.cone_angle * 0.5f);
            float base_angle = std::atan2(config.direction.z, config.direction.x);
            int rings = static_cast<int>(config.radius / config.spacing);

            for (int r = 1; r <= rings && static_cast<int>(candidates.size()) < config.max_points; ++r) {
                float dist = r * config.spacing;
                int points_ring = std::max(4, static_cast<int>(dist * half_angle * 2 / config.spacing));
                for (int i = 0; i < points_ring && static_cast<int>(candidates.size()) < config.max_points; ++i) {
                    float angle = base_angle - half_angle + (2.0f * half_angle * i / (points_ring - 1));
                    float px = center.x + std::cos(angle) * dist;
                    float pz = center.z + std::sin(angle) * dist;

                    QueryCandidate c;
                    c.position = glm::vec3(px, center.y + config.height_offset, pz);
                    if (height_func_) c.position.y = height_func_(px, pz) + config.height_offset;
                    candidates.push_back(c);
                }
            }
            break;
        }
        case GeneratorType::Random: {
            std::mt19937 rng(42);
            std::uniform_real_distribution<float> dist_angle(0.0f, 2.0f * PI);
            std::uniform_real_distribution<float> dist_r(config.inner_radius, config.radius);

            for (int i = 0; i < config.max_points; ++i) {
                float angle = dist_angle(rng);
                float r = std::sqrt(dist_r(rng) / config.radius) * config.radius;
                float px = center.x + std::cos(angle) * r;
                float pz = center.z + std::sin(angle) * r;

                QueryCandidate c;
                c.position = glm::vec3(px, center.y + config.height_offset, pz);
                if (height_func_) c.position.y = height_func_(px, pz) + config.height_offset;
                candidates.push_back(c);
            }
            break;
        }
        default:
            break;
    }

    return candidates;
}

float EQSSystem::ScoreCandidate(const QueryCandidate& candidate, const ScorerConfig& scorer,
                                 const glm::vec3& querier_pos) const {
    float score = 0.0f;

    switch (scorer.type) {
        case ScorerType::Distance: {
            glm::vec3 ref = (glm::length(scorer.reference_point) > 0.001f)
                            ? scorer.reference_point : querier_pos;
            float dist = glm::length(candidate.position - ref);
            score = 1.0f - std::min(dist / scorer.max_value, 1.0f);
            break;
        }
        case ScorerType::Visibility: {
            if (visibility_func_) {
                bool visible = visibility_func_(querier_pos, candidate.position);
                score = visible ? 0.0f : 1.0f; // Hidden = good cover
            }
            break;
        }
        case ScorerType::DotProduct: {
            glm::vec3 to_candidate = glm::normalize(candidate.position - querier_pos);
            glm::vec3 ref_dir = glm::normalize(scorer.reference_dir);
            score = (glm::dot(to_candidate, ref_dir) + 1.0f) * 0.5f; // Remap [-1,1] → [0,1]
            break;
        }
        case ScorerType::Height: {
            score = std::min(candidate.position.y / scorer.max_value, 1.0f);
            break;
        }
        case ScorerType::Reachable: {
            // Simplified: check distance as proxy for reachability
            float dist = glm::length(candidate.position - querier_pos);
            score = (dist < scorer.max_value) ? 1.0f : 0.0f;
            break;
        }
        case ScorerType::Custom: {
            // Custom scorer via registered callback
            uint32_t custom_idx = static_cast<uint32_t>(scorer.min_value);
            if (custom_idx < custom_scorers_.size()) {
                score = custom_scorers_[custom_idx].second(candidate.position, querier_pos);
            }
            break;
        }
    }

    if (scorer.invert) score = 1.0f - score;
    if (score < scorer.min_value && scorer.type == ScorerType::Distance) score = 0.0f;

    return score;
}

QueryResult EQSSystem::Execute(uint32_t template_id, const glm::vec3& querier_pos) const {
    return ExecuteAt(template_id, querier_pos, glm::vec3(0.0f));
}

QueryResult EQSSystem::ExecuteAt(uint32_t template_id, const glm::vec3& querier_pos,
                                  const glm::vec3& custom_center) const {
    QueryResult result{};

    const QueryTemplate* tmpl = nullptr;
    for (size_t i = 0; i < template_ids_.size(); ++i) {
        if (template_ids_[i] == template_id) {
            tmpl = &templates_[i];
            break;
        }
    }
    if (!tmpl) return result;

    auto start = std::chrono::high_resolution_clock::now();

    // Generate points
    GeneratorConfig gen = tmpl->generator;
    if (glm::length(custom_center) > 0.001f) gen.center = custom_center;
    auto candidates = GeneratePoints(gen, querier_pos);
    result.total_generated = static_cast<uint32_t>(candidates.size());

    // Score each candidate
    for (auto& c : candidates) {
        c.individual_scores.resize(tmpl->scorers.size());
        float combined = 0.0f;

        switch (tmpl->combine_mode) {
            case CombineMode::WeightedSum: {
                float total_weight = 0.0f;
                for (size_t s = 0; s < tmpl->scorers.size(); ++s) {
                    float score = ScoreCandidate(c, tmpl->scorers[s], querier_pos);
                    c.individual_scores[s] = score;
                    combined += score * tmpl->scorers[s].weight;
                    total_weight += tmpl->scorers[s].weight;
                }
                if (total_weight > 0.0f) combined /= total_weight;
                break;
            }
            case CombineMode::Multiply: {
                combined = 1.0f;
                for (size_t s = 0; s < tmpl->scorers.size(); ++s) {
                    float score = ScoreCandidate(c, tmpl->scorers[s], querier_pos);
                    c.individual_scores[s] = score;
                    combined *= score;
                }
                break;
            }
            case CombineMode::Minimum: {
                combined = 1.0f;
                for (size_t s = 0; s < tmpl->scorers.size(); ++s) {
                    float score = ScoreCandidate(c, tmpl->scorers[s], querier_pos);
                    c.individual_scores[s] = score;
                    combined = std::min(combined, score);
                }
                break;
            }
        }

        c.score = combined;
        c.valid = (combined > 0.0f);
    }

    // Sort by score
    if (tmpl->sort_descending) {
        std::sort(candidates.begin(), candidates.end(),
            [](const QueryCandidate& a, const QueryCandidate& b) { return a.score > b.score; });
    } else {
        std::sort(candidates.begin(), candidates.end(),
            [](const QueryCandidate& a, const QueryCandidate& b) { return a.score < b.score; });
    }

    // Trim to max results
    if (candidates.size() > tmpl->max_results) {
        candidates.resize(tmpl->max_results);
    }

    // Count valid
    for (const auto& c : candidates) {
        if (c.valid) result.valid_count++;
    }

    result.candidates = std::move(candidates);
    if (!result.candidates.empty()) {
        result.best_position = result.candidates[0].position;
        result.best_score = result.candidates[0].score;
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.query_time_ms = std::chrono::duration<float, std::milli>(end - start).count();

    return result;
}

} // namespace ai
} // namespace dse
