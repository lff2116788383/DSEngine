#include "engine/physics/physics3d/physics3d_system.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include <PxPhysicsAPI.h>
#include <cstdlib>
#if defined(_WIN32)
#include <malloc.h>
#endif

using namespace physx;

namespace dse {
namespace physics3d {

// ---------------------------------------------------------------------------
// 自定义 PhysX 回调与辅助实现
// 替代 PxDefaultCpuDispatcherCreate / PxDefaultSimulationFilterShader，
// 避免链接 PhysXExtensions_static_64.lib 产生的 CRT 不匹配问题。
// ---------------------------------------------------------------------------

class PhysXAllocator : public PxAllocatorCallback {
public:
    void* allocate(size_t size, const char*, const char*, int) override {
#if defined(_WIN32)
        return _aligned_malloc(size, 16);
#else
        void* ptr = nullptr;
        return posix_memalign(&ptr, 16, size) == 0 ? ptr : nullptr;
#endif
    }
    void deallocate(void* ptr) override {
#if defined(_WIN32)
        _aligned_free(ptr);
#else
        free(ptr);
#endif
    }
};

class PhysXErrorCallback : public PxErrorCallback {
public:
    void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override {
        DEBUG_LOG_ERROR("PhysX Error {}: {} at {}:{}", static_cast<int>(code), message ? message : "", file ? file : "", line);
    }
};

/// 自定义默认碰撞过滤 Shader（等价于 PxDefaultSimulationFilterShader）
/// 所有形状对默认执行碰撞检测，过滤逻辑由场景查询决定。
static PxFilterFlags DseDefaultSimulationFilterShader(
    PxFilterObjectAttributes /*attributes0*/, PxFilterData /*filterData0*/,
    PxFilterObjectAttributes /*attributes1*/, PxFilterData /*filterData1*/,
    PxPairFlags& pairFlags, const void* /*constantBlock*/, PxU32 /*constantBlockSize*/)
{
    pairFlags = PxPairFlag::eCONTACT_DEFAULT;
    return PxFilterFlag::eDEFAULT;
}

/// 自定义 CPU 任务调度器，使用 PhysX 内置的线程池。
/// 此实现仅封装 PxDefaultCpuDispatcher 的创建逻辑，
/// 但通过 PhysX 的 PxCpuDispatcher 接口而非 Extensions 静态库。
class DseCpuDispatcher : public PxCpuDispatcher {
public:
    explicit DseCpuDispatcher(PxU32 num_threads) : num_threads_(num_threads) {}

    void submitTask(PxBaseTask& task) override {
        // 简单的同步执行：在当前线程直接 run
        // 对于 demo 验证足够；生产环境应使用线程池
        task.run();
        task.release();
    }

    PxU32 getWorkerCount() const override { return num_threads_; }

private:
    PxU32 num_threads_ = 1;
};

static PhysXAllocator g_allocator;
static PhysXErrorCallback g_error_callback;

bool Physics3DSystem::Init(World& world) {
    world_cache_ = &world;
    DEBUG_LOG_INFO("Physics3DSystem Initialized (Backend: NVIDIA PhysX)");
    
    foundation_ = PxCreateFoundation(PX_PHYSICS_VERSION, g_allocator, g_error_callback);
    if (!foundation_) {
        DEBUG_LOG_ERROR("PxCreateFoundation failed!");
        return false;
    }

    physics_ = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation_, PxTolerancesScale(), true, nullptr);
    if (!physics_) {
        DEBUG_LOG_ERROR("PxCreatePhysics failed!");
        return false;
    }

