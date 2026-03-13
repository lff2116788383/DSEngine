#include "animation_clip_2d.h"
#include "sprite.h"
#include <rttr/registration>

using namespace rttr;

RTTR_REGISTRATION
{
    registration::class_<AnimationClip2D>("AnimationClip2D")
        .constructor<>()(rttr::policy::ctor::as_raw_ptr)
        .property("is_looping", &AnimationClip2D::is_looping, &AnimationClip2D::set_is_looping)
        .property("name", &AnimationClip2D::name, &AnimationClip2D::set_name);
}

AnimationClip2D::AnimationClip2D() {
}

AnimationClip2D::~AnimationClip2D() {
}

void AnimationClip2D::AddFrame(Sprite* sprite, float duration) {
    frames_.push_back({sprite, duration});
}
