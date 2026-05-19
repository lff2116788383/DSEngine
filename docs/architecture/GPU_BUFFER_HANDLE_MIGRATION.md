# GPU Buffer Handle 迁移设计

## 现状分析

### 当前 Handle 体系

各后端使用 `unsigned int` 作为 opaque handle，通过各自的自增计数器分配，且 handle 范围互不重叠：

| 后端    | 资源类型         | 起始偏移       | 存储方式 |
|---------|------------------|----------------|----------|
| OpenGL  | Indirect Buffer  | 600000         | `unordered_map<uint, uint>` (handle→GL buf) |
| DX11    | Buffer (VB/IB)   | ResourceMgr内部| `unordered_map<uint, DX11Buffer>` |
| DX11    | SSBO             | ResourceMgr内部| `unordered_map<uint, DX11SSBO>` |
| DX11    | Indirect Buffer  | ResourceMgr内部| `unordered_map<uint, DX11IndirectBuffer>` |
| Vulkan  | Buffer           | 410000         | `unordered_map<uint, VulkanBuffer>` |
| Vulkan  | SSBO             | 415000         | `unordered_map<uint, VulkanBuffer>` |
| Vulkan  | Indirect Buffer  | 未实现（桩）    | — |

### 问题

1. **无类型安全** — 所有 handle 都是 `unsigned int`，混用 buffer/texture/RT handle 不会产生编译错误
2. **用途分裂** — 同一概念（GPU buffer）按用途拆为 3 套独立 API（Buffer/SSBO/Indirect）
3. **Vulkan 缺失** — Indirect Draw Buffer 在 Vulkan 后端为空桩
4. **句柄范围脆弱** — 全靠约定的起始偏移避免冲突，无运行时校验

---

## 迁移目标

1. 引入轻量 typed handle wrapper（编译期区分资源类型）
2. 统一 GPU buffer 生命周期为单一 `CreateGpuBuffer` / `DeleteGpuBuffer` 接口，由 usage flags 区分用途
3. Vulkan Indirect Buffer 补全实现
4. 保持 ABI 向后兼容（渐进式替换）

---

## 方案设计

### Phase 1: Typed Handle Wrapper（低风险，可先行）

```cpp
// engine/render/rhi/rhi_handle.h
template <typename Tag>
struct TypedHandle {
    unsigned int id = 0;
    explicit operator bool() const { return id != 0; }
    bool operator==(TypedHandle o) const { return id == o.id; }
    bool operator!=(TypedHandle o) const { return id != o.id; }
};

struct TextureTag {};
struct BufferTag {};
struct RenderTargetTag {};
struct PipelineTag {};

using TextureHandle    = TypedHandle<TextureTag>;
using BufferHandle     = TypedHandle<BufferTag>;
using RenderTargetHandle = TypedHandle<RenderTargetTag>;
using PipelineHandle   = TypedHandle<PipelineTag>;
```

- 零运行时开销（sizeof 不变）
- `std::hash` 特化供 unordered_map 使用
- 逐步替换现有 `unsigned int` 参数

### Phase 2: 统一 Buffer API

```cpp
enum class GpuBufferUsage : uint32_t {
    kVertex         = 1 << 0,
    kIndex          = 1 << 1,
    kStorage        = 1 << 2,  // SSBO / StructuredBuffer
    kIndirect       = 1 << 3,
    kTransferSrc    = 1 << 4,
    kTransferDst    = 1 << 5,
};

struct GpuBufferDesc {
    size_t size = 0;
    GpuBufferUsage usage = GpuBufferUsage::kVertex;
    bool is_dynamic = false;       // HOST_VISIBLE，允许 CPU 写
    const char* debug_name = nullptr;
};

// RhiDevice 新增:
virtual BufferHandle CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) = 0;
virtual void UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) = 0;
virtual void DeleteGpuBuffer(BufferHandle handle) = 0;
```

旧 API (`CreateBuffer`, `CreateSSBO`, `CreateIndirectBuffer`) 保留为 inline 转发，标 `[[deprecated]]`。

### Phase 3: Vulkan Indirect Buffer 实现

基于 `VulkanResourceManager::CreateBuffer` 现有逻辑，添加 `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`：

```cpp
BufferHandle VulkanRhiDevice::CreateGpuBuffer(const GpuBufferDesc& desc, const void* data) {
    VkBufferUsageFlags vk_usage = 0;
    if (desc.usage & kVertex)   vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (desc.usage & kIndex)    vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (desc.usage & kStorage)  vk_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (desc.usage & kIndirect) vk_usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    // ... 复用现有 staging + memory alloc 逻辑
}
```

---

## 迁移顺序

| 步骤 | 工作量 | 风险 | 内容 |
|------|--------|------|------|
| 1    | 小     | 低   | 添加 `rhi_handle.h`，不改现有代码 |
| 2    | 中     | 低   | 新 API 并存，旧 API deprecated |
| 3    | 中     | 中   | Vulkan indirect buffer 实现 + 测试 |
| 4    | 大     | 中   | 全量替换调用点 `unsigned int` → typed handle |
| 5    | 小     | 低   | 移除旧 API，清理 deprecated 标记 |

---

## 测试策略

- 为 `TypedHandle` 添加编译期 static_assert 测试（sizeof、trivially_copyable）
- 扩展现有 `rhi_test` 单元测试验证新 API 兼容性
- Vulkan indirect buffer: 添加 smoke test（创建 + 更新 + 多次绘制）
- 三后端回归截图对比确认无视觉差异

---

## 备注

- 当前 `WINDOWS_EXPORT_ALL_SYMBOLS` 对模板类型有泄漏风险（参考 HiZTextureInfo LNK2001 修复），`TypedHandle` 因为是 trivial struct 应不受影响，但需要验证
- OpenGL 的 `indirect_buffers_` map 将统一到 ResourceManager 管理，与 DX11 对齐
