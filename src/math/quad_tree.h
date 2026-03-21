#ifndef DSE_MATH_QUAD_TREE_H
#define DSE_MATH_QUAD_TREE_H

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "phase1/ecs/world.h" // For Entity

namespace math {

struct AABB {
    glm::vec2 min;
    glm::vec2 max;

    bool Intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y);
    }
    
    bool Contains(const glm::vec2& point) const {
        return (point.x >= min.x && point.x <= max.x) &&
               (point.y >= min.y && point.y <= max.y);
    }
};

struct QuadTreeItem {
    Entity entity;
    AABB bounds;
};

class QuadTree {
public:
    QuadTree(const AABB& boundary, int capacity = 4, int max_depth = 5);
    ~QuadTree();

    bool Insert(const QuadTreeItem& item);
    void Subdivide();
    void Query(const AABB& range, std::vector<Entity>& found) const;
    void Clear();

private:
    AABB boundary_;
    int capacity_;
    int max_depth_;
    int current_depth_;

    std::vector<QuadTreeItem> items_;
    
    std::unique_ptr<QuadTree> northWest_;
    std::unique_ptr<QuadTree> northEast_;
    std::unique_ptr<QuadTree> southWest_;
    std::unique_ptr<QuadTree> southEast_;

    bool divided_;

    QuadTree(const AABB& boundary, int capacity, int max_depth, int current_depth);
};

} // namespace math

#endif // DSE_MATH_QUAD_TREE_H
