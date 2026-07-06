/**
 * @file dse_api_physics2d.cpp
 * @brief DSEngine Native C ABI — 2D Physics（Box2D 集成）
 *
 * 封装 RigidBody2D、BoxCollider2D、CircleCollider2D、PolygonCollider2D、Joint2D、Raycast2D、
 * 碰撞事件轮询、Tilemap 物理。语义与 Lua 绑定逐一等价。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/physics_2d.h"
#include "engine/ecs/tilemap.h"
#include "engine/core/service_locator.h"
#include "engine/physics/physics2d/physics2d_system.h"

#include <glm/glm.hpp>
#include <box2d/box2d.h>

using Entity = entt::entity;
using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }

void MarkRigidBodySyncDirty(World& world, Entity e) {
    if (auto* rb = world.registry().try_get<RigidBody2DComponent>(e)) {
        rb->sync_dirty_ = true;
    }
}

} // namespace

extern "C" void dse_physics2d_add_rigidbody(uint32_t e, int type, float gravity_scale, int fixed_rotation) {
    World* world = GW();
    if (!world) return;
    auto& rb = world->registry().emplace_or_replace<RigidBody2DComponent>(TE(e));
    if (type <= 0) rb.type = RigidBody2DType::Static;
    else if (type == 1) rb.type = RigidBody2DType::Kinematic;
    else rb.type = RigidBody2DType::Dynamic;
    rb.gravity_scale = gravity_scale;
    rb.fixed_rotation = (fixed_rotation != 0);
    rb.sync_dirty_ = true;
}

extern "C" void dse_physics2d_set_rigidbody_velocity(uint32_t e, float vx, float vy) {
    World* world = GW();
    if (!world) return;
    auto* rb = world->registry().try_get<RigidBody2DComponent>(TE(e));
    if (!rb) return;
    rb->velocity = glm::vec2(vx, vy);
    if (rb->runtime_body) {
        rb->runtime_body->SetLinearVelocity(b2Vec2{vx, vy});
        rb->runtime_body->SetAwake(true);
    }
}

extern "C" void dse_physics2d_add_box_collider(uint32_t e, float w, float h,
                                                float density, float friction, float restitution) {
    World* world = GW();
    if (!world) return;
    auto& col = world->registry().emplace_or_replace<BoxCollider2DComponent>(TE(e));
    col.size = glm::vec2(w, h);
    col.density = density;
    col.friction = friction;
    col.restitution = restitution;
    MarkRigidBodySyncDirty(*world, TE(e));
}

extern "C" void dse_physics2d_set_box_collider_trigger(uint32_t e, int is_trigger) {
    World* world = GW();
    if (!world) return;
    auto* col = world->registry().try_get<BoxCollider2DComponent>(TE(e));
    if (!col) return;
    col->is_trigger = (is_trigger != 0);
    if (col->runtime_fixture) col->runtime_fixture->SetSensor(is_trigger != 0);
    MarkRigidBodySyncDirty(*world, TE(e));
}

extern "C" void dse_physics2d_add_circle_collider(uint32_t e, float radius,
                                                   float density, float friction, float restitution) {
    World* world = GW();
    if (!world) return;
    auto& col = world->registry().emplace_or_replace<CircleCollider2DComponent>(TE(e));
    col.radius = radius;
    col.density = density;
    col.friction = friction;
    col.restitution = restitution;
    MarkRigidBodySyncDirty(*world, TE(e));
}

extern "C" void dse_physics2d_set_circle_collider_trigger(uint32_t e, int is_trigger) {
    World* world = GW();
    if (!world) return;
    auto* col = world->registry().try_get<CircleCollider2DComponent>(TE(e));
    if (!col) return;
    col->is_trigger = (is_trigger != 0);
    if (col->runtime_fixture) col->runtime_fixture->SetSensor(is_trigger != 0);
    MarkRigidBodySyncDirty(*world, TE(e));
}

extern "C" void dse_physics2d_add_polygon_collider(uint32_t e, const float* verts, int count,
                                                    float density, float friction, float restitution) {
    World* world = GW();
    if (!world) return;
    auto& pc = world->registry().emplace_or_replace<PolygonCollider2DComponent>(TE(e));
    pc.vertices.clear();
    pc.vertices.reserve(count > 0 ? count : 0);
    for (int i = 0; i < count && verts; ++i) {
        pc.vertices.push_back(glm::vec2(verts[i * 2], verts[i * 2 + 1]));
    }
    pc.density = density;
    pc.friction = friction;
    pc.restitution = restitution;
    MarkRigidBodySyncDirty(*world, TE(e));
}

extern "C" void dse_physics2d_set_polygon_collider_trigger(uint32_t e, int is_trigger) {
    World* world = GW();
    if (!world) return;
    auto* col = world->registry().try_get<PolygonCollider2DComponent>(TE(e));
    if (!col) return;
    col->is_trigger = (is_trigger != 0);
    if (col->runtime_fixture) col->runtime_fixture->SetSensor(is_trigger != 0);
    MarkRigidBodySyncDirty(*world, TE(e));
}

extern "C" void dse_physics2d_add_joint(uint32_t e, int type, uint32_t entity_a, uint32_t entity_b,
                                        float ax, float ay, float bx, float by, int collide_connected) {
    World* world = GW();
    if (!world) return;
    auto& jc = world->registry().emplace_or_replace<Joint2DComponent>(TE(e));
    jc.runtime_joint = nullptr;
    switch (type) {
        case 0: jc.type = Joint2DType::Revolute; break;
        case 1: jc.type = Joint2DType::Distance; break;
        case 2: jc.type = Joint2DType::Prismatic; break;
        case 3: jc.type = Joint2DType::Weld; break;
        default: jc.type = Joint2DType::Revolute; break;
    }
    jc.entity_a = TE(entity_a);
    jc.entity_b = TE(entity_b);
    jc.anchor_a = glm::vec2(ax, ay);
    jc.anchor_b = glm::vec2(bx, by);
    jc.collide_connected = (collide_connected != 0);
}

extern "C" void dse_physics2d_set_joint_revolute(uint32_t e, int enable_limit, float lower_deg, float upper_deg,
                                                 int enable_motor, float motor_speed, float max_torque) {
    World* world = GW();
    if (!world) return;
    auto* jc = world->registry().try_get<Joint2DComponent>(TE(e));
    if (!jc) return;
    jc->enable_limit = (enable_limit != 0);
    jc->lower_angle = lower_deg;
    jc->upper_angle = upper_deg;
    jc->enable_motor = (enable_motor != 0);
    jc->motor_speed = motor_speed;
    jc->max_motor_torque = max_torque;
}

extern "C" void dse_physics2d_set_joint_distance(uint32_t e, float min_len, float max_len,
                                                 float stiffness, float damping) {
    World* world = GW();
    if (!world) return;
    auto* jc = world->registry().try_get<Joint2DComponent>(TE(e));
    if (!jc) return;
    jc->min_length = min_len;
    jc->max_length = max_len;
    jc->stiffness = stiffness;
    jc->damping = damping;
}

extern "C" void dse_physics2d_set_joint_prismatic(uint32_t e, int enable_limit, float lower, float upper,
                                                  int enable_motor, float motor_speed, float max_force) {
    World* world = GW();
    if (!world) return;
    auto* jc = world->registry().try_get<Joint2DComponent>(TE(e));
    if (!jc) return;
    jc->enable_limit = (enable_limit != 0);
    jc->lower_angle = lower;
    jc->upper_angle = upper;
    jc->enable_motor = (enable_motor != 0);
    jc->motor_speed = motor_speed;
    jc->max_motor_torque = max_force;
}

extern "C" void dse_physics2d_destroy_joint(uint32_t e) {
    World* world = GW();
    if (!world) return;
    auto* jc = world->registry().try_get<Joint2DComponent>(TE(e));
    if (!jc) return;
    if (jc->runtime_joint) {
        // Joint destruction happens via Box2D world
        jc->runtime_joint = nullptr;
    }
    world->registry().remove<Joint2DComponent>(TE(e));
}

extern "C" int dse_physics2d_raycast(float sx, float sy, float ex, float ey,
                                    uint32_t* out_entity, float* out_point, float* out_normal) {
    auto* physics = dse::core::ServiceLocator::Instance().Get<Physics2DSystem>();
    if (!physics) return 0;

    glm::vec2 start(sx, sy);
    glm::vec2 end(ex, ey);
    Entity hit_entity = entt::null;
    glm::vec2 hit_point(0.0f), hit_normal(0.0f);

    if (!physics->Raycast(start, end, hit_entity, hit_point, hit_normal)) return 0;

    if (out_entity) *out_entity = static_cast<uint32_t>(static_cast<entt::id_type>(hit_entity));
    if (out_point) { out_point[0] = hit_point.x; out_point[1] = hit_point.y; }
    if (out_normal) { out_normal[0] = hit_normal.x; out_normal[1] = hit_normal.y; }
    return 1;
}

extern "C" int dse_physics2d_poll_collision_event(uint32_t e, uint32_t* out_other,
                                                   int* out_is_trigger, int* out_is_enter) {
    World* world = GW();
    if (!world) return 0;
    auto* rb = world->registry().try_get<RigidBody2DComponent>(TE(e));
    if (!rb || rb->pending_contact_events.empty()) return 0;

    const Physics2DContactEvent event = rb->pending_contact_events.front();
    rb->pending_contact_events.pop_front();
    if (out_other) *out_other = static_cast<uint32_t>(static_cast<entt::id_type>(event.other));
    if (out_is_trigger) *out_is_trigger = event.is_trigger ? 1 : 0;
    if (out_is_enter) *out_is_enter = event.is_enter ? 1 : 0;
    return 1;
}

extern "C" void dse_physics2d_add_tilemap(uint32_t e, float origin_x, float origin_y,
                                         float cell_w, float cell_h, int cols, int rows) {
    World* world = GW();
    if (!world) return;
    auto& tm = world->registry().emplace_or_replace<TilemapComponent>(TE(e));
    tm.origin = glm::vec2(origin_x, origin_y);
    tm.cell_size = glm::vec2(cell_w, cell_h);
    tm.cols = cols;
    tm.rows = rows;
    tm.tiles.resize(static_cast<size_t>(cols) * rows, 0);
}

extern "C" void dse_physics2d_set_tile(uint32_t e, int col, int row, int filled) {
    World* world = GW();
    if (!world) return;
    auto* tm = world->registry().try_get<TilemapComponent>(TE(e));
    if (!tm) return;
    if (col < 0 || col >= tm->cols || row < 0 || row >= tm->rows) return;
    tm->tiles[static_cast<size_t>(row) * tm->cols + col] = filled ? 1 : 0;
}