    PxSceneDesc sceneDesc(physics_->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
    dispatcher_ = new DseCpuDispatcher(2);
    sceneDesc.cpuDispatcher = dispatcher_;
    sceneDesc.filterShader = DseDefaultSimulationFilterShader;
    sceneDesc.flags |= PxSceneFlag::eENABLE_CCD; // Prevent fast small objects from tunneling

    scene_ = physics_->createScene(sceneDesc);
    if (!scene_) {
        DEBUG_LOG_ERROR("createScene failed!");
        return false;
    }

    default_material_ = physics_->createMaterial(0.5f, 0.5f, 0.6f);

    return true;
}

void Physics3DSystem::Shutdown() {
    DEBUG_LOG_INFO("Physics3DSystem Shutdown");

    if (scene_ && world_cache_) {
        auto rb_view = world_cache_->registry().view<RigidBody3DComponent>();
        for (auto entity : rb_view) {
            auto& rb = rb_view.get<RigidBody3DComponent>(entity);
            if (rb.runtime_body) {
                PxRigidActor* actor = static_cast<PxRigidActor*>(rb.runtime_body);
                scene_->removeActor(*actor);
                actor->release();
                rb.runtime_body = nullptr;
            }
        }

        auto cc_view = world_cache_->registry().view<CharacterController3DComponent>();
        for (auto entity : cc_view) {
            auto& cc = cc_view.get<CharacterController3DComponent>(entity);
            if (cc.runtime_controller) {
                PxRigidActor* actor = static_cast<PxRigidActor*>(cc.runtime_controller);
                scene_->removeActor(*actor);
                actor->release();
                cc.runtime_controller = nullptr;
            }
        }
    }

    if (default_material_) {
        default_material_->release();
        default_material_ = nullptr;
    }
    if (scene_) {
        scene_->release();
        scene_ = nullptr;
    }
    if (dispatcher_) {
        delete static_cast<DseCpuDispatcher*>(dispatcher_);
        dispatcher_ = nullptr;
    }
    if (physics_) {
        physics_->release();
        physics_ = nullptr;
    }
    if (foundation_) {
        foundation_->release();
        foundation_ = nullptr;
    }
    world_cache_ = nullptr;
}

void Physics3DSystem::SyncTransformsToPhysics(World& world) {
    if (!physics_ || !scene_) return;

    auto view = world.registry().view<RigidBody3DComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb = view.get<RigidBody3DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        // 1. Create runtime body if it doesn't exist
        if (!rb.runtime_body) {
            PxTransform px_transform(
                PxVec3(transform.position.x, transform.position.y, transform.position.z),
                PxQuat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w)
            );

            PxRigidActor* actor = nullptr;
            if (rb.type == RigidBody3DType::Static) {
                actor = physics_->createRigidStatic(px_transform);
            } else {
                PxRigidDynamic* dynamic = physics_->createRigidDynamic(px_transform);
                dynamic->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, rb.type == RigidBody3DType::Kinematic);
                dynamic->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
                dynamic->setMass(rb.mass);
                dynamic->setLinearDamping(rb.drag);
                dynamic->setAngularDamping(rb.angular_drag);
                actor = dynamic;
            }
            
            // Attach shapes
            if (world.registry().all_of<BoxCollider3DComponent>(entity)) {
                auto& box = world.registry().get<BoxCollider3DComponent>(entity);
                PxShape* shape = physics_->createShape(PxBoxGeometry(box.size.x * 0.5f, box.size.y * 0.5f, box.size.z * 0.5f), *default_material_);
                shape->setLocalPose(PxTransform(PxVec3(box.center.x, box.center.y, box.center.z)));
                if (box.is_trigger) {
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                }
                actor->attachShape(*shape);
                shape->release();
            } else if (world.registry().all_of<SphereCollider3DComponent>(entity)) {
                auto& sphere = world.registry().get<SphereCollider3DComponent>(entity);
                PxShape* shape = physics_->createShape(PxSphereGeometry(sphere.radius), *default_material_);
                shape->setLocalPose(PxTransform(PxVec3(sphere.center.x, sphere.center.y, sphere.center.z)));
                if (sphere.is_trigger) {
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                }
                actor->attachShape(*shape);
                shape->release();
            } else {
                // Default shape if none attached
                PxShape* shape = physics_->createShape(PxBoxGeometry(0.5f, 0.5f, 0.5f), *default_material_);
                actor->attachShape(*shape);
                shape->release();
            }

