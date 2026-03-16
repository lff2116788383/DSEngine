#include "tilemap_collider.h"
#include "tilemap.h"
#include "grid.h"
#include "physics/physics.h"
#include "physics/rigid_actor.h"
#include "physics/rigid_dynamic.h"
#include "physics/rigid_static.h"
#include "component/game_object.h"
#include <rttr/registration>

using namespace rttr;
using namespace physx;

RTTR_REGISTRATION
{
    registration::class_<TilemapCollider>("TilemapCollider")
        .constructor<>()(rttr::policy::ctor::as_raw_ptr);
}

TilemapCollider::TilemapCollider() : Component() {}

TilemapCollider::~TilemapCollider() {
    if (game_object()) {
        RigidActor* rigid_actor = game_object()->GetComponent<RigidDynamic>();
        if (!rigid_actor) rigid_actor = game_object()->GetComponent<RigidStatic>();
        
        if (rigid_actor) {
            for (auto shape : shapes_) {
                rigid_actor->DetachShape(shape);
            }
        }
    }
}

void TilemapCollider::Update() {
    Component::Update();
    
    Tilemap* tilemap = game_object()->GetComponent<Tilemap>();
    if (!tilemap) return;
    
    if (tilemap->version() != last_version_) {
        RebuildCollider();
        last_version_ = tilemap->version();
    }
}

void TilemapCollider::RebuildCollider() {
    RigidActor* rigid_actor = game_object()->GetComponent<RigidDynamic>();
    if (!rigid_actor) rigid_actor = game_object()->GetComponent<RigidStatic>();
    
    if (!rigid_actor) {
        return;
    }

    for (auto shape : shapes_) {
        rigid_actor->DetachShape(shape);
    }
    shapes_.clear();

    Tilemap* tilemap = game_object()->GetComponent<Tilemap>();
    Grid* grid = game_object()->GetComponent<Grid>();
    if (!grid && game_object()->parent()) {
        grid = dynamic_cast<GameObject*>(game_object()->parent())->GetComponent<Grid>();
    }
    
    if (!tilemap || !grid) return;

    PxMaterial* material = Physics::CreateMaterial(0.5f, 0.5f, 0.5f);
    
    tilemap->ForeachTile([&](glm::ivec2 cell_pos, Sprite* sprite) {
        if (!sprite) return;

        glm::vec3 cell_size(grid->cell_size().x, grid->cell_size().y, 1.0f);
        
        float x = cell_pos.x * (grid->cell_size().x + grid->cell_gap().x);
        float y = cell_pos.y * (grid->cell_size().y + grid->cell_gap().y);
        
        float center_x = x + cell_size.x / 2.0f;
        float center_y = y + cell_size.y / 2.0f;
        float center_z = 0.0f;
        
        PxShape* shape = Physics::CreateBoxShape(cell_size, material);
        if (shape) {
            PxTransform transform(center_x, center_y, center_z);
            shape->setLocalPose(transform);
            shape->userData = game_object();
            
            rigid_actor->AttachShape(shape);
            shapes_.push_back(shape);
        }
    });
}
