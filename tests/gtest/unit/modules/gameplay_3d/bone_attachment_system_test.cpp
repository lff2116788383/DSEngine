/**
 * @file bone_attachment_system_test.cpp
 * @brief 骨骼挂点系统单元测试
 *
 * 覆盖场景：
 * - 空 World 不崩溃
 * - BoneAttachmentComponent 默认值
 * - 恢复公式验证（final_bone_matrices * bind_globals = global）
 * - Update 正确写入 local_to_world
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "modules/gameplay_3d/bone_attachment_system.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace dse;
using namespace gameplay3d;

class BoneAttachmentSystemTest : public ::testing::Test {};

// 测试 骨骼附件系统：空世界不崩溃
TEST_F(BoneAttachmentSystemTest, EmptyWorldDoesNotCrash) {
    World world;
    EXPECT_NO_THROW(BoneAttachmentSystem::Update(world));
}

// 测试 骨骼附件系统：组件默认值
TEST_F(BoneAttachmentSystemTest, ComponentDefaultValues) {
    BoneAttachmentComponent comp;
    EXPECT_TRUE(comp.bone_name.empty());
    EXPECT_EQ(comp.offset_position, glm::vec3(0.0f));
    EXPECT_EQ(comp.offset_rotation, glm::quat(1, 0, 0, 0));
    EXPECT_EQ(comp.offset_scale, glm::vec3(1.0f));
    EXPECT_EQ(comp.cached_bone_index, -1);
    EXPECT_TRUE(comp.index_dirty);
}

// 测试 骨骼附件系统：无效当不崩溃
TEST_F(BoneAttachmentSystemTest, InvalidWhenDoesNotCrash) {
    World world;
    auto attachment = world.CreateEntity();
    world.registry().emplace<TransformComponent>(attachment);
    auto& attach_comp = world.registry().emplace<BoneAttachmentComponent>(attachment);
    attach_comp.target_entity = entt::null;
    attach_comp.bone_name = "RightHand";
    EXPECT_NO_THROW(BoneAttachmentSystem::Update(world));
}

// 测试 骨骼附件系统：不存在当不写入变换
TEST_F(BoneAttachmentSystemTest, DoesNotExistWhenNotWriteTransform) {
    World world;
    // 创建目标实体（有 Animator3D + Transform）
    auto character = world.CreateEntity();
    auto& char_xform = world.registry().emplace<TransformComponent>(character);
    char_xform.local_to_world = glm::mat4(1.0f);
    auto& anim = world.registry().emplace<Animator3DComponent>(character);
    anim.enabled = true;
    anim.skel_cache.valid = true;
    anim.skel_cache.bone_count = 1;
    anim.skel_cache.bone_name_to_index["Root"] = 0;
    anim.skel_cache.bind_globals = { glm::mat4(1.0f) };
    anim.final_bone_matrices = { glm::mat4(1.0f) };

    // 创建挂件（骨骼名不匹配）
    auto sword = world.CreateEntity();
    auto& sword_xform = world.registry().emplace<TransformComponent>(sword);
    sword_xform.local_to_world = glm::mat4(999.0f); // sentinel
    auto& attach = world.registry().emplace<BoneAttachmentComponent>(sword);
    attach.target_entity = character;
    attach.bone_name = "NonExistent";

    BoneAttachmentSystem::Update(world);

    // Transform 不应被修改
    EXPECT_EQ(sword_xform.local_to_world[0][0], 999.0f);
}

// 测试 骨骼附件系统：正确
TEST_F(BoneAttachmentSystemTest, Correct) {
    World world;
    // 创建目标实体
    auto character = world.CreateEntity();
    auto& char_xform = world.registry().emplace<TransformComponent>(character);
    char_xform.local_to_world = glm::translate(glm::mat4(1.0f), glm::vec3(10, 0, 0));

    auto& anim = world.registry().emplace<Animator3DComponent>(character);
    anim.enabled = true;
    anim.skel_cache.valid = true;
    anim.skel_cache.bone_count = 2;
    anim.skel_cache.bone_name_to_index["Root"] = 0;
    anim.skel_cache.bone_name_to_index["RightHand"] = 1;

    // bind_globals: 骨骼的绑定姿态全局矩阵
    glm::mat4 hand_bind_global = glm::translate(glm::mat4(1.0f), glm::vec3(0, 1, 0));
    anim.skel_cache.bind_globals = { glm::mat4(1.0f), hand_bind_global };

    // 模拟动画后 final_bone_matrices:
    // final = global * inv(bind_global)
    // 假设动画后 hand 在 model space 的 global = translate(0, 2, 0)
    glm::mat4 hand_global_anim = glm::translate(glm::mat4(1.0f), glm::vec3(0, 2, 0));
    glm::mat4 hand_final = hand_global_anim * glm::inverse(hand_bind_global);
    anim.final_bone_matrices = { glm::mat4(1.0f), hand_final };

    // 创建挂件
    auto sword = world.CreateEntity();
    auto& sword_xform = world.registry().emplace<TransformComponent>(sword);
    sword_xform.local_to_world = glm::mat4(1.0f);
    auto& attach = world.registry().emplace<BoneAttachmentComponent>(sword);
    attach.target_entity = character;
    attach.bone_name = "RightHand";

    BoneAttachmentSystem::Update(world);

    // 期望: sword world = char.local_to_world * hand_global_anim * offset(identity)
    // = translate(10,0,0) * translate(0,2,0) = translate(10,2,0)
    glm::vec3 result_pos = glm::vec3(sword_xform.local_to_world * glm::vec4(0, 0, 0, 1));
    EXPECT_NEAR(result_pos.x, 10.0f, 1e-4f);
    EXPECT_NEAR(result_pos.y, 2.0f, 1e-4f);
    EXPECT_NEAR(result_pos.z, 0.0f, 1e-4f);
}

// 测试 骨骼附件系统：偏移正确
TEST_F(BoneAttachmentSystemTest, OffsetCorrect) {
    World world;
    auto character = world.CreateEntity();
    auto& char_xform = world.registry().emplace<TransformComponent>(character);
    char_xform.local_to_world = glm::mat4(1.0f); // 原点

    auto& anim = world.registry().emplace<Animator3DComponent>(character);
    anim.enabled = true;
    anim.skel_cache.valid = true;
    anim.skel_cache.bone_count = 1;
    anim.skel_cache.bone_name_to_index["Hand"] = 0;
    anim.skel_cache.bind_globals = { glm::mat4(1.0f) };
    // bone global = identity (bind pose)
    anim.final_bone_matrices = { glm::mat4(1.0f) }; // final = global * inv(bind) = I * I = I

    auto sword = world.CreateEntity();
    auto& sword_xform = world.registry().emplace<TransformComponent>(sword);
    auto& attach = world.registry().emplace<BoneAttachmentComponent>(sword);
    attach.target_entity = character;
    attach.bone_name = "Hand";
    attach.offset_position = glm::vec3(0, 0, -0.5f); // 武器手柄偏移

    BoneAttachmentSystem::Update(world);

    glm::vec3 result_pos = glm::vec3(sword_xform.local_to_world * glm::vec4(0, 0, 0, 1));
    EXPECT_NEAR(result_pos.x, 0.0f, 1e-4f);
    EXPECT_NEAR(result_pos.y, 0.0f, 1e-4f);
    EXPECT_NEAR(result_pos.z, -0.5f, 1e-4f);
}

// 测试 骨骼附件系统：触发
TEST_F(BoneAttachmentSystemTest, Triggers) {
    World world;
    auto character = world.CreateEntity();
    auto& char_xform = world.registry().emplace<TransformComponent>(character);
    char_xform.local_to_world = glm::mat4(1.0f);

    auto& anim = world.registry().emplace<Animator3DComponent>(character);
    anim.enabled = true;
    anim.skel_cache.valid = true;
    anim.skel_cache.bone_count = 2;
    anim.skel_cache.bone_name_to_index["BoneA"] = 0;
    anim.skel_cache.bone_name_to_index["BoneB"] = 1;
    anim.skel_cache.bind_globals = { glm::mat4(1.0f), glm::mat4(1.0f) };
    glm::mat4 globalA = glm::translate(glm::mat4(1.0f), glm::vec3(1, 0, 0));
    glm::mat4 globalB = glm::translate(glm::mat4(1.0f), glm::vec3(0, 5, 0));
    anim.final_bone_matrices = { globalA, globalB }; // bind = I, so final = global

    auto obj = world.CreateEntity();
    auto& obj_xform = world.registry().emplace<TransformComponent>(obj);
    auto& attach = world.registry().emplace<BoneAttachmentComponent>(obj);
    attach.target_entity = character;
    attach.bone_name = "BoneA";

    BoneAttachmentSystem::Update(world);
    glm::vec3 pos1 = glm::vec3(obj_xform.local_to_world * glm::vec4(0, 0, 0, 1));
    EXPECT_NEAR(pos1.x, 1.0f, 1e-4f);

    // 切换到 BoneB
    attach.bone_name = "BoneB";
    attach.index_dirty = true;
    BoneAttachmentSystem::Update(world);
    glm::vec3 pos2 = glm::vec3(obj_xform.local_to_world * glm::vec4(0, 0, 0, 1));
    EXPECT_NEAR(pos2.y, 5.0f, 1e-4f);
}
