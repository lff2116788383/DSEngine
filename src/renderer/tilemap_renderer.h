#ifndef UNTITLED_TILEMAP_RENDERER_H
#define UNTITLED_TILEMAP_RENDERER_H

#include "component/component.h"
#include "renderer/tilemap.h"
#include "renderer/mesh_filter.h"
#include "renderer/material.h"
#include <unordered_map>

class GameObject;
class MeshRenderer;

class TilemapRenderer : public Component {
public:
    TilemapRenderer();
    ~TilemapRenderer();

    void Update() override;

    void set_color(glm::vec4 color) { color_ = color; }
    glm::vec4 color() const { return color_; }

    void set_sorting_layer(int layer) { sorting_layer_ = layer; }
    int sorting_layer() const { return sorting_layer_; }

    void set_order_in_layer(int order) { order_in_layer_ = order; }
    int order_in_layer() const { return order_in_layer_; }

private:
    void RebuildMesh();

    struct Batch {
        GameObject* game_object = nullptr;
        MeshFilter* mesh_filter = nullptr;
        MeshRenderer* mesh_renderer = nullptr;
        Texture2D* texture = nullptr;
    };

    Batch& GetOrCreateBatch(Texture2D* texture);
    void UpdateMainMesh(Texture2D* texture, std::vector<MeshFilter::Vertex>& vertices, std::vector<unsigned short>& indices);
    void CreateDegenerateMesh(MeshFilter* mesh_filter);

    glm::vec4 color_{1.0f, 1.0f, 1.0f, 1.0f};
    int sorting_layer_ = 0;
    int order_in_layer_ = 0;
    
    // Cache to check if update is needed
    size_t last_version_ = 0;
    
    // We can also check if tilemap pointer changed
    Tilemap* last_tilemap_ = nullptr;

    std::unordered_map<Texture2D*, Batch> texture_batches_;

    RTTR_ENABLE(Component)
};

#endif //UNTITLED_TILEMAP_RENDERER_H
