/**
 * @file texture_ref.cpp
 * @brief TextureRefRegistry 实现 — 托管纹理句柄的引用计数表。
 */

#include "engine/render/rhi/texture_ref.h"

namespace dse {
namespace render {

TextureRefRegistry& TextureRefRegistry::Instance() {
    // 故意泄漏：确保 registry 生命周期长于所有 TextureRef（含静态/全局存储中的），
    // 规避静态析构顺序问题。进程退出时由 OS 回收。
    static TextureRefRegistry* instance = new TextureRefRegistry();
    return *instance;
}

void TextureRefRegistry::Register(uint32_t id) {
    if (id == 0) return;
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = cells_.find(id);
    if (it == cells_.end()) {
        cells_.emplace(id, std::make_shared<TextureRefCell>());
    }
    // 已存在则保留现有 cell（含其引用计数），保证句柄复用时计数连续。
}

void TextureRefRegistry::Unregister(uint32_t id) {
    if (id == 0) return;
    std::lock_guard<std::mutex> lock(mtx_);
    // 仅从表中移除；若仍有 TextureRef 持有该控制块的 shared_ptr，控制块会存活
    // 至最后一个 TextureRef 析构，因此不会产生悬挂指针。
    cells_.erase(id);
}

int TextureRefRegistry::RefCount(uint32_t id) const {
    if (id == 0) return 0;
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = cells_.find(id);
    if (it == cells_.end()) return 0;
    return it->second->refs.load(std::memory_order_acquire);
}

bool TextureRefRegistry::IsManaged(uint32_t id) const {
    if (id == 0) return false;
    std::lock_guard<std::mutex> lock(mtx_);
    return cells_.find(id) != cells_.end();
}

std::shared_ptr<TextureRefCell> TextureRefRegistry::Find(uint32_t id) {
    if (id == 0) return nullptr;
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = cells_.find(id);
    if (it == cells_.end()) return nullptr;
    return it->second;
}

} // namespace render
} // namespace dse