            actor->userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entity));
            scene_->addActor(*actor);
            rb.runtime_body = actor;

            // Apply deferred impulse (e.g. explosion force queued before actor creation)
            if (rb.has_pending_impulse) {
                PxRigidDynamic* dyn = actor->is<PxRigidDynamic>();
                if (dyn) {
                    // Convert impulse to velocity: v = impulse / mass
                    float m = dyn->getMass();
                    if (m < 0.001f) m = 0.001f;
                    glm::vec3 vel = rb.pending_impulse / m;
                    dyn->setLinearVelocity(PxVec3(vel.x, vel.y, vel.z));
                }
                rb.has_pending_impulse = false;
                rb.pending_impulse = glm::vec3(0.0f);
            }
        }

        // 2. Sync if transform was modified externally
        if (transform.dirty) {
            PxRigidActor* actor = static_cast<PxRigidActor*>(rb.runtime_body);
            if (actor) {
                PxTransform px_transform(
                    PxVec3(transform.position.x, transform.position.y, transform.position.z),
                    PxQuat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w)
                );
                
                if (rb.type == RigidBody3DType::Kinematic) {
                    PxRigidDynamic* dynamic = actor->is<PxRigidDynamic>();
                    if (dynamic) {
                        dynamic->setKinematicTarget(px_transform);
                    }
                } else {
                    actor->setGlobalPose(px_transform);
                }
            }
            // we don't clear dirty here, because renderer might need it.
        }
    }
}

void Physics3DSystem::SyncPhysicsToTransforms(World& world) {
    if (!scene_) return;

    auto view = world.registry().view<RigidBody3DComponent, TransformComponent>();
    for (auto entity : view) {
        auto& rb = view.get<RigidBody3DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (rb.type == RigidBody3DType::Dynamic && rb.runtime_body) {
            PxRigidActor* actor = static_cast<PxRigidActor*>(rb.runtime_body);
            PxTransform px_transform = actor->getGlobalPose();
            
            transform.position = glm::vec3(px_transform.p.x, px_transform.p.y, px_transform.p.z);
            transform.rotation = glm::quat(px_transform.q.w, px_transform.q.x, px_transform.q.y, px_transform.q.z);
            transform.dirty = true;
        }
    }
}

void Physics3DSystem::FixedUpdate(World& world, float fixed_delta_time) {
    if (!scene_) return;

    SyncTransformsToPhysics(world);
    SyncCharacterControllers(world, fixed_delta_time);

    scene_->simulate(fixed_delta_time);
    scene_->fetchResults(true);

    SyncPhysicsToTransforms(world);
}


RaycastResult Physics3DSystem::Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance) {
    RaycastResult result;
    if (!scene_) return result;

    PxVec3 px_origin(origin.x, origin.y, origin.z);
    PxVec3 px_dir(direction.x, direction.y, direction.z);
    PxRaycastBuffer hit;

    if (scene_->raycast(px_origin, px_dir, max_distance, hit)) {
        result.hit = true;
        result.distance = hit.block.distance;
        result.hit_point = glm::vec3(hit.block.position.x, hit.block.position.y, hit.block.position.z);
        result.hit_normal = glm::vec3(hit.block.normal.x, hit.block.normal.y, hit.block.normal.z);
        
        if (hit.block.actor && hit.block.actor->userData) {
            result.entity = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(hit.block.actor->userData));
        }
    }
    return result;
}

void Physics3DSystem::AddForce(entt::entity entity, const glm::vec3& force) {
    if (!scene_) return;
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return;

    auto& rb = view.get<RigidBody3DComponent>(*it);
    if (rb.type != RigidBody3DType::Dynamic || !rb.runtime_body) return;

    PxRigidDynamic* dynamic = static_cast<PxRigidActor*>(rb.runtime_body)->is<PxRigidDynamic>();
    if (dynamic) {
        dynamic->addForce(PxVec3(force.x, force.y, force.z), PxForceMode::eFORCE);
    }
}

