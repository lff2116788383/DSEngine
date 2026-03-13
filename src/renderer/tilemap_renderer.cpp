#include "tilemap_renderer.h"
#include "component/game_object.h"
#include "component/transform.h"
#include "mesh_renderer.h"
#include "grid.h"
#include "texture_2d.h"
#include <unordered_map>
#include <unordered_set>

using namespace rttr;

RTTR_REGISTRATION
{
    registration::class_<TilemapRenderer>("TilemapRenderer")
        .constructor<>()(rttr::policy::ctor::as_raw_ptr)
        .property("color", &TilemapRenderer::color, &TilemapRenderer::set_color)
        .property("sorting_layer", &TilemapRenderer::sorting_layer, &TilemapRenderer::set_sorting_layer)
        .property("order_in_layer", &TilemapRenderer::order_in_layer, &TilemapRenderer::set_order_in_layer);
}

TilemapRenderer::TilemapRenderer() : Component() {
}

TilemapRenderer::~TilemapRenderer() {
}

void TilemapRenderer::Update() {
    Component::Update();

    Tilemap* tilemap = game_object()->GetComponent<Tilemap>();
    if (tilemap == nullptr) return;

    // Check if we need to rebuild mesh
    bool dirty = false;
    if (tilemap != last_tilemap_) {
        dirty = true;
        last_tilemap_ = tilemap;
    } else if (tilemap->version() != last_version_) {
        dirty = true;
    }

    if (dirty) {
        RebuildMesh();
        last_version_ = tilemap->version();
    }

    MeshRenderer* mesh_renderer = game_object()->GetComponent<MeshRenderer>();
    if (mesh_renderer != nullptr) {
        mesh_renderer->set_sorting_layer(sorting_layer_);
        mesh_renderer->set_order_in_layer(order_in_layer_);
    }

    for (auto& pair : texture_batches_) {
        Batch& batch = pair.second;
        if (batch.mesh_renderer) {
            batch.mesh_renderer->set_sorting_layer(sorting_layer_);
            batch.mesh_renderer->set_order_in_layer(order_in_layer_);
        }
    }
}

void TilemapRenderer::RebuildMesh() {
    Tilemap* tilemap = game_object()->GetComponent<Tilemap>();
    Grid* grid = game_object()->GetComponent<Grid>();
    if (grid == nullptr && game_object()->parent() != nullptr) {
        GameObject* parent_go = dynamic_cast<GameObject*>(game_object()->parent());
        if (parent_go != nullptr) {
            grid = parent_go->GetComponent<Grid>();
        }
    }
    
    if (tilemap == nullptr || grid == nullptr) return;

    struct MeshBuildData {
        std::vector<MeshFilter::Vertex> vertices;
        std::vector<unsigned short> indices;
        int vertex_offset = 0;
    };

    std::unordered_map<Texture2D*, MeshBuildData> build_map;
    std::vector<Texture2D*> texture_order;

    tilemap->ForeachTile([&](glm::ivec2 cell_pos, Sprite* sprite) {
        if (sprite == nullptr || sprite->texture() == nullptr) return;

        Texture2D* texture = sprite->texture();
        if (build_map.find(texture) == build_map.end()) {
            texture_order.push_back(texture);
        }

        MeshBuildData& data = build_map[texture];

        float x = 0.0f;
        float y = 0.0f;
        
        if (grid->cell_layout() == Grid::CellLayout::Isometric) {
             float half_w = (grid->cell_size().x + grid->cell_gap().x) * 0.5f;
             float half_h = (grid->cell_size().y + grid->cell_gap().y) * 0.5f;
             x = (cell_pos.x - cell_pos.y) * half_w;
             y = -(cell_pos.x + cell_pos.y) * half_h;
        } else {
             x = cell_pos.x * (grid->cell_size().x + grid->cell_gap().x);
             y = cell_pos.y * (grid->cell_size().y + grid->cell_gap().y);
        }

        Sprite::Rect rect = sprite->rect();
        if (rect.width <= 0 || rect.height <= 0) {
            rect.x = 0;
            rect.y = 0;
            rect.width = (float)texture->width();
            rect.height = (float)texture->height();
        }

        float width = rect.width / sprite->ppu();
        float height = rect.height / sprite->ppu();
        float pivot_x = sprite->pivot().x * width;
        float pivot_y = sprite->pivot().y * height;

        float u0 = rect.x / texture->width();
        float v0 = rect.y / texture->height();
        float u1 = (rect.x + rect.width) / texture->width();
        float v1 = (rect.y + rect.height) / texture->height();

        data.vertices.push_back({ {x - pivot_x, y - pivot_y, 0.0f}, {color_.r, color_.g, color_.b, color_.a}, {u0, v1}, {0.0f, 0.0f, 1.0f} });
        data.vertices.push_back({ {x + width - pivot_x, y - pivot_y, 0.0f}, {color_.r, color_.g, color_.b, color_.a}, {u1, v1}, {0.0f, 0.0f, 1.0f} });
        data.vertices.push_back({ {x + width - pivot_x, y + height - pivot_y, 0.0f}, {color_.r, color_.g, color_.b, color_.a}, {u1, v0}, {0.0f, 0.0f, 1.0f} });
        data.vertices.push_back({ {x - pivot_x, y + height - pivot_y, 0.0f}, {color_.r, color_.g, color_.b, color_.a}, {u0, v0}, {0.0f, 0.0f, 1.0f} });

        data.indices.push_back(data.vertex_offset + 0);
        data.indices.push_back(data.vertex_offset + 1);
        data.indices.push_back(data.vertex_offset + 2);
        data.indices.push_back(data.vertex_offset + 0);
        data.indices.push_back(data.vertex_offset + 2);
        data.indices.push_back(data.vertex_offset + 3);

        data.vertex_offset += 4;
    });

    if (texture_order.empty()) {
        MeshFilter* mesh_filter = game_object()->GetComponent<MeshFilter>();
        if (mesh_filter != nullptr) {
            CreateDegenerateMesh(mesh_filter);
        }
        for (auto& pair : texture_batches_) {
            if (pair.second.game_object) {
                pair.second.game_object->set_active_self(false);
            }
        }
        return;
    }

    Texture2D* main_texture = texture_order[0];
    UpdateMainMesh(main_texture, build_map[main_texture].vertices, build_map[main_texture].indices);

    std::unordered_set<Texture2D*> used_textures;
    used_textures.insert(main_texture);

    for (size_t i = 1; i < texture_order.size(); ++i) {
        Texture2D* texture = texture_order[i];
        MeshBuildData& data = build_map[texture];
        Batch& batch = GetOrCreateBatch(texture);
        batch.game_object->set_active_self(true);
        batch.mesh_filter->CreateMesh(data.vertices, data.indices);
        Material* material = batch.mesh_renderer->material();
        if (material == nullptr) {
            material = new Material();
            material->Parse("material/ui_image.mat");
            batch.mesh_renderer->SetMaterial(material);
        }
        material->SetTexture("u_diffuse_texture", texture);
        batch.mesh_renderer->set_sorting_layer(sorting_layer_);
        batch.mesh_renderer->set_order_in_layer(order_in_layer_);
        used_textures.insert(texture);
    }

    for (auto& pair : texture_batches_) {
        if (used_textures.find(pair.first) == used_textures.end()) {
            if (pair.second.game_object) {
                pair.second.game_object->set_active_self(false);
            }
        }
    }
}

