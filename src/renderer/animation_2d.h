#ifndef ANIMATION_2D_H
#define ANIMATION_2D_H

#include "component/component.h"
#include <rttr/type>

class AnimationClip2D;
class SpriteRenderer;

class Animation2D : public Component {
public:
    Animation2D();
    ~Animation2D();

    void set_clip(AnimationClip2D* clip);
    AnimationClip2D* clip() const { return clip_; }

    void Play();
    void Stop();
    
    void set_is_playing(bool is_playing) { is_playing_ = is_playing; }
    bool is_playing() const { return is_playing_; }

    void Update() override;

private:
    AnimationClip2D* clip_ = nullptr;
    float timer_ = 0.0f;
    int current_frame_index_ = 0;
    bool is_playing_ = false;

    SpriteRenderer* sprite_renderer_ = nullptr;

RTTR_ENABLE();
};

#endif // ANIMATION_2D_H
