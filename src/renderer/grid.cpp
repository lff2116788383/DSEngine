#include "grid.h"
#include "component/transform.h"
#include "component/game_object.h"

using namespace rttr;

RTTR_REGISTRATION
{
    registration::class_<Grid>("Grid")
        .constructor<>()(rttr::policy::ctor::as_raw_ptr)
        .property("cell_size", &Grid::cell_size, &Grid::set_cell_size)
        .property("cell_gap", &Grid::cell_gap, &Grid::set_cell_gap);
}

Grid::Grid() : Component() {
    cell_size_ = glm::vec2(1.0f, 1.0f);
    cell_gap_ = glm::vec2(0.0f, 0.0f);
}

Grid::~Grid() {
}

glm::vec3 Grid::CellToWorld(glm::ivec2 cell_pos) {
    Transform* transform = game_object()->GetComponent<Transform>();
    glm::vec3 origin = glm::vec3(0.0f);
    if (transform) {
        origin = transform->position();
    }

    float x = cell_pos.x * (cell_size_.x + cell_gap_.x);
    float y = cell_pos.y * (cell_size_.y + cell_gap_.y);
    
    // Simple 2D implementation: origin + offset
    // This assumes no rotation for now to keep it simple, but we should revisit if we want rotated grids
    return origin + glm::vec3(x, y, 0.0f);
}

glm::ivec2 Grid::WorldToCell(glm::vec3 world_pos) {
    Transform* transform = game_object()->GetComponent<Transform>();
    glm::vec3 origin = glm::vec3(0.0f);
    if (transform) {
        origin = transform->position();
    }
    
    glm::vec3 diff = world_pos - origin;
    
    int x = (int)floor(diff.x / (cell_size_.x + cell_gap_.x));
    int y = (int)floor(diff.y / (cell_size_.y + cell_gap_.y));
    
    return glm::ivec2(x, y);
}
