#include "modules/gameplay_3d/ragdoll/ragdoll_system.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include "engine/base/debug.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <cstring>
#include <cmath>
#include <algorithm>

#include "engine/physics/physics3d/i_physics3d_system.h"
#ifdef DSE_ENABLE_PHYSX
#include <PxPhysicsAPI.h>
#define DSE_HAS_PHYSX_EXTENSIONS 1
#ifdef DSE_HAS_PHYSX_EXTENSIONS
#include <extensions/PxD6Joint.h>
#endif
using namespace physx;
#endif

namespace { constexpr float kPi = 3.14159265358979323846f; }

namespace dse {
namespace gameplay3d {

void RagdollSystem::SetAssetManager(AssetManager* asset_manager) { asset_manager_ = asset_manager; }
void RagdollSystem::SetPhysics3D(physics3d::IPhysics3DSystem* physics3d) { physics3d_ = physics3d; }

void RagdollSystem::FixedUpdate(World& world, float dt) {
    (void)dt;
    auto view = world.registry().view<RagdollComponent>();
    for (auto entity : view) {
        auto& ragdoll = view.get<RagdollComponent>(entity);
        if (!ragdoll.active) continue;

        // 确保物理体已创建
        if (!ragdoll.initialized) {
            if (ragdoll.auto_setup && ragdoll.bone_setups.empty()) {
                AutoSetupBones(world, entity, ragdoll);
            }
            if (!ragdoll.bone_setups.empty()) {
                CreatePhysicsBodies(world, entity, ragdoll);
            }
        }

        if (ragdoll.initialized) {
            SyncBonesFromPhysics(world, entity, ragdoll);
        }
    }
}

void RagdollSystem::Activate(World& world, entt::entity entity,
                              const glm::vec3& impulse, const glm::vec3& impulse_point) {
    auto* ragdoll = world.registry().try_get<RagdollComponent>(entity);
    if (!ragdoll || ragdoll->active) return;

    ragdoll->active = true;

    // 禁用动画
    auto* animator = world.registry().try_get<Animator3DComponent>(entity);
    if (animator) animator->enabled = false;

    // 自动配置
    if (ragdoll->auto_setup && ragdoll->bone_setups.empty()) {
        AutoSetupBones(world, entity, *ragdoll);
    }
    if (!ragdoll->bone_setups.empty()) {
        CreatePhysicsBodies(world, entity, *ragdoll);
    }

#ifdef DSE_ENABLE_PHYSX
    // 施加初始冲量
    if (ragdoll->initialized && glm::length(impulse) > 0.001f) {
        float best_dist = FLT_MAX;
        PxRigidDynamic* best_actor = nullptr;
        for (auto& rb : ragdoll->runtime_bones) {
            if (!rb.actor) continue;
            PxRigidDynamic* actor = static_cast<PxRigidDynamic*>(rb.actor);
            PxVec3 pos = actor->getGlobalPose().p;
            float d = glm::length(glm::vec3(pos.x, pos.y, pos.z) - impulse_point);
            if (d < best_dist) {
                best_dist = d;
                best_actor = actor;
            }
        }
        if (best_actor) {
            best_actor->addForce(PxVec3(impulse.x, impulse.y, impulse.z), PxForceMode::eIMPULSE);
        }
    }
#elif defined(DSE_ENABLE_JOLT)
    if (ragdoll->initialized && physics3d_ && glm::length(impulse) > 0.001f) {
        float best_dist = FLT_MAX;
        entt::entity best_entity = entt::null;
        for (auto& rb : ragdoll->runtime_bones) {
            if (rb.bone_entity == entt::null) continue;
            auto* tf = world.registry().try_get<TransformComponent>(rb.bone_entity);
            if (!tf) continue;
            float d = glm::length(tf->position - impulse_point);
            if (d < best_dist) {
                best_dist = d;
                best_entity = rb.bone_entity;
            }
        }
        if (best_entity != entt::null) {
            physics3d_->AddImpulse(best_entity, impulse);
        }
    }
#else
    (void)impulse; (void)impulse_point;
#endif

    DEBUG_LOG_INFO("[Ragdoll] Activated entity {} with {} bones", static_cast<uint32_t>(entity), ragdoll->runtime_bones.size());
}

void RagdollSystem::Deactivate(World& world, entt::entity entity) {
    auto* ragdoll = world.registry().try_get<RagdollComponent>(entity);
    if (!ragdoll || !ragdoll->active) return;

#ifdef DSE_ENABLE_JOLT
    DestroyPhysicsBodiesJolt(world, *ragdoll);
#else
    DestroyPhysicsBodies(*ragdoll);
#endif
    ragdoll->active = false;

    // 重新启用动画
    auto* animator = world.registry().try_get<Animator3DComponent>(entity);
    if (animator) animator->enabled = true;

    DEBUG_LOG_INFO("[Ragdoll] Deactivated entity {}", static_cast<uint32_t>(entity));
}

void RagdollSystem::AutoSetupBones(World& world, entt::entity entity, RagdollComponent& ragdoll) {
    if (!asset_manager_) return;

    auto* animator = world.registry().try_get<Animator3DComponent>(entity);
    if (!animator || animator->dskel_path.empty()) return;

    auto dskel = asset_manager_->LoadDskel(animator->dskel_path);
    if (!dskel || dskel->GetData().empty()) return;

    const uint8_t* skel_data = dskel->GetData().data();
    const auto* header = reinterpret_cast<const asset::compiler::SkelHeader*>(skel_data);
    if (header->magic[0] != 'D' || header->magic[1] != 'S' || header->magic[2] != 'E' || header->magic[3] != 'S') return;

    const auto* bones = reinterpret_cast<const asset::compiler::BoneDesc*>(skel_data + sizeof(asset::compiler::SkelHeader));
    uint32_t bone_count = std::min(header->bone_count, static_cast<uint32_t>(MAX_BONES));

    // 计算 bind-pose 全局变换
    std::vector<glm::mat4> bind_globals(bone_count);
    std::vector<bool> computed(bone_count, false);
    for (uint32_t i = 0; i < bone_count; ++i) {
        int pi = bones[i].parent_index;
        if (pi < 0 || pi >= static_cast<int>(bone_count)) {
            bind_globals[i] = bones[i].local_transform;
            computed[i] = true;
        }
    }
    for (uint32_t pass = 0; pass < bone_count; ++pass) {
        bool all_done = true;
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (computed[i]) continue;
            int pi = bones[i].parent_index;
            if (computed[pi]) { bind_globals[i] = bind_globals[pi] * bones[i].local_transform; computed[i] = true; }
            else all_done = false;
        }
        if (all_done) break;
    }

