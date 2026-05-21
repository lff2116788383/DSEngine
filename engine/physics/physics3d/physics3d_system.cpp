#include "engine/physics/physics3d/physics3d_system.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include <PxPhysicsAPI.h>
#include <cooking/PxCooking.h>
// PhysXExtensions: 已从源码编译 /MD 版本
#define DSE_HAS_PHYSX_EXTENSIONS 1
#ifdef DSE_HAS_PHYSX_EXTENSIONS
#include <extensions/PxFixedJoint.h>
#include <extensions/PxRevoluteJoint.h>
#include <extensions/PxDistanceJoint.h>
#include <extensions/PxD6Joint.h>
#endif
#include <cstdlib>
#include <cmath>
#include <cfloat>

namespace { constexpr float kPi = 3.14159265358979323846f; }
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

// ---------------------------------------------------------------------------
// Simulation Event Callback（Task 1）
// ---------------------------------------------------------------------------

class DseSimulationEventCallback : public PxSimulationEventCallback {
public:
    std::vector<CollisionEvent> collision_events;
    std::vector<TriggerEvent> trigger_events;

    void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override {
        for (PxU32 i = 0; i < nbPairs; ++i) {
            const PxContactPair& cp = pairs[i];
            CollisionEvent evt;

            if (pairHeader.actors[0] && pairHeader.actors[0]->userData)
                evt.entity_a = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(pairHeader.actors[0]->userData));
            if (pairHeader.actors[1] && pairHeader.actors[1]->userData)
                evt.entity_b = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(pairHeader.actors[1]->userData));

            if (cp.events & PxPairFlag::eNOTIFY_TOUCH_FOUND)
                evt.type = CollisionEvent::Type::Enter;
            else if (cp.events & PxPairFlag::eNOTIFY_TOUCH_PERSISTS)
                evt.type = CollisionEvent::Type::Stay;
            else if (cp.events & PxPairFlag::eNOTIFY_TOUCH_LOST)
                evt.type = CollisionEvent::Type::Exit;
            else
                continue;

            // Extract contact point
            if (cp.contactCount > 0) {
                PxContactPairPoint points[1];
                PxU32 n = cp.extractContacts(points, 1);
                if (n > 0) {
                    evt.contact_point = glm::vec3(points[0].position.x, points[0].position.y, points[0].position.z);
                    evt.contact_normal = glm::vec3(points[0].normal.x, points[0].normal.y, points[0].normal.z);
                    evt.impulse = glm::length(glm::vec3(points[0].impulse.x, points[0].impulse.y, points[0].impulse.z));
                }
            }
            collision_events.push_back(evt);
        }
    }

    void onTrigger(PxTriggerPair* pairs, PxU32 count) override {
        for (PxU32 i = 0; i < count; ++i) {
            const PxTriggerPair& tp = pairs[i];
            if (tp.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
                continue;

            TriggerEvent evt;
            if (tp.triggerActor && tp.triggerActor->userData)
                evt.trigger_entity = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(tp.triggerActor->userData));
            if (tp.otherActor && tp.otherActor->userData)
                evt.other_entity = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(tp.otherActor->userData));

            evt.type = (tp.status == PxPairFlag::eNOTIFY_TOUCH_FOUND) ? TriggerEvent::Type::Enter : TriggerEvent::Type::Exit;
            trigger_events.push_back(evt);
        }
    }

    void onConstraintBreak(PxConstraintInfo* /*constraints*/, PxU32 /*count*/) override {}
    void onWake(PxActor** /*actors*/, PxU32 /*count*/) override {}
    void onSleep(PxActor** /*actors*/, PxU32 /*count*/) override {}
    void onAdvance(const PxRigidBody* const* /*bodyBuffer*/, const PxTransform* /*poseBuffer*/, const PxU32 /*count*/) override {}
};