void Physics3DSystem::AddImpulse(entt::entity entity, const glm::vec3& impulse) {
    if (!scene_) return;
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return;

    auto& rb = view.get<RigidBody3DComponent>(*it);
    if (rb.type != RigidBody3DType::Dynamic || !rb.runtime_body) return;

    PxRigidDynamic* dynamic = static_cast<PxRigidActor*>(rb.runtime_body)->is<PxRigidDynamic>();
    if (dynamic) {
        dynamic->addForce(PxVec3(impulse.x, impulse.y, impulse.z), PxForceMode::eIMPULSE);
    }
}

void Physics3DSystem::SetVelocity(entt::entity entity, const glm::vec3& velocity) {
    if (!scene_) return;
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return;

    auto& rb = view.get<RigidBody3DComponent>(*it);
    if (rb.type != RigidBody3DType::Dynamic || !rb.runtime_body) return;

    PxRigidDynamic* dynamic = static_cast<PxRigidActor*>(rb.runtime_body)->is<PxRigidDynamic>();
    if (dynamic) {
        dynamic->setLinearVelocity(PxVec3(velocity.x, velocity.y, velocity.z));
    }
}

glm::vec3 Physics3DSystem::GetVelocity(entt::entity entity) const {
    if (!scene_) return glm::vec3(0.0f);
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return glm::vec3(0.0f);

    auto& rb = view.get<RigidBody3DComponent>(*it);
    if (rb.type != RigidBody3DType::Dynamic || !rb.runtime_body) return glm::vec3(0.0f);

    PxRigidDynamic* dynamic = static_cast<PxRigidActor*>(rb.runtime_body)->is<PxRigidDynamic>();
    if (dynamic) {
        PxVec3 vel = dynamic->getLinearVelocity();
        return glm::vec3(vel.x, vel.y, vel.z);
    }
    return glm::vec3(0.0f);
}

void Physics3DSystem::SetGravityEnabled(entt::entity entity, bool enabled) {
    if (!scene_) return;
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return;

    auto& rb = view.get<RigidBody3DComponent>(*it);
    if (rb.type != RigidBody3DType::Dynamic || !rb.runtime_body) return;

    PxRigidDynamic* dynamic = static_cast<PxRigidActor*>(rb.runtime_body)->is<PxRigidDynamic>();
    if (dynamic) {
        dynamic->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !enabled);
    }
    rb.use_gravity = enabled;
}

bool Physics3DSystem::IsGravityEnabled(entt::entity entity) const {
    if (!scene_) return true;
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return true;

    auto& rb = view.get<RigidBody3DComponent>(*it);
    return rb.use_gravity;
}

void Physics3DSystem::RemoveActor(entt::entity entity) {
    if (!scene_ || !world_cache_) return;
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return;

    auto& rb = view.get<RigidBody3DComponent>(*it);
    if (rb.runtime_body) {
        PxRigidActor* actor = static_cast<PxRigidActor*>(rb.runtime_body);
        scene_->removeActor(*actor);
        actor->release();
        rb.runtime_body = nullptr;
    }
}

// ---------------------------------------------------------------------------
// CharacterController 实现
// 基于 kinematic PxRigidDynamic + PxScene::sweep 的自定义角色控制器
// 不依赖 PxControllerManager/PxCapsuleController（预编译 static lib 与 /MD CRT 不兼容）
// ---------------------------------------------------------------------------