    // 构建骨骼→setup 映射：为每个骨骼创建配置
    // 简化策略：只为有子骨骼的骨骼创建刚体（跳过叶节点以减少刚体数量）
    std::vector<int> child_count(bone_count, 0);
    for (uint32_t i = 0; i < bone_count; ++i) {
        int pi = bones[i].parent_index;
        if (pi >= 0 && pi < static_cast<int>(bone_count)) child_count[pi]++;
    }

    std::vector<int> bone_to_setup(bone_count, -1);
    ragdoll.bone_setups.clear();

    for (uint32_t i = 0; i < bone_count; ++i) {
        // 跳过没有子骨骼的叶节点（除非骨骼总数很少）
        if (child_count[i] == 0 && bone_count > 10) continue;

        RagdollBoneSetup setup;
        setup.bone_index = static_cast<int>(i);

        // 计算骨骼长度：到第一个子骨骼的距离
        float bone_length = 0.1f;
        for (uint32_t j = 0; j < bone_count; ++j) {
            if (bones[j].parent_index == static_cast<int>(i)) {
                glm::vec3 parent_pos = glm::vec3(bind_globals[i][3]);
                glm::vec3 child_pos = glm::vec3(bind_globals[j][3]);
                float len = glm::length(child_pos - parent_pos);
                if (len > bone_length) bone_length = len;
            }
        }

        setup.height = std::max(0.02f, bone_length * 0.6f);
        setup.radius = std::max(0.01f, bone_length * 0.15f);
        setup.mass = ragdoll.total_mass / static_cast<float>(bone_count);

        // 父 setup 索引
        int pi = bones[i].parent_index;
        setup.parent_setup_index = (pi >= 0 && pi < static_cast<int>(bone_count)) ? bone_to_setup[pi] : -1;

        bone_to_setup[i] = static_cast<int>(ragdoll.bone_setups.size());
        ragdoll.bone_setups.push_back(setup);
    }

