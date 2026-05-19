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
5. **无生命周期校验** — use-after-delete、double-delete、handle leak 均无运行时检测

---

## 迁移目标

1. 引入轻量 typed handle wrapper（编译期区分资源类型）
2. 统一 GPU buffer 生命周期为单一 `CreateGpuBuffer` / `DeleteGpuBuffer` 接口，由 usage flags 区分用途
3. 补充 Bind / Readback 操作，使 SSBO 旧 API 可完全 deprecated
4. Vulkan Indirect Buffer 补全实现
5. 保持 ABI 向后兼容（渐进式替换）

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
    bool operator<(TypedHandle o) const { return id < o.id; }
};

struct TextureTag {};
struct BufferTag {};
struct VertexArrayTag {};
struct RenderTargetTag {};
struct PipelineTag {};

using TextureHandle      = TypedHandle<TextureTag>;
using BufferHandle       = TypedHandle<BufferTag>;
using VertexArrayHandle  = TypedHandle<VertexArrayTag>;
using RenderTargetHandle = TypedHandle<RenderTargetTag>;
using PipelineHandle     = TypedHandle<PipelineTag>;
```

附加设施：
- `std::hash<TypedHandle<T>>` 特化供 unordered_map 使用
- `operator<` 支持 `std::map` / 排序
- Debug 模式添加 generation counter（见下方"验证层"）

### Phase 2: 统一 Buffer API

```cpp
enum class GpuBufferUsage : uint32_t {
    kVertex         = 1 << 0,
    kIndex          = 1 << 1,
    kStorage        = 1 << 2,  // SSBO / StructuredBuffer
    kIndirect       = 1 << 3,
    kTransferSrc    = 1 << 4,
    kTransferDst    = 1 << 5,
    kUniform        = 1 << 6,  // UBO / Constant Buffer（预留，暂不实现）
};
DSE_ENUM_FLAGS(GpuBufferUsage);  // 允许按位组合

struct GpuBufferDesc {
    size_t size = 0;
    GpuBufferUsage usage = GpuBufferUsage::kVertex;
    bool is_dynamic = false;       // HOST_VISIBLE，允许 CPU 写
    const char* debug_name = nullptr;
};

// RhiDevice 新增（纯虚）:
virtual BufferHandle CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) = 0;
virtual void UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) = 0;
virtual void DeleteGpuBuffer(BufferHandle handle) = 0;

// Bind / Readback（SSBO、UBO 场景必需）:
virtual void BindGpuBuffer(BufferHandle handle, uint32_t binding_point) = 0;
virtual void ReadGpuBuffer(BufferHandle handle, size_t offset, size_t size, void* dst) = 0;
```

旧 API (`CreateBuffer`, `CreateSSBO`, `CreateIndirectBuffer`, `BindSSBO`, `ReadSSBO`) 保留为 inline 转发，标 `[[deprecated]]`。

### Phase 3: Vulkan Indirect Buffer 实现

基于 `VulkanResourceManager::CreateBuffer` 现有逻辑，添加 `VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT`：

```cpp
BufferHandle VulkanRhiDevice::CreateGpuBuffer(const GpuBufferDesc& desc, const void* data) {
    VkBufferUsageFlags vk_usage = 0;
    if (has(desc.usage, kVertex))   vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (has(desc.usage, kIndex))    vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (has(desc.usage, kStorage))  vk_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (has(desc.usage, kIndirect)) vk_usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    // ... 复用现有 staging + memory alloc 逻辑
}
```

---

## 组合 Usage 兼容性矩阵

| 组合                      | OpenGL     | Vulkan     | DX11        | 备注 |
|---------------------------|------------|------------|-------------|------|
| kVertex                   | ✅          | ✅          | ✅           | — |
| kIndex                    | ✅          | ✅          | ✅           | — |
| kStorage                  | ✅ (4.3+)   | ✅          | ✅           | GL 3.3 fallback 到 UBO |
| kIndirect                 | ✅ (4.3+)   | ✅          | ✅ (11.1)    | — |
| kVertex \| kStorage       | ✅*         | ✅          | ❌           | *GL 需双重 target 绑定；DX11 无此能力 |
| kStorage \| kIndirect     | ✅ (4.3+)   | ✅          | ✅ (11.1)    | GPU culling 输出 indirect |
| kUniform                  | ✅          | ✅          | ✅           | 预留，暂由 UBOManager 管理 |

**约束**：`CreateGpuBuffer` 在 DX11 后端遇到不支持的组合时，返回空 handle 并 `DEBUG_LOG_WARN`。

---

## Debug 验证层

```cpp
// 仅在 DSE_DEBUG_HANDLES 宏启用时生效（Debug/RelWithDebInfo）
#ifdef DSE_DEBUG_HANDLES
template <typename Tag>
struct TypedHandle {
    uint32_t id         : 20;  // 1M 唯一 handle
    uint32_t generation : 12;  // 4096 代回绕检测 use-after-free
};

class HandleValidator {
    // 追踪所有活跃 handle，检测：
    // - Use-after-delete（generation 不匹配）
    // - Double-delete
    // - 帧结束时报告未释放 handle（leak detection）
};
#endif
```

Release 构建中 `TypedHandle` 仍为纯 `uint32_t`，零开销。

---

## 迁移顺序

| 步骤 | 工作量 | 风险 | 内容 |
|------|--------|------|------|
| 1    | 小     | 低   | 添加 `rhi_handle.h`（TypedHandle + hash + 验证层骨架），不改现有代码 |
| 2    | 中     | 低   | 新 API 并存（Create/Update/Delete/Bind/Read），旧 API deprecated |
| 3    | 中     | 中   | Vulkan indirect buffer 实现 + 测试 |
| 4    | 大     | 中   | 全量替换调用点 `unsigned int` → typed handle |
| 5    | 小     | 低   | 移除旧 API，清理 deprecated 标记 |
| 6    | 小     | 低   | VAO 相关接口迁移到 `VertexArrayHandle` |

---

## 测试策略

- 为 `TypedHandle` 添加编译期 static_assert 测试（sizeof == 4、trivially_copyable、trivially_destructible）
- 扩展现有 `rhi_test` 单元测试验证新 API 兼容性
- Vulkan indirect buffer: 添加 smoke test（创建 + 更新 + 多次绘制）
- 三后端回归截图对比确认无视觉差异
- Debug 验证层：编写专项测试触发 use-after-delete / double-delete，确认日志输出

---

## 备注

- 当前 `WINDOWS_EXPORT_ALL_SYMBOLS` 对模板类型有泄漏风险（参考 HiZTextureInfo LNK2001 修复），`TypedHandle` 因为是 trivial struct 应不受影响，但需要验证
- OpenGL 的 `indirect_buffers_` map 将统一到 ResourceManager 管理，与 DX11 对齐
- Vulkan 后端当前逐 buffer 调用 `vkAllocateMemory`，长期应集成 VMA（Vulkan Memory Allocator）以突破驱动 allocation limit（~4096）；本次迁移不强制引入 VMA，但统一 API 的设计为后续集成预留了空间
- `kUniform` flag 暂不实现，UBOManager 当前工作良好；未来若需 UBO 动态创建可直接启用
