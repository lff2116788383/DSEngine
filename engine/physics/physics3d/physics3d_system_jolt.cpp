#ifdef DSE_ENABLE_JOLT

#include "physics3d_system_jolt.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/base/debug.h"
#include <glm/gtc/quaternion.hpp>

// Jolt includes
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Geometry/IndexedTriangle.h>
#include <mutex>

JPH_SUPPRESS_WARNINGS

using namespace JPH;

namespace dse {
namespace physics3d {

// ---------------------------------------------------------------------------
// Jolt 层配置
// ---------------------------------------------------------------------------
namespace Layers {
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING = 1;
    static constexpr ObjectLayer SENSOR = 2;
    static constexpr ObjectLayer NUM_LAYERS = 3;
}

namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS = 2;
}

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
        if (layer == Layers::NON_MOVING) return BroadPhaseLayers::NON_MOVING;
        return BroadPhaseLayers::MOVING;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer layer) const override {
        if (layer == BroadPhaseLayers::NON_MOVING) return "NON_MOVING";
        return "MOVING";
    }
#endif
};

class ObjVsBPLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer obj_layer, BroadPhaseLayer bp_layer) const override {
        if (obj_layer == Layers::NON_MOVING)
            return bp_layer == BroadPhaseLayers::MOVING;
        return true;
    }
};

class ObjLayerPairFilterImpl final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer obj1, ObjectLayer obj2) const override {
        if (obj1 == Layers::NON_MOVING && obj2 == Layers::NON_MOVING) return false;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Contact Listener — 收集碰撞和触发事件
// ---------------------------------------------------------------------------
class DseContactListener final : public ContactListener {
public:
    std::vector<CollisionEvent>* collision_events = nullptr;
    std::vector<TriggerEvent>* trigger_events = nullptr;
    PhysicsSystem* physics_system = nullptr;
    std::unordered_map<uint32_t, std::pair<uint16_t, uint16_t>>* body_layer_map = nullptr;

    struct ContactPairInfo {
        entt::entity entity_a = entt::null;
        entt::entity entity_b = entt::null;
        bool is_sensor = false;
    };
    std::unordered_map<uint64_t, ContactPairInfo> active_contacts_;
    mutable std::mutex contact_mutex_;

    static uint64_t MakeContactKey(const SubShapeIDPair& pair) {
        uint64_t a = pair.GetBody1ID().GetIndexAndSequenceNumber();
        uint64_t b = pair.GetBody2ID().GetIndexAndSequenceNumber();
        return (a << 32) | b;
    }

    ValidateResult OnContactValidate(const Body& body1, const Body& body2, RVec3Arg base_offset, const CollideShapeResult& result) override {
        (void)base_offset; (void)result;
        if (body_layer_map && !body_layer_map->empty()) {
            auto it1 = body_layer_map->find(body1.GetID().GetIndex());
            auto it2 = body_layer_map->find(body2.GetID().GetIndex());
            if (it1 != body_layer_map->end() && it2 != body_layer_map->end()) {
                uint16_t layer1 = it1->second.first, mask1 = it1->second.second;
                uint16_t layer2 = it2->second.first, mask2 = it2->second.second;
                if ((layer1 & mask2) == 0 || (layer2 & mask1) == 0)
                    return ValidateResult::RejectAllContactsForThisBodyPair;
            }
        }
        return ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const Body& body1, const Body& body2, const ContactManifold& manifold, ContactSettings& settings) override {
        (void)settings;
        auto e1 = static_cast<entt::entity>(static_cast<uint32_t>(body1.GetUserData()));
        auto e2 = static_cast<entt::entity>(static_cast<uint32_t>(body2.GetUserData()));
        bool is_sensor = body1.IsSensor() || body2.IsSensor();

        SubShapeIDPair pair(body1.GetID(), manifold.mSubShapeID1, body2.GetID(), manifold.mSubShapeID2);
        uint64_t key = MakeContactKey(pair);

        std::lock_guard<std::mutex> lock(contact_mutex_);

        if (is_sensor) {
            entt::entity trigger_e = body1.IsSensor() ? e1 : e2;
            entt::entity other_e   = body1.IsSensor() ? e2 : e1;
            active_contacts_[key] = { trigger_e, other_e, true };
        } else {
            active_contacts_[key] = { e1, e2, false };
        }

        if (is_sensor) {
            if (trigger_events) {
                TriggerEvent te;
                te.type = TriggerEvent::Type::Enter;
                te.trigger_entity = body1.IsSensor() ? e1 : e2;
                te.other_entity = body1.IsSensor() ? e2 : e1;
                trigger_events->push_back(te);
            }
        } else {
            if (collision_events) {
                CollisionEvent ce;
                ce.type = CollisionEvent::Type::Enter;
                ce.entity_a = e1;
                ce.entity_b = e2;
                auto cp = manifold.GetWorldSpaceContactPointOn1(0);
                ce.contact_point = glm::vec3(cp.GetX(), cp.GetY(), cp.GetZ());
                auto cn = manifold.mWorldSpaceNormal;
                ce.contact_normal = glm::vec3(cn.GetX(), cn.GetY(), cn.GetZ());
                ce.impulse = 0.0f;
                collision_events->push_back(ce);
            }
        }
    }

