//
// Created by captain on 4/28/2022.
//

#ifndef INTEGRATE_PHYSX_SPHERE_COLLIDER_H
#define INTEGRATE_PHYSX_SPHERE_COLLIDER_H

#include <rttr/registration>
#include <rttr/registration_friend.h>
#include "collider.h"

using namespace rttr;

class SphereCollider : public Collider {
    RTTR_REGISTRATION_FRIEND
public:
    SphereCollider();
    ~SphereCollider();

    void CreateShape() override;

    void set_radius(float radius) { radius_ = radius; }
    float radius() const { return radius_; }

private:
    //~zh 球体碰撞器半径
    float radius_;

RTTR_ENABLE(Collider);
};


#endif //INTEGRATE_PHYSX_SPHERE_COLLIDER_H
