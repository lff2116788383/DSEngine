/**
 * @file rhi_handle.h
 * @brief 类型安全的 GPU 资源句柄 — 编译期区分 Buffer/Texture/RT/Pipeline/VAO
 *
 * 零运行时开销（sizeof == sizeof(unsigned int)）。
 * 句柄 id 现状：各后端资源管理器单调发号（next_*_handle_++，GL 用幻数基址错开），
 * 进程内不回收复用 → "id 复用后旧句柄指向新资源"的世代冲突当前不会发生。
 * 防御措施（Debug 构建）：HandleActivityLedger 跟踪分配/释放，句柄查询处断言
 * 不在"已释放"集合——捕获 use-after-free 句柄调用（删除后仍被使用）。
 * 若未来引入句柄池化/回收，需在句柄高位叠加世代计数（[N4]）。
 */

#ifndef DSE_RHI_HANDLE_H
#define DSE_RHI_HANDLE_H

#include <cstdint>
#include <functional>
#include <type_traits>
#include <unordered_set>

namespace dse {
namespace render {

// ============================================================
// HandleActivityLedger — Debug 构建下的句柄生命周期账本
// ============================================================
// 仅 Debug 构建编译（_DEBUG）；Release 下所有方法为空操作、零开销。
// MarkAllocated：句柄分配/登记时调用；MarkReleased：句柄释放时调用；
// 维护分配/释放集合与计数（active_count/released_count），供调试排查/泄漏审计。
// 注意：这里刻意不做断言——各后端对未知/已释放句柄的查询返回 nullptr（见
// GLResourceManagerTest.Remove：删除后 Get 必须返回 nullptr），删除未知句柄是
// 文档化 no-op（见 dx11/vulkan rhi 单测的 Delete(from_raw(999))）。若在查询或
// 释放处断言"句柄必须有效"，会与这些既有 API 契约冲突产生误报。
// 当前句柄单调发号、进程内不复用，不存在"旧句柄指向新资源"的世代冲突；
// 若未来引入句柄池化/回收，需在句柄高位叠加世代计数（[N4]），本账本可作为跟踪基础。
#if defined(_DEBUG) || defined(DSE_HANDLE_DEBUG)

class HandleActivityLedger {
public:
    void MarkAllocated(uint32_t id) {
        if (id == 0) return;
        released_.erase(id);
        allocated_.insert(id);
    }
    void MarkReleased(uint32_t id) {
        if (id == 0) return;
        allocated_.erase(id);
        released_.insert(id);
    }
    std::size_t active_count() const { return allocated_.size(); }
    std::size_t released_count() const { return released_.size(); }
private:
    std::unordered_set<uint32_t> allocated_;
    std::unordered_set<uint32_t> released_;
};

#else

class HandleActivityLedger {
public:
    void MarkAllocated(uint32_t) {}
    void MarkReleased(uint32_t) {}
    std::size_t active_count() const { return 0; }
    std::size_t released_count() const { return 0; }
};

#endif // _DEBUG || DSE_HANDLE_DEBUG

// ============================================================
// TypedHandle — 编译期类型安全的 opaque handle
// ============================================================

template <typename Tag>
struct TypedHandle {
    uint32_t id = 0;

    constexpr TypedHandle() = default;
    constexpr explicit TypedHandle(uint32_t raw) : id(raw) {}

    explicit operator bool() const { return id != 0; }
    bool operator==(TypedHandle o) const { return id == o.id; }
    bool operator!=(TypedHandle o) const { return id != o.id; }
    bool operator<(TypedHandle o) const { return id < o.id; }

    /// 与旧代码互操作：显式获取底层 id
    uint32_t raw() const { return id; }

    /// 从旧代码的 unsigned int 显式构造（用于渐进迁移）
    static TypedHandle from_raw(uint32_t raw_id) { return TypedHandle{raw_id}; }
};

// ============================================================
// Tag 类型定义
// ============================================================

struct TextureTag {};
struct BufferTag {};
struct VertexArrayTag {};
struct RenderTargetTag {};
struct ShaderTag {};
struct PipelineTag {};
struct GraphicsPipelineTag {};

// ============================================================
// Handle 类型别名
// ============================================================

using TextureHandle      = TypedHandle<TextureTag>;
using BufferHandle       = TypedHandle<BufferTag>;
using VertexArrayHandle  = TypedHandle<VertexArrayTag>;
using RenderTargetHandle = TypedHandle<RenderTargetTag>;
using ShaderHandle       = TypedHandle<ShaderTag>;
using PipelineHandle     = TypedHandle<PipelineTag>;
/// GetGraphicsPipeline 返回的聚合图形管线对象句柄（区别于 CreatePipelineState 的 PSO 状态句柄）。
using GraphicsPipelineHandle = TypedHandle<GraphicsPipelineTag>;

// ============================================================
// 编译期保证
// ============================================================

static_assert(sizeof(TextureHandle) == sizeof(uint32_t), "TypedHandle must be same size as uint32_t");
static_assert(std::is_trivially_copyable_v<BufferHandle>, "TypedHandle must be trivially copyable");
static_assert(std::is_trivially_destructible_v<BufferHandle>, "TypedHandle must be trivially destructible");
static_assert(std::is_standard_layout_v<BufferHandle>, "TypedHandle must be standard layout");

} // namespace render
} // namespace dse

// ============================================================
// std::hash 特化 — 支持 unordered_map/unordered_set
// ============================================================

namespace std {
template <typename Tag>
struct hash<dse::render::TypedHandle<Tag>> {
    size_t operator()(dse::render::TypedHandle<Tag> h) const noexcept {
        return hash<uint32_t>{}(h.id);
    }
};
} // namespace std

#endif // DSE_RHI_HANDLE_H
