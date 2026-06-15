/**
 * @file anim_layer_blend_system_test.cpp
 * @brief AnimLayerBlendSystem 独立单元测试
 *
 * 测试策略：
 * - 空 World 不崩溃
 * - SetAssetManager(nullptr) 安全
 * - 无 AnimLayerComponent 实体时安全
 * - 有 Animator3DComponent 但无 AnimLayerComponent 不崩溃
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/animation/anim_layer_blend_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;
using namespace dse::gameplay3d;

// 测试 动画层混合系统：设置资源管理器空指针安全
TEST(AnimLayerBlendSystemTest, SetAssetManager_NullptrSafety) {
    AnimLayerBlendSystem::SetAssetManager(nullptr);
}

// 测试 动画层混合系统：空世界不崩溃
TEST(AnimLayerBlendSystemTest, EmptyWorldDoesNotCrash) {
    World world;
    AnimLayerBlendSystem::Update(world, 1.0f / 60.0f);
}

// 测试 动画层混合系统：零增量时间不崩溃
TEST(AnimLayerBlendSystemTest, ZerodtDoesNotCrash) {
    World world;
    AnimLayerBlendSystem::Update(world, 0.0f);
}

// 测试 动画层混合系统：带动画器3D无动画层不崩溃
TEST(AnimLayerBlendSystemTest, WithAnimator3DWithoutAnimLayerDoesNotCrash) {
    World world;
    auto entity = world.registry().create();
    world.registry().emplace<Animator3DComponent>(entity);
    AnimLayerBlendSystem::Update(world, 1.0f / 60.0f);
}
