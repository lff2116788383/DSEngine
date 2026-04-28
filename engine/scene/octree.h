/**
 * @file octree.h
 * @brief 3D 空间划分结构(八叉树)，用于加速三维空间内的视锥体剔除和碰撞检测
 */

#ifndef DSE_SCENE_OCTREE_H
#define DSE_SCENE_OCTREE_H

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <entt/entt.hpp>

namespace dse {
namespace scene {

struct AABB {
    glm::vec3 min_extents;
    glm::vec3 max_extents;

    bool Contains(const glm::vec3& point) const {
        return point.x >= min_extents.x && point.x <= max_extents.x &&
               point.y >= min_extents.y && point.y <= max_extents.y &&
               point.z >= min_extents.z && point.z <= max_extents.z;
    }

    bool Intersects(const AABB& other) const {
        return !(other.min_extents.x > max_extents.x ||
                 other.max_extents.x < min_extents.x ||
                 other.min_extents.y > max_extents.y ||
                 other.max_extents.y < min_extents.y ||
                 other.min_extents.z > max_extents.z ||
                 other.max_extents.z < min_extents.z);
    }

    bool Contains(const AABB& other) const {
        return other.min_extents.x >= min_extents.x && other.max_extents.x <= max_extents.x &&
               other.min_extents.y >= min_extents.y && other.max_extents.y <= max_extents.y &&
               other.min_extents.z >= min_extents.z && other.max_extents.z <= max_extents.z;
    }
};

struct OctreeData {
    entt::entity entity;
    AABB bounds;
};

/**
 * @class Octree
 * @brief 八叉树节点结构，管理指定空间范围内的实体数据，并在超出容量时自动细分
 */
class Octree {
public:
    Octree(const AABB& bounds, int capacity = 8, int max_depth = 5, int depth = 0)
        : bounds_(bounds), capacity_(capacity), max_depth_(max_depth), depth_(depth) {}

    /**
     * @brief 插入实体数据到八叉树中
     * @param data 包含实体和其包围盒的数据
     */
    void Insert(const OctreeData& data) {
        if (!bounds_.Intersects(data.bounds)) {
            return;
        }

        if (divided_) {
            if (InsertIntoContainingChild(data)) {
                return;
            }
        }

        elements_.push_back(data);

        if (elements_.size() > capacity_ && depth_ < max_depth_) {
            Subdivide();
            std::vector<OctreeData> remaining;
            remaining.reserve(elements_.size());
            for (const auto& elem : elements_) {
                if (!InsertIntoContainingChild(elem)) {
                    remaining.push_back(elem);
                }
            }
            elements_ = std::move(remaining);
        }
    }

    /**
     * @brief 查询与指定范围相交的所有实体
     * @param range 查询包围盒
     * @param found 存放查询结果的数组
     */
    void Query(const AABB& range, std::vector<OctreeData>& found) const {
        if (!bounds_.Intersects(range)) {
            return;
        }

        for (const auto& elem : elements_) {
            if (range.Intersects(elem.bounds)) {
                found.push_back(elem);
            }
        }

        if (divided_) {
            for (int i = 0; i < 8; ++i) {
                children_[i]->Query(range, found);
            }
        }
    }

    /**
     * @brief 执行 Clear 操作
     */
    void Clear() {
        elements_.clear();
        divided_ = false;
        for (int i = 0; i < 8; ++i) {
            children_[i].reset();
        }
    }

    const AABB& GetBounds() const { return bounds_; }
    bool IsDivided() const { return divided_; }
    const std::unique_ptr<Octree>& GetChild(int index) const { return children_[index]; }
    const std::vector<OctreeData>& GetElements() const { return elements_; }

private:
    bool InsertIntoContainingChild(const OctreeData& data) {
        for (int i = 0; i < 8; ++i) {
            if (children_[i] && children_[i]->bounds_.Contains(data.bounds)) {
                children_[i]->Insert(data);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 执行 Subdivide 操作
     */
    void Subdivide() {
        glm::vec3 min_e = bounds_.min_extents;
        glm::vec3 max_e = bounds_.max_extents;
        glm::vec3 center = (min_e + max_e) * 0.5f;

        children_[0] = std::make_unique<Octree>(AABB{glm::vec3(min_e.x, center.y, center.z), glm::vec3(center.x, max_e.y, max_e.z)}, capacity_, max_depth_, depth_ + 1);
        children_[1] = std::make_unique<Octree>(AABB{glm::vec3(center.x, center.y, center.z), glm::vec3(max_e.x, max_e.y, max_e.z)}, capacity_, max_depth_, depth_ + 1);
        children_[2] = std::make_unique<Octree>(AABB{glm::vec3(min_e.x, min_e.y, center.z), glm::vec3(center.x, center.y, max_e.z)}, capacity_, max_depth_, depth_ + 1);
        children_[3] = std::make_unique<Octree>(AABB{glm::vec3(center.x, min_e.y, center.z), glm::vec3(max_e.x, center.y, max_e.z)}, capacity_, max_depth_, depth_ + 1);
        children_[4] = std::make_unique<Octree>(AABB{glm::vec3(min_e.x, center.y, min_e.z), glm::vec3(center.x, max_e.y, center.z)}, capacity_, max_depth_, depth_ + 1);
        children_[5] = std::make_unique<Octree>(AABB{glm::vec3(center.x, center.y, min_e.z), glm::vec3(max_e.x, max_e.y, center.z)}, capacity_, max_depth_, depth_ + 1);
        children_[6] = std::make_unique<Octree>(AABB{glm::vec3(min_e.x, min_e.y, min_e.z), glm::vec3(center.x, center.y, center.z)}, capacity_, max_depth_, depth_ + 1);
        children_[7] = std::make_unique<Octree>(AABB{glm::vec3(center.x, min_e.y, min_e.z), glm::vec3(max_e.x, center.y, center.z)}, capacity_, max_depth_, depth_ + 1);

        divided_ = true;
    }

    AABB bounds_;
    int capacity_;
    int max_depth_;
    int depth_;
    std::vector<OctreeData> elements_;
    bool divided_ = false;

    std::unique_ptr<Octree> children_[8];
};

} // namespace scene
} // namespace dse

#endif // DSE_SCENE_OCTREE_H