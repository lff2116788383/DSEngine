#ifndef DSE_SPINE_RENDERER_H
#define DSE_SPINE_RENDERER_H

#include "component/component.h"
#include <string>

// Forward declarations for Spine types if we don't have headers yet
struct spSkeleton;
struct spAnimationState;
struct spSkeletonData;

class SpineRenderer : public Component {
public:
    SpineRenderer();
    ~SpineRenderer();

    void LoadSkeleton(const std::string& json_path, const std::string& atlas_path);
    void SetAnimation(const std::string& name, bool loop);
    void Update(float delta_time);
    
    void OnRender();

private:
    spSkeletonData* skeleton_data_;
    spSkeleton* skeleton_;
    spAnimationState* animation_state_;
};

#endif // DSE_SPINE_RENDERER_H