/// 碰撞过滤 Shader（Task 1 + Task 6）
/// 支持 collision_layer/collision_mask 过滤，启用 contact/trigger 通知
static PxFilterFlags DseDefaultSimulationFilterShader(
    PxFilterObjectAttributes attributes0, PxFilterData filterData0,
    PxFilterObjectAttributes attributes1, PxFilterData filterData1,
    PxPairFlags& pairFlags, const void* /*constantBlock*/, PxU32 /*constantBlockSize*/)
{
    // filterData.word0 = collision_layer, filterData.word1 = collision_mask
    // Check layer/mask bidirectional
    if ((filterData0.word0 & filterData1.word1) == 0 &&
        (filterData1.word0 & filterData0.word1) == 0) {
        return PxFilterFlag::eSUPPRESS;
    }

    // Trigger handling
    if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1)) {
        pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
        return PxFilterFlag::eDEFAULT;
    }

    // Contact handling with notifications
    pairFlags = PxPairFlag::eCONTACT_DEFAULT
              | PxPairFlag::eNOTIFY_TOUCH_FOUND
              | PxPairFlag::eNOTIFY_TOUCH_PERSISTS
              | PxPairFlag::eNOTIFY_TOUCH_LOST
              | PxPairFlag::eNOTIFY_CONTACT_POINTS;
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

    // Simulation event callback (Task 1)
    auto* callback = new DseSimulationEventCallback();
    simulation_callback_ = callback;

    PxSceneDesc sceneDesc(physics_->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
    dispatcher_ = new DseCpuDispatcher(2);
    sceneDesc.cpuDispatcher = dispatcher_;
    sceneDesc.filterShader = DseDefaultSimulationFilterShader;
    sceneDesc.simulationEventCallback = callback;
    sceneDesc.flags |= PxSceneFlag::eENABLE_CCD; // Prevent fast small objects from tunneling

    scene_ = physics_->createScene(sceneDesc);
    if (!scene_) {
        DEBUG_LOG_ERROR("createScene failed!");
        return false;
    }

    default_material_ = physics_->createMaterial(0.5f, 0.5f, 0.6f);

    // Cooking (Task 3 — MeshCollider)
    PxCookingParams cookParams(physics_->getTolerancesScale());
    cooking_ = PxCreateCooking(PX_PHYSICS_VERSION, *foundation_, cookParams);
    if (!cooking_) {
        DEBUG_LOG_ERROR("PxCreateCooking failed!");
    }

    return true;
}

