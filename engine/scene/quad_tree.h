/**
 * @file quad_tree.h
 * @brief 2D 空间划分结构(四叉树)，用于加速二维空间内的碰撞检测和视锥体剔除
 */

#ifndef DSE_SCENE_QUAD_TREE_H
#define DSE_SCENE_QUAD_TREE_H

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <entt/entt.hpp>

namespace dse {
namespace scene {

struct Rect {
    float x, y, width, height;

    bool Contains(const glm::vec2& point) const {
        return point.x >= x && point.x <= x + width &&
               point.y >= y && point.y <= y + height;
    }

    bool Intersects(const Rect& other) const {
        return !(other.x > x + width ||
                 other.x + other.width < x ||
                 other.y > y + height ||
                 other.y + other.height < y);
    }

    bool Contains(const Rect& other) const {
        return other.x >= x && other.x + other.width <= x + width &&
               other.y >= y && other.y + other.height <= y + height;
    }
};

struct QuadTreeData {
    entt::entity entity;
    Rect bounds;
};

/**
 * @class QuadTree
 * @brief 四叉树节点结构，管理指定空间范围内的实体数据，并在超出容量时自动细分
 */
class QuadTree {
public:
    QuadTree(const Rect& bounds, int capacity = 4, int max_depth = 5, int depth = 0)
        : bounds_(bounds), capacity_(capacity), max_depth_(max_depth), depth_(depth) {}

    /**
     * @brief 插入实体数据到四叉树中
     * @param data 包含实体和其包围盒的数据
     */
    void Insert(const QuadTreeData& data) {
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
            std::vector<QuadTreeData> remaining;
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
    void Query(const Rect& range, std::vector<QuadTreeData>& found) const {
        if (!bounds_.Intersects(range)) {
            return;
        }

        for (const auto& elem : elements_) {
            if (range.Intersects(elem.bounds)) {
                found.push_back(elem);
            }
        }

        if (divided_) {
            top_left_->Query(range, found);
            top_right_->Query(range, found);
            bottom_left_->Query(range, found);
            bottom_right_->Query(range, found);
        }
    }

    /**
     * @brief 执行 Clear 操作
     */
    void Clear() {
        elements_.clear();
        divided_ = false;
        top_left_.reset();
        top_right_.reset();
        bottom_left_.reset();
        bottom_right_.reset();
    }

private:
    bool InsertIntoContainingChild(const QuadTreeData& data) {
        if (top_left_ && top_left_->bounds_.Contains(data.bounds)) {
            top_left_->Insert(data);
            return true;
        }
        if (top_right_ && top_right_->bounds_.Contains(data.bounds)) {
            top_right_->Insert(data);
            return true;
        }
        if (bottom_left_ && bottom_left_->bounds_.Contains(data.bounds)) {
            bottom_left_->Insert(data);
            return true;
        }
        if (bottom_right_ && bottom_right_->bounds_.Contains(data.bounds)) {
            bottom_right_->Insert(data);
            return true;
        }
        return false;
    }

    /**
     * @brief 执行 Subdivide 操作
     */
    void Subdivide() {
        float x = bounds_.x;
        float y = bounds_.y;
        float w = bounds_.width / 2.0f;
        float h = bounds_.height / 2.0f;

        top_left_ = std::make_unique<QuadTree>(Rect{x, y + h, w, h}, capacity_, max_depth_, depth_ + 1);
        top_right_ = std::make_unique<QuadTree>(Rect{x + w, y + h, w, h}, capacity_, max_depth_, depth_ + 1);
        bottom_left_ = std::make_unique<QuadTree>(Rect{x, y, w, h}, capacity_, max_depth_, depth_ + 1);
        bottom_right_ = std::make_unique<QuadTree>(Rect{x + w, y, w, h}, capacity_, max_depth_, depth_ + 1);

        divided_ = true;
    }

    Rect bounds_;
    int capacity_;
    int max_depth_;
    int depth_;
    std::vector<QuadTreeData> elements_;
    bool divided_ = false;

    std::unique_ptr<QuadTree> top_left_;
    std::unique_ptr<QuadTree> top_right_;
    std::unique_ptr<QuadTree> bottom_left_;
    std::unique_ptr<QuadTree> bottom_right_;
};

} // namespace scene
} // namespace dse

#endif // DSE_SCENE_QUAD_TREE_H
