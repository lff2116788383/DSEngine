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
        int segment_start = animator.segment_start_frame < 0 ? 0 : animator.segment_start_frame;
        int segment_end = animator.segment_end_frame >= 0 ? animator.segment_end_frame : static_cast<int>(total_frames) - 1;
        if (segment_start >= static_cast<int>(total_frames)) segment_start = 0;
        if (segment_end >= static_cast<int>(total_frames)) segment_end = static_cast<int>(total_frames) - 1;
        if (segment_end < segment_start) segment_end = segment_start;
        if (animator.current_frame < segment_start || animator.current_frame > segment_end) {
            animator.current_frame = segment_start;
        }

        animator.current_time += delta_time;
        float frame_duration = 1.0f / state.frame_rate;
        
        if (animator.current_time >= frame_duration) {
            int previous_frame = animator.current_frame;
            animator.current_time -= frame_duration;
            animator.current_frame++;
            
            if (animator.current_frame > segment_end) {
                if (animator.segment_end_frame >= 0) {
                    if (animator.segment_loop) {
                        animator.current_frame = segment_start;
                    } else {
                        animator.current_frame = segment_end;
                        animator.playing = false;
                    }
                } else {
                    if (state.loop) {
                        animator.current_frame = 0;
                    } else {
                        animator.current_frame = static_cast<int>(total_frames) - 1;
                        animator.playing = false;
                    }
                }
            }

            if (!state.events.empty() && total_frames > 1) {
                float previous_normalized = static_cast<float>(previous_frame) / static_cast<float>(total_frames - 1);
                float current_normalized = static_cast<float>(animator.current_frame) / static_cast<float>(total_frames - 1);
                bool wrapped = current_normalized < previous_normalized;
                for (const auto& event_item : state.events) {
                    bool crossed = false;
                    if (!wrapped) {
                        crossed = event_item.first > previous_normalized && event_item.first <= current_normalized;
                    } else {
                        crossed = event_item.first > previous_normalized || event_item.first <= current_normalized;
                    }
                    if (crossed) {
                        animator.fired_events.push_back(event_item.second);
                    }
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