    DEBUG_LOG_INFO("[Ragdoll] AutoSetup: {} setups from {} bones", ragdoll.bone_setups.size(), bone_count);
}

void RagdollSystem::CreatePhysicsBodies(World& world, entt::entity entity, RagdollComponent& ragdoll) {
#if defined(DSE_ENABLE_PHYSX)
    if (ragdoll.initialized) return;

    auto* animator = world.registry().try_get<Animator3DComponent>(entity);
    auto* transform = world.registry().try_get<TransformComponent>(entity);
    if (!animator || !transform || animator->dskel_path.empty() || !asset_manager_) return;

    auto dskel = asset_manager_->LoadDskel(animator->dskel_path);
    if (!dskel || dskel->GetData().empty()) return;

    const uint8_t* skel_data = dskel->GetData().data();
    const auto* header = reinterpret_cast<const asset::compiler::SkelHeader*>(skel_data);
    const auto* bones = reinterpret_cast<const asset::compiler::BoneDesc*>(skel_data + sizeof(asset::compiler::SkelHeader));
    uint32_t bone_count = std::min(header->bone_count, static_cast<uint32_t>(MAX_BONES));

    // 计算 bind-pose 全局变换
    std::vector<glm::mat4> bind_globals(bone_count);
    std::vector<bool> computed(bone_count, false);
    for (uint32_t i = 0; i < bone_count; ++i) {
        int pi = bones[i].parent_index;
        if (pi < 0 || pi >= static_cast<int>(bone_count)) {
            bind_globals[i] = bones[i].local_transform;
            computed[i] = true;
        }
    }
    for (uint32_t pass = 0; pass < bone_count; ++pass) {
        bool all_done = true;
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (computed[i]) continue;
            int pi = bones[i].parent_index;
            if (computed[pi]) { bind_globals[i] = bind_globals[pi] * bones[i].local_transform; computed[i] = true; }
            else all_done = false;
        }
        if (all_done) break;
    }

    // 获取 PhysX 场景（通过 physics3d_system 的内部指针）
    // 由于 Physics3DSystem 没有直接暴露 PxScene*，我们通过创建临时 RigidBody3D 来间接使用
    // 但更好的方式是让 physics3d_system 暴露 PxPhysics* 和 PxScene*
    // 这里用一个简化的方法：直接获取全局 PxPhysics/PxScene
    // 实际上 physics3d_system.cpp 里有全局 g_allocator 等，但不可直接访问
    // 我们使用 PxGetPhysics() 从已初始化的 PhysX SDK 获取实例

    PxPhysics* physics = nullptr;
    PxScene* scene = nullptr;

    // 通过 PxGetPhysics 获取已创建的 PxPhysics 实例
    // PhysX 4.1 没有 PxGetPhysics，需要从 Physics3DSystem 获取
    // 这里我们通过 Physics3DSystem::Init 中创建的 foundation/physics/scene
    // 但它们是 private 的...为了不修改 physics3d_system.h 太多，
    // 我们使用一个 workaround：创建 RigidBody3D + Collider 让 physics3d 自动管理
    // 但 ragdoll 需要精确控制每个骨骼的刚体位置

    // 实际方案：在 physics3d_system.h 中暴露 GetPhysics/GetScene 接口
    // 这需要修改 physics3d_system.h — 先做这个修改

    // 我们假设已有 GetPxPhysics()/GetPxScene() — 将在下面修改 physics3d_system.h 添加

    if (!physics3d_) return;

    // 使用 Physics3DSystem 暴露的 PhysX 指针
    physics = static_cast<PxPhysics*>(physics3d_->GetPxPhysics());
    scene = static_cast<PxScene*>(physics3d_->GetPxScene());
    if (!physics || !scene) return;

    PxMaterial* material = physics->createMaterial(0.6f, 0.6f, 0.3f);

