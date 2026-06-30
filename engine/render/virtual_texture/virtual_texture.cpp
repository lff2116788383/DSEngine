/**
 * @file virtual_texture.cpp
 * @brief 虚拟纹理系统实现
 */

#include "engine/render/virtual_texture/virtual_texture.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dse {
namespace vt {

// ─── VTPageCache ────────────────────────────────────────────────────────────

void VTPageCache::Init(uint32_t pool_size_pages) {
    pool_size_ = pool_size_pages;
    uint32_t total = pool_size_ * pool_size_;
    pages_.resize(total);
    free_list_.reserve(total);

    for (uint32_t i = 0; i < total; ++i) {
        pages_[i].pool_x = i % pool_size_;
        pages_[i].pool_y = i / pool_size_;
        pages_[i].occupied = false;
        free_list_.push_back(i);
    }
}

bool VTPageCache::Contains(const PageId& page) const {
    return page_map_.find(page) != page_map_.end();
}

PhysicalPage* VTPageCache::Allocate(const PageId& page, uint64_t frame) {
    // 如果已存在，直接 touch 并返回
    auto it = page_map_.find(page);
    if (it != page_map_.end()) {
        pages_[it->second].last_used_frame = frame;
        return &pages_[it->second];
    }

    uint32_t slot;
    if (!free_list_.empty()) {
        // 有空闲槽位
        slot = free_list_.back();
        free_list_.pop_back();
    } else {
        // LRU 驱逐：找到 last_used_frame 最小的
        uint64_t min_frame = UINT64_MAX;
        slot = 0;
        for (uint32_t i = 0; i < pages_.size(); ++i) {
            if (pages_[i].occupied && pages_[i].last_used_frame < min_frame) {
                min_frame = pages_[i].last_used_frame;
                slot = i;
            }
        }
        // 从映射中移除被驱逐的页
        page_map_.erase(pages_[slot].resident_page);
    }

    pages_[slot].occupied = true;
    pages_[slot].resident_page = page;
    pages_[slot].last_used_frame = frame;
    page_map_[page] = slot;

    return &pages_[slot];
}

void VTPageCache::Touch(const PageId& page, uint64_t frame) {
    auto it = page_map_.find(page);
    if (it != page_map_.end()) {
        pages_[it->second].last_used_frame = frame;
    }
}

const PhysicalPage* VTPageCache::GetPhysicalPage(const PageId& page) const {
    auto it = page_map_.find(page);
    if (it != page_map_.end()) return &pages_[it->second];
    return nullptr;
}

void VTPageCache::Clear() {
    for (auto& p : pages_) p.occupied = false;
    page_map_.clear();
    free_list_.clear();
    for (uint32_t i = 0; i < pages_.size(); ++i) {
        free_list_.push_back(i);
    }
}

// ─── VirtualTextureSystem ──────────────────────────────────────────────────

bool VirtualTextureSystem::Init(const VirtualTextureConfig& config, render::RhiDevice* rhi) {
    config_ = config;
    rhi_ = rhi;

    cache_.Init(config.pool_size_pages);

    // 初始化 page table（mip 0 的页表大小）
    uint32_t pt_size = PageTableSize();
    page_table_.resize(pt_size * pt_size);
    std::memset(page_table_.data(), 0, page_table_.size() * sizeof(PageTableEntry));

    initialized_ = true;
    return true;
}

void VirtualTextureSystem::Update(uint64_t frame_number) {
    if (!initialized_) return;
    current_frame_ = frame_number;

    ProcessFeedback();
    UploadPages();
    RebuildPageTable();
}

void VirtualTextureSystem::SubmitFeedback(const std::vector<FeedbackEntry>& entries) {
    pending_feedback_.insert(pending_feedback_.end(), entries.begin(), entries.end());
}

uint32_t VirtualTextureSystem::PageTableSize() const {
    return config_.virtual_size / kPageSize;
}

uint32_t VirtualTextureSystem::PhysicalAtlasSize() const {
    return config_.pool_size_pages * kPageSizeWithBorder;
}

float VirtualTextureSystem::CacheHitRate() const {
    uint64_t total = cache_hits_ + cache_misses_;
    if (total == 0) return 1.0f;
    return static_cast<float>(cache_hits_) / static_cast<float>(total);
}

void VirtualTextureSystem::ProcessFeedback() {
    // 去重 feedback 条目，将新页请求加入加载队列
    std::unordered_set<PageId, PageIdHash> requested;

    for (const auto& entry : pending_feedback_) {
        PageId page;
        page.x = entry.page_x;
        page.y = entry.page_y;
        page.mip = entry.mip;
        page.vt_id = entry.vt_id;

        if (cache_.Contains(page)) {
            cache_.Touch(page, current_frame_);
            ++cache_hits_;
        } else {
            if (requested.find(page) == requested.end()) {
                requested.insert(page);
                load_queue_.push_back(page);
                ++cache_misses_;
            }
        }
    }

    pending_feedback_.clear();

    // 按 mip level 排序（低 mip = 更粗糙 → 更高优先级，保证 fallback 可用）
    std::sort(load_queue_.begin(), load_queue_.end(),
        [](const PageId& a, const PageId& b) {
            if (a.mip != b.mip) return a.mip > b.mip; // 粗糙 mip 优先
            return false;
        });
}

void VirtualTextureSystem::UploadPages() {
    if (!page_load_cb_) return;

    uint32_t uploaded = 0;
    std::vector<PageId> remaining;

    for (const auto& page : load_queue_) {
        if (uploaded >= config_.max_uploads_per_frame) {
            remaining.push_back(page);
            continue;
        }

        // 尝试加载页面像素数据
        std::vector<uint8_t> pixels;
        if (page_load_cb_(page, pixels)) {
            // 分配物理页
            PhysicalPage* physical = cache_.Allocate(page, current_frame_);
            if (physical) {
                // 实际 GPU 上传由外部 RHI 处理
                // rhi_->UploadTextureSubRegion(atlas_handle,
                //     physical->pool_x * kPageSizeWithBorder,
                //     physical->pool_y * kPageSizeWithBorder,
                //     kPageSizeWithBorder, kPageSizeWithBorder,
                //     pixels.data());
                ++uploaded;
            }
        }
    }

    load_queue_ = std::move(remaining);
}

void VirtualTextureSystem::RebuildPageTable() {
    uint32_t pt_size = PageTableSize();

    for (uint32_t y = 0; y < pt_size; ++y) {
        for (uint32_t x = 0; x < pt_size; ++x) {
            PageId page;
            page.x = x;
            page.y = y;
            page.mip = 0;
            page.vt_id = 0;

            auto& entry = page_table_[y * pt_size + x];

            const PhysicalPage* pp = cache_.GetPhysicalPage(page);
            if (pp && pp->occupied) {
                entry.physical_x = static_cast<uint16_t>(pp->pool_x);
                entry.physical_y = static_cast<uint16_t>(pp->pool_y);
                entry.valid = 1;
            } else {
                // Fallback: 查找更粗糙的 mip
                bool found_fallback = false;
                for (uint32_t mip = 1; mip < kMaxMipLevels; ++mip) {
                    PageId coarser;
                    coarser.x = x >> mip;
                    coarser.y = y >> mip;
                    coarser.mip = mip;
                    coarser.vt_id = 0;

                    const PhysicalPage* fallback = cache_.GetPhysicalPage(coarser);
                    if (fallback && fallback->occupied) {
                        entry.physical_x = static_cast<uint16_t>(fallback->pool_x);
                        entry.physical_y = static_cast<uint16_t>(fallback->pool_y);
                        entry.valid = 1;
                        found_fallback = true;
                        break;
                    }
                }
                if (!found_fallback) {
                    entry.valid = 0;
                }
            }
        }
    }
}

void VirtualTextureSystem::Shutdown() {
    cache_.Clear();
    page_table_.clear();
    load_queue_.clear();
    pending_feedback_.clear();
    initialized_ = false;
}

} // namespace vt
} // namespace dse
