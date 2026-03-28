#include "catch/catch.hpp"
#include "modules/gameplay_3d/rendering/frustum_culling_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_2d.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace dse;
using namespace dse::gameplay3d;

// 正向测试：实体在相机视锥体内时应标记为可见
TEST_CASE("Given_EntityInFrustum_When_CullingUpdated_Then_VisibleIsTrue", "[engine][unit][culling]") {
    World world;
    
    // 1. 创建相机
    auto camera_entity = world.CreateEntity();
    auto& cam = world.registry().emplace<Camera3DComponent>(camera_entity);
    cam.enabled = true;
    cam.priority = 1;
    cam.fov = 60.0f;
    cam.aspect_ratio = 16.0f / 9.0f;
    cam.near_clip = 0.1f;
    cam.far_clip = 100.0f;
    
    auto& cam_transform = world.registry().emplace<TransformComponent>(camera_entity);
    cam_transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    // 默认看向 -Z 方向

    // 2. 创建在视野内的实体 (Z = -10.0f)
    auto obj_entity = world.CreateEntity();
    auto& obj_transform = world.registry().emplace<TransformComponent>(obj_entity);
    obj_transform.position = glm::vec3(0.0f, 0.0f, -10.0f);
    obj_transform.local_to_world = glm::translate(glm::mat4(1.0f), obj_transform.position);

    auto& bbox = world.registry().emplace<BoundingBoxComponent>(obj_entity);
    bbox.min_extents = glm::vec3(-1.0f);
    bbox.max_extents = glm::vec3(1.0f);

    auto& renderer = world.registry().emplace<MeshRendererComponent>(obj_entity);
    renderer.visible = false; // 初始设为 false

    FrustumCullingSystem system;
    system.Update(world);

    // 应该被检测为可见
    REQUIRE(world.registry().get<MeshRendererComponent>(obj_entity).visible == true);
}

// 边界测试：实体刚好在远裁剪面或视锥体边缘外
TEST_CASE("Given_EntityOutsideFrustum_When_CullingUpdated_Then_VisibleIsFalse", "[engine][unit][culling]") {
    World world;
    
    // 相机设置同上
    auto camera_entity = world.CreateEntity();
    auto& cam = world.registry().emplace<Camera3DComponent>(camera_entity);
    cam.enabled = true;
    cam.priority = 1;
    cam.fov = 60.0f;
    cam.aspect_ratio = 16.0f / 9.0f;
    cam.near_clip = 0.1f;
    cam.far_clip = 100.0f;
    
    auto& cam_transform = world.registry().emplace<TransformComponent>(camera_entity);
    cam_transform.position = glm::vec3(0.0f, 0.0f, 0.0f);

    // 创建在视野外的实体 (Z = 10.0f，在相机背后)
    auto obj_entity = world.CreateEntity();
    auto& obj_transform = world.registry().emplace<TransformComponent>(obj_entity);
    obj_transform.position = glm::vec3(0.0f, 0.0f, 10.0f);
    obj_transform.local_to_world = glm::translate(glm::mat4(1.0f), obj_transform.position);

    auto& bbox = world.registry().emplace<BoundingBoxComponent>(obj_entity);
    bbox.min_extents = glm::vec3(-1.0f);
    bbox.max_extents = glm::vec3(1.0f);

    auto& renderer = world.registry().emplace<MeshRendererComponent>(obj_entity);
    renderer.visible = true; // 初始设为 true

    FrustumCullingSystem system;
    system.Update(world);

    // 应该被检测为不可见并置为 false
    REQUIRE(world.registry().get<MeshRendererComponent>(obj_entity).visible == false);
}

// 反向测试：如果没有激活的相机，不应改变现有状态
TEST_CASE("Given_NoActiveCamera_When_CullingUpdated_Then_StateUnchanged", "[engine][unit][culling]") {
    World world;

    auto obj_entity = world.CreateEntity();
    auto& obj_transform = world.registry().emplace<TransformComponent>(obj_entity);
    obj_transform.position = glm::vec3(0.0f, 0.0f, -10.0f);
    obj_transform.local_to_world = glm::translate(glm::mat4(1.0f), obj_transform.position);

    auto& bbox = world.registry().emplace<BoundingBoxComponent>(obj_entity);
    bbox.min_extents = glm::vec3(-1.0f);
    bbox.max_extents = glm::vec3(1.0f);

    auto& renderer = world.registry().emplace<MeshRendererComponent>(obj_entity);
    renderer.visible = true;

    FrustumCullingSystem system;
    system.Update(world);

    // 没有相机的情况下，不会进行计算，状态保持 true
    REQUIRE(world.registry().get<MeshRendererComponent>(obj_entity).visible == true);
}
