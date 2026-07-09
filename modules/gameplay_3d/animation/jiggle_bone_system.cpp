/**
 * @file jiggle_bone_system.cpp
 * @brief 次级骨骼动力学（弹簧骨 / 乳摇）实现。见头文件说明。
 */

#include "modules/gameplay_3d/animation/jiggle_bone_system.h"
#include "engine/ecs/components_3d_animation.h"

#include <algorithm>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace dse {
namespace gameplay3d {

namespace {

/// 从含缩放的仿射矩阵提取纯旋转四元数（列归一化去掉缩放）。
glm::quat RotationFromMatrix(const glm::mat4& m) {
    glm::vec3 c0(m[0]);
    glm::vec3 c1(m[1]);
    glm::vec3 c2(m[2]);
    const float l0 = glm::length(c0);
    const float l1 = glm::length(c1);
    const float l2 = glm::length(c2);
    if (l0 < 1e-8f || l1 < 1e-8f || l2 < 1e-8f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    glm::mat3 rot(c0 / l0, c1 / l1, c2 / l2);
    return glm::normalize(glm::quat_cast(rot));
}

/// 计算把单位向量 a 旋到单位向量 b 的最短旋转（不依赖 GLM_ENABLE_EXPERIMENTAL）。
glm::quat RotationBetween(const glm::vec3& a, const glm::vec3& b) {
    const float d = glm::clamp(glm::dot(a, b), -1.0f, 1.0f);
    if (d > 0.999999f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    if (d < -0.999999f) {
        // 反向：绕任意垂直轴旋转 180°
        glm::vec3 axis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), a);
        if (glm::dot(axis, axis) < 1e-8f) {
            axis = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), a);
        }
        return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
    }
    const glm::vec3 axis = glm::normalize(glm::cross(a, b));
    const float angle = std::acos(d);
    return glm::angleAxis(angle, axis);
}

} // namespace

glm::vec3 JiggleIntegrate(const JiggleStepInput& in, float dt) {
    const float stiffness = glm::clamp(in.stiffness, 0.0f, 1.0f);
    const float damping = glm::clamp(in.damping, 0.0f, 1.0f);

    // Verlet: 由位置差得到惯性速度，施加阻尼
    const glm::vec3 velocity = (in.cur_tip - in.prev_tip) * (1.0f - damping);
    glm::vec3 tip = in.cur_tip + velocity + in.gravity * (dt * dt);

    // 弹簧：向动画驱动的静止末端回拉
    tip += (in.rest_tip - tip) * stiffness;

    // 长度约束：末端始终位于以骨骼头为球心、半径 length 的球面上
    glm::vec3 dir = tip - in.head_pos;
    const float len = glm::length(dir);
    if (len > 1e-6f) {
        tip = in.head_pos + dir * (in.length / len);
    } else {
        tip = in.rest_tip;
    }
    return tip;
}

