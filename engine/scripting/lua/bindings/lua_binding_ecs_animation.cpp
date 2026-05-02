/**
 * @file lua_binding_ecs_animation.cpp
 * @brief ECS Lua 绑定 — 2D 动画（AnimatorComponent）+ 3D 骨骼动画（Animator3DComponent / FSM）
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/components_3d.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

// ============================================================
// 2D 动画
// ============================================================

int L_EcsAddAnimator(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    world->registry().emplace_or_replace<AnimatorComponent>(e);
    return 0;
}

int L_EcsAddAnimationState(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* state_name = luaL_checkstring(L, 2);
    float fps = helper::CheckFloat(L, 3);
    bool loop = helper::CheckBool(L, 4);
    auto* animator = helper::TryGetComponent<AnimatorComponent>(*world, e);
    if (!animator) return 0;
    AnimationState state;
    state.name = state_name;
    state.frame_rate = fps;
    state.loop = loop;
    if (lua_istable(L, 5)) {
        int len = lua_rawlen(L, 5);
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 5, i);
            unsigned int handle = static_cast<unsigned int>(lua_tointeger(L, -1));
            state.frame_handles.push_back(handle);
            lua_pop(L, 1);
        }
    }
    animator->states[state_name] = state;
    return 0;
}

int L_EcsAddAnimationEvent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* state_name = luaL_checkstring(L, 2);
    float normalized_time = helper::CheckFloat(L, 3);
    const char* event_name = luaL_checkstring(L, 4);
    if (normalized_time < 0.0f) normalized_time = 0.0f;
    if (normalized_time > 1.0f) normalized_time = 1.0f;
    auto* animator = helper::TryGetComponent<AnimatorComponent>(*world, e);
    if (!animator) return 0;
    auto it = animator->states.find(state_name);
    if (it != animator->states.end()) {
        it->second.events.emplace_back(normalized_time, std::string(event_name));
    }
    return 0;
}

int L_EcsPlayAnimation(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* state_name = luaL_checkstring(L, 2);
    auto* animator = helper::TryGetComponent<AnimatorComponent>(*world, e);
    if (!animator) return 0;
    animator->current_state = state_name;
    animator->current_time = 0.0f;
    animator->current_frame = 0;
    animator->playing = true;
    return 0;
}

int L_EcsPlayAnimationSegment(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int start_frame = helper::CheckInt(L, 2);
    int end_frame = helper::CheckInt(L, 3);
    bool loop = helper::CheckBool(L, 4);
    auto* animator = helper::TryGetComponent<AnimatorComponent>(*world, e);
    if (!animator) return 0;
    animator->PlaySegment(start_frame, end_frame, loop);
    return 0;
}

int L_EcsPopAnimationEvent(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushstring(L, "");
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    auto* animator = helper::TryGetComponent<AnimatorComponent>(*world, e);
    if (animator && !animator->fired_events.empty()) {
        std::string event_name = animator->fired_events.front();
        animator->fired_events.erase(animator->fired_events.begin());
        lua_pushstring(L, event_name.c_str());
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

// ============================================================
// 3D 骨骼动画
// ============================================================

int L_EcsAddAnimator3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* danim_path = luaL_optstring(L, 2, "");
    const char* dskel_path = luaL_optstring(L, 3, "");
    auto& animator = world->registry().emplace_or_replace<Animator3DComponent>(e);
    animator.danim_path = danim_path;
    animator.dskel_path = dskel_path;
    animator.enabled = true;
    animator.current_time = 0.0f;
    animator.speed = 1.0f;
    animator.loop = true;
    return 0;
}

int L_EcsSetAnimator3DState(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (!animator) return 0;
    if (lua_gettop(L) >= 2 && lua_isstring(L, 2)) {
        const char* state_name = lua_tostring(L, 2);
        if (animator->state_machine) {
            animator->current_state_name = state_name;
            animator->state_time = 0.0f;
            animator->is_transitioning = false;
        } else {
            animator->danim_path = state_name;
        }
    }
    if (lua_gettop(L) >= 3) {
        animator->speed = helper::CheckFloat(L, 3);
    }
    if (lua_gettop(L) >= 4) {
        animator->loop = helper::CheckBool(L, 4);
    }
    return 0;
}

int L_EcsGetAnimator3DState(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = helper::CheckEntity(L, 1);
    const auto* animator = helper::TryGetComponentConst<Animator3DComponent>(*world, e);
    if (!animator) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    lua_pushstring(L, animator->current_state_name.empty() ? animator->danim_path.c_str() : animator->current_state_name.c_str());
    helper::PushFloat(L, animator->normalized_time);
    helper::PushFloat(L, animator->state_machine ? animator->state_time : animator->current_time);
    helper::PushFloat(L, animator->speed);
    helper::PushBool(L, animator->loop);
    helper::PushBool(L, animator->is_transitioning);
    helper::PushInt(L, static_cast<int>(animator->final_bone_matrices.size()));
    helper::PushBool(L, !animator->dskel_path.empty());
    return 9;
}

int L_EcsInitAnimator3DStateMachine(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (!animator) return 0;
    animator->state_machine = std::make_shared<gameplay3d::AnimationStateMachine>();
    return 0;
}

int L_EcsAddAnimator3DState(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* state_name = luaL_checkstring(L, 2);
    const char* danim_path = luaL_checkstring(L, 3);
    bool loop = helper::CheckBool(L, 4);
    float speed = helper::OptFloat(L, 5, 1.0f);

    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (!animator || !animator->state_machine) return 0;
    gameplay3d::AnimState state;
    state.name = state_name;
    state.danim_path = danim_path;
    state.loop = loop;
    state.speed = speed;
    state.is_blend_tree = false;
    animator->state_machine->AddState(state);
    return 0;
}

int L_EcsAddAnimator3DTransition(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* from_state = luaL_checkstring(L, 2);
    const char* to_state = luaL_checkstring(L, 3);
    float transition_duration = helper::OptFloat(L, 4, 0.25f);
    bool has_exit_time = helper::CheckBool(L, 5);
    float exit_time = helper::OptFloat(L, 6, 1.0f);

    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (!animator || !animator->state_machine) return 0;

    auto& fsm = *animator->state_machine;
    auto states = const_cast<std::unordered_map<std::string, gameplay3d::AnimState>&>(fsm.GetStates());
    auto it = states.find(from_state);
    if (it == states.end()) return 0;

    gameplay3d::AnimTransition trans;
    trans.target_state = to_state;
    trans.transition_duration = transition_duration;
    trans.has_exit_time = has_exit_time;
    trans.exit_time = exit_time;

    // 读取条件表（Arg 7）
    if (lua_istable(L, 7)) {
        lua_pushnil(L);
        while (lua_next(L, 7) != 0) {
            if (lua_istable(L, -1)) {
                gameplay3d::AnimTransitionCondition cond;
                lua_rawgeti(L, -1, 1); cond.parameter_name = lua_tostring(L, -1); lua_pop(L, 1);
                lua_rawgeti(L, -1, 2); int mode = lua_tointeger(L, -1); lua_pop(L, 1);
                cond.mode = static_cast<gameplay3d::AnimConditionMode>(mode);

                lua_rawgeti(L, -1, 3);
                if (lua_isnumber(L, -1)) {
                    cond.threshold = static_cast<float>(lua_tonumber(L, -1));
                    cond.int_value = static_cast<int>(lua_tointeger(L, -1));
                }
                lua_pop(L, 1);

                trans.conditions.push_back(cond);
            }
            lua_pop(L, 1);
        }
    }

    it->second.transitions.push_back(trans);
    return 0;
}

int L_EcsSetAnimator3DParamFloat(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* param_name = luaL_checkstring(L, 2);
    float value = helper::CheckFloat(L, 3);

    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (!animator || !animator->state_machine) return 0;
    // 自动添加不存在的参数
    if (animator->state_machine->GetParameters().find(param_name) == animator->state_machine->GetParameters().end()) {
        animator->state_machine->AddParameter(param_name, gameplay3d::AnimParamType::Float, 0.0f);
    }
    animator->state_machine->SetFloat(param_name, value);
    return 0;
}

int L_EcsSetAnimator3DParamTrigger(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* param_name = luaL_checkstring(L, 2);

    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (!animator || !animator->state_machine) return 0;
    if (animator->state_machine->GetParameters().find(param_name) == animator->state_machine->GetParameters().end()) {
        animator->state_machine->AddTrigger(param_name);
    }
    animator->state_machine->SetTrigger(param_name);
    return 0;
}

} // namespace

void RegisterEcsAnimationBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        // 2D 动画
        {"add_animator",              L_EcsAddAnimator},
        {"add_animation_state",       L_EcsAddAnimationState},
        {"add_animation_event",       L_EcsAddAnimationEvent},
        {"play_animation",            L_EcsPlayAnimation},
        {"play_animation_segment",    L_EcsPlayAnimationSegment},
        {"pop_animation_event",       L_EcsPopAnimationEvent},
        // 3D 骨骼动画
        {"add_animator_3d",           L_EcsAddAnimator3D},
        {"set_animator_3d_state",     L_EcsSetAnimator3DState},
        {"get_animator_3d_state",     L_EcsGetAnimator3DState},
        {"init_animator_3d_fsm",      L_EcsInitAnimator3DStateMachine},
        {"add_animator_3d_state",     L_EcsAddAnimator3DState},
        {"add_animator_3d_transition", L_EcsAddAnimator3DTransition},
        {"set_animator_3d_param_float",  L_EcsSetAnimator3DParamFloat},
        {"set_animator_3d_param_trigger", L_EcsSetAnimator3DParamTrigger},
    });
}

} // namespace dse::runtime::lua_binding
