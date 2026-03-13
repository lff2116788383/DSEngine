#include "animation_2d.h"
#include "animation_clip_2d.h"
#include "sprite_renderer.h"
#include "component/game_object.h"
#include "utils/time.h"
#include <rttr/registration>

using namespace rttr;

RTTR_REGISTRATION
{
    registration::class_<Animation2D>("Animation2D")
        .constructor<>()(rttr::policy::ctor::as_raw_ptr)
        .property("clip", &Animation2D::clip, &Animation2D::set_clip)
        .property("is_playing", &Animation2D::is_playing, &Animation2D::set_is_playing);
}

Animation2D::Animation2D() : Component() {
}

Animation2D::~Animation2D() {
}

void Animation2D::set_clip(AnimationClip2D* clip) {
    clip_ = clip;
    current_frame_index_ = 0;
    timer_ = 0.0f;
}

void Animation2D::Play() {
    is_playing_ = true;
    timer_ = 0.0f;
    current_frame_index_ = 0;
}

void Animation2D::Stop() {
    is_playing_ = false;
    current_frame_index_ = 0;
    timer_ = 0.0f;
}

void Animation2D::Update() {
    Component::Update();

    if (!is_playing_ || !clip_ || clip_->frames().empty()) {
        return;
    }

    if (sprite_renderer_ == nullptr) {
        sprite_renderer_ = game_object()->GetComponent<SpriteRenderer>();
        if (sprite_renderer_ == nullptr) {
            return;
        }
    }

    timer_ += Time::delta_time();
    const auto& frames = clip_->frames();

    while (true) {
        if (current_frame_index_ >= frames.size()) {
            if (clip_->is_looping()) {
                current_frame_index_ = 0;
            } else {
                current_frame_index_ = (int)frames.size() - 1;
                is_playing_ = false;
                break;
            }
        }
        
        float duration = frames[current_frame_index_].duration;
        if (timer_ >= duration) {
            timer_ -= duration;
            current_frame_index_++;
        } else {
            break;
        }
    }

    // Update sprite
    if (current_frame_index_ < frames.size()) {
        sprite_renderer_->set_sprite(frames[current_frame_index_].sprite);
    }
}
