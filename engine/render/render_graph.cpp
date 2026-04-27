/**
* @file render_graph.cpp
* @brief 基于 DAG 的渲染图实现 - 拓扑排序、依赖推断与 Pass 自动剔除
*/

#include "engine/render/render_graph.h"
#include "engine/render/rhi/rhi_device.h"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace dse {
namespace render {

// ============================================================
// 资源声明
// ============================================================

RenderResourceHandle RenderGraph::DeclareResource(const std::string& name) {
    auto it = resource_by_name_.find(name);
    if (it != resource_by_name_.end()) {
        return it->second;
    }

    uint32_t id = next_resource_id_++;
    ResourceNode node;
    node.id = id;
    node.name = name;
    resources_.push_back(std::move(node));

    RenderResourceHandle handle{id};
    resource_by_name_[name] = handle;
    return handle;
}

// ============================================================
// Pass 管理
// ============================================================

RenderPassHandle RenderGraph::AddPass(const std::string& name) {
    is_compiled_ = false;

    uint32_t id = next_pass_id_++;
    PassNode node;
    node.id = id;
    node.name = name;
    passes_.push_back(std::move(node));

    return RenderPassHandle{id};
}

void RenderGraph::PassRead(RenderPassHandle pass, RenderResourceHandle resource) {
    if (!pass.is_valid() || !resource.is_valid()) return;
    is_compiled_ = false;

    // 添加到 Pass 的读取列表
    for (auto& p : passes_) {
        if (p.id == pass.id) {
            // 去重
            for (auto& r : p.reads) {
                if (r.id == resource.id) return;
            }
            p.reads.push_back(resource);
            break;
        }
    }

    // 添加到资源的读者列表
    for (auto& res : resources_) {
        if (res.id == resource.id) {
            bool found = false;
            for (auto& r : res.readers) {
                if (r.id == pass.id) { found = true; break; }
            }
            if (!found) {
                res.readers.push_back(pass);
            }
            break;
        }
    }
}

void RenderGraph::PassWrite(RenderPassHandle pass, RenderResourceHandle resource) {
    if (!pass.is_valid() || !resource.is_valid()) return;
    is_compiled_ = false;

    // 添加到 Pass 的写入列表
    for (auto& p : passes_) {
        if (p.id == pass.id) {
            for (auto& w : p.writes) {
                if (w.id == resource.id) return;
            }
            p.writes.push_back(resource);
            break;
        }
    }

    // 添加到资源的写入者列表
    for (auto& res : resources_) {
        if (res.id == resource.id) {
            bool found = false;
            for (auto& w : res.writers) {
                if (w.id == pass.id) { found = true; break; }
            }
            if (!found) {
                res.writers.push_back(pass);
            }
            break;
        }
    }
}

void RenderGraph::PassSetExecute(RenderPassHandle pass, std::function<void(CommandBuffer&)> execute) {
    if (!pass.is_valid() || !execute) return;

    for (auto& p : passes_) {
        if (p.id == pass.id) {
            p.execute = std::move(execute);
            break;
        }
    }
}

void RenderGraph::MarkOutput(RenderResourceHandle resource) {
    if (!resource.is_valid()) return;
    output_resources_.insert(resource);
}

// ============================================================
// 编译（拓扑排序 + 剔除）
// ============================================================

bool RenderGraph::Compile() {
    compiled_order_.clear();

    if (passes_.empty()) {
        is_compiled_ = true;
        return true;
    }

    // ---- 1. 构建依赖图（Pass → Pass 的边） ----
    // 规则：若 Pass A 写入资源 R，Pass B 读取资源 R，则 A → B
    const size_t n = passes_.size();
    std::vector<std::vector<uint32_t>> adj(n);       // adj[i] = i 的后继列表
    std::vector<int> in_degree(n, 0);

    // Pass id → 索引映射
    std::unordered_map<uint32_t, size_t> id_to_idx;
    for (size_t i = 0; i < n; ++i) {
        id_to_idx[passes_[i].id] = i;
    }

    // 对每个资源，所有 writer → 所有 reader
    for (const auto& res : resources_) {
        for (const auto& writer_handle : res.writers) {
            for (const auto& reader_handle : res.readers) {
                auto wit = id_to_idx.find(writer_handle.id);
                auto rit = id_to_idx.find(reader_handle.id);
                if (wit == id_to_idx.end() || rit == id_to_idx.end()) continue;
                size_t wi = wit->second;
                size_t ri = rit->second;
                if (wi == ri) continue;

                // 检查是否已有此边
                bool exists = false;
                for (uint32_t succ : adj[wi]) {
                    if (succ == static_cast<uint32_t>(ri)) { exists = true; break; }
                }
                if (!exists) {
                    adj[wi].push_back(static_cast<uint32_t>(ri));
                    in_degree[ri]++;
                }
            }
        }
    }

    // ---- 2. Kahn 拓扑排序 ----
    std::vector<uint32_t> topo_order;
    topo_order.reserve(n);

    std::vector<uint32_t> queue;
    for (size_t i = 0; i < n; ++i) {
        if (in_degree[i] == 0) {
            queue.push_back(static_cast<uint32_t>(i));
        }
    }

    while (!queue.empty()) {
        uint32_t idx = queue.back();
        queue.pop_back();
        topo_order.push_back(passes_[idx].id);

        for (uint32_t succ : adj[idx]) {
            in_degree[succ]--;
            if (in_degree[succ] == 0) {
                queue.push_back(succ);
            }
        }
    }

    // 检测循环依赖
    if (topo_order.size() != n) {
        is_compiled_ = false;
        return false;
    }

    // ---- 3. 自动剔除：从外部输出资源反向追踪可达 Pass ----
    std::unordered_set<uint32_t> reachable;
    for (const auto& res_handle : output_resources_) {
        // 找到写入此资源的所有 Pass
        for (const auto& res : resources_) {
            if (res.id == res_handle.id) {
                for (const auto& writer : res.writers) {
                    MarkReachablePasses(writer.id, reachable);
                }
                // 读取此资源的 Pass 也应保留（外部消费者）
                for (const auto& reader : res.readers) {
                    MarkReachablePasses(reader.id, reachable);
                }
            }
        }
    }

    // 若未标记任何输出，则保留所有 Pass（兼容模式）
    if (output_resources_.empty()) {
        for (const auto& p : passes_) {
            reachable.insert(p.id);
        }
    }

    // ---- 4. 标记被剔除的 Pass 并生成编译顺序 ----
    for (auto& p : passes_) {
        p.is_culled = (reachable.find(p.id) == reachable.end());
    }

    for (uint32_t pass_id : topo_order) {
        for (const auto& p : passes_) {
            if (p.id == pass_id && !p.is_culled) {
                compiled_order_.push_back(pass_id);
                break;
            }
        }
    }

    is_compiled_ = true;
    return true;
}

void RenderGraph::MarkReachablePasses(uint32_t pass_id,
                                        std::unordered_set<uint32_t>& reachable) {
    if (reachable.count(pass_id) > 0) return;
    reachable.insert(pass_id);

    // 递归标记此 Pass 所读取资源的写入者
    for (const auto& p : passes_) {
        if (p.id != pass_id) continue;
        for (const auto& res_handle : p.reads) {
            for (const auto& res : resources_) {
                if (res.id == res_handle.id) {
                    for (const auto& writer : res.writers) {
                        MarkReachablePasses(writer.id, reachable);
                    }
                }
            }
        }
    }
}

// ============================================================
// 执行
// ============================================================

void RenderGraph::Execute(CommandBuffer& cmd_buffer) {
    if (!is_compiled_) {
        // 未编译时回退：按添加顺序执行所有 Pass
        for (auto& p : passes_) {
            if (p.execute) {
                p.execute(cmd_buffer);
            }
        }
        return;
    }

    for (uint32_t pass_id : compiled_order_) {
        for (const auto& p : passes_) {
            if (p.id == pass_id && p.execute) {
                p.execute(cmd_buffer);
                break;
            }
        }
    }
}

// ============================================================
// 查询与重置
// ============================================================

size_t RenderGraph::culled_pass_count() const {
    size_t count = 0;
    for (const auto& p : passes_) {
        if (p.is_culled) ++count;
    }
    return count;
}

void RenderGraph::Reset() {
    passes_.clear();
    resources_.clear();
    resource_by_name_.clear();
    output_resources_.clear();
    compiled_order_.clear();
    next_resource_id_ = 1;
    next_pass_id_ = 1;
    is_compiled_ = false;
}

} // namespace render
} // namespace dse