    void OnContactPersisted(const Body& body1, const Body& body2, const ContactManifold& manifold, ContactSettings& settings) override {
        (void)settings;
        auto e1 = static_cast<entt::entity>(static_cast<uint32_t>(body1.GetUserData()));
        auto e2 = static_cast<entt::entity>(static_cast<uint32_t>(body2.GetUserData()));

        if (!body1.IsSensor() && !body2.IsSensor() && collision_events) {
            std::lock_guard<std::mutex> lock(contact_mutex_);
            CollisionEvent ce;
            ce.type = CollisionEvent::Type::Stay;
            ce.entity_a = e1;
            ce.entity_b = e2;
            auto cp = manifold.GetWorldSpaceContactPointOn1(0);
            ce.contact_point = glm::vec3(cp.GetX(), cp.GetY(), cp.GetZ());
            auto cn = manifold.mWorldSpaceNormal;
            ce.contact_normal = glm::vec3(cn.GetX(), cn.GetY(), cn.GetZ());
            collision_events->push_back(ce);
        }
    }

    void OnContactRemoved(const SubShapeIDPair& shape_pair) override {
        uint64_t key = MakeContactKey(shape_pair);

        std::lock_guard<std::mutex> lock(contact_mutex_);

        auto it = active_contacts_.find(key);
        if (it == active_contacts_.end()) return;

        const auto& info = it->second;
        if (info.is_sensor) {
            if (trigger_events) {
                TriggerEvent te;
                te.type = TriggerEvent::Type::Exit;
                te.trigger_entity = info.entity_a;
                te.other_entity = info.entity_b;
                trigger_events->push_back(te);
            }
        } else {
            if (collision_events) {
                CollisionEvent ce;
                ce.type = CollisionEvent::Type::Exit;
                ce.entity_a = info.entity_a;
                ce.entity_b = info.entity_b;
                ce.contact_point = glm::vec3(0.0f);
                ce.contact_normal = glm::vec3(0.0f);
                ce.impulse = 0.0f;
                collision_events->push_back(ce);
            }
        }
        active_contacts_.erase(it);
    }
};

// ---------------------------------------------------------------------------
// Impl (PIMPL)
// ---------------------------------------------------------------------------
struct Physics3DSystem::Impl {
    std::unique_ptr<TempAllocatorImpl> temp_allocator;
    std::unique_ptr<JobSystemThreadPool> job_system;
    std::unique_ptr<PhysicsSystem> physics_system;
    std::unique_ptr<BPLayerInterfaceImpl> bp_layer_interface;
    std::unique_ptr<ObjVsBPLayerFilterImpl> obj_vs_bp_filter;
    std::unique_ptr<ObjLayerPairFilterImpl> obj_layer_pair_filter;
    std::unique_ptr<DseContactListener> contact_listener;

    // Entity ↔ BodyID 映射
    std::unordered_map<uint32_t, BodyID> entity_to_body;
    std::unordered_map<uint32_t, BodyID> entity_to_character_body;

    // 角色控制器
    std::unordered_map<uint32_t, Ref<CharacterVirtual>> character_virtuals;

    // 重力开关
    std::unordered_map<uint32_t, bool> gravity_override;

    // 碰撞层: BodyID.GetIndex() → (layer, mask)
    std::unordered_map<uint32_t, std::pair<uint16_t, uint16_t>> body_layer_map;

    // Joint: entity_id → Constraint*
    std::unordered_map<uint32_t, Constraint*> entity_to_constraint;

    // MeshCollider 缓存
    std::unordered_map<std::string, ShapeRefC> convex_mesh_cache;
    std::unordered_map<std::string, ShapeRefC> triangle_mesh_cache;

    ~Impl() {
        convex_mesh_cache.clear();
        triangle_mesh_cache.clear();
    }
};

// ---------------------------------------------------------------------------
// 构造 / 析构
// ---------------------------------------------------------------------------
Physics3DSystem::Physics3DSystem() = default;
Physics3DSystem::~Physics3DSystem() { Shutdown(); }

// ---------------------------------------------------------------------------
// 初始化
// ---------------------------------------------------------------------------
bool Physics3DSystem::Init(World& world) {
    world_cache_ = &world;

    // 注册 Jolt 类型（全局只调一次）
    static bool s_jolt_registered = false;
    if (!s_jolt_registered) {
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();
        s_jolt_registered = true;
    }

    impl_ = std::make_unique<Impl>();
    impl_->temp_allocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024); // 10 MB
    impl_->job_system = std::make_unique<JobSystemThreadPool>(
        cMaxPhysicsJobs, cMaxPhysicsBarriers, 2); // 2 worker threads

    impl_->bp_layer_interface = std::make_unique<BPLayerInterfaceImpl>();
    impl_->obj_vs_bp_filter = std::make_unique<ObjVsBPLayerFilterImpl>();
    impl_->obj_layer_pair_filter = std::make_unique<ObjLayerPairFilterImpl>();

    const uint max_bodies = 4096;
    const uint num_body_mutexes = 0; // auto
    const uint max_body_pairs = 4096;
    const uint max_contact_constraints = 2048;

    impl_->physics_system = std::make_unique<PhysicsSystem>();
    impl_->physics_system->Init(
        max_bodies, num_body_mutexes, max_body_pairs, max_contact_constraints,
        *impl_->bp_layer_interface, *impl_->obj_vs_bp_filter, *impl_->obj_layer_pair_filter);
    impl_->physics_system->SetGravity(Vec3(0.0f, -9.81f, 0.0f));

