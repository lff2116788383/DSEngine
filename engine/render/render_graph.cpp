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
    node.type = ResourceType::Logical;
    resources_.push_back(std::move(node));

    RenderResourceHandle handle{id};
    resource_by_name_[name] = handle;
    return handle;
}

RenderResourceHandle RenderGraph::DeclareTransient(const std::string& name, const RenderTargetDesc& desc) {
    auto it = resource_by_name_.find(name);
    if (it != resource_by_name_.end()) {
        return it->second;
    }

    uint32_t id = next_resource_id_++;
    ResourceNode node;
    node.id = id;
    node.name = name;
    node.type = ResourceType::Transient;
    node.desc = desc;
    resources_.push_back(std::move(node));

    RenderResourceHandle handle{id};
    resource_by_name_[name] = handle;
    return handle;
}

RenderResourceHandle RenderGraph::ImportResource(const std::string& name, unsigned int rt_handle) {
    auto it = resource_by_name_.find(name);
    if (it != resource_by_name_.end()) {
        // Update the handle if re-importing
        for (auto& res : resources_) {
            if (res.id == it->second.id) {
                res.rt_handle = rt_handle;
                res.type = ResourceType::Imported;
            }
        }
        return it->second;
    }

    uint32_t id = next_resource_id_++;
    ResourceNode node;
    node.id = id;
    node.name = name;
    node.type = ResourceType::Imported;
    node.rt_handle = rt_handle;
    resources_.push_back(std::move(node));

    RenderResourceHandle handle{id};
    resource_by_name_[name] = handle;
    return handle;
}

unsigned int RenderGraph::GetResourceRT(RenderResourceHandle resource) const {
    if (!resource.is_valid()) return 0;
    for (const auto& res : resources_) {
        if (res.id == resource.id) {
            return res.rt_handle;
        }
    }
    return 0;
}

