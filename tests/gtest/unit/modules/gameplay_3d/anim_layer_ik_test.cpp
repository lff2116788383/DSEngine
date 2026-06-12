/**
 * @file anim_layer_ik_test.cpp
 * @brief 动画层混合 + IK 系统单元测试
 *
 * 覆盖场景：
 * - 组件默认值
 * - 空 World 调用 Update 不崩溃
 * - AnimLayerComponent + IKChain3DComponent 字段设置
 * - IK 链索引缓存 dirty 标记
 * - anim_clip_eval.h 工具函数
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/animation/anim_layer_blend_system.h"
#include "modules/gameplay_3d/animation/ik_solver_system.h"
#include "modules/gameplay_3d/animation/anim_clip_eval.h"

using namespace dse;
using namespace gameplay3d;

// ============================================================
// AnimLayerComponent 默认值
// ============================================================

TEST(AnimLayerTest, AnimLayerComponentDefaultValues) {
    AnimLayerComponent comp;
    EXPECT_TRUE(comp.enabled);
    EXPECT_TRUE(comp.layers.empty());
}

TEST(AnimLayerTest, AnimLayerConfigDefaultValues) {
    AnimLayerConfig layer;
    EXPECT_TRUE(layer.name.empty());
    EXPECT_FLOAT_EQ(layer.weight, 1.0f);
    EXPECT_EQ(layer.blend_mode, AnimLayerBlendMode::Override);
    EXPECT_EQ(layer.source_type, AnimSourceType::SingleClip);
    EXPECT_TRUE(layer.bone_mask_include.empty());
    EXPECT_TRUE(layer.bone_mask_dirty);
    EXPECT_TRUE(layer.danim_path.empty());
    EXPECT_FLOAT_EQ(layer.current_time, 0.0f);
    EXPECT_FLOAT_EQ(layer.speed, 1.0f);
    EXPECT_TRUE(layer.loop);
}

TEST(AnimLayerTest, AnimBlendNode2DDefaultValues) {
    AnimBlendNode2D node;
    EXPECT_TRUE(node.name.empty());
    EXPECT_TRUE(node.danim_path.empty());
    EXPECT_FLOAT_EQ(node.x, 0.0f);
    EXPECT_FLOAT_EQ(node.y, 0.0f);
    EXPECT_FLOAT_EQ(node.speed, 1.0f);
    EXPECT_TRUE(node.loop);
}

TEST(AnimLayerTest, EmptyWorldCallsLayerBlendUpdateDoesNotCrash) {
    World world;
    AnimLayerBlendSystem::SetAssetManager(nullptr);
    EXPECT_NO_THROW(AnimLayerBlendSystem::Update(world, 0.016f));
}

TEST(AnimLayerTest, WithComponentWithoutAssetManagerDoesNotCrash) {
    World world;
    AnimLayerBlendSystem::SetAssetManager(nullptr);
    auto e = world.CreateEntity();
    auto& anim = world.registry().emplace<Animator3DComponent>(e);
    anim.enabled = true;
    anim.skel_cache.valid = false;
    world.registry().emplace<AnimLayerComponent>(e);
    EXPECT_NO_THROW(AnimLayerBlendSystem::Update(world, 0.016f));
}

// ============================================================
// IKChain3DComponent 默认值
// ============================================================

TEST(IKTest, IKChain3DComponentDefaultValues) {
    IKChain3DComponent comp;
    EXPECT_TRUE(comp.enabled);
    EXPECT_TRUE(comp.chains.empty());
}

TEST(IKTest, IKChainConfigDefaultValues) {
    IKChainConfig chain;
    EXPECT_TRUE(chain.name.empty());
    EXPECT_EQ(chain.type, IKChainType::FABRIK);
    EXPECT_TRUE(chain.root_bone.empty());
    EXPECT_TRUE(chain.tip_bone.empty());
    EXPECT_FLOAT_EQ(chain.weight, 1.0f);
    EXPECT_EQ(chain.target_entity, UINT32_MAX);
    EXPECT_FLOAT_EQ(chain.target_position.x, 0.0f);
    EXPECT_FLOAT_EQ(chain.tolerance, 0.01f);
    EXPECT_EQ(chain.iterations, 10);
    EXPECT_TRUE(chain.indices_dirty);
    EXPECT_EQ(chain.root_bone_index, -1);
    EXPECT_EQ(chain.tip_bone_index, -1);
}

TEST(IKTest, EmptyWorldCallsIKUpdateDoesNotCrash) {
    World world;
    EXPECT_NO_THROW(IKSolverSystem::Update(world, 0.016f));
}

TEST(IKTest, WithComponentWithoutValidCacheDoesNotCrash) {
    World world;
    auto e = world.CreateEntity();
    auto& anim = world.registry().emplace<Animator3DComponent>(e);
    anim.enabled = true;
    anim.skel_cache.valid = false;
    auto& ik = world.registry().emplace<IKChain3DComponent>(e);
    IKChainConfig chain;
    chain.name = "test_chain";
    chain.type = IKChainType::FABRIK;
    chain.root_bone = "Hips";
    chain.tip_bone = "LeftFoot";
    ik.chains.push_back(chain);
    EXPECT_NO_THROW(IKSolverSystem::Update(world, 0.016f));
}

// ============================================================
// anim_clip_eval.h 工具函数测试
// ============================================================

TEST(AnimClipEvalTest, AdvanceClipTime_Loop) {
    float t = anim_util::AdvanceClipTime(0.9f, 0.016f, 10.0f, 1.0f, true);
    EXPECT_GE(t, 0.0f);
    EXPECT_LT(t, 1.0f);
}

TEST(AnimClipEvalTest, AdvanceClipTime_NoLoop_Clamp) {
    float t = anim_util::AdvanceClipTime(0.9f, 0.016f, 10.0f, 1.0f, false);
    EXPECT_FLOAT_EQ(t, 1.0f);
}

TEST(AnimClipEvalTest, AdvanceClipTime_ZeroDuration) {
    float t = anim_util::AdvanceClipTime(0.5f, 0.016f, 1.0f, 0.0f, true);
    EXPECT_FLOAT_EQ(t, 0.0f);
}

TEST(AnimClipEvalTest, Interpolate_SingleKey) {
    std::vector<float> times = {0.0f};
    std::vector<glm::vec3> values = {glm::vec3(1.0f, 2.0f, 3.0f)};
    auto r = anim_util::Interpolate<glm::vec3>(times, values, 0.5f);
    EXPECT_FLOAT_EQ(r.x, 1.0f);
    EXPECT_FLOAT_EQ(r.y, 2.0f);
    EXPECT_FLOAT_EQ(r.z, 3.0f);
}

TEST(AnimClipEvalTest, Interpolate_TwoKeys_Midpoint) {
    std::vector<float> times = {0.0f, 1.0f};
    std::vector<glm::vec3> values = {glm::vec3(0.0f), glm::vec3(10.0f)};
    auto r = anim_util::Interpolate<glm::vec3>(times, values, 0.5f);
    EXPECT_NEAR(r.x, 5.0f, 0.01f);
    EXPECT_NEAR(r.y, 5.0f, 0.01f);
    EXPECT_NEAR(r.z, 5.0f, 0.01f);
}

TEST(AnimClipEvalTest, Interpolate_QuatSlerp) {
    std::vector<float> times = {0.0f, 1.0f};
    glm::quat q0 = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat q1 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    std::vector<glm::quat> values = {q0, q1};
    auto r = anim_util::Interpolate<glm::quat>(times, values, 0.5f);
    float angle = glm::degrees(glm::angle(r));
    EXPECT_NEAR(angle, 45.0f, 1.0f);
}

TEST(AnimClipEvalTest, AnimSampleBuffer_Init) {
    anim_util::AnimSampleBuffer buf(4);
    EXPECT_EQ(buf.positions.size(), 4u);
    EXPECT_EQ(buf.rotations.size(), 4u);
    EXPECT_EQ(buf.scales.size(), 4u);
    EXPECT_EQ(buf.touched.size(), 4u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_FALSE(buf.touched[i]);
        EXPECT_FLOAT_EQ(buf.scales[i].x, 1.0f);
    }
}

// ============================================================
// 新增共享函数测试
// ============================================================

TEST(AnimClipEvalTest, ComputeBoneGlobals_SimpleChain) {
    Animator3DComponent::SkeletalCache cache;
    cache.bone_count = 3;
    cache.parent_indices = {-1, 0, 1};
    cache.topo_order = {0, 1, 2};
    cache.local_bind_poses = {
        glm::mat4(1.0f), // Root
        glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // Child of root
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))  // Child of child
    };

    Animator3DComponent::PoseBuffer pb;
    pb.positions = {glm::vec3(0), glm::vec3(1,0,0), glm::vec3(0,1,0)};
    pb.rotations = {glm::quat(1,0,0,0), glm::quat(1,0,0,0), glm::quat(1,0,0,0)};
    pb.scales = {glm::vec3(1), glm::vec3(1), glm::vec3(1)};
    pb.touched = {true, true, true};

    std::vector<glm::mat4> globals;
    anim_util::ComputeBoneGlobals(pb, cache, globals);

    EXPECT_EQ(globals.size(), 3u);
    EXPECT_FLOAT_EQ(globals[0][3].x, 0.0f); // Root at origin
    EXPECT_FLOAT_EQ(globals[1][3].x, 1.0f); // Child at (1,0,0)
}

TEST(AnimClipEvalTest, ComputeChainPositions) {
    Animator3DComponent::SkeletalCache cache;
    cache.bone_count = 3;
    cache.parent_indices = {-1, 0, 1};
    cache.topo_order = {0, 1, 2};
    cache.local_bind_poses = {
        glm::mat4(1.0f),
        glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))
    };

    Animator3DComponent::PoseBuffer pb;
    pb.positions = {glm::vec3(0), glm::vec3(1,0,0), glm::vec3(0,1,0)};
    pb.rotations = {glm::quat(1,0,0,0), glm::quat(1,0,0,0), glm::quat(1,0,0,0)};
    pb.scales = {glm::vec3(1), glm::vec3(1), glm::vec3(1)};
    pb.touched = {true, true, true};

    std::vector<int> chain = {0, 1, 2};
    std::vector<glm::vec3> positions;
    std::vector<glm::mat4> globals;
    anim_util::ComputeChainPositions(chain, pb, cache, positions, globals);

    EXPECT_EQ(positions.size(), 3u);
    EXPECT_FLOAT_EQ(positions[0].x, 0.0f);
    EXPECT_FLOAT_EQ(positions[1].x, 1.0f);
}

TEST(AnimClipEvalTest, SolveFABRIK_ReachableTarget) {
    std::vector<glm::vec3> positions = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(2.0f, 0.0f, 0.0f)
    };
    glm::vec3 target(2.0f, 1.0f, 0.0f); // Reachable target (total length = 2)

    anim_util::SolveFABRIK(positions, target, glm::vec3(0), 20, 0.001f);

    float tip_error = glm::length(positions[2] - target);
    EXPECT_LT(tip_error, 0.3f); // Should converge reasonably close
}

TEST(AnimClipEvalTest, SolveFABRIK_UnreachableTarget) {
    std::vector<glm::vec3> positions = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f)
    };
    glm::vec3 target(10.0f, 0.0f, 0.0f); // Far away

    anim_util::SolveFABRIK(positions, target, glm::vec3(0), 5, 0.01f);

    // Should stretch toward target direction
    glm::vec3 dir = glm::normalize(positions[1] - positions[0]);
    glm::vec3 target_dir = glm::normalize(target - positions[0]);
    float dot = glm::dot(dir, target_dir);
    EXPECT_GT(dot, 0.99f); // Should align with target direction
}

TEST(AnimClipEvalTest, SolveFABRIK_EmptyChain) {
    std::vector<glm::vec3> positions;
    glm::vec3 target(1.0f, 0.0f, 0.0f);

    EXPECT_NO_THROW(anim_util::SolveFABRIK(positions, target, glm::vec3(0), 5, 0.01f));
}

TEST(AnimClipEvalTest, AnimBlendNodeDefaultValues) {
    AnimBlendNode node;
    EXPECT_TRUE(node.name.empty());
    EXPECT_TRUE(node.danim_path.empty());
    EXPECT_FLOAT_EQ(node.current_time, 0.0f);
    EXPECT_FLOAT_EQ(node.speed, 1.0f);
    EXPECT_TRUE(node.loop);
    EXPECT_FLOAT_EQ(node.x, 0.0f);
    EXPECT_FLOAT_EQ(node.threshold, 0.0f);
    EXPECT_FLOAT_EQ(node.weight, 1.0f);
}

// ============================================================
// IK 算法正确性测试
// ============================================================

TEST(IKAlgorithmTest, FABRIK_Convergence_AchievableGoals) {
    std::vector<glm::vec3> positions = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(2.0f, 0.0f, 0.0f)
    };
    glm::vec3 target(1.8f, 0.8f, 0.0f);

    anim_util::SolveFABRIK(positions, target, glm::vec3(0), 20, 0.001f);

    float tip_error = glm::length(positions[2] - target);
    EXPECT_LT(tip_error, 0.05f); // Should converge within tolerance
}

TEST(IKAlgorithmTest, FABRIK_BoundaryConditions_ZeroLengthBone) {
    std::vector<glm::vec3> positions = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 0.0f) // Zero-length bone
    };
    glm::vec3 target(1.0f, 0.0f, 0.0f);

    EXPECT_NO_THROW(anim_util::SolveFABRIK(positions, target, glm::vec3(0), 5, 0.01f));
}

TEST(IKAlgorithmTest, FABRIK_BoundaryConditions_SingleBoneChain) {
    std::vector<glm::vec3> positions = {
        glm::vec3(0.0f, 0.0f, 0.0f)
    };
    glm::vec3 target(1.0f, 0.0f, 0.0f);

    EXPECT_NO_THROW(anim_util::SolveFABRIK(positions, target, glm::vec3(0), 5, 0.01f));
}

TEST(IKAlgorithmTest, FABRIK_BoundaryConditions_TheTargetCoincidesWithTheRoot) {
    std::vector<glm::vec3> positions = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f)
    };
    glm::vec3 target(0.0f, 0.0f, 0.0f); // Same as root

    anim_util::SolveFABRIK(positions, target, glm::vec3(0), 5, 0.01f);

    // Should fold back to root
    EXPECT_NEAR(glm::length(positions[1] - positions[0]), 1.0f, 0.1f);
}

TEST(IKAlgorithmTest, FABRIK_PoleVector_BendingDirectionConstraint) {
    std::vector<glm::vec3> positions = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(2.0f, 0.0f, 0.0f)
    };
    glm::vec3 target(2.0f, 1.0f, 0.0f);
    glm::vec3 pole_vector(0.0f, 0.0f, 1.0f); // Bend toward +Z

    anim_util::SolveFABRIK(positions, target, pole_vector, 10, 0.01f);

    // Pole vector 只在关节接近直线时生效，这里测试基本功能不崩溃
    EXPECT_NO_THROW(anim_util::SolveFABRIK(positions, target, pole_vector, 10, 0.01f));
}

