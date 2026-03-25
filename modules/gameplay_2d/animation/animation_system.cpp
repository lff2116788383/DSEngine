#include "modules/gameplay_2d/animation/animation_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/assets/asset_manager.h"

void AnimationSystem::Update(World& world, float delta_time) {
    auto view = world.registry().view<AnimatorComponent, SpriteRendererComponent>();
    
    for (auto entity : view) {
        auto& animator = view.get<AnimatorComponent>(entity);
        auto& sprite = view.get<SpriteRendererComponent>(entity);
        
        if (animator.states.empty()) continue;
        
        // 1. Evaluate Transitions
        if (animator.transitions.count(animator.current_state)) {
            for (const auto& trans : animator.transitions[animator.current_state]) {
                if (animator.bool_params.count(trans.condition_param)) {
                    if (animator.bool_params[trans.condition_param] == trans.condition_value) {
                        // Switch state
                        animator.current_state = trans.to_state;
                        animator.current_time = 0.0f;
                        animator.current_frame = 0;
                        animator.playing = true;
                        break;
                    }
                }
            }
        }

        // Initialize state if empty
        if (animator.current_state.empty()) {
            animator.current_state = animator.states.begin()->first;
        }

        // 2. Play current state
        if (!animator.playing) continue;

        auto& state = animator.states[animator.current_state];
        if (state.frames.empty() && state.frame_handles.empty()) continue;
        size_t total_frames = state.frames.empty() ? state.frame_handles.size() : state.frames.size();

        animator.current_time += delta_time;
        float frame_duration = 1.0f / state.frame_rate;
        
        if (animator.current_time >= frame_duration) {
            animator.current_time -= frame_duration;
            animator.current_frame++;
            
            if (animator.current_frame >= total_frames) {
                if (state.loop) {
                    animator.current_frame = 0;
                } else {
                    animator.current_frame = total_frames - 1;
                    animator.playing = false;
                }
            }
            
            // Update the sprite texture
            if (!state.frames.empty()) {
                auto tex = state.frames[animator.current_frame];
                sprite.texture = tex;
                if (tex) {
                    sprite.texture_handle = tex->GetHandle();
                }
            } else if (!state.frame_handles.empty()) {
                sprite.texture_handle = state.frame_handles[animator.current_frame];
                sprite.texture = nullptr;
            }
        }
    }
}
