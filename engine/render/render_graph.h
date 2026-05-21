/**
* @file render_graph.h
* @brief 基于 DAG 的渲染图 - 支持依赖声明、拓扑排序与 Pass 自动剔除
*
* 核心能力：
* 1. Pass 声明读写资源，系统自动推断依赖关系
* 2. 拓扑排序确定执行顺序
* 3. 无输出被读取的 Pass 自动剔除
* 4. 与现有 FramePipeline / CommandBuffer 兼容
*
* @example
*   RenderGraph graph;
*   auto shadow_depth = graph.DeclareResource("shadow_depth");
*   auto scene_color  = graph.DeclareResource("scene_color");
*
*   graph.AddPass("ShadowMap")
*       .Write(shadow_depth)
*       .SetExecute([&](CommandBuffer& cmd) { // 阴影渲染 });
*
*   graph.AddPass("Forward")
*       .Read(shadow_depth)
*       .Write(scene_color)
*       .SetExecute([&](CommandBuffer& cmd) { // 前向渲染 });
*
*   graph.Compile();  // 拓扑排序 + 剔除
*   graph.Execute(cmd_buffer);
*/

#ifndef DSE_RENDER_RENDER_GRAPH_H
#define DSE_RENDER_RENDER_GRAPH_H

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include "engine/render/rhi/rhi_types.h"

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/// 渲染资源句柄
struct RenderResourceHandle {
    uint32_t id = 0;
    bool is_valid() const { return id != 0; }
    bool operator==(const RenderResourceHandle& other) const { return id == other.id; }
};

/// 渲染资源句柄哈希
struct RenderResourceHandleHash {
    size_t operator()(const RenderResourceHandle& h) const noexcept {
        return std::hash<uint32_t>{}(h.id);
    }
};

/// 渲染资源访问描述（Pass 声明读写依赖时使用）
struct ResourceAccess {
    RenderResourceHandle resource;
    ResourceState required_state;
};

/// 渲染 Pass 句柄
struct RenderPassHandle {
    uint32_t id = 0;
    bool is_valid() const { return id != 0; }
    bool operator==(const RenderPassHandle& other) const { return id == other.id; }
};

/**
* @class RenderGraph
* @brief 基于 DAG 的渲染图
*
* 设计原则：
* - 声明式：Pass 通过 Read/Write 声明资源依赖
* - 自动化：Compile 时自动拓扑排序和剔除无用 Pass
* - 兼容性：执行阶段使用 CommandBuffer，与现有 RHI 层无缝对接
*/
class RenderGraph {
public:
    RenderGraph() = default;
    ~RenderGraph() = default;

    // 禁止拷贝
    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    /// 声明一个纯逻辑渲染资源（不管理物理 RT，向后兼容）
    /// @param name 资源名称（如 "scene_color", "shadow_depth"）
    /// @return 资源句柄
    RenderResourceHandle DeclareResource(const std::string& name);

    /// 声明一个图内管理的瞬态资源，Compile 时自动分配/释放 RT
    /// @param name 资源名称
    /// @param desc RT 描述符
    /// @return 资源句柄
    RenderResourceHandle DeclareTransient(const std::string& name, const RenderTargetDesc& desc);

    /// 导入一个外部已分配的 RT（如 default framebuffer、编辑器共享纹理）
    /// @param name 资源名称
    /// @param rt_handle 已分配的 RT 句柄
    /// @return 资源句柄
    RenderResourceHandle ImportResource(const std::string& name, unsigned int rt_handle);

    /// 查询资源对应的物理 RT 句柄（Pass lambda 内使用）
    /// @param resource 资源句柄
    /// @return RT handle，无物理绑定时返回 0
    unsigned int GetResourceRT(RenderResourceHandle resource) const;

    /// 添加一个渲染 Pass（返回 PassBuilder 风格的引用以支持链式调用）
    /// @param name Pass 名称
    /// @return Pass 句柄
    RenderPassHandle AddPass(const std::string& name);

    /// 添加一个渲染 Pass，同时声明读写依赖与执行函数
    /// @param name   Pass 名称
    /// @param reads  该 Pass 读取的资源列表
    /// @param writes 该 Pass 写入的资源列表
    /// @param execute 执行函数
    /// @return Pass 句柄
    RenderPassHandle AddPass(const std::string& name,
                             std::vector<ResourceAccess> reads,
                             std::vector<ResourceAccess> writes,
                             std::function<void(CommandBuffer&)> execute);

    /// 为指定 Pass 声明读取资源（无状态，不参与自动屏障）
    void PassRead(RenderPassHandle pass, RenderResourceHandle resource);

    /// 为指定 Pass 声明写入资源（无状态，不参与自动屏障）
    void PassWrite(RenderPassHandle pass, RenderResourceHandle resource);

    /// 状态感知版本：声明读取并指定所需资源状态（参与自动屏障）
    void PassReadWithState(RenderPassHandle pass, RenderResourceHandle resource, ResourceState state);