TilemapRenderer::Batch& TilemapRenderer::GetOrCreateBatch(Texture2D* texture) {
    auto it = texture_batches_.find(texture);
    if (it != texture_batches_.end()) {
        return it->second;
    }

    GameObject* batch_go = new GameObject("TilemapBatch");
    batch_go->SetParent(game_object());
    batch_go->set_layer(game_object()->layer());
    auto batch_transform = batch_go->GetComponent<Transform>();
    if (!batch_transform) {
        batch_transform = batch_go->AddComponent<Transform>();
    }
    batch_transform->set_local_position(glm::vec3(0.0f));
    batch_transform->set_local_rotation(glm::vec3(0.0f));
    batch_transform->set_local_scale(glm::vec3(1.0f));

    MeshFilter* mesh_filter = batch_go->GetComponent<MeshFilter>();
    if (!mesh_filter) {
        mesh_filter = batch_go->AddComponent<MeshFilter>();
    }

    MeshRenderer* mesh_renderer = batch_go->GetComponent<MeshRenderer>();
    if (!mesh_renderer) {
        mesh_renderer = batch_go->AddComponent<MeshRenderer>();
    }

    Batch batch;
    batch.game_object = batch_go;
    batch.mesh_filter = mesh_filter;
    batch.mesh_renderer = mesh_renderer;
    batch.texture = texture;
    auto result = texture_batches_.emplace(texture, batch);
    return result.first->second;
}

void TilemapRenderer::UpdateMainMesh(Texture2D* texture, std::vector<MeshFilter::Vertex>& vertices, std::vector<unsigned short>& indices) {
    auto transform = game_object()->GetComponent<Transform>();
    if (!transform) {
        transform = game_object()->AddComponent<Transform>();
        transform->set_local_position(glm::vec3(0.0f));
        transform->set_local_rotation(glm::vec3(0.0f));
        transform->set_local_scale(glm::vec3(1.0f));
    }

    MeshFilter* mesh_filter = game_object()->GetComponent<MeshFilter>();
    if (mesh_filter == nullptr) {
        mesh_filter = game_object()->AddComponent<MeshFilter>();
    }

    MeshRenderer* mesh_renderer = game_object()->GetComponent<MeshRenderer>();
    if (mesh_renderer == nullptr) {
        mesh_renderer = game_object()->AddComponent<MeshRenderer>();
    }

    if (vertices.empty()) {
        CreateDegenerateMesh(mesh_filter);
    } else {
        mesh_filter->CreateMesh(vertices, indices);
    }

    Material* material = mesh_renderer->material();
    if (material == nullptr) {
        material = new Material();
        material->Parse("material/ui_image.mat");
        mesh_renderer->SetMaterial(material);
    }
    material->SetTexture("u_diffuse_texture", texture);
    mesh_renderer->set_sorting_layer(sorting_layer_);
    mesh_renderer->set_order_in_layer(order_in_layer_);
}

void TilemapRenderer::CreateDegenerateMesh(MeshFilter* mesh_filter) {
    std::vector<MeshFilter::Vertex> vertices;
    std::vector<unsigned short> indices;
    vertices.push_back({ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} });
    indices.push_back(0);
    indices.push_back(0);
    indices.push_back(0);
    mesh_filter->CreateMesh(vertices, indices);
}