    // 实体世界变换
    glm::mat4 entity_world = glm::translate(glm::mat4(1.0f), transform->position)
                            * glm::mat4_cast(transform->rotation)
                            * glm::scale(glm::mat4(1.0f), transform->scale);

    ragdoll.runtime_bones.resize(ragdoll.bone_setups.size());

    for (size_t si = 0; si < ragdoll.bone_setups.size(); ++si) {
        const auto& setup = ragdoll.bone_setups[si];
        if (setup.bone_index < 0 || setup.bone_index >= static_cast<int>(bone_count)) continue;

        // 骨骼全局位置
        glm::mat4 bone_world = entity_world * bind_globals[setup.bone_index];
        glm::vec3 bone_pos = glm::vec3(bone_world[3]);
        glm::quat bone_rot = glm::quat_cast(bone_world);

        PxTransform px_transform(
            PxVec3(bone_pos.x, bone_pos.y, bone_pos.z),
            PxQuat(bone_rot.x, bone_rot.y, bone_rot.z, bone_rot.w));

        PxRigidDynamic* actor = physics->createRigidDynamic(px_transform);
        actor->setMass(setup.mass);
        actor->setLinearDamping(0.5f);
        actor->setAngularDamping(0.8f);
        actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);

        // 胶囊 shape（Y 轴方向）
        PxCapsuleGeometry capsule(setup.radius, setup.height * 0.5f);
        PxShape* shape = physics->createShape(capsule, *material);
        // PhysX 胶囊默认沿 X 轴，旋转到 Y 轴
        shape->setLocalPose(PxTransform(
            PxVec3(setup.offset.x, setup.offset.y, setup.offset.z),
            PxQuat(kPi * 0.5f, PxVec3(0, 0, 1))));

        // 碰撞过滤
        PxFilterData fd;
        fd.word0 = ragdoll.collision_layer;
        fd.word1 = ragdoll.collision_mask;
        shape->setSimulationFilterData(fd);
        shape->setQueryFilterData(fd);

        actor->attachShape(*shape);
        shape->release();

        PxRigidBodyExt::updateMassAndInertia(*actor, 10.0f);

        actor->userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entity));
        scene->addActor(*actor);

        ragdoll.runtime_bones[si].actor = actor;
        ragdoll.runtime_bones[si].bone_index = setup.bone_index;

        // 创建关节连接到父骨骼
#ifdef DSE_HAS_PHYSX_EXTENSIONS
        if (setup.parent_setup_index >= 0 && setup.parent_setup_index < static_cast<int>(si)) {
            auto* parent_actor = static_cast<PxRigidDynamic*>(ragdoll.runtime_bones[setup.parent_setup_index].actor);
            if (parent_actor) {
                PxTransform local_frame_parent(PxIdentity);
                PxTransform local_frame_child(PxIdentity);

                // 关节帧：父骨骼位置到子骨骼位置的相对变换
                PxVec3 parent_pos = parent_actor->getGlobalPose().p;
                PxVec3 child_pos = actor->getGlobalPose().p;
                PxVec3 mid = (parent_pos + child_pos) * 0.5f;

                local_frame_parent.p = parent_actor->getGlobalPose().transformInv(mid);
                local_frame_child.p = actor->getGlobalPose().transformInv(mid);

                PxD6Joint* joint = PxD6JointCreate(*physics, parent_actor, local_frame_parent, actor, local_frame_child);
                if (joint) {
                    // 锁定线性自由度
                    joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
                    joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
                    joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);

                    // 限制旋转自由度
                    float swing_y_rad = setup.swing_limit_y * (kPi / 180.0f);
                    float swing_z_rad = setup.swing_limit_z * (kPi / 180.0f);
                    float twist_rad = setup.twist_limit * (kPi / 180.0f);

                    joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
                    joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);
                    joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);

                    joint->setSwingLimit(PxJointLimitCone(swing_y_rad, swing_z_rad, PxSpring(ragdoll.joint_stiffness, ragdoll.joint_damping)));
                    joint->setTwistLimit(PxJointAngularLimitPair(-twist_rad, twist_rad, PxSpring(ragdoll.joint_stiffness, ragdoll.joint_damping)));

                    ragdoll.runtime_bones[si].joint = joint;
                }
            }
        }