    // 设置碰撞监听
    impl_->contact_listener = std::make_unique<DseContactListener>();
    impl_->contact_listener->collision_events = &collision_events_;
    impl_->contact_listener->trigger_events = &trigger_events_;
    impl_->contact_listener->physics_system = impl_->physics_system.get();
    impl_->contact_listener->body_layer_map = &impl_->body_layer_map;
    impl_->physics_system->SetContactListener(impl_->contact_listener.get());

    DEBUG_LOG_INFO("Physics3DSystem Initialized (Backend: Jolt Physics v5.5.0)");
    return true;
}

// ---------------------------------------------------------------------------
// 关闭
// ---------------------------------------------------------------------------
void Physics3DSystem::Shutdown() {
    if (!impl_) return;

    // 先移除所有 constraint（必须在移除 body 之前）
    for (auto& [eid, constraint] : impl_->entity_to_constraint) {
        impl_->physics_system->RemoveConstraint(constraint);
    }
    impl_->entity_to_constraint.clear();

    // 销毁所有 body
    auto& bi = impl_->physics_system->GetBodyInterface();
    for (auto& [eid, body_id] : impl_->entity_to_body) {
        bi.RemoveBody(body_id);
        bi.DestroyBody(body_id);
    }
    impl_->entity_to_body.clear();
    impl_->body_layer_map.clear();
    impl_->character_virtuals.clear();
    impl_->gravity_override.clear();
    impl_.reset();

    DEBUG_LOG_INFO("Physics3DSystem Shutdown (Jolt)");
}

