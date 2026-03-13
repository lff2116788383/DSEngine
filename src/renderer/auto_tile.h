#ifndef DSE_AUTO_TILE_H
#define DSE_AUTO_TILE_H

#include "resource/resource.h"
#include "renderer/texture_2d.h"
#include <vector>
#include <unordered_map>

class AutoTileRule : public Resource {
public:
    enum class Connection {
        None = 0,
        Connect = 1,
        Ignore = 2
    };

    struct Rule {
        Connection neighbors[8]; // TL, T, TR, L, R, BL, B, BR
        int tile_index;
    };

    void AddRule(const Rule& rule);
    int GetTileIndex(const Connection neighbors[8]) const;

    void SetTexture(Texture2D* texture) { texture_ = texture; }
    Texture2D* GetTexture() const { return texture_; }

private:
    Texture2D* texture_;
    std::vector<Rule> rules_;
};

#endif // DSE_AUTO_TILE_H
