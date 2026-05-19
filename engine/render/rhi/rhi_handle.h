/**
 * @file rhi_handle.h
 * @brief 类型安全的 GPU 资源句柄 — 编译期区分 Buffer/Texture/RT/Pipeline/VAO
 *
 * 零运行时开销（sizeof == sizeof(unsigned int)）。
 * Debug 构建可启用 DSE_DEBUG_HANDLES 获得 generation counter 验证层。
 */

#ifndef DSE_RHI_HANDLE_H
#define DSE_RHI_HANDLE_H

#include <cstdint>
#include <functional>
#include <type_traits>

namespace dse {
namespace render {

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
struct PipelineTag {};

// ============================================================
// Handle 类型别名
// ============================================================

using TextureHandle      = TypedHandle<TextureTag>;
using BufferHandle       = TypedHandle<BufferTag>;
using VertexArrayHandle  = TypedHandle<VertexArrayTag>;
using RenderTargetHandle = TypedHandle<RenderTargetTag>;
using PipelineHandle     = TypedHandle<PipelineTag>;

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