#endif
    }

    if (material) material->release();

    ragdoll.initialized = true;
    DEBUG_LOG_INFO("[Ragdoll] Created {} physics bodies for entity {}", ragdoll.runtime_bones.size(), static_cast<uint32_t>(entity));
#elif defined(DSE_ENABLE_JOLT)
    if (ragdoll.initialized) return;

    auto* animator = world.registry().try_get<Animator3DComponent>(entity);
    auto* transform = world.registry().try_get<TransformComponent>(entity);
    if (!animator || !transform || animator->dskel_path.empty() || !asset_manager_) return;

    auto dskel = asset_manager_->LoadDskel(animator->dskel_path);
    if (!dskel || dskel->GetData().empty()) return;

    const uint8_t* skel_data = dskel->GetData().data();
    const auto* header = reinterpret_cast<const asset::compiler::SkelHeader*>(skel_data);
    const auto* bones_data = reinterpret_cast<const asset::compiler::BoneDesc*>(skel_data + sizeof(asset::compiler::SkelHeader));
    uint32_t bone_count = std::min(header->bone_count, static_cast<uint32_t>(MAX_BONES));

    std::vector<glm::mat4> bind_globals(bone_count);
    std::vector<bool> computed(bone_count, false);
    for (uint32_t i = 0; i < bone_count; ++i) {
        int pi = bones_data[i].parent_index;
        if (pi < 0 || pi >= static_cast<int>(bone_count)) {
            bind_globals[i] = bones_data[i].local_transform;
            computed[i] = true;
        }
    }
    for (uint32_t pass = 0; pass < bone_count; ++pass) {
        bool all_done = true;
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (computed[i]) continue;
            int pi = bones_data[i].parent_index;
            if (computed[pi]) { bind_globals[i] = bind_globals[pi] * bones_data[i].local_transform; computed[i] = true; }
            else all_done = false;
        }
        if (all_done) break;
    }

    glm::mat4 entity_world = glm::translate(glm::mat4(1.0f), transform->position)
                            * glm::mat4_cast(transform->rotation)
                            * glm::scale(glm::mat4(1.0f), transform->scale);

    ragdoll.runtime_bones.resize(ragdoll.bone_setups.size());

    for (size_t si = 0; si < ragdoll.bone_setups.size(); ++si) {
        const auto& setup = ragdoll.bone_setups[si];
        if (setup.bone_index < 0 || setup.bone_index >= static_cast<int>(bone_count)) continue;

        glm::mat4 bone_world = entity_world * bind_globals[setup.bone_index];
        glm::vec3 bone_pos = glm::vec3(bone_world[3]);
        glm::quat bone_rot = glm::quat_cast(bone_world);

        // 创建临时 ECS entity 作为骨骼刚体
        entt::entity bone_entity = world.CreateEntity();

        auto& tf = world.registry().emplace<TransformComponent>(bone_entity);
        tf.position = bone_pos;
        tf.rotation = bone_rot;
        tf.scale = glm::vec3(1.0f);

        auto& rb = world.registry().emplace<RigidBody3DComponent>(bone_entity);
        rb.type = RigidBody3DType::Dynamic;
        rb.mass = setup.mass;
        rb.drag = 0.5f;
        rb.angular_drag = 0.8f;
        rb.use_gravity = true;
        rb.collision_layer = ragdoll.collision_layer;
        rb.collision_mask = ragdoll.collision_mask;

        auto& cap = world.registry().emplace<CapsuleCollider3DComponent>(bone_entity);
        cap.radius = setup.radius;
        cap.height = setup.height;

        ragdoll.runtime_bones[si].bone_entity = bone_entity;
        ragdoll.runtime_bones[si].bone_index = setup.bone_index;

        // 创建关节连接到父骨骼
        if (setup.parent_setup_index >= 0 && setup.parent_setup_index < static_cast<int>(si)) {
            entt::entity parent_entity = ragdoll.runtime_bones[setup.parent_setup_index].bone_entity;
            if (parent_entity != entt::null) {
                auto& jc = world.registry().emplace<Joint3DComponent>(bone_entity);
                jc.type = Joint3DType::Spring;
                jc.connected_entity_id = static_cast<uint32_t>(parent_entity);
                jc.anchor = glm::vec3(0.0f);
                jc.connected_anchor = glm::vec3(0.0f);
                jc.spring_stiffness = ragdoll.joint_stiffness > 0.0f ? ragdoll.joint_stiffness : 500.0f;
                jc.spring_damping = ragdoll.joint_damping;
            }
        }
    }

    ragdoll.initialized = true;
    DEBUG_LOG_INFO("[Ragdoll-Jolt] Created {} ECS bone entities for entity {}", ragdoll.runtime_bones.size(), static_cast<uint32_t>(entity));
