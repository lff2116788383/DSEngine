#include "grid.h"
#include "component/transform.h"
#include "component/game_object.h"

using namespace rttr;

RTTR_REGISTRATION
{
    registration::class_<Grid>("Grid")
        .constructor<>()(rttr::policy::ctor::as_raw_ptr)
        .property("cell_size", &Grid::cell_size, &Grid::set_cell_size)
        .property("cell_gap", &Grid::cell_gap, &Grid::set_cell_gap)
        .property("cell_layout", &Grid::cell_layout, &Grid::set_cell_layout);
    
    registration::enumeration<Grid::CellLayout>("CellLayout")
        (
            value("Rectangle", Grid::CellLayout::Rectangle),
            value("Isometric", Grid::CellLayout::Isometric)
        );
}

Grid::Grid() : Component() {
    cell_size_ = glm::vec2(1.0f, 1.0f);
    cell_gap_ = glm::vec2(0.0f, 0.0f);
    cell_layout_ = CellLayout::Rectangle;
}

Grid::~Grid() {
}

glm::vec3 Grid::CellToWorld(glm::ivec2 cell_pos) {
    Transform* transform = game_object()->GetComponent<Transform>();
    glm::vec3 origin = glm::vec3(0.0f);
    if (transform) {
        origin = transform->position();
    }

    float x = 0.0f;
    float y = 0.0f;

    if (cell_layout_ == CellLayout::Isometric) {
        // Isometric Projection (Diamond)
        // ScreenX = (MapX - MapY) * (TileWidth / 2)
        // ScreenY = (MapX + MapY) * (TileHeight / 2)
        float half_w = (cell_size_.x + cell_gap_.x) * 0.5f;
        float half_h = (cell_size_.y + cell_gap_.y) * 0.5f;
        x = (cell_pos.x - cell_pos.y) * half_w;
        y = (cell_pos.x + cell_pos.y) * half_h;
        // In Flare, Y is down (positive), but usually in 2D engines Y is up.
        // Assuming standard Cartesian:
        // Wait, standard isometric usually has Y going "down-right" and X going "down-left" on screen?
        // Let's stick to standard formula.
        // If Y is up in DSEngine:
        // x = (gridX - gridY) * halfW
        // y = (gridX + gridY) * halfH * -1 (inverted Y) ?
        
        // Let's use standard for now and adjust via Camera or negative scale if needed.
        y = -(cell_pos.x + cell_pos.y) * half_h; // Invert Y to match typical 2D top-down where +Y is up
    } else {
        x = cell_pos.x * (cell_size_.x + cell_gap_.x);
        y = cell_pos.y * (cell_size_.y + cell_gap_.y);
    }
    
    return origin + glm::vec3(x, y, 0.0f);
}

glm::ivec2 Grid::WorldToCell(glm::vec3 world_pos) {
    Transform* transform = game_object()->GetComponent<Transform>();
    glm::vec3 origin = glm::vec3(0.0f);
    if (transform) {
        origin = transform->position();
    }
    
    glm::vec3 diff = world_pos - origin;
    
    if (cell_layout_ == CellLayout::Isometric) {
        // Inverse Isometric
        // x_screen = (x_grid - y_grid) * half_w
        // y_screen = -(x_grid + y_grid) * half_h
        //
        // x_screen / half_w = x_grid - y_grid  (A)
        // -y_screen / half_h = x_grid + y_grid (B)
        //
        // (A) + (B) => x_screen/half_w - y_screen/half_h = 2 * x_grid
        // (B) - (A) => -y_screen/half_h - x_screen/half_w = 2 * y_grid
        
        float half_w = (cell_size_.x + cell_gap_.x) * 0.5f;
        float half_h = (cell_size_.y + cell_gap_.y) * 0.5f;
        
        float x_grid = (diff.x / half_w - diff.y / half_h) * 0.5f;
        float y_grid = (-diff.y / half_h - diff.x / half_w) * 0.5f;
        
        return glm::ivec2((int)round(x_grid), (int)round(y_grid));
    } else {
        int x = (int)floor(diff.x / (cell_size_.x + cell_gap_.x));
        int y = (int)floor(diff.y / (cell_size_.y + cell_gap_.y));
        return glm::ivec2(x, y);
    }
}