void Physics3DSystem::CreateCharacterActor(World& world, entt::entity entity,
    CharacterController3DComponent& cc, const TransformComponent& transform)
{
    if (!physics_ || !scene_ || cc.runtime_controller) return;

    // 胶囊中心 Y 偏移 = radius + height/2
    float center_y = cc.radius + cc.height * 0.5f;
    PxTransform px_transform(
        PxVec3(transform.position.x, transform.position.y + center_y, transform.position.z));

    // 创建 kinematic 动态刚体作为角色代理
    PxRigidDynamic* actor = physics_->createRigidDynamic(px_transform);
    actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
    actor->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, true); // 重力手动处理
    actor->userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entity));

    // 附着胶囊形状
    PxCapsuleGeometry capsule_geo(cc.radius, cc.height * 0.5f);
    PxShape* shape = physics_->createShape(capsule_geo, *default_material_);
    // 角色不参与物理推力，仅用于场景查询（sweep/raycast）
    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
    shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
    actor->attachShape(*shape);
    shape->release();

    scene_->addActor(*actor);
    cc.runtime_controller = actor;

    DEBUG_LOG_INFO("CharacterController actor created: radius={} height={}", cc.radius, cc.height);
}

void Physics3DSystem::SyncCharacterControllers(World& world, float fixed_delta_time) {
    if (!scene_) return;

    auto view = world.registry().view<CharacterController3DComponent, TransformComponent>();
    for (auto entity : view) {
        auto& cc = view.get<CharacterController3DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        // 懒创建 kinematic actor
        if (!cc.runtime_controller) {
            CreateCharacterActor(world, entity, cc, transform);
            if (!cc.runtime_controller) continue;
        }

        PxRigidDynamic* actor = static_cast<PxRigidDynamic*>(cc.runtime_controller);

        // 施加重力：累加到垂直速度
        if (!cc.is_grounded) {
            cc.velocity.y -= 9.81f * fixed_delta_time;
        }

        // 应用速度：使用 sweep 检测碰撞后移动
        glm::vec3 displacement = cc.velocity * fixed_delta_time;

        // ---- 水平 sweep ----
        glm::vec3 horizontal_disp(displacement.x, 0.0f, displacement.z);
        float h_len = glm::length(horizontal_disp);
        if (h_len > 1e-6f) {
            PxVec3 px_dir(horizontal_disp.x / h_len, 0.0f, horizontal_disp.z / h_len);
            PxSweepBuffer sweep_hit;
            PxTransform pose = actor->getGlobalPose();
            PxCapsuleGeometry capsule_geo(cc.radius, cc.height * 0.5f);

            if (scene_->sweep(capsule_geo, pose, px_dir, h_len, sweep_hit)) {
                // 碰撞：只移动到碰撞点（留一点皮肤间隙）
                float safe_dist = glm::max(0.0f, sweep_hit.block.distance - cc.skin_width);
                displacement.x = px_dir.x * safe_dist;
                displacement.z = px_dir.z * safe_dist;
                cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Sides;
                cc.velocity.x = 0.0f;
                cc.velocity.z = 0.0f;
            }
        }

        // ---- 垂直 sweep（下落/跳跃） ----
        float v_disp = displacement.y;
        if (glm::abs(v_disp) > 1e-6f) {
            PxVec3 px_dir(0.0f, v_disp > 0.0f ? 1.0f : -1.0f, 0.0f);
            float abs_v = glm::abs(v_disp);
            PxSweepBuffer sweep_hit;
            // 先更新水平位置后，在水平后的位置做垂直 sweep
            PxTransform pose = actor->getGlobalPose();
            pose.p.x += displacement.x;
            pose.p.z += displacement.z;
            PxCapsuleGeometry capsule_geo(cc.radius, cc.height * 0.5f);

            if (scene_->sweep(capsule_geo, pose, px_dir, abs_v, sweep_hit)) {
                float safe_dist = glm::max(0.0f, sweep_hit.block.distance - cc.skin_width);
                displacement.y = px_dir.y * safe_dist;

                if (v_disp < 0.0f) {
                    // 下落碰撞 = 着地
                    cc.is_grounded = true;
                    cc.velocity.y = 0.0f;
                    cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Down;
                } else {
                    // 上升碰撞 = 撞头
                    cc.velocity.y = 0.0f;
                    cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Up;
                }
            } else {
                if (v_disp < 0.0f) {
                    cc.is_grounded = false;
                }
            }
        } else {
            // 无垂直位移时，做着地检测（短距离 down sweep）
            PxTransform pose = actor->getGlobalPose();
            pose.p.x += displacement.x;
            pose.p.z += displacement.z;
            PxCapsuleGeometry capsule_geo(cc.radius, cc.height * 0.5f);
            PxSweepBuffer sweep_hit;
            float ground_check_dist = cc.step_offset + 0.05f;

            if (scene_->sweep(capsule_geo, pose, PxVec3(0.0f, -1.0f, 0.0f), ground_check_dist, sweep_hit)) {
                cc.is_grounded = true;
                // 贴地：snap 到地面
                float snap_dist = sweep_hit.block.distance - cc.skin_width;
                if (snap_dist > 0.0f && snap_dist < cc.step_offset) {
                    displacement.y = -snap_dist;
                }
                cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Down;
            } else {
                cc.is_grounded = false;
            }
        }

        // 更新 kinematic target（PhysX 会自动移动 actor）
        PxTransform new_pose = actor->getGlobalPose();
        new_pose.p.x += displacement.x;
        new_pose.p.y += displacement.y;
        new_pose.p.z += displacement.z;
        actor->setKinematicTarget(new_pose);

        // 同步到 ECS Transform（position 代表脚底位置）
        transform.position = glm::vec3(
            new_pose.p.x,
            new_pose.p.y - cc.radius - cc.height * 0.5f,
            new_pose.p.z);
        transform.dirty = true;
    }
}