void JiggleBoneSystem::Update(World& world, float delta_time) {
    auto view = world.registry().view<Animator3DComponent, JiggleBoneComponent>();

    // 限制步长，避免掉帧时弹簧爆炸
    const float dt = std::min(std::max(delta_time, 0.0f), 1.0f / 30.0f);

    for (auto entity : view) {
        auto& animator = view.get<Animator3DComponent>(entity);
        auto& jiggle = view.get<JiggleBoneComponent>(entity);
        if (!jiggle.enabled || !animator.enabled) continue;

        const auto& cache = animator.skel_cache;
        if (!cache.valid || cache.bone_count == 0) continue;
        const uint32_t bone_count = cache.bone_count;
        auto& pb = animator.pose_buffer;
        if (pb.Size() < bone_count) continue;

        // ── 由当前 pose_buffer 计算模型空间全局变换（与 ComputeEntityBones 的全局段一致）──
        glm::mat4 local[MAX_BONES];
        glm::mat4 global[MAX_BONES];
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (pb.touched[i]) {
                local[i] = glm::translate(glm::mat4(1.0f), pb.positions[i])
                    * glm::mat4_cast(pb.rotations[i])
                    * glm::scale(glm::mat4(1.0f), pb.scales[i]);
            } else {
                local[i] = cache.local_bind_poses[i];
            }
        }
        for (uint32_t idx : cache.topo_order) {
            if (idx >= bone_count) continue;
            const int pi = cache.parent_indices[idx];
            if (pi < 0 || pi >= static_cast<int>(bone_count)) {
                global[idx] = local[idx];
            } else {
                global[idx] = global[pi] * local[idx];
            }
        }

        // 解析碰撞体骨骼索引并算出模型空间球心
        for (auto& col : jiggle.colliders) {
            if (col.index_dirty) {
                col.bone_index = -1;
                if (!col.bone_name.empty()) {
                    auto it = cache.bone_name_to_index.find(col.bone_name);
                    if (it != cache.bone_name_to_index.end()) col.bone_index = it->second;
                }
                col.index_dirty = false;
            }
        }

        for (auto& cfg : jiggle.bones) {
            if (cfg.index_dirty) {
                cfg.bone_index = -1;
                auto it = cache.bone_name_to_index.find(cfg.bone_name);
                if (it != cache.bone_name_to_index.end()) cfg.bone_index = it->second;
                cfg.index_dirty = false;
                cfg.initialized = false;
            }
            const int bi = cfg.bone_index;
            if (bi < 0 || bi >= static_cast<int>(bone_count)) continue;
            if (cfg.bone_length <= 1e-6f) continue;

            const glm::mat4& bg = global[bi];
            const glm::vec3 head = glm::vec3(bg[3]);
            const glm::vec3 rest_tip = glm::vec3(bg * glm::vec4(cfg.rest_dir * cfg.bone_length, 1.0f));

            if (!cfg.initialized) {
                cfg.cur_tip = rest_tip;
                cfg.prev_tip = rest_tip;
                cfg.initialized = true;
                continue; // 首帧对齐动画姿态，不产生突变
            }

            JiggleStepInput in;
            in.cur_tip = cfg.cur_tip;
            in.prev_tip = cfg.prev_tip;
            in.rest_tip = rest_tip;
            in.head_pos = head;
            in.gravity = cfg.gravity_dir * (cfg.gravity * jiggle.gravity_scale);
            in.stiffness = cfg.stiffness * jiggle.stiffness_scale;
            in.damping = cfg.damping * jiggle.damping_scale;
            in.length = cfg.bone_length;

            glm::vec3 new_tip = JiggleIntegrate(in, dt);

            // 球体碰撞：把末端推出碰撞球外，再重新施加长度约束
            for (const auto& col : jiggle.colliders) {
                if (col.radius <= 0.0f) continue;
                glm::vec3 center;
                if (col.bone_index >= 0 && col.bone_index < static_cast<int>(bone_count)) {
                    center = glm::vec3(global[col.bone_index] * glm::vec4(col.center, 1.0f));
                } else {
                    center = col.center; // 模型根空间
                }
                const glm::vec3 delta = new_tip - center;
                const float dist = glm::length(delta);
                const float min_dist = col.radius;
                if (dist < min_dist && dist > 1e-6f) {
                    new_tip = center + delta * (min_dist / dist);
                    glm::vec3 dir2 = new_tip - head;
                    const float l2 = glm::length(dir2);
                    if (l2 > 1e-6f) new_tip = head + dir2 * (cfg.bone_length / l2);
                }
            }

            cfg.prev_tip = cfg.cur_tip;
            cfg.cur_tip = new_tip;

            const glm::vec3 rest_dir_m = rest_tip - head;
            const glm::vec3 cur_dir_m = new_tip - head;
            const float rl = glm::length(rest_dir_m);
            const float cl = glm::length(cur_dir_m);
            if (rl < 1e-6f || cl < 1e-6f) continue;

            const glm::quat delta_rot = RotationBetween(rest_dir_m / rl, cur_dir_m / cl);
            const glm::quat old_global_rot = RotationFromMatrix(bg);
            const glm::quat new_global_rot = delta_rot * old_global_rot;

            const int pi = cache.parent_indices[bi];
            glm::quat parent_rot(1.0f, 0.0f, 0.0f, 0.0f);
            if (pi >= 0 && pi < static_cast<int>(bone_count)) {
                parent_rot = RotationFromMatrix(global[pi]);
            }
            const glm::quat new_local_rot = glm::normalize(glm::inverse(parent_rot) * new_global_rot);

            pb.rotations[bi] = new_local_rot;
            pb.touched[bi] = true;
        }
    }
}

} // namespace gameplay3d
} // namespace dse
