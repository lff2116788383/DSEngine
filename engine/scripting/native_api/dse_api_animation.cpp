/**
 * @file dse_api_animation.cpp
 * @brief DSEngine Native C ABI — 动画子系统（L4/L5，手写）
 *
 * 覆盖 2D 帧动画、3D 骨骼动画状态机（FSM）、动画层/混合树、IK、
 * 根运动/事件、骨骼挂点、Morph Target。均为纯 ECS 操作（无服务委托），
 * 供 Lua / C# / 编辑器端共享同一实现。语义与原 Lua 绑定逐一等价。
 *
 * 约定：
 *   - 浮点参数 NaN = 保持当前值；int 负值（按声明）= 保持。
 *   - 字符串输出走 out 缓冲（null 结尾，按 cap 截断）。
 *   - 数组输入走扁平指针 + count。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_animation.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/animation_state_machine.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
inline bool Keep(float v) { return std::isnan(v); }  // NaN => 保持当前值

// 拷贝 std::string 到 out 缓冲（null 结尾，按 cap 截断）。
inline void CopyStr(const std::string& s, char* out, int cap) {
    if (!out || cap <= 0) return;
    int n = static_cast<int>(s.size());
    if (n > cap - 1) n = cap - 1;
    std::memcpy(out, s.data(), static_cast<size_t>(n));
    out[n] = '\0';
}

}  // namespace

// ============================================================
// 2D 帧动画（AnimatorComponent，全局命名空间）
// ============================================================

extern "C" void dse_anim2d_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<AnimatorComponent>(TE(e));
}

extern "C" void dse_anim2d_add_state(uint32_t e, const char* name, float fps, int loop,
                                     const uint32_t* frame_handles, int handle_count) {
    World* world = GW();
    if (!world || !name) return;
    auto* animator = world->registry().try_get<AnimatorComponent>(TE(e));
    if (!animator) return;
    AnimationState state;
    state.name = name;
    state.frame_rate = fps;
    state.loop = (loop != 0);
    if (frame_handles && handle_count > 0) {
        state.frame_handles.reserve(static_cast<size_t>(handle_count));
        for (int i = 0; i < handle_count; ++i) {
            state.frame_handles.push_back(static_cast<unsigned int>(frame_handles[i]));
        }
    }
    animator->states[name] = std::move(state);
}

extern "C" void dse_anim2d_add_event(uint32_t e, const char* state_name,
                                     float normalized_time, const char* event_name) {
    World* world = GW();
    if (!world || !state_name || !event_name) return;
    if (normalized_time < 0.0f) normalized_time = 0.0f;
    if (normalized_time > 1.0f) normalized_time = 1.0f;
    auto* animator = world->registry().try_get<AnimatorComponent>(TE(e));
    if (!animator) return;
    auto it = animator->states.find(state_name);
    if (it != animator->states.end()) {
        it->second.events.emplace_back(normalized_time, std::string(event_name));
    }
}

extern "C" void dse_anim2d_play(uint32_t e, const char* state_name) {
    World* world = GW();
    if (!world || !state_name) return;
    auto* animator = world->registry().try_get<AnimatorComponent>(TE(e));
    if (!animator) return;
    animator->current_state = state_name;
    animator->current_time = 0.0f;
    animator->current_frame = 0;
    animator->playing = true;
}

extern "C" void dse_anim2d_play_segment(uint32_t e, int start_frame, int end_frame, int loop) {
    World* world = GW();
    if (!world) return;
    auto* animator = world->registry().try_get<AnimatorComponent>(TE(e));
    if (!animator) return;
    animator->PlaySegment(start_frame, end_frame, loop != 0);
}

extern "C" int dse_anim2d_pop_event(uint32_t e, char* out, int cap) {
    if (out && cap > 0) out[0] = '\0';
    World* world = GW();
    if (!world) return 0;
    auto* animator = world->registry().try_get<AnimatorComponent>(TE(e));
    if (animator && !animator->fired_events.empty()) {
        CopyStr(animator->fired_events.front(), out, cap);
        animator->fired_events.erase(animator->fired_events.begin());
        return 1;
    }
    return 0;
}

// ============================================================
// 3D 骨骼动画 / 状态机（Animator3DComponent）
// ============================================================

extern "C" void dse_anim3d_add(uint32_t e, const char* danim_path, const char* dskel_path) {
    World* world = GW();
    if (!world) return;
    auto& animator = world->registry().emplace_or_replace<Animator3DComponent>(TE(e));
    animator.danim_path = danim_path ? danim_path : "";
    animator.dskel_path = dskel_path ? dskel_path : "";
    animator.enabled = true;
    animator.current_time = 0.0f;
    animator.speed = 1.0f;
    animator.loop = true;
}

// state_name=null 不改状态；speed=NaN 保持；loop<0 保持。
extern "C" void dse_anim3d_set_state(uint32_t e, const char* state_name,
                                     float speed, int loop) {
    World* world = GW();
    if (!world) return;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!animator) return;
    if (state_name) {
        if (animator->state_machine) {
            animator->current_state_name = state_name;
            animator->state_time = 0.0f;
            animator->is_transitioning = false;
        } else {
            animator->danim_path = state_name;
        }
    }
    if (!Keep(speed)) animator->speed = speed;
    if (loop >= 0)    animator->loop = (loop != 0);
}

extern "C" int dse_anim3d_get_state(uint32_t e, char* out_state, int state_cap,
                                    float* out_norm, float* out_time, float* out_speed,
                                    int* out_loop, int* out_transitioning,
                                    int* out_bone_count, int* out_has_skel) {
    World* world = GW();
    const Animator3DComponent* a = world ?
        world->registry().try_get<Animator3DComponent>(TE(e)) : nullptr;
    if (!a) {
        if (out_state && state_cap > 0) out_state[0] = '\0';
        if (out_norm) *out_norm = 0.0f;
        if (out_time) *out_time = 0.0f;
        if (out_speed) *out_speed = 0.0f;
        if (out_loop) *out_loop = 0;
        if (out_transitioning) *out_transitioning = 0;
        if (out_bone_count) *out_bone_count = 0;
        if (out_has_skel) *out_has_skel = 0;
        return 0;
    }
    CopyStr(a->current_state_name.empty() ? a->danim_path : a->current_state_name,
            out_state, state_cap);
    if (out_norm) *out_norm = a->normalized_time;
    if (out_time) *out_time = a->state_machine ? a->state_time : a->current_time;
    if (out_speed) *out_speed = a->speed;
    if (out_loop) *out_loop = a->loop ? 1 : 0;
    if (out_transitioning) *out_transitioning = a->is_transitioning ? 1 : 0;
    if (out_bone_count) *out_bone_count = static_cast<int>(a->final_bone_matrices.size());
    if (out_has_skel) *out_has_skel = a->dskel_path.empty() ? 0 : 1;
    return 1;
}

extern "C" void dse_anim3d_init_fsm(uint32_t e) {
    World* world = GW();
    if (!world) return;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!animator) return;
    animator->state_machine = std::make_shared<gameplay3d::AnimationStateMachine>();
}

extern "C" void dse_anim3d_add_fsm_state(uint32_t e, const char* state_name,
                                         const char* danim_path, int loop, float speed) {
    World* world = GW();
    if (!world || !state_name || !danim_path) return;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!animator || !animator->state_machine) return;
    gameplay3d::AnimState state;
    state.name = state_name;
    state.danim_path = danim_path;
    state.loop = (loop != 0);
    state.speed = Keep(speed) ? 1.0f : speed;
    state.is_blend_tree = false;
    animator->state_machine->AddState(state);
}

// 条件以并行扁平数组传入：cond_names[i]/cond_modes[i]/cond_thresholds[i]/cond_ints[i]。
extern "C" void dse_anim3d_add_transition(uint32_t e, const char* from_state,
                                          const char* to_state, float transition_duration,
                                          int has_exit_time, float exit_time,
                                          int cond_count,
                                          const char* const* cond_names,
                                          const int* cond_modes,
                                          const float* cond_thresholds,
                                          const int* cond_ints) {
    World* world = GW();
    if (!world || !from_state || !to_state) return;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!animator || !animator->state_machine) return;

    auto& states = animator->state_machine->GetStatesMutable();
    auto it = states.find(from_state);
    if (it == states.end()) return;

    gameplay3d::AnimTransition trans;
    trans.target_state = to_state;
    trans.transition_duration = Keep(transition_duration) ? 0.25f : transition_duration;
    trans.has_exit_time = (has_exit_time != 0);
    trans.exit_time = Keep(exit_time) ? 1.0f : exit_time;

    for (int i = 0; i < cond_count; ++i) {
        gameplay3d::AnimTransitionCondition cond;
        if (cond_names && cond_names[i]) cond.parameter_name = cond_names[i];
        if (cond_modes) cond.mode = static_cast<gameplay3d::AnimConditionMode>(cond_modes[i]);
        if (cond_thresholds) cond.threshold = cond_thresholds[i];
        if (cond_ints) cond.int_value = cond_ints[i];
        trans.conditions.push_back(std::move(cond));
    }
    it->second.transitions.push_back(std::move(trans));
}

extern "C" void dse_anim3d_set_param_float(uint32_t e, const char* param_name, float value) {
    World* world = GW();
    if (!world || !param_name) return;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!animator || !animator->state_machine) return;
    auto& sm = *animator->state_machine;
    if (sm.GetParameters().find(param_name) == sm.GetParameters().end()) {
        sm.AddParameter(param_name, gameplay3d::AnimParamType::Float, 0.0f);
    }
    sm.SetFloat(param_name, value);
}

extern "C" void dse_anim3d_set_param_trigger(uint32_t e, const char* param_name) {
    World* world = GW();
    if (!world || !param_name) return;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!animator || !animator->state_machine) return;
    auto& sm = *animator->state_machine;
    if (sm.GetParameters().find(param_name) == sm.GetParameters().end()) {
        sm.AddTrigger(param_name);
    }
    sm.SetTrigger(param_name);
}

extern "C" void dse_anim3d_set_lock_root_motion(uint32_t e, int lock) {
    World* world = GW();
    if (!world) return;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!animator) return;
    animator->lock_root_motion = (lock != 0);
}

// ---- 3D 动画事件 / 根运动 ----
extern "C" void dse_anim3d_add_event(uint32_t e, const char* event_name, float trigger_time) {
    World* world = GW();
    if (!world || !event_name) return;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!animator) return;
    AnimEventConfig evt;
    evt.name = event_name;
    evt.trigger_time = trigger_time;
    evt.fired = false;
    animator->events.push_back(std::move(evt));
}

extern "C" int dse_anim3d_pop_event(uint32_t e, char* out, int cap) {
    if (out && cap > 0) out[0] = '\0';
    World* world = GW();
    if (!world) return 0;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (animator && !animator->fired_events.empty()) {
        CopyStr(animator->fired_events.front(), out, cap);
        animator->fired_events.erase(animator->fired_events.begin());
        return 1;
    }
    return 0;
}

extern "C" void dse_anim3d_set_extract_root_motion(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* animator = world->registry().try_get<Animator3DComponent>(TE(e));
    if (!animator) return;
    animator->extract_root_motion = (enabled != 0);
}

extern "C" int dse_anim3d_get_root_motion_delta(uint32_t e, float* out_xyz) {
    World* world = GW();
    const Animator3DComponent* a = world ?
        world->registry().try_get<Animator3DComponent>(TE(e)) : nullptr;
    if (!a) {
        if (out_xyz) { out_xyz[0] = 0.0f; out_xyz[1] = 0.0f; out_xyz[2] = 0.0f; }
        return 0;
    }
    if (out_xyz) {
        out_xyz[0] = a->root_motion_delta.x;
        out_xyz[1] = a->root_motion_delta.y;
        out_xyz[2] = a->root_motion_delta.z;
    }
    return 1;
}

// ============================================================
// 动画层系统（AnimLayerComponent）
// ============================================================

extern "C" void dse_animlayer_add_component(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<AnimLayerComponent>(TE(e));
}

extern "C" int dse_animlayer_add(uint32_t e, const char* name, float weight, int blend_mode) {
    World* world = GW();
    if (!world) return -1;
    auto* comp = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!comp) return -1;
    AnimLayerConfig layer;
    layer.name = name ? name : "";
    layer.weight = Keep(weight) ? 1.0f : weight;
    layer.blend_mode = static_cast<AnimLayerBlendMode>(blend_mode);
    comp->layers.push_back(std::move(layer));
    return static_cast<int>(comp->layers.size()) - 1;
}

extern "C" void dse_animlayer_set_clip(uint32_t e, int idx, const char* danim_path,
                                       float speed, int loop) {
    World* world = GW();
    if (!world || !danim_path) return;
    auto* comp = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return;
    auto& layer = comp->layers[idx];
    layer.source_type = AnimSourceType::SingleClip;
    layer.danim_path = danim_path;
    layer.speed = Keep(speed) ? 1.0f : speed;
    layer.loop = (loop != 0);
}

extern "C" void dse_animlayer_set_weight(uint32_t e, int idx, float w) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return;
    comp->layers[idx].weight = w;
}

extern "C" void dse_animlayer_set_bone_mask(uint32_t e, int idx,
                                            const char* const* bones, int count) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return;
    auto& layer = comp->layers[idx];
    layer.bone_mask_include.clear();
    if (bones) {
        for (int i = 0; i < count; ++i) {
            if (bones[i]) layer.bone_mask_include.emplace_back(bones[i]);
        }
    }
    layer.bone_mask_dirty = true;
}

extern "C" void dse_animlayer_set_blend_tree_1d(uint32_t e, int idx,
                                                const char* const* paths,
                                                const float* thresholds,
                                                const float* speeds, int count) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return;
    auto& layer = comp->layers[idx];
    layer.source_type = AnimSourceType::BlendTree1D;
    layer.blend_nodes.clear();
    for (int i = 0; i < count; ++i) {
        AnimBlendNode node;
        if (paths && paths[i]) node.danim_path = paths[i];
        node.threshold = thresholds ? thresholds[i] : 0.0f;
        node.speed = speeds ? speeds[i] : 1.0f;
        node.loop = true;
        layer.blend_nodes.push_back(std::move(node));
    }
}

extern "C" void dse_animlayer_set_blend_param(uint32_t e, int idx, float val) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->layers.size())) return;
    comp->layers[idx].blend_parameter_value = val;
}

extern "C" void dse_animlayer_set_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<AnimLayerComponent>(TE(e));
    if (!comp) return;
    comp->enabled = (enabled != 0);
}

// ============================================================
// IK 系统（IKChain3DComponent）
// ============================================================

extern "C" void dse_ik_add_component(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<IKChain3DComponent>(TE(e));
}

extern "C" int dse_ik_add_chain(uint32_t e, const char* name, int type,
                                const char* root_bone, const char* tip_bone, float weight) {
    World* world = GW();
    if (!world) return -1;
    auto* comp = world->registry().try_get<IKChain3DComponent>(TE(e));
    if (!comp) return -1;
    IKChainConfig chain;
    chain.name = name ? name : "";
    chain.type = static_cast<IKChainType>(type);
    chain.root_bone = root_bone ? root_bone : "";
    chain.tip_bone = tip_bone ? tip_bone : "";
    chain.weight = Keep(weight) ? 1.0f : weight;
    comp->chains.push_back(std::move(chain));
    return static_cast<int>(comp->chains.size()) - 1;
}

extern "C" void dse_ik_set_target(uint32_t e, int idx, float x, float y, float z) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<IKChain3DComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return;
    comp->chains[idx].target_position = glm::vec3(x, y, z);
}

// target=UINT32_MAX 表示清除目标实体。
extern "C" void dse_ik_set_target_entity(uint32_t e, int idx, uint32_t target) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<IKChain3DComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return;
    comp->chains[idx].target_entity = target;
}

extern "C" void dse_ik_set_weight(uint32_t e, int idx, float w) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<IKChain3DComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return;
    comp->chains[idx].weight = w;
}

extern "C" void dse_ik_set_pole_vector(uint32_t e, int idx, float x, float y, float z) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<IKChain3DComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return;
    comp->chains[idx].pole_vector = glm::vec3(x, y, z);
}

extern "C" void dse_ik_set_iterations(uint32_t e, int idx, int iters) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<IKChain3DComponent>(TE(e));
    if (!comp || idx < 0 || idx >= static_cast<int>(comp->chains.size())) return;
    comp->chains[idx].iterations = iters;
}

extern "C" void dse_ik_set_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<IKChain3DComponent>(TE(e));
    if (!comp) return;
    comp->enabled = (enabled != 0);
}

// ============================================================
// 骨骼挂点系统（BoneAttachmentComponent）
// ============================================================

extern "C" void dse_bone_attach_add(uint32_t e, uint32_t target, const char* bone_name) {
    World* world = GW();
    if (!world || !bone_name) return;
    auto& comp = world->registry().emplace_or_replace<BoneAttachmentComponent>(TE(e));
    comp.target_entity = TE(target);
    comp.bone_name = bone_name;
    comp.index_dirty = true;
    comp.cached_bone_index = -1;
}

extern "C" void dse_bone_attach_set_offset(uint32_t e, float px, float py, float pz,
                                           float qx, float qy, float qz, float qw,
                                           float sx, float sy, float sz) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<BoneAttachmentComponent>(TE(e));
    if (!comp) return;
    comp->offset_position = glm::vec3(px, py, pz);
    comp->offset_rotation = glm::quat(qw, qx, qy, qz);
    comp->offset_scale = glm::vec3(Keep(sx) ? 1.0f : sx,
                                   Keep(sy) ? 1.0f : sy,
                                   Keep(sz) ? 1.0f : sz);
}

extern "C" void dse_bone_attach_set_bone(uint32_t e, const char* bone_name) {
    World* world = GW();
    if (!world || !bone_name) return;
    auto* comp = world->registry().try_get<BoneAttachmentComponent>(TE(e));
    if (!comp) return;
    comp->bone_name = bone_name;
    comp->index_dirty = true;
    comp->cached_bone_index = -1;
}

extern "C" void dse_bone_attach_set_target(uint32_t e, uint32_t target) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<BoneAttachmentComponent>(TE(e));
    if (!comp) return;
    comp->target_entity = TE(target);
    comp->index_dirty = true;
    comp->cached_bone_index = -1;
}

// 由目标实体的动画姿态计算骨骼世界坐标。out_xyz 始终写入；返回 1=成功/0=失败（写 0）。
extern "C" int dse_bone_attach_get_world_pos(uint32_t target, const char* bone_name,
                                             float* out_xyz) {
    if (out_xyz) { out_xyz[0] = 0.0f; out_xyz[1] = 0.0f; out_xyz[2] = 0.0f; }
    World* world = GW();
    if (!world || !bone_name) return 0;
    auto& reg = world->registry();
    Entity t = TE(target);
    auto* anim = reg.try_get<Animator3DComponent>(t);
    auto* xform = reg.try_get<TransformComponent>(t);
    if (!anim || !xform || !anim->skel_cache.valid) return 0;
    auto it = anim->skel_cache.bone_name_to_index.find(bone_name);
    if (it == anim->skel_cache.bone_name_to_index.end() ||
        it->second >= static_cast<int>(anim->final_bone_matrices.size()) ||
        it->second >= static_cast<int>(anim->skel_cache.bind_globals.size())) {
        return 0;
    }
    int bi = it->second;
    glm::mat4 bone_model = anim->final_bone_matrices[bi] * anim->skel_cache.bind_globals[bi];
    glm::vec4 world_pos = xform->local_to_world * bone_model * glm::vec4(0, 0, 0, 1);
    if (out_xyz) {
        out_xyz[0] = world_pos.x;
        out_xyz[1] = world_pos.y;
        out_xyz[2] = world_pos.z;
    }
    return 1;
}

extern "C" void dse_bone_attach_remove(uint32_t e) {
    World* world = GW();
    if (!world) return;
    auto& reg = world->registry();
    if (reg.all_of<BoneAttachmentComponent>(TE(e))) {
        reg.remove<BoneAttachmentComponent>(TE(e));
    }
}

// ============================================================
// Morph Target（Blend Shape，MorphTargetComponent）
// ============================================================

extern "C" void dse_morph_add_component(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<MorphTargetComponent>(TE(e));
}

// deltas 扁平布局：每顶点 6 float（dpx,dpy,dpz, dnx,dny,dnz）。
extern "C" void dse_morph_add_target(uint32_t e, const char* name,
                                     const float* deltas, int float_count) {
    World* world = GW();
    if (!world || !name) return;
    auto* comp = world->registry().try_get<MorphTargetComponent>(TE(e));
    if (!comp) return;
    int vert_count = float_count / 6;
    MorphTargetData target;
    target.name = name;
    target.deltas.resize(static_cast<size_t>(vert_count));
    for (int i = 0; i < vert_count; ++i) {
        int base = i * 6;
        target.deltas[i].delta_position.x = deltas ? deltas[base + 0] : 0.0f;
        target.deltas[i].delta_position.y = deltas ? deltas[base + 1] : 0.0f;
        target.deltas[i].delta_position.z = deltas ? deltas[base + 2] : 0.0f;
        target.deltas[i].delta_normal.x = deltas ? deltas[base + 3] : 0.0f;
        target.deltas[i].delta_normal.y = deltas ? deltas[base + 4] : 0.0f;
        target.deltas[i].delta_normal.z = deltas ? deltas[base + 5] : 0.0f;
        target.deltas[i]._pad0 = 0.0f;
        target.deltas[i]._pad1 = 0.0f;
    }
    comp->targets.push_back(std::move(target));
    comp->weights.push_back(0.0f);
    if (comp->vertex_count == 0) comp->vertex_count = vert_count;
    comp->gpu_dirty = true;
}

extern "C" void dse_morph_set_weight(uint32_t e, const char* name, float w) {
    World* world = GW();
    if (!world || !name) return;
    auto* comp = world->registry().try_get<MorphTargetComponent>(TE(e));
    if (!comp) return;
    comp->SetWeight(name, w);
}

extern "C" void dse_morph_set_weight_index(uint32_t e, int idx, float w) {
    World* world = GW();
    if (!world) return;
    auto* comp = world->registry().try_get<MorphTargetComponent>(TE(e));
    if (!comp) return;
    comp->SetWeightByIndex(idx, w);
}

extern "C" float dse_morph_get_weight(uint32_t e, const char* name) {
    World* world = GW();
    if (!world || !name) return 0.0f;
    auto* comp = world->registry().try_get<MorphTargetComponent>(TE(e));
    return comp ? comp->GetWeight(name) : 0.0f;
}

extern "C" int dse_morph_get_target_count(uint32_t e) {
    World* world = GW();
    if (!world) return 0;
    auto* comp = world->registry().try_get<MorphTargetComponent>(TE(e));
    return comp ? static_cast<int>(comp->targets.size()) : 0;
}
