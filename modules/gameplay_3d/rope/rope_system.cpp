#include "modules/gameplay_3d/rope/rope_system.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include <cmath>
#include <algorithm>

namespace dse {
namespace gameplay3d {

void RopeSystem::FixedUpdate(World& world, float dt) {
    auto view = world.registry().view<RopeComponent>();
    for (auto entity : view) {
        auto& rope = view.get<RopeComponent>(entity);
        if (!rope.enabled) continue;

        if (!rope.initialized) {
            InitializeRope(world, entity, rope);
        }
        if (rope.initialized) {
            Simulate(world, entity, rope, dt);
        }
    }
}

void RopeSystem::InitializeRope(World& world, entt::entity entity, RopeComponent& rope) {
    int count = rope.segment_count + 1; // 粒子数 = 段数 + 1
    rope.positions.resize(count);
    rope.prev_positions.resize(count);

    // 确定起始位置
    glm::vec3 start = rope.start_position;
    if (rope.anchor_entity_a != 0) {
        auto anchor_a = static_cast<entt::entity>(rope.anchor_entity_a);
        auto* t = world.registry().try_get<TransformComponent>(anchor_a);
        if (t) start = t->position + rope.anchor_offset_a;
    } else {
        auto* t = world.registry().try_get<TransformComponent>(entity);
        if (t) start = t->position + rope.anchor_offset_a;
    }

    // 沿 -Y 方向初始化（自然下垂初始状态）
    for (int i = 0; i < count; ++i) {
        rope.positions[i] = start + glm::vec3(0.0f, -rope.segment_length * i, 0.0f);
        rope.prev_positions[i] = rope.positions[i];
    }

    rope.initialized = true;
    DEBUG_LOG_INFO("[Rope] Initialized: {} particles, segment_len={}", count, rope.segment_length);
}

void RopeSystem::Simulate(World& world, entt::entity entity, RopeComponent& rope, float dt) {
    if (dt <= 0.0f) return;
    int count = static_cast<int>(rope.positions.size());
    if (count < 2) return;

    // 1. Verlet 积分
    glm::vec3 gravity(0.0f, -9.81f * rope.gravity_scale, 0.0f);
    for (int i = 0; i < count; ++i) {
        glm::vec3 current = rope.positions[i];
        glm::vec3 vel = (current - rope.prev_positions[i]) * rope.damping;
        rope.prev_positions[i] = current;

        if (rope.use_gravity) {
            vel += gravity * dt * dt;
        }
        rope.positions[i] = current + vel;
    }

    // 2. 锚点约束
    if (rope.anchor_entity_a != 0) {
        auto anchor_a = static_cast<entt::entity>(rope.anchor_entity_a);
        auto* t = world.registry().try_get<TransformComponent>(anchor_a);
        if (t) rope.positions[0] = t->position + rope.anchor_offset_a;
    } else {
        auto* t = world.registry().try_get<TransformComponent>(entity);
        if (t) rope.positions[0] = t->position + rope.anchor_offset_a;
    }

    if (rope.anchor_entity_b != 0) {
        auto anchor_b = static_cast<entt::entity>(rope.anchor_entity_b);
        auto* t = world.registry().try_get<TransformComponent>(anchor_b);
        if (t) rope.positions[count - 1] = t->position + rope.anchor_offset_b;
    }

    // 3. 距离约束求解
    for (int iter = 0; iter < rope.solver_iterations; ++iter) {
        for (int i = 0; i < count - 1; ++i) {
            glm::vec3 diff = rope.positions[i + 1] - rope.positions[i];
            float dist = glm::length(diff);
            if (dist < 1e-8f) continue;

            float error = dist - rope.segment_length;
            glm::vec3 correction = diff / dist * error * 0.5f;

            // 锚点（首尾）不移动
            bool fix_i = (i == 0 && (rope.anchor_entity_a != 0));
            bool fix_next = (i + 1 == count - 1 && (rope.anchor_entity_b != 0));

            if (fix_i && fix_next) {
                // 两端都固定，不修正
            } else if (fix_i) {
                rope.positions[i + 1] -= correction;
            } else if (fix_next) {
                rope.positions[i] += correction;
            } else {
                rope.positions[i] += correction;
                rope.positions[i + 1] -= correction;
            }
        }
    }

    // 4. 简单地面碰撞
    auto hm_view = world.registry().view<TerrainHeightmapComponent>();
    for (int i = 0; i < count; ++i) {
        float ground_y = 0.0f;
        for (auto te : hm_view) {
            const auto& hm = hm_view.get<TerrainHeightmapComponent>(te);
            float h = hm.GetHeight(rope.positions[i].x, rope.positions[i].z);
            if (h > ground_y) ground_y = h;
        }
        ground_y += rope.radius;
        if (rope.positions[i].y < ground_y) {
            rope.positions[i].y = ground_y;
        }
    }

    (void)entity; // entity already used above
}

} // namespace gameplay3d
} // namespace dse
