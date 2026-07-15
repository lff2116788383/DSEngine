/**
 * @file texture_ref.h
 * @brief RAII 引用计数纹理句柄 — 让 AssetManager 能安全释放不再被引用的 GPU 纹理。
 *
 * 设计要点：
 *  - TextureRef 是 TextureHandle 的薄包装，可隐式与 TextureHandle 互转，
 *    因此绝大多数读取点（绑定、比较、日志）无需改动。
 *  - 仅对 AssetManager 显式 Register 过的"托管纹理"参与引用计数；渲染目标、
 *    设备直建纹理等"借用"句柄一律不计数、不会被回收（Find 返回 nullptr）。
 *  - 引用计数存放在共享控制块（TextureRefCell）中，TextureRef 与 Registry 都持有
 *    shared_ptr，控制块在"注册表注销 且 最后一个 TextureRef 析构"后才真正释放，
 *    因此不存在悬挂 cell（关机顺序无关），句柄 id 复用也天然安全（新 id 拿到新控制块）。
 *    拷贝/析构走无锁原子增减；仅"从句柄赋值"时才需要一次带锁查表。
 *  - 实际 GPU 删除由 AssetManager 在安全线程点执行（见 EvictLRU/UnloadUnused），
 *    TextureRef 析构只做计数递减，不直接触碰 GPU。
 */

#ifndef DSE_RHI_TEXTURE_REF_H
#define DSE_RHI_TEXTURE_REF_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "engine/core/dse_export.h"
#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

/// 单个托管纹理句柄的引用计数单元。地址在被 Unregister 前保持稳定。
struct TextureRefCell {
    std::atomic<int> refs{0};
};

inline void TextureRefIncRef(TextureRefCell* c) {
    if (c) c->refs.fetch_add(1, std::memory_order_relaxed);
}
inline void TextureRefDecRef(TextureRefCell* c) {
    if (c) c->refs.fetch_sub(1, std::memory_order_acq_rel);
}

/**
 * @class TextureRefRegistry
 * @brief 进程级"托管纹理句柄 → 引用计数"表。由 AssetManager 维护登记/注销，
 *        由 TextureRef 在赋值时查询。故意泄漏的单例以规避静态析构顺序问题。
 */
class DSE_EXPORT TextureRefRegistry {
public:
    static TextureRefRegistry& Instance();

    /// 登记一个托管纹理句柄（引用计数从 0 开始）。幂等。
    void Register(uint32_t id);
    /// 注销一个托管纹理句柄（应在 GPU 删除后调用）。
    void Unregister(uint32_t id);
    /// 查询当前引用计数；未托管句柄返回 0。
    int RefCount(uint32_t id) const;
    /// 是否为托管句柄。
    bool IsManaged(uint32_t id) const;
    /// 解析句柄对应的共享控制块；未托管返回 nullptr。仅供 TextureRef 使用。
    std::shared_ptr<TextureRefCell> Find(uint32_t id);

private:
    TextureRefRegistry() = default;
    TextureRefRegistry(const TextureRefRegistry&) = delete;
    TextureRefRegistry& operator=(const TextureRefRegistry&) = delete;

    mutable std::mutex mtx_;
    std::unordered_map<uint32_t, std::shared_ptr<TextureRefCell>> cells_;
};

/**
 * @class TextureRef
 * @brief 引用计数的纹理句柄。可当作 TextureHandle 使用（隐式转换）。
 */
class TextureRef {
public:
    TextureRef() = default;
    TextureRef(TextureHandle h) { AssignHandle(h); } // NOLINT(runtime/explicit) 允许隐式迁移
    TextureRef(const TextureRef& o) : handle_(o.handle_), cell_(o.cell_) { TextureRefIncRef(cell_.get()); }
    TextureRef(TextureRef&& o) noexcept : handle_(o.handle_), cell_(std::move(o.cell_)) {
        o.handle_ = TextureHandle{};
    }
    TextureRef& operator=(const TextureRef& o) {
        if (this != &o) {
            TextureRefIncRef(o.cell_.get());
            TextureRefDecRef(cell_.get());
            handle_ = o.handle_;
            cell_ = o.cell_;
        }
        return *this;
    }
    TextureRef& operator=(TextureRef&& o) noexcept {
        if (this != &o) {
            TextureRefDecRef(cell_.get());
            handle_ = o.handle_;
            cell_ = std::move(o.cell_);
            o.handle_ = TextureHandle{};
        }
        return *this;
    }
    // 注意：不单独提供 operator=(TextureHandle)，而是依赖隐式转换构造 + 移动赋值，
    // 以避免 `ref = {}` 在 TextureHandle 与 TextureRef 两条重载间产生歧义。
    ~TextureRef() { TextureRefDecRef(cell_.get()); }

    operator TextureHandle() const { return handle_; } // NOLINT 隐式转换，最小化读取点改动
    TextureHandle handle() const { return handle_; }
    uint32_t raw() const { return handle_.id; }
    uint32_t id() const { return handle_.id; }
    explicit operator bool() const { return handle_.id != 0; }

    bool operator==(const TextureRef& o) const { return handle_ == o.handle_; }
    bool operator!=(const TextureRef& o) const { return handle_ != o.handle_; }
    bool operator==(TextureHandle h) const { return handle_ == h; }
    bool operator!=(TextureHandle h) const { return handle_ != h; }

private:
    void AssignHandle(TextureHandle h) {
        handle_ = h;
        cell_ = h.id ? TextureRefRegistry::Instance().Find(h.id) : nullptr;
        TextureRefIncRef(cell_.get());
    }

    TextureHandle handle_{};
    std::shared_ptr<TextureRefCell> cell_;
};

inline bool operator==(TextureHandle h, const TextureRef& r) { return r == h; }
inline bool operator!=(TextureHandle h, const TextureRef& r) { return r != h; }

} // namespace render
} // namespace dse

#endif // DSE_RHI_TEXTURE_REF_H
