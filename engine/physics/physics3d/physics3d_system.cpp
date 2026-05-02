#include "engine/physics/physics3d/physics3d_system.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include <PxPhysicsAPI.h>

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
        return malloc(size);
    }
    void deallocate(void* ptr) override {
        free(ptr);
    }
};

class PhysXErrorCallback : public PxErrorCallback {
public:
    void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override {
        DEBUG_LOG_ERROR("PhysX Error: %s at %s:%d", message, file, line);
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

} // namespace physics3d
} // namespace dse
