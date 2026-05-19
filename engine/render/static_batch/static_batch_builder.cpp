/**
 * @file static_batch_builder.cpp
 * @brief StaticBatchBuilder 实现
 */

#include "engine/render/static_batch/static_batch_builder.h"
#include <algorithm>

namespace dse {
namespace render {

StaticBatchBuilder::Group& StaticBatchBuilder::FindOrCreateGroup(
    uint64_t sort_key,
    unsigned int needed_vertices,
    const MeshDrawItem& template_item)
{
    // 从后往前找同 sort_key 且容得下 needed_vertices 的组
    for (int i = static_cast<int>(groups_.size()) - 1; i >= 0; --i) {
        Group& g = groups_[i];
        if (g.sort_key != sort_key) continue;
        const unsigned int cur_count = static_cast<unsigned int>(g.merged.vertices.size());
        if (cur_count + needed_vertices <= kMaxVerticesPerBatch) {
            return g;
        }
    }
    // 创建新组，以 template_item 的材质属性初始化（清空顶点/索引/instance_transforms）
    Group newg;
    newg.sort_key = sort_key;
    newg.merged = template_item;
    newg.merged.vertices.clear();
    newg.merged.indices.clear();
    newg.merged.instance_transforms.clear();
    newg.merged.model = glm::mat4(1.0f);
    newg.merged.skinned = false;
    newg.merged.morph_enabled = false;
    newg.merged.bone_matrices.clear();
    newg.merged.morph_weights.clear();
    groups_.push_back(std::move(newg));
    return groups_.back();
}

void StaticBatchBuilder::Add(const MeshDrawItem& item) {
    if (item.vertices.empty() || item.indices.empty()) return;

    const uint64_t key = MakeSortKey(item);
    const unsigned int needed = static_cast<unsigned int>(item.vertices.size());
    Group& g = FindOrCreateGroup(key, needed, item);

    const auto base_vertex = static_cast<unsigned short>(g.merged.vertices.size());
    g.merged.vertices.insert(g.merged.vertices.end(),
                              item.vertices.begin(), item.vertices.end());
    for (unsigned short idx : item.indices) {
        g.merged.indices.push_back(static_cast<unsigned short>(idx + base_vertex));
    }
}

std::vector<MeshDrawItem> StaticBatchBuilder::Build() {
    std::vector<MeshDrawItem> result;
    result.reserve(groups_.size());
    for (auto& g : groups_) {
        result.push_back(std::move(g.merged));
    }
    return result;
}

} // namespace render
} // namespace dse
