#ifndef ANIMATION_CLIP_2D_H
#define ANIMATION_CLIP_2D_H

#include <vector>
#include <string>
#include <rttr/type>

class Sprite;

class AnimationClip2D {
public:
    AnimationClip2D();
    ~AnimationClip2D();

    struct Frame {
        Sprite* sprite;
        float duration; // Duration in seconds
    };

    void AddFrame(Sprite* sprite, float duration);
    const std::vector<Frame>& frames() const { return frames_; }
    
    void set_is_looping(bool looping) { is_looping_ = looping; }
    bool is_looping() const { return is_looping_; }

    void set_name(const std::string& name) { name_ = name; }
    std::string name() const { return name_; }

private:
    std::vector<Frame> frames_;
    bool is_looping_ = true;
    std::string name_;

RTTR_ENABLE();
};

#endif // ANIMATION_CLIP_2D_H