// ---------------------------------------------------------------------------
// 辅助：glm ↔ Jolt 转换
// ---------------------------------------------------------------------------
static inline Vec3 ToJolt(const glm::vec3& v) { return Vec3(v.x, v.y, v.z); }
static inline glm::vec3 ToGlm(const Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
static inline Quat ToJolt(const glm::quat& q) { return Quat(q.x, q.y, q.z, q.w); }
static inline glm::quat ToGlm(const Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

// ---------------------------------------------------------------------------
// FixedUpdate
// ---------------------------------------------------------------------------
void Physics3DSystem::FixedUpdate(World& world, float fixed_delta_time) {
    if (!impl_) return;

    SyncTransformsToPhysics(world);
    SyncJoints(world);

    const int collision_steps = 1;
    impl_->physics_system->Update(fixed_delta_time, collision_steps,
        impl_->temp_allocator.get(), impl_->job_system.get());

    SyncPhysicsToTransforms(world);
    SyncCharacterControllers(world, fixed_delta_time);
    CheckBrokenJoints(world);
}

// ---------------------------------------------------------------------------
// SyncTransformsToPhysics — 创建 body 或更新 kinematic 位置
// ---------------------------------------------------------------------------
void Physics3DSystem::SyncTransformsToPhysics(World& world) {
    auto& registry = world.registry();
    auto& bi = impl_->physics_system->GetBodyInterface();

    auto view = registry.view<TransformComponent, RigidBody3DComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& rb = view.get<RigidBody3DComponent>(entity);
        uint32_t eid = static_cast<uint32_t>(entity);

        if (impl_->entity_to_body.find(eid) == impl_->entity_to_body.end()) {
            // 创建新的 body
            EMotionType motion_type;
            ObjectLayer layer;
            switch (rb.type) {
                case RigidBody3DType::Static:
                    motion_type = EMotionType::Static;
                    layer = Layers::NON_MOVING;
                    break;
                case RigidBody3DType::Kinematic:
                    motion_type = EMotionType::Kinematic;
                    layer = Layers::MOVING;
                    break;
                default:
                    motion_type = EMotionType::Dynamic;
                    layer = Layers::MOVING;
                    break;
            }

            // 确定碰撞形状
            ShapeRefC shape;
            bool is_trigger = false;
            float friction = 0.5f;
            float restitution = rb.type == RigidBody3DType::Static ? 0.0f : 0.3f;

            if (registry.all_of<BoxCollider3DComponent>(entity)) {
                auto& box = registry.get<BoxCollider3DComponent>(entity);
                glm::vec3 half = box.size * 0.5f;
                shape = new BoxShape(Vec3(half.x, half.y, half.z));
                is_trigger = box.is_trigger;
                friction = box.friction;
                restitution = box.bounciness;
            } else if (registry.all_of<SphereCollider3DComponent>(entity)) {
                auto& sphere = registry.get<SphereCollider3DComponent>(entity);
                shape = new SphereShape(sphere.radius);
                is_trigger = sphere.is_trigger;
                friction = sphere.friction;
                restitution = sphere.bounciness;
            } else if (registry.all_of<CapsuleCollider3DComponent>(entity)) {
                auto& cap = registry.get<CapsuleCollider3DComponent>(entity);
                shape = new CapsuleShape(cap.height * 0.5f, cap.radius);
                is_trigger = cap.is_trigger;
                friction = cap.friction;
                restitution = cap.bounciness;
            } else if (registry.all_of<MeshCollider3DComponent>(entity)) {
                auto& mc = registry.get<MeshCollider3DComponent>(entity);
                if (!mc.runtime_shape && registry.all_of<MeshRendererComponent>(entity)) {
                    auto& mr = registry.get<MeshRendererComponent>(entity);
                    if (!mr.temp_vertices.empty() && !mr.temp_indices.empty()) {
                        bool is_dmesh = mr.mesh_path.find(".dmesh") != std::string::npos;
                        size_t stride = is_dmesh ? static_cast<size_t>(mr.dmesh_vertex_stride) : 3;
                        size_t vcount = mr.temp_vertices.size() / stride;

                        // 提取位置
                        std::vector<glm::vec3> vertices(vcount);
                        for (size_t i = 0; i < vcount; ++i) {
                            vertices[i] = glm::vec3(
                                mr.temp_vertices[i * stride + 0],
                                mr.temp_vertices[i * stride + 1],
                                mr.temp_vertices[i * stride + 2]);
                        }

                        // 转换索引
                        std::vector<uint32_t> indices(mr.temp_indices.size());
                        for (size_t i = 0; i < mr.temp_indices.size(); ++i)
                            indices[i] = mr.temp_indices[i];

                        JPH::Shape* mesh_shape = nullptr;
                        if (mc.convex) {
                            mesh_shape = CreateConvexHullShape(vertices, mr.mesh_path);
                        } else {
                            // Triangle mesh — 仅支持 Static
                            if (rb.type == RigidBody3DType::Static) {
                                mesh_shape = CreateTriangleMeshShape(vertices, indices, mr.mesh_path);
                            } else {
                                DEBUG_LOG_ERROR("MeshCollider3D: non-convex triangle mesh only supported on Static actors");
                            }
                        }
                        if (mesh_shape) {
                            shape = mesh_shape;
                            mc.runtime_shape = mesh_shape;
                            mc.prev_mesh_path = mr.mesh_path;
                            is_trigger = mc.is_trigger;
                            friction = mc.friction;
                            restitution = mc.bounciness;
                        }
                    }
                }
                // 如果 shape 创建失败，使用默认
                if (!shape) {
                    shape = new SphereShape(0.5f);
                }
            } else {
                // 默认 1m 球
                shape = new SphereShape(0.5f);
            }

            RVec3 pos(transform.position.x, transform.position.y, transform.position.z);
            Quat rot = ToJolt(transform.rotation);

            BodyCreationSettings body_settings(shape, pos, rot, motion_type, layer);
            body_settings.mUserData = static_cast<uint64>(eid);
            body_settings.mFriction = friction;
            body_settings.mRestitution = restitution;

            if (motion_type == EMotionType::Dynamic) {
                body_settings.mLinearDamping = rb.drag;
                body_settings.mAngularDamping = rb.angular_drag;
                body_settings.mGravityFactor = rb.use_gravity ? rb.gravity_scale : 0.0f;
            }

            // 传感器设置（trigger）
            // Jolt 不支持 Static sensor，必须升级为 Kinematic
            if (is_trigger) {
                body_settings.mIsSensor = true;
            }
            if (body_settings.mIsSensor && body_settings.mMotionType == EMotionType::Static) {
                body_settings.mMotionType = EMotionType::Kinematic;
                body_settings.mObjectLayer = Layers::MOVING;
            }

            Body* body = bi.CreateBody(body_settings);
            if (body) {
                EActivation activation = (motion_type == EMotionType::Dynamic || body_settings.mIsSensor)
                    ? EActivation::Activate : EActivation::DontActivate;
                bi.AddBody(body->GetID(), activation);
                impl_->entity_to_body[eid] = body->GetID();
                impl_->body_layer_map[body->GetID().GetIndex()] = { rb.collision_layer, rb.collision_mask };
            }
        } else {
            // Kinematic body — 更新目标位置
            if (rb.type == RigidBody3DType::Kinematic) {
                BodyID body_id = impl_->entity_to_body[eid];
                RVec3 pos(transform.position.x, transform.position.y, transform.position.z);
                bi.MoveKinematic(body_id, pos, ToJolt(transform.rotation), 1.0f / 60.0f);
            }

            // MeshCollider 动态更新检查
            if (registry.all_of<MeshCollider3DComponent>(entity)) {
                auto& mc = registry.get<MeshCollider3DComponent>(entity);
                if (mc.runtime_shape && registry.all_of<MeshRendererComponent>(entity)) {
                    auto& mr = registry.get<MeshRendererComponent>(entity);
                    // 检查 mesh 是否改变（通过 mesh_path）
                    if (mc.prev_mesh_path != mr.mesh_path) {
                        BodyID body_id = impl_->entity_to_body[eid];
                        bi.RemoveBody(body_id);
                        bi.DestroyBody(body_id);
                        impl_->entity_to_body.erase(eid);
                        impl_->body_layer_map.erase(body_id.GetIndex());
                        mc.runtime_shape = nullptr;
                        mc.prev_mesh_path = mr.mesh_path;
                        // 下一次循环会重新创建 body
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// SyncPhysicsToTransforms — 从物理同步回 ECS transform
// ---------------------------------------------------------------------------
void Physics3DSystem::SyncPhysicsToTransforms(World& world) {
    auto& registry = world.registry();
    auto& bi = impl_->physics_system->GetBodyInterface();

    auto view = registry.view<TransformComponent, RigidBody3DComponent>();
    for (auto entity : view) {
        auto& rb = view.get<RigidBody3DComponent>(entity);
        if (rb.type != RigidBody3DType::Dynamic) continue;

        uint32_t eid = static_cast<uint32_t>(entity);
        auto it = impl_->entity_to_body.find(eid);
        if (it == impl_->entity_to_body.end()) continue;

        RVec3 pos = bi.GetCenterOfMassPosition(it->second);
        Quat rot = bi.GetRotation(it->second);

        auto& transform = view.get<TransformComponent>(entity);
        transform.position = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
        transform.rotation = ToGlm(rot);
    }
}

// ---------------------------------------------------------------------------
// 角色控制器
// ---------------------------------------------------------------------------
void Physics3DSystem::SyncCharacterControllers(World& world, float fixed_delta_time) {
    (void)world; (void)fixed_delta_time;
    // CharacterVirtual 更新在 MoveCharacter() 中按需执行
}

void Physics3DSystem::CreateCharacterActor(World& world, entt::entity entity,
    CharacterController3DComponent& cc, const ::TransformComponent& transform)
{
    (void)world;
    if (!impl_) return;

    uint32_t eid = static_cast<uint32_t>(entity);
    if (impl_->character_virtuals.find(eid) != impl_->character_virtuals.end()) return;

    Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
    settings->mShape = new CapsuleShape(cc.height * 0.5f, cc.radius);
    settings->mMaxSlopeAngle = DegreesToRadians(cc.slope_limit > 0.0f ? cc.slope_limit : 45.0f);
    settings->mMaxStrength = 100.0f;
    settings->mPenetrationRecoverySpeed = 1.0f;

    RVec3 pos(transform.position.x, transform.position.y, transform.position.z);
    Quat rot = Quat::sIdentity();

    Ref<CharacterVirtual> character = new CharacterVirtual(settings, pos, rot, 0, impl_->physics_system.get());
    impl_->character_virtuals[eid] = character;
}

CharacterMoveResult Physics3DSystem::MoveCharacter(entt::entity entity, const glm::vec3& displacement, float min_dist, float delta_time) {
    CharacterMoveResult result;
    if (!impl_) return result;

    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->character_virtuals.find(eid);
    if (it == impl_->character_virtuals.end()) {
        // 自动创建
        if (world_cache_) {
            auto& registry = world_cache_->registry();
            if (registry.valid(entity) && registry.all_of<CharacterController3DComponent, TransformComponent>(entity)) {
                auto& cc = registry.get<CharacterController3DComponent>(entity);
                auto& tf = registry.get<TransformComponent>(entity);
                CreateCharacterActor(*world_cache_, entity, cc, tf);
                it = impl_->character_virtuals.find(eid);
            }
        }
        if (it == impl_->character_virtuals.end()) return result;
    }

    auto& character = it->second;
    Vec3 desired_velocity = ToJolt(displacement) / delta_time;

    CharacterVirtual::ExtendedUpdateSettings update_settings;
    character->ExtendedUpdate(delta_time, -impl_->physics_system->GetGravity(),
        update_settings,
        impl_->physics_system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        impl_->physics_system->GetDefaultLayerFilter(Layers::MOVING),
        {}, {}, *impl_->temp_allocator);

    character->SetLinearVelocity(desired_velocity);

    result.is_grounded = character->GetGroundState() == CharacterVirtual::EGroundState::OnGround;
    Vec3 v = character->GetLinearVelocity();
    result.velocity = ToGlm(v);

    // 同步位置回 transform
    if (world_cache_) {
        auto& registry = world_cache_->registry();
        if (registry.valid(entity) && registry.all_of<TransformComponent>(entity)) {
            auto& tf = registry.get<TransformComponent>(entity);
            RVec3 p = character->GetPosition();
            tf.position = glm::vec3(p.GetX(), p.GetY(), p.GetZ());
        }
    }

    return result;
}

bool Physics3DSystem::JumpCharacter(entt::entity entity, float jump_speed) {
    if (!impl_) return false;
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->character_virtuals.find(eid);
    if (it == impl_->character_virtuals.end()) return false;
    auto& character = it->second;
    if (character->GetGroundState() != CharacterVirtual::EGroundState::OnGround) return false;
    Vec3 v = character->GetLinearVelocity();
    character->SetLinearVelocity(Vec3(v.GetX(), jump_speed, v.GetZ()));
    return true;
}

bool Physics3DSystem::IsCharacterGrounded(entt::entity entity) const {
    if (!impl_) return false;
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->character_virtuals.find(eid);
    if (it == impl_->character_virtuals.end()) return false;
    return it->second->GetGroundState() == CharacterVirtual::EGroundState::OnGround;
}

glm::vec3 Physics3DSystem::GetCharacterPosition(entt::entity entity) const {
    if (!impl_) return glm::vec3(0.0f);
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->character_virtuals.find(eid);
    if (it == impl_->character_virtuals.end()) return glm::vec3(0.0f);
    RVec3 p = it->second->GetPosition();
    return glm::vec3(p.GetX(), p.GetY(), p.GetZ());
}

// ---------------------------------------------------------------------------
// Raycast
// ---------------------------------------------------------------------------
RaycastResult Physics3DSystem::Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance) {
    RaycastResult result;
    if (!impl_) return result;

    RRayCast ray(RVec3(origin.x, origin.y, origin.z), Vec3(direction.x, direction.y, direction.z) * max_distance);
    RayCastResult hit;
    if (impl_->physics_system->GetNarrowPhaseQuery().CastRay(ray, hit)) {
        result.hit = true;
        result.distance = hit.mFraction * max_distance;
        RVec3 hit_pos = ray.GetPointOnRay(hit.mFraction);
        result.hit_point = glm::vec3(hit_pos.GetX(), hit_pos.GetY(), hit_pos.GetZ());

        BodyLockRead lock(impl_->physics_system->GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const Body& body = lock.GetBody();
            result.entity = static_cast<entt::entity>(static_cast<uint32_t>(body.GetUserData()));
            Vec3 normal = body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hit_pos);
            result.hit_normal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// 刚体动力学 API
// ---------------------------------------------------------------------------
void Physics3DSystem::AddForce(entt::entity entity, const glm::vec3& force) {
    if (!impl_) return;
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->entity_to_body.find(eid);
    if (it == impl_->entity_to_body.end()) return;
    impl_->physics_system->GetBodyInterface().AddForce(it->second, ToJolt(force));
}

void Physics3DSystem::AddTorque(entt::entity entity, const glm::vec3& torque) {
    if (!impl_) return;
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->entity_to_body.find(eid);
    if (it == impl_->entity_to_body.end()) return;
    impl_->physics_system->GetBodyInterface().AddTorque(it->second, ToJolt(torque));
}

void Physics3DSystem::AddImpulse(entt::entity entity, const glm::vec3& impulse) {
    if (!impl_) return;
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->entity_to_body.find(eid);
    if (it == impl_->entity_to_body.end()) return;
    impl_->physics_system->GetBodyInterface().AddImpulse(it->second, ToJolt(impulse));
}

void Physics3DSystem::SetVelocity(entt::entity entity, const glm::vec3& velocity) {
    if (!impl_) return;
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->entity_to_body.find(eid);
    if (it == impl_->entity_to_body.end()) return;
    impl_->physics_system->GetBodyInterface().SetLinearVelocity(it->second, ToJolt(velocity));
}

void Physics3DSystem::SetAngularVelocity(entt::entity entity, const glm::vec3& angular_velocity) {
    if (!impl_) return;
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->entity_to_body.find(eid);
    if (it == impl_->entity_to_body.end()) return;
    impl_->physics_system->GetBodyInterface().SetAngularVelocity(it->second, ToJolt(angular_velocity));
}

glm::vec3 Physics3DSystem::GetVelocity(entt::entity entity) const {
    if (!impl_) return glm::vec3(0.0f);
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->entity_to_body.find(eid);
    if (it == impl_->entity_to_body.end()) return glm::vec3(0.0f);
    return ToGlm(impl_->physics_system->GetBodyInterface().GetLinearVelocity(it->second));
}

glm::vec3 Physics3DSystem::GetAngularVelocity(entt::entity entity) const {
    if (!impl_) return glm::vec3(0.0f);
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->entity_to_body.find(eid);
    if (it == impl_->entity_to_body.end()) return glm::vec3(0.0f);
    return ToGlm(impl_->physics_system->GetBodyInterface().GetAngularVelocity(it->second));
}

void Physics3DSystem::SetGravityEnabled(entt::entity entity, bool enabled) {
    if (!impl_) return;
    uint32_t eid = static_cast<uint32_t>(entity);
    impl_->gravity_override[eid] = enabled;
    auto it = impl_->entity_to_body.find(eid);
    if (it == impl_->entity_to_body.end()) return;
    impl_->physics_system->GetBodyInterface().SetGravityFactor(it->second, enabled ? 1.0f : 0.0f);
}

bool Physics3DSystem::IsGravityEnabled(entt::entity entity) const {
    if (!impl_) return true;
    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->gravity_override.find(eid);
    if (it != impl_->gravity_override.end()) return it->second;
    return true;
}

void Physics3DSystem::RemoveActor(entt::entity entity) {
    if (!impl_) return;
    uint32_t eid = static_cast<uint32_t>(entity);

    // 先移除关联的 constraint
    auto cit = impl_->entity_to_constraint.find(eid);
    if (cit != impl_->entity_to_constraint.end()) {
        impl_->physics_system->RemoveConstraint(cit->second);
        impl_->entity_to_constraint.erase(cit);
    }

    auto it = impl_->entity_to_body.find(eid);
    if (it != impl_->entity_to_body.end()) {
        auto& bi = impl_->physics_system->GetBodyInterface();
        impl_->body_layer_map.erase(it->second.GetIndex());
        bi.RemoveBody(it->second);
        bi.DestroyBody(it->second);
        impl_->entity_to_body.erase(it);
    }
    impl_->character_virtuals.erase(eid);
    impl_->gravity_override.erase(eid);
}

// ---------------------------------------------------------------------------
// 碰撞层
// ---------------------------------------------------------------------------
void Physics3DSystem::SetCollisionLayer(entt::entity entity, uint16_t layer, uint16_t mask) {
    if (!impl_ || !world_cache_) return;

    auto* rb = world_cache_->registry().try_get<RigidBody3DComponent>(entity);
    if (!rb) return;

    rb->collision_layer = layer;
    rb->collision_mask = mask;

    uint32_t eid = static_cast<uint32_t>(entity);
    auto it = impl_->entity_to_body.find(eid);
    if (it != impl_->entity_to_body.end()) {
        impl_->body_layer_map[it->second.GetIndex()] = { layer, mask };
    }
}

// ---------------------------------------------------------------------------
// Joint 同步 — 从 Joint3DComponent 创建 Jolt Constraint
// ---------------------------------------------------------------------------
void Physics3DSystem::SyncJoints(World& world) {
    if (!impl_) return;
    auto& bi = impl_->physics_system->GetBodyInterface();

    auto view = world.registry().view<Joint3DComponent>();
    for (auto entity : view) {
        auto& jc = view.get<Joint3DComponent>(entity);
        if (jc.runtime_joint || jc.is_broken) continue;

        // 获取 body A
        auto* rb_a = world.registry().try_get<RigidBody3DComponent>(entity);
        if (!rb_a) continue;
        uint32_t eid_a = static_cast<uint32_t>(entity);
        auto it_a = impl_->entity_to_body.find(eid_a);
        if (it_a == impl_->entity_to_body.end()) continue;

        // 获取 body B（0 = 世界锚点，Jolt 中传 Body::sFixedToWorld）
        BodyID body_b_id;
        bool use_fixed = true;
        if (jc.connected_entity_id != 0) {
            entt::entity other = static_cast<entt::entity>(jc.connected_entity_id);
            if (!world.registry().valid(other)) continue;
            auto* rb_b = world.registry().try_get<RigidBody3DComponent>(other);
            if (!rb_b) continue;
            uint32_t eid_b = static_cast<uint32_t>(other);
            auto it_b = impl_->entity_to_body.find(eid_b);
            if (it_b == impl_->entity_to_body.end()) continue;
            body_b_id = it_b->second;
            use_fixed = false;
        }

        BodyID body_a_id = it_a->second;
        Constraint* constraint = nullptr;

        // 辅助 lambda：双体用 BodyInterface，世界锚点用 BodyLockWrite + sFixedToWorld
        auto CreateTwoBody = [&](TwoBodyConstraintSettings& settings) -> Constraint* {
            if (!use_fixed) {
                return bi.CreateConstraint(&settings, body_a_id, body_b_id);
            }
            BodyLockWrite lock(impl_->physics_system->GetBodyLockInterface(), body_a_id);
            if (!lock.Succeeded()) return nullptr;
            return settings.Create(lock.GetBody(), Body::sFixedToWorld);
        };

        switch (jc.type) {
            case Joint3DType::Fixed: {
                FixedConstraintSettings settings;
                settings.mAutoDetectPoint = true;
                constraint = CreateTwoBody(settings);
                break;
            }
            case Joint3DType::Hinge: {
                HingeConstraintSettings settings;
                settings.mPoint1 = settings.mPoint2 = RVec3(
                    jc.anchor.x, jc.anchor.y, jc.anchor.z);
                settings.mHingeAxis1 = settings.mHingeAxis2 = Vec3(
                    jc.axis.x, jc.axis.y, jc.axis.z);
                settings.mNormalAxis1 = settings.mNormalAxis2 =
                    settings.mHingeAxis1.GetNormalizedPerpendicular();
                if (jc.use_limits) {
                    float lower_rad = jc.lower_limit * (JPH_PI / 180.0f);
                    float upper_rad = jc.upper_limit * (JPH_PI / 180.0f);
                    settings.mLimitsMin = lower_rad;
                    settings.mLimitsMax = upper_rad;
                }
                constraint = CreateTwoBody(settings);
                break;
            }
            case Joint3DType::Distance: {
                DistanceConstraintSettings settings;
                settings.mPoint1 = RVec3(jc.anchor.x, jc.anchor.y, jc.anchor.z);
                settings.mPoint2 = RVec3(jc.connected_anchor.x, jc.connected_anchor.y, jc.connected_anchor.z);
                settings.mMinDistance = jc.min_distance;
                settings.mMaxDistance = jc.max_distance;
                constraint = CreateTwoBody(settings);
                break;
            }
            case Joint3DType::Spring: {
                SixDOFConstraintSettings settings;
                for (int ax = 0; ax < 3; ++ax) {
                    settings.MakeFreeAxis(static_cast<SixDOFConstraintSettings::EAxis>(ax));
                    settings.mMotorSettings[ax] = MotorSettings(
                        jc.spring_stiffness, jc.spring_damping);
                }
                for (int ax = 3; ax < 6; ++ax)
                    settings.MakeFixedAxis(static_cast<SixDOFConstraintSettings::EAxis>(ax));
                constraint = CreateTwoBody(settings);
                if (constraint) {
                    auto* c6 = static_cast<SixDOFConstraint*>(constraint);
                    for (int ax = 0; ax < 3; ++ax)
                        c6->SetMotorState(static_cast<SixDOFConstraintSettings::EAxis>(ax),
                            EMotorState::Position);
                }
                break;
            }
        }

        if (constraint) {
            impl_->physics_system->AddConstraint(constraint);
            jc.runtime_joint = constraint;
            uint32_t eid = static_cast<uint32_t>(entity);
            impl_->entity_to_constraint[eid] = constraint;
        }
    }
}

// ---------------------------------------------------------------------------
// 断裂检测
// ---------------------------------------------------------------------------
void Physics3DSystem::CheckBrokenJoints(World& world) {
    if (!impl_) return;

    auto view = world.registry().view<Joint3DComponent>();
    for (auto entity : view) {
        auto& jc = view.get<Joint3DComponent>(entity);
        if (!jc.runtime_joint || jc.is_broken) continue;

        // Jolt 没有内置断裂标志，通过 mass × Δv 估算约束力
        if (jc.break_force < FLT_MAX || jc.break_torque < FLT_MAX) {
            uint32_t eid = static_cast<uint32_t>(entity);
            auto it_a = impl_->entity_to_body.find(eid);
            if (it_a == impl_->entity_to_body.end()) continue;

            auto& bi = impl_->physics_system->GetBodyInterface();
            Vec3 vel_a = bi.GetLinearVelocity(it_a->second);

            float mass_a = 1.0f;
            {
                BodyLockRead lock_a(impl_->physics_system->GetBodyLockInterface(), it_a->second);
                if (lock_a.Succeeded() && lock_a.GetBody().GetMotionProperties())
                    mass_a = 1.0f / std::max(lock_a.GetBody().GetMotionProperties()->GetInverseMass(), 1e-6f);
            }

            Vec3 vel_b = Vec3::sZero();
            float mass_b = 0.0f;
            if (jc.connected_entity_id != 0) {
                auto it_b = impl_->entity_to_body.find(jc.connected_entity_id);
                if (it_b != impl_->entity_to_body.end()) {
                    vel_b = bi.GetLinearVelocity(it_b->second);
                    BodyLockRead lock_b(impl_->physics_system->GetBodyLockInterface(), it_b->second);
                    if (lock_b.Succeeded() && lock_b.GetBody().GetMotionProperties())
                        mass_b = 1.0f / std::max(lock_b.GetBody().GetMotionProperties()->GetInverseMass(), 1e-6f);
                }
            }

            // F ≈ reduced_mass × |Δv| / dt（用固定 dt=1/60s）
            float reduced_mass = (mass_b > 0.0f) ? (mass_a * mass_b) / (mass_a + mass_b) : mass_a;
            float force_estimate = reduced_mass * (vel_a - vel_b).Length() * 60.0f;

            if (force_estimate > jc.break_force) {
                auto* constraint = static_cast<Constraint*>(jc.runtime_joint);
                impl_->physics_system->RemoveConstraint(constraint);
                impl_->entity_to_constraint.erase(eid);
                jc.runtime_joint = nullptr;
                jc.is_broken = true;
                DEBUG_LOG_INFO("[Physics3D-Jolt] Joint on entity {} broken (force={:.1f} > threshold={:.1f})",
                    eid, force_estimate, jc.break_force);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// MeshCollider 辅助函数
// ---------------------------------------------------------------------------
JPH::Shape* Physics3DSystem::CreateConvexHullShape(const std::vector<glm::vec3>& vertices, const std::string& mesh_path) {
    if (vertices.empty() || !impl_) return nullptr;

    // 检查缓存
    if (!mesh_path.empty()) {
        auto it = impl_->convex_mesh_cache.find(mesh_path);
        if (it != impl_->convex_mesh_cache.end()) {
            return const_cast<JPH::Shape*>(it->second.GetPtr());
        }
    }

    // 转换为 Jolt Vec3
    Array<Vec3> jolt_verts;
    jolt_verts.reserve(vertices.size());
    for (const auto& v : vertices) {
        jolt_verts.push_back(Vec3(v.x, v.y, v.z));
    }

    // 创建 convex hull
    ConvexHullShapeSettings settings(jolt_verts);
    settings.mMaxConvexRadius = 0.05f; // 默认凸半径
    Result<Ref<Shape>> result = settings.Create();
    if (result.IsValid()) {
        ShapeRefC shape = result.Get();
        if (!mesh_path.empty()) {
            impl_->convex_mesh_cache[mesh_path] = shape;
        }
        return const_cast<JPH::Shape*>(shape.GetPtr());
    }

    DEBUG_LOG_ERROR("[Physics3D-Jolt] Failed to create convex hull for mesh: {}", mesh_path);
    return nullptr;
}

JPH::Shape* Physics3DSystem::CreateTriangleMeshShape(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, const std::string& mesh_path) {
    if (vertices.empty() || indices.empty() || !impl_) return nullptr;

    // 检查缓存
    if (!mesh_path.empty()) {
        auto it = impl_->triangle_mesh_cache.find(mesh_path);
        if (it != impl_->triangle_mesh_cache.end()) {
            return const_cast<JPH::Shape*>(it->second.GetPtr());
        }
    }

    // 转换为 Jolt 格式 - 使用 Array<Float3> 作为 VertexList
    Array<Float3> jolt_verts;
    jolt_verts.reserve(vertices.size());
    for (const auto& v : vertices) {
        jolt_verts.push_back(Float3(v.x, v.y, v.z));
    }

    Array<IndexedTriangle> jolt_indices;
    jolt_indices.reserve(indices.size() / 3);
    for (size_t i = 0; i < indices.size(); i += 3) {
        jolt_indices.push_back(IndexedTriangle(
            static_cast<uint32>(indices[i]),
            static_cast<uint32>(indices[i + 1]),
            static_cast<uint32>(indices[i + 2])
        ));
    }

    // 创建 triangle mesh
    MeshShapeSettings settings(jolt_verts, jolt_indices);
    settings.mMaxTrianglesPerLeaf = 4; // 平衡性能和内存
    Result<Ref<Shape>> result = settings.Create();
    if (result.IsValid()) {
        ShapeRefC shape = result.Get();
        if (!mesh_path.empty()) {
            impl_->triangle_mesh_cache[mesh_path] = shape;
        }
        return const_cast<JPH::Shape*>(shape.GetPtr());
    }

    DEBUG_LOG_ERROR("[Physics3D-Jolt] Failed to create triangle mesh for mesh: {}", mesh_path);
    return nullptr;
}

} // namespace physics3d
} // namespace dse

#endif // DSE_ENABLE_JOLT
