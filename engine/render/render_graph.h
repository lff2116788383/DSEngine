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

class CommandBuffer;

namespace dse {
namespace render {

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

    /// 声明一个渲染资源
    /// @param name 资源名称（如 "scene_color", "shadow_depth"）
    /// @return 资源句柄
    RenderResourceHandle DeclareResource(const std::string& name);

    /// 添加一个渲染 Pass（返回 PassBuilder 风格的引用以支持链式调用）
    /// @param name Pass 名称
    /// @return Pass 句柄
    RenderPassHandle AddPass(const std::string& name);

    /// 为指定 Pass 声明读取资源
    void PassRead(RenderPassHandle pass, RenderResourceHandle resource);

    /// 为指定 Pass 声明写入资源
    void PassWrite(RenderPassHandle pass, RenderResourceHandle resource);

    /// 为指定 Pass 设置执行函数
    void PassSetExecute(RenderPassHandle pass, std::function<void(CommandBuffer&)> execute);

    /// 设置外部输出资源（这些资源所在 Pass 不会被剔除）
    /// @param resource 必须保留的外部输出资源
    void MarkOutput(RenderResourceHandle resource);

    /// 编译渲染图（拓扑排序 + 无用 Pass 剔除）
    /// @return true 编译成功，false 存在循环依赖
    bool Compile();

    /// 执行编译后的渲染图
    /// @param cmd_buffer 命令缓冲区
    void Execute(CommandBuffer& cmd_buffer);

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
    /// 内部资源节点
    struct ResourceNode {
        uint32_t id = 0;
        std::string name;
        /// 写入此资源的 Pass（最后一个写入者，用于依赖推断）
        std::vector<RenderPassHandle> writers;
        /// 读取此资源的 Pass 列表
        std::vector<RenderPassHandle> readers;
    };

    /// 内部 Pass 节点
    struct PassNode {
        uint32_t id = 0;
        std::string name;
        std::vector<RenderResourceHandle> reads;
        std::vector<RenderResourceHandle> writes;
        std::function<void(CommandBuffer&)> execute;
        bool is_culled = false;  ///< 编译后标记是否被剔除
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

    bool is_compiled_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_RENDER_GRAPH_H
