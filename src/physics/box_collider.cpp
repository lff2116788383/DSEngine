//
// Created by captain on 4/28/2022.
//

#include "box_collider.h"
#include <rttr/registration>
#include "physics.h"

using namespace rttr;
RTTR_REGISTRATION//注册反射
{
    registration::class_<BoxCollider>("BoxCollider")
            .constructor<>()(rttr::policy::ctor::as_raw_ptr)
            .property("size", &BoxCollider::size, &BoxCollider::set_size);
}

BoxCollider::BoxCollider():Collider(),size_(1,1,1)
{

}

BoxCollider::~BoxCollider() {

}

void BoxCollider::set_size(const glm::vec3& size) {
    size_ = size;
    // If shape exists, we might need to recreate or update it?
    // PhysX shapes are immutable for geometry usually, need to recreate or set geometry
    // For simplicity, we just store it. If runtime update is needed, we need to access px_shape_
    if (px_shape_) {
        // PxBoxGeometry geometry(size_.x/2, size_.y/2, size_.z/2); // PhysX uses half-extents
        // px_shape_->setGeometry(geometry);
        // But we don't have access to PxBoxGeometry easily here without including PhysX headers
        // So let's leave runtime update for now, or just re-create shape if possible.
        // For now, this property is mainly for initialization.
    }
}

void BoxCollider::CreateShape() {
    if(px_shape_== nullptr){
        px_shape_=Physics::CreateBoxShape(size_,px_material_);
    }
}