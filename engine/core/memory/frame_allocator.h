/**
 * @file frame_allocator.h
 * @brief 多缓冲帧线性分配器：N 个 LinearAllocator 轮转，配合渲染快照 fence 安全复位。
 *
 * 缓冲数 N = 帧延迟 + 1（单线程渲染 N=2 即可；为未来并行渲染默认 N=2，可配到更高）。
 * 每帧在「安全 fence 点」调用 BeginFrame() 推进到下一缓冲并复位它——
 * 该点必须保证上一个使用该缓冲的渲染帧已完成（见 FramePipeline::Render 中
 * WaitForRenderComplete 之后调用）。详见设计文档 §3.5。
 */

#ifndef DSE_CORE_MEMORY_FRAME_ALLOCATOR_H
#define DSE_CORE_MEMORY_FRAME_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include "engine/core/memory/allocator.h"
#include "engine/core/memory/linear_allocator.h"

namespace dse {
namespace core {

/// 帧分配器最大缓冲数上限。
constexpr uint32_t kMaxFrameBuffers = 4;

/**
 * @class FrameAllocator
 * @brief 轮转多缓冲的每帧线性分配器（主线程使用）。
 */
class DSE_EXPORT FrameAllocator {
public:
    FrameAllocator() = default;

    /**
     * @brief 初始化。
     * @param per_buffer_capacity 每个缓冲的字节容量。
     * @param buffer_count 缓冲数（>=2，<=kMaxFrameBuffers）。
     * @param tag 统计标签。
     */
    void Init(size_t per_buffer_capacity, uint32_t buffer_count = 2,
              MemoryTag tag = MemoryTag::FrameTemp);
    void Shutdown();

    /// 推进到下一缓冲并复位它（在安全 fence 点每帧调用一次）。
    void BeginFrame();

    /// 从当前缓冲分配（容量不足返回 nullptr）。
    void* Alloc(size_t size, size_t alignment = kDefaultAlignment);

    LinearAllocator& Current() { return buffers_[current_]; }
    const LinearAllocator& Current() const { return buffers_[current_]; }

    uint32_t BufferCount() const { return buffer_count_; }
    uint32_t CurrentIndex() const { return current_; }
    uint64_t FrameNumber() const { return frame_number_; }
    bool IsInitialized() const { return buffer_count_ > 0; }

private:
    LinearAllocator buffers_[kMaxFrameBuffers];
    uint32_t buffer_count_ = 0;
    uint32_t current_ = 0;
    uint64_t frame_number_ = 0;
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_FRAME_ALLOCATOR_H
