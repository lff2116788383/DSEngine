/**
 * @file lua_binding_ecs_animation.cpp
 * @brief ECS Lua 绑定 — 2D 动画（AnimatorComponent）+ 3D 骨骼动画（Animator3DComponent / FSM）
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/animation_state_machine.h"
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
    auto& states = fsm.GetStatesMutable();
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

int L_EcsSetAnimator3DLockRootMotion(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (!animator) return 0;
    animator->lock_root_motion = lua_toboolean(L, 2) != 0;
    return 0;
}

// ============================================================
// 动画层系统 (AnimLayerComponent)
// ============================================================

int L_EcsAddAnimLayerComponent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    world->registry().emplace_or_replace<AnimLayerComponent>(e);
    return 0;
}

int L_EcsAddAnimLayer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* comp = helper::TryGetComponent<AnimLayerComponent>(*world, e);
    if (!comp) { helper::PushInt(L, -1); return 1; }

    AnimLayerConfig layer;
    layer.name = luaL_optstring(L, 2, "");
    layer.weight = helper::OptFloat(L, 3, 1.0f);
    int mode = helper::OptInt(L, 4, 0);
    layer.blend_mode = static_cast<AnimLayerBlendMode>(mode);
    comp->layers.push_back(std::move(layer));
    helper::PushInt(L, static_cast<int>(comp->layers.size()) - 1);
    return 1;
}

int L_EcsSetAnimLayerClip(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    const char* danim_path = luaL_checkstring(L, 3);
    auto* comp = helper::TryGetComponent<AnimLayerComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return 0;
    auto& layer = comp->layers[idx];
    layer.source_type = AnimSourceType::SingleClip;
    layer.danim_path = danim_path;
    layer.speed = helper::OptFloat(L, 4, 1.0f);
    layer.loop = helper::OptBool(L, 5, true);
    return 0;
}

int L_EcsSetAnimLayerWeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    float w = helper::CheckFloat(L, 3);
    auto* comp = helper::TryGetComponent<AnimLayerComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return 0;
    comp->layers[idx].weight = w;
    return 0;
}

int L_EcsSetAnimLayerBoneMask(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    auto* comp = helper::TryGetComponent<AnimLayerComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return 0;
    auto& layer = comp->layers[idx];
    layer.bone_mask_include.clear();
    if (lua_istable(L, 3)) {
        int len = static_cast<int>(lua_rawlen(L, 3));
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 3, i);
            if (lua_isstring(L, -1)) {
                layer.bone_mask_include.emplace_back(lua_tostring(L, -1));
            }
            lua_pop(L, 1);
        }
    }
    layer.bone_mask_dirty = true;
    return 0;
}

int L_EcsSetAnimLayerBlendTree1D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    auto* comp = helper::TryGetComponent<AnimLayerComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return 0;
    auto& layer = comp->layers[idx];
    layer.source_type = AnimSourceType::BlendTree1D;
    layer.blend_nodes.clear();
    if (lua_istable(L, 3)) {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            if (lua_istable(L, -1)) {
                AnimBlendNode node;
                lua_rawgeti(L, -1, 1); node.danim_path = luaL_optstring(L, -1, ""); lua_pop(L, 1);
                lua_rawgeti(L, -1, 2); node.threshold = static_cast<float>(luaL_optnumber(L, -1, 0.0)); lua_pop(L, 1);
                lua_rawgeti(L, -1, 3); node.speed = static_cast<float>(luaL_optnumber(L, -1, 1.0)); lua_pop(L, 1);
                node.loop = true;
                layer.blend_nodes.push_back(std::move(node));
            }
            lua_pop(L, 1);
        }
    }
    return 0;
}

int L_EcsSetAnimLayerBlendParam(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    float val = helper::CheckFloat(L, 3);
    auto* comp = helper::TryGetComponent<AnimLayerComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return 0;
    comp->layers[idx].blend_parameter_value = val;
    return 0;
}

int L_EcsSetAnimLayerEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* comp = helper::TryGetComponent<AnimLayerComponent>(*world, e);
    if (!comp) return 0;
    comp->enabled = helper::CheckBool(L, 2);
    return 0;
}

// ============================================================
// IK 系统 (IKChain3DComponent)
// ============================================================

int L_EcsAddIKComponent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    world->registry().emplace_or_replace<IKChain3DComponent>(e);
    return 0;
}

int L_EcsAddIKChain(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* comp = helper::TryGetComponent<IKChain3DComponent>(*world, e);
    if (!comp) { helper::PushInt(L, -1); return 1; }

    IKChainConfig chain;
    chain.name = luaL_optstring(L, 2, "");
    int type = helper::OptInt(L, 3, 0);
    chain.type = static_cast<IKChainType>(type);
    chain.root_bone = luaL_optstring(L, 4, "");
    chain.tip_bone = luaL_optstring(L, 5, "");
    chain.weight = helper::OptFloat(L, 6, 1.0f);
    comp->chains.push_back(std::move(chain));
    helper::PushInt(L, static_cast<int>(comp->chains.size()) - 1);
    return 1;
}

int L_EcsSetIKTarget(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    auto* comp = helper::TryGetComponent<IKChain3DComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return 0;
    comp->chains[idx].target_position = helper::CheckVec3(L, 3);
    return 0;
}

int L_EcsSetIKTargetEntity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    auto* comp = helper::TryGetComponent<IKChain3DComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return 0;
    if (lua_isnil(L, 3)) {
        comp->chains[idx].target_entity = UINT32_MAX;
    } else {
        Entity target = helper::CheckEntity(L, 3);
        comp->chains[idx].target_entity = static_cast<uint32_t>(target);
    }
    return 0;
}

int L_EcsSetIKWeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    float w = helper::CheckFloat(L, 3);
    auto* comp = helper::TryGetComponent<IKChain3DComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return 0;
    comp->chains[idx].weight = w;
    return 0;
}

int L_EcsSetIKPoleVector(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    auto* comp = helper::TryGetComponent<IKChain3DComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return 0;
    comp->chains[idx].pole_vector = helper::CheckVec3(L, 3);
    return 0;
}

int L_EcsSetIKIterations(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    int iters = helper::CheckInt(L, 3);
    auto* comp = helper::TryGetComponent<IKChain3DComponent>(*world, e);
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return 0;
    comp->chains[idx].iterations = iters;
    return 0;
}

int L_EcsSetIKEnabled(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* comp = helper::TryGetComponent<IKChain3DComponent>(*world, e);
    if (!comp) return 0;
    comp->enabled = helper::CheckBool(L, 2);
    return 0;
}

// ============================================================
// 3D 动画事件 (Animator3DComponent)
// ============================================================

int L_EcsAddAnimator3DEvent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const char* event_name = luaL_checkstring(L, 2);
    float trigger_time = helper::CheckFloat(L, 3);
    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (!animator) return 0;
    AnimEventConfig evt;
    evt.name = event_name;
    evt.trigger_time = trigger_time;
    evt.fired = false;
    animator->events.push_back(std::move(evt));
    return 0;
}

int L_EcsPopAnimator3DEvent(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushstring(L, ""); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (animator && !animator->fired_events.empty()) {
        std::string name = animator->fired_events.front();
        animator->fired_events.erase(animator->fired_events.begin());
        lua_pushstring(L, name.c_str());
        return 1;
    }
    lua_pushstring(L, "");
    return 1;
}

int L_EcsSetAnimator3DExtractRootMotion(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* animator = helper::TryGetComponent<Animator3DComponent>(*world, e);
    if (!animator) return 0;
    animator->extract_root_motion = helper::CheckBool(L, 2);
    return 0;
}

int L_EcsGetAnimator3DRootMotionDelta(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    const auto* animator = helper::TryGetComponentConst<Animator3DComponent>(*world, e);
    if (!animator) return 0;
    helper::PushVec3(L, animator->root_motion_delta);
    return 3;
}

// ============================================================
// 骨骼挂点系统 (BoneAttachmentComponent)
// ============================================================

// ecs.add_bone_attachment(entity, target_entity, bone_name)
int L_EcsAddBoneAttachment(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    Entity target = helper::CheckEntity(L, 2);
    const char* bone = luaL_checkstring(L, 3);
    auto& comp = world->registry().emplace_or_replace<dse::BoneAttachmentComponent>(e);
    comp.target_entity = target;
    comp.bone_name = bone;
    comp.index_dirty = true;
    comp.cached_bone_index = -1;
    return 0;
}

// ecs.set_bone_attachment_offset(entity, px,py,pz, rx,ry,rz,rw, [sx,sy,sz])
int L_EcsSetBoneAttachmentOffset(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* comp = helper::TryGetComponent<dse::BoneAttachmentComponent>(*world, e);
    if (!comp) return 0;
    comp->offset_position = glm::vec3(
        helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4));
    comp->offset_rotation = glm::quat(
        helper::CheckFloat(L, 8), // w
        helper::CheckFloat(L, 5), // x
        helper::CheckFloat(L, 6), // y
        helper::CheckFloat(L, 7));// z
    comp->offset_scale = glm::vec3(
        helper::OptFloat(L, 9, 1.0f), helper::OptFloat(L, 10, 1.0f), helper::OptFloat(L, 11, 1.0f));
    return 0;
}

// ecs.set_bone_attachment_bone(entity, bone_name)
int L_EcsSetBoneAttachmentBone(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* comp = helper::TryGetComponent<dse::BoneAttachmentComponent>(*world, e);
    if (!comp) return 0;
    comp->bone_name = luaL_checkstring(L, 2);
    comp->index_dirty = true;
    comp->cached_bone_index = -1;
    return 0;
}

// ecs.set_bone_attachment_target(entity, target_entity)
int L_EcsSetBoneAttachmentTarget(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* comp = helper::TryGetComponent<dse::BoneAttachmentComponent>(*world, e);
    if (!comp) return 0;
    comp->target_entity = helper::CheckEntity(L, 2);
    comp->index_dirty = true;
    comp->cached_bone_index = -1;
    return 0;
}

// ecs.get_bone_world_position(target_entity, bone_name) -> x, y, z
int L_EcsGetBoneWorldPosition(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity target = helper::CheckEntity(L, 1);
    const char* bone_name = luaL_checkstring(L, 2);
    auto* anim = world->registry().try_get<Animator3DComponent>(target);
    auto* xform = world->registry().try_get<TransformComponent>(target);
    if (!anim || !xform || !anim->skel_cache.valid) {
        helper::PushVec3(L, glm::vec3(0.0f));
        return 3;
    }
    auto it = anim->skel_cache.bone_name_to_index.find(bone_name);
    if (it == anim->skel_cache.bone_name_to_index.end() ||
        it->second >= static_cast<int>(anim->final_bone_matrices.size()) ||
        it->second >= static_cast<int>(anim->skel_cache.bind_globals.size())) {
        helper::PushVec3(L, glm::vec3(0.0f));
        return 3;
    }
    int bi = it->second;
    glm::mat4 bone_model = anim->final_bone_matrices[bi] * anim->skel_cache.bind_globals[bi];
    glm::vec4 world_pos = xform->local_to_world * bone_model * glm::vec4(0, 0, 0, 1);
    helper::PushVec3(L, glm::vec3(world_pos));
    return 3;
}

// ecs.remove_bone_attachment(entity)
int L_EcsRemoveBoneAttachment(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    if (world->registry().all_of<dse::BoneAttachmentComponent>(e)) {
        world->registry().remove<dse::BoneAttachmentComponent>(e);
    }
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
        {"set_animator_3d_lock_root_motion", L_EcsSetAnimator3DLockRootMotion},
        // 3D 动画事件 + Root Motion
        {"add_animator_3d_event",            L_EcsAddAnimator3DEvent},
        {"pop_animator_3d_event",            L_EcsPopAnimator3DEvent},
        {"set_animator_3d_extract_root_motion", L_EcsSetAnimator3DExtractRootMotion},
        {"get_animator_3d_root_motion_delta", L_EcsGetAnimator3DRootMotionDelta},
        // 动画层系统
        {"add_anim_layer_component",         L_EcsAddAnimLayerComponent},
        {"add_anim_layer",                   L_EcsAddAnimLayer},
        {"set_anim_layer_clip",              L_EcsSetAnimLayerClip},
        {"set_anim_layer_weight",            L_EcsSetAnimLayerWeight},
        {"set_anim_layer_bone_mask",         L_EcsSetAnimLayerBoneMask},
        {"set_anim_layer_blend_tree_1d",     L_EcsSetAnimLayerBlendTree1D},
        {"set_anim_layer_blend_param",       L_EcsSetAnimLayerBlendParam},
        {"set_anim_layer_enabled",           L_EcsSetAnimLayerEnabled},
        // IK 系统
        {"add_ik_component",                 L_EcsAddIKComponent},
        {"add_ik_chain",                     L_EcsAddIKChain},
        {"set_ik_target",                    L_EcsSetIKTarget},
        {"set_ik_target_entity",             L_EcsSetIKTargetEntity},
        {"set_ik_weight",                    L_EcsSetIKWeight},
        {"set_ik_pole_vector",               L_EcsSetIKPoleVector},
        {"set_ik_iterations",                L_EcsSetIKIterations},
        {"set_ik_enabled",                   L_EcsSetIKEnabled},
        // 骨骼挂点系统
        {"add_bone_attachment",              L_EcsAddBoneAttachment},
        {"set_bone_attachment_offset",       L_EcsSetBoneAttachmentOffset},
        {"set_bone_attachment_bone",         L_EcsSetBoneAttachmentBone},
        {"set_bone_attachment_target",       L_EcsSetBoneAttachmentTarget},
        {"get_bone_world_position",          L_EcsGetBoneWorldPosition},
        {"remove_bone_attachment",           L_EcsRemoveBoneAttachment},
    });
}

} // namespace dse::runtime::lua_binding
