#include "math/quad_tree.h"

namespace math {

QuadTree::QuadTree(const AABB& boundary, int capacity, int max_depth)
    : QuadTree(boundary, capacity, max_depth, 0) {
}

QuadTree::QuadTree(const AABB& boundary, int capacity, int max_depth, int current_depth)
    : boundary_(boundary), capacity_(capacity), max_depth_(max_depth), current_depth_(current_depth), divided_(false) {
}

QuadTree::~QuadTree() {
}

bool QuadTree::Insert(const QuadTreeItem& item) {
    if (!boundary_.Intersects(item.bounds)) {
        return false;
    }

    if (items_.size() < capacity_ || current_depth_ == max_depth_) {
        items_.push_back(item);
        return true;
    }

    if (!divided_) {
        Subdivide();
    }

    if (northWest_->Insert(item)) return true;
    if (northEast_->Insert(item)) return true;
    if (southWest_->Insert(item)) return true;
    if (southEast_->Insert(item)) return true;

    return false;
}

void QuadTree::Subdivide() {
    float x = boundary_.min.x;
    float y = boundary_.min.y;
    float w = boundary_.max.x - boundary_.min.x;
    float h = boundary_.max.y - boundary_.min.y;
    
    float half_w = w / 2.0f;
    float half_h = h / 2.0f;

    AABB nw = { {x, y + half_h}, {x + half_w, y + h} };
    AABB ne = { {x + half_w, y + half_h}, {x + w, y + h} };
    AABB sw = { {x, y}, {x + half_w, y + half_h} };
    AABB se = { {x + half_w, y}, {x + w, y + half_h} };

    northWest_ = std::make_unique<QuadTree>(nw, capacity_, max_depth_, current_depth_ + 1);
    northEast_ = std::make_unique<QuadTree>(ne, capacity_, max_depth_, current_depth_ + 1);
    southWest_ = std::make_unique<QuadTree>(sw, capacity_, max_depth_, current_depth_ + 1);
    southEast_ = std::make_unique<QuadTree>(se, capacity_, max_depth_, current_depth_ + 1);

    divided_ = true;
}

void QuadTree::Query(const AABB& range, std::vector<Entity>& found) const {
    if (!boundary_.Intersects(range)) {
        return;
    }

    for (const auto& item : items_) {
        if (range.Intersects(item.bounds)) {
            found.push_back(item.entity);
        }
    }

    if (divided_) {
        northWest_->Query(range, found);
        northEast_->Query(range, found);
        southWest_->Query(range, found);
        southEast_->Query(range, found);
    }
}

void QuadTree::Clear() {
    items_.clear();
    divided_ = false;
    northWest_.reset();
    northEast_.reset();
    southWest_.reset();
    southEast_.reset();
}

} // namespace math
