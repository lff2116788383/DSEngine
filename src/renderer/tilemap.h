#ifndef UNTITLED_TILEMAP_H
#define UNTITLED_TILEMAP_H

#include "component/component.h"
#include "renderer/sprite.h"
#include "renderer/grid.h"
#include <map>
#include <string>
#include <vector>

class Tilemap : public Component {
public:
    Tilemap();
    ~Tilemap();

    // Set a sprite for a specific cell coordinate
    void SetTile(glm::ivec2 cell_pos, Sprite* sprite);
    
    // Get the sprite at a specific cell coordinate
    Sprite* GetTile(glm::ivec2 cell_pos);
    
    // Clear all tiles
    void ClearAllTiles();

    bool SaveToFile(const std::string& file_path);
    bool LoadFromFile(const std::string& file_path);

    // Iterate over all tiles
    // Callback: void(glm::ivec2 pos, Sprite* sprite)
    template<typename Func>
    void ForeachTile(Func func) {
        for (auto& pair : tiles_) {
            // Unpack key from pair<int, int>
            glm::ivec2 pos(pair.first.first, pair.first.second);
            func(pos, pair.second);
        }
    }

    const std::map<std::pair<int, int>, Sprite*>& tiles() const { return tiles_; }

    // Versioning for efficient updates
    size_t version() const { return version_; }

private:
    // Using std::pair<int, int> as key because glm::ivec2 doesn't have default hash in standard map
    // We could use a custom hash for unordered_map, but map is fine for now.
    std::map<std::pair<int, int>, Sprite*> tiles_;
    size_t version_ = 0;

    RTTR_ENABLE(Component)
};

#endif //UNTITLED_TILEMAP_H