#else
    (void)world; (void)entity; (void)ragdoll;
#endif
}

void RagdollSystem::DestroyPhysicsBodies(RagdollComponent& ragdoll) {
#ifdef DSE_ENABLE_PHYSX
    for (auto& rb : ragdoll.runtime_bones) {
        if (rb.joint) {
            static_cast<PxJoint*>(rb.joint)->release();
            rb.joint = nullptr;
        }
    }
    for (auto& rb : ragdoll.runtime_bones) {
        if (rb.actor) {
            PxRigidDynamic* actor = static_cast<PxRigidDynamic*>(rb.actor);
            actor->getScene()->removeActor(*actor);
            actor->release();
            rb.actor = nullptr;
        }
    }
#endif
    ragdoll.runtime_bones.clear();
    ragdoll.initialized = false;
}

void RagdollSystem::DestroyPhysicsBodiesJolt(World& world, RagdollComponent& ragdoll) {
    for (auto& rb : ragdoll.runtime_bones) {
        if (rb.bone_entity != entt::null) {
            if (physics3d_) physics3d_->RemoveActor(rb.bone_entity);
            if (world.registry().valid(rb.bone_entity))
                world.registry().destroy(rb.bone_entity);
            rb.bone_entity = entt::null;
        }
    }
    ragdoll.runtime_bones.clear();
    ragdoll.initialized = false;
}

void RagdollSystem::SyncBonesFromPhysics(World& world, entt::entity entity, RagdollComponent& ragdoll) {
#if defined(DSE_ENABLE_PHYSX)
    auto* animator = world.registry().try_get<Animator3DComponent>(entity);
    auto* transform = world.registry().try_get<TransformComponent>(entity);
    if (!animator || !transform) return;
    if (animator->final_bone_matrices.empty()) return;

    // 实体逆世界变换
    glm::mat4 entity_world = glm::translate(glm::mat4(1.0f), transform->position)
                            * glm::mat4_cast(transform->rotation)
                            * glm::scale(glm::mat4(1.0f), transform->scale);
    glm::mat4 inv_entity_world = glm::inverse(entity_world);

    // 加载骨骼 bind globals 用于计算 final = physics_global * inv(bind_global)
    if (!asset_manager_) return;
    auto dskel = asset_manager_->LoadDskel(animator->dskel_path);
    if (!dskel || dskel->GetData().empty()) return;

    const uint8_t* skel_data = dskel->GetData().data();
    const auto* header = reinterpret_cast<const asset::compiler::SkelHeader*>(skel_data);
    const auto* bones = reinterpret_cast<const asset::compiler::BoneDesc*>(skel_data + sizeof(asset::compiler::SkelHeader));
    uint32_t bone_count = std::min(header->bone_count, static_cast<uint32_t>(MAX_BONES));

    std::vector<glm::mat4> bind_globals(bone_count);
    std::vector<bool> computed(bone_count, false);
    for (uint32_t i = 0; i < bone_count; ++i) {
        int pi = bones[i].parent_index;
        if (pi < 0 || pi >= static_cast<int>(bone_count)) {
            bind_globals[i] = bones[i].local_transform;
            computed[i] = true;
        }
    }
    for (uint32_t pass = 0; pass < bone_count; ++pass) {
        bool all_done = true;
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (computed[i]) continue;
            int pi = bones[i].parent_index;
            if (computed[pi]) { bind_globals[i] = bind_globals[pi] * bones[i].local_transform; computed[i] = true; }
            else all_done = false;
        }
        if (all_done) break;
    }

    for (const auto& rb : ragdoll.runtime_bones) {
        if (!rb.actor || rb.bone_index < 0 || rb.bone_index >= static_cast<int>(bone_count)) continue;
        if (rb.bone_index >= static_cast<int>(animator->final_bone_matrices.size())) continue;

        PxRigidDynamic* actor = static_cast<PxRigidDynamic*>(rb.actor);
        PxTransform pose = actor->getGlobalPose();

        // PhysX 世界位姿 → 模型空间
        glm::mat4 physics_world = glm::translate(glm::mat4(1.0f), glm::vec3(pose.p.x, pose.p.y, pose.p.z))
                                * glm::mat4_cast(glm::quat(pose.q.w, pose.q.x, pose.q.y, pose.q.z));

        glm::mat4 physics_local = inv_entity_world * physics_world;

        // final_bone_matrix = physics_local * inv(bind_global)
        animator->final_bone_matrices[rb.bone_index] = physics_local * glm::inverse(bind_globals[rb.bone_index]);
    }