void RenderGraph::SetRhiDevice(RhiDevice* device) {
    rhi_device_ = device;
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
            p.resource_states[resource.id] = ResourceState::ShaderRead;
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
            p.resource_states[resource.id] = ResourceState::RenderTarget;
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

RenderPassHandle RenderGraph::AddPass(const std::string& name,
                                       std::vector<ResourceAccess> reads,
                                       std::vector<ResourceAccess> writes,
                                       std::function<void(CommandBuffer&)> execute) {
    RenderPassHandle handle = AddPass(name);
    for (const auto& ra : reads) {
        if (ra.required_state != ResourceState::Undefined)
            PassReadWithState(handle, ra.resource, ra.required_state);
        else
            PassRead(handle, ra.resource);
    }
    for (const auto& wa : writes) {
        if (wa.required_state != ResourceState::Undefined)
            PassWriteWithState(handle, wa.resource, wa.required_state);
        else
            PassWrite(handle, wa.resource);
    }
    PassSetExecute(handle, std::move(execute));
    return handle;
}

void RenderGraph::PassReadWithState(RenderPassHandle pass, RenderResourceHandle resource, ResourceState state) {
    PassRead(pass, resource);
    if (!pass.is_valid() || !resource.is_valid()) return;
    for (auto& p : passes_) {
        if (p.id == pass.id) {
            p.resource_states[resource.id] = state;
            break;
        }
    }
}

void RenderGraph::PassWriteWithState(RenderPassHandle pass, RenderResourceHandle resource, ResourceState state) {
    PassWrite(pass, resource);
    if (!pass.is_valid() || !resource.is_valid()) return;
    for (auto& p : passes_) {
        if (p.id == pass.id) {
            p.resource_states[resource.id] = state;
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

    // ---- 0. WAW 冲突检测：同一资源被多个 Pass 写入 ----
    for (const auto& res : resources_) {
        if (res.writers.size() > 1) {
            is_compiled_ = false;
            return false;
        }
    }

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

    size_t queue_head = 0;
    while (queue_head < queue.size()) {
        uint32_t idx = queue[queue_head++];
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

    // ---- 4. 构建 id→index 映射（Execute 阶段 O(1) 查找） ----
    pass_id_to_idx_.clear();
    pass_id_to_idx_.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        pass_id_to_idx_[passes_[i].id] = i;
    }

    // ---- 5. 标记被剔除的 Pass 并生成编译顺序 ----
    for (auto& p : passes_) {
        p.is_culled = (reachable.find(p.id) == reachable.end());
    }

    for (uint32_t pass_id : topo_order) {
        auto it = pass_id_to_idx_.find(pass_id);
        if (it != pass_id_to_idx_.end() && !passes_[it->second].is_culled) {
            compiled_order_.push_back(pass_id);
        }
    }

    // ---- 6. 生命周期分析：计算每个资源的 [first_use, last_use] ----
    for (auto& res : resources_) {
        res.first_use = -1;
        res.last_use = -1;
    }
    for (int order_idx = 0; order_idx < static_cast<int>(compiled_order_.size()); ++order_idx) {
        uint32_t pass_id = compiled_order_[order_idx];
        auto pit = pass_id_to_idx_.find(pass_id);
        if (pit == pass_id_to_idx_.end()) continue;
        const auto& p = passes_[pit->second];
        auto update_lifetime = [&](const RenderResourceHandle& rh) {
            for (auto& res : resources_) {
                if (res.id == rh.id) {
                    if (res.first_use < 0) res.first_use = order_idx;
                    res.last_use = order_idx;
                    break;
                }
            }
        };
        for (const auto& r : p.reads)  update_lifetime(r);
        for (const auto& w : p.writes) update_lifetime(w);
    }

    // ---- 7. 为 Transient 资源分配物理 RT（带别名复用） ----
    if (rhi_device_) {
        // 按 first_use 排序的 transient 资源索引
        std::vector<size_t> transient_indices;
        for (size_t i = 0; i < resources_.size(); ++i) {
            if (resources_[i].type == ResourceType::Transient && resources_[i].first_use >= 0) {
                transient_indices.push_back(i);
            }
        }
        std::sort(transient_indices.begin(), transient_indices.end(),
            [this](size_t a, size_t b) { return resources_[a].first_use < resources_[b].first_use; });

        // 空闲池：{rt_handle, desc, available_after_order_idx}
        struct FreeSlot {
            unsigned int rt_handle;
            RenderTargetDesc desc;
            int free_after;
        };
        std::vector<FreeSlot> free_pool;

        for (size_t idx : transient_indices) {
            auto& res = resources_[idx];
            // 尝试从空闲池中找到 desc 匹配且已释放的 RT
            bool reused = false;
            for (auto it = free_pool.begin(); it != free_pool.end(); ++it) {
                if (it->free_after < res.first_use && it->desc == res.desc) {
                    res.rt_handle = it->rt_handle;
                    // 更新此 slot 的释放时间
                    it->free_after = res.last_use;
                    reused = true;
                    break;
                }
            }
            if (!reused) {
                res.rt_handle = rhi_device_->CreateRenderTarget(res.desc);
                free_pool.push_back({res.rt_handle, res.desc, res.last_use});
            }
        }
    }

    // ---- 8. 屏障分析：资源状态追踪 + 自动 RT 绑定 ----
    // 重置所有资源的编译状态
    for (auto& res : resources_) {
        res.compiled_state = ResourceState::Undefined;
    }
    // 清空所有 Pass 的编译输出
    for (auto& p : passes_) {
        p.pre_barriers.clear();
        p.auto_bind_rt = 0;
    }
    // 沿编译顺序遍历，计算转换
    for (uint32_t pass_id : compiled_order_) {
        auto pit = pass_id_to_idx_.find(pass_id);
        if (pit == pass_id_to_idx_.end()) continue;
        auto& p = passes_[pit->second];

        auto find_resource = [&](uint32_t res_id) -> ResourceNode* {
            for (auto& res : resources_) {
                if (res.id == res_id) return &res;
            }
            return nullptr;
        };

        // 检查该 Pass 所有读取/写入资源的状态要求
        for (const auto& [res_id, required] : p.resource_states) {
            if (required == ResourceState::Undefined) continue;
            ResourceNode* res = find_resource(res_id);
            if (!res) continue;

            // 状态变化 → 插入屏障
            if (res->compiled_state != required && res->compiled_state != ResourceState::Undefined) {
                p.pre_barriers.push_back({res->rt_handle, res->compiled_state, required});
            }

            // 自动 RT 绑定：写入状态为 RenderTarget 且有物理 RT
            if (required == ResourceState::RenderTarget && res->rt_handle != 0) {
                p.auto_bind_rt = res->rt_handle;
            }

            res->compiled_state = required;
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
        for (auto& p : passes_) {
            if (p.execute) {
                p.execute(cmd_buffer);
            }
        }
        return;
    }

    const bool use_gpu_timer = rhi_device_ && gpu_timing_enabled_
                               && rhi_device_->SupportsGpuTimer();

    for (uint32_t pass_id : compiled_order_) {
        auto it = pass_id_to_idx_.find(pass_id);
        if (it == pass_id_to_idx_.end()) continue;
        auto& p = passes_[it->second];
        if (!p.execute) continue;

        GpuTimerId timer_id = kInvalidGpuTimerId;
        if (use_gpu_timer) {
            timer_id = rhi_device_->GetOrCreateGpuTimer(p.name);
            rhi_device_->BeginGpuTimer(timer_id);
        }

        // 自动屏障插入
        if (rhi_device_) {
            for (const auto& b : p.pre_barriers) {
                rhi_device_->TransitionRenderTarget(b.rt_handle, b.from, b.to);
            }
        }

        // 自动 RT 绑定
        if (p.auto_bind_rt != 0) {
            RenderPassDesc rpd{};
            rpd.render_target = p.auto_bind_rt;
            cmd_buffer.BeginRenderPass(rpd);
        }

        p.execute(cmd_buffer);

        // 自动 RT 解绑
        if (p.auto_bind_rt != 0) {
            cmd_buffer.EndRenderPass();
        }

        if (timer_id != kInvalidGpuTimerId) {
            rhi_device_->EndGpuTimer(timer_id);
        }
    }
}

void RenderGraph::ExecuteWithCallback(CommandBuffer& cmd_buffer,
                                      const std::function<void(const std::string&)>& post_pass) {
    if (!is_compiled_) {
        for (auto& p : passes_) {
            if (p.execute) {
                p.execute(cmd_buffer);
                if (post_pass) post_pass(p.name);
            }
        }
        return;
    }

    const bool use_gpu_timer = rhi_device_ && gpu_timing_enabled_
                               && rhi_device_->SupportsGpuTimer();

    for (uint32_t pass_id : compiled_order_) {
        auto it = pass_id_to_idx_.find(pass_id);
        if (it == pass_id_to_idx_.end()) continue;
        auto& p = passes_[it->second];
        if (!p.execute) continue;

        GpuTimerId timer_id = kInvalidGpuTimerId;
        if (use_gpu_timer) {
            timer_id = rhi_device_->GetOrCreateGpuTimer(p.name);
            rhi_device_->BeginGpuTimer(timer_id);
        }

        // 自动屏障插入
        if (rhi_device_) {
            for (const auto& b : p.pre_barriers) {
                rhi_device_->TransitionRenderTarget(b.rt_handle, b.from, b.to);
            }
        }

        // 自动 RT 绑定
        if (p.auto_bind_rt != 0) {
            RenderPassDesc rpd{};
            rpd.render_target = p.auto_bind_rt;
            cmd_buffer.BeginRenderPass(rpd);
        }

        p.execute(cmd_buffer);

        // 自动 RT 解绑
        if (p.auto_bind_rt != 0) {
            cmd_buffer.EndRenderPass();
        }

        if (timer_id != kInvalidGpuTimerId) {
            rhi_device_->EndGpuTimer(timer_id);
        }

        if (post_pass) post_pass(p.name);
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
    // 释放 Transient 类型资源的物理 RT
    if (rhi_device_) {
        // 收集去重的 transient RT handle（alias 复用时多个资源共享同一 handle）
        std::unordered_set<unsigned int> freed;
        for (const auto& res : resources_) {
            if (res.type == ResourceType::Transient && res.rt_handle != 0) {
                if (freed.insert(res.rt_handle).second) {
                    rhi_device_->DeleteRenderTarget(res.rt_handle);
                }
            }
        }
    }
    passes_.clear();
    resources_.clear();
    resource_by_name_.clear();
    output_resources_.clear();
    compiled_order_.clear();
    pass_id_to_idx_.clear();
    next_resource_id_ = 1;
    next_pass_id_ = 1;
    is_compiled_ = false;
}

} // namespace render
} // namespace dse
