#ifndef UNTITLED_GRID_H
#define UNTITLED_GRID_H

#include "component/component.h"
#include <glm/glm.hpp>

class Grid : public Component {
public:
    Grid();
    ~Grid();

    void set_cell_size(glm::vec2 cell_size) { cell_size_ = cell_size; }
    glm::vec2 cell_size() const { return cell_size_; }

    void set_cell_gap(glm::vec2 cell_gap) { cell_gap_ = cell_gap; }
    glm::vec2 cell_gap() const { return cell_gap_; }

    // Convert cell coordinates to world position
    glm::vec3 CellToWorld(glm::ivec2 cell_pos);

    // Convert world position to cell coordinates
    glm::ivec2 WorldToCell(glm::vec3 world_pos);

    enum class CellLayout {
        Rectangle,
        Isometric
    };

    void set_cell_layout(CellLayout layout) { cell_layout_ = layout; }
    CellLayout cell_layout() const { return cell_layout_; }

private:
    glm::vec2 cell_size_;
    glm::vec2 cell_gap_;
    CellLayout cell_layout_ = CellLayout::Rectangle;

    RTTR_ENABLE(Component)
};

#endif //UNTITLED_GRID_H
