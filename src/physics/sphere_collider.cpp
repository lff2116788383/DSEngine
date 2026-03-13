//
// Created by captain on 4/28/2022.
//

#include "sphere_collider.h"
#include <rttr/registration>
#include "physics.h"

using namespace rttr;
RTTR_REGISTRATION//注册反射
{
    registration::class_<SphereCollider>("SphereCollider")
            .constructor<>()(rttr::policy::ctor::as_raw_ptr)
            .property("radius", &SphereCollider::radius, &SphereCollider::set_radius);
}

SphereCollider::SphereCollider():Collider(),radius_(1.0f)
{

}

SphereCollider::~SphereCollider() {

}

void SphereCollider::set_radius(float radius) {
    radius_ = radius;
    if (px_shape_) {
        // Similar to BoxCollider, updating runtime shape geometry is tricky without headers.
    }
}

void SphereCollider::CreateShape() {
    if(px_shape_== nullptr){
        px_shape_=Physics::CreateSphereShape(radius_,px_material_);
    }
}