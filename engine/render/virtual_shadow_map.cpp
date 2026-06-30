/**
 * @file virtual_shadow_map.cpp
 * @brief 虚拟阴影贴图系统实现
 */

#include "engine/render/virtual_shadow_map.h"
#include <cmath>
#include <algorithm>

namespace dse {
namespace render {

void VirtualShadowMapSystem::Init(const VSMConfig& config) {
    config_ = config;

    uint32_t pages_per_dim = config_.virtual_resolution / config_.page_size;
    uint32_t total_virtual_pages = pages_per_dim * pages_per_dim * config_.clipmap_levels;

    page_table_.resize(total_virtual_pages);
    physical_pool_.resize(config_.physical_pool_pages);

    for (auto& pp : physical_pool_) {
        pp.allocated = false;
        pp.owner_hash = 0;
        pp.last_rendered_frame = 0;
    }

    initialized_ = true;
}

void VirtualShadowMapSystem::Shutdown() {
    page_table_.clear();
    physical_pool_.clear();
    pages_to_render_.clear();
    lights_.clear();
    requested_pages_.clear();
    mapped_count_ = 0;
    initialized_ = false;
}

uint32_t VirtualShadowMapSystem::RegisterLight(const ShadowLightInfo& info) {
    lights_.push_back(info);
    return info.light_id;
}

void VirtualShadowMapSystem::UnregisterLight(uint32_t light_id) {
    lights_.erase(std::remove_if(lights_.begin(), lights_.end(),
        [light_id](const ShadowLightInfo& l) { return l.light_id == light_id; }),
        lights_.end());

    // Free pages belonging to this light
    for (auto& page : page_table_) {
        if (page.light_id == light_id && page.state != PageState::Unmapped) {
            if (page.physical_x != UINT32_MAX) {
                uint32_t pool_idx = page.physical_y * (config_.physical_pool_pages) + page.physical_x;
                if (pool_idx < physical_pool_.size()) {
                    physical_pool_[pool_idx].allocated = false;
                    mapped_count_--;
                }
            }
            page.state = PageState::Unmapped;
            page.physical_x = UINT32_MAX;
            page.physical_y = UINT32_MAX;
        }
    }
}

void VirtualShadowMapSystem::UpdateLight(uint32_t light_id, const ShadowLightInfo& info) {
    for (auto& l : lights_) {
        if (l.light_id == light_id) {
            bool moved = (l.direction != info.direction || l.view_proj != info.view_proj);
            l = info;
            l.moved = moved;
            if (moved) {
                // Invalidate all pages for this light
                for (auto& page : page_table_) {
                    if (page.light_id == light_id && page.state == PageState::Cached) {
                        page.state = PageState::Dirty;
                    }
                }
            }
            break;
        }
    }
}

void VirtualShadowMapSystem::SubmitFeedback(const std::vector<glm::uvec3>& sampled_pages) {
    for (const auto& sp : sampled_pages) {
        uint32_t hash = PageHash(sp.x, sp.y, sp.z, 0);
        requested_pages_.insert(hash);
    }
}

void VirtualShadowMapSystem::InvalidateRegion(uint32_t light_id,
                                               const glm::vec3& aabb_min,
                                               const glm::vec3& aabb_max) {
    (void)aabb_min; (void)aabb_max;
    // Mark overlapping pages as dirty
    for (auto& page : page_table_) {
        if (page.light_id == light_id && page.state == PageState::Cached) {
            page.state = PageState::Dirty;
        }
    }
}

void VirtualShadowMapSystem::BeginFrame(uint32_t frame_number, const glm::vec3& camera_pos) {
    if (!initialized_) return;
    current_frame_ = frame_number;
    camera_pos_ = camera_pos;
    pages_to_render_.clear();
    rendered_this_frame_ = 0;

    // Collect pages that need rendering (dirty + newly requested)
    uint32_t budget = config_.max_pages_per_frame;

    for (auto& page : page_table_) {
        if (pages_to_render_.size() >= budget) break;

        if (page.state == PageState::Dirty || page.state == PageState::Requested) {
            // Allocate physical page if needed
            if (page.physical_x == UINT32_MAX) {
                uint32_t phys_idx = AllocatePhysicalPage();
                if (phys_idx == UINT32_MAX) {
                    EvictLRU();
                    phys_idx = AllocatePhysicalPage();
                }
                if (phys_idx != UINT32_MAX) {
                    uint32_t pool_dim = static_cast<uint32_t>(std::sqrt(config_.physical_pool_pages));
                    page.physical_x = phys_idx % pool_dim;
                    page.physical_y = phys_idx / pool_dim;
                    physical_pool_[phys_idx].allocated = true;
                    physical_pool_[phys_idx].owner_hash = PageHash(page.virtual_x, page.virtual_y,
                                                                    page.mip_level, page.light_id);
                    mapped_count_++;
                }
            }
            if (page.physical_x != UINT32_MAX) {
                pages_to_render_.push_back(page);
            }
        }
    }

    requested_pages_.clear();
}

void VirtualShadowMapSystem::MarkPageRendered(uint32_t virtual_x, uint32_t virtual_y,
                                               uint32_t mip, uint32_t light_id) {
    uint32_t pages_per_dim = config_.virtual_resolution / config_.page_size;
    uint32_t idx = mip * pages_per_dim * pages_per_dim + virtual_y * pages_per_dim + virtual_x;
    if (idx < page_table_.size()) {
        page_table_[idx].state = PageState::Cached;
        page_table_[idx].last_used_frame = current_frame_;
        rendered_this_frame_++;
    }
}

void VirtualShadowMapSystem::EndFrame() {
    // Update LRU timestamps for accessed pages
    for (auto& page : page_table_) {
        if (page.state == PageState::Cached) {
            // Keep alive pages near camera
        }
    }
}

bool VirtualShadowMapSystem::LookupPage(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id,
                                          uint32_t& out_px, uint32_t& out_py) const {
    (void)light_id;
    uint32_t pages_per_dim = config_.virtual_resolution / config_.page_size;
    uint32_t idx = mip * pages_per_dim * pages_per_dim + vy * pages_per_dim + vx;
    if (idx >= page_table_.size()) return false;
    const auto& page = page_table_[idx];
    if (page.state == PageState::Cached && page.physical_x != UINT32_MAX) {
        out_px = page.physical_x;
        out_py = page.physical_y;
        return true;
    }
    return false;
}

VSMStats VirtualShadowMapSystem::GetStats() const {
    uint32_t dirty = 0;
    for (const auto& p : page_table_) {
        if (p.state == PageState::Dirty) dirty++;
    }

    uint32_t allocated = 0;
    for (const auto& p : physical_pool_) {
        if (p.allocated) allocated++;
    }

    uint32_t pool_usage = config_.physical_pool_pages > 0
        ? (allocated * 100) / config_.physical_pool_pages : 0;

    uint32_t cache_hit = mapped_count_ > 0
        ? ((mapped_count_ - rendered_this_frame_) * 100) / mapped_count_ : 100;

    return {
        static_cast<uint32_t>(page_table_.size()),
        mapped_count_,
        dirty,
        rendered_this_frame_,
        cache_hit,
        pool_usage
    };
}

glm::mat4 VirtualShadowMapSystem::GetClipmapLevelMatrix(uint32_t level) const {
    if (level >= config_.clipmap_levels) return glm::mat4(1.0f);

    float half_size = config_.far_plane * std::pow(0.5f, static_cast<float>(config_.clipmap_levels - 1 - level));
    glm::mat4 proj = glm::ortho(-half_size, half_size, -half_size, half_size,
                                 config_.near_plane, config_.far_plane);

    // Look down from above camera
    glm::mat4 view = glm::lookAt(
        camera_pos_ + glm::vec3(0, config_.far_plane * 0.5f, 0),
        camera_pos_,
        glm::vec3(0, 0, -1));

    return proj * view;
}

uint32_t VirtualShadowMapSystem::AllocatePhysicalPage() {
    for (uint32_t i = 0; i < config_.physical_pool_pages; ++i) {
        if (!physical_pool_[i].allocated) return i;
    }
    return UINT32_MAX;
}

void VirtualShadowMapSystem::FreePhysicalPage(uint32_t page_index) {
    if (page_index < physical_pool_.size()) {
        physical_pool_[page_index].allocated = false;
        physical_pool_[page_index].owner_hash = 0;
    }
}

void VirtualShadowMapSystem::EvictLRU() {
    // Find oldest cached page and evict
    uint32_t oldest_frame = UINT32_MAX;
    size_t oldest_idx = SIZE_MAX;

    for (size_t i = 0; i < page_table_.size(); ++i) {
        if (page_table_[i].state == PageState::Cached &&
            page_table_[i].last_used_frame < oldest_frame) {
            oldest_frame = page_table_[i].last_used_frame;
            oldest_idx = i;
        }
    }

    if (oldest_idx != SIZE_MAX) {
        auto& page = page_table_[oldest_idx];
        uint32_t pool_dim = static_cast<uint32_t>(std::sqrt(config_.physical_pool_pages));
        uint32_t phys_idx = page.physical_y * pool_dim + page.physical_x;
        FreePhysicalPage(phys_idx);
        page.state = PageState::Unmapped;
        page.physical_x = UINT32_MAX;
        page.physical_y = UINT32_MAX;
        mapped_count_--;
    }
}

uint32_t VirtualShadowMapSystem::PageHash(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id) const {
    return ((light_id & 0xFF) << 24) | ((mip & 0xF) << 20) | ((vy & 0x3FF) << 10) | (vx & 0x3FF);
}

} // namespace render
} // namespace dse
