#include "tilemap.h"
#include "app/application.h"
#include <fstream>
#include <sstream>

using namespace rttr;

RTTR_REGISTRATION
{
    registration::class_<Tilemap>("Tilemap")
        .constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

Tilemap::Tilemap() : Component() {
}

Tilemap::~Tilemap() {
    tiles_.clear();
}

void Tilemap::SetTile(glm::ivec2 cell_pos, Sprite* sprite) {
    if (sprite == nullptr) {
        tiles_.erase({cell_pos.x, cell_pos.y});
    } else {
        tiles_[{cell_pos.x, cell_pos.y}] = sprite;
    }
    version_++;
}

Sprite* Tilemap::GetTile(glm::ivec2 cell_pos) {
    auto it = tiles_.find({cell_pos.x, cell_pos.y});
    if (it != tiles_.end()) {
        return it->second;
    }
    return nullptr;
}

void Tilemap::ClearAllTiles() {
    tiles_.clear();
    version_++;
}

bool Tilemap::SaveToFile(const std::string& file_path) {
    std::ofstream output(Application::data_path() + file_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    for (auto& pair : tiles_) {
        Sprite* sprite = pair.second;
        if (!sprite) {
            continue;
        }
        std::string texture_path = sprite->texture_path();
        if (texture_path.empty()) {
            continue;
        }
        Sprite::Rect rect = sprite->rect();
        Sprite::Vector2 pivot = sprite->pivot();
        output << pair.first.first << " " << pair.first.second << " " << texture_path << " "
               << rect.x << " " << rect.y << " " << rect.width << " " << rect.height << " "
               << pivot.x << " " << pivot.y << " " << sprite->ppu() << "\n";
    }
    return true;
}

bool Tilemap::LoadFromFile(const std::string& file_path) {
    std::ifstream input(Application::data_path() + file_path);
    if (!input.is_open()) {
        return false;
    }

    tiles_.clear();

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream stream(line);
        int x = 0;
        int y = 0;
        std::string texture_path;
        float rect_x = 0.0f;
        float rect_y = 0.0f;
        float rect_w = 0.0f;
        float rect_h = 0.0f;
        float pivot_x = 0.5f;
        float pivot_y = 0.5f;
        float ppu = 100.0f;
        if (!(stream >> x >> y >> texture_path >> rect_x >> rect_y >> rect_w >> rect_h >> pivot_x >> pivot_y >> ppu)) {
            continue;
        }

        Sprite* sprite = Sprite::CreateFromAtlas(texture_path, rect_x, rect_y, rect_w, rect_h, pivot_x, pivot_y, ppu);
        if (!sprite) {
            continue;
        }
        tiles_[{x, y}] = sprite;
    }

    version_++;
    return true;
}