    /// 状态感知版本：声明写入并指定目标资源状态
    /// state == RenderTarget 时自动绑定 RT；state == UnorderedAccess 时标记 UAV 写
    void PassWriteWithState(RenderPassHandle pass, RenderResourceHandle resource, ResourceState state);

    /// 为指定 Pass 设置执行函数
    void PassSetExecute(RenderPassHandle pass, std::function<void(CommandBuffer&)> execute);

    /// 设置外部输出资源（这些资源所在 Pass 不会被剔除）
    /// @param resource 必须保留的外部输出资源
    void MarkOutput(RenderResourceHandle resource);

    /// 设置 RHI 设备（用于 Compile 时自动分配瞬态 RT）
    void SetRhiDevice(RhiDevice* device);

    /// 编译渲染图（拓扑排序 + 无用 Pass 剔除 + 瞬态 RT 分配）
    /// @return true 编译成功，false 存在循环依赖
    bool Compile();

    /// 执行编译后的渲染图（单线程，顺序执行）
    /// @param cmd_buffer 命令缓冲区
    void Execute(CommandBuffer& cmd_buffer);

    /// 执行渲染图，每个 Pass 执行后调用回调（用于诊断）
    /// @param cmd_buffer 命令缓冲区
    /// @param post_pass 每个 pass 执行后调用，参数为 pass 名称
    void ExecuteWithCallback(CommandBuffer& cmd_buffer,
                             const std::function<void(const std::string&)>& post_pass);

    /// 重置渲染图（清空所有 Pass 和资源声明）
    void Reset();

    // --- 查询 ---

    /// 获取编译后实际执行的 Pass 数量
    size_t compiled_pass_count() const { return compiled_order_.size(); }

    /// 获取被剔除的 Pass 数量
    size_t culled_pass_count() const;

    /// 检查是否已编译
    bool is_compiled() const { return is_compiled_; }

private:
    /// 资源类型
    enum class ResourceType : uint8_t {
        Logical,    ///< 纯逻辑（无物理 RT，向后兼容）
        Transient,  ///< 图内管理（Compile 时分配，Reset 时释放）
        Imported    ///< 外部导入（图不管理生命周期）
    };

    /// 内部资源节点
    struct ResourceNode {
        uint32_t id = 0;
        std::string name;
        ResourceType type = ResourceType::Logical;
        RenderTargetDesc desc{};        ///< Transient 类型的 RT 描述
        unsigned int rt_handle = 0;      ///< 实际分配的 GPU RT（Transient/Imported）
        int first_use = -1;             ///< 生命周期：编译顺序中的首次使用位置
        int last_use = -1;              ///< 生命周期：编译顺序中的最后使用位置
        ResourceState compiled_state = ResourceState::Undefined; ///< Compile 时追踪的当前状态
        /// 写入此资源的 Pass（最后一个写入者，用于依赖推断）
        std::vector<RenderPassHandle> writers;
        /// 读取此资源的 Pass 列表
        std::vector<RenderPassHandle> readers;
    };

    /// 编译阶段生成的屏障描述
    struct BarrierEntry {
        unsigned int rt_handle;
        ResourceState from;
        ResourceState to;
    };

    /// 内部 Pass 节点
    struct PassNode {
        uint32_t id = 0;
        std::string name;
        std::vector<RenderResourceHandle> reads;
        std::vector<RenderResourceHandle> writes;
        std::function<void(CommandBuffer&)> execute;
        bool is_culled = false;  ///< 编译后标记是否被剔除
        /// 状态感知：resource_id → 该 Pass 对此资源要求的状态
        std::unordered_map<uint32_t, ResourceState> resource_states;
        /// Compile 输出：执行前需插入的屏障
        std::vector<BarrierEntry> pre_barriers;
        /// Compile 输出：自动绑定的 RT（0 = 不自动绑定）
        unsigned int auto_bind_rt = 0;
    };

    /// 递归标记可达 Pass（从外部输出反向追踪）
    void MarkReachablePasses(uint32_t pass_id,
                              std::unordered_set<uint32_t>& reachable);

    uint32_t next_resource_id_ = 1;
    uint32_t next_pass_id_ = 1;

    std::vector<PassNode> passes_;
    std::vector<ResourceNode> resources_;

    /// name → resource handle 映射
    std::unordered_map<std::string, RenderResourceHandle> resource_by_name_;

    /// 外部输出资源集合
    std::unordered_set<RenderResourceHandle, RenderResourceHandleHash> output_resources_;

    /// 编译后的执行顺序（pass id 列表）
    std::vector<uint32_t> compiled_order_;

    /// pass id → passes_ 索引映射（Compile 时构建，Execute 时 O(1) 查找）
    std::unordered_map<uint32_t, size_t> pass_id_to_idx_;

    /// RHI 设备指针（用于 Transient RT 分配，可为 null）
    RhiDevice* rhi_device_ = nullptr;

    bool is_compiled_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_RENDER_GRAPH_H