#elif defined(DSE_ENABLE_JOLT)
    auto* animator = world.registry().try_get<Animator3DComponent>(entity);
    auto* transform = world.registry().try_get<TransformComponent>(entity);
    if (!animator || !transform) return;
    if (animator->final_bone_matrices.empty()) return;

    glm::mat4 entity_world = glm::translate(glm::mat4(1.0f), transform->position)
                            * glm::mat4_cast(transform->rotation)
                            * glm::scale(glm::mat4(1.0f), transform->scale);
    glm::mat4 inv_entity_world = glm::inverse(entity_world);

    if (!asset_manager_) return;
    auto dskel = asset_manager_->LoadDskel(animator->dskel_path);
    if (!dskel || dskel->GetData().empty()) return;

    const uint8_t* skel_data = dskel->GetData().data();
    const auto* header = reinterpret_cast<const asset::compiler::SkelHeader*>(skel_data);
    const auto* bones_data = reinterpret_cast<const asset::compiler::BoneDesc*>(skel_data + sizeof(asset::compiler::SkelHeader));
    uint32_t bone_count = std::min(header->bone_count, static_cast<uint32_t>(MAX_BONES));

    std::vector<glm::mat4> bind_globals(bone_count);
    std::vector<bool> computed(bone_count, false);
    for (uint32_t i = 0; i < bone_count; ++i) {
        int pi = bones_data[i].parent_index;
        if (pi < 0 || pi >= static_cast<int>(bone_count)) {
            bind_globals[i] = bones_data[i].local_transform;
            computed[i] = true;
        }
    }
    for (uint32_t pass = 0; pass < bone_count; ++pass) {
        bool all_done = true;
        for (uint32_t i = 0; i < bone_count; ++i) {
            if (computed[i]) continue;
            int pi = bones_data[i].parent_index;
            if (computed[pi]) { bind_globals[i] = bind_globals[pi] * bones_data[i].local_transform; computed[i] = true; }
            else all_done = false;
        }
        if (all_done) break;
    }

    for (const auto& rb : ragdoll.runtime_bones) {
        if (rb.bone_entity == entt::null || rb.bone_index < 0 || rb.bone_index >= static_cast<int>(bone_count)) continue;
        if (rb.bone_index >= static_cast<int>(animator->final_bone_matrices.size())) continue;

        auto* bone_tf = world.registry().try_get<TransformComponent>(rb.bone_entity);
        if (!bone_tf) continue;

        glm::mat4 physics_world = glm::translate(glm::mat4(1.0f), bone_tf->position)
                                * glm::mat4_cast(bone_tf->rotation);

        glm::mat4 physics_local = inv_entity_world * physics_world;
        animator->final_bone_matrices[rb.bone_index] = physics_local * glm::inverse(bind_globals[rb.bone_index]);
    }
#else
    (void)world; (void)entity; (void)ragdoll;
#endif
}

} // namespace gameplay3d
} // namespace dse
