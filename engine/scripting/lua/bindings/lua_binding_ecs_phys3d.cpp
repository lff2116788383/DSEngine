/**
 * @file lua_binding_ecs_phys3d.cpp
 * @brief ECS Lua 绑定 — 3D 物理（RigidBody3D、碰撞体、Raycast）
 *
 * @note 文件名使用 phys3d 而非 physics3d，以规避 CMake 中
 *       DSE_ENABLE_PHYSX=OFF 时对 physics3d 模式的 GLOB 过滤。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/core/service_locator.h"
#include "engine/physics/physics3d/i_physics3d_system.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <algorithm>
#include <cmath>
#include <cfloat>

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

int L_EcsAddRigidBody3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int type = helper::OptInt(L, 2, 2);
    float mass = helper::OptFloat(L, 3, 1.0f);
    auto& rb = world->registry().emplace_or_replace<RigidBody3DComponent>(e);
    rb.type = static_cast<RigidBody3DType>(type);
    rb.mass = mass;
    return 0;
}

int L_EcsAddBoxCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float x = helper::CheckFloat(L, 2);
    float y = helper::CheckFloat(L, 3);
    float z = helper::CheckFloat(L, 4);
    auto& collider = world->registry().emplace_or_replace<BoxCollider3DComponent>(e);
    collider.size = glm::vec3(x, y, z);
    return 0;
}

int L_EcsAddSphereCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float radius = helper::CheckFloat(L, 2);
    auto& collider = world->registry().emplace_or_replace<SphereCollider3DComponent>(e);
    collider.radius = radius;
    return 0;
}

/// 对 3D 刚体施加持续的力（需 PhysX 后端）— 委托 dse_rigidbody3d_add_force
int L_EcsRigidBody3DAddForce(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float fx = helper::CheckFloat(L, 2);
    float fy = helper::CheckFloat(L, 3);
    float fz = helper::CheckFloat(L, 4);
    dse_rigidbody3d_add_force(EID(e), fx, fy, fz);
    return 0;
}

/// 对 3D 刚体施加瞬时冲量（需 PhysX 后端）— 委托 dse_rigidbody3d_add_impulse
int L_EcsRigidBody3DAddImpulse(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float ix = helper::CheckFloat(L, 2);
    float iy = helper::CheckFloat(L, 3);
    float iz = helper::CheckFloat(L, 4);
    dse_rigidbody3d_add_impulse(EID(e), ix, iy, iz);
    return 0;
}

/// 对 3D 刚体施加扭矩 — 委托 dse_rigidbody3d_add_torque
int L_EcsRigidBody3DAddTorque(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float tx = helper::CheckFloat(L, 2);
    float ty = helper::CheckFloat(L, 3);
    float tz = helper::CheckFloat(L, 4);
    dse_rigidbody3d_add_torque(EID(e), tx, ty, tz);
    return 0;
}

/// 设置 3D 刚体角速度 — 委托 dse_rigidbody3d_set_angular_velocity
int L_EcsRigidBody3DSetAngularVelocity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float ax = helper::CheckFloat(L, 2);
    float ay = helper::CheckFloat(L, 3);
    float az = helper::CheckFloat(L, 4);
    dse_rigidbody3d_set_angular_velocity(EID(e), ax, ay, az);
    return 0;
}

/// 获取 3D 刚体角速度，返回 ax,ay,az — 委托 dse_rigidbody3d_get_angular_velocity
int L_EcsRigidBody3DGetAngularVelocity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float v[3] = {0.0f, 0.0f, 0.0f};
    dse_rigidbody3d_get_angular_velocity(EID(e), v);
    helper::PushVec3(L, glm::vec3(v[0], v[1], v[2]));
    return 3;
}

/// 设置 3D 刚体线速度（需 PhysX 后端）— 委托 dse_rigidbody3d_set_velocity（含组件缓存同步）
int L_EcsRigidBody3DSetVelocity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float vx = helper::CheckFloat(L, 2);
    float vy = helper::CheckFloat(L, 3);
    float vz = helper::CheckFloat(L, 4);
    dse_rigidbody3d_set_velocity(EID(e), vx, vy, vz);
    return 0;
}

/// 获取 3D 刚体线速度（需 PhysX 后端），返回 vx,vy,vz — 委托 dse_rigidbody3d_get_velocity
int L_EcsRigidBody3DGetVelocity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float v[3] = {0.0f, 0.0f, 0.0f};
    dse_rigidbody3d_get_velocity(EID(e), v);
    helper::PushVec3(L, glm::vec3(v[0], v[1], v[2]));
    return 3;
}

/// 设置 3D 刚体是否受重力（需 PhysX 后端）— 委托 dse_rigidbody3d_set_gravity（含组件缓存同步）
int L_EcsRigidBody3DSetGravity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    bool enabled = helper::CheckBool(L, 2);
    dse_rigidbody3d_set_gravity(EID(e), enabled ? 1 : 0);
    return 0;
}

// ============================================================
// 3D Raycast — 委托 dse_physics3d_raycast（C ABI 封装 PhysX/Jolt 加速 + ECS 回退）
// raycast 核心实现已上移至 native_api/dse_api_physics3d.cpp，三端共享；
// 本 Lua 包装仅做参数读取 + 结果入栈，行为与原实现逐值等价。
// ============================================================

int L_Physics3DRaycast(lua_State* L) {
    glm::vec3 origin = helper::CheckVec3(L, 1);
    glm::vec3 direction = helper::CheckVec3(L, 4);
    float max_dist = helper::OptFloat(L, 7, 1000.0f);

    uint32_t hit_entity = 0;
    float point[3]  = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
    float distance  = 0.0f;
    const int hit = dse_physics3d_raycast(origin.x, origin.y, origin.z,
                                          direction.x, direction.y, direction.z,
                                          max_dist, &hit_entity, point, normal, &distance);

    lua_pushboolean(L, hit ? 1 : 0);
    if (!hit) return 1;
    helper::PushEntity(L, static_cast<Entity>(static_cast<entt::id_type>(hit_entity)));
    helper::PushVec3(L, glm::vec3(point[0], point[1], point[2]));
    helper::PushVec3(L, glm::vec3(normal[0], normal[1], normal[2]));
    helper::PushFloat(L, distance);
    return 9;
}

// ============================================================
// CharacterController3D 绑定
// ============================================================

/// 添加角色控制器组件：add_character_controller_3d(entity, radius, height, [slope_limit, step_offset])
int L_EcsAddCharacterController3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float radius = helper::OptFloat(L, 2, 0.3f);
    float height = helper::OptFloat(L, 3, 1.0f);
    float slope_limit = helper::OptFloat(L, 4, 45.0f);
    float step_offset = helper::OptFloat(L, 5, 0.3f);
    auto& cc = world->registry().emplace_or_replace<CharacterController3DComponent>(e);
    cc.radius = radius;
    cc.height = height;
    cc.slope_limit = slope_limit;
    cc.step_offset = step_offset;
    lua_pushboolean(L, 1);
    return 1;
}

/// 移动角色控制器：character_controller_3d_move(entity, dx, dy, dz, [min_dist, dt]) → is_grounded, vel_x, vel_y, vel_z, collision_flags
/// 委托 dse_character_controller3d_move（服务优先 + ECS 回退，逐值等价）
int L_EcsCharacterController3DMove(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float dx = helper::CheckFloat(L, 2);
    float dy = helper::CheckFloat(L, 3);
    float dz = helper::CheckFloat(L, 4);
    float min_dist = helper::OptFloat(L, 5, 0.0f);
    float dt = helper::OptFloat(L, 6, 1.0f / 60.0f);

    float vel[3] = {0.0f, 0.0f, 0.0f};
    uint32_t flags = 0;
    int grounded = dse_character_controller3d_move(EID(e), dx, dy, dz, min_dist, dt, vel, &flags);
    lua_pushboolean(L, grounded);
    helper::PushVec3(L, glm::vec3(vel[0], vel[1], vel[2]));
    lua_pushinteger(L, static_cast<lua_Integer>(flags));
    return 5;
}

/// 角色跳跃：character_controller_3d_jump(entity, jump_speed) → success — 委托 dse_character_controller3d_jump
int L_EcsCharacterController3DJump(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float jump_speed = helper::OptFloat(L, 2, 5.0f);
    lua_pushboolean(L, dse_character_controller3d_jump(EID(e), jump_speed));
    return 1;
}

/// 查询是否着地：character_controller_3d_is_grounded(entity) → is_grounded
int L_EcsCharacterController3DIsGrounded(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        lua_pushboolean(L, physics->IsCharacterGrounded(e) ? 1 : 0);
        return 1;
    }
#endif
    // 无 PhysX 回退：从组件缓存读取
    World* world = GetWorld();
    if (world) {
        const auto* cc = helper::TryGetComponentConst<CharacterController3DComponent>(*world, e);
        if (cc) {
            lua_pushboolean(L, cc->is_grounded ? 1 : 0);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

/// 获取角色位置：character_controller_3d_get_position(entity) → x, y, z
int L_EcsCharacterController3DGetPosition(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        glm::vec3 pos = physics->GetCharacterPosition(e);
        helper::PushVec3(L, pos);
        return 3;
    }
#endif
    // 回退到 Transform
    World* world = GetWorld();
    if (world) {
        const auto* transform = helper::TryGetComponentConst<TransformComponent>(*world, e);
        if (transform) {
            helper::PushVec3(L, transform->position);
            return 3;
        }
    }
    lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
    return 3;
}

// ============================================================
// TerrainHeightmap 绑定
// ============================================================

/// add_terrain_heightmap(entity, origin_x, origin_z, block_size, cols, rows, [scale, flip_z])
int L_EcsAddTerrainHeightmap(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float ox = helper::CheckFloat(L, 2);
    float oz = helper::CheckFloat(L, 3);
    float bs = helper::CheckFloat(L, 4);
    int cols = helper::CheckInt(L, 5);
    int rows = helper::CheckInt(L, 6);
    float sc = helper::OptFloat(L, 7, 1.0f);
    bool fz = helper::OptBool(L, 8, false);
    auto& hm = world->registry().emplace_or_replace<TerrainHeightmapComponent>(e);
    hm.origin_x = ox;
    hm.origin_z = oz;
    hm.block_size = bs;
    hm.cols = cols;
    hm.rows = rows;
    hm.scale = sc;
    hm.flip_z = fz;
    return 0;
}

/// terrain_heightmap_set_data(entity, table)
int L_EcsTerrainHeightmapSetData(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* hm = helper::TryGetComponent<TerrainHeightmapComponent>(*world, e);
    if (!hm) return 0;
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = static_cast<int>(lua_rawlen(L, 2));
    hm->heights.resize(n);
    for (int i = 0; i < n; ++i) {
        lua_rawgeti(L, 2, i + 1);
        hm->heights[i] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    return 0;
}

/// terrain_get_height(world_x, world_z) -> height_y
int L_EcsTerrainGetHeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnumber(L, 0.0); return 1; }
    float wx = helper::CheckFloat(L, 1);
    float wz = helper::CheckFloat(L, 2);
    float best_y = 0.0f;
    bool found = false;
    auto view = world->registry().view<TerrainHeightmapComponent>();
    for (auto te : view) {
        const auto& hm = view.get<TerrainHeightmapComponent>(te);
        float h = hm.GetHeight(wx, wz);
        if (!found || h > best_y) { best_y = h; found = true; }
    }
    helper::PushFloat(L, best_y);
    return 1;
}

// ============================================================
// 碰撞/触发事件查询（Task 1）
// ============================================================

/// physics_3d_get_collision_events() -> table of {type, entity_a, entity_b, px, py, pz, nx, ny, nz, impulse}
int L_Physics3DGetCollisionEvents(lua_State* L) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        const auto& events = physics->GetCollisionEvents();
        lua_newtable(L);
        for (size_t i = 0; i < events.size(); ++i) {
            const auto& e = events[i];
            lua_newtable(L);
            lua_pushinteger(L, static_cast<int>(e.type));
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint32_t>(e.entity_a)));
            lua_setfield(L, -2, "entity_a");
            lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint32_t>(e.entity_b)));
            lua_setfield(L, -2, "entity_b");
            lua_pushnumber(L, e.contact_point.x); lua_setfield(L, -2, "px");
            lua_pushnumber(L, e.contact_point.y); lua_setfield(L, -2, "py");
            lua_pushnumber(L, e.contact_point.z); lua_setfield(L, -2, "pz");
            lua_pushnumber(L, e.contact_normal.x); lua_setfield(L, -2, "nx");
            lua_pushnumber(L, e.contact_normal.y); lua_setfield(L, -2, "ny");
            lua_pushnumber(L, e.contact_normal.z); lua_setfield(L, -2, "nz");
            lua_pushnumber(L, e.impulse); lua_setfield(L, -2, "impulse");
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        return 1;
    }
#endif
    lua_newtable(L);
    return 1;
}

/// physics_3d_get_trigger_events() -> table of {type, trigger_entity, other_entity}
int L_Physics3DGetTriggerEvents(lua_State* L) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        const auto& events = physics->GetTriggerEvents();
        lua_newtable(L);
        for (size_t i = 0; i < events.size(); ++i) {
            const auto& e = events[i];
            lua_newtable(L);
            lua_pushinteger(L, static_cast<int>(e.type));
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint32_t>(e.trigger_entity)));
            lua_setfield(L, -2, "trigger_entity");
            lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint32_t>(e.other_entity)));
            lua_setfield(L, -2, "other_entity");
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        return 1;
    }
#endif
    lua_newtable(L);
    return 1;
}

// ============================================================
// MeshCollider3D 绑定（Task 3）
// ============================================================

/// add_mesh_collider_3d(entity, convex, [is_trigger])
int L_EcsAddMeshCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool convex = helper::OptBool(L, 2, false);
    bool is_trigger = helper::OptBool(L, 3, false);
    auto& mc = world->registry().emplace_or_replace<MeshCollider3DComponent>(e);
    mc.convex = convex;
    mc.is_trigger = is_trigger;
    return 0;
}

// ============================================================
// CapsuleCollider3D 绑定（Task 4）
// ============================================================

/// add_capsule_collider_3d(entity, radius, height, [direction, is_trigger])
int L_EcsAddCapsuleCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float radius = helper::CheckFloat(L, 2);
    float height = helper::CheckFloat(L, 3);
    int direction = helper::OptInt(L, 4, 1); // default Y-axis
    bool is_trigger = helper::OptBool(L, 5, false);
    auto& cap = world->registry().emplace_or_replace<CapsuleCollider3DComponent>(e);
    cap.radius = radius;
    cap.height = height;
    cap.direction = direction;
    cap.is_trigger = is_trigger;
    return 0;
}

// ============================================================
// Joint3D 绑定（Task 5）
// ============================================================

/// add_joint_3d(entity_a, entity_b_id, type, ax,ay,az, bx,by,bz, [break_force, break_torque])
int L_EcsAddJoint3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int connected_id = helper::CheckInt(L, 2);
    int type = helper::OptInt(L, 3, 0);
    float ax = helper::OptFloat(L, 4, 0.0f);
    float ay = helper::OptFloat(L, 5, 0.0f);
    float az = helper::OptFloat(L, 6, 0.0f);
    float bx = helper::OptFloat(L, 7, 0.0f);
    float by = helper::OptFloat(L, 8, 0.0f);
    float bz = helper::OptFloat(L, 9, 0.0f);
    float bf = helper::OptFloat(L, 10, FLT_MAX);
    float bt = helper::OptFloat(L, 11, FLT_MAX);

    auto& jc = world->registry().emplace_or_replace<Joint3DComponent>(e);
    jc.type = static_cast<Joint3DType>(type);
    jc.connected_entity_id = static_cast<uint32_t>(connected_id);
    jc.anchor = glm::vec3(ax, ay, az);
    jc.connected_anchor = glm::vec3(bx, by, bz);
    jc.break_force = bf;
    jc.break_torque = bt;
    return 0;
}

/// set_joint_3d_hinge_limits(entity, lower_deg, upper_deg)
int L_EcsSetJoint3DHingeLimits(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* jc = helper::TryGetComponent<Joint3DComponent>(*world, e);
    if (!jc) return 0;
    jc->use_limits = true;
    jc->lower_limit = helper::CheckFloat(L, 2);
    jc->upper_limit = helper::CheckFloat(L, 3);
    return 0;
}

/// set_joint_3d_spring(entity, stiffness, damping)
int L_EcsSetJoint3DSpring(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* jc = helper::TryGetComponent<Joint3DComponent>(*world, e);
    if (!jc) return 0;
    jc->spring_stiffness = helper::CheckFloat(L, 2);
    jc->spring_damping = helper::CheckFloat(L, 3);
    return 0;
}

/// set_joint_3d_distance(entity, min_dist, max_dist)
int L_EcsSetJoint3DDistance(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* jc = helper::TryGetComponent<Joint3DComponent>(*world, e);
    if (!jc) return 0;
    jc->min_distance = helper::CheckFloat(L, 2);
    jc->max_distance = helper::CheckFloat(L, 3);
    return 0;
}

/// is_joint_3d_broken(entity) -> bool
int L_EcsIsJoint3DBroken(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* jc = helper::TryGetComponentConst<Joint3DComponent>(*world, e);
    lua_pushboolean(L, (jc && jc->is_broken) ? 1 : 0);
    return 1;
}

// ============================================================
// 碰撞层设置（Task 6）
// ============================================================

/// set_collision_layer(entity, layer, mask)
int L_EcsSetCollisionLayer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int layer = helper::CheckInt(L, 2);
    int mask = helper::CheckInt(L, 3);

    auto* rb = helper::TryGetComponent<RigidBody3DComponent>(*world, e);
    if (!rb) return 0;
    rb->collision_layer = static_cast<uint16_t>(layer);
    rb->collision_mask = static_cast<uint16_t>(mask);

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetCollisionLayer(e, static_cast<uint16_t>(layer), static_cast<uint16_t>(mask));
    }
#endif
    return 0;
}

/// set_collider_trigger(entity, is_trigger) — 设置碰撞体 trigger 标志
int L_EcsSetColliderTrigger(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool trigger = helper::CheckBool(L, 2);
    if (auto* box = helper::TryGetComponent<BoxCollider3DComponent>(*world, e)) box->is_trigger = trigger;
    if (auto* sph = helper::TryGetComponent<SphereCollider3DComponent>(*world, e)) sph->is_trigger = trigger;
    if (auto* cap = helper::TryGetComponent<CapsuleCollider3DComponent>(*world, e)) cap->is_trigger = trigger;
    if (auto* mc = helper::TryGetComponent<MeshCollider3DComponent>(*world, e)) mc->is_trigger = trigger;
    return 0;
}

/// set_collider_material(entity, friction, bounciness)
int L_EcsSetColliderMaterial(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float friction = helper::CheckFloat(L, 2);
    float bounciness = helper::CheckFloat(L, 3);
    if (auto* box = helper::TryGetComponent<BoxCollider3DComponent>(*world, e)) {
        box->friction = friction; box->bounciness = bounciness;
    }
    if (auto* sph = helper::TryGetComponent<SphereCollider3DComponent>(*world, e)) {
        sph->friction = friction; sph->bounciness = bounciness;
    }
    if (auto* cap = helper::TryGetComponent<CapsuleCollider3DComponent>(*world, e)) {
        cap->friction = friction; cap->bounciness = bounciness;
    }
    if (auto* mc = helper::TryGetComponent<MeshCollider3DComponent>(*world, e)) {
        mc->friction = friction; mc->bounciness = bounciness;
    }
    return 0;
}

// ============================================================
// Overlap Query 绑定
// ============================================================

/// physics_3d_overlap_sphere(cx, cy, cz, radius) -> {entity1, entity2, ...}
int L_Physics3DOverlapSphere(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_newtable(L); return 1; }
    float cx = helper::CheckFloat(L, 1);
    float cy = helper::CheckFloat(L, 2);
    float cz = helper::CheckFloat(L, 3);
    float radius = helper::CheckFloat(L, 4);
    glm::vec3 center(cx, cy, cz);
    float r2 = radius * radius;

    lua_newtable(L);
    int idx = 1;

    auto box_view = world->registry().view<TransformComponent, BoxCollider3DComponent>();
    for (auto entity : box_view) {
        const auto& t = box_view.get<TransformComponent>(entity);
        const auto& c = box_view.get<BoxCollider3DComponent>(entity);
        glm::vec3 bc = t.position + c.center;
        glm::vec3 half = glm::abs(t.scale * c.size) * 0.5f;
        glm::vec3 bmin = bc - half, bmax = bc + half;
        glm::vec3 closest;
        closest.x = std::max(bmin.x, std::min(center.x, bmax.x));
        closest.y = std::max(bmin.y, std::min(center.y, bmax.y));
        closest.z = std::max(bmin.z, std::min(center.z, bmax.z));
        glm::vec3 diff = center - closest;
        if (glm::dot(diff, diff) <= r2) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, idx++);
        }
    }

    auto sph_view = world->registry().view<TransformComponent, SphereCollider3DComponent>();
    for (auto entity : sph_view) {
        const auto& t = sph_view.get<TransformComponent>(entity);
        const auto& c = sph_view.get<SphereCollider3DComponent>(entity);
        glm::vec3 sc = t.position + c.center;
        float max_s = std::max(std::fabs(t.scale.x),
                      std::max(std::fabs(t.scale.y), std::fabs(t.scale.z)));
        float sr = c.radius * max_s;
        float combined = radius + sr;
        glm::vec3 diff = center - sc;
        if (glm::dot(diff, diff) <= combined * combined) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, idx++);
        }
    }

    return 1;
}

/// physics_3d_overlap_box(min_x, min_y, min_z, max_x, max_y, max_z) -> {entity1, entity2, ...}
int L_Physics3DOverlapBox(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_newtable(L); return 1; }
    glm::vec3 qmin = helper::CheckVec3(L, 1);
    glm::vec3 qmax = helper::CheckVec3(L, 4);

    lua_newtable(L);
    int idx = 1;

    auto box_view = world->registry().view<TransformComponent, BoxCollider3DComponent>();
    for (auto entity : box_view) {
        const auto& t = box_view.get<TransformComponent>(entity);
        const auto& c = box_view.get<BoxCollider3DComponent>(entity);
        glm::vec3 bc = t.position + c.center;
        glm::vec3 half = glm::abs(t.scale * c.size) * 0.5f;
        glm::vec3 bmin = bc - half, bmax = bc + half;
        if (qmin.x <= bmax.x && qmax.x >= bmin.x &&
            qmin.y <= bmax.y && qmax.y >= bmin.y &&
            qmin.z <= bmax.z && qmax.z >= bmin.z) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, idx++);
        }
    }

    auto sph_view = world->registry().view<TransformComponent, SphereCollider3DComponent>();
    for (auto entity : sph_view) {
        const auto& t = sph_view.get<TransformComponent>(entity);
        const auto& c = sph_view.get<SphereCollider3DComponent>(entity);
        glm::vec3 sc = t.position + c.center;
        float max_s = std::max(std::fabs(t.scale.x),
                      std::max(std::fabs(t.scale.y), std::fabs(t.scale.z)));
        float sr = c.radius * max_s;
        glm::vec3 closest;
        closest.x = std::max(qmin.x, std::min(sc.x, qmax.x));
        closest.y = std::max(qmin.y, std::min(sc.y, qmax.y));
        closest.z = std::max(qmin.z, std::min(sc.z, qmax.z));
        glm::vec3 diff = sc - closest;
        if (glm::dot(diff, diff) <= sr * sr) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, idx++);
        }
    }

    return 1;
}

} // namespace

void RegisterEcsPhysics3DBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_rigidbody_3d",          L_EcsAddRigidBody3D},
        {"add_box_collider_3d",       L_EcsAddBoxCollider3D},
        {"add_sphere_collider_3d",    L_EcsAddSphereCollider3D},
        {"rigidbody_3d_add_force",            L_EcsRigidBody3DAddForce},
        {"rigidbody_3d_add_torque",           L_EcsRigidBody3DAddTorque},
        {"rigidbody_3d_add_impulse",          L_EcsRigidBody3DAddImpulse},
        {"rigidbody_3d_set_velocity",         L_EcsRigidBody3DSetVelocity},
        {"rigidbody_3d_get_velocity",         L_EcsRigidBody3DGetVelocity},
        {"rigidbody_3d_set_angular_velocity", L_EcsRigidBody3DSetAngularVelocity},
        {"rigidbody_3d_get_angular_velocity", L_EcsRigidBody3DGetAngularVelocity},
        {"rigidbody_3d_set_gravity",          L_EcsRigidBody3DSetGravity},
        {"physics_3d_raycast",        L_Physics3DRaycast},
        {"add_character_controller_3d",          L_EcsAddCharacterController3D},
        {"character_controller_3d_move",         L_EcsCharacterController3DMove},
        {"character_controller_3d_jump",         L_EcsCharacterController3DJump},
        {"character_controller_3d_is_grounded",  L_EcsCharacterController3DIsGrounded},
        {"character_controller_3d_get_position", L_EcsCharacterController3DGetPosition},
        {"add_terrain_heightmap",          L_EcsAddTerrainHeightmap},
        {"terrain_heightmap_set_data",     L_EcsTerrainHeightmapSetData},
        {"terrain_get_height",             L_EcsTerrainGetHeight},
        {"physics_3d_overlap_sphere",      L_Physics3DOverlapSphere},
        {"physics_3d_overlap_box",         L_Physics3DOverlapBox},
        // Task 1: Collision/Trigger events
        {"physics_3d_get_collision_events", L_Physics3DGetCollisionEvents},
        {"physics_3d_get_trigger_events",   L_Physics3DGetTriggerEvents},
        // Task 3: MeshCollider3D
        {"add_mesh_collider_3d",            L_EcsAddMeshCollider3D},
        // Task 4: CapsuleCollider3D
        {"add_capsule_collider_3d",         L_EcsAddCapsuleCollider3D},
        // Task 5: Joints
        {"add_joint_3d",                    L_EcsAddJoint3D},
        {"set_joint_3d_hinge_limits",       L_EcsSetJoint3DHingeLimits},
        {"set_joint_3d_spring",             L_EcsSetJoint3DSpring},
        {"set_joint_3d_distance",           L_EcsSetJoint3DDistance},
        {"is_joint_3d_broken",              L_EcsIsJoint3DBroken},
        // Task 6: Collision layers
        {"set_collision_layer",             L_EcsSetCollisionLayer},
        // Helpers
        {"set_collider_trigger",            L_EcsSetColliderTrigger},
        {"set_collider_material",           L_EcsSetColliderMaterial},
    });
}

} // namespace dse::runtime::lua_binding