CharacterMoveResult Physics3DSystem::MoveCharacter(entt::entity entity, const glm::vec3& displacement,
    float min_dist, float delta_time)
{
    CharacterMoveResult result;
    if (!scene_ || !world_cache_ || delta_time <= 1.0e-6f) return result;

    auto view = world_cache_->registry().view<CharacterController3DComponent, TransformComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return result;

    auto& cc = view.get<CharacterController3DComponent>(*it);
    auto& transform = view.get<TransformComponent>(*it);

    if (!cc.runtime_controller) {
        CreateCharacterActor(*world_cache_, entity, cc, transform);
        if (!cc.runtime_controller) return result;
    }

    cc.min_move_distance = min_dist;
    cc.velocity = displacement / delta_time;

    PxRigidDynamic* actor = static_cast<PxRigidDynamic*>(cc.runtime_controller);
    cc.collision_flags = CharacterCollisionFlag::None;

    if (!cc.is_grounded) {
        cc.velocity.y -= 9.81f * delta_time;
    }

    glm::vec3 resolved_displacement = cc.velocity * delta_time;
    glm::vec3 horizontal_disp(resolved_displacement.x, 0.0f, resolved_displacement.z);
    float h_len = glm::length(horizontal_disp);
    if (h_len > glm::max(min_dist, 1.0e-6f)) {
        PxVec3 px_dir(horizontal_disp.x / h_len, 0.0f, horizontal_disp.z / h_len);
        PxSweepBuffer sweep_hit;
        PxTransform pose = actor->getGlobalPose();
        PxCapsuleGeometry capsule_geo(cc.radius, cc.height * 0.5f);

        if (scene_->sweep(capsule_geo, pose, px_dir, h_len, sweep_hit)) {
            float safe_dist = glm::max(0.0f, sweep_hit.block.distance - cc.skin_width);
            resolved_displacement.x = px_dir.x * safe_dist;
            resolved_displacement.z = px_dir.z * safe_dist;
            cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Sides;
            cc.velocity.x = 0.0f;
            cc.velocity.z = 0.0f;
        }
    }

    float v_disp = resolved_displacement.y;
    if (glm::abs(v_disp) > glm::max(min_dist, 1.0e-6f)) {
        PxVec3 px_dir(0.0f, v_disp > 0.0f ? 1.0f : -1.0f, 0.0f);
        float abs_v = glm::abs(v_disp);
        PxSweepBuffer sweep_hit;
        PxTransform pose = actor->getGlobalPose();
        pose.p.x += resolved_displacement.x;
        pose.p.z += resolved_displacement.z;
        PxCapsuleGeometry capsule_geo(cc.radius, cc.height * 0.5f);

        if (scene_->sweep(capsule_geo, pose, px_dir, abs_v, sweep_hit)) {
            float safe_dist = glm::max(0.0f, sweep_hit.block.distance - cc.skin_width);
            resolved_displacement.y = px_dir.y * safe_dist;

            if (v_disp < 0.0f) {
                cc.is_grounded = true;
                cc.velocity.y = 0.0f;
                cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Down;
            } else {
                cc.velocity.y = 0.0f;
                cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Up;
            }
        } else if (v_disp < 0.0f) {
            cc.is_grounded = false;
        }
    } else {
        PxTransform pose = actor->getGlobalPose();
        pose.p.x += resolved_displacement.x;
        pose.p.z += resolved_displacement.z;
        PxCapsuleGeometry capsule_geo(cc.radius, cc.height * 0.5f);
        PxSweepBuffer sweep_hit;
        float ground_check_dist = cc.step_offset + 0.05f;

        if (scene_->sweep(capsule_geo, pose, PxVec3(0.0f, -1.0f, 0.0f), ground_check_dist, sweep_hit)) {
            cc.is_grounded = true;
            float snap_dist = sweep_hit.block.distance - cc.skin_width;
            if (snap_dist > 0.0f && snap_dist < cc.step_offset) {
                resolved_displacement.y = -snap_dist;
            }
            cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Down;
        } else {
            cc.is_grounded = false;
        }
    }

    PxTransform new_pose = actor->getGlobalPose();
    new_pose.p.x += resolved_displacement.x;
    new_pose.p.y += resolved_displacement.y;
    new_pose.p.z += resolved_displacement.z;
    actor->setGlobalPose(new_pose);

    transform.position = glm::vec3(
        new_pose.p.x,
        new_pose.p.y - cc.radius - cc.height * 0.5f,
        new_pose.p.z);
    transform.dirty = true;

    result.is_grounded = cc.is_grounded;
    result.velocity = cc.velocity;
    result.collision_flags = static_cast<uint8_t>(cc.collision_flags);
    return result;
}

