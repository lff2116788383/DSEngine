//
// Created by cp on 2022/4/23.
//

#include "rigid_dynamic.h"
#include <rttr/registration>
#include "physics.h"
#include "component/game_object.h"
#include "component/transform.h"
#include "utils/debug.h"
#include "collider.h"

using namespace rttr;
RTTR_REGISTRATION{
    registration::class_<RigidDynamic>("RigidDynamic")
            .constructor<>()(rttr::policy::ctor::as_raw_ptr)
            .property("is_2d_mode", &RigidDynamic::is_2d_mode, &RigidDynamic::set_2d_mode);
}

RigidDynamic::RigidDynamic():enable_ccd_(false){

}

RigidDynamic::~RigidDynamic(){

}

void RigidDynamic::set_2d_mode(bool enable) {
    is_2d_mode_ = enable;
    if (px_rigid_actor_ != nullptr) {
        PxRigidDynamic* px_rigid_dynamic = dynamic_cast<PxRigidDynamic*>(px_rigid_actor_);
        if (px_rigid_dynamic) {
            px_rigid_dynamic->setRigidDynamicLockFlag(PxRigidDynamicLockFlag::eLOCK_LINEAR_Z, enable);
            px_rigid_dynamic->setRigidDynamicLockFlag(PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, enable);
            px_rigid_dynamic->setRigidDynamicLockFlag(PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, enable);
        }
    }
}

void RigidDynamic::UpdateCCDState(){
    PxRigidDynamic* px_rigid_dynamic=dynamic_cast<PxRigidDynamic*>(px_rigid_actor_);
    //~en enable continuous collision detection due to high-speed motion.
    //~zh 对高速运动，开启连续碰撞检测。
    px_rigid_dynamic->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, enable_ccd_);
}

void RigidDynamic::Awake() {
    Transform* transform=game_object()->GetComponent<Transform>();
    PxRigidDynamic* px_rigid_dynamic=Physics::CreateRigidDynamic(transform->position(), game_object()->name());
    px_rigid_actor_=dynamic_cast<PxRigidActor*>(px_rigid_dynamic);
    
    // Apply 2D mode if set
    if (is_2d_mode_) {
        px_rigid_dynamic->setRigidDynamicLockFlag(PxRigidDynamicLockFlag::eLOCK_LINEAR_Z, true);
        px_rigid_dynamic->setRigidDynamicLockFlag(PxRigidDynamicLockFlag::eLOCK_ANGULAR_X, true);
        px_rigid_dynamic->setRigidDynamicLockFlag(PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y, true);
    }
    
    RigidActor::Awake();
}

void RigidDynamic::Update() {

}

void RigidDynamic::FixedUpdate() {
    if(px_rigid_actor_ == nullptr){
        DEBUG_LOG_ERROR("px_rigid_actor_== nullptr");
        return;
    }
    //PxRigidBody受Physx物理模拟驱动，位置被改变。获取最新的位置，去更新Transform。
    PxTransform px_transform=px_rigid_actor_->getGlobalPose();
    Transform* transform=game_object()->GetComponent<Transform>();
    transform->set_local_position(glm::vec3(px_transform.p.x,px_transform.p.y,px_transform.p.z));
}