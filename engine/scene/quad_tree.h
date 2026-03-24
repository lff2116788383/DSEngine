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
};

struct QuadTreeData {
    entt::entity entity;
    Rect bounds;
};

class QuadTree {
public:
    QuadTree(const Rect& bounds, int capacity = 4, int max_depth = 5, int depth = 0)
        : bounds_(bounds), capacity_(capacity), max_depth_(max_depth), depth_(depth) {}

    void Insert(const QuadTreeData& data) {
        if (!bounds_.Intersects(data.bounds)) {
            return;
        }

        if (divided_) {
            top_left_->Insert(data);
            top_right_->Insert(data);
            bottom_left_->Insert(data);
            bottom_right_->Insert(data);
            return;
        }

        elements_.push_back(data);

        if (elements_.size() > capacity_ && depth_ < max_depth_) {
            Subdivide();
            for (const auto& elem : elements_) {
                top_left_->Insert(elem);
                top_right_->Insert(elem);
                bottom_left_->Insert(elem);
                bottom_right_->Insert(elem);
            }
            elements_.clear();
        }
    }

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

    void Clear() {
        elements_.clear();
        divided_ = false;
        top_left_.reset();
        top_right_.reset();
        bottom_left_.reset();
        bottom_right_.reset();
    }

private:
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
