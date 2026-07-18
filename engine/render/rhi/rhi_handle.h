/**
 * @file rhi_handle.h
 * @brief 类型安全的 GPU 资源句柄 — 编译期区分 Buffer/Texture/RT/Pipeline/VAO
 *
 * 零运行时开销（sizeof == sizeof(unsigned int)）。
 * 注意：当前 id 为裸 uint32_t，无世代校验（注释曾提及的 DSE_DEBUG_HANDLES 世代验证层尚未实现）。
 * 后果：id 回收复用后，滞留的旧句柄会静默绑定到新资源（无崩溃、无告警，表现为诡异渲染错误）。
 * TODO: [N4] 至少在 Debug 构建落实世代验证层（句柄高位叠加世代计数 + 查表时校验）。
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