void Physics3DSystem::Shutdown() {
    DEBUG_LOG_INFO("Physics3DSystem Shutdown");

    if (scene_ && world_cache_) {
        // Release joints (Task 5)
        auto joint_view = world_cache_->registry().view<Joint3DComponent>();
        for (auto entity : joint_view) {
            auto& jc = joint_view.get<Joint3DComponent>(entity);
            if (jc.runtime_joint) {
                static_cast<PxBase*>(jc.runtime_joint)->release();
                jc.runtime_joint = nullptr;
            }
        }

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

    // Release material cache (Task 2)
    for (auto& [key, mat] : material_cache_) {
        if (mat) mat->release();
    }
    material_cache_.clear();

    // Release mesh caches (Task 3)
    for (auto& [key, mesh] : convex_mesh_cache_) {
        if (mesh) mesh->release();
    }
    convex_mesh_cache_.clear();
    for (auto& [key, mesh] : triangle_mesh_cache_) {
        if (mesh) mesh->release();
    }
    triangle_mesh_cache_.clear();

    // Release cooking (Task 3)
    if (cooking_) {
        cooking_->release();
        cooking_ = nullptr;
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
    // Release simulation callback (Task 1)
    if (simulation_callback_) {
        delete static_cast<DseSimulationEventCallback*>(simulation_callback_);
        simulation_callback_ = nullptr;
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

// ---------------------------------------------------------------------------
// 材质缓存（Task 2）
// ---------------------------------------------------------------------------

PxMaterial* Physics3DSystem::GetOrCreateMaterial(float friction, float bounciness) {
    // Use friction for both static and dynamic friction
    MaterialKey key{friction, friction, bounciness};
    auto it = material_cache_.find(key);
    if (it != material_cache_.end()) return it->second;

    PxMaterial* mat = physics_->createMaterial(friction, friction, bounciness);
    material_cache_[key] = mat;
    return mat;
}

// ---------------------------------------------------------------------------
// 碰撞层设置（Task 6）
// ---------------------------------------------------------------------------

void Physics3DSystem::SetCollisionLayer(entt::entity entity, uint16_t layer, uint16_t mask) {
    if (!scene_ || !world_cache_) return;

    auto* rb = world_cache_->registry().try_get<RigidBody3DComponent>(entity);
    if (!rb) return;

    rb->collision_layer = layer;
    rb->collision_mask = mask;

    if (rb->runtime_body) {
        PxRigidActor* actor = static_cast<PxRigidActor*>(rb->runtime_body);
        PxFilterData fd;
        fd.word0 = layer;
        fd.word1 = mask;
        PxU32 nb = actor->getNbShapes();
        std::vector<PxShape*> shapes(nb);
        actor->getShapes(shapes.data(), nb);
        for (PxU32 i = 0; i < nb; ++i) {
            shapes[i]->setSimulationFilterData(fd);
            shapes[i]->setQueryFilterData(fd);
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: set filter data on shape from rigid body layer/mask
// ---------------------------------------------------------------------------
static void SetShapeFilterData(PxShape* shape, const RigidBody3DComponent& rb) {
    PxFilterData fd;
    fd.word0 = rb.collision_layer;
    fd.word1 = rb.collision_mask;
    shape->setSimulationFilterData(fd);
    shape->setQueryFilterData(fd);
}

// ---------------------------------------------------------------------------
// Helper: detach all shapes from actor (for dirty re-creation, Task 7)
// ---------------------------------------------------------------------------
static void DetachAllShapes(PxRigidActor* actor) {
    PxU32 nb = actor->getNbShapes();
    std::vector<PxShape*> shapes(nb);
    actor->getShapes(shapes.data(), nb);
    for (PxU32 i = 0; i < nb; ++i) {
        actor->detachShape(*shapes[i]);
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
                dynamic->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
                dynamic->setMass(rb.mass);
                dynamic->setLinearDamping(rb.drag);
                dynamic->setAngularDamping(rb.angular_drag);
                actor = dynamic;
            }
            
            bool shape_attached = false;

            // BoxCollider3D
            if (world.registry().all_of<BoxCollider3DComponent>(entity)) {
                auto& box = world.registry().get<BoxCollider3DComponent>(entity);
                PxMaterial* mat = GetOrCreateMaterial(box.friction, box.bounciness);
                PxShape* shape = physics_->createShape(PxBoxGeometry(box.size.x * 0.5f, box.size.y * 0.5f, box.size.z * 0.5f), *mat);
                shape->setLocalPose(PxTransform(PxVec3(box.center.x, box.center.y, box.center.z)));
                if (box.is_trigger) {
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                }
                SetShapeFilterData(shape, rb);
                actor->attachShape(*shape);
                shape->release();
                box.prev_size = box.size;
                box.prev_bounciness = box.bounciness;
                box.prev_friction = box.friction;
                shape_attached = true;
            }

            // SphereCollider3D
            if (world.registry().all_of<SphereCollider3DComponent>(entity)) {
                auto& sphere = world.registry().get<SphereCollider3DComponent>(entity);
                PxMaterial* mat = GetOrCreateMaterial(sphere.friction, sphere.bounciness);
                PxShape* shape = physics_->createShape(PxSphereGeometry(sphere.radius), *mat);
                shape->setLocalPose(PxTransform(PxVec3(sphere.center.x, sphere.center.y, sphere.center.z)));
                if (sphere.is_trigger) {
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                }
                SetShapeFilterData(shape, rb);
                actor->attachShape(*shape);
                shape->release();
                sphere.prev_radius = sphere.radius;
                sphere.prev_bounciness = sphere.bounciness;
                sphere.prev_friction = sphere.friction;
                shape_attached = true;
            }

            // CapsuleCollider3D (Task 4)
            if (world.registry().all_of<CapsuleCollider3DComponent>(entity)) {
                auto& cap = world.registry().get<CapsuleCollider3DComponent>(entity);
                PxMaterial* mat = GetOrCreateMaterial(cap.friction, cap.bounciness);
                PxCapsuleGeometry capsule_geo(cap.radius, cap.height * 0.5f);
                PxShape* shape = physics_->createShape(capsule_geo, *mat);
                // PhysX capsule default axis is X; rotate for Y or Z
                PxQuat rot = PxQuat(PxIdentity);
                if (cap.direction == 1) // Y-axis
                    rot = PxQuat(kPi * 0.5f, PxVec3(0, 0, 1));
                else if (cap.direction == 2) // Z-axis
                    rot = PxQuat(kPi * 0.5f, PxVec3(0, 1, 0));
                shape->setLocalPose(PxTransform(PxVec3(cap.center.x, cap.center.y, cap.center.z), rot));
                if (cap.is_trigger) {
                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                }
                SetShapeFilterData(shape, rb);
                actor->attachShape(*shape);
                shape->release();
                cap.prev_radius = cap.radius;
                cap.prev_height = cap.height;
                cap.prev_bounciness = cap.bounciness;
                cap.prev_friction = cap.friction;
                shape_attached = true;
            }

            // MeshCollider3D (Task 3)
            if (world.registry().all_of<MeshCollider3DComponent>(entity)) {
                auto& mc = world.registry().get<MeshCollider3DComponent>(entity);
                if (!mc.runtime_shape && world.registry().all_of<MeshRendererComponent>(entity)) {
                    auto& mr = world.registry().get<MeshRendererComponent>(entity);
                    if (!mr.temp_vertices.empty() && !mr.temp_indices.empty()) {
                        bool is_dmesh = mr.mesh_path.find(".dmesh") != std::string::npos;
                        size_t stride = is_dmesh ? static_cast<size_t>(mr.dmesh_vertex_stride) : 3;
                        size_t vcount = mr.temp_vertices.size() / stride;

                        // Extract positions
                        std::vector<PxVec3> px_verts(vcount);
                        for (size_t i = 0; i < vcount; ++i) {
                            px_verts[i] = PxVec3(
                                mr.temp_vertices[i * stride + 0],
                                mr.temp_vertices[i * stride + 1],
                                mr.temp_vertices[i * stride + 2]);
                        }

                        // Convert indices to 32-bit
                        std::vector<PxU32> px_indices(mr.temp_indices.size());
                        for (size_t i = 0; i < mr.temp_indices.size(); ++i)
                            px_indices[i] = static_cast<PxU32>(mr.temp_indices[i]);

                        PxMaterial* mat = GetOrCreateMaterial(mc.friction, mc.bounciness);

                        if (mc.convex) {
                            // Check cache
                            PxConvexMesh* convex = nullptr;
                            auto cache_it = convex_mesh_cache_.find(mr.mesh_path);
                            if (cache_it != convex_mesh_cache_.end()) {
                                convex = cache_it->second;
                            } else if (cooking_) {
                                PxConvexMeshDesc desc;
                                desc.points.count = static_cast<PxU32>(vcount);
                                desc.points.stride = sizeof(PxVec3);
                                desc.points.data = px_verts.data();
                                desc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

                                convex = cooking_->createConvexMesh(desc, physics_->getPhysicsInsertionCallback());
                                if (convex && !mr.mesh_path.empty())
                                    convex_mesh_cache_[mr.mesh_path] = convex;
                            }
                            if (convex) {
                                PxShape* shape = physics_->createShape(PxConvexMeshGeometry(convex), *mat);
                                if (mc.is_trigger) {
                                    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                                    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                                }
                                SetShapeFilterData(shape, rb);
                                actor->attachShape(*shape);
                                shape->release();
                                mc.runtime_shape = convex;
                                shape_attached = true;
                            }
                        } else {
                            // Triangle mesh — only for Static actors
                            if (rb.type == RigidBody3DType::Static) {
                                PxTriangleMesh* tri = nullptr;
                                auto cache_it = triangle_mesh_cache_.find(mr.mesh_path);
                                if (cache_it != triangle_mesh_cache_.end()) {
                                    tri = cache_it->second;
                                } else if (cooking_) {
                                    PxTriangleMeshDesc desc;
                                    desc.points.count = static_cast<PxU32>(vcount);
                                    desc.points.stride = sizeof(PxVec3);
                                    desc.points.data = px_verts.data();
                                    desc.triangles.count = static_cast<PxU32>(px_indices.size() / 3);
                                    desc.triangles.stride = sizeof(PxU32) * 3;
                                    desc.triangles.data = px_indices.data();

                                    tri = cooking_->createTriangleMesh(desc, physics_->getPhysicsInsertionCallback());
                                    if (tri && !mr.mesh_path.empty())
                                        triangle_mesh_cache_[mr.mesh_path] = tri;
                                }
                                if (tri) {
                                    PxShape* shape = physics_->createShape(PxTriangleMeshGeometry(tri), *mat);
                                    if (mc.is_trigger) {
                                        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                                        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                                    }
                                    SetShapeFilterData(shape, rb);
                                    actor->attachShape(*shape);
                                    shape->release();
                                    mc.runtime_shape = tri;
                                    shape_attached = true;
                                }
                            } else {
                                DEBUG_LOG_ERROR("MeshCollider3D: non-convex triangle mesh only supported on Static actors");
                            }
                        }
                    }
                }
            }

            // Default shape if nothing attached
            if (!shape_attached) {
                PxShape* shape = physics_->createShape(PxBoxGeometry(0.5f, 0.5f, 0.5f), *default_material_);
                SetShapeFilterData(shape, rb);
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
                    float m = dyn->getMass();
                    if (m < 0.001f) m = 0.001f;
                    glm::vec3 vel = rb.pending_impulse / m;
                    dyn->setLinearVelocity(PxVec3(vel.x, vel.y, vel.z));
                }
                rb.has_pending_impulse = false;
                rb.pending_impulse = glm::vec3(0.0f);
            }
        } else {
            // --- Runtime dirty detection (Task 7) ---
            PxRigidActor* actor = static_cast<PxRigidActor*>(rb.runtime_body);
            bool needs_reshape = false;

            // Check BoxCollider dirty
            if (world.registry().all_of<BoxCollider3DComponent>(entity)) {
                auto& box = world.registry().get<BoxCollider3DComponent>(entity);
                if (box.size != box.prev_size || box.bounciness != box.prev_bounciness || box.friction != box.prev_friction) {
                    needs_reshape = true;
                }
            }
            // Check SphereCollider dirty
            if (world.registry().all_of<SphereCollider3DComponent>(entity)) {
                auto& sphere = world.registry().get<SphereCollider3DComponent>(entity);
                if (sphere.radius != sphere.prev_radius || sphere.bounciness != sphere.prev_bounciness || sphere.friction != sphere.prev_friction) {
                    needs_reshape = true;
                }
            }
            // Check CapsuleCollider dirty
            if (world.registry().all_of<CapsuleCollider3DComponent>(entity)) {
                auto& cap = world.registry().get<CapsuleCollider3DComponent>(entity);
                if (cap.radius != cap.prev_radius || cap.height != cap.prev_height ||
                    cap.bounciness != cap.prev_bounciness || cap.friction != cap.prev_friction) {
                    needs_reshape = true;
                }
            }

            if (needs_reshape && actor) {
                DetachAllShapes(actor);

                if (world.registry().all_of<BoxCollider3DComponent>(entity)) {
                    auto& box = world.registry().get<BoxCollider3DComponent>(entity);
                    PxMaterial* mat = GetOrCreateMaterial(box.friction, box.bounciness);
                    PxShape* shape = physics_->createShape(PxBoxGeometry(box.size.x * 0.5f, box.size.y * 0.5f, box.size.z * 0.5f), *mat);
                    shape->setLocalPose(PxTransform(PxVec3(box.center.x, box.center.y, box.center.z)));
                    if (box.is_trigger) {
                        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                    }
                    SetShapeFilterData(shape, rb);
                    actor->attachShape(*shape);
                    shape->release();
                    box.prev_size = box.size;
                    box.prev_bounciness = box.bounciness;
                    box.prev_friction = box.friction;
                }
                if (world.registry().all_of<SphereCollider3DComponent>(entity)) {
                    auto& sphere = world.registry().get<SphereCollider3DComponent>(entity);
                    PxMaterial* mat = GetOrCreateMaterial(sphere.friction, sphere.bounciness);
                    PxShape* shape = physics_->createShape(PxSphereGeometry(sphere.radius), *mat);
                    shape->setLocalPose(PxTransform(PxVec3(sphere.center.x, sphere.center.y, sphere.center.z)));
                    if (sphere.is_trigger) {
                        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                    }
                    SetShapeFilterData(shape, rb);
                    actor->attachShape(*shape);
                    shape->release();
                    sphere.prev_radius = sphere.radius;
                    sphere.prev_bounciness = sphere.bounciness;
                    sphere.prev_friction = sphere.friction;
                }
                if (world.registry().all_of<CapsuleCollider3DComponent>(entity)) {
                    auto& cap = world.registry().get<CapsuleCollider3DComponent>(entity);
                    PxMaterial* mat = GetOrCreateMaterial(cap.friction, cap.bounciness);
                    PxCapsuleGeometry capsule_geo(cap.radius, cap.height * 0.5f);
                    PxShape* shape = physics_->createShape(capsule_geo, *mat);
                    PxQuat rot = PxQuat(PxIdentity);
                    if (cap.direction == 1) rot = PxQuat(kPi * 0.5f, PxVec3(0, 0, 1));
                    else if (cap.direction == 2) rot = PxQuat(kPi * 0.5f, PxVec3(0, 1, 0));
                    shape->setLocalPose(PxTransform(PxVec3(cap.center.x, cap.center.y, cap.center.z), rot));
                    if (cap.is_trigger) {
                        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                        shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
                    }
                    SetShapeFilterData(shape, rb);
                    actor->attachShape(*shape);
                    shape->release();
                    cap.prev_radius = cap.radius;
                    cap.prev_height = cap.height;
                    cap.prev_bounciness = cap.bounciness;
                    cap.prev_friction = cap.friction;
                }
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
    SyncJoints(world);

    // Clear previous frame events (Task 1)
    collision_events_.clear();
    trigger_events_.clear();
    auto* callback = static_cast<DseSimulationEventCallback*>(simulation_callback_);
    if (callback) {
        callback->collision_events.clear();
        callback->trigger_events.clear();
    }

    scene_->simulate(fixed_delta_time);
    scene_->fetchResults(true);

    // Collect events from callback (Task 1)
    if (callback) {
        collision_events_ = std::move(callback->collision_events);
        trigger_events_ = std::move(callback->trigger_events);
    }

    SyncPhysicsToTransforms(world);
    CheckBrokenJoints(world);
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

void Physics3DSystem::AddTorque(entt::entity entity, const glm::vec3& torque) {
    if (!scene_) return;
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return;

    auto& rb = view.get<RigidBody3DComponent>(*it);
    if (rb.type != RigidBody3DType::Dynamic || !rb.runtime_body) return;

    PxRigidDynamic* dynamic = static_cast<PxRigidActor*>(rb.runtime_body)->is<PxRigidDynamic>();
    if (dynamic) {
        dynamic->addTorque(PxVec3(torque.x, torque.y, torque.z), PxForceMode::eFORCE);
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

void Physics3DSystem::SetAngularVelocity(entt::entity entity, const glm::vec3& angular_velocity) {
    if (!scene_) return;
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return;

    auto& rb = view.get<RigidBody3DComponent>(*it);
    if (rb.type != RigidBody3DType::Dynamic || !rb.runtime_body) return;

    PxRigidDynamic* dynamic = static_cast<PxRigidActor*>(rb.runtime_body)->is<PxRigidDynamic>();
    if (dynamic) {
        dynamic->setAngularVelocity(PxVec3(angular_velocity.x, angular_velocity.y, angular_velocity.z));
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

glm::vec3 Physics3DSystem::GetAngularVelocity(entt::entity entity) const {
    if (!scene_) return glm::vec3(0.0f);
    auto view = world_cache_->registry().view<RigidBody3DComponent>();
    auto it = view.find(entity);
    if (it == view.end()) return glm::vec3(0.0f);

    auto& rb = view.get<RigidBody3DComponent>(*it);
    if (rb.type != RigidBody3DType::Dynamic || !rb.runtime_body) return glm::vec3(0.0f);

    PxRigidDynamic* dynamic = static_cast<PxRigidActor*>(rb.runtime_body)->is<PxRigidDynamic>();
    if (dynamic) {
        PxVec3 vel = dynamic->getAngularVelocity();
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

// sweep 时排除角色自身 actor，避免自碰撞导致位移归零
class SelfExcludeQueryFilter : public PxQueryFilterCallback {
public:
    const PxRigidActor* self_actor;
    explicit SelfExcludeQueryFilter(const PxRigidActor* self) : self_actor(self) {}

    PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape*,
        const PxRigidActor* actor, PxHitFlags&) override {
        return (actor == self_actor) ? PxQueryHitType::eNONE : PxQueryHitType::eBLOCK;
    }
    PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override {
        return PxQueryHitType::eBLOCK;
    }
};

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

        // sweep 过滤：排除自身 actor
        SelfExcludeQueryFilter self_filter(actor);
        PxQueryFilterData filter_data;
        filter_data.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;

        // ---- 水平 sweep ----
        glm::vec3 horizontal_disp(displacement.x, 0.0f, displacement.z);
        float h_len = glm::length(horizontal_disp);
        if (h_len > 1e-6f) {
            PxVec3 px_dir(horizontal_disp.x / h_len, 0.0f, horizontal_disp.z / h_len);
            PxSweepBuffer sweep_hit;
            PxTransform pose = actor->getGlobalPose();
            PxCapsuleGeometry capsule_geo(cc.radius, cc.height * 0.5f);

            if (scene_->sweep(capsule_geo, pose, px_dir, h_len, sweep_hit,
                              PxHitFlags(PxHitFlag::eDEFAULT), filter_data, &self_filter)) {
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

            if (scene_->sweep(capsule_geo, pose, px_dir, abs_v, sweep_hit,
                              PxHitFlags(PxHitFlag::eDEFAULT), filter_data, &self_filter)) {
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

            if (scene_->sweep(capsule_geo, pose, PxVec3(0.0f, -1.0f, 0.0f), ground_check_dist, sweep_hit,
                              PxHitFlags(PxHitFlag::eDEFAULT), filter_data, &self_filter)) {
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

        // 地形高度图贴地（PhysX 场景可能没有地形碰撞体）
        float foot_y = new_pose.p.y - cc.radius - cc.height * 0.5f;
        float terrain_y = -1e10f;
        auto hm_view = world.registry().view<TerrainHeightmapComponent>();
        for (auto te : hm_view) {
            float h = hm_view.get<TerrainHeightmapComponent>(te).GetHeight(new_pose.p.x, new_pose.p.z);
            if (h > terrain_y) terrain_y = h;
        }
        if (terrain_y > -1e9f && foot_y <= terrain_y) {
            new_pose.p.y = terrain_y + cc.radius + cc.height * 0.5f;
            cc.is_grounded = true;
            cc.velocity.y = 0.0f;
            cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Down;
        }

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

    // sweep 过滤：排除自身 actor
    SelfExcludeQueryFilter self_filter(actor);
    PxQueryFilterData filter_data;
    filter_data.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;

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

        if (scene_->sweep(capsule_geo, pose, px_dir, h_len, sweep_hit,
                          PxHitFlags(PxHitFlag::eDEFAULT), filter_data, &self_filter)) {
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

        if (scene_->sweep(capsule_geo, pose, px_dir, abs_v, sweep_hit,
                          PxHitFlags(PxHitFlag::eDEFAULT), filter_data, &self_filter)) {
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

        if (scene_->sweep(capsule_geo, pose, PxVec3(0.0f, -1.0f, 0.0f), ground_check_dist, sweep_hit,
                          PxHitFlags(PxHitFlag::eDEFAULT), filter_data, &self_filter)) {
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

    // 地形高度图贴地（PhysX 场景可能没有地形碰撞体）
    {
        float foot_y = new_pose.p.y - cc.radius - cc.height * 0.5f;
        float terrain_y = -1e10f;
        auto hm_view = world_cache_->registry().view<TerrainHeightmapComponent>();
        for (auto te : hm_view) {
            float h = hm_view.get<TerrainHeightmapComponent>(te).GetHeight(new_pose.p.x, new_pose.p.z);
            if (h > terrain_y) terrain_y = h;
        }
        if (terrain_y > -1e9f && foot_y <= terrain_y) {
            new_pose.p.y = terrain_y + cc.radius + cc.height * 0.5f;
            cc.is_grounded = true;
            cc.velocity.y = 0.0f;
            cc.collision_flags = cc.collision_flags | CharacterCollisionFlag::Down;
        }
    }

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


// ---------------------------------------------------------------------------
// Joint / Constraint 支持（Task 5）
// 注意：需要 PhysXExtensions 库（当前预编译版 CRT 不兼容，暂时禁用）
// ---------------------------------------------------------------------------

#ifdef DSE_HAS_PHYSX_EXTENSIONS
void Physics3DSystem::SyncJoints(World& world) {
    if (!physics_ || !scene_) return;

    auto view = world.registry().view<Joint3DComponent>();
    for (auto entity : view) {
        auto& jc = view.get<Joint3DComponent>(entity);
        if (jc.runtime_joint || jc.is_broken) continue;

        // Get actor A (this entity must have RigidBody3D)
        auto* rb_a = world.registry().try_get<RigidBody3DComponent>(entity);
        if (!rb_a || !rb_a->runtime_body) continue;
        PxRigidActor* actor_a = static_cast<PxRigidActor*>(rb_a->runtime_body);

        // Get actor B (connected entity, 0 = world anchor)
        PxRigidActor* actor_b = nullptr;
        if (jc.connected_entity_id != 0) {
            entt::entity other = static_cast<entt::entity>(jc.connected_entity_id);
            if (world.registry().valid(other)) {
                auto* rb_b = world.registry().try_get<RigidBody3DComponent>(other);
                if (rb_b && rb_b->runtime_body)
                    actor_b = static_cast<PxRigidActor*>(rb_b->runtime_body);
            }
            if (!actor_b) continue; // connected entity not ready yet
        }

        PxTransform frame_a(PxVec3(jc.anchor.x, jc.anchor.y, jc.anchor.z));
        PxTransform frame_b(PxVec3(jc.connected_anchor.x, jc.connected_anchor.y, jc.connected_anchor.z));

        PxJoint* joint = nullptr;

        switch (jc.type) {
            case Joint3DType::Fixed: {
                joint = PxFixedJointCreate(*physics_, actor_a, frame_a, actor_b, frame_b);
                break;
            }
            case Joint3DType::Hinge: {
                auto* rev = PxRevoluteJointCreate(*physics_, actor_a, frame_a, actor_b, frame_b);
                if (rev && jc.use_limits) {
                    float lower_rad = jc.lower_limit * (kPi / 180.0f);
                    float upper_rad = jc.upper_limit * (kPi / 180.0f);
                    rev->setLimit(PxJointAngularLimitPair(lower_rad, upper_rad));
                    rev->setRevoluteJointFlag(PxRevoluteJointFlag::eLIMIT_ENABLED, true);
                }
                joint = rev;
                break;
            }
            case Joint3DType::Distance: {
                auto* dist = PxDistanceJointCreate(*physics_, actor_a, frame_a, actor_b, frame_b);
                if (dist) {
                    dist->setMinDistance(jc.min_distance);
                    dist->setMaxDistance(jc.max_distance);
                    dist->setDistanceJointFlag(PxDistanceJointFlag::eMIN_DISTANCE_ENABLED, true);
                    dist->setDistanceJointFlag(PxDistanceJointFlag::eMAX_DISTANCE_ENABLED, true);
                }
                joint = dist;
                break;
            }
            case Joint3DType::Spring: {
                auto* d6 = PxD6JointCreate(*physics_, actor_a, frame_a, actor_b, frame_b);
                if (d6) {
                    // Free all linear axes with spring drive
                    d6->setMotion(PxD6Axis::eX, PxD6Motion::eFREE);
                    d6->setMotion(PxD6Axis::eY, PxD6Motion::eFREE);
                    d6->setMotion(PxD6Axis::eZ, PxD6Motion::eFREE);
                    PxD6JointDrive drive(jc.spring_stiffness, jc.spring_damping, PX_MAX_F32, true);
                    d6->setDrive(PxD6Drive::eX, drive);
                    d6->setDrive(PxD6Drive::eY, drive);
                    d6->setDrive(PxD6Drive::eZ, drive);
                    // Drive target is identity (pull back to anchor)
                    d6->setDrivePosition(PxTransform(PxIdentity));
                }
                joint = d6;
                break;
            }
        }

        if (joint) {
            if (jc.break_force < FLT_MAX || jc.break_torque < FLT_MAX) {
                joint->setBreakForce(jc.break_force, jc.break_torque);
            }
            jc.runtime_joint = joint;
        }
    }
}

void Physics3DSystem::CheckBrokenJoints(World& world) {
    auto view = world.registry().view<Joint3DComponent>();
    for (auto entity : view) {
        auto& jc = view.get<Joint3DComponent>(entity);
        if (!jc.runtime_joint || jc.is_broken) continue;

        PxJoint* joint = static_cast<PxJoint*>(jc.runtime_joint);
        if (joint->getConstraintFlags() & PxConstraintFlag::eBROKEN) {
            jc.is_broken = true;
            joint->release();
            jc.runtime_joint = nullptr;
            DEBUG_LOG_INFO("[Physics3D] Joint on entity {} has broken", static_cast<uint32_t>(entity));
        }
    }
}
#else
// Stub implementations when PhysXExtensions is not available
void Physics3DSystem::SyncJoints(World& /*world*/) {}
void Physics3DSystem::CheckBrokenJoints(World& /*world*/) {}
#endif // DSE_HAS_PHYSX_EXTENSIONS

} // namespace physics3d
} // namespace dse
