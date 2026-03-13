//
// Created by cp on 2022/4/23.
//

#ifndef UNTITLED_RIGID_DYNAMIC_H
#define UNTITLED_RIGID_DYNAMIC_H

#include "rigid_actor.h"

class Collider;
class RigidDynamic : public RigidActor{
public:
    RigidDynamic();
    ~RigidDynamic();

    void UpdateCCDState();

    bool enable_ccd(){return enable_ccd_;}
    void set_enable_ccd(bool enable_ccd){
        enable_ccd_=enable_ccd;
        UpdateCCDState();
    }
    
    /// 设置是否启用2D模式（锁定Z轴位移，锁定X、Y轴旋转）
    void set_2d_mode(bool enable);
    bool is_2d_mode() const { return is_2d_mode_; }

public:
    /// Awake里反序列化给成员变量赋值。
    void Awake() override;

    void Update() override;

    void FixedUpdate() override;

private:
    bool enable_ccd_;
    bool is_2d_mode_ = false;

RTTR_ENABLE(RigidActor)
};


#endif //UNTITLED_RIGID_DYNAMIC_H
