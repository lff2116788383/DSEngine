#include "sprite_renderer.h"
#include "component/game_object.h"
#include "mesh_filter.h"
#include "mesh_renderer.h"
#include "material.h"

using namespace rttr;

RTTR_REGISTRATION
{
    registration::class_<SpriteRenderer>("SpriteRenderer")
        .constructor<>()(rttr::policy::ctor::as_raw_ptr)
        .property("sprite", &SpriteRenderer::sprite, &SpriteRenderer::set_sprite)
        .property("color", &SpriteRenderer::color, &SpriteRenderer::set_color)
        .property("sorting_layer", &SpriteRenderer::sorting_layer, &SpriteRenderer::set_sorting_layer)
        .property("order_in_layer", &SpriteRenderer::order_in_layer, &SpriteRenderer::set_order_in_layer);
}

SpriteRenderer::SpriteRenderer() : Component() {
}

SpriteRenderer::~SpriteRenderer() {
}

void SpriteRenderer::set_sprite(Sprite* sprite) {
    sprite_ = sprite;
}

void SpriteRenderer::Update() {
    Component::Update();

    if (sprite_ == nullptr) return;

    // Check if we need to update mesh (simple check: if sprite changed)
    // In a real engine, we'd also check if sprite properties changed or dirty flag
    if (sprite_ != last_sprite_) {
        last_sprite_ = sprite_;

        MeshFilter* mesh_filter = game_object()->GetComponent<MeshFilter>();
        if (mesh_filter == nullptr) {
            mesh_filter = game_object()->AddComponent<MeshFilter>();
        }

        Texture2D* texture = sprite_->texture();
        if (!texture) return;

        Sprite::Rect rect = sprite_->rect();
        if (rect.width <= 0 || rect.height <= 0) {
            rect.x = 0;
            rect.y = 0;
            rect.width = (float)texture->width();
            rect.height = (float)texture->height();
        }

        // Calculate vertices based on PPU and Pivot
        float width = rect.width / sprite_->ppu();
        float height = rect.height / sprite_->ppu();
        float pivot_x = sprite_->pivot().x * width;
        float pivot_y = sprite_->pivot().y * height;

        // Calculate UVs
        float u0 = rect.x / texture->width();
        float v0 = rect.y / texture->height();
        float u1 = (rect.x + rect.width) / texture->width();
        float v1 = (rect.y + rect.height) / texture->height();

        // 4 Vertices for a Quad
        // 0: Bottom-Left
        // 1: Bottom-Right
        // 2: Top-Right
        // 3: Top-Left
        std::vector<MeshFilter::Vertex> vertex_vector = {
            { {-pivot_x, -pivot_y, 0.0f}, {color_.r, color_.g, color_.b, color_.a}, {u0, v1}, {0.0f, 0.0f, 1.0f} },
            { {width - pivot_x, -pivot_y, 0.0f}, {color_.r, color_.g, color_.b, color_.a}, {u1, v1}, {0.0f, 0.0f, 1.0f} },
            { {width - pivot_x, height - pivot_y, 0.0f}, {color_.r, color_.g, color_.b, color_.a}, {u1, v0}, {0.0f, 0.0f, 1.0f} },
            { {-pivot_x, height - pivot_y, 0.0f}, {color_.r, color_.g, color_.b, color_.a}, {u0, v0}, {0.0f, 0.0f, 1.0f} }
        };

        std::vector<unsigned short> index_vector = {
            0, 1, 2,
            0, 2, 3
        };

        mesh_filter->CreateMesh(vertex_vector, index_vector);

        // Ensure MeshRenderer exists
        MeshRenderer* mesh_renderer = game_object()->GetComponent<MeshRenderer>();
        if (mesh_renderer == nullptr) {
            mesh_renderer = game_object()->AddComponent<MeshRenderer>();
            // Use a default sprite material
            Material* material = new Material();
            material->Parse("material/ui_image.mat"); // Reusing UI material for now as it's unlit
            material->SetTexture("u_diffuse_texture", texture);
            mesh_renderer->SetMaterial(material);
        } else {
            // Update material texture if needed
            Material* material = mesh_renderer->material();
            if (material) {
                material->SetTexture("u_diffuse_texture", texture);
            }
        }
    }

    // Always update sorting order
    MeshRenderer* mesh_renderer = game_object()->GetComponent<MeshRenderer>();
    if (mesh_renderer != nullptr) {
        mesh_renderer->set_sorting_layer(sorting_layer_);
        mesh_renderer->set_order_in_layer(order_in_layer_);
    }
}