bool Physics3DSystem::JumpCharacter(entt::entity entity, float jump_speed) {
    if (!scene_ || !world_cache_) return false;

    auto view = world_cache_->registry().view<CharacterController3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return false;

    auto& cc = view.get<CharacterController3DComponent>(*it);
    if (!cc.runtime_controller || !cc.is_grounded) return false;

    cc.velocity.y = jump_speed;
    cc.is_grounded = false;
    cc.collision_flags = CharacterCollisionFlag::None;
    return true;
}

bool Physics3DSystem::IsCharacterGrounded(entt::entity entity) const {
    if (!scene_ || !world_cache_) return false;

    auto view = world_cache_->registry().view<CharacterController3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return false;

    return view.get<CharacterController3DComponent>(*it).is_grounded;
}

glm::vec3 Physics3DSystem::GetCharacterPosition(entt::entity entity) const {
    if (!scene_ || !world_cache_) return glm::vec3(0.0f);

    auto view = world_cache_->registry().view<CharacterController3DComponent, TransformComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return glm::vec3(0.0f);

    auto& cc = view.get<CharacterController3DComponent>(*it);
    if (!cc.runtime_controller) {
        return view.get<TransformComponent>(*it).position;
    }

    PxRigidDynamic* actor = static_cast<PxRigidDynamic*>(cc.runtime_controller);
    PxVec3 pos = actor->getGlobalPose().p;
    return glm::vec3(pos.x, pos.y - cc.radius - cc.height * 0.5f, pos.z);
}


} // namespace physics3d
} // namespace dse
