/**
 * @file static_batch_builder.h
 * @brief 静态网格合批工具 — CPU 侧将同材质静态物体的顶点索引数据合并，减少 DrawCall
 *
 * 用法：
 *   StaticBatchBuilder builder;
 *   builder.Add(draw_item);          // draw_item.vertices 已在世界空间
 *   auto batches = builder.Build();  // 按材质键合并 → vector<MeshDrawItem>
 *
 * 限制：每个批次最多 65535 顶点（unsigned short 索引上限）；
 *       超出后自动开启新批次（相同材质键）。
 */

#pragma once

#include <vector>
#include <cstdint>
#include "engine/render/rhi/rhi_types.h"

namespace dse {
namespace render {

/**
 * @class StaticBatchBuilder
 * @brief 按 MakeSortKey 对静态 MeshDrawItem 进行合批的构建器
 *
 * 典型使用场景：MeshRenderSystem 在首帧收集 is_static 物体，
 * 调用 Add() 后调用 Build() 得到合并后的绘制项列表，缓存至后续帧复用。
 */
class StaticBatchBuilder {
public:
    StaticBatchBuilder() = default;
    ~StaticBatchBuilder() = default;

    /**
     * @brief 添加一个静态网格绘制项（顶点须已转换至世界空间，model 可为任意值）
     *
     * 若当前批次顶点数加上新项顶点数超过 65535，会自动创建同键的新批次。
     * @param item  世界空间顶点 + 材质属性填充完整的 MeshDrawItem（model 将被忽略）
     */
    void Add(const MeshDrawItem& item);

    /**
     * @brief 构建并返回合并后的绘制项列表
     *
     * 每个返回项：
     * - vertices  : 世界空间合并顶点
     * - indices   : 偏移后的合并索引
     * - model     : 单位矩阵（顶点已在世界空间）
     * - 材质属性  : 与合批来源相同
     *
     * @return 合并后的 MeshDrawItem 列表，每项代表一个材质批次
     */
    std::vector<MeshDrawItem> Build();

    void Clear() { groups_.clear(); }
    bool empty() const { return groups_.empty(); }
    int group_count() const { return static_cast<int>(groups_.size()); }

private:
    static constexpr unsigned int kMaxVerticesPerBatch = 65535u;

    struct Group {
        uint64_t sort_key = 0;
        MeshDrawItem merged;  ///< 累积合并结果，model=identity，顶点已在世界空间
    };

    std::vector<Group> groups_;

    /// 找到或创建适合添加 needed_vertices 个顶点的组（同一 sort_key 下可有多个分段）
    Group& FindOrCreateGroup(uint64_t sort_key, unsigned int needed_vertices,
                              const MeshDrawItem& template_item);
};

} // namespace render
} // namespace dse
